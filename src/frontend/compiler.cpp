#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/numeric.hpp"
#include "void/reference_capabilities.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <functional>
#include <bit>
#include <iterator>
#include <list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
// These handler boundaries keep unrelated temporaries out of recursive semantic
// frames even in Release. The existing depth budget must fit a 1 MiB stack.
#if defined(_MSC_VER)
#define VOID_SEMANTIC_HANDLER __declspec(noinline)
#else
#define VOID_SEMANTIC_HANDLER __attribute__((noinline))
#endif
namespace vd {
namespace {
using Type = ir::Type;
enum class TokenKind { invalid, name, number, path, string, symbol, newline, indent, dedent, end };
struct Token { TokenKind kind; std::string text; std::size_t offset; std::string decoded = {}; std::optional<std::size_t> source_width = {}; };
using Code = DiagnosticCode;
SourceSpan span(const SourceFile& source, const Token& token) {
    auto begin = std::min(token.offset, source.text().size());
    auto length = token.kind == TokenKind::path ? token.text.size() + 2 :
        (token.kind == TokenKind::name || token.kind == TokenKind::number || token.kind == TokenKind::string || token.kind == TokenKind::symbol) ? token.text.size() : 0;
    if (token.text.empty() && begin < source.text().size()) {
        if (token.kind == TokenKind::symbol && source.text()[begin] != '\r' && source.text()[begin] != '\n') {
            length = 1;
            while (begin + length < source.text().size() && (static_cast<unsigned char>(source.text()[begin + length]) & 0xc0) == 0x80) ++length;
        } else if (token.kind == TokenKind::number || token.kind == TokenKind::path) {
            auto end = source.text().find_first_of(token.kind == TokenKind::path ? "\r\n" : " ():,\r\n", begin);
            length = (end == std::string::npos ? source.text().size() : end) - begin;
        }
    }
    if(token.source_width)length=*token.source_width;
    return {std::make_shared<const SourceFile>(source), {}, begin, std::min(begin + length, source.text().size())};
}
[[noreturn]] void error(const SourceFile& source, const Token& token, Code code, const std::string& message,
                        const std::string& hint = {}, std::vector<RelatedLocation> related = {},
                        std::string expected = {}, std::string received = {}) {
    Diagnostic d{code}; d.description = message; d.primary = span(source, token);
    d.hint = hint; d.related = std::move(related);
    d.expected_type = std::move(expected); d.received_type = std::move(received);
    throw CompilationError({std::move(d)});
}
// A poisoned dependency has already produced a primary error. It is never a value.
struct Poison {};
const Type error_type = Type::invalid; // AST-only; the lowering gate excludes it.
class Diagnostics {
    std::vector<Diagnostic> items;
    std::map<std::string, std::pair<std::string, std::shared_ptr<const SourceFile>>> snapshots;
    std::set<std::tuple<std::string, std::size_t, std::size_t, int, std::string>> seen;
    bool suppressed = false;
    unsigned expression_depth = 0, specialization_depth = 0;
    std::vector<RelatedLocation> context;
public:
    struct Context {
        Diagnostics& owner;std::size_t previous;unsigned previous_depth;
        Context(Diagnostics& d,const std::vector<RelatedLocation>& locations,std::optional<unsigned> depth={}):owner(d),previous(d.context.size()),previous_depth(d.specialization_depth){d.context.insert(d.context.end(),locations.begin(),locations.end());if(depth)d.specialization_depth=*depth;}
        ~Context(){owner.context.resize(previous);owner.specialization_depth=previous_depth;}
    };
    void bind(const SourceFile& source, const std::string& identity) {
        snapshots.insert_or_assign(source.path(), std::make_pair(identity, std::make_shared<const SourceFile>(source)));
    }
    void add(const CompilationError& failure) {
        for (auto d : failure.diagnostics()) {
            d.related.insert(d.related.end(),context.begin(),context.end());
            auto own = [&](SourceSpan& location) {
                auto found = snapshots.find(location.source->path());
                if (found != snapshots.end() && (&found->second.second->text() == &location.source->text() || found->second.second->text() == location.source->text())) {
                    location.module_identity = found->second.first; location.source = found->second.second;
                }
            };
            own(d.primary); for (auto& related : d.related) own(related.span);
            std::set<std::tuple<std::string, std::size_t, std::size_t, std::string>> related_seen;
            std::erase_if(d.related, [&](const auto& related) {
                return !related_seen.emplace(related.span.source->path(), related.span.begin, related.span.end, related.description).second;
            });
            auto key = std::make_tuple(d.primary.source->path(), d.primary.begin, d.primary.end, static_cast<int>(d.code), d.description);
            if (seen.contains(key)) continue;
            if (seen.size() == 100) { suppressed = true; continue; }
            seen.insert(std::move(key));
            items.push_back(std::move(d));
        }
    }
    void warn(const SourceFile& source,const Token& token,Code code,const std::string& message) {
        if(items.size()-seen.size()>=100)return;
        auto location=span(source,token);if(auto found=snapshots.find(source.path());found!=snapshots.end()){location.module_identity=found->second.first;location.source=found->second.second;}
        for(const auto& item:items)if(item.severity==Severity::warning && item.primary.source->path()==source.path() && item.primary.begin==location.begin && item.code==code)return;
        Diagnostic d{code};d.severity=Severity::warning;d.primary=std::move(location);d.description=message;d.related=context;items.push_back(std::move(d));
    }
    std::vector<Diagnostic> warnings() const {std::vector<Diagnostic> result;for(const auto& d:items)if(d.severity==Severity::warning)result.push_back(d);return result;}
    bool failed() const { return std::any_of(items.begin(), items.end(), [](const auto& d) { return d.severity == Severity::error; }); }
    unsigned& semantic_depth() { return expression_depth; }
    unsigned current_specialization_depth() const {return specialization_depth;}
    bool full() { if (seen.size() < 100) return false; suppressed = true; return true; }
    bool within(const SourceFile& source, std::size_t begin, std::size_t end) const {
        return std::any_of(items.begin(), items.end(), [&](const auto& d) {
            return d.primary.source->path() == source.path() && d.primary.begin >= begin && (d.primary.begin < end || (end == source.text().size() && d.primary.begin == end));
        });
    }
    std::size_t size() const { return seen.size(); }
    void finish() {
        if (!failed()) return;
        // VFS owner, byte offset, then code: independent of discovery and host enumeration.
        std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
            return std::tie(a.primary.module_identity, a.primary.begin, a.code) < std::tie(b.primary.module_identity, b.primary.begin, b.code);
        });
        if (suppressed) { auto d = items.back();d.severity=Severity::error; d.code = Code::limit; d.description = "Diagnostic limit of 100 reached; further diagnostic analysis stopped."; d.related.clear(); d.expected_type.clear(); d.received_type.clear(); d.hint = "Fix the reported errors and check again."; items.push_back(std::move(d)); }
        throw CompilationError(std::move(items));
    }
};
class Depth {
    unsigned& depth;
public:
    Depth(unsigned& d, const SourceFile& source, const Token& token, unsigned limit = 128) : depth(d) {
        if (depth >= limit) error(source, token, Code::nesting, "Source nesting limit exceeded.", "Split deeply nested expressions or blocks into smaller functions.");
        ++depth;
    }
    ~Depth() { --depth; }
};
bool letter(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
const std::set<std::string> reserved = {"func", "var", "return", "if", "else", "for", "break", "continue", "pass",
    "async", "await", "spawn", "event", "defer", "with", "enum", "is", "match", "property", "and", "or", "not", "true", "false", "null", "while", "const", "use", "struct", "in", "reverse", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "string", "as", "private", "public", "internal"};
#include "strings.inc"
std::vector<Token> lex(const SourceFile& source, Diagnostics& diagnostics) {
    const auto& text = source.text();
    std::vector<Token> tokens;
    std::vector<std::size_t> indents{0};
    std::size_t p = text.starts_with("\xef\xbb\xbf") ? 3 : 0;
    bool beginning = true;unsigned braces=0;std::size_t line_tokens=0;
    struct LiteralBlock {unsigned braces;std::size_t parent,indent;};std::vector<LiteralBlock> literal_blocks;
    auto emit = [&](TokenKind kind, std::string value, std::size_t offset) { tokens.push_back({kind, std::move(value), offset}); };
    while (p < text.size()) {
        if (diagnostics.full()) break;
        try {
        if(beginning && !literal_blocks.empty() && !braces){
            auto end=p;while(end<text.size() && text[end]==' ')++end;
            const bool blank=end==text.size() || text[end]=='#' || text[end]=='\r' || text[end]=='\n';
            if(!blank && end-p<=literal_blocks.back().indent){auto block=literal_blocks.back();literal_blocks.pop_back();while(indents.size()>block.parent+1){indents.pop_back();emit(TokenKind::dedent,"<dedent>",end);}indents.pop_back();braces=block.braces;}
        }
        if(beginning && braces){while(p<text.size() && text[p]==' ')++p;beginning=false;}
        if (beginning) {
            auto start = p;
            while (p < text.size() && text[p] == ' ') ++p;
            if (p < text.size() && text[p] == '\t') error(source, {TokenKind::symbol, "", p}, Code::indentation, "Tabs are not supported in source indentation.", "Use spaces consistently (four spaces recommended).");
            bool blank = p == text.size() || text[p] == '#' || text[p] == '\r' || text[p] == '\n';
            if (!blank) {
                auto width = p - start;
                if (width > indents.back()) { indents.push_back(width); emit(TokenKind::indent, "<indent>", p); }
                else if (width < indents.back()) {
                    while (width < indents.back()) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
                    if (width != indents.back()) error(source, {TokenKind::symbol, "", p}, Code::indentation, "Indentation does not match an enclosing block.", "Align this statement with an earlier block level.");
                }
            }
            beginning = false;
        }
        if (p == text.size()) break;
        auto start = p;
        char c = text[p];
        if (c == ' ') { ++p; continue; }
        if (c == '#') { while (p < text.size() && text[p] != '\r' && text[p] != '\n') ++p; continue; }
        if (c == '\r' || c == '\n') {
            ++p; if (c == '\r' && p < text.size() && text[p] == '\n') ++p;
            if(braces && tokens.size()>line_tokens && tokens.back().text==":"){
                for(auto n=line_tokens;n<tokens.size();++n)if(tokens[n].text=="match" || (tokens[n].text=="func" && n+1<tokens.size() && tokens[n+1].text=="(")){
                    auto line=tokens[n].offset;while(line && text[line-1]!='\n' && text[line-1]!='\r')--line;std::size_t width=0;while(line+width<text.size() && text[line+width]==' ')++width;
                    literal_blocks.push_back({braces,indents.size(),width});indents.push_back(width);braces=0;break;
                }
            }
            if (!braces && !tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", start);
            line_tokens=tokens.size();beginning = true; continue;
        }
        if (c == '"') {
            if (tokens.empty() || tokens.back().text!="use") { tokens.push_back(string_token(source,p)); continue; }
            ++p;
            while (p < text.size() && text[p] != '"' && text[p] != '\r' && text[p] != '\n') ++p;
            if (p == text.size() || text[p] != '"') error(source, {TokenKind::path, "", start}, Code::import_quote, "Unterminated import path.", "Close the path with a double quote on this line.");
            emit(TokenKind::path, text.substr(start + 1, p - start - 1), start); ++p; continue;
        }
        if ((c=='O' && text.substr(p,2)=="Ok") || (c=='E' && text.substr(p,3)=="Err")) {
            const auto width=c=='O'?2u:3u;
            if(p+width<text.size() && (letter(text[p+width]) || digit(text[p+width])))error(source,{TokenKind::symbol,"",p},Code::identifier,"Only Ok and Err use uppercase spelling.");
            p+=width;emit(TokenKind::name,text.substr(start,width),start);continue;
        }
        if (letter(c)) {
            while (p < text.size() && (letter(text[p]) || digit(text[p]))) ++p;
            emit(TokenKind::name, text.substr(start, p - start), start); continue;
        }
        if (digit(c)) {
            while (p < text.size() && digit(text[p])) ++p;
            if (p + 1 < text.size() && text[p] == '.' && digit(text[p + 1])) {
                ++p; while (p < text.size() && digit(text[p])) ++p;
            }
            if (p < text.size() && (text[p] == 'f' || text[p] == 'd')) ++p;
            if (p < text.size() && (letter(text[p]) || (text[p] == '.' && text.substr(p,2)!="..") || (text[p] >= 'A' && text[p] <= 'Z')))
                error(source, {TokenKind::number, "", start}, Code::numeric_literal, "Unsupported numeric literal.", "Use decimal digits, an optional fractional part, and optional f or d suffix; separators and exponents are not implemented.");
            emit(TokenKind::number, text.substr(start, p - start), start); continue;
        }
        if(text.substr(p,2)=="..") {auto op=text.substr(p,3)=="..<" ? "..<" : "..";p+=std::string_view(op).size();emit(TokenKind::symbol,op,start);continue;}
        std::string pair = text.substr(p, 2);
        if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" || pair == "+=" ||
            pair == "-=" || pair == "*=" || pair == "/=" || pair == "%=" || pair == "++" || pair == "--") {
            p += 2; emit(TokenKind::symbol, pair, start); continue;
        }
        if (std::string("{}[]():,.+-*/%=<>?").find(c) != std::string::npos) { if(c=='{' && (tokens.empty() || tokens.back().text!="defer"))++braces;else if(c=='}' && braces)--braces;++p; emit(TokenKind::symbol, std::string(1, c), start); continue; }
        error(source, {TokenKind::symbol, "", p}, Code::character, "Unexpected character in source.",
              c == '\t' ? "Use spaces instead of tabs." : "Use supported expressions and lowercase ASCII identifiers; Unicode identifiers and collection/indexing syntax are deferred.");
        } catch (const CompilationError& e) {
            diagnostics.add(e);
            auto line = source.location(p).line;
            while (!tokens.empty() && tokens.back().kind != TokenKind::indent && tokens.back().kind != TokenKind::dedent && tokens.back().kind != TokenKind::newline && source.location(tokens.back().offset).line == line) tokens.pop_back();
            emit(TokenKind::invalid, "<invalid source line>", p);
            // Consume the bad line, leaving its newline to the ordinary lexer.
            while (p < text.size() && text[p] != '\r' && text[p] != '\n') ++p;
            beginning = false;
        }
    }
    if (!tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", p);
    while (indents.size() > 1) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
    emit(TokenKind::end, "<end of file>", p);
    return tokens;
}
struct Expr;
struct Pattern {
    Pattern() = default;
    // vectors of owning nodes can appear copyable to library traits even though
    // instantiating their copy bodies fails. Make relocation move-only explicitly.
    Pattern(const Pattern&) = delete;
    Pattern& operator=(const Pattern&) = delete;
    Pattern(Pattern&&) = default;
    Pattern& operator=(Pattern&&) = default;
    enum class Kind { wildcard, capture, value, range, constructor, list, dictionary } kind=Kind::wildcard;
    Token token{TokenKind::name,"",0};std::string spelling;
    std::vector<Pattern> children;std::vector<std::unique_ptr<Expr>> values;std::vector<Token> fields;
    std::vector<unsigned> indexes;
    std::vector<Type> type_arguments;
    Type type=Type::none,narrowed=Type::none;unsigned slot=0,tag=0;bool rest=false,exclusive=false,nullable=false,annotated_type=false;
};
struct Expr {
    bool event=false, asynchronous_call=false;
    std::string event_owner,native_symbol;
    Type environment=Type::none;
    enum class Kind { spawned, task_cancel, awaited, lambda, capture_reference, function_reference, function_call, function_null, test, match, sum, propagate, literal, name, call, unary, binary, conversion, field, construction, method_call, base_initialization, array_literal, array_index, array_default, array_slice, dictionary_literal } kind;
    Token token;
    Token qualifier_token{TokenKind::name, "", 0};
    std::string op, qualifier, target, source_target;
    std::vector<Pattern> patterns;
    std::vector<Type> type_arguments;
    std::uint64_t value = 0;
    ir::ConstantValue aggregate_value;
    std::vector<Token> names;
    std::vector<std::unique_ptr<Expr>> omitted;
    std::string initializer;
    std::vector<unsigned> field_indexes;
    std::string literal_text;
    std::vector<std::unique_ptr<Expr>> parts;
    Type type = Type::none;
    unsigned slot = 0;
    bool receiver_addressable = false, receiver_readonly = false, implicit = false;
    std::string virtual_slot;
    bool super_call = false, interface_call = false, property = false, receiver_writes = false, selected_receiver = false, setter_mutates = false;
    std::string setter;
    Expr(Kind k, Token t) : kind(k), token(std::move(t)) {}
    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;
    Expr(Expr&&) = default;
    Expr& operator=(Expr&&) = default;
};
struct Statement {
    enum class Kind { scoped, deferred, match, variable, assignment, expression, returned, conditional, loop, broken, continued, no_op } kind;
    Token token;
    std::optional<Pattern> pattern;
    std::string name, op, constant_syntax;
    std::optional<Type> annotation;
    std::unique_ptr<Expr> expression;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> selection;
    std::unique_ptr<Expr> range_end;
    Token binder{TokenKind::name,"",0}, index_binder{TokenKind::name,"",0};
    unsigned index_slot=0;
    bool reverse=false, exclusive=false;
    std::vector<Token> fields;
    std::vector<bool> implicit_fields;
    std::vector<unsigned> field_indexes;
    Type assigned_type = Type::none;
    std::vector<Statement> body, otherwise;
    unsigned slot = 0, flow = 0;
    bool readonly = false, invalid = false, using_field = false, event = false, task_group = false;
    Statement(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Parameter {
    Token token; Type type; unsigned slot = 0;
    std::unique_ptr<Expr> default_expression = {};
    std::optional<ir::ConstantValue> default_value = {};
    std::string default_syntax = {};
    std::size_t default_begin = 0, default_end = 0;
};
struct ConstructionContext {
    std::set<unsigned> parameters;
    std::map<unsigned,unsigned> aliases;
    std::map<const Expr*,std::set<unsigned>> calls;
};
struct GenericParameter { Token token; std::optional<Type> constraint; };
struct Function {
    struct Capture {unsigned outer_slot;Type cell;Token token;bool snapshot=false;};
    bool anonymous=false, deferred=false, bound_receiver=false, asynchronous=false;
    Type task=Type::none;
    unsigned binding_ordinal=0;
    Type cleanup_type=Type::none;
    std::list<Function> lambdas;
    std::vector<Capture> captures;
    std::map<unsigned,Type> captured_locals;
    std::map<unsigned,unsigned> capture_inputs;
    std::vector<GenericParameter> generics;
    std::vector<Token> syntax;
    std::vector<std::pair<Token,Type>> type_uses;
    std::map<std::string,Type> substitutions;
    std::map<std::string,std::string> substitution_sources;
    std::string generic_origin, concrete_name, concrete_method_name, display_name;
    std::vector<RelatedLocation> instantiation_context;
    unsigned specialization_depth=0;
    std::string native_symbol;
    bool analyzed=false, prepared=false, defaults_prepared=false, initializer_prepared=false;
    std::map<std::set<unsigned>,ConstructionContext> construction_contexts;
    std::string construction_contract;
    Token token;
    bool file_private = false, parse_failed = false;
    std::string symbol, visibility = "public";
    std::vector<Parameter> parameters;
    std::optional<Parameter> receiver;
    bool receiver_mutates = false, initializer = false;
    unsigned accessor = 0; // 1 getter, 2 setter; bodies use the ordinary method checker.
    bool virtual_method = false, overriding = false, interface_requirement = false;
    std::string virtual_slot;
    std::unique_ptr<Expr> base_prelude;
    std::string body_syntax, initialization_contract, capability_contract;
    ir::ReferenceEffects reference_effects, virtual_effects;
    bool virtual_mutates=false;
    std::vector<std::unique_ptr<Expr>> initialization_defaults;
    ir::ConstantValue initialization_value;
    Type result = Type::none;
    std::vector<Statement> body;
    std::vector<Type> locals;
};
struct Constant {
    Statement declaration;
    bool file_private = false;
    std::string visibility = "public";
    unsigned state = 0;
    Type type = Type::none;
    std::uint64_t value = 0;
    ir::ConstantValue aggregate_value;
};
struct GenericApplication {Token token;std::string name;std::shared_ptr<ir::ValueType> definition;std::vector<Type> arguments;Type resolved=Type::none;};
struct ArraySyntax {
    Token token;
    Type element;
    std::shared_ptr<ir::ValueType> definition;
    std::unique_ptr<Expr> extent;
    Type resolved = Type::none;
    unsigned state = 0;
    bool template_syntax=false;
    std::optional<Type> key={};
};
struct FunctionSyntax {Token token;std::vector<Type> parameters;Type result;std::shared_ptr<ir::ValueType> definition;Type resolved=Type::none;};
struct ResultSyntax { Token token; Type success, failure; std::shared_ptr<ir::ValueType> definition; Type resolved=Type::none; };
struct Unit;
struct Structure {
    std::vector<Token> case_tokens;
    Structure() = default;
    // Fields own noncopyable AST nodes. Container copy traits do not inspect
    // vector<Statement>'s copy body; never let reallocation select that path.
    Structure(const Structure&) = delete;
    Structure& operator=(const Structure&) = delete;
    Structure(Structure&&) = default;
    Structure& operator=(Structure&&) = default;
    Token token;
    std::vector<GenericParameter> generics;
    std::vector<Token> syntax;
    std::vector<std::pair<Token,Type>> type_uses;
    std::vector<Type> type_arguments;
    Structure* generic_origin=nullptr;
    bool hierarchy_checked=false;
    unsigned specialization_depth=0;
    std::shared_ptr<ir::ValueType> definition;
    std::string visibility = "public";
    std::vector<Statement> fields;
    std::optional<Type> base_type;
    Token base_token{TokenKind::name,"",0};
    std::vector<std::pair<Unit*,Statement*>> effective_fields;
    ir::ConstantValue defaults;
    unsigned layout_state = 0, default_state = 0, layout_depth = 1;
    std::size_t expanded_fields = 0;
    Function* initializer = nullptr;
};
static_assert(!std::is_copy_constructible_v<Pattern> && !std::is_copy_assignable_v<Pattern>);
static_assert(std::is_move_constructible_v<Pattern> && std::is_move_assignable_v<Pattern>);
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<Pattern&>())),Pattern&&>);
static_assert(!std::is_copy_constructible_v<Expr> && !std::is_copy_assignable_v<Expr>);
static_assert(std::is_move_constructible_v<Expr> && std::is_move_assignable_v<Expr>);
static_assert(std::is_same_v<decltype(std::move_if_noexcept(std::declval<Expr&>())),Expr&&>);
static_assert(std::is_move_constructible_v<Statement> && std::is_move_assignable_v<Statement>);
static_assert(!std::is_copy_constructible_v<Function> && !std::is_copy_assignable_v<Function>);
static_assert(std::is_move_constructible_v<Function> && std::is_move_assignable_v<Function>);
static_assert(!std::is_copy_constructible_v<Statement> && !std::is_copy_assignable_v<Statement>);
static_assert(!std::is_copy_constructible_v<Structure> && !std::is_copy_assignable_v<Structure>);
static_assert(std::is_move_constructible_v<Structure> && std::is_move_assignable_v<Structure>);
struct Import {
    Token path, alias;
    std::string target;
    bool explicit_alias = false;
};
struct Unit {
    SourceFile source;
    std::string identity;
    std::vector<Import> imports;
    std::list<Function> functions, generic_functions;
    std::vector<Constant> constants;
    std::list<Structure> structures, generic_structures;
    std::list<FunctionSyntax> function_types;
    std::list<ResultSyntax> results;
    std::list<ArraySyntax> arrays;
    std::list<GenericApplication> applications;
    std::vector<std::pair<Token,Type>> type_uses;
    std::map<std::string,Function*> specialized_functions;
    std::map<std::string,Structure*> specialized_structures;
    std::map<std::string, Structure*> structure_names;
    std::map<std::string, Constant*> constant_names;
    std::map<std::string, Function*> declarations, methods;
    std::map<std::string, std::vector<Unit*>> qualifiers;
    std::set<std::string> explicit_qualifiers;
    std::set<std::string> failed_qualifiers, invalid_declarations;
    std::vector<Type> synthetic_types;
    std::function<Type(Type,const Token&)> resolve_callable;
    std::function<Type(const std::vector<Type>&,Type)> make_callable;
    std::function<Type(Type)> make_task;
    std::function<Type(const std::string&,Type,bool)> make_capture_record;
    std::function<Type()> make_cleanup;
    std::function<Type(Type,const Token&)> resolve_result;
    std::function<Type(Type,const Token&)> resolve_array;
    std::function<Type(Type,Type)> make_dictionary;
    std::function<Type(Type,const Token&)> resolve_application;
    std::function<Function&(Unit&,Unit&,Function&,const std::vector<Type>&,const Token&,unsigned,Type)> specialize_function;
    std::function<Structure&(Unit&,Unit&,Structure&,const std::vector<Type>&,const Token&)> specialize_structure;
    bool failed_import = false;
    bool parse_failed = false;
    std::shared_ptr<ir::Module> member_catalog;
    Unit(SourceFile s, std::string id) : source(std::move(s)), identity(std::move(id)) {}
};
class Parser {
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
    Function* parsing_function=nullptr;
    struct FunctionScope {Function*& active;Function* saved;FunctionScope(Function*& slot,Function& next):active(slot),saved(slot){active=&next;}~FunctionScope(){active=saved;}};
    std::vector<std::pair<Token,Type>> type_uses;
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
            if (declaration && current_depth == 0 && line_start && (at("func") || at("property") || at("interface") || at("const") || at("value") || at("struct") || at("virtual") || at("override") || at("use") || at("private") || at("public") || at("internal"))) return;
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
    void parameters(Function& f, bool defaults = true) {
        need("(");
        if (!at(")")) do {
            bool readonly_parameter=accept("const");
            auto token = name(); need(":"); nodes=0; Parameter parameter{token,type()}; parameter.type.readonly=readonly_parameter;
            if (accept("=")) {
                if(!defaults) error(source,tokens[p-1],Code::argument_count,"Interface requirements cannot declare parameter defaults.");
                auto begin = p; parameter.default_begin = peek().offset; nodes = 0; parameter.default_expression = expression();
                parameter.default_end = peek().offset;
                for (auto n = begin; n < p; ++n) parameter.default_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
            }
            f.parameters.push_back(std::move(parameter));
        } while (accept(","));
        need(")"); if (accept("->")) {nodes=0;f.result = type();}
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
        if (peek().kind != TokenKind::name || reserved.contains(peek().text))
            error(source, peek(), Code::identifier, "Expected a lowercase identifier.");
        return take();
    }
    std::vector<GenericParameter> generic_parameters() {
        std::vector<GenericParameter> result;
        if(!accept("<"))return result;
        do {
            auto token=name();
            if(!generic_names.insert(token.text).second)error(source,token,Code::duplicate,"Duplicate generic type parameter '"+token.text+"'.");
            std::optional<Type> constraint;if(accept(":"))constraint=type();
            result.push_back({token,constraint});
        }while(accept(","));
        need(">");return result;
    }
    std::vector<Type> type_arguments() {
        std::vector<Type> result;need("<");do {result.push_back(type());}while(accept(","));need(">");return result;
    }
    bool type_call(bool value=false) const {
        if(!at("<"))return false;
        unsigned nesting=0;
        for(auto n=p;n<tokens.size();++n) {
            if(tokens[n].text=="<")++nesting;
            else if(tokens[n].text==">") {if(!--nesting){if(n+1>=tokens.size())return false;const auto& next=tokens[n+1];return next.text=="(" || (value && (next.kind==TokenKind::newline || next.kind==TokenKind::end || next.text=="," || next.text==")" || next.text=="]" || next.text=="}" || next.text==":"));}}
            else if(tokens[n].kind==TokenKind::newline || tokens[n].kind==TokenKind::end)return false;
        }
        return false;
    }
    // Keep branch-specific temporaries out of every recursive type frame. The
    // existing shared Depth guard still counts each source type exactly once.
    VOID_SEMANTIC_HANDLER Type function_type() {
        auto token=take();need("(");std::vector<Type> parameters;
        if(!at(")"))do{auto readonly=accept("const");auto parameter=type();parameter.readonly=readonly;parameters.push_back(parameter);}while(accept(","));need(")");
        auto result=accept("->")?type():Type(Type::none);
        auto definition=std::make_shared<ir::ValueType>();definition->callable=true;definition->name=definition->identity="func";
        function_types.push_back({token,std::move(parameters),result,definition});Type value(definition);value.nullable=accept("?");return value;
    }
    VOID_SEMANTIC_HANDLER Type result_type() {
        auto token=take();need("<");auto success=type();need(",");auto failure=type();need(">");
        auto d=std::make_shared<ir::ValueType>();d->name=d->identity="result";d->result_type=true;
        results.push_back({token,success,failure,d});return Type(d);
    }
    VOID_SEMANTIC_HANDLER Type dictionary_type() {
        auto token=take();auto key=type();need(":");auto value=type();auto close=need("}");
        if(at("?"))error(source,take(),Code::unsupported_type,"Nullable dictionaries are deferred.");
        auto d=std::make_shared<ir::ValueType>();d->name=d->identity=source.text().substr(token.offset,close.offset+1-token.offset);
        arrays.push_back({token,value,d,{}});arrays.back().key=key;return Type(d,Type::dictionary);
    }
    VOID_SEMANTIC_HANDLER Type array_type() {
        auto token=take();auto element=type();std::unique_ptr<Expr> extent;
        if(accept(",")) extent=expression();
        auto close=need("]");
        if(at("?")) error(source,take(),Code::unsupported_type,"Nullable arrays are deferred.");
        auto d=std::make_shared<ir::ValueType>();d->name=source.text().substr(token.offset,close.offset+1-token.offset);d->identity=d->name;
        Type result(d,extent ? Type::fixed_array : Type::growable_array);
        arrays.push_back({token,element,d,std::move(extent)});return result;
    }
    VOID_SEMANTIC_HANDLER Type named_type() {
        static const std::map<std::string, Type> types = {
            {"i8",Type::i8},{"i16",Type::i16},{"i32",Type::i32},{"i64",Type::i64},
            {"u8",Type::u8},{"u16",Type::u16},{"u32",Type::u32},{"u64",Type::u64},
            {"f32",Type::f32},{"f64",Type::f64},{"bool",Type::boolean},{"string",Type::string}};
        auto found = types.find(peek().text);
        if (found != types.end()) { take(); if (at("?")) error(source,take(),Code::unsupported_type,"Only managed references can be nullable in this phase."); return found->second; }
        if (peek().kind != TokenKind::name || reserved.contains(peek().text)) error(source,peek(),Code::unsupported_type,"Unsupported type '" + peek().text + "'.","Use a numeric type, bool, string or a visible value/managed struct.");
        auto token = name();
        std::string spelling = token.text;
        auto reference_token = token;
        if (accept(".")) { reference_token = name(); spelling += "." + reference_token.text; }
        if(spelling==token.text && substitutions.contains(spelling)) {
            if(at("?"))error(source,take(),Code::unsupported_type,"Nullable generic parameters are not implemented.");
            return substitutions.at(spelling);
        }
        auto d = std::make_shared<ir::ValueType>(); d->name = spelling; d->identity = spelling; d->source_offset = token.offset;
        if(at("<")){applications.push_back({reference_token,spelling,d,type_arguments()});d->name=source.text().substr(token.offset,tokens[p-1].offset+1-token.offset);}
        Type result(d,Type::unresolved); result.nullable = accept("?");
        if(result.nullable && generic_names.contains(spelling))error(source,reference_token,Code::unsupported_type,"Nullable generic parameters are not implemented.");
        type_uses.emplace_back(reference_token,result);
        return result;
    }
    Type type() {
        Depth guard(depth,source,peek());
        if(accept("(")){auto value=type();need(")");if(accept("?")){if(!ir::reference_value(value) && !(value.definition && value.definition->callable))error(source,tokens[p-1],Code::unsupported_type,"Only references and function values can be nullable.");value.nullable=true;}return value;}
        if(at("func"))return function_type();
        if(at("result"))return result_type();
        if(at("{"))return dictionary_type();
        if(at("["))return array_type();
        return named_type();
    }
    void newline() { if(multiline_expression && !at("<newline>")){multiline_expression=false;return;}multiline_expression=false;need("<newline>"); }
    std::vector<Statement> block() {
        Depth guard(depth, source, peek(), 64);
        need(":"); newline(); need("<indent>");
        std::vector<Statement> body;
        bool recovered = false;
        while (peek().kind != TokenKind::dedent && peek().kind != TokenKind::end) {
            if (diagnostics.full()) { p = tokens.size() - 1; break; }
            auto start = p;
            try { body.push_back(statement()); }
            catch (const CompilationError& e) {
                diagnostics.add(e); recovered = true;
                if ((tokens[start].text == "var" || tokens[start].text == "const") && start + 1 < tokens.size() && tokens[start + 1].kind == TokenKind::name && !reserved.contains(tokens[start + 1].text)) {
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
#include "pattern_parser.inc"
    static int precedence(const std::string& op) {
        if (op == "or") return 1;
        if (op == "and") return 2;
        if(op=="is")return 3;
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
        if (op == "+" || op == "-") return 4;
        if (op == "*" || op == "/" || op == "%") return 5;
        return 0;
    }
    std::unique_ptr<Expr> postfix(std::unique_ptr<Expr> e) {
        while(true) {
            if(accept(".")) {
                auto member=std::make_unique<Expr>(Expr::Kind::field,name());
                member->parts.push_back(std::move(e));e=std::move(member);
            } else if(at("[")) {
                if(++nodes>512) error(source,peek(),Code::nesting,"Expression size limit exceeded.");
                auto selected=std::make_unique<Expr>(Expr::Kind::array_index,take());
                selected->parts.push_back(std::move(e));selected->parts.push_back(expression());
                if(at("..") || at("..<")){selected->kind=Expr::Kind::array_slice;selected->op=take().text;selected->parts.push_back(expression());}
                need("]");e=std::move(selected);
            } else if(accept("?")) {
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
                } while(accept(","));
                need(")");
            } else break;
        }
        return e;
    }
    std::unique_ptr<Expr> primary() {
        if (++nodes > 512) error(source, peek(), Code::nesting, "Expression size limit exceeded.", "Split this expression into smaller local values.");
        auto token=take();
        if(token.text=="func") {
            if(!parsing_function)error(source,token,Code::syntax,"Anonymous functions require a function body.");
            Function lambda;lambda.anonymous=true;lambda.token=token;lambda.token.text="$lambda$"+std::to_string(parsing_function->lambdas.size());lambda.token.source_width=4;lambda.visibility="private";
            parameters(lambda);auto* parent=parsing_function;auto begin=p;{FunctionScope scope(parsing_function,lambda);lambda.body=block();}
            for(auto n=begin;n<p;++n)lambda.body_syntax+=std::to_string(tokens[n].text.size())+":"+tokens[n].text;
            auto expression=std::make_unique<Expr>(Expr::Kind::lambda,token);expression->slot=static_cast<unsigned>(parent->lambdas.size());parent->lambdas.push_back(std::move(lambda));multiline_expression=true;return expression;
        }
        if(token.text=="match")return match_expression(token);
        if(token.kind==TokenKind::invalid) throw Poison{};
        if(token.text=="{") {
            auto e=std::make_unique<Expr>(Expr::Kind::dictionary_literal,token);
            if(!at("}"))do{e->parts.push_back(expression());need(":");e->parts.push_back(expression());}while(accept(",") && !at("}"));
            need("}");return postfix(std::move(e));
        }
        if(token.text=="[") {
            auto e=std::make_unique<Expr>(Expr::Kind::array_literal,token);
            if(!at("]")) do { e->parts.push_back(expression()); } while(accept(","));
            need("]");return postfix(std::move(e));
        }
        if(token.kind==TokenKind::string || token.kind==TokenKind::number || token.text=="true" || token.text=="false" || token.text=="null") {
            auto e=std::make_unique<Expr>(Expr::Kind::literal,token);
            if(token.kind==TokenKind::string) {e->type=Type::string;e->aggregate_value.text=token.decoded;}
            else if(token.kind==TokenKind::number) e->literal_text=token.text;
            else {e->type=token.text=="null" ? Type::null_value : Type::boolean;e->value=token.text=="true";}
            return postfix(std::move(e));
        }
        if(token.text=="(") {auto e=expression();need(")");return postfix(std::move(e));}
        if(token.kind==TokenKind::name && !reserved.contains(token.text)) return postfix(std::make_unique<Expr>(Expr::Kind::name,token));
        error(source,token,Code::syntax,"Expected an expression, found '"+token.text+"'.");
    }
    std::unique_ptr<Expr> unary() {
        Depth guard(depth, source, peek());
        if(accept("spawn")) {auto e=std::make_unique<Expr>(Expr::Kind::spawned,tokens[p-1]);e->parts.push_back(unary());return e;}
        if(accept("await")) {auto e=std::make_unique<Expr>(Expr::Kind::awaited,tokens[p-1]);e->parts.push_back(unary());return e;}
        if (at("not") || at("-") || at("+")) {
            auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::unary, token); e->op = token.text;
            // not binds below comparisons: not a < b means not (a < b).
            e->parts.push_back(token.text == "not" ? expression(3) : unary());
            if ((token.text == "-" || token.text == "+") && e->parts[0]->kind == Expr::Kind::literal && !e->parts[0]->literal_text.empty() &&
                e->parts[0]->literal_text.front() != '-' && e->parts[0]->literal_text.front() != '+') {
                e->kind = Expr::Kind::literal; e->literal_text = token.text + e->parts[0]->literal_text; e->parts.clear();
            }
            return e;
        }
        return primary();
    }
    std::unique_ptr<Expr> expression(int minimum = 1) {
        Depth guard(depth, source, peek());
        auto left = unary();
        while (precedence(peek().text) >= minimum || (at("as") && minimum <= 6)) {
            if (at("as")) {
                if (++nodes > 512) error(source, peek(), Code::nesting, "Expression size limit exceeded.", "Split this expression into smaller local values.");
                auto token = take();
                if (at("?")) error(source,peek(),Code::character,"The as? conversion is not supported.","Use a checked as conversion; fallible conversion syntax is deferred.");
                auto e = std::make_unique<Expr>(Expr::Kind::conversion, token);
                e->parts.push_back(std::move(left)); e->type = type(); left = std::move(e); continue;
            }
            auto token = take();
            if(token.text=="is") {auto e=std::make_unique<Expr>(Expr::Kind::test,token);e->parts.push_back(std::move(left));e->patterns.push_back(pattern());left=std::move(e);continue;}
            auto e = std::make_unique<Expr>(Expr::Kind::binary, token); e->op = token.text;
            e->parts.push_back(std::move(left)); e->parts.push_back(expression(precedence(token.text) + 1));
            left = std::move(e);
        }
        return left;
    }
    Statement conditional(Token token) {
        Depth guard(depth, source, token, 64);
        Statement s(Statement::Kind::conditional, std::move(token));
        s.expression = expression(); s.body = block();
        if (accept("else")) {
            if (at("if")) { auto next = take(); s.otherwise.push_back(conditional(next)); }
            else s.otherwise = block();
        }
        return s;
    }
    Statement statement() {
        nodes = 0;
        if (at("pass")) {
            Statement s(Statement::Kind::no_op, take());
            newline(); return s;
        }
        if (at("while")) error(source, peek(), Code::while_loop, "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:'.");
        if(at("match")) {
            Statement s(Statement::Kind::match,take());s.expression=expression();need(":");newline();need("<indent>");
            while(!at("<dedent>") && !at("<end of file>")){Statement arm(Statement::Kind::no_op,peek());arm.pattern=pattern();arm.body=block();s.body.push_back(std::move(arm));}
            need("<dedent>");return s;
        }
        if (at("if")) return conditional(take());
        if (at("for")) {
            Statement s(Statement::Kind::loop, take());
            if(peek().kind==TokenKind::name && (peek(1).text=="in" || peek(1).text==",")) {
                s.binder=name();if(accept(","))s.index_binder=name();need("in");s.reverse=accept("reverse");
                s.expression=expression();
                if(at("..") || at("..<")) {s.exclusive=take().text=="..<";s.range_end=expression();}
            } else if (!at(":")) s.expression = expression();
            s.body = block(); return s;
        }
        if (at("var") || at("const")) {
            auto start = p;
            bool readonly = at("const");
            Statement s(Statement::Kind::variable, take()); s.readonly = readonly; auto n = name(); s.name = n.text; s.token = n;
            if (accept(":")) s.annotation = type();
            if (accept("=")) s.expression = expression();
            if (readonly && !s.expression) error(source, s.token, Code::initializer, "A const requires an initializer.");
            if (readonly) for (auto i = start; i < p; ++i)
                s.constant_syntax += std::to_string(tokens[i].text.size()) + ":" + tokens[i].text;
            newline(); return s;
        }
        if(at("with")){
            Statement statement(Statement::Kind::scoped,take());statement.expression=expression(7);
            if(accept("as"))statement.binder=name();else statement.binder={TokenKind::name,"$with$"+std::to_string(statement.token.offset),statement.token.offset};
            statement.body=block();return statement;
        }
        if(at("defer")) {
            auto token=take();Depth guard(depth,source,token,64);Statement statement(Statement::Kind::deferred,token);
            if(at("{")) {
                need("{");newline();need("<indent>");Function lambda;lambda.anonymous=true;lambda.deferred=true;lambda.token=token;lambda.token.text="$defer$"+std::to_string(parsing_function->lambdas.size());lambda.visibility="private";
                auto* parent=parsing_function;{FunctionScope scope(parsing_function,lambda);
                while(!at("<dedent>") && !at("<end of file>"))lambda.body.push_back(this->statement());need("<dedent>");need("}");newline();}
                statement.expression=std::make_unique<Expr>(Expr::Kind::lambda,token);statement.expression->slot=parent->lambdas.size();parent->lambdas.push_back(std::move(lambda));
            }else{statement.expression=expression();newline();}
            return statement;
        }
        if (at("return")) {
            Statement s(Statement::Kind::returned, take());
            if (peek().kind != TokenKind::newline) s.expression = expression();
            newline(); return s;
        }
        if (at("break") || at("continue")) {
            auto token = take(); Statement s(token.text == "break" ? Statement::Kind::broken : Statement::Kind::continued, token);
            newline(); return s;
        }
        if (peek().kind == TokenKind::name && p + 1 < tokens.size()) {
            auto end = p + 1;unsigned indexes=0;bool after_index=false;
            while(end+1<tokens.size()) {
                if(tokens[end].text=="." && tokens[end+1].kind==TokenKind::name){after_index|=indexes!=0;end+=2;continue;}
                if(tokens[end].text!="[")break;
                ++indexes;unsigned brackets=0;
                do {if(tokens[end].text=="[")++brackets;else if(tokens[end].text=="]")--brackets;++end;}
                while(end+1<tokens.size() && brackets && tokens[end].kind!=TokenKind::newline);
            }
            auto op = tokens[end].text;
            if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "++" || op == "--") {
                Statement s(Statement::Kind::assignment, name());s.name=s.token.text;
                if(indexes>1 || after_index)s.selection=postfix(std::make_unique<Expr>(Expr::Kind::name,s.token));
                else {while(accept("."))s.fields.push_back(name());if(accept("[")){s.index=expression();need("]");}}
                s.op = take().text;
                if (op == "++" || op == "--") {
                    s.expression = std::make_unique<Expr>(Expr::Kind::literal, s.token); s.expression->literal_text = "1";
                } else s.expression = expression();
                newline(); return s;
            }
        }
        Statement s(Statement::Kind::expression, peek()); s.expression = expression(); newline(); return s;
    }
public:
    explicit Parser(const SourceFile& s, Diagnostics& d) : source(s), tokens(lex(s, d)), diagnostics(d) {}
    Parser(const SourceFile& s,Diagnostics& d,std::vector<Token> syntax,std::map<std::string,Type> bindings)
        :source(s),tokens(std::move(syntax)),diagnostics(d),substitutions(std::move(bindings)),instantiating(true) {
        tokens.push_back({TokenKind::end,"<end of file>",tokens.empty()?0:tokens.back().offset});
    }
    void parse(Unit& unit) {
        auto before = diagnostics.size();
        while (peek().kind != TokenKind::end) {
            if (diagnostics.full()) { unit.parse_failed = true; break; }
            auto start = p;nodes=0;generic_names.clear();const auto use_begin=type_uses.size(),array_begin=arrays.size();
            auto template_syntax=[&](auto& declaration) {declaration.type_uses.assign(type_uses.begin()+use_begin,type_uses.end());type_uses.resize(use_begin);auto n=std::size_t{0};for(auto& a:arrays)if(n++>=array_begin)a.template_syntax=true;};
            try {
            if (accept("<newline>")) continue;
            if (peek().kind == TokenKind::invalid) throw Poison{};
            if (at("while")) error(source, peek(), Code::while_loop, "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:' inside a function.");
            if (accept("use")) {
                if (!unit.functions.empty() || !unit.constants.empty() || !unit.structures.empty()) error(source, peek(), Code::syntax, "Imports must precede constant and function declarations.", "Move use declarations to the start of this file.");
                if (peek().kind != TokenKind::path) error(source, peek(), Code::import_path, "Expected a quoted import path.", "Use \"/folder/file.void\" or \"./file.void\".");
                Import import; import.path = take(); import.alias = import.path;
                if (accept("as")) { import.alias = name(); import.explicit_alias = true; }
                newline(); unit.imports.push_back(std::move(import)); continue;
            }
            Function f;
            if (accept("private")) { f.file_private = true; f.visibility = "private"; }
            else if (!accept("public") && accept("internal")) f.visibility = "internal"; // All graph files belong to this project.
            if (at("const")) {
                auto declaration = statement();
                unit.constants.push_back({std::move(declaration), f.file_private, f.visibility,0,Type::none,0,{}}); continue;
            }
            if(accept("enum")) {
                Structure enumeration;enumeration.token=name();enumeration.visibility=f.visibility;
                enumeration.definition=std::make_shared<ir::ValueType>();enumeration.definition->name=enumeration.token.text;
                Statement tag(Statement::Kind::variable,enumeration.token);tag.name="$tag";tag.annotation=Type::i32;enumeration.fields.push_back(std::move(tag));
                need(":");newline();need("<indent>");std::set<std::string> cases;
                while(!at("<dedent>") && !at("<end of file>")) {
                    auto token=name();enumeration.case_tokens.push_back(token);if(!cases.insert(token.text).second)error(source,token,Code::duplicate,"Duplicate enum variant.");
                    ir::SumCase variant{token.text,static_cast<unsigned>(enumeration.fields.size()),0};
                    if(accept("(")) {if(!at(")"))do {
                        Statement field(Statement::Kind::variable,name());field.name="$"+token.text+"$"+field.token.text;need(":");field.annotation=type();enumeration.fields.push_back(std::move(field));++variant.count;
                    }while(accept(","));
                    need(")");}
                    enumeration.definition->cases.push_back(variant);newline();
                }
                need("<dedent>");if(cases.empty())error(source,enumeration.token,Code::syntax,"An enum requires at least one variant.");
                unit.structures.push_back(std::move(enumeration));continue;
            }
            if (accept("interface")) {
                Structure structure; structure.token=name(); structure.visibility=f.visibility;
                structure.definition=std::make_shared<ir::ValueType>(); structure.definition->name=structure.token.text; structure.definition->interface_type=true;
                need(":");newline();need("<indent>");unsigned count=0;
                while(!at("<dedent>") && !at("<end of file>")) {
                    if(peek().text=="async")error(source,peek(),Code::unsupported_type,"Async interface methods require a reviewed calling and dispatch contract.");
                    need("func");Function requirement;requirement.interface_requirement=true;
                    requirement.visibility=f.visibility;requirement.file_private=f.file_private;
                    requirement.token=name();
                    if(requirement.token.text=="init" || requirement.token.text=="deinit") error(source,requirement.token,Code::initializer_deferred,"Lifecycle methods are not interface requirements.");
                    auto unresolved=std::make_shared<ir::ValueType>();unresolved->name=structure.token.text;unresolved->source_offset=structure.token.offset;
                    requirement.receiver=Parameter{structure.token,Type(unresolved,Type::unresolved)};
                    parameters(requirement,false);newline();unit.functions.push_back(std::move(requirement));++count;
                }
                if(!count) error(source,structure.token,Code::syntax,"An interface requires at least one method; marker interfaces are deferred.");
                need("<dedent>");unit.structures.push_back(std::move(structure));continue;
            }
            bool value_declaration = accept("value");
            if (value_declaration || at("struct")) {
                need("struct"); Structure structure; structure.token = name(); structure.visibility = f.visibility;
                structure.definition = std::make_shared<ir::ValueType>(); structure.definition->name = structure.token.text; structure.definition->managed = !value_declaration;
                structure.generics=generic_parameters();
                if (accept("extends")) { structure.base_token=peek(); structure.base_type=type(); }
                need(":"); newline();
                bool fields=accept("<indent>");
                if (!fields && !structure.definition->managed) need("<indent>");
                while (fields && !at("<dedent>") && !at("<end of file>")) {
                    bool composition = at("using") && peek(1).text != ":";
                    if (composition) take();
                    bool event=accept("event");auto start = p;
                    nodes=0;
                    Statement field(Statement::Kind::variable, name()); field.name = field.token.text;
                    field.using_field = composition;field.event=event;
                    if (composition && !at(":")) {
                        p = start; field.annotation = type();
                        field.token = tokens[p - (field.annotation->nullable ? 2 : 1)];
                        field.name = field.token.text;
                    } else { need(":"); field.annotation = type(); }
                    auto begin = p; nodes = 0;
                    if (accept("=")){if(event)error(source,field.token,Code::initializer,"Events start empty and cannot declare an initializer.");field.expression = expression();}
                    for (auto n = begin; n < p; ++n) field.constant_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
                    newline(); structure.fields.push_back(std::move(field));
                }
                if (fields) need("<dedent>");
                if(!instantiating && !structure.generics.empty()) {
                    template_syntax(structure);structure.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_structures.push_back(std::move(structure));
                } else {structure.generics.clear();unit.structures.push_back(std::move(structure));}
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
            f.asynchronous=accept("async");
            f.virtual_method=accept("virtual");
            f.overriding=!f.virtual_method && accept("override");
            need("func");
            if (accept("(")) {
                auto receiver = name(); auto receiver_type = type(); need(")");
                f.receiver = Parameter{receiver,receiver_type};
            }
            f.token = name();
            f.generics=generic_parameters();
            parameters(f);
            auto body_begin = p;{FunctionScope scope(parsing_function,f);f.body=block();}
            for (auto n = body_begin; n < p; ++n) f.body_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
            f.parse_failed = diagnostics.within(source, f.token.offset, peek().offset);
            const bool generic_receiver=f.receiver && std::any_of(applications.begin(),applications.end(),[&](const auto& a){return a.definition==f.receiver->type.definition;});
            if(!instantiating && (!f.generics.empty() || generic_receiver)) {
                template_syntax(f);f.syntax.assign(tokens.begin()+start,tokens.begin()+p);unit.generic_functions.push_back(std::move(f));
            } else {f.generics.clear();f.substitutions=substitutions;unit.functions.push_back(std::move(f));}
            } catch (const CompilationError& e) {
                diagnostics.add(e); unit.parse_failed = true;
                auto declaration = start;
                if (tokens[declaration].text == "private" || tokens[declaration].text == "public" || tokens[declaration].text == "internal") ++declaration;
                if (declaration + 1 < tokens.size() && (tokens[declaration].text == "func" || tokens[declaration].text == "const") && tokens[declaration + 1].kind == TokenKind::name && !reserved.contains(tokens[declaration + 1].text))
                    unit.invalid_declarations.insert(tokens[declaration + 1].text);
                synchronize(start, true);
            }
            catch (const Poison&) { unit.parse_failed = true; synchronize(start, true); }
            if (p == start && peek().kind != TokenKind::end) take();
        }
        unit.parse_failed |= diagnostics.size() != before;
        unit.type_uses = std::move(type_uses);
        unit.function_types=std::move(function_types);unit.results=std::move(results);
        unit.arrays = std::move(arrays);unit.applications=std::move(applications);
    }
};
ir::Op operation(const std::string& op) {
    static const std::map<std::string, ir::Op> ops = {{"+", ir::Op::add}, {"-", ir::Op::subtract}, {"*", ir::Op::multiply},
        {"/", ir::Op::divide}, {"%", ir::Op::remainder}, {"<", ir::Op::less}, {"<=", ir::Op::less_equal},
        {">", ir::Op::greater}, {">=", ir::Op::greater_equal}, {"==", ir::Op::equal}, {"!=", ir::Op::not_equal}};
    return ops.at(op);
}

// Flow sets describe possible exits from a statement, independently of LLVM.
constexpr unsigned next_flow = 1, return_flow = 2, break_flow = 4, continue_flow = 8;
bool always(const Expr* e) { return !e || (e->kind == Expr::Kind::literal && e->type == Type::boolean && e->value == 1); }
// Type lookup follows the same owned graph and direct-import boundary as values.
std::pair<Unit*, Structure*> structure_lookup(Unit& unit, Unit& root,
    const std::map<std::string,std::unique_ptr<Unit>>& units, const std::string& spelling, const Token& token) {
    auto dot = spelling.find('.'); auto name = spelling.substr(dot == std::string::npos ? 0 : dot + 1);
    if (dot == std::string::npos && unit.invalid_declarations.contains(name)) throw Poison{};
    std::map<std::string,std::pair<Unit*,Structure*>> candidates;
    auto add = [&](Unit* owner) {
        auto found = owner->structure_names.find(name);
        if (found != owner->structure_names.end() && (owner == &unit || found->second->visibility != "private"))
            candidates.emplace(owner->identity,std::make_pair(owner,found->second));
    };
    if (dot != std::string::npos) {
        auto qualifier = spelling.substr(0,dot); auto found = unit.qualifiers.find(qualifier);
        if (unit.failed_qualifiers.contains(qualifier)) throw Poison{};
        if (found == unit.qualifiers.end()) return {nullptr,nullptr};
        if (found->second.size() != 1) error(unit.source,token,Code::ambiguous,"Ambiguous import qualifier '" + qualifier + "'.");
        add(found->second.front());
    } else {
        add(&unit);
        if (!candidates.empty()) return candidates.begin()->second;
        if (unit.constant_names.contains(name) || unit.declarations.contains(name)) return {nullptr,nullptr};
        add(&root);
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) add(units.at(import.target).get());
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            auto& owner = *units.at(import.target);
            if (!candidates.empty() && ((owner.declarations.contains(name) && !owner.declarations.at(name)->file_private) ||
                (owner.constant_names.contains(name) && !owner.constant_names.at(name)->file_private)))
                error(unit.source,token,Code::ambiguous,"Ambiguous imported declaration '" + name + "'.","Use explicit aliases.");
        }
    }
    if (candidates.size() > 1) error(unit.source,token,Code::ambiguous,"Ambiguous imported type '" + name + "'.","Use explicit aliases.");
    return candidates.empty() ? std::pair<Unit*,Structure*>{nullptr,nullptr} : candidates.begin()->second;
}
Type resolve_type(Type t, Unit& unit, Unit& root, const std::map<std::string,std::unique_ptr<Unit>>& units, const Token& token) {
    if(t.definition && t.definition->callable){auto result=unit.resolve_callable(t,token);result.nullable=t.nullable;result.readonly=t.readonly;return result;}
    if(t.definition && t.definition->result_type){auto result=unit.resolve_result(t,token);result.readonly=t.readonly;result.readonly_references=t.readonly_references;return result;}
    if(ir::collection_type(t)) return unit.resolve_array(t,token);
    if (t.kind != Type::unresolved) return t;
    for(auto& application:unit.applications)if(application.definition==t.definition) {
        auto result=application.resolved;
        if(result==Type::none)result=unit.resolve_application(t,token);
        result.nullable=t.nullable;result.readonly=t.readonly;
        if(result.nullable && !ir::reference_value(result))error(unit.source,token,Code::unsupported_type,"Only managed references can be nullable.");
        return result;
    }
    auto [owner, structure] = structure_lookup(unit,root,units,t.definition->name,token); (void)owner;
    auto location = token; location.offset = t.definition->source_offset; location.text = t.definition->name;
    if (!structure) error(unit.source,location,Code::unsupported_type,"Unsupported type '" + t.definition->name + "'.","Use a numeric type, bool, string or a visible value/managed struct.");
    if(!structure->generics.empty())error(unit.source,token,Code::unsupported_type,"Generic type '"+structure->token.text+"' requires concrete type arguments.");
    if (structure->layout_state == 3) throw Poison{};
    Type result(structure->definition, structure->definition->interface_type ? Type::interface_ref : structure->definition->managed ? Type::managed_ref : Type::aggregate);
    result.nullable = t.nullable; result.readonly=t.readonly;
    if (result.nullable && !ir::reference_value(result)) error(unit.source,token,Code::unsupported_type,"Only managed references can be nullable in this phase.");
    return result;
}
ir::ConstantValue constant_value(const SourceFile&, const Expr&);
class Semantics {
    const SourceFile& source;
    Unit& unit;
    const std::map<std::string, std::unique_ptr<Unit>>& units;
    std::set<std::string> visible_names;
    Unit& root;
    std::function<void(Unit&, Constant&)> evaluate;
    std::function<void(Unit&, Structure&)> defaults;
    Diagnostics& diagnostics;
    bool invalid_function = false;
    struct Variable { Type type; unsigned slot; bool readonly; Token declaration;Function* owner=nullptr; bool task_group=false; };
    std::function<Variable*(const std::string&)> enclosing;
    std::vector<std::map<std::string, Variable>> scopes;
    Function* current = nullptr;
    unsigned loops = 0;
    bool suppress_zero = false,pattern_bindings=false,allow_event=false;
    Variable* lookup(const std::string& name) {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            auto found = scope->find(name); if (found != scope->end()) return &found->second;
        }
        if(enclosing && current)if(auto* outer=enclosing(name)) {
            const bool snapshot=outer->owner->receiver && outer->slot==outer->owner->receiver->slot && outer->type.kind==Type::aggregate;
            Type cell;
            if(!snapshot && outer->owner->captured_locals.contains(outer->slot))cell=outer->owner->captured_locals.at(outer->slot);
            else {
                cell=unit.make_capture_record(outer->owner->symbol+"::$cell$"+std::to_string(outer->slot),outer->type,true);
                if(!snapshot)outer->owner->captured_locals.emplace(outer->slot,cell);
            }
            const auto slot=static_cast<unsigned>(current->locals.size());current->locals.push_back(outer->type);
            current->capture_inputs.emplace(slot,static_cast<unsigned>(current->captures.size()));current->captured_locals.emplace(slot,cell);
            current->captures.push_back({outer->slot,cell,outer->declaration,snapshot});
            auto [position,inserted]=scopes.front().emplace(name,Variable{outer->type,slot,outer->readonly,outer->declaration,current,outer->task_group});return &position->second;
        }
        return nullptr;
    }
    std::pair<Unit*, Constant*> constant(const Expr& e) {
        if (e.qualifier.empty() && unit.invalid_declarations.contains(e.token.text)) throw Poison{};
        std::map<std::string, std::pair<Unit*, Constant*>> candidates;
        auto add = [&](Unit* owner, bool own = false) {
            if (owner->invalid_declarations.contains(e.token.text)) throw Poison{};
            auto found = owner->constant_names.find(e.token.text);
            if (found != owner->constant_names.end()) {
                if (found->second->file_private && !own)
                    return;
                candidates.emplace(owner->identity, std::make_pair(owner, found->second));
            }
        };
        if (!e.qualifier.empty()) {
            auto q = unit.qualifiers.find(e.qualifier);
            if (unit.failed_qualifiers.contains(e.qualifier)) throw Poison{};
            if (q == unit.qualifiers.end()) error(source, e.qualifier_token, Code::visibility, "Unknown import qualifier '" + e.qualifier + "'.");
            if (q->second.size() != 1) error(source, e.qualifier_token, Code::ambiguous, "Ambiguous import qualifier '" + e.qualifier + "'.");
            add(q->second.front(), q->second.front() == &unit);
            if (candidates.empty() && q->second.front()->parse_failed) throw Poison{};
            if (candidates.empty()) error(source, e.token, Code::visibility, "Unknown or private imported constant '" + e.token.text + "'.");
        } else {
            add(&unit, true);
            if (!candidates.empty()) return candidates.begin()->second;
            if (unit.declarations.contains(e.token.text)) return {nullptr, nullptr};
            add(&root, &root == &unit);
            for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) add(units.at(import.target).get());
            // A value and a function cannot silently compete for an imported binding.
            for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
                auto* owner = units.at(import.target).get();
                auto f = owner->declarations.find(e.token.text);
                if (!candidates.empty() && f != owner->declarations.end() && !f->second->file_private)
                    error(source, e.token, Code::ambiguous, "Ambiguous imported declaration '" + e.token.text + "'.", "Use explicit aliases.");
            }
        }
        if (candidates.size() > 1) error(source, e.token, Code::ambiguous, "Ambiguous imported constant '" + e.token.text + "'.", "Use explicit aliases.");
        return candidates.empty() ? std::pair<Unit*, Constant*>{nullptr, nullptr} : candidates.begin()->second;
    }
    const Function& resolve(const Expr& e) {
        if (e.qualifier.empty() && unit.invalid_declarations.contains(e.token.text)) throw Poison{};
        if (!e.qualifier.empty()) {
            auto module = unit.qualifiers.find(e.qualifier);
            if (unit.failed_qualifiers.contains(e.qualifier)) throw Poison{};
            if (module == unit.qualifiers.end()) error(source, e.qualifier_token, Code::visibility, "Unknown import qualifier '" + e.qualifier + "'.", "Import the module with this alias before calling it.");
            if (module->second.size() != 1) error(source, e.qualifier_token, Code::ambiguous, "Ambiguous import qualifier '" + e.qualifier + "'.", "Assign distinct explicit aliases with as to modules sharing a filename stem.");
            auto* target = module->second.front();
            if (target->invalid_declarations.contains(e.token.text)) throw Poison{};
            if (target->constant_names.contains(e.token.text)) error(source, e.token, Code::not_callable, "A const value is not callable.");
            auto found = target->declarations.find(e.token.text);
            if (found == target->declarations.end() && target->parse_failed) throw Poison{};
            if (found == target->declarations.end()) error(source, e.token, Code::unknown_function, "Unknown imported function '" + e.qualifier + "." + e.token.text + "'.");
            if (found->second->file_private && target != &unit)
                error(source, e.token, Code::visibility, "Function '" + e.token.text + "' is private to " + target->identity + ".", "Call a public function from that file instead.");
            return *found->second;
        }
        if (unit.constant_names.contains(e.token.text)) error(source, e.token, Code::not_callable, "A const value is not callable.");
        auto own = unit.declarations.find(e.token.text);
        if (own != unit.declarations.end()) return *own->second;
        if (constant(e).second) error(source, e.token, Code::not_callable, "A const value is not callable.");
        std::map<std::string, Function*> candidates;
        bool hidden = false;
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty()) {
            if (units.at(import.target)->invalid_declarations.contains(e.token.text)) throw Poison{};
            auto found = units.at(import.target)->declarations.find(e.token.text);
            if (found != units.at(import.target)->declarations.end()) {
                if (found->second->file_private) hidden = true;
                else candidates.emplace(found->second->symbol, found->second);
            }
        }
        if (candidates.size() > 1) {
            std::string owners;
            for (const auto& [id, f] : candidates) { (void)f; if (!owners.empty()) owners += ", "; owners += id.substr(0, id.rfind("::")); }
            error(source, e.token, Code::ambiguous, "Ambiguous imported function '" + e.token.text + "' from " + owners + ".", "Use a module qualifier or import as an explicit alias.");
        }
        if (!candidates.empty()) return *candidates.begin()->second;
        if (hidden) error(source, e.token, Code::visibility, "Function '" + e.token.text + "' is private to its declaring file.", "Call a public function instead.");
        if (unit.failed_import) throw Poison{};
        error(source, e.token, Code::unknown_function, "Unknown function '" + e.token.text + "'.", "Declare it here or import its declaring file; imports are not transitive exports.");
    }
#include "generic_inference.inc"
    struct Member {
        std::vector<unsigned> path;
        const Function* function = nullptr;
    };
    const Function* direct_method(Type receiver, const Token& token) {
        auto key = receiver.definition->identity + "::method::" + token.text;
        for (const auto& [id,owner] : units)
            if (auto found=owner->methods.find(key); found!=owner->methods.end()) return found->second;
        return nullptr;
    }
    bool accessible(const Function& f) {
        for (const auto& [id,owner]:units) if (owner->methods.contains(f.symbol))
            return (owner.get()==&unit || !f.file_private) &&
                (owner.get()==&unit || owner.get()==&root || std::any_of(unit.imports.begin(),unit.imports.end(),[&](const auto& i){return i.target==id;}));
        return false;
    }
    std::optional<Member> direct_member(Type t,const Token& token) {
        auto base=t.definition->base.lock();
        for (unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<t.definition->fields.size();++n)
            if (t.definition->fields[n].name==token.text) return Member{{n},nullptr};
        if (auto* f=direct_method(t,token)) return Member{{},f};
        return {};
    }
    Member member(Type t,const Token& token,bool calling = false) {
        if (t==error_type) throw Poison{};
        if (t.nullable) error(source,token,calling ? Code::receiver : Code::type_mismatch,"Unwrap a nullable reference before member access.","Use a checked as conversion to its non-nullable type.");
        if(t.kind==Type::interface_ref) {
            if(auto* requirement=direct_method(t,token)) {if(!accessible(*requirement)) error(source,token,Code::visibility,"Interface requirement is not accessible; import its original interface module.");return Member{{},requirement};}
            error(source,token,calling ? Code::unknown_method : Code::unknown_identifier,"Interface views expose only their declared method requirements.");
        }
        if (t.kind!=Type::aggregate && t.kind!=Type::managed_ref)
            error(source,token,calling ? Code::receiver : Code::type_mismatch,"Member access requires a struct.");
        auto selected=ir::select_member(*root.member_catalog,t,token.text,unit.identity);
        if(selected.status==ir::MemberSelection::inaccessible)
            error(source,token,Code::visibility,"Member '"+token.text+"' is not accessible from this module.","Import its owning module and use a public method.");
        if(selected.status==ir::MemberSelection::ambiguous) {
            std::vector<RelatedLocation> related;std::string routes;
            auto [owner,structure]=owned_structure(t);
            for(const auto& [path,method]:selected.candidates) {
                auto [embedded_owner,embedded]=structure->effective_fields.at(path.front());
                auto route=embedded->name+"."+token.text;if(!routes.empty()) routes+=", ";routes+=route;
                related.push_back({span(embedded_owner->source,embedded->token),"Candidate through '"+route+"'."});
                auto child=ir::field_type(t,path.front());auto [child_owner,child_structure]=owned_structure(child);
                auto declaration=method.empty() ? child_structure->effective_fields.at(path.back()).second->token : direct_method(child,token)->token;
                related.push_back({span(child_owner->source,declaration),"Promoted member declared here."});
            }
            error(source,token,Code::ambiguous,"Ambiguous promoted member '"+token.text+"': "+routes+".","Qualify the intended embedded field explicitly; types and signatures do not select a component.",std::move(related));
        }
        if(selected.status==ir::MemberSelection::found) {
            if(selected.method.empty()){auto path_owner=t;for(auto index:selected.path){if(path_owner.definition->native_resource)error(source,token,Code::unsupported_type,"Native resource payload is binding-private.");path_owner=ir::field_type(path_owner,index);}}
            const Function* target=nullptr;
            if(!selected.method.empty()) for(const auto& [id,u]:units) for(const auto& f:u->functions) if(f.symbol==selected.method) target=&f;
            return {selected.path,target};
        }
        error(source,token,calling ? Code::unknown_method : Code::unknown_identifier,std::string(calling ? "Unknown method '" : "Unknown field '")+token.text+"' in "+ir::type_name(t)+".");
    }
    void embedded_receiver(std::unique_ptr<Expr>& base,unsigned index,const Token& written) {
        auto t=base->type;
        auto projected=std::make_unique<Expr>(Expr::Kind::field,written);
        projected->token.text=t.definition->fields[index].name;
        projected->slot=index; projected->type=ir::field_type(t,index);
        projected->target=t.definition->fields[index].owner+"::field::"+projected->token.text;
        projected->implicit=true; projected->parts.push_back(std::move(base)); base=std::move(projected);
    }
    bool module_call(const Expr& e) {
        const auto& base = *e.parts[0];
        if (base.kind != Expr::Kind::name || lookup(base.token.text)) return false;
        if (unit.qualifiers.contains(base.token.text) || unit.failed_qualifiers.contains(base.token.text)) return true;
        // Retain the established unknown-qualifier diagnostics for a.b().
        return !constant(base).second;
    }
    // Keep parent-only temporaries off recursively nested ordinary call frames.
    Type super_expression(Expr& e) {
        if (!current || !current->receiver || current->receiver->type.definition->base.expired())
            error(source,e.parts[0]->token,Code::receiver,"super requires a method on a derived managed struct.");
        if (e.token.text=="init") error(source,e.token,Code::initializer,"super.init(...) is permitted only as the first derived initializer prelude.");
        if(e.token.text=="deinit" && current->token.text!="deinit")error(source,e.token,Code::initializer_deferred,"super.deinit is only valid within deinit.");
        const Function* selected=nullptr;
        for (auto base=current->receiver->type.definition->base.lock();base && !selected;base=base->base.lock()) selected=direct_method(Type(base,Type::managed_ref),e.token);
        if (!selected) error(source,e.token,Code::unknown_method,"No real parent method named '"+e.token.text+"'; super does not promote using members.");
        if (!accessible(*selected)) error(source,e.token,Code::visibility,"Parent method is not accessible from this module.");
        e.parts[0]=std::make_unique<Expr>(Expr::Kind::name,current->receiver->token); expression(*e.parts[0]); e.parts[0]->implicit=true;
        e.target=selected->symbol; e.super_call=true; e.virtual_slot=selected->virtual_slot;
        e.receiver_addressable=true; e.slot=current->receiver->slot;
        auto original=std::move(e.parts[0]);
        e.parts[0]=std::make_unique<Expr>(Expr::Kind::conversion,original->token); e.parts[0]->op="upcast"; e.parts[0]->implicit=true; e.parts[0]->type=selected->receiver->type; e.parts[0]->parts.push_back(std::move(original));
        if (std::any_of(e.names.begin(),e.names.end(),[](const auto& t){return !t.text.empty();})) error(source,e.token,Code::argument_count,"Use positional method arguments.");
        return e.type=call_arguments(e,*selected,1);
    }
    VOID_SEMANTIC_HANDLER Type method_expression(Expr& e, std::optional<Type> expected) {
        if(auto variant=enum_expression(e,true))return *variant;
        bool super=e.parts[0]->kind==Expr::Kind::name && e.parts[0]->token.text=="super" && !lookup("super") && !unit.qualifiers.contains("super") && !unit.failed_qualifiers.contains("super");
        if (super) return super_expression(e);
        if ((e.token.text == "init") && !(e.parts[0]->kind==Expr::Kind::name && unit.qualifiers.contains(e.parts[0]->token.text)))
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        if (module_call(e)) {
            e.qualifier = e.parts[0]->token.text; e.qualifier_token = e.parts[0]->token;
            e.parts.erase(e.parts.begin()); e.kind = Expr::Kind::call;
            return call_expression(e);
        }
        auto event_access=allow_event;allow_event=e.token.text=="emit";auto receiver=expression(*e.parts[0]);allow_event=event_access;
        if(receiver.definition && receiver.definition->task_result && e.token.text=="cancel"){
            if(e.parts.size()!=1 || !e.type_arguments.empty() || !current)error(source,e.token,Code::unsupported_type,"Task cancellation requires a function activation and no arguments.");
            if(receiver.nullable)error(source,e.token,Code::type_mismatch,"Task cancellation requires a non-null handle.");
            e.kind=Expr::Kind::task_cancel;return e.type=Type::none;
        }
        (void)expected;
        return resolved_method_expression(e,receiver);
    }
    // Resolve the receiver before reserving mutation/member-check temporaries.
    // Deep receiver chains must reach the existing semantic nesting guard on
    // the supported 1 MiB native stack in Debug as well as optimized builds.
    VOID_SEMANTIC_HANDLER Type resolved_method_expression(Expr& e,Type receiver) {
        if(e.parts[0]->event){
            bool owner=false;if(current && current->receiver)for(auto type=current->receiver->type.definition;type;type=type->base.lock())owner|=type->identity==e.parts[0]->event_owner;
            if(e.token.text!="emit" || !owner)error(source,e.token,Code::visibility,"Only methods of the event owner may emit it.");
            auto signature=ir::array_element(receiver);if(e.parts.size()!=signature.definition->call_parameters.size()+1 || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect event emission arguments.");
            for(unsigned n=1;n<e.parts.size();++n){auto parameter=Type(signature.definition->call_parameters[n-1]);expect(expression(*e.parts[n],parameter),parameter,e.parts[n]->token);}e.target="$event.emit";return e.type=Type::none;
        }
        if(receiver.definition && receiver.definition->callable && e.token.text=="call"){e.kind=Expr::Kind::function_call;return function_invocation(e);}
        if(receiver.kind==Type::dictionary && (e.token.text=="contains" || e.token.text=="remove" || e.token.text=="clear" || e.token.text=="get")) {
            if(e.token.text=="get")error(source,e.token,Code::unsupported_type,"Dictionary get awaits its approved result error type/signature; use checked indexing or contains.");
            if(!current)error(source,e.token,Code::constant_call,"Dictionary operations require runtime evaluation.");
            if(e.parts.size()!=(e.token.text=="clear"?1u:2u) || std::any_of(e.names.begin(),e.names.end(),[](const auto& n){return !n.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect dictionary positional arguments.");
            if(e.parts.size()==2)expect(expression(*e.parts[1],ir::readonly_view(ir::dictionary_key(receiver))),ir::readonly_view(ir::dictionary_key(receiver)),e.parts[1]->token);
            e.target="$dict."+e.token.text;return e.type=e.token.text=="clear"?Type(Type::none):Type(Type::boolean);
        }
        if(ir::array_type(receiver) && (e.token.text=="push" || e.token.text=="remove_at" || e.token.text=="insert" || e.token.text=="clear" || e.token.text=="pop")) {
            if(e.token.text=="pop")error(source,e.token,Code::unsupported_type,"pop awaits its approved result error type/signature; use checked remove_at(-1).");
            if(!current)error(source,e.token,Code::constant_call,"List mutation is not a pure constant expression.");
            if(receiver.kind!=Type::growable_array)error(source,e.token,Code::type_mismatch,"Fixed arrays cannot change length.");
            const auto count=e.token.text=="clear"?1u:e.token.text=="insert"?3u:2u;
            if(e.parts.size()!=count || std::any_of(e.names.begin(),e.names.end(),[](const auto& n){return !n.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect positional arguments for list operation.");
            e.target="$array."+e.token.text;
            if(count>1) {
                const bool index=e.token.text!="push";auto argument=expression(*e.parts[1],index?std::nullopt:std::optional(ir::array_element(receiver)));
                if(index){if(!numeric::integer(argument))error(source,e.parts[1]->token,Code::type_mismatch,"List index requires an integer, excluding bool and floats.");}
                else expect(argument,ir::array_element(receiver),e.parts[1]->token);
            }
            if(count==3)expect(expression(*e.parts[2],ir::array_element(receiver)),ir::array_element(receiver),e.parts[2]->token);
            return e.type=e.token.text=="remove_at"?ir::array_element(receiver):Type(Type::none);
        }
        if (!current) error(source,e.token,Code::constant_call,"A compile-time constant cannot call a method.","Use a local const for runtime values.");
        if (e.token.text == "init")
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        // A stored callable struct uses its ordinary call method. Keep the
        // field/property read in the receiver expression, preserving selection.
        if(!e.property && receiver.definition && !receiver.nullable && (receiver.kind==Type::aggregate || receiver.kind==Type::managed_ref)) {
            auto selected=ir::select_member(*root.member_catalog,receiver,e.token.text,unit.identity);
            bool callable_field=selected.status==ir::MemberSelection::found && selected.method.empty();
            if(!selected.method.empty())for(const auto& f:root.member_catalog->functions)
                if(f.name==selected.method && f.accessor==1 && (f.result.kind==Type::aggregate || f.result.kind==Type::managed_ref || f.result.kind==Type::interface_ref))callable_field=true;
            if(callable_field) {
                Depth depth(diagnostics.semantic_depth(),source,e.token);
                auto field=std::make_unique<Expr>(Expr::Kind::field,e.token);field->parts.push_back(std::move(e.parts[0]));
                auto type=expression(*field);
                if(type.kind!=Type::aggregate && type.kind!=Type::managed_ref && type.kind!=Type::interface_ref)
                    error(source,e.token,Code::not_callable,"Field '"+e.token.text+"' is not callable.");
                e.parts[0]=std::move(field);e.token.source_width=e.token.text.size();e.token.text="call";
                return resolved_method_expression(e,type);
            }
        }
        const auto& target = [&]() -> const Function& {
            auto check_arguments = [&] { for (std::size_t n=1; n<e.parts.size(); ++n) expression(*e.parts[n]); };
            try {
                if(auto* specialized=concrete_method(e,receiver))return *specialized;
                auto selected=member(receiver,e.token,true);
                if (!selected.function) error(source,e.token,Code::not_callable,"Field '"+e.token.text+"' is not callable.");
                for (auto index:selected.path) embedded_receiver(e.parts[0],index,e.token);
                receiver=e.parts[0]->type;
                return *selected.function;
            }
            catch (const CompilationError&) { check_arguments(); throw; }
            catch (const Poison&) { check_arguments(); throw; }
        }();
        if(target.accessor && !e.property)error(source,e.token,Code::not_callable,"A property uses field access, without parentheses.");
        e.target = target.symbol; e.virtual_slot=target.virtual_slot; e.interface_call=target.interface_requirement;
        // Record the resolved place now, then check its permission after the
        // complete method graph has reached its receiver-effect fixed point.
        const Expr* base = e.parts[0].get(); std::vector<unsigned> path;
        while (base->kind == Expr::Kind::field) { path.push_back(base->slot); base = base->parts[0].get(); }
        if (base->kind == Expr::Kind::literal && !base->target.empty()) e.receiver_readonly = true;
        if (base->kind == Expr::Kind::name) {
            if (auto* v = lookup(base->token.text)) {
                e.receiver_addressable = true; e.receiver_readonly = v->readonly; e.slot = v->slot;
                e.field_indexes.assign(path.rbegin(),path.rend());
            }
        }
        if (ir::reference_value(receiver)) e.receiver_addressable=true;
        if(e.parts[0]->property && !e.parts[0]->setter.empty()) {
            e.receiver_addressable=e.parts[0]->receiver_addressable;
            e.receiver_readonly=e.parts[0]->receiver_readonly;e.slot=e.parts[0]->slot;
            e.field_indexes=e.parts[0]->field_indexes;
        }
        const Expr* selected=e.parts[0].get();bool indexed_receiver=false;
        while(selected->kind==Expr::Kind::field || selected->kind==Expr::Kind::array_index || selected->property) {
            indexed_receiver|=selected->kind==Expr::Kind::array_index;e.selected_receiver|=selected->property;
            selected=selected->parts[0].get();
        }
        // Indexed inline values still cannot expose a mutable receiver alias.
        // Copy-back would overwrite changes made through aliases in the callee.
        if(indexed_receiver && receiver.kind==Type::aggregate) {e.selected_receiver=false;e.receiver_addressable=false;}
        else if(e.selected_receiver && selected->kind==Expr::Kind::name)if(auto* v=lookup(selected->token.text)) {
            e.receiver_addressable=true;e.slot=v->slot;e.receiver_readonly=v->readonly;
        }
        const Expr* candidate=e.parts[0].get();
        while (candidate->kind==Expr::Kind::field) {
            if (candidate->parts[0]->type.kind==Type::managed_ref) e.receiver_addressable=true;
            candidate=candidate->parts[0].get();
        }
        if (std::any_of(e.names.begin(),e.names.end(),[](const auto& t) { return !t.text.empty(); }))
            error(source,e.token,Code::argument_count,"Named method arguments are not implemented; use positional arguments.");
        if (receiver.kind==Type::managed_ref && Type::key(receiver)!=Type::key(target.receiver->type)) {
            auto original=std::move(e.parts[0]);
            e.parts[0]=std::make_unique<Expr>(Expr::Kind::conversion,original->token); e.parts[0]->op="upcast"; e.parts[0]->implicit=true;
            e.parts[0]->type=target.receiver->type; e.parts[0]->type.readonly=receiver.readonly; e.parts[0]->parts.push_back(std::move(original));
        }
        return e.type = call_arguments(e,target,1);
    }
    Type call_arguments(Expr& e, const Function& target, std::size_t first, bool constructor = false) {
        e.asynchronous_call=target.asynchronous;e.native_symbol=target.native_symbol;
        if(e.interface_call) {
            std::vector<const Expr*> pending;for(std::size_t n=first;n<e.parts.size();++n)pending.push_back(e.parts[n].get());
            while(!pending.empty()){auto* part=pending.back();pending.pop_back();if(part->kind==Expr::Kind::awaited)error(source,part->token,Code::unsupported_type,"await cannot retain a prepared interface receiver across suspension.","Await arguments into locals before selecting the interface method.");for(const auto& child:part->parts)pending.push_back(child.get());}
        }
        std::vector<bool> bound(target.parameters.size());
        std::vector<unsigned> mapping;
        bool named = false, invalid = false;
        unsigned positional = 0;
        for (std::size_t n = first; n < e.parts.size(); ++n) {
            auto index = positional++;
            const auto& label = e.names[n-first];
            if (constructor && !label.text.empty()) {
                named = true;
                auto found = std::find_if(target.parameters.begin(),target.parameters.end(),[&](const auto& p) { return p.token.text == label.text; });
                if (found == target.parameters.end()) error(source,label,Code::argument_count,"Unknown initializer parameter '" + label.text + "'.");
                index = static_cast<unsigned>(found-target.parameters.begin());
            } else if (constructor && named) error(source,e.parts[n]->token,Code::argument_count,"Positional arguments must precede named arguments.");
            if (index >= bound.size()) error(source,e.token,Code::argument_count,"Incorrect argument count for '" + e.token.text + "'.");
            if (bound[index]) error(source,label.text.empty() ? e.parts[n]->token : label,Code::duplicate,"Initializer parameter is supplied more than once.");
            bound[index] = true; mapping.push_back(index);
            auto actual = expression(*e.parts[n],target.parameters[index].type);
            if(target.native_symbol=="vd_native_fixture_call#register" && index==1 && e.parts[n]->kind!=Expr::Kind::function_reference)error(source,e.parts[n]->token,Code::unsupported_type,"Native callback registration requires a directly named function or a literal closure created in this generation.");
            try { expect(actual,target.parameters[index].type,e.parts[n]->token); }
            catch (const CompilationError& failure) { diagnostics.add(failure); invalid = true; }
            invalid |= actual == error_type;
        }
        for (unsigned n=0;n<bound.size();++n) if (!bound[n]) {
            const auto& parameter = target.parameters[n];
            if (!constructor || !parameter.default_value) {
                std::vector<RelatedLocation> related;
                if (constructor) { auto [owner,structure] = owned_structure(target.receiver->type); (void)structure; related.push_back({span(owner->source,parameter.token),"Required initializer parameter declared here."}); }
                error(source,e.token,Code::argument_count,"Incorrect argument count: missing required argument '" + parameter.token.text + "'.","Supply an explicit constructed value or every required argument.",std::move(related));
            }
            auto value = std::make_unique<Expr>(Expr::Kind::literal,e.token); value->type = parameter.type;
            value->aggregate_value = *parameter.default_value; value->value = parameter.default_value->bits;
            e.parts.push_back(std::move(value)); mapping.push_back(n);
        }
        if (constructor) e.field_indexes = std::move(mapping);
        return invalid ? error_type : target.asynchronous ? unit.make_task(target.result) : target.result;
    }
    std::pair<Unit*,Structure*> owned_structure(Type type) {
        for (const auto& [id,owner] : units) for (auto& structure : owner->structures)
            if (structure.definition == type.definition) return {owner.get(),&structure};
        throw std::logic_error("Missing owned value type");
    }
    void construction_fields(Expr& e, Unit&, Structure& structure, bool runtime, bool initializer_declaration = false) {
        Depth guard(diagnostics.semantic_depth(),source,e.token,128);
        for (unsigned n=0;n<structure.effective_fields.size();++n) {
            if (initializer_declaration && structure.base_type && n<structure.base_type->definition->fields.size()) continue;
            if (!e.initializer.empty() || std::find(e.field_indexes.begin(),e.field_indexes.end(),n)==e.field_indexes.end()) {
                const auto& field = *structure.effective_fields[n].second;
                auto& field_owner=*structure.effective_fields[n].first;
                if(field.expression)continue;
                if(ir::array_type(*field.annotation) && needs_array_recipe(*field.annotation)) {auto value=array_recipe(*field.annotation,initializer_declaration ? field.token : e.token,runtime,field_owner);value->slot=n;e.omitted.push_back(std::move(value));continue;}
                if(field.annotation->kind!=Type::aggregate || (field.annotation->definition && (field.annotation->definition->callable || !field.annotation->definition->cases.empty())))continue;
                auto [child_owner,child] = owned_structure(*field.annotation);
                auto value = std::make_unique<Expr>(Expr::Kind::construction,initializer_declaration ? field.token : e.token); value->slot = n;
                try { construction(*value,*child_owner,*child,runtime,field_owner); }
                catch (CompilationError& failure) {
                    auto errors = failure.diagnostics();
                    for (auto& d : errors) d.related.push_back({span(field_owner.source,field.token),"Default construction required for this field."});
                    throw CompilationError(std::move(errors));
                }
                e.omitted.push_back(std::move(value));
            }
        }
    }
    void construction(Expr& e, Unit& owner, Structure& structure, bool runtime, Unit& access) {
        if(structure.definition->native_resource)error(source,e.token,Code::unsupported_type,"Native resources are nonforgeable; use the checked binding factory.");
        if(structure.definition->interface_type) error(source,e.token,Code::initializer,"An interface cannot be constructed; convert a complete conforming managed value.");
        defaults(owner,structure);
        if (structure.definition->managed && !runtime) error(source,e.token,Code::constant_call,"Managed allocation is not a compile-time expression.");
        e.kind = Expr::Kind::construction; e.type = Type(structure.definition,structure.definition->interface_type ? Type::interface_ref : structure.definition->managed ? Type::managed_ref : Type::aggregate); e.target = structure.definition->identity;
        e.aggregate_value = structure.defaults;
        if (auto* init = structure.initializer) {
            if (!runtime) error(source,e.token,Code::constant_call,"Compile-time expressions cannot execute custom initialization.","Construct this value in a function or local const.",{{span(owner.source,init->token),"Runtime initializer declared here."}});
            if (init->file_private && &access != &owner) error(source,e.token,Code::visibility,"Initializer is private to " + owner.identity + ".");
            e.initializer = init->symbol;
            if (call_arguments(e,*init,0,true) == error_type) e.type = error_type;
        } else {
            std::set<unsigned> initialized;
            bool invalid=false;
            for (unsigned n=0;n<e.parts.size();++n) {
                if (e.names[n].text.empty()) error(source,e.parts[n]->token,Code::argument_count,"Construction requires named fields.","Use field = expression for a type without init.");
                auto& fields = structure.definition->fields;
                auto field = std::find_if(fields.begin(),fields.end(),[&](const auto& f) { return f.name == e.names[n].text; });
                if(field!=fields.end() && field->event)error(source,e.names[n],Code::initializer,"Events always start empty; subscribe after construction.");
                if (field == fields.end()) error(source,e.names[n],Code::unknown_identifier,"Unknown constructor field '" + e.names[n].text + "'.");
                auto index = static_cast<unsigned>(field-fields.begin());
                if (!initialized.insert(index).second) error(source,e.names[n],Code::duplicate,"Duplicate initializer for field '" + field->name + "'.");
                e.field_indexes.push_back(index);
                auto actual=expression(*e.parts[n],field->type);
                invalid|=actual==error_type;
                expect(actual,field->type,e.parts[n]->token);
            }
            if (invalid) throw Poison{};
        }
        if (!structure.initializer) for (unsigned n=0;n<structure.effective_fields.size();++n) {
            const auto& field=*structure.effective_fields[n].second; auto& field_owner=*structure.effective_fields[n].first;
            Type t = *field.annotation;
            if (ir::requires_initialization(t) && std::find(e.field_indexes.begin(),e.field_indexes.end(),n)==e.field_indexes.end())
                error(source,e.token,Code::argument_count,"A non-nullable reference field requires an explicit value: " + field.name + ".",{},{{span(field_owner.source,field.token),"Required field declared here."}});
        }
        if (!structure.initializer || !structure.definition->managed) construction_fields(e,owner,structure,runtime);
    }
    void expect(Type actual, Type expected, const Token& token) {
        if (actual == error_type || expected == error_type) return;
        if (actual.kind == Type::managed_ref && expected.kind == Type::managed_ref && ir::subtype(actual,expected) && (!actual.nullable || expected.nullable)) return;
        if(actual.kind==Type::interface_ref && expected.kind==Type::interface_ref && Type::key(actual)==Type::key(expected) && (!actual.nullable || expected.nullable)) return;
        if(actual.definition && expected.definition && actual.definition->callable && expected.definition->callable && Type::key(actual)==Type::key(expected) && (!actual.nullable || expected.nullable))return;
        if (actual != expected) error(source, token, Code::type_mismatch, "Type mismatch: expected " + std::string(ir::type_name(expected)) + ", received " + ir::type_name(actual) + ".", {}, {}, ir::type_name(expected), ir::type_name(actual));
    }
    unsigned declare(const Token& token, Type type, bool readonly = false) {
        if (lookup(token.text) || visible_names.contains(token.text)) {
            std::vector<RelatedLocation> related;
            if (auto* previous = lookup(token.text)) {
                related.push_back({span(source, previous->declaration), "Previously declared"});
                previous->type = error_type;
            }
            else {
                auto add = [&](const Unit& owner) {
                    if (auto c = owner.constant_names.find(token.text); c != owner.constant_names.end() && (!c->second->file_private || &owner == &unit))
                        related.push_back({span(owner.source, c->second->declaration.token), "Previously declared"});
                    if (auto f = owner.declarations.find(token.text); f != owner.declarations.end() && (!f->second->file_private || &owner == &unit))
                        related.push_back({span(owner.source, f->second->token), "Previously declared"});
                };
                add(unit); if (&root != &unit) add(root);
                for (const auto& import : unit.imports) if (!import.target.empty() && !import.explicit_alias) add(*units.at(import.target));
            }
            error(source, token, Code::duplicate, "Duplicate declaration or shadowing of '" + token.text + "'.", "Assign to the existing variable, or choose a distinct name.", std::move(related));
        }
        auto slot = static_cast<unsigned>(current->locals.size()); current->locals.push_back(type);
        scopes.back().emplace(token.text, Variable{type, slot, readonly, token,current}); return slot;
    }
    // Qualified lookup needs a temporary AST node only on this cold path.
    // Keep it out of every recursive receiver-chain hint's native stack frame.
    VOID_SEMANTIC_HANDLER std::optional<Type> qualified_anchor(Expr& e,Expr::Kind kind) {
        Expr qualified(kind,e.token);qualified.qualifier=e.parts[0]->token.text;qualified.qualifier_token=e.parts[0]->token;
        if(kind==Expr::Kind::call) {
            auto [owner,structure]=structure_lookup(unit,root,units,qualified.qualifier+"."+e.token.text,qualified.qualifier_token);
            if(structure){auto& concrete=concrete_structure(e,*owner,*structure,1);return Type(concrete.definition,concrete.definition->managed?Type::managed_ref:Type::aggregate);}
            return concrete_function(e,resolve(qualified),1).result;
        }
        return anchor(qualified);
    }
    // Hints resolve typed anchors without prematurely defaulting untyped literals.
    std::optional<Type> anchor(Expr& e) {
        try {
        // Hints share the semantic budget; an exhausted hint falls back to
        // ordinary checking, which reports the existing nesting diagnostic.
        Depth guard(diagnostics.semantic_depth(),source,e.token,256);
        using K = Expr::Kind;
        if (e.kind == K::literal) {
            if (e.token.text == "null") return {};
            if (e.literal_text.empty()) return e.type;
            if (e.literal_text.back() == 'f') return Type::f32;
            if (e.literal_text.back() == 'd') return Type::f64;
            return {};
        }
        if (e.kind == K::conversion) return e.type;
        if (e.kind == K::name) {
            if (auto* v = e.qualifier.empty() ? lookup(e.token.text) : nullptr) return v->type;
            auto [owner,c] = constant(e); if (c) { evaluate(*owner,*c); return c->type; }
            return {};
        }
        if(e.kind==K::array_index || e.kind==K::array_slice) {
            auto owner=anchor(*e.parts[0]);
            if(owner && owner->kind==Type::dictionary && e.kind==K::array_index)return ir::dictionary_value(*owner);
            if(owner && ir::array_type(*owner)) return e.kind==K::array_slice?slice_type(*owner):ir::array_element(*owner);
            return {};
        }
        if(e.kind==K::array_literal) return {};
        if (e.kind == K::method_call) {
            if (module_call(e)) {
                return qualified_anchor(e,K::call);
            }
            auto receiver = anchor(*e.parts[0]);
            if (!receiver) return std::nullopt;
            if(ir::array_type(*receiver) && e.token.text=="remove_at")return ir::array_element(*receiver);
            if(auto* specialized=concrete_method(e,*receiver))return specialized->result;
            auto selected=member(*receiver,e.token,true);
            return selected.function ? std::optional<Type>(selected.function->result) : std::nullopt;
        }
        if (e.kind == K::call) {
            if(auto* value=lookup(e.token.text);value && value->type.definition && value->type.definition->callable)return Type(*value->type.definition->call_result);
            if(current && e.qualifier.empty() && !lookup(e.token.text) && current->substitutions.contains(e.token.text))return current->substitutions.at(e.token.text);
            auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
            if (structure) {auto& concrete=concrete_structure(e,*owner,*structure);return Type(concrete.definition,concrete.definition->interface_type?Type::interface_ref:concrete.definition->managed?Type::managed_ref:Type::aggregate);}
            return concrete_function(e,resolve(e)).result;
        }
        if (e.kind == K::field) {
            const auto& operand = *e.parts[0];
            if (operand.kind == K::name && !lookup(operand.token.text) && unit.qualifiers.contains(operand.token.text)) {
                return qualified_anchor(e,K::name);
            }
            auto base = anchor(*e.parts[0]);
            if(base && ir::collection_type(*base) && e.token.text=="count")return Type::i64;
            if (base && (base->kind == Type::aggregate || base->kind == Type::managed_ref)) {
                auto selected=member(*base,e.token);
                if (!selected.function) { auto t=*base; for (auto index:selected.path) t=ir::field_type(t,index); return t; }
            }
            return {};
        }
        if (e.op == "not" || e.op == "and" || e.op == "or" || e.op == "==" || e.op == "!=" || e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") return Type::boolean;
        for (auto& part : e.parts) if (auto t = anchor(*part)) return t;
        return {};
        } catch (const CompilationError&) { return {}; }
          catch (const Poison&) { return {}; }
    }
    bool decimal(const Expr& e) {
        if (e.kind == Expr::Kind::literal && e.literal_text.find('.') != std::string::npos) return true;
        for (auto& part : e.parts) if (decimal(*part)) return true;
        return false;
    }
    // Apply contextual rewrites only after the recursive handler has returned.
    Type interface_conversion(Expr& e,Type from,Type target) {
        if(from.nullable && !target.nullable) error(source,e.token,Code::type_mismatch,"Unwrap the concrete nullable reference before converting to a non-nullable interface.");
        auto concrete=from;concrete.nullable=false;
        ir::Module::Binding binding;
        try { binding=ir::structural_binding(*root.member_catalog,concrete,target,unit.identity); }
        catch(const std::invalid_argument& failure) {error(source,e.token,Code::type_mismatch,failure.what());}
        catch(const std::length_error& failure) {error(source,e.token,Code::limit,failure.what());}
        if(!current && !(from.nullable && target.nullable && constant_value(source,e).bits==0)) error(source,e.token,Code::constant_call,"Interface bindings are runtime views; only nullable all-null templates are compile-time values.");
        auto& bindings=root.member_catalog->interface_bindings;
        if(std::none_of(bindings.begin(),bindings.end(),[&](const auto& b){return b.key==binding.key;})) bindings.push_back(binding);
        auto operand=std::make_unique<Expr>(std::move(e));
        e=Expr(Expr::Kind::conversion,operand->token);e.op="interface";e.target=binding.key;e.implicit=true;
        e.type=target;e.type.readonly=from.readonly;e.parts.push_back(std::move(operand));return e.type;
    }
    VOID_SEMANTIC_HANDLER Type contextual_conversion(Expr& e, Type type, const std::optional<Type>& expected) {
        if(expected && expected->kind==Type::interface_ref && type!=error_type && type.kind!=Type::interface_ref) return interface_conversion(e,type,*expected);
        if(expected && type!=error_type && expected->definition && expected->definition->callable && type.definition && !type.definition->callable && !type.nullable && (type.kind==Type::aggregate || type.kind==Type::managed_ref || type.kind==Type::interface_ref)){
            return bind_method(e,Token{TokenKind::name,"call",e.token.offset},*expected);
        }
        if (expected && type!=error_type && ir::subtype(type,*expected) && (!type.nullable || expected->nullable) && Type::key(type)!=Type::key(*expected)) {
            auto operand=std::make_unique<Expr>(std::move(e));
            e=Expr(Expr::Kind::conversion,operand->token); e.op="upcast"; e.implicit=true;
            e.type=*expected; e.type.readonly=type.readonly; e.parts.push_back(std::move(operand)); type=e.type;
        }
        return type;
    }
    Type expression(Expr& e, std::optional<Type> expected = {}) {
        try {
            Depth guard(diagnostics.semantic_depth(), source, e.token, 256);
            auto outer_bindings=pattern_bindings;if(e.kind!=Expr::Kind::test)pattern_bindings=false;
            auto type = expression_impl(e, expected);pattern_bindings=outer_bindings;
            type = contextual_conversion(e,type,expected);
            if (type == error_type) { e.type = error_type; invalid_function = true; }
            return type;
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
          catch (const Poison&) {}
        invalid_function = true;
        return e.type = error_type;
    }
    VOID_SEMANTIC_HANDLER Type spawned_expression(Expr& e) {
        if(!current || !current->asynchronous || current->deferred)error(source,e.token,Code::unsupported_type,"spawn requires an async body outside deferred cleanup.");
        auto result=expression(*e.parts[0]);auto& call=*e.parts[0];
        if(call.kind!=Expr::Kind::call || call.asynchronous_call || call.target.empty() || call.target.starts_with("$"))error(source,e.token,Code::unsupported_type,"spawn requires a directly named synchronous noncapturing function.");
        std::vector<Type> transfer{result};for(const auto& part:call.parts)transfer.push_back(part->type);
        while(!transfer.empty()){auto type=transfer.back();transfer.pop_back();if(!ir::worker_transfer_safe(type))error(source,e.token,Code::unsupported_type,"spawn transfers only scalars, immutable strings and recursively safe fixed values.");
            if(type.kind==Type::aggregate){Token deinit=e.token;deinit.text="deinit";if(direct_method(type,deinit))error(source,e.token,Code::unsupported_type,"Native/resource wrappers cannot cross a worker boundary.");for(const auto& field:type.definition->fields)transfer.push_back(field.type);}
            else if(type.kind==Type::fixed_array)transfer.push_back(ir::array_element(type));
        }
        return e.type=unit.make_task(result);
    }
    VOID_SEMANTIC_HANDLER Type awaited_expression(Expr& e) {
        if(!current || !current->asynchronous || current->deferred)error(source,e.token,Code::unsupported_type,"await requires an async function and is not allowed in deferred code.");
        auto task=expression(*e.parts[0]);if(!task.definition || !task.definition->task_result || task.kind!=Type::managed_ref || task.nullable || task.construction)error(source,e.token,Code::type_mismatch,"await requires a non-null inferred task handle.");
        return e.type=Type(*task.definition->task_result)==Type::none?Type::none:ir::field_type(task,1);
    }
    // Keep the recursive dispatch frame small: reserving every handler's
    // temporaries here exhausts a 1 MiB Debug stack before the depth guard.
    Type expression_impl(Expr& e, std::optional<Type> expected) {
        switch (e.kind) {
        case Expr::Kind::spawned:return spawned_expression(e);
        case Expr::Kind::awaited:return awaited_expression(e);
        case Expr::Kind::lambda:return lambda_expression(e);
        case Expr::Kind::task_cancel:case Expr::Kind::capture_reference:case Expr::Kind::function_null: case Expr::Kind::function_reference:return e.type;
        case Expr::Kind::function_call:return function_invocation(e);
        case Expr::Kind::test:return test_expression(e);
        case Expr::Kind::match:return match_expression(e,expected);
        case Expr::Kind::sum:return e.type;
        case Expr::Kind::propagate:return propagate_expression(e);
        case Expr::Kind::construction: return e.type; // An inferred literal may revisit its already checked constructor.
        case Expr::Kind::dictionary_literal: return dictionary_expression(e,expected);
        case Expr::Kind::array_literal: return array_expression(e,expected);
        case Expr::Kind::array_index: return index_expression(e);
        case Expr::Kind::array_slice: return slice_expression(e);
        case Expr::Kind::literal: return literal_expression(e, expected);
        case Expr::Kind::name: return name_expression(e);
        case Expr::Kind::field: return field_expression(e, expected);
        case Expr::Kind::call:
            if(e.token.text=="Ok" || e.token.text=="Err")return result_expression(e,expected);
            return call_expression(e);
        case Expr::Kind::method_call: return method_expression(e, expected);
        case Expr::Kind::conversion: return conversion_expression(e);
        case Expr::Kind::unary: return unary_expression(e, expected);
        default: return binary_expression(e, expected);
        }
    }
    VOID_SEMANTIC_HANDLER Type test_expression(Expr& e){
        if(!current)error(source,e.token,Code::constant_call,"Patterns require a function body; compile-time pattern evaluation is not implemented.");
        auto subject=expression(*e.parts[0]);if(!pattern_bindings)scopes.emplace_back();check_pattern(e.patterns[0],subject);if(!pattern_bindings)scopes.pop_back();return e.type=Type::boolean;
    }
    VOID_SEMANTIC_HANDLER Type match_expression(Expr& e,std::optional<Type> expected){
        if(!current)error(source,e.token,Code::constant_call,"Patterns require a function body; compile-time pattern evaluation is not implemented.");return check_match(e,expected);
    }
    VOID_SEMANTIC_HANDLER Type propagate_expression(Expr& e){
        auto type=expression(*e.parts[0]);
        if(current && current->deferred)error(source,e.token,Code::invalid_value,"? cannot propagate out of deferred code.");
        if(!current || !type.definition || !type.definition->result_type || !current->result.definition || !current->result.definition->result_type)error(source,e.token,Code::type_mismatch,"? requires a result value in a result-returning function.");
        expect(ir::field_type(type,2),ir::field_type(current->result,2),e.token);return e.type=ir::field_type(type,1);
    }
#include "pattern_semantics.inc"
    VOID_SEMANTIC_HANDLER Type lambda_expression(Expr& e) {
        if(!current)error(source,e.token,Code::constant_call,"A closure needs a function activation.");
        auto parsed=current->lambdas.begin();std::advance(parsed,e.slot);unit.functions.push_back(std::move(*parsed));auto& lambda=unit.functions.back();
        lambda.symbol=current->symbol+"::$lambda$"+std::to_string(e.slot);lambda.concrete_name=lambda.symbol.substr(unit.identity.size()+2);lambda.display_name="<lambda>";lambda.prepared=true;lambda.instantiation_context=current->instantiation_context;lambda.specialization_depth=current->specialization_depth;lambda.substitutions=current->substitutions;lambda.substitution_sources=current->substitution_sources;
        for(auto& parameter:lambda.parameters)parameter.type=resolve_type(parameter.type,unit,root,units,parameter.token);lambda.result=resolve_type(lambda.result,unit,root,units,e.token);
        std::vector<Type> parameters;for(const auto& parameter:lambda.parameters)parameters.push_back(parameter.type);
        lambda.parameters.insert(lambda.parameters.begin(),Parameter{Token{TokenKind::name,"$environment",e.token.offset},Type::none});
        Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.enclosing=[&](const std::string& name){return lookup(name);};checker.analyze_one(lambda);
        auto environment=unit.make_capture_record(lambda.symbol+"::$captures",Type::none,false);
        auto definition=std::const_pointer_cast<ir::ValueType>(environment.definition);
        for(unsigned n=0;n<lambda.captures.size();++n){const auto& capture=lambda.captures[n];definition->fields.push_back({"$capture$"+std::to_string(n),capture.cell,definition->identity});auto reference=std::make_unique<Expr>(Expr::Kind::capture_reference,capture.token);reference->slot=capture.outer_slot;reference->type=capture.cell;reference->implicit=capture.snapshot;e.parts.push_back(std::move(reference));}
        lambda.parameters[0].type=environment;lambda.locals[lambda.parameters[0].slot]=environment;
        root.member_catalog->functions.push_back(member_header_for_lambda(lambda));
        e.environment=environment;e.implicit=true;e.kind=Expr::Kind::function_reference;e.target=lambda.symbol;return e.type=unit.make_callable(parameters,lambda.result);
    }
    ir::Function member_header_for_lambda(const Function& function) {
        ir::Function header;header.name=function.symbol;header.module_identity=unit.identity;header.result=function.result;header.bound_receiver=function.bound_receiver;header.closure=true;header.parameters.clear();for(const auto& parameter:function.parameters)header.parameters.push_back(parameter.type);header.visibility="private";return header;
    }
    VOID_SEMANTIC_HANDLER Type bind_method(Expr& value,const Token& method,Type signature,std::vector<Type> type_arguments={}){
        if(!current)error(source,method,Code::constant_call,"Bound callbacks require a function activation; use a named function in a pure template.");
        auto receiver=value.type;auto original=std::make_unique<Expr>(std::move(value));
        Member selected{};if(receiver.kind==Type::interface_ref || ir::select_member(*root.member_catalog,receiver,method.text,unit.identity).status!=ir::MemberSelection::missing)selected=member(receiver,method);
        if(selected.function){
            for(auto field:selected.path)embedded_receiver(original,field,method);receiver=original->type;
            auto* target=selected.function;
            if(!target->virtual_slot.empty())for(auto& [id,owner]:units)for(auto& function:owner->functions)if(function.symbol==target->virtual_slot)target=&function;
            auto canonical=target->receiver->type;
            if(receiver.kind==Type::managed_ref && canonical.kind==Type::managed_ref && Type::key(receiver)!=Type::key(canonical)){
                auto source=std::move(original);original=std::make_unique<Expr>(Expr::Kind::conversion,source->token);original->op="upcast";original->implicit=true;canonical.readonly=receiver.readonly;original->type=canonical;original->parts.push_back(std::move(source));receiver=canonical;
            }
        }
        const bool direct=receiver.kind==Type::managed_ref || receiver.kind==Type::interface_ref;
        auto identity=current->symbol+"::$bound$"+std::to_string(current->binding_ordinal++)+"$"+method.text;
        if(direct)identity=unit.identity+"::$bound$"+Type::key(receiver)+"$"+method.text+"$"+Type::key(signature);
        for(auto argument:type_arguments){auto key=ir::array_identity(resolve_type(argument,unit,root,units,method),{});identity+="$"+std::to_string(key.size())+":"+key;}
        Function* existing=nullptr;for(auto& f:unit.functions)if(f.symbol==identity)existing=&f;
        if(!existing){
            Function adapter;adapter.token=method;adapter.symbol=identity;adapter.concrete_name=identity.substr(unit.identity.size()+2);adapter.display_name="<bound "+method.text+">";adapter.visibility="private";adapter.prepared=true;adapter.instantiation_context=current->instantiation_context;adapter.specialization_depth=current->specialization_depth;adapter.result=*signature.definition->call_result;
            Token receiver_name{TokenKind::name,"$bound_receiver",method.offset};adapter.parameters.push_back({receiver_name,receiver});
            auto call=std::make_unique<Expr>(Expr::Kind::method_call,method);call->type_arguments=type_arguments;call->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,receiver_name));
            for(unsigned n=0;n<signature.definition->call_parameters.size();++n){Token name{TokenKind::name,"$argument$"+std::to_string(n),method.offset};adapter.parameters.push_back({name,signature.definition->call_parameters[n]});call->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,name));call->names.push_back({});}
            Statement invoke(adapter.result==Type::none?Statement::Kind::expression:Statement::Kind::returned,method);invoke.expression=std::move(call);adapter.body.push_back(std::move(invoke));unit.functions.push_back(std::move(adapter));existing=&unit.functions.back();
            Semantics checker(unit,units,root,evaluate,diagnostics,defaults);checker.analyze_one(*existing);existing->anonymous=true;
            {
                existing->bound_receiver=direct;
                auto environment=unit.make_capture_record(identity+"::$receiver",receiver,true);std::const_pointer_cast<ir::ValueType>(environment.definition)->base=signature.definition->fields[1].type.target;
                existing->parameters[0].type=environment;existing->locals[0]=environment;
                auto& invoke=*existing->body[0].expression;auto field=std::make_unique<Expr>(Expr::Kind::field,receiver_name);field->slot=0;field->type=receiver;field->parts=std::move(invoke.parts[0]->parts);field->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,receiver_name));field->parts[0]->type=environment;field->parts[0]->slot=0;invoke.parts[0]=std::move(field);invoke.field_indexes.insert(invoke.field_indexes.begin(),0);
            }
            root.member_catalog->functions.push_back(member_header_for_lambda(*existing));
        }
        value=Expr(Expr::Kind::function_reference,method);value.type=signature;value.target=identity;value.environment=existing->parameters[0].type;value.op="value_bound";value.implicit=true;value.parts.push_back(std::move(original));return signature;
    }
    VOID_SEMANTIC_HANDLER void deferred_call(Statement& statement) {
        auto& call=*statement.expression;expression(call);
        if(call.kind!=Expr::Kind::call && call.kind!=Expr::Kind::method_call && call.kind!=Expr::Kind::function_call)error(source,statement.token,Code::invalid_value,"defer requires a call or a deferred block.");
        Function thunk;thunk.anonymous=true;thunk.deferred=true;thunk.prepared=thunk.analyzed=true;thunk.token=statement.token;thunk.token.text="$deferred$"+std::to_string(unit.functions.size());thunk.symbol=current->symbol+"::"+thunk.token.text;thunk.concrete_name=thunk.symbol.substr(unit.identity.size()+2);thunk.display_name="<defer>";thunk.visibility="private";
        auto environment=unit.make_capture_record(thunk.symbol+"::$captures",Type::none,false);auto definition=std::const_pointer_cast<ir::ValueType>(environment.definition);
        thunk.parameters.push_back({Token{TokenKind::name,"$environment",statement.token.offset},environment});thunk.locals.push_back(environment);
        auto reference=std::make_unique<Expr>(Expr::Kind::function_reference,statement.token);reference->target=thunk.symbol;reference->type=unit.make_callable({},Type::none);reference->environment=environment;reference->implicit=true;
        for(unsigned n=0;n<call.parts.size();++n){
            auto& part=call.parts[n];auto type=part->type;auto cell=unit.make_capture_record(thunk.symbol+"::$cell$"+std::to_string(n),type,true);definition->fields.push_back({"$capture$"+std::to_string(n),cell,definition->identity});
            auto snapshot=std::make_unique<Expr>(Expr::Kind::capture_reference,part->token);snapshot->type=cell;snapshot->implicit=true;snapshot->parts.push_back(std::move(part));reference->parts.push_back(std::move(snapshot));
            auto slot=static_cast<unsigned>(thunk.locals.size());thunk.locals.push_back(type);thunk.captured_locals.emplace(slot,cell);thunk.capture_inputs.emplace(slot,n);
            part=std::make_unique<Expr>(Expr::Kind::name,reference->parts.back()->token);part->type=type;part->slot=slot;
        }
        call.receiver_addressable=false;call.selected_receiver=false;
        Statement invoke(Statement::Kind::expression,statement.token);invoke.expression=std::move(statement.expression);thunk.body.push_back(std::move(invoke));statement.expression=std::move(reference);unit.functions.push_back(std::move(thunk));root.member_catalog->functions.push_back(member_header_for_lambda(unit.functions.back()));
    }
    VOID_SEMANTIC_HANDLER Type function_invocation(Expr& e) {
        auto type=expression(*e.parts[0]);if(!type.definition || !type.definition->callable)error(source,e.token,Code::not_callable,"Value is not a function.");
        const auto& parameters=type.definition->call_parameters;
        if(e.parts.size()!=parameters.size()+1 || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect positional function-value arguments.");
        for(unsigned n=0;n<parameters.size();++n)expect(expression(*e.parts[n+1],Type(parameters[n])),Type(parameters[n]),e.parts[n+1]->token);
        return e.type=Type(*type.definition->call_result);
    }
    Type sum_expression(Expr& e,Type type,unsigned tag,unsigned first) {
        const auto& variant=type.definition->cases.at(tag);
        if(e.parts.size()-first!=variant.count || std::any_of(e.names.begin(),e.names.end(),[](const auto& name){return !name.text.empty();}))error(source,e.token,Code::argument_count,"Incorrect variant payload arguments.");
        for(unsigned n=0;n<variant.count;++n)expect(expression(*e.parts[first+n],ir::field_type(type,variant.begin+n)),ir::field_type(type,variant.begin+n),e.parts[first+n]->token);
        if(first)e.parts.erase(e.parts.begin(),e.parts.begin()+first);
        e.kind=Expr::Kind::sum;e.slot=tag;return e.type=type;
    }
    Type result_expression(Expr& e,std::optional<Type> expected) {
        if(!expected || !expected->definition || !expected->definition->result_type)error(source,e.token,Code::type_mismatch,"Ok/Err require a contextual result<t, e> type.","Annotate the variable or the enclosing function result.");
        return sum_expression(e,*expected,e.token.text=="Ok"?0:1,0);
    }
    std::optional<Type> enum_expression(Expr& e,bool call) {
        auto& base=*e.parts[0];std::string spelling;
        if(base.kind==Expr::Kind::name && !lookup(base.token.text))spelling=base.token.text;
        else if(base.kind==Expr::Kind::field && base.parts[0]->kind==Expr::Kind::name && unit.qualifiers.contains(base.parts[0]->token.text))spelling=base.parts[0]->token.text+"."+base.token.text;
        if(spelling.empty())return {};
        auto [owner,structure]=structure_lookup(unit,root,units,spelling,base.token);
        if(!structure || structure->definition->cases.empty())return {};
        const auto& variants=structure->definition->cases;
        auto found=std::find_if(variants.begin(),variants.end(),[&](const auto& variant){return variant.name==e.token.text;});
        if(found==variants.end())error(source,e.token,Code::unknown_identifier,"Unknown enum variant '"+e.token.text+"'.");
        if(!call && found->count)error(source,e.token,Code::argument_count,"Variant requires its payload arguments.");
        auto result=sum_expression(e,Type(structure->definition),static_cast<unsigned>(found-variants.begin()),1);e.target=structure->definition->identity+"::variant::"+found->name;return result;
    }
    VOID_SEMANTIC_HANDLER Type dictionary_expression(Expr& e,std::optional<Type> expected) {
        if(!expected){
            if(e.parts.empty())error(source,e.token,Code::unsupported_type,"Empty dictionary literal requires a typed context.");
            std::optional<Type> key,value;for(unsigned n=0;n<e.parts.size();n+=2){if(!key)key=anchor(*e.parts[n]);if(!value)value=anchor(*e.parts[n+1]);}
            if(!key)key=expression(*e.parts[0]);if(!value)value=expression(*e.parts[1]);
            if(*key==Type::null_value || *value==Type::null_value || *key==Type::none || *value==Type::none)error(source,e.token,Code::unsupported_type,"Dictionary entries require concrete types or a typed context.");expected=unit.make_dictionary(*key,*value);
        }
        if(expected->kind!=Type::dictionary)error(source,e.token,Code::type_mismatch,"Dictionary literal requires dictionary context.");
        if(!ir::dictionary_key_supported(ir::dictionary_key(*expected)))error(source,e.token,Code::unsupported_type,"Unsupported dictionary key type.");
        for(unsigned n=0;n<e.parts.size();n+=2){expect(expression(*e.parts[n],ir::dictionary_key(*expected)),ir::dictionary_key(*expected),e.parts[n]->token);expect(expression(*e.parts[n+1],ir::dictionary_value(*expected)),ir::dictionary_value(*expected),e.parts[n+1]->token);}
        return e.type=*expected;
    }
    VOID_SEMANTIC_HANDLER Type array_expression(Expr& e,std::optional<Type> expected) {
        if(!expected) {
            if(e.parts.empty())error(source,e.token,Code::unsupported_type,"An empty array literal requires contextual typing.");
            std::optional<Type> element;
            for(auto& part:e.parts) if(auto hint=anchor(*part)) {element=hint;break;}
            if(!element)element=expression(*e.parts.front());
            if(*element==Type::none || *element==Type::null_value)error(source,e.token,Code::unsupported_type,"Array element requires a concrete value type.");
            *element=ir::with_readonly_paths(*element,{});auto definition=std::make_shared<ir::ValueType>();definition->identity=ir::array_identity(*element,{});definition->name=definition->identity;definition->element=ir::FieldType(*element);
            expected=Type(definition,Type::growable_array);
        }
        if(!ir::array_type(*expected)) error(source,e.token,Code::unsupported_type,"Array literals require a fixed or growable array context.");
        if(expected->kind==Type::fixed_array && e.parts.size()!=*expected->definition->extent) error(source,e.token,Code::type_mismatch,"Fixed-array literal requires exactly "+std::to_string(*expected->definition->extent)+" elements.");
        for(auto& part:e.parts) expect(expression(*part,ir::array_element(*expected)),ir::array_element(*expected),part->token);
        return e.type=*expected;
    }
    Type slice_type(Type source) {
        auto element=ir::array_element(source);auto definition=std::make_shared<ir::ValueType>();definition->identity=definition->name=ir::array_identity(element,{});definition->element=ir::FieldType(element);
        Type result(definition,Type::growable_array);ir::ReferencePaths paths;
        for(auto path:ir::readonly_paths(element)){path.insert(path.begin(),ir::array_element_path);paths.insert(std::move(path));}
        return ir::with_readonly_paths(result,paths);
    }
    VOID_SEMANTIC_HANDLER Type slice_expression(Expr& e) {
        auto array=expression(*e.parts[0]);if(array==error_type)return error_type;
        if(!ir::array_type(array))error(source,e.token,Code::unsupported_type,"Slicing requires an array.");
        for(unsigned n=1;n<3;++n)if(!numeric::integer(expression(*e.parts[n])))error(source,e.parts[n]->token,Code::type_mismatch,"Slice endpoint requires an integer, excluding bool and floats.");
        if(!current)error(source,e.token,Code::constant_call,"Slicing is a runtime array operation.");
        return e.type=slice_type(array);
    }
    VOID_SEMANTIC_HANDLER Type index_expression(Expr& e) {
        auto array=expression(*e.parts[0]);
        if(array==error_type) return error_type;
        if(array.kind==Type::dictionary){if(!current)error(source,e.token,Code::constant_call,"Dictionary lookup requires runtime evaluation.");expect(expression(*e.parts[1],ir::readonly_view(ir::dictionary_key(array))),ir::readonly_view(ir::dictionary_key(array)),e.parts[1]->token);return e.type=ir::dictionary_value(array);}
        if(!ir::array_type(array)) error(source,e.token,Code::unsupported_type,"Indexing requires an array or dictionary.");
        auto index=expression(*e.parts[1]);
        if(index==error_type) return error_type;
        if(!numeric::integer(index)) error(source,e.parts[1]->token,Code::type_mismatch,"Array index requires an integer, excluding bool and floating types.");
        return e.type=ir::array_element(array);
    }
    VOID_SEMANTIC_HANDLER Type literal_expression(Expr& e, std::optional<Type> expected) {
        if (e.token.text == "null") {
            if(expected && expected->definition && expected->definition->callable && expected->nullable){e.kind=Expr::Kind::function_null;return e.type=*expected;}
            if (!expected || !ir::reference_value(*expected) || !expected->nullable) error(source,e.token,Code::type_mismatch,"null requires a nullable managed-reference context.","Use a type annotation such as node?.");
            return e.type = *expected;
        }
        if (e.literal_text.empty()) return e.type;
        Type target = anchor(e).value_or(expected && numeric::number(*expected) && (!decimal(e) || numeric::floating(*expected)) ? *expected : decimal(e) ? Type::f32 : Type::i32);
        try { e.value = numeric::literal(e.literal_text,target); }
        catch (const std::invalid_argument& failure) { error(source,e.token,numeric::floating(target) ? Code::numeric_representability : Code::integer_range,failure.what()); }
        return e.type = target;
    }
    VOID_SEMANTIC_HANDLER Type name_expression(Expr& e) {
        if(!(e.qualifier.empty() && lookup(e.token.text))) {
            bool candidate=false;
            if(e.qualifier.empty() && !unit.constant_names.contains(e.token.text) && !(root.constant_names.contains(e.token.text) && (!root.constant_names.at(e.token.text)->file_private || &root==&unit))){for(const auto& [id,u]:units)candidate|=u->declarations.contains(e.token.text);}
            else if(auto found=unit.qualifiers.find(e.qualifier);found!=unit.qualifiers.end())for(auto* u:found->second)candidate|=u->declarations.contains(e.token.text);
            if(candidate) {
                auto* function=&resolve(e);
                if(!function->generics.empty()){
                    if(e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"A generic function value requires explicit concrete type arguments.");
                    std::vector<Type> arguments;for(auto type:e.type_arguments)arguments.push_back(resolve_type(type,unit,root,units,e.token));auto& owner=generic_owner(*function);function=&unit.specialize_function(unit,owner,const_cast<Function&>(*function),arguments,e.token,current?current->specialization_depth+1:0,Type::none);
                }else if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"This function value is not generic.");
                if(function->asynchronous)error(source,e.token,Code::unsupported_type,"Async function values require a reviewed async callable contract; call the function directly.");
                std::vector<Type> parameters;for(const auto& parameter:function->parameters)parameters.push_back(parameter.type);
                e.target=function->symbol;e.kind=Expr::Kind::function_reference;return e.type=unit.make_callable(parameters,function->result);
            }
        }
        if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Explicit value type arguments require a named generic function.");
        using K = Expr::Kind;
        auto* v = e.qualifier.empty() ? lookup(e.token.text) : nullptr;
        if (!v) {
            auto [owner, c] = constant(e);
            if (c) {
                evaluate(*owner, *c); e.target = owner->identity + "::" + c->declaration.name; e.kind = K::literal; e.value = c->value;
                e.aggregate_value = c->aggregate_value;
                // Fresh list objects in pure templates retain readonly authority.
                // Existing null reference templates remain ordinary value defaults.
                auto paths=ir::readonly_paths(c->type);
                for(const auto& path:ir::reference_paths(c->type)) {
                    auto leaf=c->type;for(auto field:path)leaf=ir::field_type(leaf,field);
                    if(leaf.kind==Type::growable_array || leaf.kind==Type::dictionary)paths.insert(path);
                }
                return e.type = ir::with_readonly_paths(c->type,paths);
            }
        }
        if (!v && unit.failed_import) throw Poison{};
        if (!v) error(source, e.token, Code::unknown_identifier, "Unknown identifier '" + e.token.text + "'.", "Declare it with var before use.");
        if(v->task_group)error(source,e.token,Code::unsupported_type,"Task group aliases are lexical scopes, not first-class values; jobs.spawn is deferred.");
        e.slot = v->slot; return e.type = v->type;
    }
    VOID_SEMANTIC_HANDLER Type field_expression(Expr& e, std::optional<Type> expected) {
        using K = Expr::Kind;
        auto& base = *e.parts[0];
        if (base.kind == K::name && !lookup(base.token.text) && unit.qualifiers.contains(base.token.text)) {
            e.qualifier = base.token.text; e.qualifier_token = base.token; e.kind = K::name; e.parts.clear();
            return expression_impl(e,expected);
        }
        if(auto variant=enum_expression(e,false))return *variant;
        auto t = expression(base);
        if (t == error_type) return error_type;
        if(t.definition && t.definition->task_result)error(source,e.token,Code::unsupported_type,"Inferred task handles support await; task internals are not source fields.");
        if(base.event)error(source,e.token,Code::invalid_value,"Event subscription storage is private.");
        if(ir::collection_type(t) && e.token.text=="count") {if(t.kind==Type::dictionary && !current)error(source,e.token,Code::constant_call,"Dictionary count requires runtime evaluation.");e.target=t.kind==Type::dictionary?"$dict.count":"$array.count";return e.type=Type::i64;}
        if (t.nullable) error(source,e.token,Code::type_mismatch,"Unwrap the nullable reference before field access.");
        if (t.kind != Type::aggregate && t.kind != Type::managed_ref && t.kind!=Type::interface_ref) error(source,e.token,Code::type_mismatch,"Field access requires a struct or interface.");
        if(t.kind==Type::interface_ref && !direct_method(t,e.token))error(source,e.token,Code::type_mismatch,"Field access requires a struct; interface views expose only declared methods.");
        if(t.definition->callable)error(source,e.token,Code::type_mismatch,"Function values are invoked directly; their representation is private.");
        if(!t.definition->cases.empty())error(source,e.token,Code::type_mismatch,"Enum payloads are accessed through patterns.");
        Member selected;
        if(!e.type_arguments.empty()){auto* method=concrete_method(e,t,true);if(!method)error(source,e.token,Code::unsupported_type,"This member is not a generic method.");selected={{},method};}
        else if(expected && expected->definition && expected->definition->callable && t.kind!=Type::interface_ref && ir::select_member(*root.member_catalog,t,e.token.text,unit.identity).status==ir::MemberSelection::missing){auto receiver=std::move(e.parts[0]);auto token=e.token;e=std::move(*receiver);return bind_method(e,token,*expected);}
        else selected=member(t,e.token);
        if(selected.function && selected.function->accessor==1) {
            for(auto index:selected.path)embedded_receiver(e.parts[0],index,e.token);
            e.kind=K::method_call;e.property=true;
            auto result=resolved_method_expression(e,e.parts[0]->type);
            const auto setter=selected.function->symbol.substr(0,selected.function->symbol.rfind("::method::"))+"::method::$set$"+e.token.text;
            for(const auto& [id,u]:units)if(u->methods.contains(setter))e.setter=setter;
            return result;
        }
        if(selected.function){
            const auto& target=*selected.function;std::vector<Type> parameters;for(const auto& parameter:target.parameters)parameters.push_back(parameter.type);
            if(target.asynchronous)error(source,e.token,Code::unsupported_type,"Async method values require a reviewed async callable contract.");auto signature=unit.make_callable(parameters,target.result);auto arguments=e.type_arguments;auto receiver=std::move(e.parts[0]);auto token=e.token;e=std::move(*receiver);return bind_method(e,token,signature,std::move(arguments));
        }
        for (unsigned n=0;n+1<selected.path.size();++n) embedded_receiver(e.parts[0],selected.path[n],e.token);
        auto owner=e.parts[0]->type;
        if(owner.definition && owner.definition->native_resource)error(source,e.token,Code::unsupported_type,"Native resource payload is binding-private.");
        e.slot=selected.path.back(); e.target=owner.definition->fields[e.slot].owner+"::field::"+e.token.text;
        e.event=owner.definition->fields[e.slot].event;e.event_owner=owner.definition->fields[e.slot].owner;if(e.event && !allow_event)error(source,e.token,Code::invalid_value,"Events support subscriptions and owner emission; subscriber storage cannot escape.");
        return e.type=ir::field_type(owner,e.slot);
    }
    VOID_SEMANTIC_HANDLER Type call_expression(Expr& e) {
        if(current && e.qualifier.empty() && !lookup(e.token.text) && current->substitutions.contains(e.token.text)) {
            if(!e.type_arguments.empty())error(source,e.token,Code::unsupported_type,"Type parameters are not generic constructors.");
            if(current->substitution_sources.contains(e.token.text))e.source_target=current->substitution_sources.at(e.token.text);
            const auto type=current->substitutions.at(e.token.text);
            if(type.definition && type.definition->task_result)error(source,e.token,Code::not_callable,"Inferred task handles have no source constructor; call an async function.");
            if(!type.definition || type.nullable || (type.kind!=Type::aggregate && type.kind!=Type::managed_ref))error(source,e.token,Code::not_callable,"The specialized type has no source constructor.");
            auto [owner,structure]=owned_structure(type);construction(e,*owner,*structure,true,unit);return e.type;
        }
        auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
        if (structure && !(e.qualifier.empty() && lookup(e.token.text))) {
            if(!structure->definition->cases.empty())error(source,e.token,Code::not_callable,"Construct enum values with type.variant and its payload arguments.");
            auto& concrete=concrete_structure(e,*owner,*structure);
            construction(e,*owner,concrete,current != nullptr,unit);
            return e.type;
        }
        if (std::any_of(e.names.begin(),e.names.end(),[](const auto& t) { return !t.text.empty(); })) error(source,e.token,Code::argument_count,"Named arguments are currently supported only for value construction.");
        if (!current) error(source, e.token, Code::constant_call, "A compile-time constant cannot call a function.", "Use a local const for runtime values.");
        if (auto* value = e.qualifier.empty() ? lookup(e.token.text) : nullptr) {
            if (value->type == error_type) throw Poison{};
            if(value->type.kind==Type::aggregate || value->type.kind==Type::managed_ref || value->type.kind==Type::interface_ref) {
                auto base=std::make_unique<Expr>(Expr::Kind::name,e.token);expression(*base);
                e.parts.insert(e.parts.begin(),std::move(base));e.kind=Expr::Kind::method_call;
                e.token.source_width=e.token.text.size();e.token.text="call";
                return resolved_method_expression(e,value->type);
            }
            error(source, e.token, Code::not_callable, "A local value is not callable.");
        }
        const auto& target = [&]() -> const Function& {
            auto check_arguments = [&] { for (auto& argument : e.parts) expression(*argument); };
            try { return concrete_function(e,resolve(e)); }
            catch (const CompilationError&) { check_arguments(); throw; }
            catch (const Poison&) { check_arguments(); throw; }
        }();
        e.target = target.symbol;
        return e.type = call_arguments(e,target,0);
    }
    VOID_SEMANTIC_HANDLER Type conversion_expression(Expr& e) {
        using K = Expr::Kind;
        Type target = e.type = resolve_type(e.type,unit,root,units,e.token);
        auto& operand = *e.parts[0];
        if (operand.kind == K::literal && !operand.literal_text.empty() && !anchor(operand)) {
            if (!numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
            if (!decimal(operand) || numeric::floating(target)) {
                try { operand.value = numeric::literal(operand.literal_text,target,true); }
                catch (const std::invalid_argument& failure) {
                    // A checked operation in an unevaluated constant branch must
                    // remain typed, without executing its range check eagerly.
                    if ((!current || suppress_zero) && numeric::integer(target)) {
                        for (auto candidate : {Type::i32,Type::i64,Type::u64}) {
                            try { operand.value = numeric::literal(operand.literal_text,candidate); operand.type = candidate; operand.literal_text.clear(); return target; }
                            catch (const std::invalid_argument&) {}
                        }
                    }
                    error(source,e.token,Code::numeric_conversion,failure.what());
                }
                operand.type = target; operand.literal_text.clear(); return target;
            }
        }
        auto from = expression(operand,operand.token.text=="null" && (target.kind==Type::interface_ref || (target.definition && target.definition->callable)) && target.nullable ? std::optional<Type>(target) : std::nullopt);
        if (from == error_type) return error_type;
        if(from.definition && target.definition && from.definition->callable && target.definition->callable && Type::key(from)==Type::key(target))return e.type=target;
        if(target.kind==Type::interface_ref && from.kind!=Type::interface_ref) {
            auto converted=interface_conversion(operand,from,target);e.op="identity";return e.type=converted;
        }
        if(from.kind==Type::interface_ref && target.kind==Type::interface_ref && Type::key(from)==Type::key(target)) {
            if(!from.nullable || target.nullable) e.op="upcast";
            target.readonly=from.readonly;return e.type=target;
        }
        if (from.kind==Type::managed_ref && target.kind==Type::managed_ref && Type::key(from)==Type::key(target) && !target.nullable) {
            target.readonly=from.readonly;return e.type=target;
        }
        if (ir::subtype(from,target) && (!from.nullable || target.nullable)) {
            e.op="upcast"; target.readonly=from.readonly; return e.type=target;
        }
        if (ir::reference_value(from) || ir::reference_value(target)) error(source,e.token,Code::numeric_conversion,"Checked reference conversion requires the same nominal type or an ancestor without nullability strengthening.","Unwrap nullable references before upcasting; source downcasts and unrelated reference casts are not supported.");
        if (!numeric::number(from) || !numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
        return target;
    }
    VOID_SEMANTIC_HANDLER Type unary_expression(Expr& e, std::optional<Type> expected) {
        auto left = expression(*e.parts[0], e.op == "not" ? std::optional<Type>(Type::boolean) : expected);
        if (left == error_type) return error_type;
        if (e.op == "not") expect(left,Type::boolean,e.token);
        else if (!numeric::number(left)) error(source,e.token,Code::numeric_operator,"Type mismatch: unary arithmetic requires a numeric operand.");
        return e.type = left;
    }
    VOID_SEMANTIC_HANDLER Type binary_expression(Expr& e, std::optional<Type> expected) {
        using K = Expr::Kind;
        bool logical = e.op == "and" || e.op == "or";
        auto left_hint = anchor(*e.parts[0]), right_hint = anchor(*e.parts[1]);
        auto context = logical ? std::optional<Type>(Type::boolean) : left_hint ? left_hint : right_hint ? right_hint :
            expected && numeric::number(*expected) ? expected : std::optional<Type>(decimal(e) ? Type::f32 : Type::i32);
        if (context && (ir::reference_value(*context) || (context->definition && context->definition->callable)) && (e.op=="==" || e.op=="!=")) context->nullable=true;
        auto left = expression(*e.parts[0],context);
        bool previous = suppress_zero;
        if (e.parts[0]->kind == K::literal && ((e.op == "and" && !e.parts[0]->value) || (e.op == "or" && e.parts[0]->value))) suppress_zero = true;
        auto right = expression(*e.parts[1],context); suppress_zero = previous;
        if (left == error_type || right == error_type) return error_type;
        if (logical) {
            expect(left,Type::boolean,e.parts[0]->token); expect(right,Type::boolean,e.parts[1]->token); return e.type = Type::boolean;
        }
        if (left==Type::string || right==Type::string) {
            expect(right,left,e.token);
            if (left!=Type::string || (e.op!="+" && e.op!="==" && e.op!="!=")) error(source,e.token,Code::numeric_operator,"Strings support only +, == and !=.");
            return e.type=e.op=="+" ? Type::string : Type::boolean;
        }
        if (e.op != "==" && e.op != "!=" && !numeric::number(left)) expect(left,Type::i32,e.parts[0]->token);
        if ((e.op=="==" || e.op=="!=") && left.kind==Type::managed_ref && right.kind==Type::managed_ref) {
            auto normalize=[&](std::unique_ptr<Expr>& operand,Type target) {
                auto original=std::move(operand); target.nullable=original->type.nullable; target.readonly=original->type.readonly;
                operand=std::make_unique<Expr>(K::conversion,original->token); operand->op="upcast"; operand->type=target; operand->parts.push_back(std::move(original));
            };
            if (Type::key(left)!=Type::key(right)) {
                if (ir::subtype(right,left)) { normalize(e.parts[1],left); right=e.parts[1]->type; }
                else if (ir::subtype(left,right)) { normalize(e.parts[0],right); left=e.parts[0]->type; }
                else error(source,e.token,Code::type_mismatch,"Reference equality requires related nominal types or explicit common-base views.");
            }
        } else if(!(left.definition && right.definition && left.definition->callable && right.definition->callable && Type::key(left)==Type::key(right)))expect(right,left,e.token);
        if (e.op == "==" || e.op == "!=") {
            if(left.definition && right.definition && left.definition->callable && right.definition->callable && Type::key(left)==Type::key(right))return e.type=Type::boolean;
            if (left.kind == Type::aggregate) error(source,e.token,Code::numeric_operator,"Value struct equality is not implemented.");
            if (left == Type::none) error(source,e.token,Code::invalid_value,"Cannot compare a no-result call.");
            return e.type = Type::boolean;
        }
        if (!numeric::number(left)) expect(left,Type::i32,e.parts[0]->token);
        if (!numeric::supports(operation(e.op),left)) error(source,e.token,Code::numeric_operator,"Illegal numeric operator % for " + std::string(ir::type_name(left)) + ".");
        if (current && !suppress_zero && numeric::integer(left) && (e.op == "/" || e.op == "%") && e.parts[1]->kind == K::literal && e.parts[1]->value == 0)
            error(source,e.parts[1]->token,Code::zero_divisor,"Division or remainder by zero.","Use a nonzero divisor.");
        return e.type = (e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") ? Type::boolean : left;
    }
    unsigned sequence(std::vector<Statement>& body, bool nested = true) {
        if (nested) scopes.emplace_back();
        unsigned flow = next_flow;
        for (auto& s : body) {
            if (diagnostics.full()) break;
            unsigned result = next_flow;
            auto old_zero = suppress_zero;
            try { result = statement(s); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            catch (const Poison&) {}
            suppress_zero = old_zero;
            // An invalid return remains a return for flow recovery, never valid IR.
            if (s.kind == Statement::Kind::returned) result = return_flow;
            if (flow & next_flow) flow = (flow & ~next_flow) | result;
        }
        if (nested) scopes.pop_back();
        return flow;
    }
    unsigned statement(Statement& s) {
        using K = Statement::Kind;
        unsigned flow = next_flow;
        switch (s.kind) {
        case K::no_op: break;
        case K::variable: {
            if (s.invalid) { s.slot = declare(s.token, error_type); invalid_function = true; break; }
            if (s.annotation) s.annotation = resolve_type(*s.annotation,unit,root,units,s.token);
            Type t = s.expression ? expression(*s.expression, s.annotation) : s.annotation.value_or(Type::none);
            if (t == Type::none) {
                s.slot = declare(s.token, error_type, s.readonly);
                error(source, s.token, Code::invalid_value, "Variable requires a value type.", "Supply a numeric/bool annotation or a value-producing initializer.");
            }
            if (s.annotation) {
                try { expect(t, *s.annotation, s.expression ? s.expression->token : s.token); if (t!=error_type) t=*s.annotation; }
                catch (const CompilationError& failure) { diagnostics.add(failure); t = error_type; }
            }
            if(!s.expression && t.definition && t.definition->callable) {
                if(!t.nullable)error(source,s.token,Code::initializer,"A non-nullable function value requires an initializer.");
                s.expression=std::make_unique<Expr>(Expr::Kind::function_null,s.token);s.expression->type=t;
            }
            if (!s.expression && ir::reference_value(t) && !t.nullable) error(source,s.token,Code::initializer,"A non-nullable reference requires an initializer.","Construct the object explicitly using its type.");
            if (!s.expression && ir::collection_type(t)) {
                if(needs_array_recipe(t))s.expression=array_recipe(t,s.token,true,unit);
                else {s.expression=std::make_unique<Expr>(Expr::Kind::literal,s.token);s.expression->type=t;s.expression->aggregate_value=array_default(t,s.token);}
            }
            if(!s.expression && t.definition && !t.definition->cases.empty())error(source,s.token,Code::initializer,"Enum/result values require an explicit variant initializer.");
            if (!s.expression && t.kind == Type::aggregate) {
                auto [owner,structure] = owned_structure(t);
                s.expression = std::make_unique<Expr>(Expr::Kind::construction,s.token);
                construction(*s.expression,*owner,*structure,true,unit);

            }
            if (!ir::reference_value(t) && t.kind!=Type::growable_array && t.kind!=Type::dictionary) t.readonly=false;
            s.slot = declare(s.token, t, s.readonly); break;
        }
        case K::scoped:{
            auto& call=*s.expression;
            if(call.kind==Expr::Kind::method_call && call.token.text=="group" && call.parts.size()==1 && call.parts[0]->kind==Expr::Kind::name && call.parts[0]->token.text=="tasks" && !lookup("tasks") && !visible_names.contains("tasks")){
                if(!current || !current->asynchronous || current->deferred || !call.type_arguments.empty())error(source,s.token,Code::unsupported_type,"tasks.group() requires an async scope.");
                call.kind=Expr::Kind::literal;call.type=Type::u64;call.parts.clear();s.task_group=true;scopes.emplace_back();s.slot=declare(s.binder,Type::u64,true);lookup(s.binder.text)->task_group=true;
                flow=sequence(s.body,false);scopes.pop_back();break;
            }
            auto type=expression(*s.expression);if(type.kind!=Type::managed_ref || type.nullable)error(source,s.token,Code::type_mismatch,"with requires a non-nullable managed resource with deinit.");
            scopes.emplace_back();s.slot=declare(s.binder,type,false);
            auto cell=unit.make_capture_record(current->symbol+"::$cell$"+std::to_string(s.slot),type,true);auto initial=type;initial.nullable=true;std::const_pointer_cast<ir::ValueType>(cell.definition)->fields[0].type=initial;current->captured_locals.emplace(s.slot,cell);
            Function lambda;lambda.anonymous=true;lambda.token=s.token;lambda.token.text="$with$"+std::to_string(current->lambdas.size());lambda.visibility="private";
            Statement invoke(Statement::Kind::expression,s.token);invoke.expression=std::make_unique<Expr>(Expr::Kind::method_call,Token{TokenKind::name,"deinit",s.token.offset});invoke.expression->parts.push_back(std::make_unique<Expr>(Expr::Kind::name,s.binder));lambda.body.push_back(std::move(invoke));
            s.selection=std::make_unique<Expr>(Expr::Kind::lambda,s.token);s.selection->slot=current->lambdas.size();current->lambdas.push_back(std::move(lambda));expression(*s.selection);current->cleanup_type=unit.make_cleanup();flow=sequence(s.body,false);scopes.pop_back();break;
        }
        case K::deferred:
            if(!current)error(source,s.token,Code::invalid_value,"defer requires a function activation.");
            if(s.expression->kind==Expr::Kind::lambda)expression(*s.expression);else deferred_call(s);
            current->cleanup_type=unit.make_cleanup();break;
        case K::assignment: {
            auto* variable = lookup(s.name);
            if ((variable && variable->readonly) || (!variable && visible_names.contains(s.name)))
                error(source, s.token, Code::readonly, "Cannot mutate readonly const or declaration '" + s.name + "'.", "Use var for a mutable local binding.");
            if (!variable) error(source, s.token, Code::unknown_identifier, "Unknown identifier '" + s.name + "'.", "Declare it with var before assignment.");
            auto t = variable->type; s.slot = variable->slot;
            if(!s.selection && !s.fields.empty()) {
                auto probe=t;
                for(const auto& field:s.fields) {
                    auto selected=member(probe,field);
                    if(selected.function) {
                        if(selected.function->accessor==1) {
                            auto target=std::make_unique<Expr>(Expr::Kind::name,s.token);
                            for(const auto& name:s.fields) {auto child=std::make_unique<Expr>(Expr::Kind::field,name);child->parts.push_back(std::move(target));target=std::move(child);}
                            if(s.index) {auto child=std::make_unique<Expr>(Expr::Kind::array_index,s.index->token);child->parts.push_back(std::move(target));child->parts.push_back(std::move(s.index));target=std::move(child);}
                            s.selection=std::move(target);s.fields.clear();
                        }
                        break;
                    }
                    for(auto index:selected.path)probe=ir::field_type(probe,index);
                }
            }
            if(s.selection) {
                auto previous=allow_event;allow_event=true;t=expression(*s.selection);allow_event=previous;
                std::function<void(const Expr&,bool)> writable=[&](const Expr& e,bool writeback){
                    if(e.property && writeback && e.setter.empty())error(source,e.token,Code::readonly,"Property has no setter.");
                    if(e.kind==Expr::Kind::field || e.kind==Expr::Kind::array_index || e.property) {
                        auto owner=e.parts[0]->type;
                        writable(*e.parts[0],writeback && (owner.kind==Type::aggregate || owner.kind==Type::fixed_array));
                    }
                };writable(*s.selection,true);
                if(s.selection->target=="$array.count" || s.selection->target=="$dict.count")error(source,s.selection->token,Code::readonly,"Array count is readonly.");
                s.event=s.selection->event;s.assigned_type=t;
                if(s.event){if(s.op!="+=" && s.op!="-=")error(source,s.token,Code::invalid_value,"Events support only += and -= subscription changes.");auto signature=ir::array_element(t);expect(expression(*s.expression,signature),signature,s.expression->token);break;}
                expect(expression(*s.expression,t),t,s.expression->token);
                if(s.op!="=" && !numeric::number(t) && !(t==Type::string && s.op=="+="))error(source,s.token,Code::type_mismatch,"Unsupported compound element assignment.");
                if(s.op=="%=" && numeric::floating(t))error(source,s.token,Code::numeric_operator,"Illegal compound floating remainder operator.");
                if(numeric::integer(t) && (s.op=="/=" || s.op=="%=") && s.expression->kind==Expr::Kind::literal && s.expression->value==0)error(source,s.expression->token,Code::zero_divisor,"Division or remainder by zero.");
                break;
            }
            std::vector<Token> expanded;
            for (const auto& field : s.fields) {
                auto selected=member(t,field);
                if (selected.function) error(source,field,Code::invalid_value,"Cannot assign to method '"+field.text+"'.");
                for (unsigned n=0;n<selected.path.size();++n) {
                    auto index=selected.path[n]; auto token=field;
                    token.text=t.definition->fields[index].name;
                    expanded.push_back(token); s.implicit_fields.push_back(n+1<selected.path.size());
                    s.event=t.definition->fields[index].event;s.field_indexes.push_back(index); t=ir::field_type(t,index);
                }
            }
            s.fields=std::move(expanded);
            if(s.index) {
                if(!ir::collection_type(t)) error(source,s.token,Code::unsupported_type,"Selected assignment requires an array or dictionary.");
                auto index=expression(*s.index,t.kind==Type::dictionary?std::optional(ir::dictionary_key(t)):std::nullopt);
                if(t.kind==Type::dictionary){expect(index,ir::dictionary_key(t),s.index->token);t=ir::dictionary_value(t);}
                else {if(index!=error_type && !numeric::integer(index))error(source,s.index->token,Code::type_mismatch,"Array index requires an integer.");t=ir::array_element(t);}
            }
            s.assigned_type = t;
            if(s.event){if((s.op!="+=" && s.op!="-=") || s.index)error(source,s.token,Code::invalid_value,"Events support only += and -= subscription changes.");auto signature=ir::array_element(t);expect(expression(*s.expression,signature),signature,s.expression->token);break;}
            expect(expression(*s.expression, t), t, s.expression->token);
            if (s.op != "=" && t != error_type && !numeric::number(t) && !(t==Type::string && s.op=="+=")) expect(t,Type::i32,s.token);
            if (s.op == "%=" && numeric::floating(t)) error(source,s.token,Code::numeric_operator,"Illegal compound floating remainder operator.");
            if (numeric::integer(t) && (s.op == "/=" || s.op == "%=") && s.expression->kind == Expr::Kind::literal && s.expression->value == 0)
                error(source, s.expression->token, Code::zero_divisor, "Division or remainder by zero.");
            break;
        }
        case K::expression:
            if (expression(*s.expression) == error_type) break;
            if (s.expression->kind != Expr::Kind::spawned && s.expression->kind != Expr::Kind::task_cancel && s.expression->kind != Expr::Kind::awaited && s.expression->kind != Expr::Kind::call && s.expression->kind != Expr::Kind::method_call && s.expression->kind!=Expr::Kind::function_call) error(source, s.token, Code::invalid_value, "Only function calls can be standalone expressions.", "Assign this value with var or return it.");
            break;
        case K::returned:
            if(current->deferred)error(source,s.token,Code::invalid_value,"Deferred code cannot return directly.");
            if (s.expression) {
                auto result = expression(*s.expression, current->result);
                if (result == error_type) { flow = return_flow; break; }
                expect(result, current->result, s.token);
            } else expect(Type::none, current->result, s.token);
            // A no-result call is a statement, not a return value.
            if (s.expression && current->result == Type::none) error(source, s.token, Code::invalid_value, "A no-result function cannot return an expression.", "Call the function, then use an empty return.");
            flow = return_flow; break;
        case K::conditional: {
            scopes.emplace_back();
            try { condition(*s.expression); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            pattern_bindings=false;
            auto yes=sequence(s.body);scopes.pop_back();
            flow=yes | (s.otherwise.empty()?next_flow:sequence(s.otherwise));break;
        }
        case K::match: {
            auto subject=expression(*s.expression);flow=0;
            for(auto& arm:s.body){scopes.emplace_back();check_pattern(*arm.pattern,subject);flow|=sequence(arm.body);scopes.pop_back();}
            if(!warn_match(s.token,subject,[&](auto visit){for(const auto& arm:s.body)visit(*arm.pattern);}))flow|=next_flow;break;
        }
        case K::loop: {
            if(!s.binder.text.empty()) {
                Type item;
                if(s.range_end) {
                    if(!s.index_binder.text.empty())error(source,s.index_binder,Code::unsupported_iteration,"Ranges have one item binding.");
                    auto hint=anchor(*s.expression);if(!hint)hint=anchor(*s.range_end);
                    auto first=expression(*s.expression,hint);auto last=expression(*s.range_end,first);expect(last,first,s.range_end->token);
                    if(first!=Type::i32 && first!=Type::i64)error(source,s.token,Code::type_mismatch,"Range endpoints require matching i32 or i64 values.");
                    item=first;
                } else {
                    auto array=expression(*s.expression);
                    if(!ir::collection_type(array))error(source,s.token,Code::unsupported_iteration,"Iteration requires an array, dictionary or integer range.");
                    item=array.kind==Type::dictionary?ir::dictionary_value(array):ir::array_element(array);
                }
                scopes.emplace_back();s.slot=declare(s.binder,item);
                if(!s.index_binder.text.empty())s.index_slot=declare(s.index_binder,s.expression->type.kind==Type::dictionary?ir::dictionary_key(s.expression->type):Type(Type::i64));
            } else if (s.expression) {
                scopes.emplace_back();
                try { condition(*s.expression); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            ++loops; auto body = sequence(s.body); --loops;
            if(!s.binder.text.empty() || s.expression)scopes.pop_back();
            flow = body & return_flow;
            if (!s.binder.text.empty() || !always(s.expression.get()) || (body & break_flow)) flow |= next_flow;
            break;
        }
        case K::broken: case K::continued:
            if (!loops) error(source, s.token, Code::loop_control, s.token.text + " is only valid inside a for loop.");
            flow = s.kind == K::broken ? break_flow : continue_flow; break;
        }
        return s.flow = flow;
    }
public:
    Type pure_expression(Expr& e) { return expression(e); }
    Semantics(Unit& u, const std::map<std::string, std::unique_ptr<Unit>>& graph, Unit& r,
        std::function<void(Unit&, Constant&)> eval, Diagnostics& d, std::function<void(Unit&,Structure&)> defs)
        : source(u.source), unit(u), units(graph), root(r), evaluate(std::move(eval)), defaults(std::move(defs)), diagnostics(d) {
        for (const auto& [id,owner] : units) for (const auto& [name,structure] : owner->structure_names)
            if (owner.get() == &unit || (structure->visibility != "private" && (owner.get() == &root || std::any_of(unit.imports.begin(),unit.imports.end(),[&](const auto& i) { return !i.explicit_alias && i.target == id; })))) visible_names.insert(name);
        for (const auto& [name, c] : root.constant_names) if (!c->file_private || &root == &unit) visible_names.insert(name);
        for (const auto& [name, c] : unit.constant_names) { (void)c; visible_names.insert(name); }
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty())
            for (const auto& [name, c] : units.at(import.target)->constant_names) if (!c->file_private) visible_names.insert(name);
        for (const auto& [name, f] : unit.declarations) { (void)f; visible_names.insert(name); }
        for (const auto& [name, target] : unit.qualifiers) { (void)target; visible_names.insert(name); }
        for (const auto& import : unit.imports) if (!import.explicit_alias && !import.target.empty())
            for (const auto& [name, f] : units.at(import.target)->declarations)
                if (!f->file_private) visible_names.insert(name);
    }
    void validate_generic_receiver(Type receiver,const Token& token) {
        auto selected=ir::select_member(*root.member_catalog,receiver,token.text,unit.identity);
        if(selected.path.empty() && !selected.method.empty())
            error(source,token,Code::receiver,"Inherited methods cannot be hidden by a method-specific generic declaration.");
    }
    void validate_header_names(Function& f) {
        current=&f;scopes={{}};
        if(f.receiver)declare(f.receiver->token,f.receiver->type);
        for(const auto& p:f.parameters)declare(p.token,p.type,p.type.readonly);
        f.locals.clear();current=nullptr;scopes.clear();
    }
    void analyze_constant(Constant& c) {
        if (c.declaration.annotation) c.declaration.annotation = resolve_type(*c.declaration.annotation,unit,root,units,c.declaration.token);
        c.type = expression(*c.declaration.expression, c.declaration.annotation);
        if (c.type == error_type) throw Poison{};
        if (c.declaration.annotation) expect(c.type, *c.declaration.annotation, c.declaration.token);
    }
    void validate_parameter_structure(const Function& f) {
        bool optional=false;
        for(const auto& p:f.parameters) {
            if(p.default_expression) {
                if(!f.receiver || f.token.text!="init")error(source,p.token,Code::initializer_deferred,"Default parameters are supported on initializers only in this phase.");
                optional=true;
            } else if(optional)error(source,p.token,Code::argument_count,"Required parameters must precede defaulted parameters.");
        }
    }
    void parameter_defaults(Function& f) {
        if(f.defaults_prepared)return;f.defaults_prepared=true;
        validate_parameter_structure(f);
        for(auto& p:f.parameters)if(p.default_expression) {
            expect(expression(*p.default_expression,p.type),p.type,p.token);
            if(p.default_expression->type==error_type)throw Poison{};
            p.default_value=constant_value(source,*p.default_expression);
        }
    }
    void initializer_fields(Function& f) {
        if (!f.initializer || f.initializer_prepared) return;f.initializer_prepared=true;
        auto [owner,structure] = owned_structure(f.receiver->type);
        if (f.receiver->type.kind==Type::aggregate) for (const auto& field:structure->fields) {
            auto type=*field.annotation;
            const bool complete_default=field.expression && type.definition && (type.definition->callable || !type.definition->cases.empty());
            if (ir::requires_initialization(type) && !complete_default)
                error(source,f.token,Code::initializer,"Value init requires every field to be valid before its body; a non-nullable reference has no safe default.","Use named-field construction without init for a value with required reference fields.",{{span(owner->source,field.token),"Required reference field declared here."}});
        }
        Expr plan(Expr::Kind::construction,f.token); plan.initializer=f.symbol;
        construction_fields(plan,*owner,*structure,true,true);
        f.initialization_defaults=std::move(plan.omitted); f.initialization_value=structure->defaults;
    }
    bool needs_array_recipe(Type t) {
        if(t.kind==Type::growable_array || t.kind==Type::dictionary)return false;
        if(t.kind==Type::fixed_array)return *t.definition->extent && needs_array_recipe(ir::array_element(t));
        if(t.kind!=Type::aggregate || t.definition->callable)return false;
        if(t.definition->result_type)return false;
        auto [owner,structure]=owned_structure(t);if(structure->initializer)return true;
        for(const auto& field:t.definition->fields)if(needs_array_recipe(field.type))return true;
        return false;
    }
    std::unique_ptr<Expr> array_recipe(Type t,const Token& token,bool runtime,Unit& access) {
        auto e=std::make_unique<Expr>(Expr::Kind::literal,token);e->type=t;
        if(t.kind==Type::growable_array || t.kind==Type::dictionary)return e;
        if(t.kind==Type::fixed_array) {
            if(!*t.definition->extent){e->aggregate_value.repeat=0;return e;}
            e->kind=Expr::Kind::array_default;e->parts.push_back(array_recipe(ir::array_element(t),token,runtime,access));return e;
        }
        if(t.definition && t.definition->callable){if(!t.nullable)error(source,token,Code::initializer,"An array of functions requires complete values.");e->kind=Expr::Kind::function_null;return e;}
        if(t.definition && !t.definition->cases.empty())error(source,token,Code::initializer,"Enum/result elements require explicit variants.");
        if(t.kind==Type::aggregate){auto [owner,structure]=owned_structure(t);construction(*e,*owner,*structure,runtime,access);}
        else if(ir::reference_value(t) && !t.nullable)error(source,token,Code::initializer,"A required array reference needs an explicit initializer.");
        return e;
    }
    ir::ConstantValue array_default(Type t,const Token& token) {
        ir::ConstantValue value;if(t.kind==Type::growable_array || t.kind==Type::dictionary)return value;value.repeat=*t.definition->extent;
        if(!*value.repeat) return value;
        auto element=ir::array_element(t);
        if(ir::reference_value(element) && !element.nullable)error(source,token,Code::initializer,"A nonempty array of required references needs a complete literal.");
        if(ir::collection_type(element)) value.fields.push_back(array_default(element,token));
        else if(element.kind==Type::aggregate) {
            if(element.definition->callable){if(!element.nullable)error(source,token,Code::initializer,"A nonempty array of functions requires complete values.");value.fields.emplace_back();return value;}
            if(!element.definition->cases.empty())error(source,token,Code::initializer,"Enum/result elements require explicit variants.");
            auto [owner,structure]=owned_structure(element);defaults(*owner,*structure);
            value.fields.push_back(structure->defaults);
        } else value.fields.emplace_back();
        return value;
    }
    ir::ConstantValue field_default(Statement& field) {
        auto t = *field.annotation;
        if (field.expression) {
            expect(expression(*field.expression,t),t,field.token);
            if (field.expression->type == error_type) throw Poison{};
            return constant_value(source,*field.expression);
        }
        if(ir::collection_type(t)) {
            return array_default(t,field.token);
        }
        if (t.kind != Type::aggregate) return {};
        if(t.definition->callable)return {};
        if(!t.definition->cases.empty())return {};
        for (const auto& [id,owner] : units) for (auto& nested : owner->structures) if (nested.definition == t.definition) {
            defaults(*owner,nested); return nested.defaults;
        }
        throw std::logic_error("Missing owned value type");
    }
    void analyze_one(Function& f) {
            if(f.interface_requirement) {
                f.analyzed=true;f.locals={f.receiver->type};f.receiver->slot=0;
                for(auto& p:f.parameters) {p.slot=static_cast<unsigned>(f.locals.size());f.locals.push_back(p.type);}
                return;
            }
            if (diagnostics.full()) return;
            if(f.analyzed)return;f.analyzed=true;
            Diagnostics::Context generic_context(diagnostics,f.instantiation_context,f.specialization_depth);
            current = &f; scopes = {{}}; invalid_function = false;
            auto before = diagnostics.size();
            if (f.receiver) {
                try { f.receiver->slot = declare(f.receiver->token,f.receiver->type); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            for (auto& p : f.parameters) {
                try {if(f.anonymous && &p==&f.parameters.front()){p.slot=static_cast<unsigned>(f.locals.size());f.locals.push_back(p.type);}else p.slot = declare(p.token, p.type, p.type.readonly); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            if (f.base_prelude) {
                try {
                    auto [base_owner,base]=owned_structure(*owned_structure(f.receiver->type).second->base_type);
                    construction(*f.base_prelude,*base_owner,*base,true,unit);
                    f.base_prelude->kind=Expr::Kind::base_initialization;
                } catch (const CompilationError& failure) { diagnostics.add(failure); }
                  catch (const Poison&) {}
            }
            if(!f.native_symbol.empty())return;
            auto flow = sequence(f.body, false);
            if (f.result != Type::none && (flow & next_flow) && diagnostics.size() == before && !f.parse_failed && !invalid_function)
                try { error(source, f.token, Code::missing_return, "Missing return path in '" + f.token.text + "'.", "Return a " + std::string(ir::type_name(f.result)) + " on every path that reaches the end."); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
    }
    void analyze(){for(auto& f:unit.functions)analyze_one(f);}

};
struct StringPool {
    std::vector<std::string> values;
    std::map<std::string,unsigned,std::less<>> indexes;
    std::size_t bytes=0;
    unsigned intern(std::string_view text,const SourceFile& source,const Token& token) {
        auto found=indexes.find(text); if(found!=indexes.end()) return found->second;
        if(text.size()>ir::string_constant_limit || text.size()>ir::string_pool_limit-bytes)
            error(source,token,Code::limit,"String constants exceed the 16 MiB value or 64 MiB module-pool limit.");
        auto id=static_cast<unsigned>(values.size());
        indexes.emplace(text,id); values.emplace_back(text); bytes+=text.size(); return id;
    }
};
// All owned templates, including unused declaration metadata, belong to the
// compiled graph's pool. Checking and lowering enforce the same finite budget.
StringPool string_pool(const std::map<std::string,std::unique_ptr<Unit>>& units) {
    StringPool pool;
    for(const auto& [id,u]:units) {
        std::vector<const Expr*> expressions;
        auto value=[&](const ir::ConstantValue& root,const Token& token) {
            std::vector<const ir::ConstantValue*> pending{&root};
            while(!pending.empty()) {
                auto* current=pending.back();pending.pop_back();
                if(!current->text.empty()) pool.intern(current->text,u->source,token);
                for(const auto& child:current->fields) pending.push_back(&child);
            }
        };
        auto patterns=[&](const Pattern& root){std::vector<const Pattern*> pending{&root};while(!pending.empty()){auto* p=pending.back();pending.pop_back();for(const auto& value:p->values)expressions.push_back(value.get());for(const auto& child:p->children)pending.push_back(&child);}};
        auto statements=[&](const std::vector<Statement>& body) {
            std::vector<const Statement*> pending; for(const auto& s:body) pending.push_back(&s);
            while(!pending.empty()) {
                auto* s=pending.back();pending.pop_back();if(s->pattern)patterns(*s->pattern); if(s->expression) expressions.push_back(s->expression.get());
                for(const auto& child:s->body) pending.push_back(&child);
                for(const auto& child:s->otherwise) pending.push_back(&child);
            }
        };
        for(const auto& c:u->constants) {value(c.aggregate_value,c.declaration.token);if(c.declaration.expression) expressions.push_back(c.declaration.expression.get());}
        for(const auto& structure:u->structures) {value(structure.defaults,structure.token);statements(structure.fields);}
        for(const auto& f:u->functions) {
            statements(f.body); value(f.initialization_value,f.token);
            if(f.base_prelude) expressions.push_back(f.base_prelude.get());
            for(const auto& e:f.initialization_defaults) expressions.push_back(e.get());
            for(const auto& p:f.parameters) {if(p.default_value) value(*p.default_value,p.token);if(p.default_expression) expressions.push_back(p.default_expression.get());}
        }
        while(!expressions.empty()) {
            auto* e=expressions.back();expressions.pop_back();value(e->aggregate_value,e->token);for(const auto& p:e->patterns)patterns(p);
            for(const auto& child:e->parts) expressions.push_back(child.get());
            for(const auto& child:e->omitted) expressions.push_back(child.get());
        }
    }
    return pool;
}
class Lowering {
    const SourceFile& source;
    ir::Module& module;
    StringPool& strings;
    ir::Function output;
    std::optional<unsigned> current;
    const ConstructionContext* context=nullptr;
    std::map<unsigned,unsigned> cells;
    std::optional<unsigned> cleanup_head,return_slot;
    struct WithScope {unsigned slot;bool group=false;std::optional<unsigned> owned_group={};};
    std::vector<WithScope> with_scopes;
    std::vector<std::size_t> loop_with_depth;
    unsigned register_cleanup(unsigned handler,bool active,const Token& token){
        ir::Instruction node{ir::Op::gc_construct};auto type=output.locals[*cleanup_head];type.nullable=false;node.type=type;node.arguments={emit({ir::Op::load,*cleanup_head},token),handler,constant(Type::boolean,{static_cast<unsigned>(active),{}},token)};auto value=emit(node,token);emit({ir::Op::store,*cleanup_head,value},token);return capture(value,type,token);
    }
    void join_group(unsigned slot,bool cancel,const Token& token){ir::Instruction join{ir::Op::async_group_join,emit({ir::Op::load,slot},token)};join.immediate=cancel;emit(join,token);}
    void leave_with(WithScope scope,const Token& token,bool cancel=true){
        if(scope.group){join_group(scope.slot,cancel,token);return;}
        if(scope.owned_group)join_group(*scope.owned_group,cancel,token);
        auto slot=scope.slot;
        auto owner=emit({ir::Op::load,slot},token);ir::Instruction deactivate{ir::Op::gc_write,owner,constant(Type::boolean,{0,{}},token)};deactivate.immediate=2;emit(deactivate,token);
        auto handler=extract_managed(owner,1,token);emit({ir::Op::function_call,handler},token);
    }
    const Statement* normal_return=nullptr;
    std::vector<unsigned> cleanup_jumps;
    void return_through_cleanup(std::optional<unsigned> value,const Token& token,bool early=false){
        if(output.asynchronous && early)emit({ir::Op::async_cancel_children},token);
        if(output.asynchronous)for(auto scope=with_scopes.rbegin();scope!=with_scopes.rend();++scope){if(scope->group)join_group(scope->slot,true,token);else if(scope->owned_group)join_group(*scope->owned_group,true,token);}
        if(!cleanup_head && !output.asynchronous){terminate(value?ir::Terminator{ir::Terminator::Kind::return_value,*value}:ir::Terminator{ir::Terminator::Kind::return_void});return;}
        if(value)emit({ir::Op::store,*return_slot,*value},token);
        cleanup_jumps.push_back(*current);terminate({ir::Terminator::Kind::jump});
    }
    std::vector<std::pair<unsigned, unsigned>> loops; // continue, break destinations
    unsigned block() {
        auto id = static_cast<unsigned>(output.blocks.size());
        output.blocks.push_back({{}, {ir::Terminator::Kind::unreachable}}); return id;
    }
    void terminate(ir::Terminator end) { output.blocks[*current].end = end; current.reset(); }
    void jump(unsigned to) { terminate({ir::Terminator::Kind::jump, 0, to}); }
    unsigned emit(ir::Instruction i, const Token& token) {
        if(i.op==ir::Op::load && cells.contains(i.a)) {
            auto owner=emit({ir::Op::load,cells.at(i.a)},token);
            auto value=emit({ir::Op::gc_read,owner},token);auto stored=ir::field_type(output.locals.at(cells.at(i.a)),0);
            if(stored.nullable && !output.locals.at(i.a).nullable){ir::Instruction unwrap{ir::Op::convert,value};unwrap.type=output.locals.at(i.a);value=emit(unwrap,token);}return value;
        }
        if(i.op==ir::Op::store && cells.contains(i.a)) {
            auto home=cells.at(i.a);
            if(i.initialization) {
                ir::Instruction construct{ir::Op::gc_construct};construct.type=output.locals.at(home);construct.arguments={i.b};
                auto owner=emit(construct,token);return emit({ir::Op::store,home,owner},token);
            }
            return emit({ir::Op::gc_write,emit({ir::Op::load,home},token),i.b},token);
        }
        if(i.op==ir::Op::store)i.initialization=false;
        if(i.receiver && cells.contains(i.receiver->slot)) {
            i.receiver->slot=cells.at(i.receiver->slot);i.receiver->fields.insert(i.receiver->fields.begin(),0);
        }
        if(i.op==ir::Op::dict_new)module.coordinated=true;
        i.source = source.location(token.offset);
        if (i.op==ir::Op::function_call || i.op==ir::Op::panic || i.op==ir::Op::pattern_equal || i.op==ir::Op::type_cast || i.op==ir::Op::sum_get || i.op==ir::Op::dict_get || i.op==ir::Op::dict_set || i.op==ir::Op::dict_contains || i.op==ir::Op::dict_remove || i.op==ir::Op::dict_clear || i.op==ir::Op::dict_entry || i.op==ir::Op::dict_check || i.op==ir::Op::dict_new || i.op==ir::Op::array_slice || i.op==ir::Op::list_insert || i.op==ir::Op::list_clear || i.op==ir::Op::list_push || i.op==ir::Op::list_remove || i.op==ir::Op::list_index || i.op==ir::Op::list_commit || i.op==ir::Op::array_get || i.op==ir::Op::array_set || i.op==ir::Op::string_literal || i.op==ir::Op::string_concat || i.op==ir::Op::string_equal || i.op == ir::Op::call || i.op==ir::Op::interface_pack || i.op==ir::Op::interface_prepare || i.op==ir::Op::interface_call || i.op == ir::Op::divide || i.op == ir::Op::remainder || i.op == ir::Op::convert || i.op == ir::Op::gc_construct || i.op == ir::Op::gc_read || i.op == ir::Op::gc_write)
            i.source_excerpt = source.excerpt(token.offset);
        auto& instructions = output.blocks[*current].instructions;
        auto id = static_cast<unsigned>(instructions.size()); instructions.push_back(std::move(i)); return id;
    }
    unsigned extract_managed(unsigned owner,unsigned field,const Token& token){ir::Instruction read{ir::Op::gc_read,owner};read.immediate=field;return emit(read,token);}
    unsigned repeated_array(Type t,std::uint64_t count,const Token& token,const std::function<unsigned()>& element) {
        ir::Instruction allocation{ir::Op::array_allocate};allocation.type=t;
        auto owner=capture(emit(allocation,token),t,token);
        auto index=snapshot(constant(Type::u64,{0,{}},token),Type::u64,token);
        auto test=block(),body=block(),done=block();jump(test);current=test;
        terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,emit({ir::Op::load,index},token),constant(Type::u64,{count,{}},token)},token),body,done});current=body;
        auto value=element();ir::Instruction initialize{ir::Op::array_initialize,emit({ir::Op::load,owner},token),emit({ir::Op::load,index},token)};initialize.arguments={value};emit(initialize,token);
        emit({ir::Op::store,index,emit({ir::Op::add,emit({ir::Op::load,index},token),constant(Type::u64,{1,{}},token)},token)},token);jump(test);current=done;return emit({ir::Op::load,owner},token);
    }
    unsigned constant(Type t, const ir::ConstantValue& value, const Token& token) {
        if(t.definition && t.definition->callable){ir::Instruction instruction{value.text.empty()?ir::Op::function_null:ir::Op::function_reference};instruction.type=t;instruction.callee=value.text;return emit(instruction,token);}
        if(t.definition && !t.definition->cases.empty()) {
            ir::Instruction sum{ir::Op::sum};sum.type=t;sum.immediate=value.fields.at(0).bits;
            const auto& variant=t.definition->cases.at(sum.immediate);
            for(unsigned n=0;n<variant.count;++n)sum.arguments.push_back(constant(ir::field_type(t,variant.begin+n),value.fields.at(variant.begin+n),token));
            return emit(sum,token);
        }
        ir::Instruction i{t.kind==Type::dictionary ? ir::Op::dict_new : ir::array_type(t) ? ir::Op::fixed_array : t.kind == Type::aggregate ? ir::Op::aggregate : t==Type::string ? ir::Op::string_literal : ir::Op::constant}; i.type = t;
        if (t==Type::string) {
            i.immediate=strings.intern(value.text,source,token);
            module.coordinated=true; return emit(i,token);
        }
        if(t.kind==Type::dictionary){for(unsigned n=0;n<value.fields.size();n+=2){auto key=constant(ir::dictionary_key(t),value.fields[n],token);auto child=constant(ir::dictionary_value(t),value.fields[n+1],token);i.arguments.push_back(dictionary_pair(key,child,t,token));}return emit(i,token);}
        if(ir::array_type(t)) {
            i.array_repeat=value.repeat.has_value();
            if(value.repeat && *value.repeat!=*t.definition->extent) throw std::logic_error("Mismatched repeated default extent");
            for(const auto& element:value.fields) i.arguments.push_back(constant(ir::array_element(t),element,token));
        }
        else if (t.kind == Type::aggregate) for (unsigned n = 0; n < t.definition->fields.size(); ++n)
            i.arguments.push_back(constant(ir::field_type(t,n),value.fields.at(n),token));
        else i.immediate = std::bit_cast<std::int64_t>(value.bits);
        return emit(i,token);
    }
    unsigned extract(unsigned value, unsigned field, const Token& token) {
        ir::Instruction i{ir::Op::extract,value}; i.immediate = field; return emit(i,token);
    }
    unsigned insert(unsigned aggregate, unsigned value, unsigned field, const Token& token) {
        ir::Instruction i{ir::Op::insert,aggregate,value}; i.immediate = field; return emit(i,token);
    }
    unsigned replace_path(unsigned aggregate, unsigned value, const std::vector<unsigned>& path, unsigned depth, const Token& token) {
        if (depth == path.size()) return value;
        auto child = extract(aggregate,path[depth],token);
        return insert(aggregate,replace_path(child,value,path,depth+1,token),path[depth],token);
    }
    Type expression_type(const Expr& e) const {
        if (e.kind==Expr::Kind::name) return output.locals.at(e.slot);
        auto t=e.type;
        if (e.kind==Expr::Kind::conversion && e.op=="upcast" && expression_type(*e.parts[0]).construction) t.construction=true;
        return t;
    }
    unsigned view_origin(unsigned value) const {
        const auto& instruction=output.blocks[*current].instructions.at(value);
        if (instruction.op==ir::Op::stage_prefix) return view_origin(instruction.a);
        if (instruction.op!=ir::Op::load) throw std::logic_error("construction snapshot requires a selected view");
        return output.construction_origins.at(instruction.a);
    }
    unsigned snapshot(unsigned value, Type t, const Token& token) {
        auto slot=static_cast<unsigned>(output.locals.size()); output.locals.push_back(t);
        if (t.construction) {
            output.construction_origins[slot]=view_origin(value);
        }
        emit({ir::Op::store,slot,value},token); return slot;
    }
    unsigned read_place(const ir::Place& place, const Token& token) {
        auto t=output.locals.at(place.slot); auto value=emit({ir::Op::load,place.slot},token);
        for (auto field:place.fields) {
            ir::Instruction i{t.kind==Type::managed_ref ? ir::Op::gc_read : ir::Op::extract,value}; i.immediate=field;
            value=emit(i,token); t=ir::field_type(t,field);
        }
        return value;
    }
    ir::Place select_place(ir::Place place, const std::vector<unsigned>& path, const Token& token) {
        auto t=output.locals.at(place.slot);
        for (auto field:path) {
            if (t.kind==Type::managed_ref) {
                auto slot=snapshot(read_place(place,token),t,token); place={slot,{}};
            }
            place.fields.push_back(field); t=ir::field_type(t,field);
        }
        return place;
    }
    ir::Place place(const Expr& e) {
        if (e.kind==Expr::Kind::name) return {e.slot,{}};
        if (e.kind==Expr::Kind::field) {
            if (e.parts[0]->type.kind==Type::managed_ref) {
                auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token);
                return {slot,{e.slot}};
            }
            auto parent=place(*e.parts[0]); parent.fields.push_back(e.slot); return parent;
        }
        auto slot=snapshot(expression(e),expression_type(e),e.token); return {slot,{}};
    }
    void write_place(const ir::Place& place, unsigned value, const Token& token) {
        auto t=output.locals.at(place.slot);
        if (place.fields.empty()) { emit({ir::Op::store,place.slot,value},token); return; }
        auto root=emit({ir::Op::load,place.slot},token);
        if (t.kind==Type::managed_ref) {
            auto field=place.fields.front();
            if (place.fields.size()>1) {
                ir::Instruction read{ir::Op::gc_read,root}; read.immediate=field;
                auto aggregate=emit(read,token);
                value=replace_path(aggregate,value,place.fields,1,token);
            }
            ir::Instruction write{ir::Op::gc_write,root,value}; write.immediate=field; emit(write,token);
        } else {
            value=replace_path(root,value,place.fields,0,token); emit({ir::Op::store,place.slot,value},token);
        }
    }
    struct Selection {Type type;std::function<unsigned()> read;std::function<void(unsigned)> write;bool dependent=false;bool read_before_write=true;};
    unsigned capture(unsigned value,Type type,const Token& token) {
        if(!ir::array_type(type))return snapshot(value,type,token);
        auto slot=static_cast<unsigned>(output.locals.size());output.locals.push_back(type);ir::Instruction i{ir::Op::array_capture,value};i.immediate=slot;emit(i,token);return slot;
    }
    unsigned dictionary_pair(unsigned key,unsigned value,Type type,const Token& token) {ir::Instruction pair{ir::Op::aggregate};pair.type=ir::array_element(type);pair.arguments={key,value,constant(Type::u64,{0,{}},token)};return emit(pair,token);}
    void dictionary_set(unsigned owner,unsigned key,unsigned value,unsigned version,Type type,const Token& token) {
        ir::Instruction write{ir::Op::dict_set,owner,dictionary_pair(key,value,type,token)};write.arguments={version};emit(write,token);
    }
    Selection select_expression(const Expr& e) {
        auto token=e.token;
        if(e.kind==Expr::Kind::name) {
            auto slot=e.slot;return {e.type,[=,this]{return emit({ir::Op::load,slot},token);},[=,this](unsigned value){emit({ir::Op::store,slot,value},token);}};
        }
        if(e.property) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));
            const bool managed=parent->type.kind==Type::managed_ref;
            // A computed value receiver is evaluated before the RHS. A normal
            // local value receiver keeps its selected live storage instead.
            if(e.selected_receiver && !managed)parent->read();
            std::optional<unsigned> owner;if(managed)owner=snapshot(parent->read(),parent->type,token);
            auto value_slot=std::make_shared<std::optional<unsigned>>();
            const bool setter_mutates=e.setter_mutates;
            auto invoke=[=,this](const std::string& target,std::optional<unsigned> value) {
                auto slot=owner.value_or(~0u);if(!owner)slot=snapshot(parent->read(),parent->type,token);
                ir::Instruction call{ir::Op::call};call.callee=target;call.receiver=ir::Place{slot,{}};if(value)call.arguments={*value};
                auto result=emit(call,token);
                if(value && !managed && setter_mutates)parent->write(emit({ir::Op::load,slot},token));
                return result;
            };
            auto type=e.type;auto getter=e.target,setter=e.setter;
            return {type,[=,this]{if(!*value_slot)*value_slot=snapshot(invoke(getter,{}),type,token);return emit({ir::Op::load,**value_slot},token);},[=](unsigned value){invoke(setter,value);},parent->dependent,false};
        }
        if(e.kind==Expr::Kind::field) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));auto field=e.slot;
            if(parent->type.kind==Type::managed_ref) {
                auto owner=snapshot(parent->read(),parent->type,token);
                return {e.type,[=,this]{ir::Instruction i{ir::Op::gc_read,emit({ir::Op::load,owner},token)};i.immediate=field;return emit(i,token);},[=,this](unsigned value){ir::Instruction i{ir::Op::gc_write,emit({ir::Op::load,owner},token),value};i.immediate=field;emit(i,token);}};
            }
            return {e.type,[=,this]{ir::Instruction i{ir::Op::extract,parent->read()};i.immediate=field;return emit(i,token);},[=,this](unsigned value){ir::Instruction i{ir::Op::insert,parent->read(),value};i.immediate=field;parent->write(emit(i,token));},parent->dependent};
        }
        if(e.kind==Expr::Kind::array_index) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));
            if(parent->type.kind==Type::dictionary) {
                auto owner=snapshot(parent->read(),parent->type,token);auto key=snapshot(expression(*e.parts[1]),e.parts[1]->type,token);auto version=snapshot(emit({ir::Op::dict_version,emit({ir::Op::load,owner},token)},token),Type::u64,token);
                return {e.type,[=,this]{auto object=emit({ir::Op::load,owner},token);emit({ir::Op::dict_check,object,emit({ir::Op::load,version},token)},token);return emit({ir::Op::dict_get,object,emit({ir::Op::load,key},token)},token);},[=,this](unsigned value){dictionary_set(emit({ir::Op::load,owner},token),emit({ir::Op::load,key},token),value,emit({ir::Op::load,version},token),parent->type,token);},true,false};
            }
            // Reading a nested list reference severs its former inline ancestry.
            std::optional<unsigned> selected_owner;
            if(parent->type.kind==Type::growable_array && parent->dependent)selected_owner=capture(parent->read(),parent->type,token);
            auto index=snapshot(expression(*e.parts[1]),e.parts[1]->type,token);
            if(parent->type.kind==Type::growable_array) {
                auto owner=selected_owner.value_or(~0u);if(!selected_owner)owner=capture(parent->read(),parent->type,token);
                auto normalized=snapshot(emit({ir::Op::list_index,emit({ir::Op::load,owner},token),emit({ir::Op::load,index},token)},token),Type::u64,token);
                auto version=snapshot(emit({ir::Op::list_version,emit({ir::Op::load,owner},token)},token),Type::u64,token);
                return {e.type,[=,this]{auto object=emit({ir::Op::load,owner},token);emit({ir::Op::list_check,object,emit({ir::Op::load,version},token)},token);return emit({ir::Op::array_get,object,emit({ir::Op::load,normalized},token)},token);},[=,this](unsigned value){ir::Instruction i{ir::Op::list_commit,emit({ir::Op::load,owner},token),emit({ir::Op::load,normalized},token)};i.arguments={value,emit({ir::Op::load,version},token)};emit(i,token);},true};
            }
            // Bounds/index effects precede the RHS, while later reads reacquire
            // the current fixed value from the original selected owning place.
            emit({ir::Op::array_get,parent->read(),emit({ir::Op::load,index},token)},token);
            return {e.type,[=,this]{return emit({ir::Op::array_get,parent->read(),emit({ir::Op::load,index},token)},token);},[=,this](unsigned value){ir::Instruction i{ir::Op::array_set,parent->read(),emit({ir::Op::load,index},token)};i.arguments={value};parent->write(emit(i,token));},parent->dependent};
        }
        auto slot=capture(expression(e),e.type,token);
        return {e.type,[=,this]{return emit({ir::Op::load,slot},token);},[=,this](unsigned value){emit({ir::Op::store,slot,value},token);}};
    }
#include "pattern_lowering.inc"
    VOID_SEMANTIC_HANDLER unsigned spawned(const Expr& e) {
            auto& call=*e.parts[0];ir::Function wrapper;wrapper.name=call.target+"::$spawn."+std::to_string(module.functions.size());wrapper.module_identity=call.target.substr(0,call.target.find("::"));wrapper.source_name=wrapper.name.substr(wrapper.module_identity.size()+2);wrapper.source=source.location(e.token.offset);wrapper.visibility="private";wrapper.asynchronous=true;wrapper.worker=true;wrapper.task=e.type;wrapper.result=call.type;wrapper.parameters.clear();
            ir::Block body;ir::Instruction invoke{ir::Op::call};invoke.callee=call.target;invoke.source=wrapper.source;
            for(unsigned n=0;n<call.parts.size();++n){wrapper.parameters.push_back(call.parts[n]->type);invoke.arguments.push_back(n);body.instructions.push_back({ir::Op::argument,n});}
            unsigned value=body.instructions.size();body.instructions.push_back(invoke);
            if(call.type!=Type::none){wrapper.locals.push_back(call.type);body.instructions.push_back({ir::Op::store,0,value});}
            body.end={ir::Terminator::Kind::jump,0,1};wrapper.blocks.push_back(std::move(body));wrapper.cleanup_entry=1;
            ir::Block cleanup;cleanup.instructions.push_back({ir::Op::async_drain});if(call.type==Type::none)cleanup.end={ir::Terminator::Kind::return_void};else{cleanup.instructions.push_back({ir::Op::load,0});cleanup.end={ir::Terminator::Kind::return_value,1};}wrapper.blocks.push_back(std::move(cleanup));
            auto target=wrapper.name;module.functions.push_back(std::move(wrapper));return expression(call,target);

    }
    unsigned expression(const Expr& e,std::string_view parallel_target={}) {
        using K = Expr::Kind;
        switch (e.kind) {
        case K::lambda:throw std::logic_error("Unanalyzed anonymous function");
        case K::capture_reference:{
            if(!e.implicit)return emit({ir::Op::load,cells.at(e.slot)},e.token);
            ir::Instruction cell{ir::Op::gc_construct};cell.type=e.type;cell.arguments={e.parts.empty()?emit({ir::Op::load,e.slot},e.token):expression(*e.parts[0])};return emit(cell,e.token);
        }
        case K::function_reference:{ir::Instruction reference{ir::Op::function_reference};reference.type=e.type;reference.callee=e.target;
            if(e.op=="bound")reference.arguments={expression(*e.parts[0])};
            else if(!e.parts.empty() || e.implicit){ir::Instruction environment{ir::Op::gc_construct};
                for(const auto& function:module.functions)if(function.name==e.target)environment.type=function.parameters.at(0);
                // The concrete environment is retained on the source expression.
                environment.type=e.environment;
                for(const auto& part:e.parts)environment.arguments.push_back(expression(*part));reference.arguments={emit(environment,e.token)};
            }
            return emit(reference,e.token);}
        case K::function_null:{ir::Instruction value{ir::Op::function_null};value.type=e.type;return emit(value,e.token);}
        case K::function_call:{auto function=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);std::vector<unsigned> arguments;for(unsigned n=1;n<e.parts.size();++n)arguments.push_back(snapshot(expression(*e.parts[n]),e.parts[n]->type,e.token));ir::Instruction call{ir::Op::function_call,emit({ir::Op::load,function},e.token)};for(auto slot:arguments)call.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(call,e.token);}
        case K::test: return lower_test(e);
        case K::match: return lower_match(e);
        case K::sum: {
            ir::Instruction construct{ir::Op::sum};construct.type=e.type;construct.immediate=e.slot;
            std::vector<unsigned> slots;for(const auto& child:e.parts)slots.push_back(snapshot(expression(*child),child->type,child->token));
            for(auto slot:slots)construct.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(construct,e.token);
        }
        case K::propagate: {
            const auto type=e.parts[0]->type;
            auto source=capture(expression(*e.parts[0]),type,e.token);
            auto success=block(),failure=block();
            auto tag=extract(emit({ir::Op::load,source},e.token),0,e.token);
            terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,tag,constant(Type::i32,{0,{}},e.token)},e.token),success,failure});
            current=failure;ir::Instruction payload{ir::Op::sum_get,emit({ir::Op::load,source},e.token)};payload.immediate=2;
            auto value=emit(payload,e.token);ir::Instruction propagate{ir::Op::sum};propagate.type=output.result;propagate.immediate=1;propagate.arguments={value};
            return_through_cleanup(emit(propagate,e.token),e.token,true);
            current=success;ir::Instruction payload_ok{ir::Op::sum_get,emit({ir::Op::load,source},e.token)};payload_ok.immediate=1;return emit(payload_ok,e.token);
        }
        case K::array_default: return repeated_array(e.type,*e.type.definition->extent,e.token,[&]{return expression(*e.parts[0]);});
        case K::dictionary_literal: {
            std::vector<unsigned> pairs;for(unsigned n=0;n<e.parts.size();n+=2){auto key=snapshot(expression(*e.parts[n]),e.parts[n]->type,e.token);auto value=snapshot(expression(*e.parts[n+1]),e.parts[n+1]->type,e.token);pairs.push_back(snapshot(dictionary_pair(emit({ir::Op::load,key},e.token),emit({ir::Op::load,value},e.token),e.type,e.token),ir::array_element(e.type),e.token));}
            ir::Instruction create{ir::Op::dict_new};create.type=e.type;for(auto pair:pairs)create.arguments.push_back(emit({ir::Op::load,pair},e.token));return emit(create,e.token);
        }
        case K::array_literal: {
            ir::Instruction i{ir::Op::fixed_array};i.type=e.type;
            std::vector<unsigned> slots;
            for(const auto& part:e.parts) {auto value=expression(*part);slots.push_back(snapshot(value,part->type,part->token));}
            for(auto slot:slots) i.arguments.push_back(emit({ir::Op::load,slot},e.token));
            return emit(i,e.token);
        }
        case K::array_index: return select_expression(e).read();
        case K::array_slice: {
            auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);
            auto first=snapshot(expression(*e.parts[1]),e.parts[1]->type,e.token);
            auto last=snapshot(expression(*e.parts[2]),e.parts[2]->type,e.token);
            ir::Instruction slice{ir::Op::array_slice,emit({ir::Op::load,owner},e.token),emit({ir::Op::load,first},e.token)};slice.type=e.type;slice.immediate=e.op=="..<";slice.arguments={emit({ir::Op::load,last},e.token)};return emit(slice,e.token);
        }
        case K::base_initialization: throw std::logic_error("base initialization is a dedicated prelude");
        case K::literal: {
            return constant(e.type,(e.type.kind == Type::aggregate || ir::collection_type(e.type) || e.type==Type::string) ? e.aggregate_value : ir::ConstantValue{e.value,{}},e.token);
        }
        case K::field: { auto value=expression(*e.parts[0]); if(e.target=="$dict.count")return emit({ir::Op::dict_count,value},e.token);if(e.target=="$array.count")return emit({ir::Op::array_count,value},e.token); ir::Instruction i{e.parts[0]->type.kind==Type::managed_ref ? ir::Op::gc_read : ir::Op::extract,value}; i.immediate=e.slot; return emit(i,e.token); }
        case K::construction: {
            std::vector<unsigned> arguments;
            for (const auto& part : e.parts) {
                auto argument = static_cast<unsigned>(output.locals.size()); output.locals.push_back(part->type);
                auto value = expression(*part); emit({ir::Op::store,argument,value},part->token); arguments.push_back(argument);
            }
            if (e.initializer.empty()) {
                std::vector<unsigned> fields;
                for (unsigned field=0;field<e.type.definition->fields.size();++field) {
                    Type t=ir::field_type(e.type,field);
                    std::optional<unsigned> selected;
                    for (unsigned n=0;n<arguments.size();++n) if (e.field_indexes[n]==field) selected=emit({ir::Op::load,arguments[n]},e.token);
                    for (const auto& child:e.omitted) if (child->slot==field) selected=expression(*child);
                    auto value=selected ? *selected : constant(t,e.aggregate_value.fields.at(field),e.token);
                    fields.push_back(snapshot(value,t,e.token));
                }
                ir::Instruction i{e.type.kind==Type::managed_ref ? ir::Op::gc_construct : ir::Op::aggregate}; i.type=e.type;
                for (auto field:fields) i.arguments.push_back(emit({ir::Op::load,field},e.token));
                return emit(i,e.token);
            }
            bool staged=e.type.kind==Type::managed_ref;
            auto storage_type=e.type; storage_type.construction=staged;
            auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(storage_type);
            unsigned initial;
            if (staged) {
                output.construction_origins[slot]=slot;
                ir::Instruction begin{ir::Op::stage_begin,slot}; begin.type=storage_type; initial=emit(begin,e.token);
            } else initial=constant(e.type,e.aggregate_value,e.token);
            emit({ir::Op::store,slot,initial},e.token);
            for (unsigned field=0;!staged && field<e.type.definition->fields.size();++field) {
                std::optional<unsigned> value;
                if (staged) {
                    Type field_type=e.type.definition->fields[field].type;
                    if (ir::requires_initialization(field_type)) continue;
                    value=constant(field_type,e.aggregate_value.fields.at(field),e.token);
                }
                if (e.initializer.empty()) for (unsigned n=0;n<arguments.size();++n)
                    if (e.field_indexes[n]==field) value = emit({ir::Op::load,arguments[n]},e.token);
                for (const auto& child : e.omitted) if (child->slot==field) value = expression(*child);
                if (value) {
                    auto aggregate = emit({ir::Op::load,slot},e.token);
                    if (staged) { ir::Instruction write{ir::Op::gc_write,aggregate,*value}; write.immediate=field; emit(write,e.token); }
                    else { auto updated = insert(aggregate,*value,field,e.token); emit({ir::Op::store,slot,updated},e.token); }
                }
            }
            if (!e.initializer.empty()) {
                ir::Instruction call{ir::Op::call}; call.callee=e.initializer; call.receiver=ir::Place{slot,{}}; call.initialization=true;
                call.arguments.resize(arguments.size());
                for (unsigned n=0;n<arguments.size();++n) call.arguments[e.field_indexes[n]]=emit({ir::Op::load,arguments[n]},e.token);
                emit(call,e.token);
            }
            if (staged) {
                ir::Instruction commit{ir::Op::gc_construct}; commit.type=e.type; commit.receiver=ir::Place{slot,{}};
                for (unsigned field=0;field<e.type.definition->fields.size();++field) {
                    auto view=emit({ir::Op::load,slot},e.token);
                    ir::Instruction read{ir::Op::gc_read,view}; read.immediate=field;
                    commit.arguments.push_back(emit(read,e.token));
                }
                return emit(commit,e.token);
            }
            return emit({ir::Op::load,slot},e.token);
        }
        case K::spawned:return spawned(e);
        case K::task_cancel: return emit({ir::Op::async_cancel,expression(*e.parts[0])},e.token);
        case K::awaited: return emit({ir::Op::await,expression(*e.parts[0])},e.token);
        case K::name: return emit({ir::Op::load, e.slot}, e.token);
        case K::method_call: case K::call: {
            if(e.target=="$event.emit")return event_emit(e);
            if(e.target.starts_with("$dict.")) {
                auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);auto key=e.parts.size()>1?expression(*e.parts[1]):0;
                return emit({e.token.text=="clear"?ir::Op::dict_clear:e.token.text=="contains"?ir::Op::dict_contains:ir::Op::dict_remove,emit({ir::Op::load,owner},e.token),key},e.token);
            }
            if(e.target.starts_with("$array.")) {
                auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);
                if(e.token.text=="clear")return emit({ir::Op::list_clear,emit({ir::Op::load,owner},e.token)},e.token);
                auto argument=expression(*e.parts[1]);
                if(e.token.text=="insert") {
                    ir::Instruction boundary{ir::Op::list_index,emit({ir::Op::load,owner},e.token),argument};boundary.immediate=1;
                    auto index=snapshot(emit(boundary,e.token),Type::u64,e.token);
                    auto version=snapshot(emit({ir::Op::list_version,emit({ir::Op::load,owner},e.token)},e.token),Type::u64,e.token);
                    auto value=snapshot(expression(*e.parts[2]),e.parts[2]->type,e.token);
                    ir::Instruction insert{ir::Op::list_insert,emit({ir::Op::load,owner},e.token),emit({ir::Op::load,index},e.token)};insert.arguments={emit({ir::Op::load,value},e.token),emit({ir::Op::load,version},e.token)};return emit(insert,e.token);
                }
                return emit({e.token.text=="push" ? ir::Op::list_push : ir::Op::list_remove,emit({ir::Op::load,owner},e.token),argument},e.token);
            }
            std::optional<ir::Place> receiver;
            std::optional<Selection> selected_receiver;
            std::size_t first = 0;
            if (e.kind == K::method_call) {
                first = 1;
                if(e.interface_call) {
                    ir::Instruction prepare{ir::Op::interface_prepare,expression(*e.parts[0])};prepare.callee=e.target;
                    auto prepared=expression_type(*e.parts[0]);prepared.kind=Type::prepared_receiver;prepared.nullable=false;prepared.preparation=e.target;
                    prepare.type=prepared;auto slot=snapshot(emit(prepare,e.token),prepared,e.token);receiver=ir::Place{slot,{}};
                } else
                if (e.parts[0]->type.kind==Type::managed_ref) {
                    auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}};
                } else if(e.selected_receiver) {
                    selected_receiver=select_expression(*e.parts[0]);
                    auto slot=snapshot(selected_receiver->read(),expression_type(*e.parts[0]),e.token);receiver=ir::Place{slot,{}};
                } else if (e.receiver_addressable) receiver=place(*e.parts[0]);
                else { auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}}; }

            }
            // Materialize each argument in a slot: later short-circuit arguments
            // may create new blocks, while IR instruction values stay block-local.
            std::vector<unsigned> slots;std::optional<unsigned> native_callback;
            for (std::size_t n = first; n < e.parts.size(); ++n) {
                const auto& arg = e.parts[n];
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(expression_type(*arg));
                auto value = expression(*arg);if(e.native_symbol=="vd_native_fixture_call#register" && n==1)native_callback=value;
                if (expression_type(*arg).construction) {
                    output.construction_origins[slot]=view_origin(value);
                }
                emit({ir::Op::store, slot, value}, arg->token); slots.push_back(slot);
            }
            if(selected_receiver && !e.parts[0]->property)emit({ir::Op::store,receiver->slot,selected_receiver->read()},e.token);
            ir::Instruction i{e.asynchronous_call || !parallel_target.empty() ? ir::Op::async_call : e.interface_call ? ir::Op::interface_call : !e.virtual_slot.empty() && !e.super_call ? ir::Op::virtual_call : ir::Op::call}; i.callee = parallel_target.empty()?e.target+(e.super_call && e.token.text=="deinit"?"::$body$"+Type::key(e.parts[0]->type):""):std::string(parallel_target); i.receiver = receiver;
            if (context) if (auto found=context->calls.find(&e);found!=context->calls.end())
                i.callee=ir::construction_name(e.target,{found->second.begin(),found->second.end()});
            for (auto slot : slots) i.arguments.push_back(emit({ir::Op::load, slot}, e.token));
            if(!e.native_symbol.empty()){i.op=ir::Op::native_call;i.callee=e.native_symbol;i.type=e.type;if(native_callback)i.arguments[1]=*native_callback;}
            auto result=emit(i,e.token);
            if(selected_receiver) {
                // Forward declarations need not have been lowered yet. The
                // checked call already records whether the receiver can mutate.
                if(e.receiver_writes)selected_receiver->write(emit({ir::Op::load,receiver->slot},e.token));
            }
            return result;
        }
        case K::conversion: {
            auto value = expression(*e.parts[0]); if(e.op=="identity" && expression_type(e)==expression_type(*e.parts[0])) return value; ir::Instruction i{e.op=="interface" ? ir::Op::interface_pack : e.op=="upcast" ? (expression_type(e).construction ? ir::Op::stage_prefix : ir::Op::upcast) : ir::Op::convert,value}; i.type = expression_type(e);i.callee=e.target; return emit(i,e.token);
        }
        case K::unary: {
            auto value = expression(*e.parts[0]);
            if (e.op == "not") return emit({ir::Op::logical_not, value}, e.token);
            if (e.op == "+") return value;
            return emit({ir::Op::negate,value},e.token);
        }
        case K::binary: {
            auto left = expression(*e.parts[0]);
            auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(e.parts[0]->type);
            emit({ir::Op::store, slot, left}, e.token);
            if (e.op == "and" || e.op == "or") {
                auto rhs = block(), merge = block();
                terminate({ir::Terminator::Kind::branch, left, e.op == "and" ? rhs : merge, e.op == "and" ? merge : rhs});
                current = rhs; auto right = expression(*e.parts[1]); emit({ir::Op::store, slot, right}, e.token); jump(merge);
                current = merge; return emit({ir::Op::load, slot}, e.token);
            }
            auto right = expression(*e.parts[1]); left = emit({ir::Op::load, slot}, e.token);
            if(e.parts[0]->type==Type::string) {
                auto equal=emit({e.op=="+" ? ir::Op::string_concat : ir::Op::string_equal,left,right},e.token);
                return e.op=="!=" ? emit({ir::Op::logical_not,equal},e.token) : equal;
            }
            return emit({operation(e.op), left, right}, e.token);
        }
        }
        throw std::logic_error("unknown typed expression");
    }
    void initialize_base(const Expr& e) {
        std::vector<unsigned> arguments;
        for (const auto& part:e.parts) arguments.push_back(snapshot(expression(*part),expression_type(*part),part->token));
        ir::Instruction call{ir::Op::base_init}; call.type=e.type; call.callee=e.initializer;
        call.receiver=ir::Place{0,{}};
        if (!e.initializer.empty()) {
            call.arguments.resize(arguments.size());
            for (unsigned n=0;n<arguments.size();++n) call.arguments[e.field_indexes[n]]=emit({ir::Op::load,arguments[n]},e.token);
        } else {
            std::vector<unsigned> fields;
            for (unsigned field=0;field<e.type.definition->fields.size();++field) {
                std::optional<unsigned> selected;
                for(unsigned n=0;n<arguments.size();++n) if(e.field_indexes[n]==field) selected=emit({ir::Op::load,arguments[n]},e.token);
                for(const auto& child:e.omitted) if(child->slot==field) selected=expression(*child);
                Type t=e.type.definition->fields[field].type;
                fields.push_back(snapshot(selected ? *selected : constant(t,e.aggregate_value.fields.at(field),e.token),t,e.token));
            }
            for(auto field:fields) call.arguments.push_back(emit({ir::Op::load,field},e.token));
        }
        emit(call,e.token);
    }
    unsigned event_emit(const Expr& expression_node){
        const auto& token=expression_node.token;auto array=expression_node.parts[0]->type;auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};auto owner=snapshot(expression(*expression_node.parts[0]),array,token);std::vector<unsigned> arguments;for(unsigned n=1;n<expression_node.parts.size();++n)arguments.push_back(snapshot(expression(*expression_node.parts[n]),expression_node.parts[n]->type,token));
        auto count=snapshot(emit({ir::Op::array_count,load(owner)},token),Type::i64,token);ir::Instruction slice{ir::Op::array_slice,load(owner),constant(Type::i64,{0,{}},token)};slice.type=array;slice.immediate=1;slice.arguments={load(count)};auto subscribers=capture(emit(slice,token),array,token);auto cursor=snapshot(constant(Type::i64,{0,{}},token),Type::i64,token);auto test=block(),body=block(),done=block();jump(test);current=test;terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,load(cursor),load(count)},token),body,done});current=body;
        ir::Instruction call{ir::Op::function_call,emit({ir::Op::array_get,load(subscribers),load(cursor)},token)};for(auto argument:arguments)call.arguments.push_back(load(argument));emit(call,token);emit({ir::Op::store,cursor,emit({ir::Op::add,load(cursor),constant(Type::i64,{1,{}},token)},token)},token);jump(test);current=done;return 0;
    }
    void event_subscription(const Statement& statement){
        const auto& token=statement.token;auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};auto array=statement.assigned_type;auto selected=statement.selection?select_expression(*statement.selection):Selection{array,[&]{return read_place(ir::Place{statement.slot,statement.field_indexes},token);},{}};auto owner=snapshot(selected.read(),array,token);auto callback=snapshot(expression(*statement.expression),statement.expression->type,token);
        if(statement.op=="+="){emit({ir::Op::list_push,load(owner),load(callback)},token);return;}
        auto cursor=snapshot(emit({ir::Op::array_count,load(owner)},token),Type::i64,token);auto test=block(),body=block(),remove=block(),done=block();jump(test);current=test;terminate({ir::Terminator::Kind::branch,emit({ir::Op::greater,load(cursor),constant(Type::i64,{0,{}},token)},token),body,done});current=body;emit({ir::Op::store,cursor,emit({ir::Op::subtract,load(cursor),constant(Type::i64,{1,{}},token)},token)},token);auto selected_callback=emit({ir::Op::array_get,load(owner),load(cursor)},token);terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,selected_callback,load(callback)},token),remove,test});current=remove;emit({ir::Op::list_remove,load(owner),load(cursor)},token);jump(done);current=done;
    }
    void sequence(const std::vector<Statement>& body) { for (const auto& s : body) if (current) statement(s); }
    void statement(const Statement& s) {
        using K = Statement::Kind;
        switch (s.kind) {
        case K::match: {
            auto subject=capture(expression(*s.expression),s.expression->type,s.token);auto done=block();
            for(const auto& arm:s.body){auto yes=block(),no=block();lower_pattern(*arm.pattern,subject,yes,no);current=yes;sequence(arm.body);if(current)jump(done);current=no;}
            if(s.flow&next_flow){jump(done);current=done;}else terminate({ir::Terminator::Kind::unreachable});break;
        }
        case K::scoped:{
            if(s.task_group){emit({ir::Op::store,s.slot,emit({ir::Op::async_group_enter},s.token)},s.token);with_scopes.push_back({s.slot,true});sequence(s.body);if(current)join_group(s.slot,false,s.token);with_scopes.pop_back();break;}
            std::optional<unsigned> group;if(output.asynchronous)group=snapshot(emit({ir::Op::async_group_enter},s.token),Type::u64,s.token);
            auto home=cells.at(s.slot);auto cell=output.locals.at(home);auto field=ir::field_type(cell,0);ir::Instruction construct{ir::Op::gc_construct};construct.type=cell;construct.arguments={constant(field,{0,{}},s.token)};emit({ir::Op::store,home,emit(construct,s.token)},s.token);
            auto node=register_cleanup(expression(*s.selection),false,s.token);
            auto value=expression(*s.expression);emit({ir::Op::store,s.slot,value},s.token);
            ir::Instruction activate{ir::Op::gc_write,emit({ir::Op::load,node},s.token),constant(Type::boolean,{1,{}},s.token)};activate.immediate=2;emit(activate,s.token);
            with_scopes.push_back({node,false,group});sequence(s.body);if(current)leave_with(with_scopes.back(),s.token,false);with_scopes.pop_back();break;
        }
        case K::deferred:register_cleanup(expression(*s.expression),true,s.token);break;
        case K::no_op: break;
        case K::variable: {
            ir::Instruction zero{ir::Op::constant}; zero.type = output.locals[s.slot];
            auto value = s.expression ? expression(*s.expression) : emit(zero, s.token);
            ir::Instruction store{ir::Op::store,s.slot,value};store.initialization=true;emit(store,s.token); break;
        }
        case K::assignment: {
            if(s.event){event_subscription(s);break;}
            if(s.selection) {
                auto selected=select_expression(*s.selection);auto old=(selected.read_before_write || s.op!="=")?selected.read():0;std::optional<unsigned> old_slot;
                if(s.op!="=")old_slot=snapshot(old,s.assigned_type,s.token);
                auto value=expression(*s.expression);
                if(old_slot)value=emit({s.assigned_type==Type::string ? ir::Op::string_concat : operation(s.op.substr(0,1)),emit({ir::Op::load,*old_slot},s.token),value},s.token);
                selected.write(value);break;
            }
            if(s.index) {
                auto selected=select_place({s.slot,{}},s.field_indexes,s.token);
                auto array_type=output.locals[selected.slot];for(auto field:selected.fields)array_type=ir::field_type(array_type,field);
                if(array_type.kind==Type::dictionary) {
                    auto owner=snapshot(read_place(selected,s.token),array_type,s.token);auto index_slot=snapshot(expression(*s.index),s.index->type,s.index->token);auto object=emit({ir::Op::load,owner},s.token);auto version=snapshot(emit({ir::Op::dict_version,object},s.token),Type::u64,s.token);std::optional<unsigned> old;
                    if(s.op!="=")old=snapshot(emit({ir::Op::dict_get,object,emit({ir::Op::load,index_slot},s.token)},s.token),s.assigned_type,s.token);
                    auto value=expression(*s.expression);if(old)value=emit({s.assigned_type==Type::string?ir::Op::string_concat:operation(s.op.substr(0,1)),emit({ir::Op::load,*old},s.token),value},s.token);
                    dictionary_set(emit({ir::Op::load,owner},s.token),emit({ir::Op::load,index_slot},s.token),value,emit({ir::Op::load,version},s.token),array_type,s.token);break;
                }
                auto index=expression(*s.index);auto index_slot=snapshot(index,s.index->type,s.index->token);
                auto array=read_place(selected,s.token);
                if(array_type.kind==Type::growable_array) {
                    auto owner_slot=snapshot(array,array_type,s.token);
                    auto normalized=emit({ir::Op::list_index,array,index},s.index->token);
                    auto normalized_slot=snapshot(normalized,Type::u64,s.token);
                    std::optional<unsigned> old_slot;
                    if(s.op!="=")old_slot=snapshot(emit({ir::Op::array_get,array,normalized},s.index->token),s.assigned_type,s.token);
                    auto version_slot=snapshot(emit({ir::Op::list_version,array},s.token),Type::u64,s.token);
                    auto value=expression(*s.expression);
                    if(s.op!="=")value=emit({s.assigned_type==Type::string ? ir::Op::string_concat : operation(s.op.substr(0,1)),emit({ir::Op::load,*old_slot},s.token),value},s.token);
                    ir::Instruction commit{ir::Op::list_commit,emit({ir::Op::load,owner_slot},s.token),emit({ir::Op::load,normalized_slot},s.token)};
                    commit.arguments={value,emit({ir::Op::load,version_slot},s.token)};emit(commit,s.index->token);break;
                }
                auto old=emit({ir::Op::array_get,array,index},s.index->token); // Bounds check before RHS, even for '='.
                std::optional<unsigned> old_slot;
                if(s.op!="=") old_slot=snapshot(old,s.assigned_type,s.token);
                auto value=expression(*s.expression);
                if(old_slot) value=emit({s.assigned_type==Type::string ? ir::Op::string_concat : operation(s.op.substr(0,1)),emit({ir::Op::load,*old_slot},s.token),value},s.token);
                index=emit({ir::Op::load,index_slot},s.index->token);
                array=read_place(selected,s.token); // Reacquire current inline bytes after RHS effects.
                ir::Instruction update{ir::Op::array_set,array,index};update.arguments={value};
                auto updated=emit(update,s.index->token);write_place(selected,updated,s.token);break;
            }
            auto selected=select_place({s.slot,{}},s.field_indexes,s.token);
            // Load compound-assignment LHS before evaluating its RHS.
            std::optional<unsigned> old_slot;
            if (s.op != "=") {
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(s.assigned_type); old_slot = slot;
                auto old = read_place(selected,s.token);
                emit({ir::Op::store, slot, old}, s.token);
            }
            auto value = expression(*s.expression);
            if (old_slot) {
                auto old = emit({ir::Op::load, *old_slot}, s.token);
                value = emit({s.assigned_type==Type::string ? ir::Op::string_concat : operation(s.op.substr(0, 1)), old, value}, s.token);
            }
            write_place(selected,value,s.token); break;
        }
        case K::expression: expression(*s.expression); break;
        case K::returned:
            return_through_cleanup(s.expression?std::optional<unsigned>{expression(*s.expression)}:std::nullopt,s.token,&s!=normal_return);
            break;
        case K::conditional: {
            auto yes=block(),no=block(),merge=block();
            lower_condition(*s.expression,yes,no);
            current = yes; sequence(s.body); bool yes_next = current.has_value(); if (current) jump(merge);
            current = no; sequence(s.otherwise); bool no_next = current.has_value(); if (current) jump(merge);
            if (yes_next || no_next) current = merge;
            break;
        }
        case K::loop: {
            if(!s.binder.text.empty()) {
                const auto item=output.locals[s.slot];
                auto scalar=[&](Type t,std::uint64_t bits){return constant(t,{bits,{}},s.token);};
                auto load=[&](unsigned slot){return emit({ir::Op::load,slot},s.token);};
                auto store=[&](unsigned slot,unsigned value){ir::Instruction store{ir::Op::store,slot,value};store.initialization=slot==s.slot || (!s.index_binder.text.empty() && slot==s.index_slot);emit(store,s.token);};
                unsigned cursor,terminal,direction;std::optional<unsigned> owner,version;std::optional<Selection> fixed_view;
                auto test=block(),body=block(),step=block(),done=block();
                if(s.range_end) {
                    auto first=snapshot(expression(*s.expression),item,s.token);
                    auto last=snapshot(expression(*s.range_end),item,s.token);
                    direction=snapshot(emit({ir::Op::less_equal,load(first),load(last)},s.token),Type::boolean,s.token);
                    cursor=snapshot(load(s.reverse?last:first),item,s.token);
                    terminal=snapshot(load(s.reverse?first:last),item,s.token);
                    if(s.reverse) {
                        store(direction,emit({ir::Op::logical_not,load(direction)},s.token));
                        if(s.exclusive) {
                            auto prepare=block();terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,load(first),load(last)},s.token),prepare,done});current=prepare;
                            auto up=block(),down=block(),ready=block();terminate({ir::Terminator::Kind::branch,load(direction),up,down});
                            current=up;store(cursor,emit({ir::Op::add,load(cursor),scalar(item,1)},s.token));jump(ready);
                            current=down;store(cursor,emit({ir::Op::subtract,load(cursor),scalar(item,1)},s.token));jump(ready);current=ready;
                        }
                    }
                } else {
                    auto array=s.expression->type;
                    // Root the selected handle without invoking fixed value-copy storage.
                    owner=static_cast<unsigned>(output.locals.size());output.locals.push_back(array);
                    if(array.kind==Type::fixed_array)fixed_view=select_expression(*s.expression);
                    if(array.kind==Type::dictionary)store(*owner,expression(*s.expression));else {ir::Instruction capture{ir::Op::array_capture,fixed_view ? fixed_view->read() : expression(*s.expression)};capture.immediate=*owner;emit(capture,s.token);}
                    auto count=snapshot(emit({array.kind==Type::dictionary?ir::Op::dict_count:ir::Op::array_count,load(*owner)},s.token),Type::i64,s.token);
                    if(array.kind==Type::growable_array || array.kind==Type::dictionary)version=snapshot(emit({array.kind==Type::dictionary?ir::Op::dict_version:ir::Op::list_version,load(*owner)},s.token),Type::u64,s.token);
                    cursor=snapshot(s.reverse ? emit({ir::Op::subtract,load(count),scalar(Type::i64,1)},s.token) : scalar(Type::i64,0),Type::i64,s.token);
                    terminal=snapshot(s.reverse ? scalar(Type::i64,~std::uint64_t{0}) : load(count),Type::i64,s.token);
                    direction=snapshot(scalar(Type::boolean,!s.reverse),Type::boolean,s.token);
                }
                jump(test);current=test;
                if(fixed_view){ir::Instruction refresh{ir::Op::array_capture,fixed_view->read()};refresh.immediate=*owner;emit(refresh,s.token);}
                if(version)emit({s.expression->type.kind==Type::dictionary?ir::Op::dict_check:ir::Op::list_check,load(*owner),load(*version)},s.token);
                if(!s.range_end || (s.exclusive && !s.reverse))terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,load(cursor),load(terminal)},s.token),body,done});else jump(body);
                current=body;
                if(!s.range_end && s.expression->type.kind==Type::dictionary){auto entry=emit({ir::Op::dict_entry,load(*owner),load(cursor)},s.token);ir::Instruction value{ir::Op::extract,entry};value.immediate=1;store(s.slot,emit(value,s.token));if(!s.index_binder.text.empty()){ir::Instruction key{ir::Op::extract,entry};key.immediate=0;store(s.index_slot,emit(key,s.token));}}
                else {store(s.slot,s.range_end ? load(cursor) : emit({ir::Op::array_get,load(*owner),load(cursor)},s.token));
                if(!s.index_binder.text.empty())store(s.index_slot,load(cursor));}
                loops.emplace_back(step,done);loop_with_depth.push_back(with_scopes.size());sequence(s.body);if(current)jump(step);loops.pop_back();loop_with_depth.pop_back();
                current=step;
                if(s.range_end && (!s.exclusive || s.reverse)) {
                    auto advance=block();terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,load(cursor),load(terminal)},s.token),done,advance});current=advance;
                }
                auto up=block(),down=block();terminate({ir::Terminator::Kind::branch,load(direction),up,down});
                current=up;store(cursor,emit({ir::Op::add,load(cursor),scalar(s.range_end?item:Type(Type::i64),1)},s.token));jump(test);
                current=down;store(cursor,emit({ir::Op::subtract,load(cursor),scalar(s.range_end?item:Type(Type::i64),1)},s.token));jump(test);
                current=done;break;
            }
            auto test = block(), body = block(), done = block(); jump(test);
            current = test;
            if (always(s.expression.get())) jump(body);
            else lower_condition(*s.expression,body,done);
            loops.emplace_back(test, done);loop_with_depth.push_back(with_scopes.size());current = body; sequence(s.body); if (current) jump(test); loops.pop_back();loop_with_depth.pop_back();
            if (s.flow & next_flow) current = done;
            break;
        }
        case K::broken:for(auto n=with_scopes.size();n>loop_with_depth.back();--n)leave_with(with_scopes[n-1],s.token);jump(loops.back().second); break;
        case K::continued:for(auto n=with_scopes.size();n>loop_with_depth.back();--n)leave_with(with_scopes[n-1],s.token);jump(loops.back().first); break;
        }
    }
public:
    explicit Lowering(const SourceFile& s, ir::Module& m, StringPool& p) : source(s), module(m), strings(p) {}
    ir::Function lower(const Function& f, const ConstructionContext* construction=nullptr) {
        context=construction;normal_return=!f.body.empty() && f.body.back().kind==Statement::Kind::returned?&f.body.back():nullptr;
        output = {}; output.name = f.symbol; output.source_name = f.concrete_name.empty() ? f.token.text : f.concrete_name; output.display_name=f.display_name; output.source = source.location(f.token.offset);
        output.asynchronous=f.asynchronous;output.task=f.task;
        output.bound_receiver=f.bound_receiver;output.closure=f.anonymous;cells.clear();cleanup_head.reset();return_slot.reset();cleanup_jumps.clear();with_scopes.clear();loop_with_depth.clear();
        output.parameters.clear(); output.result = f.result; output.locals = f.locals; current = block();
        if(f.cleanup_type!=Type::none){cleanup_head=output.locals.size();auto head=f.cleanup_type;head.nullable=true;output.locals.push_back(head);}
        if((cleanup_head || f.asynchronous) && f.result!=Type::none){return_slot=output.locals.size();output.locals.push_back(f.result);}
        for(const auto& [slot,type]:f.captured_locals){cells.emplace(slot,static_cast<unsigned>(output.locals.size()));output.locals.push_back(type);}
        for(const auto& [slot,field]:f.capture_inputs){
            auto environment=emit({ir::Op::argument,0},f.token);ir::Instruction read{ir::Op::gc_read,environment};read.immediate=field;
            emit({ir::Op::store,cells.at(slot),emit(read,f.token)},f.token);
        }
        if(f.receiver && cells.contains(f.receiver->slot)){
            auto value=emit({ir::Op::argument,0},f.receiver->token);ir::Instruction store{ir::Op::store,f.receiver->slot,value};store.initialization=true;emit(store,f.receiver->token);
        }
        output.virtual_slot=f.virtual_slot; output.overriding=f.overriding; output.visibility=f.visibility;
        if(f.receiver) output.method_name=f.concrete_method_name.empty()?f.token.text:f.concrete_method_name;
        output.accessor=f.accessor; output.receiver = f.receiver.has_value(); output.receiver_mutates = f.receiver_mutates; output.initializer = f.initializer;
        if (f.receiver) {
            output.source_name = f.receiver->type.definition->name + "::method::" + output.method_name;
            output.parameters.push_back(f.receiver->type);
        }
        for (std::size_t n = 0; n < f.parameters.size(); ++n) {
            const auto& p = f.parameters[n]; output.parameters.push_back(p.type);
            auto value = emit({ir::Op::argument, static_cast<unsigned>(n + (f.receiver ? 1 : 0))}, p.token); ir::Instruction store{ir::Op::store,p.slot,value};store.initialization=true;emit(store,p.token);
        }
        if (context) {
            output.construction_origins=context->aliases;
            for (const auto& [slot,origin]:context->aliases) output.locals.at(slot).construction=true;
            for (auto index:context->parameters) output.parameters.at(index).construction=true;
            if (!f.initializer) output.name=ir::construction_name(f.symbol,{context->parameters.begin(),context->parameters.end()});
        }
        if (f.base_prelude) initialize_base(*f.base_prelude);
        if (f.initializer && f.receiver->type.kind==Type::managed_ref) {
            auto base=f.receiver->type.definition->base.lock();
            for (unsigned field=base ? static_cast<unsigned>(base->fields.size()) : 0;field<f.receiver->type.definition->fields.size();++field) {
                Type t=f.receiver->type.definition->fields[field].type;
                if (ir::requires_initialization(t)) continue;
                std::optional<unsigned> selected;
                for(const auto& child:f.initialization_defaults) if(child->slot==field) selected=expression(*child);
                auto value=selected ? *selected : constant(t,f.initialization_value.fields.at(field),f.token);
                auto view=emit({ir::Op::load,0},f.token);
                ir::Instruction write{ir::Op::gc_write,view,value}; write.immediate=field; emit(write,f.token);
            }
        }
        sequence(f.body);
        if(current)return_through_cleanup({},f.token);
        if(cleanup_head || f.asynchronous){
            auto entry=block();output.cleanup_entry=entry;for(auto from:cleanup_jumps)output.blocks[from].end.yes=entry;current=entry;
            if(f.asynchronous)emit({ir::Op::async_drain},f.token);
            if(cleanup_head){
            // A separate loop entry prevents executing the structured drain again.
            auto loop=block();jump(loop);current=loop;
            auto head=output.locals[*cleanup_head];auto node=head;node.nullable=false;
            auto body=block(),done=block();auto selected=emit({ir::Op::load,*cleanup_head},f.token);terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,selected,constant(head,{0,{}},f.token)},f.token),body,done});
            current=body;ir::Instruction unwrap{ir::Op::convert,emit({ir::Op::load,*cleanup_head},f.token)};unwrap.type=node;auto owner=emit(unwrap,f.token);auto handler=capture(extract_managed(owner,1,f.token),ir::field_type(node,1),f.token);auto previous=extract_managed(owner,0,f.token);emit({ir::Op::store,*cleanup_head,previous},f.token);auto active=extract_managed(owner,2,f.token);auto invoke=block();terminate({ir::Terminator::Kind::branch,active,invoke,loop});current=invoke;emit({ir::Op::cleanup_call,emit({ir::Op::load,handler},f.token)},f.token);jump(loop);
            current=done;}if(return_slot)terminate({ir::Terminator::Kind::return_value,emit({ir::Op::load,*return_slot},f.token)});else terminate({ir::Terminator::Kind::return_void});
        }
        return std::move(output);
    }
};
// VFS paths are normalized before touching the host filesystem. Physical root
// spelling never participates in language identity or native symbol identity.
std::string folded(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}
bool identifier(const std::string& text) {
    return !text.empty() && letter(text[0]) && !reserved.contains(text) &&
        std::all_of(text.begin(), text.end(), [](char c) { return letter(c) || digit(c); });
}
void portable_component(const SourceFile& source, const Token& at, const std::string& component) {
    bool invalid = component.empty() || component.back() == ' ' || component.back() == '.';
    for (unsigned char c : component)
        invalid |= c < 32 || c >= 127 || std::string("<>:\"|?*\\").find(static_cast<char>(c)) != std::string::npos;
    auto base = folded(component.substr(0, component.find('.')));
    invalid |= base == "con" || base == "prn" || base == "aux" || base == "nul" ||
        (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) && base[3] >= '1' && base[3] <= '9');
    if (invalid) error(source, at, Code::import_path, "Non-portable module path component '" + component + "'.",
        "Use printable ASCII filenames without Windows-reserved characters/names or trailing dots/spaces; Unicode module filenames are deferred.");
}
std::string normalize(const Unit& from, const Token& path) {
    const auto& text = path.text;
    if (text.empty() || text.find('\\') != std::string::npos)
        error(from.source, path, Code::import_path, "Invalid import path.", "Use a nonempty VFS path with / separators on every platform.");
    bool absolute = text[0] == '/';
    if (text.starts_with("//")) error(from.source, path, Code::import_path, "Invalid root-relative import path.", "A single leading / denotes the virtual project root, not an OS or network path.");
    if (!absolute && !text.starts_with("./") && !text.starts_with("../") && text.find(':') != std::string::npos)
        error(from.source, path, Code::import_path, "OS filesystem paths are not valid VOID imports.", "Use a project-root /path.void or importer-relative ./path.void instead of a drive path.");
    if (!absolute && !text.starts_with("./") && !text.starts_with("../"))
        error(from.source, path, Code::import_path, "Package imports are not implemented.", "Use /project/file.void, ./file.void or ../file.void; OS drive paths are not VFS paths.");
    std::vector<std::string> parts;
    if (!absolute) {
        auto parent = utf8_path(from.identity).parent_path();
        for (const auto& component : parent.relative_path()) parts.push_back(path_utf8(component));
    }
    std::size_t begin = absolute ? 1 : 0;
    while (begin <= text.size()) {
        auto end = text.find('/', begin); if (end == std::string::npos) end = text.size();
        auto component = text.substr(begin, end - begin);
        if (component == "..") {
            if (parts.empty()) error(from.source, path, Code::import_path, "Import escapes the project root.", "External imports require an explicit permission/identity contract and are not enabled in this phase.");
            parts.pop_back();
        } else if (!component.empty() && component != ".") {
            portable_component(from.source, path, component); parts.push_back(std::move(component));
        }
        if (end == text.size()) break;
        begin = end + 1;
    }
    if (parts.empty() || text.back() == '/' || !parts.back().ends_with(".void"))
        error(from.source, path, Code::import_path, "Import must name a .void source file.", "Supply an exact-case filename, not a directory.");
    std::string id;
    for (const auto& component : parts) id += "/" + component;
    return id;
}
class SourceReadError : public CompilationError {
    static Diagnostic record(const std::string& path, const std::string& message) {
        Diagnostic d{Code::import_read}; d.description = message;
        d.primary = {std::make_shared<const SourceFile>(path, ""), {}, 0, 0};
        d.hint = "Check the source path, permissions and storage availability.";
        return d;
    }
public:
    SourceReadError(const std::string& path, const std::string& message) : CompilationError({record(path, message)}) {}
};
class SourceGraph {
    std::filesystem::path root;
    Diagnostics& diagnostics;
    unsigned depth = 0;
    const SourceOverrides& overrides;
    const NativeProject* native=nullptr;
public:
    std::map<std::string, std::unique_ptr<Unit>> units;
    explicit SourceGraph(std::filesystem::path project_root, Diagnostics& d, const SourceOverrides& o,const NativeProject* bindings=nullptr) : root(std::move(project_root)), diagnostics(d), overrides(o),native(bindings) {}
    Unit& load(const Unit& from, const Token& path) {
        auto id = normalize(from, path);
        if (auto found = units.find(id); found != units.end()) return *found->second;
        Depth guard(depth, from.source, path);
        auto overlay = overrides.find(id);
        if (overlay != overrides.end() && !overlay->second)
            error(from.source, path, Code::import_read, "Source is explicitly unavailable: '" + id + "'.", "Restore the document or close its override to return to disk.");
        auto physical = root;
        std::string prefix;
        std::error_code ec;
        for (const auto& part : utf8_path(id).relative_path()) {
            auto spelling = path_utf8(part);
            std::set<std::string> matches;
            for (std::filesystem::directory_iterator it(physical, ec), end; !ec && it != end; it.increment(ec)) {
                auto candidate = path_utf8(it->path().filename());
                if (folded(candidate) == folded(spelling)) matches.insert(candidate);
            }
            // Merge virtual children into the same exact-case namespace. Missing
            // virtual parents are permitted; existing unreadable parents are not.
            if (ec == std::errc::no_such_file_or_directory) ec.clear();
            for (const auto& [key, value] : overrides) {
                (void)value;
                if (!key.starts_with(prefix + "/")) continue;
                auto child = key.substr(prefix.size()+1);
                child = child.substr(0, child.find('/'));
                if (folded(child) == folded(spelling)) matches.insert(child);
            }
            if (ec) error(from.source, path, Code::import_read, "Cannot inspect import directory for '" + id + "'.", "Check that each parent exists, is a readable directory and uses the exact filename case.");
            if (matches.size() > 1) error(from.source, path, Code::import_path, "Filesystem case collision for '" + spelling + "'.", "Rename case-only siblings so this project has the same identity on every platform.");
            if (matches.empty()) error(from.source, path, Code::import_read, "Cannot find imported file '" + id + "'.", "Check this path relative to the importing file or virtual project root.");
            if (*matches.begin() != spelling) error(from.source, path, Code::import_path, "Import path case mismatch: requested '" + spelling + "', found '" + *matches.begin() + "'.", "Use the exact on-disk spelling on every platform.");
            physical /= part;
            prefix += "/" + spelling;
            if (prefix != id && overrides.contains(prefix))
                error(from.source, path, Code::import_path, "An overridden source cannot also be a virtual directory: '" + prefix + "'.");
            auto status = std::filesystem::symlink_status(physical, ec);
            if (ec == std::errc::no_such_file_or_directory && overlay != overrides.end()) ec.clear();
            if (ec) error(from.source, path, Code::import_read, "Cannot inspect imported path '" + id + "'.");
            if (std::filesystem::is_symlink(status)) error(from.source, path, Code::import_path, "Symbolic links are not supported in module paths.", "Import a regular project file; symlink identity and external permissions are deferred.");
            if (std::filesystem::exists(status)) {
                auto resolved_part = std::filesystem::canonical(physical, ec);
                if (ec || resolved_part != physical) error(from.source, path, Code::import_path, "Module path contains a filesystem alias.");
            }
        }
        if (overlay == overrides.end()) {
            if (!std::filesystem::is_regular_file(physical, ec) || ec)
                error(from.source, path, Code::import_read, "Imported path is not a regular .void source file.");
        }
        // Existing host files retain alias restrictions even when overlaid. New
        // virtual files have no host identity; their owned bytes are authoritative.
        if (std::filesystem::exists(physical, ec)) {
            if (!std::filesystem::is_regular_file(physical, ec) || ec)
                error(from.source, path, Code::import_path, "Module path names a non-file host object.");
            for (const auto& [other_id, other] : units) {
                (void)other;
                auto other_path = root / utf8_path(other_id).relative_path();
                if (std::filesystem::equivalent(physical, other_path, ec) && !ec)
                    error(from.source, path, Code::import_path, "Hard-link aliases have conflicting module identities: '" + id + "' and '" + other_id + "'.", "Use one VFS filename for this physical source.");
                ec.clear();
            }
        }
        // Publish before traversing imports: function-only declaration cycles are safe.
        std::unique_ptr<Unit> unit;
        try { unit = std::make_unique<Unit>(overlay == overrides.end() ? read_source(physical) : *overlay->second, id); }
        catch (const SourceReadError&) { error(from.source, path, Code::import_read, "Cannot read imported file '" + id + "'.", "Check source file permissions and storage availability."); }
        catch (const CompilationError& failure) {
            auto records = failure.diagnostics();
            for (auto& record : records) record.primary.module_identity = id;
            throw CompilationError(std::move(records));
        }
        auto* result = unit.get(); units.emplace(id, std::move(unit));
        diagnostics.bind(result->source, id);
        auto before = diagnostics.size();
        Parser(result->source, diagnostics).parse(*result);
        result->parse_failed |= diagnostics.size() != before;
        if(native && id==native->module){for(auto& structure:result->structures)structure.definition->native_resource=true;for(auto& f:result->functions)for(const auto& b:native->bindings)if(f.token.text==b.name)f.native_symbol=b.symbol;}

        for (auto& import : result->imports) {
            try {
                auto& target = load(*result, import.path);
                import.target = target.identity;
            } catch (const CompilationError& failure) {
                diagnostics.add(failure); result->failed_import = true;
                auto qualifier = import.explicit_alias ? import.alias.text : path_utf8(utf8_path(import.path.text).stem());
                result->failed_qualifiers.insert(qualifier);
            }
        }
        return *result;
    }
};
// Only typed, pure ASTs reach this evaluator. The shared software kernel
// preserves declared precision and wrapping independently of host arithmetic.
ir::ConstantValue constant_value(const SourceFile& source, const Expr& e) {
    if(e.kind==Expr::Kind::sum) {
        ir::ConstantValue result;result.fields.resize(e.type.definition->fields.size());result.fields[0].bits=e.slot;
        const auto& variant=e.type.definition->cases.at(e.slot);
        for(unsigned n=0;n<e.parts.size();++n)result.fields[variant.begin+n]=constant_value(source,*e.parts[n]);return result;
    }
    if(e.kind==Expr::Kind::function_reference)return {0,{},e.target};
    if(e.kind==Expr::Kind::function_null)return {};
    if(e.kind==Expr::Kind::dictionary_literal){ir::ConstantValue result;for(const auto& part:e.parts)result.fields.push_back(constant_value(source,*part));return result;}
    if (e.kind == Expr::Kind::literal) return (e.type.kind == Type::aggregate || ir::collection_type(e.type) || e.type==Type::string) ? e.aggregate_value : ir::ConstantValue{e.value,{}};
    if(e.kind==Expr::Kind::array_literal) {
        ir::ConstantValue result;for(const auto& part:e.parts) result.fields.push_back(constant_value(source,*part));return result;
    }
    if(e.kind==Expr::Kind::array_index) {
        auto array=constant_value(source,*e.parts[0]);auto index=constant_value(source,*e.parts[1]);
        try {
            const auto selected=ir::checked_array_index(index.bits,e.parts[1]->type,array.repeat.value_or(array.fields.size()));
            return array.fields.at(array.repeat ? 0 : static_cast<std::size_t>(selected));
        }
        catch(const std::out_of_range&) {error(source,e.token,Code::invalid_value,"Constant array index out of bounds.");}
    }
    if (e.kind == Expr::Kind::construction) {
        auto result = e.aggregate_value;
        for (unsigned n = 0; n < e.parts.size(); ++n) result.fields[e.field_indexes[n]] = constant_value(source,*e.parts[n]);
        for (const auto& child : e.omitted) result.fields[child->slot] = constant_value(source,*child);
        return result;
    }
    if (e.kind == Expr::Kind::field) {auto value=constant_value(source,*e.parts[0]);if(e.target=="$array.count")return {value.repeat.value_or(value.fields.size()),{}};return value.fields.at(e.slot);}
    if (e.kind==Expr::Kind::binary && e.parts[0]->type==Type::string) {
        auto a=constant_value(source,*e.parts[0]).text;
        auto b=constant_value(source,*e.parts[1]).text;
        if(e.op=="+") {
            if(b.size()>ir::string_constant_limit-a.size()) error(source,e.token,Code::limit,"Folded string exceeds the 16 MiB constant limit.");
            a+=b; return {0,{},std::move(a)};
        }
        return {static_cast<std::uint64_t>(e.op=="==" ? a==b : a!=b),{}};
    }
    auto a = constant_value(source,*e.parts[0]).bits;
    try {
        if (e.kind == Expr::Kind::conversion) {
            if (ir::reference_value(e.type)) { if (!a && !e.type.nullable) throw std::invalid_argument("Cannot unwrap null in a compile-time expression"); return {a,{}}; }
            return {numeric::convert(a,e.parts[0]->type,e.type),{}};
        }
        if (e.kind == Expr::Kind::unary) return {e.op == "not" ? !a : e.op == "-" ? numeric::negate(a,e.type) : a,{}};
        if (e.op == "and" && !a) return {};
        if (e.op == "or" && a) return {1,{}};
        auto b = constant_value(source,*e.parts[1]).bits;
        if (e.op == "and" || e.op == "or") return {b,{}};
        if (ir::reference_value(e.parts[0]->type)) return {static_cast<std::uint64_t>(e.op=="==" ? a==b : a!=b),{}};
        return {numeric::binary(operation(e.op),a,b,e.parts[0]->type),{}};
    } catch (const CompilationError&) { throw; }
      catch (const std::invalid_argument& failure) {
        error(source,e.token,e.kind == Expr::Kind::conversion ? Code::numeric_conversion : Code::zero_divisor,failure.what());
    }
}
#include "capabilities.inc"
#include "construction.inc"
#include "inheritance.inc"
ir::Function member_header(const Unit& u,const Function& f) {
    ir::Function header;header.name=f.symbol;header.module_identity=u.identity;header.method_name=f.concrete_method_name.empty()?f.token.text:f.concrete_method_name;
    header.asynchronous=f.asynchronous;header.task=f.task;header.bound_receiver=f.bound_receiver;header.closure=f.anonymous;header.accessor=f.accessor;header.receiver=static_cast<bool>(f.receiver);header.initializer=f.initializer;header.visibility=f.visibility;header.result=f.result;header.virtual_slot=f.virtual_slot;header.receiver_mutates=f.receiver_mutates;
    header.parameters.clear();if(f.receiver)header.parameters.push_back(f.receiver->type);
    for(const auto& p:f.parameters)header.parameters.push_back(p.type);
    return header;
}
ir::Module member_catalog(const std::map<std::string,std::unique_ptr<Unit>>& units,const std::string& entry) {
    ir::Module m;m.root_identity=entry;
    for(const auto& [id,u]:units) {
        auto& imports=m.source_imports[id];for(const auto& i:u->imports) if(!i.target.empty()) imports.insert(i.target);
        for(const auto& s:u->structures) m.nominal_types.emplace_back(s.definition,s.definition->interface_type ? Type::interface_ref : s.definition->managed ? Type::managed_ref : Type::aggregate);
        for(const auto& f:u->functions) if(!f.parse_failed) {
            auto header=member_header(*u,f);
            if(f.interface_requirement) m.interface_requirements.push_back(std::move(header));else m.functions.push_back(std::move(header));
        }
    }
    return m;
}
void validate_graph(std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry, Diagnostics& diagnostics) {
    auto arena = std::make_shared<ir::TypeArena>();
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        structure.definition->self = structure.definition;
        structure.definition->arena = arena;
        arena->definitions.push_back(structure.definition);
    }
    // Every external resolved Type aliases this table; field edges are nonowning.

    // Discover every signature before checking any body, including cyclic imports.
    for (auto& [id, unit] : units) {
    std::vector<Function*> declarations;for(auto& f:unit->functions)declarations.push_back(&f);for(auto& f:unit->generic_functions)declarations.push_back(&f);
    for(auto* declaration:declarations) {auto& f=*declaration;
        if (f.receiver) continue;
        f.symbol = id + "::" + f.token.text;
        auto [previous, inserted] = unit->declarations.emplace(f.token.text, &f);
        if (!inserted) {
            unit->invalid_declarations.insert(f.token.text);
            try { error(unit->source, f.token, Code::duplicate, "Duplicate function '" + f.token.text + "'.", {}, {{span(unit->source, previous->second->token), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    }
    auto& root = *units.at(entry);
    for(auto& [id,u]:units)for(auto& s:u->generic_structures) {
        s.definition->identity=id+"::"+s.token.text;s.definition->module_identity=id;s.definition->visibility=s.visibility;
        if(u->declarations.contains(s.token.text) || !u->structure_names.emplace(s.token.text,&s).second)
            error(u->source,s.token,Code::duplicate,"Duplicate generic declaration '"+s.token.text+"'.");
    }
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        auto name = structure.token.text; structure.definition->identity = id + "::" + name;
        structure.definition->source_offset=structure.token.offset;structure.definition->module_identity=id;structure.definition->visibility=structure.visibility;
        if (unit->declarations.contains(name) || !unit->structure_names.emplace(name,&structure).second) {
            unit->invalid_declarations.insert(name);
            try { error(unit->source,structure.token,Code::duplicate,"Duplicate declaration '" + name + "'."); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        auto& token = c.declaration.token;
        auto previous = unit->constant_names.find(token.text);
        if (unit->structure_names.contains(token.text) || unit->declarations.contains(token.text) || previous != unit->constant_names.end()) {
            auto location = unit->structure_names.contains(token.text) ? unit->structure_names.at(token.text)->token : unit->declarations.contains(token.text) ? unit->declarations.at(token.text)->token : previous->second->declaration.token;
            try { error(unit->source, token, Code::duplicate, "Duplicate declaration '" + token.text + "'.", {}, {{span(unit->source, location), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            c.state = 3; unit->invalid_declarations.insert(token.text);
        } else unit->constant_names.emplace(token.text, &c);
    }
    for (auto& [id, unit] : units) {
        if (unit.get() != &root) for (const auto& [name, c] : root.constant_names) if (!c->file_private) {
            try {
            if (unit->constant_names.contains(name)) error(unit->source, unit->constant_names.at(name)->declaration.token, Code::duplicate, "Declaration shadows project-root constant '" + name + "'.", {}, {{span(root.source, c->declaration.token), "Previously declared"}});
            if (unit->declarations.contains(name)) error(unit->source, unit->declarations.at(name)->token, Code::duplicate, "Declaration shadows project-root constant '" + name + "'.", {}, {{span(root.source, c->declaration.token), "Previously declared"}});
            } catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
        }
        if (unit.get() != &root) for (const auto& [name,type] : root.structure_names) if (type->visibility != "private") {
            auto duplicate = [&](const Token& token) {
                try { error(unit->source,token,Code::duplicate,"Declaration shadows project-root value type '" + name + "'.",{},{{span(root.source,type->token),"Previously declared"}}); }
                catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
            };
            if (unit->structure_names.contains(name)) duplicate(unit->structure_names.at(name)->token);
            if (unit->constant_names.contains(name)) duplicate(unit->constant_names.at(name)->declaration.token);
            if (unit->declarations.contains(name)) duplicate(unit->declarations.at(name)->token);
        }
        if (unit.get() != &root) for (const auto& [name,c] : root.constant_names) if (!c->file_private && unit->structure_names.contains(name)) {
            try { error(unit->source,unit->structure_names.at(name)->token,Code::duplicate,"Declaration shadows project-root constant '" + name + "'.",{},{{span(root.source,c->declaration.token),"Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); unit->invalid_declarations.insert(name); }
        }
        for (const auto& import : unit->imports) {
            if (import.target.empty()) continue;
            if (units.at(import.target)->parse_failed) unit->failed_import = true;
            auto alias = import.explicit_alias ? import.alias.text : path_utf8(utf8_path(import.target).stem());
            try {
            if (!import.explicit_alias && !identifier(alias)) continue;
            if (unit->structure_names.contains(alias) || (root.structure_names.contains(alias) && root.structure_names.at(alias)->visibility != "private") || (import.explicit_alias && unit->declarations.contains(alias)) || unit->constant_names.contains(alias) || (root.constant_names.contains(alias) && !root.constant_names.at(alias)->file_private))
                error(unit->source, import.alias, Code::alias_conflict, "Import alias '" + alias + "' conflicts with " + (unit->declarations.contains(alias) ? "a function declaration." : "a constant declaration."), "Choose a distinct explicit alias with as.");
            auto* target = units.at(import.target).get();
            auto& targets = unit->qualifiers[alias];
            bool repeated = std::find(targets.begin(), targets.end(), target) != targets.end();
            bool different = std::any_of(targets.begin(), targets.end(), [&](Unit* previous) { return previous != target; });
            if (different && (import.explicit_alias || unit->explicit_qualifiers.contains(alias))) {
                std::vector<RelatedLocation> related;
                for (const auto& previous : unit->imports) {
                    if (&previous == &import) break;
                    auto previous_alias = previous.explicit_alias ? previous.alias.text : path_utf8(utf8_path(previous.target).stem());
                    if (previous_alias == alias && !previous.target.empty()) { related.push_back({span(unit->source, previous.alias), "Previously imported"}); break; }
                }
                error(unit->source, import.alias, Code::alias_conflict, "Conflicting import alias '" + alias + "'.", "Give these modules different aliases using as.", std::move(related));
            }
            if (import.explicit_alias) unit->explicit_qualifiers.insert(alias);
            if (!repeated) targets.push_back(target);
            } catch (const CompilationError& failure) { diagnostics.add(failure); unit->failed_qualifiers.insert(alias); }
        }
    }
    // Extents may request existing pure value constants before the ordinary
    // body/default pass. Discover runtime-only init presence first, so staged
    // resolution cannot accidentally treat such construction as a pure template.
    for(auto& [id,u]:units) for(auto& f:u->functions)
        if(f.receiver && f.token.text=="init" && f.receiver->type.kind==Type::unresolved) {
            auto found=u->structure_names.find(f.receiver->type.definition->name);
            if(found!=u->structure_names.end()) found->second->initializer=&f;
        }
    root.member_catalog=std::make_shared<ir::Module>(member_catalog(units,entry));
    std::function<void(Unit&,Function&)> prepare_function;
    bool headers_ready=false;
    std::function<void(Unit&,Structure&)> layout;
    std::function<void(Unit&,Structure&)> defaults;
    std::function<void(Unit&,Constant&)> evaluate;
    std::vector<std::string> dependency;
    evaluate = [&](Unit& owner, Constant& c) {
        if (c.state == 2) return;
        if (c.state == 3) throw Poison{};
        if (dependency.size() >= 128) error(owner.source, c.declaration.token, Code::nesting, "Constant dependency depth limit exceeded.", "Reduce the dependency chain between constants.");
        auto key = owner.identity + "::" + c.declaration.name;
        if (c.state == 1) {
            std::string chain;
            for (const auto& item : dependency) chain += item + " -> ";
            error(owner.source, c.declaration.token, Code::constant_cycle, "Circular constant dependency: " + chain + key + ".");
        }
        c.state = 1; dependency.push_back(key);
        try {
            Semantics(owner, units, root, evaluate, diagnostics, defaults).analyze_constant(c);
            c.aggregate_value = constant_value(owner.source, *c.declaration.expression); c.value = c.aggregate_value.bits;
            c.state = 2; dependency.pop_back();
        } catch (...) { c.state = 3; dependency.pop_back(); throw; }
    };
    defaults = [&](Unit& owner, Structure& structure) {
        if (structure.definition->interface_type) {structure.default_state=2;return;}
        if(structure.layout_state==1) error(owner.source,structure.token,Code::constant_cycle,"Circular layout/constant dependency.");
        if(structure.layout_state==0) layout(owner,structure);
        if (structure.layout_state != 2) throw Poison{};
        if (structure.default_state == 2) return;
        if (structure.default_state == 3) throw Poison{};
        if (structure.default_state == 1) error(owner.source,structure.token,Code::constant_cycle,"Circular field default dependency for '" + structure.token.text + "'.");
        Depth guard(diagnostics.semantic_depth(),owner.source,structure.token,128);
        structure.default_state = 1;
        try {
            Semantics semantics(owner,units,root,evaluate,diagnostics,defaults);
            if (auto base=structure.definition->base.lock()) for (auto& [id,u]:units) for (auto& parent:u->structures) if (parent.definition==base) {
                defaults(*u,parent); structure.defaults.fields=parent.defaults.fields;
            }
            for (auto& field : structure.fields) structure.defaults.fields.push_back(semantics.field_default(field));
            structure.default_state = 2;
        } catch (...) { structure.default_state = 3; throw; }
    };
    auto array_extent=[&](Unit& owner,ArraySyntax& syntax) {
        std::optional<std::uint64_t> extent;
            if(syntax.extent) {
                Semantics semantics(owner,units,root,evaluate,diagnostics,defaults);
                auto type=semantics.pure_expression(*syntax.extent);
                if(type==error_type) throw Poison{};
                if(!numeric::integer(type)) error(owner.source,syntax.extent->token,Code::unsupported_type,"Fixed-array extent requires a pure nonnegative integer module-constant expression.");
                const auto value=constant_value(owner.source,*syntax.extent).bits;
                if(numeric::signed_integer(type) && (value & (std::uint64_t{1}<<(numeric::width(type)-1))))
                    error(owner.source,syntax.extent->token,Code::integer_range,"Fixed-array extent cannot be negative.");
                if(value>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    error(owner.source,syntax.extent->token,Code::integer_range,"Array extent is not representable as an i64 count.");
                extent=value;
            }
        return extent;
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_arrays;
    for(auto& [id,u]:units)u->make_dictionary=[&](Type key,Type value) {
        auto raw=ir::dictionary_type(key,value);auto& canonical=canonical_arrays[Type::key(raw)];if(!canonical)canonical=std::const_pointer_cast<ir::ValueType>(raw.definition);
        return ir::with_readonly_paths(Type(canonical,Type::dictionary),ir::readonly_paths(raw));
    };
    for(auto& [id,u]:units) u->resolve_array=[&,owner=u.get()](Type raw,const Token&)->Type {
        auto found=std::find_if(owner->arrays.begin(),owner->arrays.end(),[&](const auto& a){return a.definition==raw.definition;});
        if(found==owner->arrays.end()) return raw; // Already canonical.
        auto& syntax=*found;
        if(syntax.state==2) {auto result=syntax.resolved;result.readonly=raw.readonly;return result;}
        if(syntax.state==3) throw Poison{};
        if(syntax.state==1) error(owner->source,syntax.token,Code::constant_cycle,"Circular array extent/type dependency.");
        Depth guard(diagnostics.semantic_depth(),owner->source,syntax.token,128);syntax.state=1;
        try {
            auto element=resolve_type(syntax.element,*owner,root,units,syntax.token);
            if(syntax.key) {
                auto key=resolve_type(*syntax.key,*owner,root,units,syntax.token);
                if(key.kind==Type::aggregate)for(auto& [id,u]:units)for(auto& child:u->structures)if(child.definition==key.definition)layout(*u,child);
                if(!ir::dictionary_key_supported(key))error(owner->source,syntax.token,Code::unsupported_type,"Unsupported dictionary key type.");
                syntax.resolved=owner->make_dictionary(key,element);syntax.state=2;auto result=syntax.resolved;result.readonly=raw.readonly;return result;
            }
            auto extent=array_extent(*owner,syntax);
            if(extent && element.kind==Type::aggregate) for(auto& [child_id,child_owner]:units) for(auto& child:child_owner->structures)
                if(child.definition==element.definition) {
                    layout(*child_owner,child);
                }

            // Fixed ownership remains finite; growable edges have pointer-sized storage.
            ir::CompactValueShape shape;
            try {shape=extent ? ir::compact_value_shape(element) : ir::CompactValueShape{0,sizeof(void*),1};}
            catch(const std::length_error& failure) {error(owner->source,syntax.token,Code::limit,failure.what());}
            catch(const std::invalid_argument& failure) {error(owner->source,syntax.token,Code::unsupported_type,failure.what());}
            if(shape.depth>=128) error(owner->source,syntax.token,Code::nesting,"Array value shape depth limit exceeded.");
            if(shape.template_nodes>=65536) error(owner->source,syntax.token,Code::limit,"Array default-template analysis work exhausted.");
            if(extent && shape.scalar_bytes && *extent>std::numeric_limits<std::uint64_t>::max()/shape.scalar_bytes)
                error(owner->source,syntax.token,Code::limit,"Array logical scalar extent is not representable in 64 bits.");
            const auto key=ir::array_identity(element,extent);
            auto& canonical=canonical_arrays[key];
            if(!canonical) {
                canonical=std::make_shared<ir::ValueType>();canonical->identity=key;canonical->name=key;
                canonical->element=ir::FieldType(element);canonical->extent=extent;
                canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);
            }
            syntax.resolved=Type(canonical,extent ? Type::fixed_array : Type::growable_array);syntax.state=2;
            auto result=syntax.resolved;result.readonly=raw.readonly;return result;
        } catch(...) {syntax.state=3;throw;}
    };
    auto environment=std::make_shared<ir::ValueType>();environment->name="$environment";environment->identity="$void::"+environment->name;environment->managed=true;environment->visibility="private";
    environment->self=environment;environment->arena=arena;arena->definitions.push_back(environment);
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_tasks;
    for(auto& [id,u]:units)u->make_task=[&](Type result) {
        auto eventual=result;result=ir::with_readonly_paths(result,{});
        auto key="$task:"+std::to_string(result.kind)+":"+Type::key(result)+(result.nullable?"?":"")+":"+ir::capability_key(result);
        auto& d=canonical_tasks[key];if(!d){d=std::make_shared<ir::ValueType>();d->identity=d->name=key;d->managed=true;d->visibility="private";d->task_result=ir::FieldType(result);
            d->fields={{"$status",Type::i32,key},{"$result",result==Type::none?Type::boolean:result,key}};d->self=d;d->arena=arena;arena->definitions.push_back(d);root.synthetic_types.emplace_back(d,Type::managed_ref);}
        return ir::with_field_type(Type(d,Type::managed_ref),1,eventual);
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_callables;
    for(auto& [id,u]:units) {
        u->make_callable=[&](const std::vector<Type>& parameters,Type result)->Type {
            std::string key="$func:";for(auto type:parameters){const auto part=ir::array_identity(type,{})+":"+ir::capability_key(type);key+=std::to_string(part.size())+":"+part;}key+="->"+ir::array_identity(result,{})+":"+ir::capability_key(result);
            auto& canonical=canonical_callables[key];
            if(!canonical){if(canonical_callables.size()==1)root.synthetic_types.emplace_back(environment,Type::managed_ref);canonical=std::make_shared<ir::ValueType>();canonical->identity=canonical->name=key;canonical->callable=true;canonical->call_result=ir::FieldType(result);
                canonical->display_name="func(";for(unsigned n=0;n<parameters.size();++n){if(n)canonical->display_name+=", ";canonical->display_name+=ir::type_name(parameters[n]);canonical->call_parameters.emplace_back(parameters[n]);}canonical->display_name+=")";if(result!=Type::none)canonical->display_name+=" -> "+ir::type_name(result);
                Type nullable_environment(environment,Type::managed_ref);nullable_environment.nullable=true;canonical->fields={{"$function",Type::u64,key},{"$environment",nullable_environment,key}};
                canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);root.synthetic_types.emplace_back(canonical);
            }
            return Type(canonical);
        };
        u->resolve_callable=[&,owner=u.get()](Type raw,const Token& token)->Type {
            auto syntax=std::find_if(owner->function_types.begin(),owner->function_types.end(),[&](const auto& value){return value.definition==raw.definition;});
            if(syntax==owner->function_types.end())return raw;
            if(syntax->resolved!=Type::none)return syntax->resolved;
            Depth guard(diagnostics.semantic_depth(),owner->source,token,128);std::vector<Type> parameters;
            for(auto type:syntax->parameters)parameters.push_back(resolve_type(type,*owner,root,units,token));
            return syntax->resolved=owner->make_callable(parameters,resolve_type(syntax->result,*owner,root,units,token));
        };
    }
    std::map<std::string,Type> cleanup_records;
    for(auto& [id,u]:units)u->make_cleanup=[&,owner=u.get()]()->Type{
        if(auto found=cleanup_records.find(owner->identity);found!=cleanup_records.end())return found->second;
        auto definition=std::make_shared<ir::ValueType>();definition->identity=owner->identity+"::$cleanup";definition->name="$cleanup";definition->module_identity=owner->identity;definition->visibility="private";definition->managed=true;definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);Type node(definition,Type::managed_ref);auto next=node;next.nullable=true;
        definition->fields.push_back({"$previous",next,definition->identity});definition->fields.push_back({"$handler",owner->make_callable({},Type::none),definition->identity});definition->fields.push_back({"$active",Type::boolean,definition->identity});owner->synthetic_types.push_back(node);cleanup_records.emplace(owner->identity,node);return node;
    };
    std::map<std::string,Type> capture_records;
    for(auto& [id,u]:units)u->make_capture_record=[&,owner=u.get()](const std::string& identity,Type value,bool cell)->Type {
        if(auto found=capture_records.find(identity);found!=capture_records.end())return found->second;
        auto definition=std::make_shared<ir::ValueType>();definition->identity=identity;definition->module_identity=owner->identity;definition->name=identity.substr(owner->identity.size()+2);definition->managed=true;definition->visibility="private";
        if(cell)definition->fields.push_back({"$value",value,identity});else definition->base=environment;
        definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);Type type(definition,Type::managed_ref);owner->synthetic_types.push_back(type);capture_records.emplace(identity,type);return type;
    };
    std::map<std::string,std::shared_ptr<ir::ValueType>> canonical_results;
    for(auto& [id,u]:units)u->resolve_result=[&,owner=u.get()](Type raw,const Token& token)->Type {
        auto found=std::find_if(owner->results.begin(),owner->results.end(),[&](const auto& syntax){return syntax.definition==raw.definition;});
        if(found==owner->results.end())return raw;
        if(found->resolved!=Type::none)return found->resolved;
        Depth guard(diagnostics.semantic_depth(),owner->source,token,128);
        auto success=resolve_type(found->success,*owner,root,units,token),failure=resolve_type(found->failure,*owner,root,units,token);
        const auto key="$result:"+ir::array_identity(success,{})+":"+ir::array_identity(failure,{});
        auto& canonical=canonical_results[key];
        if(!canonical) {
            canonical=std::make_shared<ir::ValueType>();canonical->identity=canonical->name=key;canonical->result_type=true;
            canonical->display_name="result<"+ir::type_name(success)+", "+ir::type_name(failure)+">";
            canonical->cases={{"Ok",1,1},{"Err",2,1}};
            canonical->fields={{"$tag",Type::i32,key},{"$ok",success,key},{"$err",failure,key}};
            canonical->self=canonical;canonical->arena=arena;arena->definitions.push_back(canonical);
        }
        return found->resolved=Type(canonical);
    };
    layout = [&](Unit& owner, Structure& structure) {
        if (structure.definition->interface_type) {structure.layout_state=2;return;}
        if (structure.layout_state == 2) return;
        if (structure.layout_state == 1) error(owner.source,structure.token,Code::unsupported_type,"Recursive value layout or inheritance for '" + structure.token.text + "'.","Use an acyclic nesting of value types.");
        if (structure.layout_state == 3) throw Poison{};
        Depth guard(diagnostics.semantic_depth(),owner.source,structure.token,128);
        structure.layout_state = 1;
        try {
            std::set<std::string> names;
            if (structure.base_type) {
                auto t=resolve_type(*structure.base_type,owner,root,units,structure.base_token);
                if (!structure.definition->managed || t.kind!=Type::managed_ref || t.nullable)
                    error(owner.source,structure.base_token,Code::unsupported_type,"Only a managed struct may extend a non-nullable managed struct.");
                Structure* parent=nullptr; Unit* parent_owner=nullptr;
                for (auto& [id,u]:units) for (auto& candidate:u->structures) if (candidate.definition==t.definition) { parent=&candidate; parent_owner=u.get(); }
                if (!parent) throw Poison{};
                if ((structure.visibility=="public" && parent->visibility!="public") || (structure.visibility=="internal" && parent->visibility=="private"))
                    error(owner.source,structure.base_token,Code::visibility,"A derived type cannot expose a less-visible base type.",{},{{span(parent_owner->source,parent->token),"Base type declared here."}});
                layout(*parent_owner,*parent);
                if(parent->definition->native_resource)error(owner.source,structure.base_token,Code::unsupported_type,"Native resource identities cannot be extended or forged.");
                structure.base_type=t; structure.definition->base=parent->definition;
                structure.definition->fields=parent->definition->fields;
                structure.effective_fields=parent->effective_fields;
                structure.expanded_fields=parent->expanded_fields;
                structure.layout_depth=parent->layout_depth+1;
                if(structure.layout_depth>128) error(owner.source,structure.base_token,Code::nesting,"Physical layout depth limit exceeded.","Reduce the combined inheritance and inline-value depth; the current limit is 128.");
                for (const auto& field:structure.definition->fields) names.insert(field.name);
            }
            for (auto& field : structure.fields) {
                if (!names.insert(field.name).second) error(owner.source,field.token,Code::duplicate,"Duplicate field '" + field.name + "'.");
                auto t = resolve_type(*field.annotation,owner,root,units,field.token);
                if(field.event){
                    if(!t.definition || !t.definition->callable || t.nullable || Type(*t.definition->call_result)!=Type::none)error(owner.source,field.token,Code::type_mismatch,"An event requires a non-nullable, no-result function signature.");
                    auto definition=std::make_shared<ir::ValueType>();definition->name=definition->identity=ir::array_identity(t,{});definition->element=ir::FieldType(t);definition->self=definition;definition->arena=arena;arena->definitions.push_back(definition);t=Type(definition,Type::growable_array);
                }
                if (field.using_field && ((t.kind!=Type::aggregate && t.kind!=Type::managed_ref) || t.nullable || (t.definition && (t.definition->callable || !t.definition->cases.empty()))))
                    error(owner.source,field.token,Code::unsupported_type,"A using field requires a non-nullable nominal struct type.","Use an ordinary field for primitives or nullable references.");
                std::size_t expanded = 1;
                if(t.kind==Type::fixed_array) {
                    const auto shape=ir::compact_value_shape(t);
                    expanded=shape.template_nodes;structure.layout_depth=std::max(structure.layout_depth,shape.depth+1);
                    if(structure.layout_depth>128) error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.");
                }
                if (t.kind == Type::aggregate) for (auto& [id,u] : units) for (auto& child : u->structures) if (child.definition == t.definition) {
                    layout(*u,child); expanded += child.expanded_fields; structure.layout_depth=std::max(structure.layout_depth,child.layout_depth+1);
                    if(structure.layout_depth>128) error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.","Reduce the combined inheritance and inline-value depth; the current limit is 128.");
                }
                if (expanded > 65536 - structure.expanded_fields) error(owner.source,field.token,Code::nesting,"Expanded value layout limit exceeded.","Reduce nested value duplication; the compiler currently permits 65536 expanded fields per value.");
                structure.expanded_fields += expanded;
                field.annotation = t; structure.definition->fields.push_back({field.name,t,structure.definition->identity,field.using_field,field.event});
                structure.effective_fields.emplace_back(&owner,&field);
            }
            structure.layout_state = 2;
        } catch (...) { structure.layout_state = 3; throw; }
    };
#include "generics.inc"
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        try { layout(*unit,structure); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    prepare_function = [&](Unit& owner,Function& f) {
        auto* unit=&owner;
        if(f.prepared)return;f.prepared=true;
        auto resolve = [&](Type& t, const Token& token) {
            try { t = resolve_type(t,*unit,root,units,token); }
            catch (const CompilationError& failure) { diagnostics.add(failure); t = error_type; f.parse_failed = true; }
            catch (const Poison&) { t = error_type; f.parse_failed = true; }
        };
        resolve(f.result,f.token);
        for (auto& parameter : f.parameters) resolve(parameter.type,parameter.token);
        if (f.receiver) {
            resolve(f.receiver->type,f.receiver->token);
            try {
                auto t = f.receiver->type;
                if (t == error_type) throw Poison{};
                if (((t.kind != Type::aggregate && t.kind != Type::managed_ref) && !(f.interface_requirement && t.kind==Type::interface_ref)) || t.nullable) error(unit->source,f.receiver->token,Code::receiver,"Receiver declarations require a non-nullable struct type.");
                if (!unit->structure_names.contains(t.definition->name) || unit->structure_names.at(t.definition->name)->definition != t.definition)
                    error(unit->source,f.receiver->token,Code::receiver,"Methods must be declared in their value type's owning module; extension methods are deferred.");
                if(t.kind==Type::interface_ref && !f.interface_requirement) error(unit->source,f.token,Code::receiver,"Interfaces declare requirements, not receiver implementations.");
                f.initializer = f.token.text == "init";
                if(f.token.text=="deinit" && (t.kind!=Type::managed_ref || f.result!=Type::none || !f.parameters.empty()))error(unit->source,f.token,Code::initializer_deferred,"deinit requires a managed receiver, no arguments and no result.");
                if (f.initializer && f.result != Type::none) error(unit->source,f.token,Code::type_mismatch,"An initializer must not declare a result; fallible construction is deferred.");
                for (const auto& field : t.definition->fields) if (field.name == f.token.text)
                    error(unit->source,f.token,Code::duplicate,"Method name conflicts with field '" + f.token.text + "'.");
                if(f.interface_requirement) for(const auto& check:[&] {std::vector<Type> types{f.result};for(const auto& p:f.parameters) types.push_back(p.type);return types;}())
                    if(check.definition && ((f.visibility=="public" && check.definition->visibility!="public") || (f.visibility=="internal" && check.definition->visibility=="private")))
                        error(unit->source,f.token,Code::visibility,"An interface requirement cannot expose a less-visible type.");
                f.symbol = t.definition->identity + "::method::" + (f.concrete_method_name.empty()?f.token.text:f.concrete_method_name);
                if (!unit->methods.emplace(f.symbol,&f).second) error(unit->source,f.token,Code::duplicate,"Duplicate receiver method '" + f.token.text + "'.");
                if (f.initializer) unit->structure_names.at(t.definition->name)->initializer = &f;
            } catch (const CompilationError& failure) { diagnostics.add(failure); f.parse_failed = true; }
              catch (const Poison&) { f.parse_failed = true; }
        }
    };
    bool pending_headers=true;
    while(pending_headers) {pending_headers=false;for(auto& [id,u]:units)for(auto& f:u->functions)if(!f.prepared){prepare_function(*u,f);pending_headers=true;}}
    headers_ready=true;
    validate_inheritance(units,root,diagnostics);
    root.member_catalog=std::make_shared<ir::Module>(member_catalog(units,entry));
#include "generic_declarations.inc"
    for(auto& check:pending_generic_constraints)check();
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        try { defaults(*unit,structure); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        if (diagnostics.full()) break;
        try { evaluate(*unit, c); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
        if (f.parse_failed) continue;
        try { Semantics(*unit,units,root,evaluate,diagnostics,defaults).parameter_defaults(f); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
        if (f.parse_failed) continue;
        try { Semantics(*unit,units,root,evaluate,diagnostics,defaults).initializer_fields(f); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    bool pending_bodies=true;
    while(pending_bodies && !diagnostics.full()) {
        pending_bodies=false;
        for(auto& [id,u]:units) {
            for(const auto& f:u->functions)if(!f.analyzed && !f.interface_requirement){pending_bodies=true;break;}
            Semantics(*u,units,root,evaluate,diagnostics,defaults).analyze();
        }
    }
    diagnostics.finish();
    try {root.member_catalog->interface_bindings=ir::interface_universe(*root.member_catalog);}
    catch(const std::length_error& failure) {error(root.source,root.structures.front().token,Code::limit,failure.what(),"Split unrelated providers into separately compiled projects.");}
    std::map<std::string,Function*> functions;
    std::map<std::string,std::set<std::string>> dependencies;
    std::vector<std::string> pending;
    for (auto& [id,u] : units) for (auto& f : u->functions) {
        functions.emplace(f.symbol,&f);
        if (f.initializer) pending.push_back(f.symbol);
        std::vector<const Statement*> statements; for (const auto& s : f.body) statements.push_back(&s);
        while (!statements.empty()) {
            auto* statement=statements.back(); statements.pop_back();
            for (const auto& child : statement->body) statements.push_back(&child);
            for (const auto& child : statement->otherwise) statements.push_back(&child);
            std::vector<const Expr*> expressions; if(statement->index) expressions.push_back(statement->index.get()); if (statement->expression) expressions.push_back(statement->expression.get());
            while (!expressions.empty()) {
                auto* e=expressions.back(); expressions.pop_back();
                for (const auto& part : e->parts) expressions.push_back(part.get());
                for (const auto& part : e->omitted) expressions.push_back(part.get());
                if (e->kind==Expr::Kind::call || e->kind==Expr::Kind::method_call) dependencies[f.symbol].insert(e->target);
                if(!e->virtual_slot.empty() && !e->super_call) for(const auto& [id,owner]:units) for(const auto& target:owner->functions)
                    if(target.virtual_slot==e->virtual_slot) dependencies[f.symbol].insert(target.symbol);
                if (!e->initializer.empty()) dependencies[f.symbol].insert(e->initializer);
            }
        }
    }
    for(auto& [id,u]:units) for(auto& f:u->functions) {
        std::vector<const Expr*> expressions;
        if(f.base_prelude) expressions.push_back(f.base_prelude.get());
        for(const auto& child:f.initialization_defaults) expressions.push_back(child.get());
        while(!expressions.empty()) {
            const auto* e=expressions.back();expressions.pop_back();
            for(const auto& child:e->parts) expressions.push_back(child.get());
            for(const auto& child:e->omitted) expressions.push_back(child.get());
            if(!e->target.empty()) dependencies[f.symbol].insert(e->target);
            if(!e->initializer.empty()) dependencies[f.symbol].insert(e->initializer);
            if(!e->virtual_slot.empty() && !e->super_call) for(const auto& [id,owner]:units) for(const auto& target:owner->functions)
                if(target.virtual_slot==e->virtual_slot) dependencies[f.symbol].insert(target.symbol);
        }
    }
    for(const auto& binding:root.member_catalog->interface_bindings) for(const auto& [requirement,route]:binding.methods) {
        dependencies[requirement].insert(route.second);
        const auto& selected=*functions.at(route.second);
        if(!selected.virtual_slot.empty()) for(const auto& [name,target]:functions) if(target->virtual_slot==selected.virtual_slot) dependencies[requirement].insert(name);
    }
    std::set<std::string> frozen;
    while (!pending.empty()) {
        auto key=pending.back(); pending.pop_back();
        if (!functions.contains(key) || !frozen.insert(key).second) continue;
        auto& f=*functions.at(key); f.initialization_contract=f.body_syntax;
        for (const auto& dependency : dependencies[key]) pending.push_back(dependency);
    }
    for (auto& [id,f] : functions) if (f->initializer) for (const auto& p : f->parameters)
        f->initialization_contract += std::to_string(p.token.text.size()) + ":" + p.token.text + std::to_string(p.default_syntax.size()) + ":" + p.default_syntax;
    // Recovered trees and AST-only poisoned types can never enter capability analysis or IR.
    diagnostics.finish();
    analyze_capabilities(units,diagnostics);
    diagnostics.finish();
    analyze_construction(units,diagnostics);
    diagnostics.finish();
    (void)string_pool(units);
    for(auto& [id,u]:units)for(auto& f:u->functions)if(f.asynchronous){f.task=u->make_task(f.result);if(!ir::async_boundary_safe(f.result))error(u->source,f.token,Code::unsupported_type,"Callable-containing results cannot escape an async task in Delivery 1.");for(const auto& parameter:f.parameters)if(!ir::async_boundary_safe(parameter.type))error(u->source,parameter.token,Code::unsupported_type,"Callable-containing or borrowed arguments cannot cross an async task boundary in Delivery 1.");if(f.receiver && !ir::async_boundary_safe(f.receiver->type))error(u->source,f.token,Code::unsupported_type,"Callable-containing managed receivers cannot cross an async task boundary in Delivery 1.");if(f.initializer || f.accessor || (f.receiver && f.token.text=="deinit") || f.virtual_method || f.overriding || f.interface_requirement || (f.receiver && f.receiver->type.kind!=Type::managed_ref))error(u->source,f.token,Code::unsupported_type,"Async lifecycle methods, accessors, value receivers and virtual/interface methods require a reviewed contract.");}
    for(auto& [id,u]:units) {u->make_task={};u->make_cleanup={};u->make_capture_record={};u->make_callable={};u->resolve_callable={};u->resolve_result={};u->resolve_array={};u->resolve_application={};u->specialize_function={};u->specialize_structure={};} // No closures into this validation frame escape.
}
ir::Module lower_graph(const std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry) {
    ir::Module module;
    auto strings=string_pool(units);
    for(const auto& [id,unit]:units)for(auto type:unit->synthetic_types){module.nominal_types.push_back(type);if(type.kind==Type::managed_ref)module.coordinated=true;}
    for (const auto& [id,unit] : units) for (const auto& structure : unit->structures) {
        module.nominal_types.emplace_back(structure.definition,structure.definition->interface_type ? Type::interface_ref : structure.definition->managed ? Type::managed_ref : Type::aggregate);
        if (structure.definition->managed || structure.definition->interface_type) module.coordinated=true;
    }
    std::set<std::string> results;
    for(const auto& [id,unit]:units)for(const auto& syntax:unit->results)if(syntax.resolved!=Type::none && results.insert(Type::key(syntax.resolved)).second)module.nominal_types.push_back(syntax.resolved);
    module.identity = entry; module.entry_symbol = entry + "::entry";module.root_identity=entry;
    for(const auto& [id,u]:units) {auto& imports=module.source_imports[id];for(const auto& i:u->imports) if(!i.target.empty()) imports.insert(i.target);}
    module.source_path = units.at(entry)->source.path();
    for (auto& [id, unit] : units) {
        Lowering lowering(unit->source,module,strings);
        for (const auto& f : unit->functions) {
            if(!f.native_symbol.empty()){module.native_library="void.fixture.v1";module.coordinated=true;continue;}
            if(f.interface_requirement) {
                ir::Function requirement;requirement.name=f.symbol;requirement.parameters={f.receiver->type};
                for(const auto& p:f.parameters) requirement.parameters.push_back(p.type);
                requirement.result=f.result;requirement.receiver=true;requirement.method_name=f.token.text;requirement.visibility=f.visibility;requirement.module_identity=id;
                module.interface_requirements.push_back(std::move(requirement));continue;
            }
            if (!f.initializer || f.receiver->type.kind!=Type::managed_ref) {
                auto lowered = lowering.lower(f); lowered.module_identity = id;
                if(f.receiver && f.token.text=="deinit"){
                    auto wrapper=lowered;wrapper.locals={lowered.parameters[0]};wrapper.blocks.clear();wrapper.cleanup_entry.reset();wrapper.closure=wrapper.bound_receiver=false;
                    const auto body_suffix="::$body$"+Type::key(lowered.parameters[0]);lowered.name+=body_suffix;lowered.display_name=lowered.source_name;lowered.source_name+=body_suffix;lowered.method_name+=body_suffix;lowered.visibility="private";lowered.virtual_slot.clear();lowered.overriding=false;
                    ir::Instruction load{ir::Op::load,0};load.source=wrapper.source;ir::Instruction release{ir::Op::gc_release,0};release.source=wrapper.source;ir::Instruction call{ir::Op::call};call.callee=lowered.name;call.receiver=ir::Place{0,{}};call.source=wrapper.source;
                    wrapper.blocks={{{load,release},{ir::Terminator::Kind::branch,1,1,2}},{{call},{ir::Terminator::Kind::return_void}},{{},{ir::Terminator::Kind::return_void}}};module.functions.push_back(std::move(wrapper));
                }
                module.functions.push_back(std::move(lowered));
            }
            for (const auto& [mask,context]:f.construction_contexts) {
                auto lowered=lowering.lower(f,&context); lowered.module_identity=id;
                module.functions.push_back(std::move(lowered));
            }
        }
    }
    module.interface_bindings=units.at(entry)->member_catalog->interface_bindings;
    module.strings=std::move(strings.values);
    if(!module.strings.empty()) module.coordinated=true;
    std::function<void(Type)> trace_string=[&](Type t) {
        if(ir::collection_type(t) || ir::reference_paths(t,true)!=ir::reference_paths(t)) module.coordinated=true;
        if(t.kind==Type::aggregate) for(const auto& field:t.definition->fields) trace_string(field.type);
    };
    for(const auto& f:module.functions) { trace_string(f.result); for(auto t:f.parameters) trace_string(t); for(auto t:f.locals) trace_string(t); }
    for(auto t:module.nominal_types) if(t.definition) trace_string(Type(t.definition));
    ir::validate(module); return module;
}

}
std::shared_ptr<const ValidatedAnalysis> validated_source(const SourceFile&);
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path&, const SourceOverrides& = {}, bool verify_disk = false,const NativeProject* = nullptr);
// This factory is the only way to construct a successful result.
struct AnalysisAccess {
    static std::shared_ptr<const ValidatedAnalysis> build(std::map<std::string, std::unique_ptr<Unit>> units, const std::string& entry,std::vector<Diagnostic> warnings={}) {
        auto result = std::shared_ptr<ValidatedAnalysis>(new ValidatedAnalysis);result->warnings_=std::move(warnings);
        result->entry_ = entry;
        auto graph = std::make_shared<const std::map<std::string, std::unique_ptr<Unit>>>(std::move(units));
        auto field = [](std::string& out, const std::string& value) { out += std::to_string(value.size()) + ":" + value; };
        std::map<std::string,std::string> source_identities;
        for(const auto& [id,u]:*graph) {
            for(const auto& f:u->functions)if(!f.generic_origin.empty())source_identities.emplace(f.symbol,f.generic_origin);
            for(const auto& s:u->structures)if(s.generic_origin)source_identities.emplace(s.definition->identity,s.generic_origin->definition->identity);
        }
        auto source_identity=[&](const std::string& key) {
            auto prefix=key;
            while(true) {
                if(auto found=source_identities.find(prefix);found!=source_identities.end())return found->second+key.substr(prefix.size());
                auto end=prefix.rfind("::");if(end==std::string::npos)return key;prefix.resize(end);
            }
        };
        std::set<std::string> visible_declarations;
        std::set<std::tuple<std::string,std::string,std::string,std::size_t,std::size_t,bool>> visible_references;
        auto declaration = [&](AnalyzedDeclaration d,bool native_contract=true,bool visible=true) {
            if (d.owner.empty()) d.owner = d.module;
            if(native_contract) {
            field(result->contract_, "declaration"); field(result->contract_, d.identity); field(result->contract_, d.visibility);
            field(result->contract_, std::to_string(static_cast<int>(d.kind)));
            field(result->contract_, d.type.kind == Type::aggregate ? d.type.definition->identity : ir::type_name(d.type));
            field(result->contract_,ir::capability_key(d.type));
            field(result->contract_, std::to_string(d.parameters.size()));
            for (auto t : d.parameters) {
                field(result->contract_, t.kind == Type::aggregate ? t.definition->identity : ir::type_name(t));
                field(result->contract_,ir::capability_key(t));
            }
            field(result->contract_,d.receiver.kind == Type::none ? "" : ir::type_name(d.receiver));
            field(result->contract_,ir::capability_key(d.receiver));
            field(result->contract_,d.capability_contract);
            field(result->contract_,d.using_field ? "using" : "ordinary-field");
            field(result->contract_,d.base_identity);field(result->contract_,d.virtual_slot);field(result->contract_,d.override_target);
            for (const auto& mask:d.construction_parameters) {
                field(result->contract_,"construction-context");
                for (auto parameter:mask) field(result->contract_,std::to_string(parameter));
            }
            for (const auto& name:d.required_initialization_fields) field(result->contract_,name);
            field(result->contract_,d.receiver_mutates ? "mutating" : "nonmutating");
            field(result->contract_,d.initialization_contract);
            if (d.kind != AnalyzedDeclaration::Kind::function && d.kind != AnalyzedDeclaration::Kind::receiver_method && d.kind!=AnalyzedDeclaration::Kind::interface_requirement && d.kind != AnalyzedDeclaration::Kind::initializer) {
                field(result->constants_, d.identity); field(result->constants_, d.visibility);
                field(result->constants_,d.using_field ? "using" : "ordinary-field");
                field(result->constants_, d.type.kind == Type::aggregate ? d.type.definition->identity : ir::type_name(d.type)); field(result->constants_, d.constant_syntax);
            }
            }
            d.identity=source_identity(d.identity);d.owner=source_identity(d.owner);d.virtual_slot=source_identity(d.virtual_slot);d.override_target=source_identity(d.override_target);
            if(visible && visible_declarations.insert(d.identity).second)result->declarations_.push_back(std::move(d));
        };
        auto reference = [&](const std::string& from, const std::string& to, const Expr& e, const Unit& u, bool implicit = false) {
            auto location = span(u.source,e.token); location.module_identity = u.identity;
            auto spelling = e.qualifier + "::" + e.token.text;
            auto origin=source_identity(from),target=source_identity(to);
            if(visible_references.emplace(origin,target,u.identity,location.begin,location.end,implicit).second)
                result->references_.push_back({origin,target,spelling,std::move(location),implicit});
            field(result->contract_,"reference"); field(result->contract_,from); field(result->contract_,to);
        };
        for (const auto& [id, unit] : *graph) {
            AnalyzedModule module{id,unit->source,{}};
            field(result->contract_,"module"); field(result->contract_,id);
            auto synthetic_types=unit->synthetic_types;std::sort(synthetic_types.begin(),synthetic_types.end(),[](auto a,auto b){return Type::key(a)<Type::key(b);});
            for(auto type:synthetic_types){
                field(result->contract_,"compiler-value-layout");field(result->contract_,Type::key(type));auto base=type.definition->base.lock();field(result->contract_,base?base->identity:"");
                for(const auto& member:type.definition->fields){field(result->contract_,member.name);field(result->contract_,ir::type_name(member.type));field(result->contract_,ir::capability_key(member.type));}
            }
            // Canonical binding records: offsets and written path normalization are irrelevant.
            std::set<std::string> imports;
            for (const auto& i : unit->imports) {
                module.imports.push_back(i.target);
                std::string binding; field(binding,i.target);
                imports.insert(std::move(binding));
            }
            std::sort(module.imports.begin(),module.imports.end());
            module.imports.erase(std::unique(module.imports.begin(),module.imports.end()),module.imports.end());
            field(result->contract_,std::to_string(imports.size()));
            for (const auto& i : imports) field(result->contract_,i);
            result->modules_.push_back(std::move(module));
            auto location = [&](const Token& t) { auto l = span(unit->source,t); l.module_identity=id; return l; };
#include "generic_tooling.inc"
            std::function<void(const Expr&,const std::string&,const std::map<unsigned,std::string>&,bool)> visit_expression;
            auto expression = [&](const Expr& e, const std::string& owner, const std::map<unsigned,std::string>& locals) { visit_expression(e,owner,locals,false); };
            visit_expression = [&](const Expr& e, const std::string& owner, const auto& locals, bool implicit) {
                if(!e.source_target.empty()) {reference(owner,e.source_target,e,*unit);if(!e.target.empty())reference(owner,e.target,e,*unit,true);}
                else if(e.kind==Expr::Kind::conversion && e.op=="interface") {
                    reference(owner,e.type.definition->identity,e,*unit,true);
                    for(const auto& binding:graph->at(entry)->member_catalog->interface_bindings) if(binding.key==e.target)
                        for(const auto& [requirement,route]:binding.methods) {reference(owner,requirement,e,*unit,true);reference(owner,route.second,e,*unit,true);}
                } else if (!e.target.empty() && !e.target.starts_with("$array.") && !e.target.starts_with("$dict.")) reference(owner,e.target,e,*unit,implicit || e.implicit);
                else if (e.kind == Expr::Kind::name && locals.contains(e.slot)) reference(owner,locals.at(e.slot),e,*unit,implicit || e.implicit);
                if(!e.virtual_slot.empty() && !e.super_call) for(const auto& [id,target_unit]:*graph) for(const auto& target:target_unit->functions)
                    if(target.virtual_slot==e.virtual_slot) reference(owner,target.symbol,e,*unit,true);
                if (!e.initializer.empty()) reference(owner,e.initializer,e,*unit,implicit);
                if(!e.setter.empty())reference(owner,e.setter,e,*unit,true);
                if ((e.kind == Expr::Kind::construction || e.kind==Expr::Kind::base_initialization) && e.initializer.empty()) for (unsigned n = 0; n < e.names.size(); ++n) {
                    Expr use(Expr::Kind::name,e.names[n]); reference(owner,e.type.definition->fields.at(e.field_indexes[n]).owner + "::field::" + e.names[n].text,use,*unit);
                }
                std::vector<const Pattern*> patterns;for(const auto& p:e.patterns)patterns.push_back(&p);
                while(!patterns.empty()){auto* p=patterns.back();patterns.pop_back();for(const auto& value:p->values)visit_expression(*value,owner,locals,implicit);for(const auto& child:p->children)patterns.push_back(&child);}
                for (const auto& part : e.parts) visit_expression(*part,owner,locals,implicit);
                for (const auto& part : e.omitted) visit_expression(*part,owner,locals,true);
            };
            for(const auto& array:unit->arrays) if(array.extent && !array.template_syntax) {
                expression(*array.extent,id,{});
                field(result->contract_,"array-extent");field(result->contract_,ir::type_name(array.resolved));
            }
            for (const auto& [token,raw] : unit->type_uses) {
                auto t = resolve_type(raw,*unit,*graph->at(entry),*graph,token);
                // Attribute annotation dependencies to their enclosing declaration,
                // keeping invalidation edges between owned declaration identities.
                std::string owner = id; std::size_t nearest = 0;
                auto consider = [&](const Token& declaration, const std::string& key) {
                    if (declaration.offset <= token.offset && (owner == id || declaration.offset >= nearest)) { nearest = declaration.offset; owner = key; }
                };
                for (const auto& f : unit->functions) consider(f.receiver ? f.receiver->token : f.token,f.symbol);
                for (const auto& c : unit->constants) consider(c.declaration.token,id + "::" + c.declaration.name);
                for (const auto& structure : unit->structures) {
                    consider(structure.token,structure.definition->identity);
                    for (const auto& member : structure.fields) consider(member.token,structure.definition->identity + "::field::" + member.name);
                }
                Expr use(Expr::Kind::name,token); reference(owner,t.definition->identity,use,*unit);
            }
            for (const auto& structure : unit->structures) {
                auto key = structure.definition->identity;
                std::string layout;
                for(const auto& variant:structure.definition->cases){field(layout,"variant");field(layout,variant.name);field(layout,std::to_string(variant.begin));field(layout,std::to_string(variant.count));}
                for (const auto& member : structure.fields) {
                    auto t = *member.annotation; auto identity = key + "::field::" + member.name;
                    field(layout,member.event?"event":member.using_field ? "using" : "ordinary-field"); field(layout,member.name); field(layout,t.kind == Type::aggregate ? t.definition->identity : ir::type_name(t)); field(layout,member.constant_syntax);
                    AnalyzedDeclaration record{structure.definition->managed ? AnalyzedDeclaration::Kind::managed_field : AnalyzedDeclaration::Kind::value_field,identity,id,structure.visibility,location(member.token),{},t,member.constant_syntax,{},key};
                    record.using_field=member.using_field; declaration(std::move(record),true,structure.definition->cases.empty());
                    if (member.expression) expression(*member.expression,identity,{});
                }
                for(unsigned n=0;n<structure.case_tokens.size();++n){const auto& variant=structure.definition->cases[n];std::vector<Type> parameters;for(unsigned field=0;field<variant.count;++field)parameters.push_back(ir::field_type(Type(structure.definition),variant.begin+field));
                    declaration({variant.count?AnalyzedDeclaration::Kind::function:AnalyzedDeclaration::Kind::module_constant,key+"::variant::"+variant.name,id,structure.visibility,location(structure.case_tokens[n]),parameters,Type(structure.definition),std::to_string(n),variant.count?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{n},key});
                }
                AnalyzedDeclaration record{structure.definition->interface_type ? AnalyzedDeclaration::Kind::interface_type : structure.definition->managed ? AnalyzedDeclaration::Kind::managed_structure : AnalyzedDeclaration::Kind::value_structure,key,id,structure.visibility,location(structure.token),{},Type(structure.definition,structure.definition->interface_type ? Type::interface_ref : structure.definition->managed ? Type::managed_ref : Type::aggregate),layout,{},id};
                if(auto base=structure.definition->base.lock()) record.base_identity=base->identity;
                declaration(std::move(record));
            }
            for (const auto& c : unit->constants) {
                auto key = id + "::" + c.declaration.name;
                declaration({AnalyzedDeclaration::Kind::module_constant,key,id,c.visibility,location(c.declaration.token),{},c.type,c.declaration.constant_syntax,(c.type.kind == Type::aggregate || ir::collection_type(c.type) || c.type==Type::string) ? std::optional<std::uint64_t>{} : std::optional<std::uint64_t>{c.value}});
                expression(*c.declaration.expression,key,{});
            }
            for (const auto& f : unit->functions) {
                std::vector<Type> parameters; for (const auto& p : f.parameters) parameters.push_back(p.type);
                AnalyzedDeclaration method_record{f.interface_requirement ? AnalyzedDeclaration::Kind::interface_requirement : f.initializer ? AnalyzedDeclaration::Kind::initializer : f.receiver ? AnalyzedDeclaration::Kind::receiver_method : AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,f.result,{}};
                if (f.receiver) { method_record.owner = f.receiver->type.definition->identity; method_record.receiver = f.receiver->type; method_record.receiver_mutates = f.receiver_mutates; }
                method_record.initialization_contract=f.initialization_contract;
                method_record.virtual_slot=f.virtual_slot;
                if(f.overriding) method_record.override_target=f.virtual_slot;
                method_record.capability_contract=f.capability_contract+(f.asynchronous?";async-effect":"");
                for (const auto& [mask,context]:f.construction_contexts)
                    method_record.construction_parameters.emplace_back(mask.begin(),mask.end());
                if (f.initializer && f.receiver->type.kind==Type::managed_ref)
                    for (const auto& member:f.receiver->type.definition->fields) {
                        Type t=member.type;
                        if (ir::requires_initialization(t)) method_record.required_initialization_fields.push_back(member.name);
                    }
                if (f.initializer) for (const auto& p : f.parameters) {
                    AnalyzedParameter parameter{p.token.text,p.type,location(p.token)};
                    if (p.default_expression) { auto range=location(p.default_expression->token); range.begin=p.default_begin; range.end=p.default_end; parameter.default_source=std::move(range); }
                    method_record.initializer_parameters.push_back(std::move(parameter));
                }
                declaration(std::move(method_record),true,!f.anonymous);
                if(f.overriding) {Expr use(Expr::Kind::name,f.token);reference(f.symbol,f.virtual_slot,use,*unit,true);}
                for (const auto& p : f.parameters) if (p.default_expression) expression(*p.default_expression,f.symbol,{});
                std::map<unsigned,std::string> locals;
                std::map<std::string,unsigned> names;
                std::function<void(const std::vector<Statement>&)> body;
                body = [&](const auto& statements) {
                    for (const auto& s : statements) {
                        auto owner = f.symbol;
                        if (s.readonly) {
                            owner += "::const::" + s.name + "::" + std::to_string(names[s.name]++);
                            declaration({AnalyzedDeclaration::Kind::local_constant,owner,id,"local",location(s.token),{},f.locals.at(s.slot),s.constant_syntax,{},f.symbol});
                        }
                        if (s.kind == Statement::Kind::assignment && !s.fields.empty()) {
                            auto t = f.locals.at(s.slot);
                            for (unsigned n = 0; n < s.fields.size(); ++n) {
                                Expr use(Expr::Kind::name,s.fields[n]); reference(owner,t.definition->fields.at(s.field_indexes[n]).owner + "::field::" + s.fields[n].text,use,*unit,s.implicit_fields[n]);
                                t = t.definition->fields.at(s.field_indexes[n]).type;
                            }
                        }
                        if (s.selection) expression(*s.selection,owner,locals);
                        if (s.range_end) expression(*s.range_end,owner,locals);
                        if (s.index) expression(*s.index,owner,locals);
                        if (s.expression) expression(*s.expression,owner,locals);
                        if(s.pattern){std::vector<const Pattern*> patterns{&*s.pattern};while(!patterns.empty()){auto* p=patterns.back();patterns.pop_back();for(const auto& value:p->values)expression(*value,owner,locals);for(const auto& child:p->children)patterns.push_back(&child);}}
                        if (s.readonly) locals.emplace(s.slot,owner);
                        body(s.body); body(s.otherwise);
                    }
                };
                if(f.base_prelude) expression(*f.base_prelude,f.symbol,locals);
                for(const auto& child:f.initialization_defaults) visit_expression(*child,f.symbol,locals,true);
                body(f.body);
            }
        }
        for(const auto& binding:graph->at(entry)->member_catalog->interface_bindings) {
            field(result->contract_,"interface-binding");field(result->contract_,binding.key);
            for(const auto& [requirement,route]:binding.methods) {
                const auto& owner=*graph->at(binding.interface.definition->module_identity);
                Expr use(Expr::Kind::name,Token{TokenKind::name,binding.interface.definition->name,binding.interface.definition->source_offset});
                reference(requirement,route.second,use,owner,true);
            }
        }
        field(result->contract_,result->constants_);
        result->lower_ = [graph,entry] { return lower_graph(*graph,entry); };
        return result;
    }
};
ir::Module ValidatedAnalysis::lower() const {
    auto module = lower_(); module.constant_contract = constants_; module.analysis = shared_from_this(); return module;
}
AnalysisResult analyze_source(const SourceFile& source) {
    try { auto program=validated_source(source);return {program,program->warnings()}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
AnalysisResult analyze_project(const std::filesystem::path& entry,const NativeProject& bindings) {
    try{validate_native_project(bindings);require_module_identity(bindings.module);auto root=std::filesystem::absolute(entry).parent_path();if(std::filesystem::exists(root/utf8_path(bindings.module).relative_path()))throw std::invalid_argument("Native descriptor module collides with a project source file");SourceOverrides overrides;
        overrides.emplace(bindings.module,SourceFile(path_utf8(root/utf8_path(bindings.module).relative_path()),native_project_source(bindings)));
        auto program=validated_project(entry,overrides,true,&bindings);return {program,program->warnings()};
    }catch(const CompilationError& e){return {nullptr,e.diagnostics()};}
}
AnalysisResult analyze_project(const std::filesystem::path& entry) {
    try { auto program=validated_project(entry);return {program,program->warnings()}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
SourceFile read_source(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw SourceReadError(path_utf8(path), "Cannot read source file.");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw SourceReadError(path_utf8(path), "Cannot finish reading source file.");
    return {path_utf8(path), std::move(text)};
}
std::shared_ptr<const ValidatedAnalysis> validated_source(const SourceFile& source) {
    std::map<std::string, std::unique_ptr<Unit>> units;
    auto identity = source.path().empty() ? std::string("<source>") : source.path();
    auto unit = std::make_unique<Unit>(source, identity);
    Diagnostics diagnostics; diagnostics.bind(source, identity);
    Parser(source, diagnostics).parse(*unit);
    if (!unit->imports.empty()) {
        try { error(source, unit->imports.front().path, Code::import_context, "Imports need a filesystem project context.", "Use compile_project or the check/run/emit CLI with a source filename."); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        unit->failed_import = true;
    }
    units.emplace(identity, std::move(unit));
    validate_graph(units, identity, diagnostics);
    return AnalysisAccess::build(std::move(units), identity,diagnostics.warnings());
}
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path& entry, const SourceOverrides& overrides, bool verify_disk,const NativeProject* bindings) {
    for (const auto& [id, source] : overrides) { (void)source; require_module_identity(id); }
    std::error_code ec;
    auto absolute = std::filesystem::absolute(entry, ec);
    if (ec) error(SourceFile(path_utf8(entry), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the entry source path.");
    auto root = std::filesystem::canonical(absolute.parent_path(), ec);
    if (ec) error(SourceFile(path_utf8(entry), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the project root directory.", "Check that the entry's parent directory exists and is readable.");
    // A diagnostic anchor is needed even before the entry file can be opened.
    Unit anchor(SourceFile(path_utf8(absolute), ""), "/");
    Diagnostics diagnostics; diagnostics.bind(anchor.source, "/" + path_utf8(absolute.filename()));
    SourceGraph graph(root, diagnostics, overrides,bindings);
    auto& first = [&]() -> Unit& {
        try { return graph.load(anchor, {TokenKind::path, "/" + path_utf8(absolute.filename()), 0}); }
        catch (const CompilationError& failure) { diagnostics.add(failure); diagnostics.finish(); throw; }
    }();
    // A disk read is not an atomic filesystem transaction. Re-discover paths and
    // compare exact bytes before accepting this captured graph. Overlays are frozen.
    if (verify_disk) {
        Diagnostics check; SourceGraph again(root, check, overrides,bindings);
        again.load(anchor, {TokenKind::path, first.identity, 0});
        bool same = graph.units.size() == again.units.size();
        for (const auto& [id, unit] : graph.units) {
            auto next = again.units.find(id);
            same &= next != again.units.end() && unit->source.text() == next->second->source.text();
        }
        if (!same) error(anchor.source, {TokenKind::end, "", 0}, Code::import_read,
            "Project sources changed during analysis.", "Retry analysis with a stable filesystem or open document overlays.");
    }
    validate_graph(graph.units, first.identity, diagnostics);
    auto identity = first.identity;
    return AnalysisAccess::build(std::move(graph.units), identity,diagnostics.warnings());
}
void require_module_identity(const std::string& identity) {
    Unit anchor(SourceFile("<document identity>", ""), "/");
    if (identity.empty() || identity[0] != '/' || normalize(anchor, {TokenKind::path, identity, 0}) != identity)
        throw std::invalid_argument("Document identity must be an exact canonical root-relative .void path");
}
AnalysisResult analyze_project(const std::filesystem::path& entry, const SourceOverrides& overrides) {
    try {auto program=validated_project(entry,overrides,true);return {program,program->warnings()};}
    catch (const CompilationError& failure) { return {{}, failure.diagnostics()}; }
}
ir::Module compile_source(const SourceFile& source) {
    auto result = analyze_source(source);
    if (!result.succeeded()) throw CompilationError(std::move(result.diagnostics));
    return result.program->lower();
}
SourceProgram compile_project(const std::filesystem::path& entry) {
    auto result = analyze_project(entry);
    if (!result.succeeded()) throw CompilationError(std::move(result.diagnostics));
    auto module = result.program->lower();
    for (const auto& m : result.program->modules()) if (m.identity == module.identity) return {m.source,std::move(module)};
    throw std::logic_error("Validated entry absent");
}
namespace {
void entry_signature(const SourceFile& source,const std::string& identity,bool found,bool valid,std::size_t offset) {
    Diagnostics diagnostics;diagnostics.bind(source,identity);
    try {
        if(valid)return;
        if(found)error(source,{TokenKind::name,"entry",offset},Code::entry,"The CLI entry point must have signature entry(value: i32) -> i32.","Other functions can have any supported parameter list; choose an i32 adapter for the CLI.");
        error(source,{TokenKind::end,"",0},Code::entry,"No entry function found.","Define func entry(value: i32) -> i32 to run or emit this file; check accepts libraries without an entry.");
    } catch(const CompilationError& failure){diagnostics.add(failure);diagnostics.finish();}
}
}
void require_source_entry(const ValidatedAnalysis& analysis) {
    for(const auto& module:analysis.modules())if(module.identity==analysis.entry_identity()) {
        for(const auto& declaration:analysis.declarations())if(declaration.identity==module.identity+"::entry" && declaration.kind==AnalyzedDeclaration::Kind::function && !declaration.generic_template) {
            entry_signature(module.source,module.identity,true,declaration.parameters==std::vector<Type>{Type::i32} && declaration.type==Type::i32,declaration.source.begin);return;
        }
        entry_signature(module.source,module.identity,false,false,0);return;
    }
    throw std::logic_error("Validated entry absent");
}
void require_source_entry(const SourceFile& source, const ir::Module& module) {
    for(const auto& f:module.functions)if(f.name==module.entry_symbol) {
        std::size_t offset=0;
        while(offset<source.text().size() && source.location(offset).line<f.source.line)++offset;
        while(offset<source.text().size() && source.location(offset).column<f.source.column)++offset;
        entry_signature(source,module.identity,true,f.parameters==std::vector<Type>{Type::i32} && f.result==Type::i32,offset);return;
    }
    entry_signature(source,module.identity,false,false,0);
}
}

#undef VOID_SEMANTIC_HANDLER
