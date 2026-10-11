#include "void/frontend.hpp"
#include "void/numeric.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <functional>
#include <bit>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
namespace vd {
namespace {
using Type = ir::Type;
enum class TokenKind { invalid, name, number, path, symbol, newline, indent, dedent, end };
struct Token { TokenKind kind; std::string text; std::size_t offset; };
using Code = DiagnosticCode;
SourceSpan span(const SourceFile& source, const Token& token) {
    auto begin = std::min(token.offset, source.text().size());
    auto length = token.kind == TokenKind::path ? token.text.size() + 2 :
        (token.kind == TokenKind::name || token.kind == TokenKind::number || token.kind == TokenKind::symbol) ? token.text.size() : 0;
    if (token.text.empty() && begin < source.text().size()) {
        if (token.kind == TokenKind::symbol && source.text()[begin] != '\r' && source.text()[begin] != '\n') {
            length = 1;
            while (begin + length < source.text().size() && (static_cast<unsigned char>(source.text()[begin + length]) & 0xc0) == 0x80) ++length;
        } else if (token.kind == TokenKind::number || token.kind == TokenKind::path) {
            auto end = source.text().find_first_of(token.kind == TokenKind::path ? "\r\n" : " ():,\r\n", begin);
            length = (end == std::string::npos ? source.text().size() : end) - begin;
        }
    }
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
constexpr Type error_type = static_cast<Type>(255); // AST-only; the lowering gate excludes it.
class Diagnostics {
    std::vector<Diagnostic> items;
    std::map<std::string, std::pair<std::string, std::shared_ptr<const SourceFile>>> snapshots;
    std::set<std::tuple<std::string, std::size_t, std::size_t, int, std::string>> seen;
    bool suppressed = false;
    unsigned expression_depth = 0;
public:
    void bind(const SourceFile& source, const std::string& identity) {
        snapshots.insert_or_assign(source.path(), std::make_pair(identity, std::make_shared<const SourceFile>(source)));
    }
    void add(const CompilationError& failure) {
        for (auto d : failure.diagnostics()) {
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
            if (items.size() == 100) { suppressed = true; continue; }
            seen.insert(std::move(key));
            items.push_back(std::move(d));
        }
    }
    bool failed() const { return std::any_of(items.begin(), items.end(), [](const auto& d) { return d.severity == Severity::error; }); }
    unsigned& semantic_depth() { return expression_depth; }
    bool full() { if (items.size() < 100) return false; suppressed = true; return true; }
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
        if (suppressed) { auto d = items.back(); d.code = Code::limit; d.description = "Diagnostic limit of 100 reached; further diagnostic analysis stopped."; d.related.clear(); d.expected_type.clear(); d.received_type.clear(); d.hint = "Fix the reported errors and check again."; items.push_back(std::move(d)); }
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
const std::set<std::string> reserved = {"func", "var", "return", "if", "else", "for", "break", "continue",
    "and", "or", "not", "true", "false", "while", "const", "use", "struct", "in", "reverse", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "as", "private", "public", "internal"};
std::vector<Token> lex(const SourceFile& source, Diagnostics& diagnostics) {
    const auto& text = source.text();
    std::vector<Token> tokens;
    std::vector<std::size_t> indents{0};
    std::size_t p = text.starts_with("\xef\xbb\xbf") ? 3 : 0;
    bool beginning = true;
    auto emit = [&](TokenKind kind, std::string value, std::size_t offset) { tokens.push_back({kind, std::move(value), offset}); };
    while (p < text.size()) {
        if (diagnostics.full()) break;
        try {
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
            if (!tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", start);
            beginning = true; continue;
        }
        if (c == '"') {
            ++p;
            while (p < text.size() && text[p] != '"' && text[p] != '\r' && text[p] != '\n') ++p;
            if (p == text.size() || text[p] != '"') error(source, {TokenKind::path, "", start}, Code::import_quote, "Unterminated import path.", "Close the path with a double quote on this line.");
            emit(TokenKind::path, text.substr(start + 1, p - start - 1), start); ++p; continue;
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
            if (p < text.size() && (letter(text[p]) || text[p] == '.' || (text[p] >= 'A' && text[p] <= 'Z')))
                error(source, {TokenKind::number, "", start}, Code::numeric_literal, "Unsupported numeric literal.", "Use decimal digits, an optional fractional part, and optional f or d suffix; separators, exponents and ranges are not implemented.");
            emit(TokenKind::number, text.substr(start, p - start), start); continue;
        }
        std::string pair = text.substr(p, 2);
        if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" || pair == "+=" ||
            pair == "-=" || pair == "*=" || pair == "/=" || pair == "%=" || pair == "++" || pair == "--") {
            p += 2; emit(TokenKind::symbol, pair, start); continue;
        }
        if (std::string("():,.+-*/%=<>").find(c) != std::string::npos) { ++p; emit(TokenKind::symbol, std::string(1, c), start); continue; }
        error(source, {TokenKind::symbol, "", p}, Code::character, "Unexpected character in source.",
              c == '\t' ? "Use spaces instead of tabs." : "Use the supported numeric/bool syntax and lowercase ASCII identifiers; Unicode identifiers and strings are not implemented yet.");
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
struct Expr {
    enum class Kind { literal, name, call, unary, binary, conversion } kind;
    Token token;
    Token qualifier_token{TokenKind::name, "", 0};
    std::string op, qualifier, target;
    std::uint64_t value = 0;
    std::string literal_text;
    std::vector<std::unique_ptr<Expr>> parts;
    Type type = Type::none;
    unsigned slot = 0;
    Expr(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Statement {
    enum class Kind { variable, assignment, expression, returned, conditional, loop, broken, continued } kind;
    Token token;
    std::string name, op, constant_syntax;
    std::optional<Type> annotation;
    std::unique_ptr<Expr> expression;
    std::vector<Statement> body, otherwise;
    unsigned slot = 0, flow = 0;
    bool readonly = false, invalid = false;
    Statement(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Parameter { Token token; Type type; unsigned slot = 0; };
struct Function {
    Token token;
    bool file_private = false, parse_failed = false;
    std::string symbol, visibility = "public";
    std::vector<Parameter> parameters;
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
};
struct Import {
    Token path, alias;
    std::string target;
    bool explicit_alias = false;
};
struct Unit {
    SourceFile source;
    std::string identity;
    std::vector<Import> imports;
    std::vector<Function> functions;
    std::vector<Constant> constants;
    std::map<std::string, Constant*> constant_names;
    std::map<std::string, Function*> declarations;
    std::map<std::string, std::vector<Unit*>> qualifiers;
    std::set<std::string> explicit_qualifiers;
    std::set<std::string> failed_qualifiers, invalid_declarations;
    bool failed_import = false;
    bool parse_failed = false;
    Unit(SourceFile s, std::string id) : source(std::move(s)), identity(std::move(id)) {}
};
class Parser {
    const SourceFile& source;
    std::vector<Token> tokens;
    std::size_t p = 0;
    Diagnostics& diagnostics;
    unsigned depth = 0, nodes = 0;
    const Token& peek() const { return tokens[std::min(p, tokens.size() - 1)]; }
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
            if (declaration && current_depth == 0 && line_start && (at("func") || at("const") || at("use") || at("private") || at("public") || at("internal"))) return;
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
    Type type() {
        static const std::map<std::string, Type> types = {
            {"i8",Type::i8},{"i16",Type::i16},{"i32",Type::i32},{"i64",Type::i64},
            {"u8",Type::u8},{"u16",Type::u16},{"u32",Type::u32},{"u64",Type::u64},
            {"f32",Type::f32},{"f64",Type::f64},{"bool",Type::boolean}};
        auto found = types.find(peek().text);
        if (found != types.end()) { take(); return found->second; }
        error(source, peek(), Code::unsupported_type, "Unsupported type '" + peek().text + "'.", "Use an exact-width numeric type or bool; omit the return annotation for no result.");
    }
    void newline() { need("<newline>"); }
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
    static int precedence(const std::string& op) {
        if (op == "or") return 1;
        if (op == "and") return 2;
        if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") return 3;
        if (op == "+" || op == "-") return 4;
        if (op == "*" || op == "/" || op == "%") return 5;
        return 0;
    }
    std::unique_ptr<Expr> primary() {
        if (++nodes > 512) error(source, peek(), Code::nesting, "Expression size limit exceeded.", "Split this expression into smaller local values.");
        auto token = take();
        if (token.kind == TokenKind::invalid) throw Poison{};
        if (token.kind == TokenKind::number || token.text == "true" || token.text == "false") {
            auto e = std::make_unique<Expr>(Expr::Kind::literal, token);
            if (token.kind == TokenKind::number) {
                e->literal_text = token.text;
            } else { e->type = Type::boolean; e->value = token.text == "true"; }
            return e;
        }
        if (token.text == "(" ) { auto e = expression(); need(")"); return e; }
        if (token.kind == TokenKind::name && !reserved.contains(token.text)) {
            auto e = std::make_unique<Expr>(Expr::Kind::name, token);
            if (accept(".")) {
                e->qualifier = token.text;
                e->qualifier_token = token;
                e->token = name();

            }
            if (accept("(")) {
                e->kind = Expr::Kind::call;
                if (!at(")")) do { e->parts.push_back(expression()); } while (accept(","));
                need(")");
            }
            return e;
        }
        error(source, token, Code::syntax, "Expected an expression, found '" + token.text + "'.");
    }
    std::unique_ptr<Expr> unary() {
        Depth guard(depth, source, peek());
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
                auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::conversion, token);
                e->parts.push_back(std::move(left)); e->type = type(); left = std::move(e); continue;
            }
            auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::binary, token); e->op = token.text;
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
        if (at("while")) error(source, peek(), Code::while_loop, "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:'.");
        if (at("if")) return conditional(take());
        if (at("for")) {
            Statement s(Statement::Kind::loop, take());
            if (!at(":")) s.expression = expression();
            if (at("in") || at(",")) error(source, peek(), Code::unsupported_iteration, "Collection/range iteration is not implemented.", "Use 'for condition:' or 'for:' for this subset.");
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
            auto op = tokens[p + 1].text;
            if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "++" || op == "--") {
                Statement s(Statement::Kind::assignment, name()); s.name = s.token.text; s.op = take().text;
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
    void parse(Unit& unit) {
        auto before = diagnostics.size();
        while (peek().kind != TokenKind::end) {
            if (diagnostics.full()) { unit.parse_failed = true; break; }
            auto start = p;
            try {
            if (accept("<newline>")) continue;
            if (peek().kind == TokenKind::invalid) throw Poison{};
            if (at("while")) error(source, peek(), Code::while_loop, "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:' inside a function.");
            if (accept("use")) {
                if (!unit.functions.empty() || !unit.constants.empty()) error(source, peek(), Code::syntax, "Imports must precede constant and function declarations.", "Move use declarations to the start of this file.");
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
                unit.constants.push_back({std::move(declaration), f.file_private, f.visibility}); continue;
            }
            need("func"); f.token = name(); need("(");
            if (!at(")")) do { auto token = name(); need(":"); f.parameters.push_back({token, type()}); } while (accept(","));
            need(")"); if (accept("->")) f.result = type(); f.body = block(); f.parse_failed = diagnostics.within(source, f.token.offset, peek().offset); unit.functions.push_back(std::move(f));
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
class Semantics {
    const SourceFile& source;
    Unit& unit;
    const std::map<std::string, std::unique_ptr<Unit>>& units;
    std::set<std::string> visible_names;
    Unit& root;
    std::function<void(Unit&, Constant&)> evaluate;
    Diagnostics& diagnostics;
    bool invalid_function = false;
    struct Variable { Type type; unsigned slot; bool readonly; Token declaration; };
    std::vector<std::map<std::string, Variable>> scopes;
    Function* current = nullptr;
    unsigned loops = 0;
    bool suppress_zero = false;
    Variable* lookup(const std::string& name) {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            auto found = scope->find(name); if (found != scope->end()) return &found->second;
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
    void expect(Type actual, Type expected, const Token& token) {
        if (actual == error_type || expected == error_type) return;
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
        scopes.back().emplace(token.text, Variable{type, slot, readonly, token}); return slot;
    }
    // Hints resolve typed anchors without prematurely defaulting untyped literals.
    std::optional<Type> anchor(Expr& e) {
        try {
        using K = Expr::Kind;
        if (e.kind == K::literal) {
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
        if (e.kind == K::call) return resolve(e).result;
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
    Type expression(Expr& e, std::optional<Type> expected = {}) {
        try {
            Depth guard(diagnostics.semantic_depth(), source, e.token, 256);
            auto type = expression_impl(e, expected);
            if (type == error_type) { e.type = error_type; invalid_function = true; }
            return type;
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
          catch (const Poison&) {}
        invalid_function = true;
        return e.type = error_type;
    }
    Type expression_impl(Expr& e, std::optional<Type> expected) {
        using K = Expr::Kind;
        if (e.kind == K::literal) {
            if (e.literal_text.empty()) return e.type;
            Type target = anchor(e).value_or(expected && numeric::number(*expected) && (!decimal(e) || numeric::floating(*expected)) ? *expected : decimal(e) ? Type::f32 : Type::i32);
            try { e.value = numeric::literal(e.literal_text,target); }
            catch (const std::invalid_argument& failure) { error(source,e.token,numeric::floating(target) ? Code::numeric_representability : Code::integer_range,failure.what()); }
            return e.type = target;
        }
        if (e.kind == K::name) {
            auto* v = e.qualifier.empty() ? lookup(e.token.text) : nullptr;
            if (!v) {
                auto [owner, c] = constant(e);
                if (c) {
                    evaluate(*owner, *c); e.target = owner->identity + "::" + c->declaration.name; e.kind = K::literal; e.value = c->value;
                    return e.type = c->type;
                }
            }
            if (!v && unit.failed_import) throw Poison{};
            if (!v) error(source, e.token, Code::unknown_identifier, "Unknown identifier '" + e.token.text + "'.", "Declare it with var before use.");
            e.slot = v->slot; return e.type = v->type;
        }
        if (e.kind == K::call) {
            if (!current) error(source, e.token, Code::constant_call, "A compile-time constant cannot call a function.", "Use a local const for runtime values.");
            if (auto* value = e.qualifier.empty() ? lookup(e.token.text) : nullptr) {
                if (value->type == error_type) throw Poison{};
                error(source, e.token, Code::not_callable, "A local value is not callable.");
            }
            const auto& target = [&]() -> const Function& {
                auto check_arguments = [&] { for (auto& argument : e.parts) expression(*argument); };
                try { return resolve(e); }
                catch (const CompilationError&) { check_arguments(); throw; }
                catch (const Poison&) { check_arguments(); throw; }
            }();
            e.target = target.symbol;
            std::vector<Type> arguments;
            for (std::size_t i=0; i<e.parts.size(); ++i) arguments.push_back(expression(*e.parts[i], i<target.parameters.size() ? std::optional<Type>(target.parameters[i].type) : std::nullopt));
            if (e.parts.size() != target.parameters.size()) error(source, e.token, Code::argument_count, "Incorrect argument count for '" + e.token.text + "': expected " + std::to_string(target.parameters.size()) + ", received " + std::to_string(e.parts.size()) + ".");
            bool invalid = false;
            for (std::size_t i = 0; i < e.parts.size(); ++i) {
                try { expect(arguments[i], target.parameters[i].type, e.parts[i]->token); }
                catch (const CompilationError& failure) { diagnostics.add(failure); invalid = true; }
                invalid |= arguments[i] == error_type;
            }
            return e.type = invalid ? error_type : target.result;
        }
        if (e.kind == K::conversion) {
            Type target = e.type;
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
            auto from = expression(operand);
            if (from == error_type) return error_type;
            if (!numeric::number(from) || !numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
            return target;
        }
        if (e.kind == K::unary) {
            auto left = expression(*e.parts[0], e.op == "not" ? std::optional<Type>(Type::boolean) : expected);
            if (left == error_type) return error_type;
            if (e.op == "not") expect(left,Type::boolean,e.token);
            else if (!numeric::number(left)) error(source,e.token,Code::numeric_operator,"Type mismatch: unary arithmetic requires a numeric operand.");
            return e.type = left;
        }
        bool logical = e.op == "and" || e.op == "or";
        auto left_hint = anchor(*e.parts[0]), right_hint = anchor(*e.parts[1]);
        auto context = logical ? std::optional<Type>(Type::boolean) : left_hint ? left_hint : right_hint ? right_hint :
            expected && numeric::number(*expected) ? expected : std::optional<Type>(decimal(e) ? Type::f32 : Type::i32);
        auto left = expression(*e.parts[0],context);
        bool previous = suppress_zero;
        if (e.parts[0]->kind == K::literal && ((e.op == "and" && !e.parts[0]->value) || (e.op == "or" && e.parts[0]->value))) suppress_zero = true;
        auto right = expression(*e.parts[1],context); suppress_zero = previous;
        if (left == error_type || right == error_type) return error_type;
        if (logical) {
            expect(left,Type::boolean,e.parts[0]->token); expect(right,Type::boolean,e.parts[1]->token); return e.type = Type::boolean;
        }
        if (e.op != "==" && e.op != "!=" && !numeric::number(left)) expect(left,Type::i32,e.parts[0]->token);
        expect(right,left,e.token);
        if (e.op == "==" || e.op == "!=") {
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
        case K::variable: {
            if (s.invalid) { s.slot = declare(s.token, error_type); invalid_function = true; break; }
            Type t = s.expression ? expression(*s.expression, s.annotation) : s.annotation.value_or(Type::none);
            if (t == Type::none) {
                s.slot = declare(s.token, error_type, s.readonly);
                error(source, s.token, Code::invalid_value, "Variable requires a value type.", "Supply a numeric/bool annotation or a value-producing initializer.");
            }
            if (s.annotation) {
                try { expect(t, *s.annotation, s.expression ? s.expression->token : s.token); }
                catch (const CompilationError& failure) { diagnostics.add(failure); t = error_type; }
            }
            s.slot = declare(s.token, t, s.readonly); break;
        }
        case K::assignment: {
            auto* variable = lookup(s.name);
            if ((variable && variable->readonly) || (!variable && visible_names.contains(s.name)))
                error(source, s.token, Code::readonly, "Cannot mutate readonly const or declaration '" + s.name + "'.", "Use var for a mutable local binding.");
            if (!variable) error(source, s.token, Code::unknown_identifier, "Unknown identifier '" + s.name + "'.", "Declare it with var before assignment.");
            auto t = variable->type; s.slot = variable->slot;
            expect(expression(*s.expression, t), t, s.expression->token);
            if (s.op != "=" && t != error_type && !numeric::number(t)) expect(t,Type::i32,s.token);
            if (s.op == "%=" && numeric::floating(t)) error(source,s.token,Code::numeric_operator,"Illegal compound floating remainder operator.");
            if (numeric::integer(t) && (s.op == "/=" || s.op == "%=") && s.expression->kind == Expr::Kind::literal && s.expression->value == 0)
                error(source, s.expression->token, Code::zero_divisor, "Division or remainder by zero.");
            break;
        }
        case K::expression:
            if (expression(*s.expression) == error_type) break;
            if (s.expression->kind != Expr::Kind::call) error(source, s.token, Code::invalid_value, "Only function calls can be standalone expressions.", "Assign this value with var or return it.");
            break;
        case K::returned:
            if (s.expression) {
                auto result = expression(*s.expression, current->result);
                if (result == error_type) { flow = return_flow; break; }
                expect(result, current->result, s.token);
            } else expect(Type::none, current->result, s.token);
            // A no-result call is a statement, not a return value.
            if (s.expression && current->result == Type::none) error(source, s.token, Code::invalid_value, "A no-result function cannot return an expression.", "Call the function, then use an empty return.");
            flow = return_flow; break;
        case K::conditional:
            try { expect(expression(*s.expression), Type::boolean, s.expression->token); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
            flow = sequence(s.body) | (s.otherwise.empty() ? next_flow : sequence(s.otherwise)); break;
        case K::loop: {
            if (s.expression) {
                try { expect(expression(*s.expression), Type::boolean, s.expression->token); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            ++loops; auto body = sequence(s.body); --loops;
            flow = body & return_flow;
            if (!always(s.expression.get()) || (body & break_flow)) flow |= next_flow;
            break;
        }
        case K::broken: case K::continued:
            if (!loops) error(source, s.token, Code::loop_control, s.token.text + " is only valid inside a for loop.");
            flow = s.kind == K::broken ? break_flow : continue_flow; break;
        }
        return s.flow = flow;
    }
public:
    Semantics(Unit& u, const std::map<std::string, std::unique_ptr<Unit>>& graph, Unit& r,
        std::function<void(Unit&, Constant&)> eval, Diagnostics& d)
        : source(u.source), unit(u), units(graph), root(r), evaluate(std::move(eval)), diagnostics(d) {
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
    void analyze_constant(Constant& c) {
        c.type = expression(*c.declaration.expression, c.declaration.annotation);
        if (c.type == error_type) throw Poison{};
        if (c.declaration.annotation) expect(c.type, *c.declaration.annotation, c.declaration.token);
    }
    void analyze() {
        for (auto& f : unit.functions) {
            if (diagnostics.full()) break;
            current = &f; scopes = {{}}; invalid_function = false;
            auto before = diagnostics.size();
            for (auto& p : f.parameters) {
                try { p.slot = declare(p.token, p.type); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            auto flow = sequence(f.body, false);
            if (f.result != Type::none && (flow & next_flow) && diagnostics.size() == before && !f.parse_failed && !invalid_function)
                try { error(source, f.token, Code::missing_return, "Missing return path in '" + f.token.text + "'.", "Return a " + std::string(ir::type_name(f.result)) + " on every path that reaches the end."); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
};
class Lowering {
    const SourceFile& source;
    ir::Function output;
    std::optional<unsigned> current;
    std::vector<std::pair<unsigned, unsigned>> loops; // continue, break destinations
    unsigned block() {
        auto id = static_cast<unsigned>(output.blocks.size());
        output.blocks.push_back({{}, {ir::Terminator::Kind::unreachable}}); return id;
    }
    void terminate(ir::Terminator end) { output.blocks[*current].end = end; current.reset(); }
    void jump(unsigned to) { terminate({ir::Terminator::Kind::jump, 0, to}); }
    unsigned emit(ir::Instruction i, const Token& token) {
        i.source = source.location(token.offset);
        if (i.op == ir::Op::call || i.op == ir::Op::divide || i.op == ir::Op::remainder || i.op == ir::Op::convert)
            i.source_excerpt = source.excerpt(token.offset);
        auto& instructions = output.blocks[*current].instructions;
        auto id = static_cast<unsigned>(instructions.size()); instructions.push_back(std::move(i)); return id;
    }
    unsigned expression(const Expr& e) {
        using K = Expr::Kind;
        switch (e.kind) {
        case K::literal: {
            ir::Instruction i{ir::Op::constant}; i.immediate = std::bit_cast<std::int64_t>(e.value); i.type = e.type; return emit(i, e.token);
        }
        case K::name: return emit({ir::Op::load, e.slot}, e.token);
        case K::call: {
            // Materialize each argument in a slot: later short-circuit arguments
            // may create new blocks, while IR instruction values stay block-local.
            std::vector<unsigned> slots;
            for (const auto& arg : e.parts) {
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(arg->type);
                auto value = expression(*arg); emit({ir::Op::store, slot, value}, arg->token); slots.push_back(slot);
            }
            ir::Instruction i{ir::Op::call}; i.callee = e.target;
            for (auto slot : slots) i.arguments.push_back(emit({ir::Op::load, slot}, e.token));
            return emit(i, e.token);
        }
        case K::conversion: {
            auto value = expression(*e.parts[0]); ir::Instruction i{ir::Op::convert,value}; i.type = e.type; return emit(i,e.token);
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
            return emit({operation(e.op), left, right}, e.token);
        }
        }
        throw std::logic_error("unknown typed expression");
    }
    void sequence(const std::vector<Statement>& body) { for (const auto& s : body) if (current) statement(s); }
    void statement(const Statement& s) {
        using K = Statement::Kind;
        switch (s.kind) {
        case K::variable: {
            ir::Instruction zero{ir::Op::constant}; zero.type = output.locals[s.slot];
            auto value = s.expression ? expression(*s.expression) : emit(zero, s.token);
            emit({ir::Op::store, s.slot, value}, s.token); break;
        }
        case K::assignment: {
            // Load compound-assignment LHS before evaluating its RHS.
            std::optional<unsigned> old_slot;
            if (s.op != "=") {
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(output.locals[s.slot]); old_slot = slot;
                auto old = emit({ir::Op::load, s.slot}, s.token); emit({ir::Op::store, slot, old}, s.token);
            }
            auto value = expression(*s.expression);
            if (old_slot) {
                auto old = emit({ir::Op::load, *old_slot}, s.token);
                value = emit({operation(s.op.substr(0, 1)), old, value}, s.token);
            }
            emit({ir::Op::store, s.slot, value}, s.token); break;
        }
        case K::expression: expression(*s.expression); break;
        case K::returned:
            if (s.expression) { auto value = expression(*s.expression); terminate({ir::Terminator::Kind::return_value, value}); }
            else terminate({ir::Terminator::Kind::return_void});
            break;
        case K::conditional: {
            auto condition = expression(*s.expression); auto yes = block(), no = block(), merge = block();
            terminate({ir::Terminator::Kind::branch, condition, yes, no});
            current = yes; sequence(s.body); bool yes_next = current.has_value(); if (current) jump(merge);
            current = no; sequence(s.otherwise); bool no_next = current.has_value(); if (current) jump(merge);
            if (yes_next || no_next) current = merge;
            break;
        }
        case K::loop: {
            auto test = block(), body = block(), done = block(); jump(test);
            current = test;
            if (always(s.expression.get())) jump(body);
            else { auto condition = expression(*s.expression); terminate({ir::Terminator::Kind::branch, condition, body, done}); }
            loops.emplace_back(test, done); current = body; sequence(s.body); if (current) jump(test); loops.pop_back();
            if (s.flow & next_flow) current = done;
            break;
        }
        case K::broken: jump(loops.back().second); break;
        case K::continued: jump(loops.back().first); break;
        }
    }
public:
    explicit Lowering(const SourceFile& s) : source(s) {}
    ir::Function lower(const Function& f) {
        output = {}; output.name = f.symbol; output.source_name = f.token.text; output.source = source.location(f.token.offset);
        output.parameters.clear(); output.result = f.result; output.locals = f.locals; current = block();
        for (std::size_t n = 0; n < f.parameters.size(); ++n) {
            const auto& p = f.parameters[n]; output.parameters.push_back(p.type);
            auto value = emit({ir::Op::argument, static_cast<unsigned>(n)}, p.token); emit({ir::Op::store, p.slot, value}, p.token);
        }
        sequence(f.body);
        if (current) terminate({ir::Terminator::Kind::return_void}); // Only reachable for checked no-result functions.
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
        auto parent = std::filesystem::path(from.identity).parent_path();
        for (const auto& component : parent.relative_path()) parts.push_back(component.generic_string());
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
public:
    std::map<std::string, std::unique_ptr<Unit>> units;
    explicit SourceGraph(std::filesystem::path project_root, Diagnostics& d, const SourceOverrides& o) : root(std::move(project_root)), diagnostics(d), overrides(o) {}
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
        for (const auto& part : std::filesystem::path(id).relative_path()) {
            auto spelling = part.generic_string();
            std::set<std::string> matches;
            for (std::filesystem::directory_iterator it(physical, ec), end; !ec && it != end; it.increment(ec)) {
                auto candidate = it->path().filename().generic_string();
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
                auto other_path = root / std::filesystem::path(other_id).relative_path();
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
        for (auto& import : result->imports) {
            try {
                auto& target = load(*result, import.path);
                import.target = target.identity;
            } catch (const CompilationError& failure) {
                diagnostics.add(failure); result->failed_import = true;
                auto qualifier = import.explicit_alias ? import.alias.text : std::filesystem::path(import.path.text).stem().generic_string();
                result->failed_qualifiers.insert(qualifier);
            }
        }
        return *result;
    }
};
// Only typed, pure ASTs reach this evaluator. The shared software kernel
// preserves declared precision and wrapping independently of host arithmetic.
std::uint64_t constant_value(const SourceFile& source, const Expr& e) {
    if (e.kind == Expr::Kind::literal) return e.value;
    auto a = constant_value(source,*e.parts[0]);
    try {
        if (e.kind == Expr::Kind::conversion) return numeric::convert(a,e.parts[0]->type,e.type);
        if (e.kind == Expr::Kind::unary) return e.op == "not" ? !a : e.op == "-" ? numeric::negate(a,e.type) : a;
        if (e.op == "and" && !a) return 0;
        if (e.op == "or" && a) return 1;
        auto b = constant_value(source,*e.parts[1]);
        if (e.op == "and" || e.op == "or") return b;
        return numeric::binary(operation(e.op),a,b,e.parts[0]->type);
    } catch (const CompilationError&) { throw; }
      catch (const std::invalid_argument& failure) {
        error(source,e.token,e.kind == Expr::Kind::conversion ? Code::numeric_conversion : Code::zero_divisor,failure.what());
    }
}
void validate_graph(std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry, Diagnostics& diagnostics) {
    // Discover every signature before checking any body, including cyclic imports.
    for (auto& [id, unit] : units) for (auto& f : unit->functions) {
        f.symbol = id + "::" + f.token.text;
        auto [previous, inserted] = unit->declarations.emplace(f.token.text, &f);
        if (!inserted) {
            unit->invalid_declarations.insert(f.token.text);
            try { error(unit->source, f.token, Code::duplicate, "Duplicate function '" + f.token.text + "'.", {}, {{span(unit->source, previous->second->token), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    auto& root = *units.at(entry);
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        auto& token = c.declaration.token;
        auto previous = unit->constant_names.find(token.text);
        if (unit->declarations.contains(token.text) || previous != unit->constant_names.end()) {
            auto location = unit->declarations.contains(token.text) ? unit->declarations.at(token.text)->token : previous->second->declaration.token;
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
        for (const auto& import : unit->imports) {
            if (import.target.empty()) continue;
            if (units.at(import.target)->parse_failed) unit->failed_import = true;
            auto alias = import.explicit_alias ? import.alias.text : std::filesystem::path(import.target).stem().generic_string();
            try {
            if (!import.explicit_alias && !identifier(alias)) continue;
            if ((import.explicit_alias && unit->declarations.contains(alias)) || unit->constant_names.contains(alias) || (root.constant_names.contains(alias) && !root.constant_names.at(alias)->file_private))
                error(unit->source, import.alias, Code::alias_conflict, "Import alias '" + alias + "' conflicts with " + (unit->declarations.contains(alias) ? "a function declaration." : "a constant declaration."), "Choose a distinct explicit alias with as.");
            auto* target = units.at(import.target).get();
            auto& targets = unit->qualifiers[alias];
            bool repeated = std::find(targets.begin(), targets.end(), target) != targets.end();
            bool different = std::any_of(targets.begin(), targets.end(), [&](Unit* previous) { return previous != target; });
            if (different && (import.explicit_alias || unit->explicit_qualifiers.contains(alias))) {
                std::vector<RelatedLocation> related;
                for (const auto& previous : unit->imports) {
                    if (&previous == &import) break;
                    auto previous_alias = previous.explicit_alias ? previous.alias.text : std::filesystem::path(previous.target).stem().generic_string();
                    if (previous_alias == alias && !previous.target.empty()) { related.push_back({span(unit->source, previous.alias), "Previously imported"}); break; }
                }
                error(unit->source, import.alias, Code::alias_conflict, "Conflicting import alias '" + alias + "'.", "Give these modules different aliases using as.", std::move(related));
            }
            if (import.explicit_alias) unit->explicit_qualifiers.insert(alias);
            if (!repeated) targets.push_back(target);
            } catch (const CompilationError& failure) { diagnostics.add(failure); unit->failed_qualifiers.insert(alias); }
        }
    }
    std::vector<std::string> dependency;
    std::function<void(Unit&, Constant&)> evaluate = [&](Unit& owner, Constant& c) {
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
            Semantics(owner, units, root, evaluate, diagnostics).analyze_constant(c);
            c.value = constant_value(owner.source, *c.declaration.expression);
            c.state = 2; dependency.pop_back();
        } catch (...) { c.state = 3; dependency.pop_back(); throw; }
    };
    for (auto& [id, unit] : units) for (auto& c : unit->constants) {
        if (diagnostics.full()) break;
        try { evaluate(*unit, c); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id, unit] : units) Semantics(*unit, units, root, evaluate, diagnostics).analyze();
    // Recovered trees and AST-only poisoned types can never enter validated IR.
    diagnostics.finish();
}
ir::Module lower_graph(const std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry) {
    ir::Module module; module.identity = entry; module.entry_symbol = entry + "::entry";
    module.source_path = units.at(entry)->source.path();
    for (auto& [id, unit] : units) {
        Lowering lowering(unit->source);
        for (const auto& f : unit->functions) {
            auto lowered = lowering.lower(f); lowered.module_identity = id;
            module.functions.push_back(std::move(lowered));
        }
    }
    ir::validate(module); return module;
}

}
std::shared_ptr<const ValidatedAnalysis> validated_source(const SourceFile&);
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path&, const SourceOverrides& = {}, bool verify_disk = false);
// This factory is the only way to construct a successful result.
struct AnalysisAccess {
    static std::shared_ptr<const ValidatedAnalysis> build(std::map<std::string, std::unique_ptr<Unit>> units, const std::string& entry) {
        auto result = std::shared_ptr<ValidatedAnalysis>(new ValidatedAnalysis);
        result->entry_ = entry;
        auto graph = std::make_shared<const std::map<std::string, std::unique_ptr<Unit>>>(std::move(units));
        auto field = [](std::string& out, const std::string& value) { out += std::to_string(value.size()) + ":" + value; };
        auto declaration = [&](AnalyzedDeclaration d) {
            if (d.owner.empty()) d.owner = d.module;
            field(result->contract_, "declaration"); field(result->contract_, d.identity); field(result->contract_, d.visibility);
            field(result->contract_, std::to_string(static_cast<int>(d.kind)));
            field(result->contract_, ir::type_name(d.type));
            field(result->contract_, std::to_string(d.parameters.size()));
            for (auto t : d.parameters) field(result->contract_, ir::type_name(t));
            if (d.kind != AnalyzedDeclaration::Kind::function) {
                field(result->constants_, d.identity); field(result->constants_, d.visibility);
                field(result->constants_, ir::type_name(d.type)); field(result->constants_, d.constant_syntax);
            }
            result->declarations_.push_back(std::move(d));
        };
        auto reference = [&](const std::string& from, const std::string& to, const Expr& e, const Unit& u) {
            auto location = span(u.source,e.token); location.module_identity = u.identity;
            auto spelling = e.qualifier + "::" + e.token.text;
            result->references_.push_back({from,to,spelling,std::move(location)});
            field(result->contract_,"reference"); field(result->contract_,from); field(result->contract_,to);
        };
        for (const auto& [id, unit] : *graph) {
            AnalyzedModule module{id,unit->source,{}};
            field(result->contract_,"module"); field(result->contract_,id);
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
            std::function<void(const Expr&,const std::string&,const std::map<unsigned,std::string>&)> expression;
            expression = [&](const Expr& e, const std::string& owner, const auto& locals) {
                if (!e.target.empty()) reference(owner,e.target,e,*unit);
                else if (e.kind == Expr::Kind::name && locals.contains(e.slot)) reference(owner,locals.at(e.slot),e,*unit);
                for (const auto& part : e.parts) expression(*part,owner,locals);
            };
            for (const auto& c : unit->constants) {
                auto key = id + "::" + c.declaration.name;
                declaration({AnalyzedDeclaration::Kind::module_constant,key,id,c.visibility,location(c.declaration.token),{},c.type,c.declaration.constant_syntax,c.value});
                expression(*c.declaration.expression,key,{});
            }
            for (const auto& f : unit->functions) {
                std::vector<Type> parameters; for (const auto& p : f.parameters) parameters.push_back(p.type);
                declaration({AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,f.result,{}});
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
                        if (s.expression) expression(*s.expression,owner,locals);
                        if (s.readonly) locals.emplace(s.slot,owner);
                        body(s.body); body(s.otherwise);
                    }
                };
                body(f.body);
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
    try { return {validated_source(source),{}}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
AnalysisResult analyze_project(const std::filesystem::path& entry) {
    try { return {validated_project(entry),{}}; }
    catch (const CompilationError& e) { return {nullptr,e.diagnostics()}; }
}
SourceFile read_source(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw SourceReadError(path.generic_string(), "Cannot read source file.");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw SourceReadError(path.generic_string(), "Cannot finish reading source file.");
    return {path.generic_string(), std::move(text)};
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
    return AnalysisAccess::build(std::move(units), identity);
}
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path& entry, const SourceOverrides& overrides, bool verify_disk) {
    for (const auto& [id, source] : overrides) { (void)source; require_module_identity(id); }
    std::error_code ec;
    auto absolute = std::filesystem::absolute(entry, ec);
    if (ec) error(SourceFile(entry.generic_string(), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the entry source path.");
    auto root = std::filesystem::canonical(absolute.parent_path(), ec);
    if (ec) error(SourceFile(entry.generic_string(), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the project root directory.", "Check that the entry's parent directory exists and is readable.");
    // A diagnostic anchor is needed even before the entry file can be opened.
    Unit anchor(SourceFile(absolute.generic_string(), ""), "/");
    Diagnostics diagnostics; diagnostics.bind(anchor.source, "/" + absolute.filename().generic_string());
    SourceGraph graph(root, diagnostics, overrides);
    auto& first = [&]() -> Unit& {
        try { return graph.load(anchor, {TokenKind::path, "/" + absolute.filename().generic_string(), 0}); }
        catch (const CompilationError& failure) { diagnostics.add(failure); diagnostics.finish(); throw; }
    }();
    // A disk read is not an atomic filesystem transaction. Re-discover paths and
    // compare exact bytes before accepting this captured graph. Overlays are frozen.
    if (verify_disk) {
        Diagnostics check; SourceGraph again(root, check, overrides);
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
    return AnalysisAccess::build(std::move(graph.units), identity);
}
void require_module_identity(const std::string& identity) {
    Unit anchor(SourceFile("<document identity>", ""), "/");
    if (identity.empty() || identity[0] != '/' || normalize(anchor, {TokenKind::path, identity, 0}) != identity)
        throw std::invalid_argument("Document identity must be an exact canonical root-relative .void path");
}
AnalysisResult analyze_project(const std::filesystem::path& entry, const SourceOverrides& overrides) {
    try { return {validated_project(entry, overrides, true), {}}; }
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
void require_source_entry(const SourceFile& source, const ir::Module& module) {
    Diagnostics diagnostics; diagnostics.bind(source, module.identity);
    try {
    for (const auto& f : module.functions) if (f.name == module.entry_symbol) {
        if (f.parameters == std::vector<Type>{Type::i32} && f.result == Type::i32) return;
        std::size_t offset = 0;
        while (offset < source.text().size() && source.location(offset).line < f.source.line) ++offset;
        while (offset < source.text().size() && source.location(offset).column < f.source.column) ++offset;
        error(source, {TokenKind::name, "entry", offset}, Code::entry, "The CLI entry point must have signature entry(value: i32) -> i32.", "Other functions can have any supported parameter list; choose an i32 adapter for the CLI.");
    }
    error(source, {TokenKind::end, "", 0}, Code::entry, "No entry function found.", "Define func entry(value: i32) -> i32 to run or emit this file; check accepts libraries without an entry.");
    } catch (const CompilationError& failure) { diagnostics.add(failure); diagnostics.finish(); }
}
}
