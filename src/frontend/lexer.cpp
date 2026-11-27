#include "lexer.hpp"
#include "diagnostics.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace vd::frontend {
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
                        const std::string& hint, std::vector<RelatedLocation> related,
                        std::string expected, std::string received) {
    Diagnostic d{code}; d.description = message; d.primary = span(source, token);
    d.hint = hint; d.related = std::move(related);
    d.expected_type = std::move(expected); d.received_type = std::move(received);
    throw CompilationError({std::move(d)});
}

const std::set<std::string> reserved = {"compile", "func", "var", "return", "if", "else", "for", "break", "continue", "pass",
    "static", "operator", "async", "await", "spawn", "event", "defer", "with", "enum", "has", "is", "match", "property", "and", "or", "not", "true", "false", "null", "while", "const", "use", "struct", "in", "reverse", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "string", "as", "private", "public", "internal"};

Indentation indentation_at(std::string_view text, std::size_t start) {
    Indentation result{start};
    while (result.end < text.size()) {
        const char c = text[result.end];
        if (c != ' ' && c != '\t') break;
        if (!result.style) result.style = c;
        if (c != result.style && result.mixed == std::string::npos) result.mixed = result.end;
        result.width += c == '\t' ? 4 : 1;
        ++result.end;
    }
    return result;
}

namespace {
#include "strings.inc"
#include "conditional.inc"
std::vector<Token> lex_range(const SourceFile& source, Diagnostics& diagnostics, std::size_t begin, std::size_t end, unsigned interpolation_depth, bool expand_interpolation=true, EditorRecovery* recovery=nullptr, const BuildConditions* configuration=nullptr, std::vector<ConditionalRegion>* regions=nullptr,
                             std::vector<std::pair<std::size_t, std::size_t>>* standalone_comments=nullptr) {
    const auto& text = source.text();
    ConditionalSelection selection(source,diagnostics,configuration?*configuration:native_build_conditions(),regions);
    std::vector<Token> tokens;
    struct IndentLevel {std::size_t width; char style;};
    std::vector<IndentLevel> indents{{0, 0}};
    std::size_t p = begin ? begin : text.starts_with("\xef\xbb\xbf") ? 3 : 0;
    end=std::min(end,text.size());
    unsigned parentheses=0;
    struct HeaderDelimiter {unsigned parentheses,braces;std::size_t indent;};
    std::optional<HeaderDelimiter> header_delimiter;
    bool beginning = !begin;unsigned braces=interpolation_depth?1:0;std::size_t line_tokens=0;
    struct LiteralBlock {unsigned braces;std::size_t parent,indent;bool conditional;};std::vector<LiteralBlock> literal_blocks;
    auto emit = [&](TokenKind kind, std::string value, std::size_t offset) { tokens.push_back({kind, std::move(value), offset}); };
    while (p < end) {
        if (diagnostics.full()) break;
        try {
        if (beginning) {
            const auto start = p;
            auto prefix = indentation_at(text, start);
            p = prefix.end;
            if(header_delimiter && prefix.width<=header_delimiter->indent) {
                auto last=p;while(last<end && (letter(text[last]) || digit(text[last])))++last;
                const auto word=std::string_view(text).substr(p,last-p);
                auto next=last;while(next<end && (text[next]==' ' || text[next]=='\t'))++next;
                const bool value_declaration=word=="value" && text.compare(next,6,"struct")==0;
                if(word=="func" || word=="property" || word=="interface" || word=="struct" || value_declaration ||
                   word=="use" || word=="public" || word=="private" || word=="internal" || word=="async" ||
                   word=="virtual" || word=="override" || word=="compile") {
                    // An unclosed declaration must not swallow later declarations.
                    parentheses=header_delimiter->parentheses;braces=header_delimiter->braces;header_delimiter.reset();
                    if(!tokens.empty() && tokens.back().kind!=TokenKind::newline)emit(TokenKind::newline,"<newline>",start);
                    line_tokens=tokens.size();
                }
            }
            if(!begin && !selection.line(start,prefix,braces!=0)) {
                while(p<end && text[p]!='\r' && text[p]!='\n')++p;
                if(p<end && text[p++]=='\r' && p<end && text[p]=='\n')++p;
                continue;
            }
            const bool blank = p == text.size() || text[p] == '#' || text[p] == '\r' || text[p] == '\n';
            if (standalone_comments && p < end &&
                (text.compare(p, 10, "# section ") == 0 || text.compare(p, 12, "# endsection") == 0)) {
                const auto newline = text.find_first_of("\r\n", p);
                standalone_comments->emplace_back(p, std::min(newline, end));
            }
            if (!blank && !braces && !literal_blocks.empty()) {
                const auto block = literal_blocks.back();
                const bool alternative = block.conditional && prefix.width == block.indent && text.compare(p, 4, "else") == 0;
                if (prefix.width <= block.indent && !alternative) {
                    literal_blocks.pop_back();
                    while (indents.size() > block.parent + 1) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
                    indents.pop_back();
                    braces = block.braces;
                }
            }
            if (!blank && !braces) {
                if (prefix.mixed != std::string::npos)
                    error(source, {TokenKind::symbol, "", prefix.mixed}, Code::indentation, "Mixed tabs and spaces in source indentation.", "Use only tabs or only spaces in this block.");
                const bool dedenting = prefix.width < indents.back().width;
                while (prefix.width < indents.back().width) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
                const auto enclosing = indents.back();
                if (dedenting && prefix.width != enclosing.width)
                    error(source, {TokenKind::symbol, "", p}, Code::indentation, "Indentation does not match an enclosing block.", "Align this statement with an earlier block level.");
                const bool different_style=enclosing.width && prefix.style!=enclosing.style;
                if (different_style) {
                    // The block is still unambiguous at an existing column. Keep
                    // the original bytes and the strict diagnostic on the normal pass.
                    if(recovery && recovery->available() && (prefix.width==enclosing.width || recovery->presentation))
                        recovery->record();
                    else error(source, {TokenKind::symbol, "", start}, Code::indentation, "Inconsistent tabs and spaces in an enclosing block.", "Match the indentation style of the enclosing block.");
                }
                if (prefix.width > enclosing.width) {
                    indents.push_back({prefix.width, different_style ? enclosing.style : prefix.style});
                    emit(TokenKind::indent, "<indent>", p);
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
            if(braces && tokens.size()>line_tokens && tokens.back().text==":"){
                for(auto n=line_tokens;n<tokens.size();++n)if(tokens[n].text=="match" || tokens[n].text=="if" || (tokens[n].text=="func" && n+1<tokens.size() && tokens[n+1].text=="(")){
                    auto line=tokens[n].offset;while(line && text[line-1]!='\n' && text[line-1]!='\r')--line;
                    auto prefix=indentation_at(text,line);prefix.width=selection.width(prefix.width);
                    if(prefix.mixed!=std::string::npos || (prefix.width && indents.back().width && prefix.style!=indents.back().style))
                        error(source,{TokenKind::symbol,"",prefix.mixed==std::string::npos?line:prefix.mixed},Code::indentation,"Inconsistent tabs and spaces in an enclosing block.","Match the indentation style of the enclosing block.");
                    literal_blocks.push_back({braces,indents.size(),prefix.width,tokens[n].text=="if"});indents.push_back({prefix.width,prefix.style});braces=0;break;
                }
            }
            ++p; if (c == '\r' && p < text.size() && text[p] == '\n') ++p;
            if (!braces && !tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", start);
            line_tokens=tokens.size();beginning = true; continue;
        }
        if (c == '"') {
            if (tokens.empty() || tokens.back().text!="use") {
                if(interpolation_depth>=64)error(source,{TokenKind::symbol,"",p},Code::nesting,"String interpolation nesting limit exceeded.");
                std::vector<InterpolationHole> substitutions;std::vector<Token> chunks;
                auto token=string_token(source,p,&substitutions,&chunks);
                if(substitutions.empty() || !expand_interpolation)tokens.push_back(std::move(token));
                else {
                    auto internal=[&](std::string name,std::size_t at){Token t{TokenKind::name,std::move(name),at};t.source_width=0;tokens.push_back(std::move(t));};
                    internal("$interpolation",token.offset);emit(TokenKind::symbol,"(",token.offset);
                    for(std::size_t n=0;n<chunks.size();++n){
                        if(n)emit(TokenKind::symbol,",",chunks[n].offset);
                        tokens.push_back(std::move(chunks[n]));
                        if(n<substitutions.size()){
                            const auto hole=substitutions[n];emit(TokenKind::symbol,",",hole.begin);
                            internal("$hole",hole.begin);emit(TokenKind::symbol,"(",hole.begin);emit(TokenKind::symbol,"(",hole.begin);
                            auto embedded=lex_range(source,diagnostics,hole.begin,hole.end,interpolation_depth+1);
                            // Keep statement-body indentation/newlines, dropping only
                            // the artificial expression-range terminator newline.
                            if(embedded.size()>=2 && embedded[embedded.size()-2].kind==TokenKind::newline)embedded.erase(embedded.end()-2);
                            for(auto& t:embedded)if(t.kind!=TokenKind::end)tokens.push_back(std::move(t));
                            emit(TokenKind::symbol,")",hole.end);emit(TokenKind::symbol,",",hole.spec_begin);
                            Token spec{TokenKind::string,"\""+text.substr(hole.spec_begin,hole.spec_end-hole.spec_begin)+"\"",hole.spec_begin,text.substr(hole.spec_begin,hole.spec_end-hole.spec_begin)};
                            spec.source_width=hole.spec_end-hole.spec_begin;spec.format_spec=hole.spec_begin>hole.begin && text[hole.spec_begin-1]==':';tokens.push_back(std::move(spec));emit(TokenKind::symbol,")",hole.spec_end);
                        }
                    }
                    emit(TokenKind::symbol,")",p-1);
                }
                continue;
            }
            ++p;
            while (p < text.size() && text[p] != '"' && text[p] != '\r' && text[p] != '\n') ++p;
            if (p == text.size() || text[p] != '"') error(source, {TokenKind::path, "", start}, Code::import_quote, "Unterminated import path.", "Close the path with a double quote on this line.");
            emit(TokenKind::path, text.substr(start + 1, p - start - 1), start); ++p; continue;
        }
        if(c=='O' || c=='E' || c=='S' || c=='N') {
            const std::string_view names[]={"Ok","Err","Some","None"};
            auto spelling=std::find_if(std::begin(names),std::end(names),[&](auto name){return std::string_view(text).substr(p).starts_with(name);});
            if(spelling!=std::end(names)) {
                const auto width=spelling->size();
                if(p+width<text.size() && (letter(text[p+width]) || digit(text[p+width])))
                    error(source,{TokenKind::symbol,"",p},Code::identifier,"Only Ok, Err, Some and None use uppercase spelling.");
                p+=width;emit(TokenKind::name,std::string(*spelling),start);continue;
            }
        }
        if (letter(c)) {
            while (p < text.size() && (letter(text[p]) || digit(text[p]))) ++p;
            if(p-start==2 && text.compare(start,2,"as")==0 && p<end && text[p]=='?') {
                ++p;emit(TokenKind::symbol,"as?",start);continue;
            }
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
        if(text.substr(p,3)=="<<=" || text.substr(p,3)==">>=") {
            p+=3;emit(TokenKind::symbol,text.substr(start,3),start);continue;
        }
        std::string pair = text.substr(p, 2);
        if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" || pair == "+=" ||
            pair == "&=" || pair == "|=" || pair == "^=" || pair == "-=" || pair == "*=" || pair == "/=" || pair == "%=" || pair == "++" || pair == "--") {
            p += 2; emit(TokenKind::symbol, pair, start); continue;
        }
        if (std::string("{}[]():,.+-*/%=<>?&|^~").find(c) != std::string::npos) { if(c=='('){
                // Parameter lists and function types use the same continuation
                // formatting as calls. The closing ')' restores block layout.
                auto first=line_tokens;
                while(first<tokens.size() && (tokens[first].kind==TokenKind::indent || tokens[first].kind==TokenKind::dedent || tokens[first].text=="public" || tokens[first].text=="private" || tokens[first].text=="internal" || tokens[first].text=="virtual" || tokens[first].text=="override" || tokens[first].text=="async" || tokens[first].text=="compile"))++first;
                if(!header_delimiter && first<tokens.size() && (tokens[first].text=="func" || tokens[first].text=="property" || tokens[first].text=="interface" || tokens[first].text=="operator")) {
                    auto line=start;while(line && text[line-1]!='\n' && text[line-1]!='\r')--line;
                    header_delimiter=HeaderDelimiter{parentheses,braces,indentation_at(text,line).width};
                }
                ++parentheses;++braces;
            }else if(c==')' && parentheses){if(header_delimiter && parentheses==header_delimiter->parentheses+1)header_delimiter.reset();if(braces)--braces;--parentheses;}
            else if(c=='{' && (tokens.empty() || tokens.back().text!="defer"))++braces;else if(c=='}' && braces)--braces;++p; emit(TokenKind::symbol, std::string(1, c), start); continue; }
        error(source, {TokenKind::symbol, "", p}, Code::character, "Unexpected character in source.",
              c == '\t' ? "Use spaces between tokens. Tabs are only allowed in indentation or multiline strings." : "Use supported expressions and lowercase ASCII identifiers; Unicode identifiers and collection/indexing syntax are deferred.");
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
    if(!begin)selection.finish();
    if (!tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", p);
    while (indents.size() > 1) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
    emit(TokenKind::end, "<end of file>", p);
    return tokens;
}
}

std::vector<Token> lex(const SourceFile& source, Diagnostics& diagnostics, bool expand_interpolation,
                       EditorRecovery* recovery, const BuildConditions* configuration,
                       std::vector<ConditionalRegion>* regions) {
    return lex_range(source, diagnostics, 0, std::string::npos, 0, expand_interpolation, recovery,
                     configuration, regions);
}
}

namespace vd {
std::vector<SourceSection> source_sections(const SourceFile& source,const BuildConditions* conditions,
                                           std::vector<std::pair<unsigned,unsigned>>* blocks) {
    const std::string_view text=source.text();
    if(text.find("# section ")==std::string_view::npos || text.find("# endsection")==std::string_view::npos)return {};
    frontend::Diagnostics diagnostics;
    std::vector<std::pair<std::size_t, std::size_t>> comments;
    // Reuse lexical comment boundaries, including multiline strings and BOM.
    const auto tokens=frontend::lex_range(source, diagnostics, 0, std::string::npos, 0, false, nullptr,
                                          conditions, nullptr, &comments);

    std::vector<SourceSection> pending, sections;
    for (const auto& [begin, end] : comments) {
        auto marker = text.substr(begin, end - begin);
        marker.remove_suffix(marker.size() - marker.find_last_not_of(" \t") - 1);
        if (marker == "# endsection") {
            if (pending.empty()) continue;
            auto section = pending.back();
            pending.pop_back();
            section.end = begin + marker.size();
            sections.push_back(section);
        } else if (marker.starts_with("# section ")) {
            const auto name = marker.find_first_not_of(" \t", 10);
            if (name != std::string_view::npos)
                pending.push_back({begin, begin + name, begin + marker.size(), 0});
        }
    }
    std::sort(sections.begin(), sections.end(), [](const auto& left, const auto& right) {
        return left.begin < right.begin;
    });
    // Clients switch from indentation fallback to language-server folding
    // when explicit ranges arrive. Keep the lexer-indented bodies in that view.
    if(blocks && !sections.empty()) {
        using K=frontend::TokenKind;
        std::vector<unsigned> starts;
        std::size_t last=0;
        for(const auto& token:tokens) {
            if(token.kind==K::indent)starts.push_back(source.location(last).line-1);
            else if(token.kind==K::dedent && !starts.empty()) {
                const auto start=starts.back(),end=source.location(last).line-1;
                starts.pop_back();
                if(start<end)blocks->emplace_back(start,end);
            } else if(token.kind!=K::newline && token.kind!=K::end) {
                const auto width=token.source_width.value_or(token.text.size());
                last=token.kind==K::invalid || !width?token.offset:
                    std::min(source.text().size(),token.offset+width-1);
            }
        }
    }
    return sections;
}

}
