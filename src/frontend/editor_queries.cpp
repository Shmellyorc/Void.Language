#include "diagnostics.hpp"
#include "model.hpp"
#include "parser.hpp"
#include "void/compiler_observation.hpp"
#include <algorithm>
#include <iterator>

namespace vd {

bool valid_editor_identifier(const std::string& name) {
    using namespace frontend;
    return !name.empty() && letter(name[0]) && !reserved.contains(name) &&
        std::all_of(name.begin(),name.end(),[](char c){return letter(c)||digit(c);});
}
std::optional<EditorAssistance> editor_namespace_assistance(const ValidatedAnalysis& known,const SourceFile& source,
    const std::string& module,std::size_t cursor) {
    using namespace frontend;
    const auto found=std::find_if(known.modules().begin(),known.modules().end(),[&](const auto& m){return m.identity==module;});
    if(found==known.modules().end() || found->qualifiers.empty() || cursor>source.text().size())return {};
    const auto& text=source.text();
    const auto line=source.location(cursor).line-1;
    const auto start=source.line_starts().at(line);
    const auto newline=text.find_first_of("\r\n",cursor);
    const auto stop=newline==std::string::npos?text.size():newline;
    if(!cursor || text.find("_void_editor_cursor_")!=std::string::npos)return {};
    const auto dot=text.rfind('.',cursor-1);
    if(dot==std::string::npos || dot<start)return {};
    auto end=dot+1;
    while(end<text.size() && (letter(text[end]) || digit(text[end])))++end;
    if(cursor<dot+1 || cursor>end)return {};
    const auto name=std::string_view(text).substr(dot+1,end-dot-1);
    // Use the ordinary lexer and parser to classify this exact selection. A
    // name in a string/comment, type annotation, import or chained value is not
    // a namespace expression merely because its text contains an alias and dot.
    observation::Scope syntax_timing(observation::Phase::lex_parse);
    Diagnostics diagnostics;
    EditorRecovery recovery{module,{}};
    const auto lexed=lex(source,diagnostics,false,&recovery,&found->conditions);
    if(diagnostics.failed())return {};
    auto physical=std::find_if(lexed.begin(),lexed.end(),[&](const auto& token) {
        return token.offset==dot && token.kind==TokenKind::symbol && token.text==".";
    });
    if(physical==lexed.end() || physical==lexed.begin() || std::prev(physical)->kind!=TokenKind::name)return {};
    const auto receiver=std::prev(physical)->text;
    auto target=found->qualifiers.find(receiver);
    if(target==found->qualifiers.end())return {};
    for(const auto& declaration:known.declarations())if(declaration.module==module &&
        declaration.kind!=AnalyzedDeclaration::Kind::import_alias && declaration.source.source &&
        declaration.source.source->text().substr(declaration.source.begin,declaration.source.end-declaration.source.begin)==receiver)
        return {}; // Conservative delegation for any possible local shadow.
    auto repaired=text;
    repaired.replace(dot+1,end-dot-1,"_void_editor_cursor_");
    int parentheses=0;
    for(const auto& token:lexed)if(token.offset>=start && token.offset<stop) {
        if(token.text=="(")++parentheses;else if(token.text==")")--parentheses;
    }
    if(parentheses<0 || parentheses>32)return {};
    if(parentheses)repaired.insert(stop+std::string_view("_void_editor_cursor_").size()-(end-dot-1),static_cast<std::size_t>(parentheses),')');
    SourceFile parsed_source(source.path(),std::move(repaired));
    Unit parsed(parsed_source,module);
    Diagnostics syntax;
    std::vector<Token> syntax_tokens;
    parse_unit(parsed,syntax,module,found->conditions,&syntax_tokens);
    if(syntax.failed())return {};
    // Body edits do not change an imported namespace's declarations. Compare
    // actual compiler tokens outside ordinary function bodies, not timestamps
    // or an ad-hoc import scanner. New imports, types, globals, signatures and
    // entry declarations delegate to the ordinary project probe.
    Unit previous(found->source,module);Diagnostics previous_syntax;
    std::vector<Token> previous_tokens;
    parse_unit(previous,previous_syntax,module,found->conditions,&previous_tokens);
    if(previous_syntax.failed())return {};
    auto headers=[](const SourceFile& source,const std::vector<Token>& tokens,const Unit& unit) {
        std::vector<std::pair<std::size_t,std::size_t>> bodies;
        for(const auto* functions:{&unit.functions,&unit.generic_functions})for(const auto& function:*functions) {
            if(function.body.empty() || function.anonymous || function.global_initializer || function.token.text.starts_with("$"))continue;
            const auto begin=source.line_starts().at(source.location(function.body.front().token.offset).line-1);
            if(function.source_end>begin)bodies.emplace_back(begin,function.source_end);
        }
        std::sort(bodies.begin(),bodies.end());
        std::vector<std::pair<std::size_t,std::size_t>> ranges;
        for(const auto& body:bodies) {
            if(!ranges.empty() && body.first<ranges.back().second)ranges.back().second=std::max(ranges.back().second,body.second);
            else ranges.push_back(body);
        }
        std::vector<std::pair<TokenKind,std::string>> result;
        for(const auto& token:tokens) {
            const auto next=std::upper_bound(ranges.begin(),ranges.end(),token.offset,[](auto offset,const auto& span){return offset<span.first;});
            const bool body=next!=ranges.begin() && (token.offset<std::prev(next)->second ||
                // The lexer supplies a newline at EOF for an unterminated last
                // line. Like its closing dedent, that token belongs to the body;
                // it must not turn an ordinary body edit into a header change.
                ((token.kind==TokenKind::dedent || token.kind==TokenKind::newline) && token.offset==std::prev(next)->second));
            if(!body)result.emplace_back(token.kind,token.text);
        }
        return result;
    };
    if(headers(parsed_source,syntax_tokens,parsed)!=headers(found->source,previous_tokens,previous))return {};
    bool expression=false;
    for(const auto& function:parsed.functions) {
        if(function.receiver && function.receiver->token.text==receiver)return {};
        for(const auto& parameter:function.parameters)if(parameter.token.text==receiver)return {};
        std::vector<const Statement*> statements;
        std::vector<const Expr*> expressions;
        for(const auto& statement:function.body)statements.push_back(&statement);
        while(!statements.empty()) {
            const auto& statement=*statements.back();statements.pop_back();
            if(statement.name==receiver || statement.binder.text==receiver || statement.index_binder.text==receiver)return {};
            for(const auto& binding:statement.bindings)if(binding.token.text==receiver)return {};
            for(const auto& field:statement.fields)if(field.text==receiver)return {};
            if(statement.pattern) {
                std::vector<const Pattern*> pending{&*statement.pattern};
                while(!pending.empty()) {
                    const auto& pattern=*pending.back();pending.pop_back();
                    if(pattern.kind==Pattern::Kind::capture && pattern.token.text==receiver)return {};
                    for(const auto& child:pattern.children)pending.push_back(&child);
                }
            }
            for(const auto* part:{statement.expression.get(),statement.index.get(),statement.selection.get(),statement.range_end.get()})if(part)expressions.push_back(part);
            for(const auto* body:{&statement.body,&statement.otherwise})for(const auto& child:*body)statements.push_back(&child);
        }
        while(!expressions.empty()) {
            const auto& part=*expressions.back();expressions.pop_back();
            if(part.token.text=="_void_editor_cursor_" && (part.kind==Expr::Kind::field || part.kind==Expr::Kind::method_call) &&
                !part.parts.empty() && part.parts.front()->kind==Expr::Kind::name && part.parts.front()->token.text==receiver)expression=true;
            std::vector<const Pattern*> patterns;for(const auto& pattern:part.patterns)patterns.push_back(&pattern);
            while(!patterns.empty()) {
                const auto& pattern=*patterns.back();patterns.pop_back();
                if(pattern.kind==Pattern::Kind::capture && pattern.token.text==receiver)return {};
                for(const auto& child:pattern.children)patterns.push_back(&child);
            }
            for(const auto& child:part.parts)expressions.push_back(child.get());
        }
    }
    if(!expression)return {};
    syntax_timing.finish();
    observation::Scope candidates_timing(observation::EditorDetail::candidates);
    EditorAssistance out;out.begin=dot+1;out.end=end;
    const auto prefix=name.substr(0,cursor-out.begin);
    std::map<std::string,EditorCandidate> candidates;
    auto consider=[&](const AnalyzedDeclaration& d) {
        if(d.static_member || d.module!=target->second || !source_visible(d.visibility,d.module,module) ||
           !d.source.source || d.source.end<=d.source.begin)return;
        const auto label=d.source.source->text().substr(d.source.begin,d.source.end-d.source.begin);
        if(!label.starts_with(prefix) || !valid_editor_identifier(label))return;
        using K=AnalyzedDeclaration::Kind;
        int kind;
        std::string detail;
        switch(d.kind) {
        case K::function: kind=3;break;
        case K::global_variable: kind=6;break;
        case K::module_constant: if(d.editor_symbol_kind==22)return;kind=21;break;
        case K::value_structure: case K::managed_structure: case K::interface_type:
            kind=7;detail=d.generic_template?"generic type":"type";break;
        default:return;
        }
        if(d.builtin)return;
        if(detail.empty())detail=!d.signature.empty() && d.kind==K::function?d.signature:ir::type_name(d.type);
        candidates.try_emplace(label,EditorCandidate{label,std::move(detail),kind,{},
            d.documentation.text(d.kind==K::function && d.type!=Type::none)});
    };
    for(const auto& d:known.declarations())consider(d);
    for(const auto& d:known.editor_declarations())consider(d);
    for(auto& [label,candidate]:candidates) {
        if(out.candidates.size()==2048){out.incomplete=true;break;}
        out.candidates.push_back(std::move(candidate));
    }
    return out;
}

std::optional<std::string> format_source(const SourceFile& source, bool insert_spaces) {
    using namespace frontend;
    // Parse with the ordinary compiler before proposing edits. This is local
    // syntax validation, never import discovery or a second semantic resolver.
    Diagnostics diagnostics;
    Unit parsed(source,source.path());
    std::vector<Token> parsed_tokens;
    parse_unit(parsed,diagnostics,{},native_build_conditions(),&parsed_tokens);
    if(diagnostics.failed() || !parsed.conditional_regions.empty())return {};
    auto tokens=lex(source,diagnostics,false);
    if(diagnostics.failed())return {};
    const auto& text=source.text();
    struct Edit {std::size_t begin,end;std::string text;};std::vector<Edit> edits;
    unsigned depth=0,brace_depth=0,parentheses=0;std::vector<bool> braces;std::set<std::size_t> lines;
    const Token* previous=nullptr;
    for(const auto& t:tokens) {
        if(t.kind==TokenKind::indent){++depth;continue;}
        if(t.kind==TokenKind::dedent){if(depth)--depth;continue;}
        if(t.kind==TokenKind::newline || t.kind==TokenKind::end)continue;
        if(t.text=="}" && !braces.empty()){if(braces.back() && brace_depth)--brace_depth;braces.pop_back();}
        auto r=span(source,t);
        auto start=text.find_last_of("\r\n",t.offset ? t.offset-1 : 0);start=start==std::string::npos?0:start+1;
        if(lines.insert(start).second) {
            auto indent=start;if(!start && text.starts_with("\xef\xbb\xbf"))indent=3;
            const auto end=indentation_at(text,indent).end;
            if(end==t.offset && !parentheses)edits.push_back({indent,end,std::string((depth+brace_depth)*(insert_spaces?4:1),insert_spaces?' ':'\t')});
        }
        if(previous) {
            auto left=span(source,*previous);
            if(left.end<=r.begin){auto gap=text.substr(left.end,r.begin-left.end);
                if(gap.find_first_not_of(' ')==std::string::npos) {
                    std::string replacement=gap.empty()?"":" ";
                    if(t.text=="," || t.text==":" || t.text==")" || t.text=="]" || t.text=="." || previous->text=="." || previous->text=="(" || previous->text=="[")replacement="";
                    else if(previous->text=="," || previous->text==":" || t.text=="=" || previous->text=="=")replacement=" ";
                    edits.push_back({left.end,r.begin,replacement});
                }
            }
        }
        if(t.text=="{"){bool literal=!previous || previous->text!="defer";braces.push_back(literal);if(literal)++brace_depth;}
        if(t.text=="(")++parentheses;else if(t.text==")" && parentheses)--parentheses;
        previous=&t;
    }
    // Retokenization proves the same structural indentation, including literal
    // blocks. Ambiguous/unsupported relationships produce no edit.
    auto result=text;for(auto it=edits.rbegin();it!=edits.rend();++it)result.replace(it->begin,it->end-it->begin,it->text);
    Diagnostics check;auto after=lex(SourceFile(source.path(),result),check,false);
    auto same_tokens=[](const auto& before,const auto& after) {
        if(before.size()!=after.size())return false;
        for(std::size_t n=0;n<before.size();++n)if(before[n].kind!=after[n].kind || before[n].text!=after[n].text || before[n].decoded!=after[n].decoded)return false;
        return true;
    };
    if(check.failed() || !same_tokens(tokens,after))return {};
    // Literal bytes alone are insufficient: reindenting the physical opening
    // line can change indentation-sensitive bodies inside an unchanged string.
    // Prove the parser sees the identical expanded stream as well.
    auto expanded=lex(SourceFile(source.path(),result),check);
    if(check.failed() || !same_tokens(parsed_tokens,expanded))return {};
    return result;
}
}
