#include "parser.hpp"
#include "diagnostics.hpp"
#include "model.hpp"
#include "stack_guard.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <charconv>
#include <iterator>
#include <limits>

namespace vd::frontend {
namespace {
class Parser {
    BuildConditions conditions;
    std::vector<ConditionalRegion> conditional_regions;
    const SourceFile& source;
    std::vector<Token> tokens;
    std::size_t p = 0;
    Diagnostics& diagnostics;
    unsigned depth = 0, nodes = 0;
    std::list<FunctionSyntax> function_types;
    std::list<ResultSyntax> results;
    std::list<ArraySyntax> arrays;
    std::list<GenericApplication> applications;
    std::map<std::string,Type> substitutions;
    std::set<std::string> generic_names;
    bool instantiating=false,multiline_expression=false;
    bool recover_editor=false;
    Function* parsing_function=nullptr;
    struct FunctionScope {Function*& active;Function* saved;FunctionScope(Function*& slot,Function& next):active(slot),saved(slot){active=&next;}~FunctionScope(){active=saved;}};
    std::vector<TypeUse> type_uses;
    const Token& peek(std::size_t ahead = 0) const { return tokens[std::min(p + ahead, tokens.size() - 1)]; }
    void synchronize(std::size_t start, bool declaration) {
        // Reconstruct indentation at the failure and its safe boundary.
        int baseline = 0, current_depth = 0;
        for (std::size_t i = 0; i < start; ++i) {
            if (tokens[i].kind == TokenKind::indent) ++baseline;
            if (tokens[i].kind == TokenKind::dedent) --baseline;
        }
        current_depth = baseline;
        for (std::size_t i = start; i < p && i < tokens.size(); ++i) {
            if (tokens[i].kind == TokenKind::indent) ++current_depth;
            if (tokens[i].kind == TokenKind::dedent) --current_depth;
        }
        bool line_start = p == 0 || tokens[std::min(p, tokens.size()) - 1].kind == TokenKind::newline;
        while (peek().kind != TokenKind::end) {
            if (declaration && current_depth == 0 && line_start && (at("compile") || at("func") || at("property") || at("interface") || at("const") || at("var") || at("value") || at("struct") || at("virtual") || at("override") || at("use") || at("private") || at("public") || at("internal"))) return;
            if (!declaration && current_depth <= baseline) {
                if (peek().kind == TokenKind::dedent) return;
                if (line_start && p > start) return;
            }
            auto token = take();
            if (token.kind == TokenKind::indent) ++current_depth;
            if (token.kind == TokenKind::dedent) --current_depth;
            line_start = token.kind == TokenKind::newline || token.kind == TokenKind::dedent;
        }
    }
    Type grouped_type_end(Type value) {
        need(")");
        if(accept("?")) {
            if(value.nullable)error(source,tokens[p-1],Code::unsupported_type,"A nullable type cannot have a second ? suffix.");
            value.nullable=true;
        }
        return value;
    }
    Type signature_result(std::vector<ReturnComponent>* components=nullptr) {
        if(!at("("))return type();
        const auto opening=take();
        Depth guard(depth,source,opening);
        const bool named=peek(1).text==":";
        if(!named) {
            auto site=peek();auto first=type();
            if(!at(","))return grouped_type_end(first);
            if(components)components->push_back({site,{},std::move(first)});
            take();
        }
        if(!components)
            error(source,opening,Code::unsupported_type,"Function-value types do not support multiple returns.");
        std::set<std::string> names;
        do {
            std::optional<Token> label;
            if((peek(1).text==":")!=named)
                error(source,peek(),Code::syntax,"Return components must be either all named or all unnamed.");
            if(named) {
                label=name();need(":");
                if(!names.insert(label->text).second)
                    error(source,*label,Code::duplicate,"Duplicate return component name '"+label->text+"'.");
            }
            auto site=peek();
            components->push_back({site,std::move(label),type()});
        } while(accept(","));
        need(")");
        if(components->size()<2)
            error(source,opening,Code::argument_count,"A multiple-return declaration requires at least two components.");
        return Type::none;
    }
    void function_result(Function& f) {
        nodes=0;const auto site=peek();
        f.result=signature_result(&f.returns);
        if(!f.returns.empty() && (f.anonymous || f.asynchronous || !f.operator_glyph.empty()))
            error(source,site,Code::unsupported_type,"Multiple returns are not supported for anonymous functions, async functions or operators.");
    }
    void parameters(Function& f, bool defaults = true) {
        need("(");
        if (!at(")")) do {
            bool readonly_parameter=accept("const");
            auto token = name(); nodes=0; Parameter parameter{token,Type::none};
            if(accept(":")) {
                const auto begin=p;parameter.type=type();
                if(!f.operator_glyph.empty()){std::string spelling;for(auto n=begin;n<p;++n)spelling+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;f.operator_pattern+=std::to_string(spelling.size())+":"+spelling;}
            }
            else if(!at("=") || !defaults || !f.generics.empty()) error(source,token,Code::type_mismatch,"A parameter requires an explicit type or a concrete compile-time default; generic parameter types must be explicit.");
            parameter.type.readonly=readonly_parameter;
            if (accept("=")) {
                if(!defaults) error(source,tokens[p-1],Code::argument_count,f.operator_glyph.empty()?"Interface requirements cannot declare parameter defaults.":"Operator operands cannot declare defaults.");
                auto begin = p; parameter.default_begin = peek().offset; nodes = 0; parameter.default_expression = expression();
                parameter.default_end = peek().offset;
                for (auto n = begin; n < p; ++n) parameter.default_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
            }
            f.parameters.push_back(std::move(parameter));
        } while (accept(",") && !at(")"));
        need(")"); if (accept("->")) function_result(f);
    }
    void operator_declaration(Structure& owner,std::string visibility) {
        need("operator");Function f;f.visibility=std::move(visibility);f.token=take();f.operator_glyph=f.token.text;f.operator_pattern=std::to_string(f.operator_glyph.size())+":"+f.operator_glyph;
        if(f.operator_glyph=="!=")error(source,f.token,Code::syntax,"Declare operator ==; != is derived from that equality declaration.");
        if(!ir::operator_arity(f.operator_glyph,1) && !ir::operator_arity(f.operator_glyph,2))error(source,f.token,Code::syntax,"Unsupported operator declaration '"+f.operator_glyph+"'.");
        if(at("<"))error(source,peek(),Code::syntax,"Operators cannot introduce operator-specific type parameters.");
        parameters(f,false);
        if(!ir::operator_arity(f.operator_glyph,f.parameters.size()))error(source,f.token,Code::argument_count,"Operator has invalid arity; only unary +/-, or two operands for the approved binary glyphs, are permitted.");
        for(const auto& operand:f.parameters)if(!operand.type.readonly)error(source,operand.token,Code::readonly,"Operator operands require explicit const.");
        if(f.result==Type::none)error(source,f.token,Code::type_mismatch,"An operator requires an explicit storable result type.");
        if((f.operator_glyph=="==" || f.operator_glyph=="<" || f.operator_glyph==">" || f.operator_glyph=="<=" || f.operator_glyph==">=") && f.result!=Type::boolean)error(source,f.token,Code::type_mismatch,"Equality and ordering operators must return exactly bool.");
        f.operator_owner=Type(owner.definition,owner.definition->managed?Type::managed_ref:Type::aggregate);
        auto begin=p;{FunctionScope scope(parsing_function,f);f.body=block();}
        for(auto n=begin;n<p;++n)f.body_syntax+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;
        f.source_end=peek().offset;f.parse_failed=diagnostics.within(source,f.token.offset,f.source_end);f.substitutions=substitutions;
        if(f.operator_glyph=="=="){owner.definition->custom_equality=true;owner.definition->custom_equality_offset=f.token.offset;}
        owner.operators.push_back(std::move(f));
    }
    bool at(const std::string& text) const { return peek().text == text; }
    Token take() { auto result = peek(); if (p + 1 < tokens.size()) ++p; return result; }
    bool accept(const std::string& text) { if (!at(text)) return false; ++p; return true; }
    Token need(const std::string& text) {
        if (!at(text)) {
            if (text == "<indent>") error(source, peek(), Code::syntax, "Expected an indented block.", "Indent the body with spaces beneath its declaration or control-flow statement.");
            if (text == "<newline>") error(source, peek(), Code::syntax, "Expected end of statement, found '" + peek().text + "'.", "Put each statement on its own line; postfix ++/-- cannot be used as values.");
            error(source, peek(), Code::syntax, "Expected '" + text + "', found '" + peek().text + "'.");
        }
        return take();
    }
    Token name() {
        if (peek().kind != TokenKind::name || reserved.contains(peek().text) || at("Some") || at("None"))
            error(source, peek(), Code::identifier, "Expected a lowercase identifier.");
        return take();
    }
    std::vector<GenericParameter> generic_parameters() {
        std::vector<GenericParameter> result;
        if(!accept("<"))return result;
        do {
            auto token=name();
            if(!generic_names.insert(token.text).second)error(source,token,Code::duplicate,"Duplicate generic type parameter '"+token.text+"'.");
            std::optional<Type> constraint;std::vector<Token> syntax;
            if(accept(":")) {
                auto begin=p;const auto uses=type_uses.size(),applications_before=applications.size(),arrays_before=arrays.size(),functions_before=function_types.size(),results_before=results.size();
                constraint=type();syntax.assign(tokens.begin()+begin,tokens.begin()+p);
                if(instantiating) {
                    // Constraints are bound by specialization before reparsing
                    // the body. Do not publish a second, unbound annotation copy.
                    type_uses.resize(uses);
                    while(applications.size()>applications_before)applications.pop_back();
                    while(arrays.size()>arrays_before)arrays.pop_back();
                    while(function_types.size()>functions_before)function_types.pop_back();
                    while(results.size()>results_before)results.pop_back();
                }
            }
            result.push_back({token,constraint,std::move(syntax)});
        }while(accept(","));
        need(">");return result;
    }
    std::vector<Type> type_arguments() {
        std::vector<Type> result;need("<");do {result.push_back(type());}while(accept(","));need(">");return result;
    }
    bool type_call(bool value=false,bool relationship=false,std::size_t start=std::string::npos) const {
        if(start==std::string::npos)start=p;
        if(start>=tokens.size() || tokens[start].text!="<")return false;
        if(binary_operator(start)=="<<")return false;
        unsigned nesting=0;
        for(auto n=start;n<tokens.size();++n) {
            if(tokens[n].text=="<")++nesting;
            else if(tokens[n].text==">") {if(!--nesting){if(n+1>=tokens.size())return false;const auto& next=tokens[n+1];if(relationship)return next.text=="has" || (next.text=="?" && n+2<tokens.size() && tokens[n+2].text=="has");return next.text=="(" || (value && (next.text=="." || next.kind==TokenKind::newline || next.kind==TokenKind::end || next.text=="," || next.text==")" || next.text=="]" || next.text=="}" || next.text==":"));}}
            else if(tokens[n].kind==TokenKind::newline || tokens[n].kind==TokenKind::end)return false;
        }
        return false;
    }
    // Keep branch-specific temporaries out of every recursive type frame. The
    // existing shared Depth guard still counts each source type exactly once.
    VOID_SEMANTIC_HANDLER Type function_type() {
        auto token=take();need("(");std::vector<Type> parameters;
        if(!at(")"))do{auto readonly=accept("const");auto parameter=type();parameter.readonly=readonly;parameters.push_back(parameter);}while(accept(",") && !at(")"));need(")");
        auto result=accept("->")?signature_result():Type(Type::none);
        auto definition=std::make_shared<ir::ValueType>();definition->callable=true;definition->name=definition->identity="func";
        function_types.push_back({token,std::move(parameters),result,definition});Type value(definition);value.nullable=accept("?");return value;
    }
    VOID_SEMANTIC_HANDLER Type result_type() {
        auto token=take();need("<");auto success=type();need(",");auto failure=type();auto close=need(">");
        auto d=std::make_shared<ir::ValueType>();d->name=d->identity="result";d->display_name=source.text().substr(token.offset,close.offset+1-token.offset);d->result_type=true;
        results.push_back({token,success,failure,d});return Type(d);
    }
    VOID_SEMANTIC_HANDLER Type dictionary_type() {
        auto token=take();auto key=type();need(":");auto value=type();auto close=need("}");
        const bool nullable=accept("?");
        auto d=std::make_shared<ir::ValueType>();d->name=d->identity=source.text().substr(token.offset,close.offset+1-token.offset);
        arrays.push_back({token,value,d,{}});arrays.back().key=key;Type result(d,Type::dictionary);result.nullable=nullable;return result;
    }
    VOID_SEMANTIC_HANDLER Type array_type() {
        auto token=take();auto element=type();std::unique_ptr<Expr> extent;
        if(accept(",")) extent=expression();
        auto close=need("]");
        const bool nullable=accept("?");
        auto d=std::make_shared<ir::ValueType>();d->name=source.text().substr(token.offset,close.offset+1-token.offset);d->identity=d->name;
        Type result(d,extent ? Type::fixed_array : Type::growable_array);result.nullable=nullable;
        arrays.push_back({token,element,d,std::move(extent)});return result;
    }
    VOID_SEMANTIC_HANDLER Type named_type(bool qualified=true) {
        static const std::map<std::string, Type> types = {
            {"i8",Type::i8},{"i16",Type::i16},{"i32",Type::i32},{"i64",Type::i64},
            {"u8",Type::u8},{"u16",Type::u16},{"u32",Type::u32},{"u64",Type::u64},
            {"f32",Type::f32},{"f64",Type::f64},{"bool",Type::boolean},{"string",Type::string}};
        auto found = types.find(peek().text);
        if (found != types.end()) {auto result=found->second;type_uses.push_back({take(),result,{}});result.nullable=accept("?");return result;}
        if (peek().kind != TokenKind::name || reserved.contains(peek().text)) error(source,peek(),Code::unsupported_type,"Unsupported type '" + peek().text + "'.","Use a numeric type, bool, string or a visible value/managed struct.");
        auto token = name();
        std::string spelling = token.text;
        auto reference_token = token;
        if (qualified && accept(".")) { reference_token = name(); spelling += "." + reference_token.text; }
        if(spelling==token.text && substitutions.contains(spelling)) {
            auto result=substitutions.at(spelling);if(accept("?"))result.nullable=true;return result;
        }
        auto d = std::make_shared<ir::ValueType>(); d->name = spelling; d->identity = spelling; d->source_offset = token.offset;
        if(at("<")){applications.push_back({reference_token,spelling,d,type_arguments()});d->name=source.text().substr(token.offset,tokens[p-1].offset+1-token.offset);}
        Type result(d,Type::unresolved); result.nullable = accept("?");
        type_uses.push_back({reference_token,result,reference_token.offset==token.offset?std::optional<Token>{}:token});
        return result;
    }
    Type type(bool static_owner=false) {
        Depth guard(depth,source,peek());
        if(accept("("))return grouped_type_end(type());
        if(at("func"))return function_type();
        if(at("result")){auto result=result_type();result.nullable=accept("?");return result;}
        if(at("{"))return dictionary_type();
        if(at("["))return array_type();
        return named_type(!static_owner);
    }
    void newline() { if(multiline_expression && !at("<newline>")){multiline_expression=false;return;}multiline_expression=false;need("<newline>"); }
    bool repair_available() const { return recover_editor && diagnostics.recovery->available(); }
    Token member_name() {
        if(at("string") || at("has") || at("with"))return take();
        if(repair_available() && !diagnostics.editor) {
            const auto& next=peek();
            if(next.kind==TokenKind::newline || at(":") || at(",") || at(")") || at("]") || at("}"))
                return {TokenKind::name,"",next.offset};
            if(next.kind==TokenKind::name && reserved.contains(next.text))return take();
        }
        return name();
    }
    bool recover_leaf_statement() {
        if(!repair_available())return false;
        // A dropped declaration cannot shadow a written use. For completion,
        // only probe a member, where the omitted name cannot enter the candidate set.
        const bool member_or_presentation=!diagnostics.editor || diagnostics.recovery->member_probe;
        if(member_or_presentation && (at("var") || at("const")) && peek(1).kind==TokenKind::name && !reserved.contains(peek(1).text) &&
           peek(2).text=="=" && peek(3).kind==TokenKind::newline && peek(4).kind!=TokenKind::indent &&
           std::none_of(tokens.begin()+p+2,tokens.end(),[&](const auto& t){return t.text==peek(1).text;})) {
            p+=4;
        } else if(at("pass") && peek(1).kind==TokenKind::name &&
                  peek(2).kind==TokenKind::newline && peek(3).kind!=TokenKind::indent) {
            p+=3;
        } else return false;
        diagnostics.recovery->record();
        return true;
    }
    std::vector<Statement> block_body() {
        need("<indent>");
        std::vector<Statement> body;
        bool recovered = false;
        while (peek().kind != TokenKind::dedent && peek().kind != TokenKind::end) {
            if (diagnostics.full()) { p = tokens.size() - 1; break; }
            auto start = p;
            if(recover_leaf_statement()){recovered=true;continue;}
            try { body.push_back(statement()); }
            catch (const CompilationError& e) {
                diagnostics.add(e); recovered = true;
                if ((tokens[start].text == "var" || tokens[start].text == "const") &&
                    start+2<tokens.size() && tokens[start+2].text!="," &&
                    tokens[start+1].kind==TokenKind::name && !reserved.contains(tokens[start+1].text)) {
                    Statement invalid(Statement::Kind::variable, tokens[start + 1]);
                    invalid.name = invalid.token.text; invalid.invalid = true; body.push_back(std::move(invalid));
                }
                synchronize(start, false);
            }
            catch (const Poison&) { recovered = true; synchronize(start, false); }
            if (p == start && peek().kind != TokenKind::dedent && peek().kind != TokenKind::end) take();
        }
        if (body.empty() && !recovered) error(source, peek(), Code::syntax, "Expected an indented statement.");
        need("<dedent>"); return body;
    }
    std::vector<Statement> block() {
        Depth guard(depth, source, peek(), 64);
        need(":"); newline();
        return block_body();
    }
#include "pattern_parser.inc"

    // Keep angle tokens separate for nested generic types. Only adjacent,
    // equal angles in expression position form a shift (not `< <` or `> >`).
    std::string binary_operator(std::size_t start=std::string::npos) const {
        if(start==std::string::npos)start=p;
        if(start>=tokens.size())return {};
        const auto& first=tokens[start];
        if((first.text=="<" || first.text==">") && start+1<tokens.size() &&
           tokens[start+1].text==first.text && tokens[start+1].offset==first.offset+1)
            return first.text+tokens[start+1].text;
        return first.text;
    }
    static int precedence(const std::string& op) {
        if (op == "or") return 1;
        if (op == "and") return 2;
        if(op=="is")return 3;
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
        if (op == "|") return 4;
        if (op == "^") return 5;
        if (op == "&") return 6;
        if (op == "<<" || op == ">>") return 7;
        if (op == "+" || op == "-") return 8;
        if (op == "*" || op == "/" || op == "%") return 9;
        return 0;
    }
    std::unique_ptr<Expr> postfix(std::unique_ptr<Expr> e, bool propagation = true) {
        while(true) {
            if(accept(".")) {
                auto member=std::make_unique<Expr>(Expr::Kind::field,member_name());
                member->parts.push_back(std::move(e));e=std::move(member);
            } else if(at("[")) {
                if(++nodes>512) error(source,peek(),Code::nesting,"Expression size limit exceeded.");
                auto selected=std::make_unique<Expr>(Expr::Kind::array_index,take());
                selected->parts.push_back(std::move(e));selected->parts.push_back(expression());
                if(at("..") || at("..<")){selected->kind=Expr::Kind::array_slice;selected->op=take().text;selected->parts.push_back(expression());}
                need("]");e=std::move(selected);
            } else if(propagation && accept("?")) {
                auto propagation=std::make_unique<Expr>(Expr::Kind::propagate,tokens[p-1]);propagation->parts.push_back(std::move(e));e=std::move(propagation);
            } else if(type_call(true) && (e->kind==Expr::Kind::name || e->kind==Expr::Kind::field)) {
                e->type_arguments=type_arguments();
            } else if(accept("(")) {
                if(e->kind==Expr::Kind::field) e->kind=Expr::Kind::method_call;
                else if(e->kind==Expr::Kind::name) e->kind=Expr::Kind::call;
                else {
                    auto invocation=std::make_unique<Expr>(Expr::Kind::method_call,e->token);invocation->token.source_width=e->token.text.size();invocation->token.text="call";
                    invocation->parts.push_back(std::move(e));e=std::move(invocation);
                }
                if(!at(")")) do {
                    Token named{TokenKind::name,"",peek().offset};
                    if(p+1<tokens.size() && tokens[p+1].text=="=") {named=name();need("=");}
                    e->names.push_back(named);e->parts.push_back(expression());
                } while(accept(",") && !at(")"));
                need(")");
            } else break;
        }
        return e;
    }
    std::unique_ptr<Expr> primary(bool propagation) {
        if (++nodes > 512) error(source, peek(), Code::nesting, "Expression size limit exceeded.", "Split this expression into smaller local values.");
        auto token=take();
        if(token.text=="func") {
            if(!parsing_function)error(source,token,Code::syntax,"Anonymous functions require a function body.");
            Function lambda;lambda.anonymous=true;lambda.token=token;lambda.token.text="$lambda$"+std::to_string(parsing_function->lambdas.size());lambda.token.source_width=4;lambda.visibility="private";
            parameters(lambda);auto* parent=parsing_function;auto begin=p;{FunctionScope scope(parsing_function,lambda);lambda.body=block();}
            for(auto n=begin;n<p;++n)lambda.body_syntax+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;
            auto expression=std::make_unique<Expr>(Expr::Kind::lambda,token);expression->slot=static_cast<unsigned>(parent->lambdas.size());parent->lambdas.push_back(std::move(lambda));multiline_expression=true;return expression;
        }
        if(token.text=="if")return choice_expression(token);
        if(token.kind==TokenKind::name){
            auto cursor=p;
            while(cursor+1<tokens.size() && tokens[cursor].text=="." && tokens[cursor+1].kind==TokenKind::name)cursor+=2;
            const bool relationship=tokens[cursor].text=="has" || (tokens[cursor].text=="?" && cursor+1<tokens.size() && tokens[cursor+1].text=="has") || type_call(false,true,cursor);
            if(relationship){--p;return has_expression();}
        }
        if(token.text=="match")return match_expression(token);
        if(token.kind==TokenKind::invalid) throw Poison{};
        if(token.text=="{") {
            auto e=std::make_unique<Expr>(Expr::Kind::dictionary_literal,token);
            if(!at("}"))do{e->parts.push_back(expression());need(":");e->parts.push_back(expression());}while(accept(",") && !at("}"));
            need("}");return postfix(std::move(e),propagation);
        }
        if(token.text=="[") {
            auto e=std::make_unique<Expr>(Expr::Kind::array_literal,token);
            if(!at("]")) do { e->parts.push_back(expression()); } while(accept(","));
            need("]");return postfix(std::move(e),propagation);
        }
        if(token.kind==TokenKind::string || token.kind==TokenKind::number || token.text=="true" || token.text=="false" || token.text=="null") {
            auto e=std::make_unique<Expr>(Expr::Kind::literal,token);
            if(token.kind==TokenKind::string) {e->type=Type::string;e->aggregate_value.text=token.decoded;}
            else if(token.kind==TokenKind::number) e->literal_text=token.text;
            else {e->type=token.text=="null" ? Type::null_value : Type::boolean;e->value=token.text=="true";}
            return postfix(std::move(e),propagation);
        }
        if(token.text=="(") {auto e=expression();need(")");return postfix(std::move(e),propagation);}
        if(token.kind==TokenKind::name && (!reserved.contains(token.text) || ((numeric::primitive(token.text)!=Type::none || token.text=="string") && at(".")) || (token.text=="string" && at("(")))) return postfix(std::make_unique<Expr>(Expr::Kind::name,token),propagation);
        error(source,token,Code::syntax,"Expected an expression, found '"+token.text+"'.");
    }
    std::unique_ptr<Expr> unary(bool propagation = true) {
        Depth guard(depth, source, peek());
        if(accept("spawn")) {auto e=std::make_unique<Expr>(Expr::Kind::spawned,tokens[p-1]);e->parts.push_back(unary(propagation));return e;}
        // Calls/member/index chains belong to the task operand, but an ungrouped
        // '?' belongs to its awaited result. Parentheses (and argument/index
        // expressions) enter expression() afresh, retaining explicit grouping.
        if(accept("await")) {auto e=std::make_unique<Expr>(Expr::Kind::awaited,tokens[p-1]);e->parts.push_back(unary(false));return postfix(std::move(e),propagation);}
        if (at("not") || at("-") || at("+") || at("~")) {
            auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::unary, token); e->op = token.text;
            // not binds below comparisons: not a < b means not (a < b).
            e->parts.push_back(token.text == "not" ? expression(3) : unary(propagation));
            if ((token.text == "-" || token.text == "+") && e->parts[0]->kind == Expr::Kind::literal && !e->parts[0]->literal_text.empty() &&
                e->parts[0]->literal_text.front() != '-' && e->parts[0]->literal_text.front() != '+') {
                e->kind = Expr::Kind::literal; e->literal_text = token.text + e->parts[0]->literal_text; e->parts.clear();
            }
            return e;
        }
        return primary(propagation);
    }
    VOID_SEMANTIC_HANDLER std::unique_ptr<Expr> has_expression() {
        auto left=type();auto site=need("has");auto right=type();
        auto e=std::make_unique<Expr>(Expr::Kind::has,site);e->type_arguments={left,right};return e;
    }
    std::unique_ptr<Expr> expression(int minimum = 1) {
        Depth guard(depth, source, peek());
        auto left = unary();
        while (precedence(binary_operator()) >= minimum || ((at("as") || at("as?")) && minimum <= 10)) {
            if (at("as") || at("as?")) {
                if (++nodes > 512) error(source, peek(), Code::nesting, "Expression size limit exceeded.", "Split this expression into smaller local values.");
                auto token = take();
                if (token.text=="as" && at("?")) error(source,peek(),Code::syntax,"Write the fallible conversion operator as? without an intervening space.");
                auto e = std::make_unique<Expr>(Expr::Kind::conversion, token);
                e->parts.push_back(std::move(left)); e->type = type(); left = std::move(e); continue;
            }
            auto op=binary_operator();auto token=take();
            if(op.size()==2 && (op=="<<" || op==">>")){take();token.text=op;}
            if(token.text=="is") {auto e=std::make_unique<Expr>(Expr::Kind::test,token);e->parts.push_back(std::move(left));e->patterns.push_back(pattern());left=std::move(e);continue;}
            auto e = std::make_unique<Expr>(Expr::Kind::binary, token); e->op = token.text;
            e->parts.push_back(std::move(left)); e->parts.push_back(expression(precedence(token.text) + 1));
            left = std::move(e);
        }
        return left;
    }
    // Conditions in if and for share the same editor-only member-hole grammar.
    // Keep ordinary parsing strict; a completion probe can arrive before the
    // user has written the colon or an indented body.
    VOID_SEMANTIC_HANDLER void condition_block(Statement& s) {
        const auto* probe=diagnostics.editor;
        const bool typed_member=peek(2).kind==TokenKind::name && peek(3).kind==TokenKind::newline &&
            !(probe && peek(2).text=="_void_editor_cursor_" && peek(2).offset==probe->offset);
        const bool missing_member=repair_available() && peek().kind==TokenKind::name && peek(1).text=="." &&
            (typed_member || peek(2).kind==TokenKind::newline || (peek(2).text==":" && peek(3).kind==TokenKind::newline));
        if(missing_member) {
            auto receiver=std::make_unique<Expr>(Expr::Kind::name,name());
            auto dot=take();
            auto member=typed_member?take():Token{TokenKind::name,"",peek().offset};
            s.selection=std::make_unique<Expr>(Expr::Kind::field,std::move(member));
            s.selection->parts.push_back(std::move(receiver));
            s.expression=std::make_unique<Expr>(Expr::Kind::literal,std::move(dot));
            s.expression->type=Type::boolean;
            accept(":");
        } else s.expression=expression();
        bool incomplete_probe=false;
        if(repair_available() && at("<newline>")) {
            // The member can be under `not`, a comparison or a call. Its
            // receiver is still checked by the ordinary semantic probe.
            std::vector<const Expr*> expressions{s.expression.get()};
            while(!expressions.empty()) {
                const auto* expression=expressions.back();expressions.pop_back();
                if(expression->kind==Expr::Kind::field &&
                   ((probe && expression->token.text=="_void_editor_cursor_" && expression->token.offset==probe->offset) ||
                    (!probe && expression->token.text.empty()))){incomplete_probe=true;break;}
                for(const auto& part:expression->parts)expressions.push_back(part.get());
            }
        }
        if(missing_member || incomplete_probe) {
            Depth block_depth(depth, source, peek(), 64);
            diagnostics.recovery->record();
            newline();
            if(at("<indent>"))s.body=block_body();
        } else s.body=block();
    }
    Statement conditional(Token token) {
        Depth guard(depth, source, token, 64);
        Statement s(Statement::Kind::conditional, std::move(token));
        condition_block(s);
        if (accept("else")) {
            if (at("if")) { auto next = take(); s.otherwise.push_back(conditional(next)); }
            else s.otherwise = block();
        }
        return s;
    }
    Statement statement() {
        nodes = 0;
        if(at("yield") && ((parsing_function && parsing_function->generator && peek(1).text!="=") ||
           peek(1).kind==TokenKind::number || peek(1).kind==TokenKind::string ||
           (peek(1).kind==TokenKind::name && peek(1).text!="as"))) {
            Statement yield(Statement::Kind::yielded,take());
            yield.expression=expression();
            newline();
            return yield;
        }
        if(at("defer"))return deferred_statement();
        if(at("for"))return loop_statement();
        return ordinary_statement();
    }
    // Keep unrelated declaration temporaries off nested loop parser frames,
    // so the unchanged 64-block guard can diagnose on a 1 MiB native stack.
    VOID_SEMANTIC_HANDLER Statement loop_statement() {
        Statement s(Statement::Kind::loop, take());
        // Await is also an ordinary condition expression (`for await ready():`).
        // Only consume the iteration modifier when a binder header follows it.
        s.asynchronous_iteration=at("await") && peek(1).kind==TokenKind::name &&
            (peek(2).text=="in" || peek(2).text==",");
        if(s.asynchronous_iteration)take();
        if(peek().kind==TokenKind::name && (peek(1).text=="in" || peek(1).text==",")) {
            s.binder=name();if(accept(","))s.index_binder=name();need("in");s.reverse=accept("reverse");
            s.expression=expression();
            if(at("..") || at("..<")) {s.exclusive=take().text=="..<";s.range_end=expression();}
        } else if (!at(":")) {
            condition_block(s);
            return s;
        }
        s.body = block(); return s;
    }
    // A deferred body recurses through statement(). Keep unrelated statement
    // temporaries off that path so its existing 64-level guard can report.
    VOID_SEMANTIC_HANDLER Statement deferred_statement() {
        auto token=take();Depth guard(depth,source,token,64);Statement statement(Statement::Kind::deferred,token);
        if(at("{")) {
            need("{");newline();need("<indent>");Function lambda;lambda.anonymous=true;lambda.deferred=true;lambda.token=token;lambda.token.text="$defer$"+std::to_string(parsing_function->lambdas.size());lambda.visibility="private";
            auto* parent=parsing_function;{FunctionScope scope(parsing_function,lambda);
            while(!at("<dedent>") && !at("<end of file>"))lambda.body.push_back(this->statement());need("<dedent>");need("}");newline();}
            statement.expression=std::make_unique<Expr>(Expr::Kind::lambda,token);statement.expression->slot=parent->lambdas.size();parent->lambdas.push_back(std::move(lambda));
        }else{statement.expression=expression();newline();}
        return statement;
    }
    VOID_SEMANTIC_HANDLER Statement ordinary_statement() {
        if (at("pass")) {
            Statement s(Statement::Kind::no_op, take());
            newline(); return s;
        }
        if (at("while")) {
            Diagnostic diagnostic(Code::while_loop);
            diagnostic.primary = span(source, peek());
            diagnostic.description = "VOID uses 'for' for all loops.";
            diagnostic.hint = "Replace 'while condition:' with 'for condition:'.";
            // The literal condition has one interpretation in both spellings.
            // Keep iteration headers and incomplete conditions diagnostic-only.
            if ((peek(1).text == "true" || peek(1).text == "false") && peek(2).text == ":")
                diagnostic.fix = SourceFix{"Replace 'while' with 'for'", "for"};
            throw CompilationError({std::move(diagnostic)});
        }
        if(at("match")) {
            Statement s(Statement::Kind::match,take());s.expression=expression();need(":");newline();need("<indent>");
            while(!at("<dedent>") && !at("<end of file>")) {
                Statement arm(Statement::Kind::no_op,peek());arm.pattern=pattern();
                if(recover_match_arm(*arm.pattern)) { if(at("<indent>"))arm.body=block_body(); }
                else arm.body=block();
                s.body.push_back(std::move(arm));
            }
            need("<dedent>");return s;
        }
        if (at("if")) return conditional(take());
        if (at("var") || at("const")) {
            auto start = p;
            bool readonly = at("const");
            Statement s(Statement::Kind::variable, take()); s.readonly = readonly; auto n = name(); s.name = n.text; s.token = n;
            if(accept(",")) {
                s.kind=Statement::Kind::destructure;
                s.bindings.push_back({n});
                do { s.bindings.push_back({name()}); } while(accept(","));
                need("=");s.expression=expression();
                newline();s.source_end=peek().offset;
                return s;
            }
            if (accept(":")) s.annotation = type();
            if (accept("=")) s.expression = expression();
            if (readonly && !s.expression) error(source, s.token, Code::initializer, "A const requires an initializer.");
            if (readonly) for (auto i = start; i < p; ++i)
                s.constant_syntax += std::to_string(tokens[i].text.size()) + ":" + tokens[i].text;
            newline();s.source_end=peek().offset; return s;
        }
        if(at("with")){
            // Stop before conversions so `as` still introduces the resource binder.
            Statement statement(Statement::Kind::scoped,take());statement.expression=expression(11);
            if(accept("as"))statement.binder=name();else statement.binder={TokenKind::name,"$with$"+std::to_string(statement.token.offset),statement.token.offset};
            statement.body=block();return statement;
        }
        if (at("return")) {
            Statement s(Statement::Kind::returned, take());
            if (peek().kind != TokenKind::newline) s.expression = expression();
            if(s.expression && accept(",")) {
                auto pack=std::make_unique<Expr>(Expr::Kind::return_pack,s.token);
                pack->parts.push_back(std::move(s.expression));
                do { pack->parts.push_back(expression()); } while(accept(","));
                s.expression=std::move(pack);
            }
            if(!s.expression && parsing_function && parsing_function->token.text=="$application") {
                s.expression=std::make_unique<Expr>(Expr::Kind::literal,s.token);s.expression->literal_text="0";
            }
            newline(); return s;
        }
        if (at("break") || at("continue")) {
            auto token = take(); Statement s(token.text == "break" ? Statement::Kind::broken : Statement::Kind::continued, token);
            newline(); return s;
        }
        if (peek().kind == TokenKind::name && p + 1 < tokens.size()) {
            auto end = p + 1;unsigned indexes=0;bool after_index=false,generic_selection=false;
            while(end+1<tokens.size()) {
                if(tokens[end].text=="<") {
                    auto saved=end;unsigned nesting=0;do {if(tokens[end].text=="<")++nesting;else if(tokens[end].text==">")--nesting;++end;}while(end+1<tokens.size() && nesting && tokens[end].kind!=TokenKind::newline);
                    if(nesting || tokens[end].text!="."){end=saved;break;}generic_selection=true;continue;
                }
                if(tokens[end].text=="." && tokens[end+1].kind==TokenKind::name){after_index|=indexes!=0;end+=2;continue;}
                if(tokens[end].text!="[")break;
                ++indexes;unsigned brackets=0;
                do {if(tokens[end].text=="[")++brackets;else if(tokens[end].text=="]")--brackets;++end;}
                while(end+1<tokens.size() && brackets && tokens[end].kind!=TokenKind::newline);
            }
            auto op = tokens[end].text;
            if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "&=" || op == "|=" || op == "^=" || op == "<<=" || op == ">>=" || op == "++" || op == "--") {
                Statement s(Statement::Kind::assignment, name());s.name=s.token.text;
                if(indexes>1 || after_index || generic_selection)s.selection=postfix(std::make_unique<Expr>(Expr::Kind::name,s.token));
                else {while(accept("."))s.fields.push_back(name());if(accept("[")){s.index=expression();need("]");}}
                s.compound_token=take();s.op = s.compound_token.text;
                if (op == "++" || op == "--") {
                    s.expression = std::make_unique<Expr>(Expr::Kind::literal, s.token); s.expression->literal_text = "1";
                } else s.expression = expression();
                newline(); return s;
            }
        }
        Statement s(Statement::Kind::expression, peek()); s.expression = expression(); newline(); return s;
    }
public:
    explicit Parser(const SourceFile& s, Diagnostics& d, std::string_view module = {},const BuildConditions& configuration=native_build_conditions())
        : conditions(configuration),source(s), tokens(lex(s,d,true,d.recovery && d.recovery->module==module?d.recovery:nullptr,&conditions,&conditional_regions)), diagnostics(d) {}
    Parser(const SourceFile& s,Diagnostics& d,std::vector<Token> syntax,std::map<std::string,Type> bindings)
        :source(s),tokens(std::move(syntax)),diagnostics(d),substitutions(std::move(bindings)),instantiating(true) {
        tokens.push_back({TokenKind::end,"<end of file>",tokens.empty()?0:tokens.back().offset});
    }
    std::vector<Token> take_syntax_tokens() { return std::move(tokens); }
    Type parse_type(Unit& unit) {
        auto result=type();need("<end of file>");
        unit.type_uses.insert(unit.type_uses.end(),std::make_move_iterator(type_uses.begin()),std::make_move_iterator(type_uses.end()));
        unit.function_types.splice(unit.function_types.end(),function_types);unit.results.splice(unit.results.end(),results);
        unit.arrays.splice(unit.arrays.end(),arrays);unit.applications.splice(unit.applications.end(),applications);
        return result;
    }
    void parse(Unit& unit) {
        unit.conditions=conditions;unit.conditional_regions=conditional_regions;
        recover_editor=diagnostics.recovery && diagnostics.recovery->module==unit.identity;
        auto before = diagnostics.size();
        Function application; application.token={TokenKind::name,"$application",0}; application.result=Type::i32; application.visibility="private";
        while (peek().kind != TokenKind::end) {
            if (diagnostics.full()) { unit.parse_failed = true; break; }
            auto start = p;nodes=0;generic_names.clear();const auto use_begin=type_uses.size(),array_begin=arrays.size();
            auto template_syntax=[&](auto& declaration) {declaration.type_uses.assign(type_uses.begin()+use_begin,type_uses.end());type_uses.resize(use_begin);auto n=std::size_t{0};for(auto& a:arrays)if(n++>=array_begin)a.template_syntax=true;};
            try {
            if (accept("<newline>")) continue;
            if (peek().kind == TokenKind::invalid) throw Poison{};
            if (accept("native")) {
                auto provider=take();
                if(provider.kind!=TokenKind::string || !unit.c_provider.empty() ||
                   !unit.imports.empty() || !unit.functions.empty() || !unit.structures.empty() || !unit.globals.empty() || !unit.constants.empty())
                    error(source,provider,Code::syntax,"A native declaration module starts with one literal provider identity.");
                unit.c_provider=provider.decoded;newline();continue;
            }
            if (at("while")) error(source, peek(), Code::while_loop, "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:' inside a function.");
            if (accept("use")) {
                if (!unit.functions.empty() || !unit.constants.empty() || !unit.globals.empty() || !unit.structures.empty() || unit.application_body) error(source, peek(), Code::syntax, "Imports must precede constant and function declarations.", "Move use declarations to the start of this file.");
                Import import; import.path = take();
                if(import.path.kind!=TokenKind::path) {
                    auto segment=[&](const Token& t) {return !t.text.empty() && letter(t.text[0]) && std::all_of(t.text.begin(),t.text.end(),[](char c){return letter(c)||digit(c);});};
                    if(!segment(import.path))error(source,import.path,Code::import_path,"Expected a standard module or quoted project path.");
                    import.path.kind=TokenKind::name;
                    while(accept("/")) {auto next=take();if(!segment(next))error(source,next,Code::import_path,"Expected a standard module segment after /.");import.path.text+="/"+next.text;}
                    import.path.source_width=peek().offset-import.path.offset;
                }
                import.alias = import.path;
                if (accept("as")) { import.alias = name(); import.explicit_alias = true; }
                newline(); unit.imports.push_back(std::move(import)); continue;
            }
            Function f;
            if (accept("private")) { f.file_private = true; f.visibility = "private"; }
            else if (!accept("public") && accept("internal")) f.visibility = "internal"; // All graph files belong to this project.
            f.compile_time=accept("compile");
            if(f.compile_time && !at("func"))error(source,peek(),Code::syntax,"Expected compile func; compile is not a modifier for other declarations.");
            const bool static_declaration=accept("static");
            if(static_declaration && !at("func") && !at("async"))error(source,peek(),Code::syntax,"Use static func for a type-level method; static fields belong inside a struct.");
            if (at("var")) {
                if(static_declaration)error(source,peek(),Code::syntax,"Static fields belong inside a struct declaration.");
                auto begin = p;
                auto declaration = statement();
                if(declaration.kind==Statement::Kind::destructure)
                    error(source,declaration.token,Code::unsupported_type,"Multiple-return destructuring requires a local declaration.");
                Global global; global.token = declaration.token; global.visibility = f.visibility;
                for (auto n = begin; n < p; ++n) global.syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
                f.token = global.token; f.token.source_width = global.token.text.size(); f.token.text = "$global$" + global.token.text;
                f.global_initializer = true; f.visibility = "private";
                declaration.token.text = declaration.name = "$value$" + global.token.text;
                f.body.push_back(std::move(declaration));
                unit.functions.push_back(std::move(f)); global.initializer = &unit.functions.back();
                unit.globals.push_back(std::move(global)); continue;
            }
            if (at("const")) {
                auto declaration = statement();
                if(declaration.kind==Statement::Kind::destructure)
                    error(source,declaration.token,Code::unsupported_type,"Multiple-return destructuring requires a local declaration.");
                unit.constants.push_back({std::move(declaration), f.file_private, f.visibility,0,Type::none,0,{}}); continue;
            }
            if(at("enum") || (at("flags") && peek(1).text=="enum")) {
                const bool flags=accept("flags");need("enum");
                Structure enumeration;enumeration.token=name();enumeration.visibility=f.visibility;
                enumeration.definition=std::make_shared<ir::ValueType>();enumeration.definition->name=enumeration.token.text;
                enumeration.generics=generic_parameters();enumeration.definition->enum_flags=flags;
                if(accept("(")) {
                    auto backing=take();auto type=numeric::primitive(backing.text);
                    if(!numeric::integer(type))error(source,backing,Code::unsupported_type,"A numeric enum requires a fixed-width integer backing.");
                    enumeration.definition->enum_backing=type.kind;
                    need(")");
                } else if(flags) {
                    enumeration.definition->enum_backing=Type::u32;
                } else {
                    Statement tag(Statement::Kind::variable,enumeration.token);tag.name="$tag";tag.annotation=Type::i32;enumeration.fields.push_back(std::move(tag));
                }
                need(":");newline();need("<indent>");std::set<std::string> cases;std::map<std::uint64_t,Token> discriminants;
                while(!at("<dedent>") && !at("<end of file>")) {
                    std::string visibility="public";
                    if(at("private") || at("internal") || at("public"))visibility=take().text;
                    if(at("operator") && !enumeration.definition->enum_backing){operator_declaration(enumeration,visibility);continue;}
                    if(visibility!="public")error(source,peek(),Code::syntax,"Enum variant visibility modifiers are not supported.");
                    auto token=name();enumeration.case_tokens.push_back(token);if(!cases.insert(token.text).second)error(source,token,Code::duplicate,"Duplicate enum variant.");
                    if(enumeration.definition->enum_backing) {
                        if(!letter(token.text.front()))error(source,token,Code::identifier,"Numeric enum member names are lowercase.");
                        std::unique_ptr<Expr> value;
                        if(accept("="))value=expression();
                        else if(!flags)error(source,peek(),Code::syntax,"A numeric enum member requires = <constant expression>.");
                        enumeration.numeric_expressions.push_back(std::move(value));
                        enumeration.definition->numeric_members.push_back({token.text,0});newline();continue;
                    }
                    ir::SumCase variant{token.text,static_cast<unsigned>(enumeration.fields.size()),0};
                    if(accept("(")) {if(!at(")"))do {
                        Statement field(Statement::Kind::variable,name());field.name="$"+token.text+"$"+field.token.text;need(":");field.annotation=type();enumeration.fields.push_back(std::move(field));++variant.count;
                    }while(accept(","));
                    need(")");}
                    enumeration.definition->cases.push_back(variant);newline();
                }
                need("<dedent>");if(cases.empty())error(source,enumeration.token,Code::syntax,"An enum requires at least one variant.");
                enumeration.source_end=peek().offset;
                if(!instantiating && !enumeration.generics.empty()) {
                    template_syntax(enumeration);enumeration.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_structures.push_back(std::move(enumeration));
                } else {enumeration.generics.clear();unit.functions.splice(unit.functions.end(),enumeration.operators);unit.structures.push_back(std::move(enumeration));}
                continue;
            }
            if (accept("interface")) {
                Structure structure; structure.token=name(); structure.visibility=f.visibility;
                structure.definition=std::make_shared<ir::ValueType>(); structure.definition->name=structure.token.text; structure.definition->interface_type=true;structure.generics=generic_parameters();
                need(":");newline();need("<indent>");unsigned count=0;
                while(!at("<dedent>") && !at("<end of file>")) {
                    if(peek().text=="async")error(source,peek(),Code::unsupported_type,"Async interface methods require a reviewed calling and dispatch contract.");
                    need("func");Function requirement;requirement.interface_requirement=true;
                    requirement.visibility=f.visibility;requirement.file_private=f.file_private;
                    requirement.token=at("string")?take():name();
                    if(requirement.token.text=="init" || requirement.token.text=="deinit") error(source,requirement.token,Code::initializer_deferred,"Lifecycle methods are not interface requirements.");
                    requirement.receiver=Parameter{structure.token,Type(structure.definition,Type::interface_ref)};
                    parameters(requirement,false);newline();structure.requirements.push_back(std::move(requirement));++count;
                }
                if(!count) error(source,structure.token,Code::syntax,"An interface requires at least one method; marker interfaces are deferred.");
                need("<dedent>");structure.source_end=peek().offset;
                if(!instantiating && !structure.generics.empty()) {
                    template_syntax(structure);structure.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_structures.push_back(std::move(structure));
                } else {structure.generics.clear();unit.functions.splice(unit.functions.end(),structure.requirements);unit.structures.push_back(std::move(structure));}
                continue;
            }
            bool value_declaration = at("value") && peek(1).text=="struct"; if(value_declaration)take();
            if (value_declaration || at("struct")) {
                need("struct"); Structure structure; structure.token = name(); structure.visibility = f.visibility;
                structure.definition = std::make_shared<ir::ValueType>(); structure.definition->name = structure.token.text; structure.definition->managed = !value_declaration;
                structure.generics=generic_parameters();
                if (accept("extends")) { structure.base_token=peek(); structure.base_type=type(); }
                need(":"); newline();
                bool fields=accept("<indent>");
                if (!fields && !structure.definition->managed) need("<indent>");
                while (fields && !at("<dedent>") && !at("<end of file>")) {
                    std::string visibility="public";
                    if(at("private") || at("internal") || at("public"))visibility=take().text;
                    if(at("operator")){operator_declaration(structure,visibility);continue;}
                    if((at("const") || at("async") || at("virtual") || at("override") || at("static") || at("func")) && peek(1).text=="operator")error(source,peek(),Code::syntax,"Operators permit only one optional visibility modifier; const belongs on each operand.");
                    const bool static_field=accept("static");
                    bool readonly=accept("const");
                    if(at("private") || at("internal") || at("public") || at("const"))error(source,peek(),Code::syntax,"Duplicate, conflicting or out-of-order field modifiers; use visibility then const.");
                    bool composition = at("using") && peek(1).text != ":";
                    if (composition) take();
                    bool event=accept("event");auto start = p;
                    nodes=0;
                    Statement field(Statement::Kind::variable, name()); field.name = field.token.text;
                    field.visibility=visibility;field.readonly=readonly;
                    if(event && readonly)error(source,field.token,Code::readonly,"An event cannot be a const field.");
                    field.using_field = composition;field.event=event;
                    if (composition && !at(":")) {
                        p = start; field.annotation = type();
                        field.token = tokens[p - (field.annotation->nullable ? 2 : 1)];
                        field.name = field.token.text;
                    } else if(accept(":"))field.annotation=type();
                    else if(!static_field)error(source,peek(),Code::syntax,"An instance field requires a type annotation.");
                    auto begin = p; nodes = 0;
                    if (accept("=")){if(event)error(source,field.token,Code::initializer,"Events start empty and cannot declare an initializer.");field.expression = expression();}
                    for (auto n = begin; n < p; ++n) field.constant_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
                    if(static_field && (composition || event))error(source,field.token,Code::syntax,"Static fields cannot be using fields or events.");
                    if(static_field && readonly && !field.expression)error(source,field.token,Code::initializer,"A static const requires an initializer.");
                    newline();field.source_end=peek().offset;
                    (static_field?structure.static_fields:structure.fields).push_back(std::move(field));
                }
                if (fields) need("<dedent>");
                structure.source_end=peek().offset;
                if(!instantiating && !structure.generics.empty()) {
                    template_syntax(structure);structure.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_structures.push_back(std::move(structure));
                } else {structure.generics.clear();unit.functions.splice(unit.functions.end(),structure.operators);unit.structures.push_back(std::move(structure));}
                continue;
            }
            if(accept("property")) {
                need("(");auto receiver=name();auto receiver_begin=p;auto receiver_type=type();auto receiver_end=p;need(")");auto property=name();need(":");auto type_begin=p;auto property_type=type();auto type_end=p;
                need(":");newline();need("<indent>");bool getter=false,setter=false;
                while(!at("<dedent>") && !at("<end of file>")) {
                    Function accessor;accessor.file_private=f.file_private;accessor.visibility=f.visibility;
                    accessor.receiver=Parameter{receiver,receiver_type};accessor.token=property;
                    auto begin=p;
                    if(accept("get")) {if(getter)error(source,property,Code::duplicate,"Duplicate property getter.");getter=true;accessor.accessor=1;accessor.result=property_type;}
                    else if(accept("set")) {
                        if(setter)error(source,property,Code::duplicate,"Duplicate property setter.");setter=true;accessor.accessor=2;accessor.token.text="$set$"+property.text;accessor.token.source_width=property.text.size();
                        need("(");auto value=name();need(")");accessor.parameters.push_back({value,property_type});
                    } else error(source,peek(),Code::syntax,"Expected get or set in a property.");
                    auto body_begin=p;{FunctionScope scope(parsing_function,accessor);accessor.body=block();}
                    for(auto n=body_begin;n<p;++n)accessor.body_syntax+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;
                    accessor.source_end=peek().offset;
                    accessor.parse_failed=diagnostics.within(source,tokens[begin].offset,peek().offset);
                    // Generic accessors re-enter the same ordinary function parser on
                    // specialization, with the original accessor role retained.
                    bool generic_receiver=std::any_of(applications.begin(),applications.end(),[&](const auto& a){return a.definition==receiver_type.definition;});
                    if(!instantiating && generic_receiver) {
                        accessor.syntax.push_back({TokenKind::name,"func",property.offset});
                        accessor.syntax.push_back({TokenKind::symbol,"(",receiver.offset});accessor.syntax.push_back(receiver);
                        accessor.syntax.insert(accessor.syntax.end(),tokens.begin()+receiver_begin,tokens.begin()+receiver_end);
                        accessor.syntax.push_back({TokenKind::symbol,")",receiver.offset});accessor.syntax.push_back(accessor.token);
                        accessor.syntax.push_back({TokenKind::symbol,"(",property.offset});
                        if(accessor.accessor==2) {accessor.syntax.push_back(accessor.parameters[0].token);accessor.syntax.push_back({TokenKind::symbol,":",property.offset});accessor.syntax.insert(accessor.syntax.end(),tokens.begin()+type_begin,tokens.begin()+type_end);}
                        accessor.syntax.push_back({TokenKind::symbol,")",property.offset});
                        if(accessor.accessor==1) {accessor.syntax.push_back({TokenKind::symbol,"->",property.offset});accessor.syntax.insert(accessor.syntax.end(),tokens.begin()+type_begin,tokens.begin()+type_end);}
                        accessor.syntax.insert(accessor.syntax.end(),tokens.begin()+body_begin,tokens.begin()+p);
                        template_syntax(accessor);unit.generic_functions.push_back(std::move(accessor));
                    } else {accessor.substitutions=substitutions;unit.functions.push_back(std::move(accessor));}
                }
                need("<dedent>");if(!getter)error(source,property,Code::syntax,"A property requires a getter.");continue;
            }
            f.generator=at("generator") && (peek(1).text=="func" || peek(1).text=="async");
            if(f.generator)take();
            if(!at("func") && !at("async") && !at("virtual") && !at("override")) {
                unit.application_body=true;
                FunctionScope scope(parsing_function,application);
                application.body.push_back(statement());
                for(auto n=start;n<p;++n)application.body_syntax+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;
                continue;
            }
            f.asynchronous=accept("async");
            if(f.asynchronous && at("generator") && peek(1).text=="func"){take();f.generator=true;}
            f.virtual_method=accept("virtual");
            f.overriding=!f.virtual_method && accept("override");
            need("func");
            if(static_declaration) {
                auto site=peek();auto owner=type(true);need(".");f.static_owner=Parameter{site,owner};
            } else if (accept("(")) {
                auto receiver = name(); auto receiver_type = type(); need(")");
                f.receiver = Parameter{receiver,receiver_type};
            }
            f.token = f.receiver && at("string") ? take() : name();
            f.generics=generic_parameters();
            parameters(f);
            auto body_begin = p;{
                FunctionScope scope(parsing_function,f);
                // A qualified annotation probe can precede the declaration's
                // colon (or its body). Retain that header only for completion;
                // ordinary parsing still owns the missing-colon diagnostic.
                const auto* probe=diagnostics.editor;
                const bool annotation_probe=repair_available() && probe && (at("<newline>") || at(":")) &&
                    std::any_of(type_uses.begin()+use_begin,type_uses.end(),[&](const auto& use) {
                        return use.qualifier && use.token.text=="_void_editor_cursor_" && use.token.offset==probe->offset;
                    });
                if(annotation_probe) {
                    Depth guard(depth,source,peek(),64);
                    diagnostics.recovery->record();accept(":");newline();
                    if(at("<indent>"))f.body=block_body();
                } else f.body=block();
            }
            for (auto n = body_begin; n < p; ++n) f.body_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
            f.source_end=peek().offset;
            f.parse_failed = diagnostics.within(source, f.token.offset, peek().offset);
            const auto* owner=f.receiver?&*f.receiver:f.static_owner?&*f.static_owner:nullptr;
            const bool generic_receiver=owner && std::any_of(applications.begin(),applications.end(),[&](const auto& a){return a.definition==owner->type.definition;});
            if(!instantiating && (!f.generics.empty() || generic_receiver)) {
                template_syntax(f);f.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_functions.push_back(std::move(f));
            } else {f.generics.clear();f.substitutions=substitutions;unit.functions.push_back(std::move(f));}
            } catch (const CompilationError& e) {
                diagnostics.add(e); unit.parse_failed = true;
                auto declaration = start;
                if (tokens[declaration].text == "private" || tokens[declaration].text == "public" || tokens[declaration].text == "internal") ++declaration;
                if (declaration + 1 < tokens.size() && (tokens[declaration].text == "func" || tokens[declaration].text == "const" || tokens[declaration].text == "var") && tokens[declaration + 1].kind == TokenKind::name && !reserved.contains(tokens[declaration + 1].text))
                    unit.invalid_declarations.insert(tokens[declaration + 1].text);
                synchronize(start, true);
            }
            catch (const Poison&) { unit.parse_failed = true; synchronize(start, true); }
            if (p == start && peek().kind != TokenKind::end) take();
        }
        if(unit.application_body) {
            Statement end(Statement::Kind::returned,application.token);end.expression=std::make_unique<Expr>(Expr::Kind::literal,Token{TokenKind::number,"0",0});end.expression->literal_text="0";application.body.push_back(std::move(end));
            application.source_end=source.text().size();unit.functions.push_back(std::move(application));
        }
        if(!instantiating) {
            for(const auto& f:unit.functions)if(!f.anonymous && f.token.text!="$application")validate_documentation(source,f,diagnostics);
            for(const auto& f:unit.generic_functions)validate_documentation(source,f,diagnostics);
            for(const auto& structure:unit.generic_structures) {
                for(const auto& f:structure.operators)validate_documentation(source,f,diagnostics);
                for(const auto& f:structure.requirements)validate_documentation(source,f,diagnostics);
            }
        }
        unit.parse_failed |= diagnostics.size() != before;
        unit.type_uses = std::move(type_uses);
        unit.function_types=std::move(function_types);unit.results=std::move(results);
        unit.arrays = std::move(arrays);unit.applications=std::move(applications);
    }
};
}

void parse_unit(Unit& unit, Diagnostics& diagnostics, std::string_view module,
                const BuildConditions& conditions, std::vector<Token>* syntax) {
    Parser parser(unit.source, diagnostics, module, conditions);
    parser.parse(unit);
    if (syntax) *syntax = parser.take_syntax_tokens();
}

void parse_specialization(Unit& unit, Diagnostics& diagnostics, std::vector<Token> syntax,
                          std::map<std::string, Type> bindings) {
    Parser(unit.source, diagnostics, std::move(syntax), std::move(bindings)).parse(unit);
}

Type parse_constraint(Unit& unit, Diagnostics& diagnostics, const std::vector<Token>& syntax,
                      const std::map<std::string, Type>& bindings) {
    return Parser(unit.source, diagnostics, syntax, bindings).parse_type(unit);
}
}
