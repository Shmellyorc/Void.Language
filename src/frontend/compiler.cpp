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
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <type_traits>
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
const Type error_type = Type::invalid; // AST-only; the lowering gate excludes it.
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
    "and", "or", "not", "true", "false", "null", "while", "const", "use", "struct", "in", "reverse", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64", "bool", "as", "private", "public", "internal"};
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
        if (std::string("():,.+-*/%=<>?").find(c) != std::string::npos) { ++p; emit(TokenKind::symbol, std::string(1, c), start); continue; }
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
    enum class Kind { literal, name, call, unary, binary, conversion, field, construction, method_call, base_initialization } kind;
    Token token;
    Token qualifier_token{TokenKind::name, "", 0};
    std::string op, qualifier, target;
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
    bool super_call = false;
    Expr(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Statement {
    enum class Kind { variable, assignment, expression, returned, conditional, loop, broken, continued } kind;
    Token token;
    std::string name, op, constant_syntax;
    std::optional<Type> annotation;
    std::unique_ptr<Expr> expression;
    std::vector<Token> fields;
    std::vector<bool> implicit_fields;
    std::vector<unsigned> field_indexes;
    Type assigned_type = Type::none;
    std::vector<Statement> body, otherwise;
    unsigned slot = 0, flow = 0;
    bool readonly = false, invalid = false, using_field = false;
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
struct Function {
    std::map<std::set<unsigned>,ConstructionContext> construction_contexts;
    std::string construction_contract;
    Token token;
    bool file_private = false, parse_failed = false;
    std::string symbol, visibility = "public";
    std::vector<Parameter> parameters;
    std::optional<Parameter> receiver;
    bool receiver_mutates = false, initializer = false;
    bool virtual_method = false, overriding = false;
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
struct Unit;
struct Structure {
    Structure() = default;
    // Fields own noncopyable AST nodes. Container copy traits do not inspect
    // vector<Statement>'s copy body; never let reallocation select that path.
    Structure(const Structure&) = delete;
    Structure& operator=(const Structure&) = delete;
    Structure(Structure&&) = default;
    Structure& operator=(Structure&&) = default;
    Token token;
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
    std::vector<Function> functions;
    std::vector<Constant> constants;
    std::vector<Structure> structures;
    std::vector<std::pair<Token,Type>> type_uses;
    std::map<std::string, Structure*> structure_names;
    std::map<std::string, Constant*> constant_names;
    std::map<std::string, Function*> declarations, methods;
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
            if (declaration && current_depth == 0 && line_start && (at("func") || at("const") || at("value") || at("struct") || at("virtual") || at("override") || at("use") || at("private") || at("public") || at("internal"))) return;
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
        if (found != types.end()) { take(); if (at("?")) error(source,take(),Code::unsupported_type,"Only managed references can be nullable in this phase."); return found->second; }
        if (peek().kind != TokenKind::name || reserved.contains(peek().text)) error(source,peek(),Code::unsupported_type,"Unsupported type '" + peek().text + "'.","Use a numeric type, bool or a visible value/managed struct.");
        auto token = name();
        std::string spelling = token.text;
        auto reference_token = token;
        if (accept(".")) { reference_token = name(); spelling += "." + reference_token.text; }
        auto d = std::make_shared<ir::ValueType>(); d->name = spelling; d->identity = spelling; d->source_offset = token.offset;
        Type result(d,Type::unresolved); result.nullable = accept("?"); type_uses.emplace_back(reference_token,result); return result;
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
        if (token.kind == TokenKind::number || token.text == "true" || token.text == "false" || token.text == "null") {
            auto e = std::make_unique<Expr>(Expr::Kind::literal, token);
            if (token.kind == TokenKind::number) {
                e->literal_text = token.text;
            } else { e->type = token.text == "null" ? Type::null_value : Type::boolean; e->value = token.text == "true"; }
            return e;
        }
        if (token.text == "(" || (token.kind == TokenKind::name && !reserved.contains(token.text))) {
            std::unique_ptr<Expr> e;
            if (token.text == "(") { e = expression(); need(")"); }
            else e = std::make_unique<Expr>(Expr::Kind::name, token);
            while (true) {
                if (accept(".")) {
                    auto member = std::make_unique<Expr>(Expr::Kind::field, name());
                    member->parts.push_back(std::move(e)); e = std::move(member);
                } else if (accept("(")) {
                    if (e->kind == Expr::Kind::field) e->kind = Expr::Kind::method_call;
                    else if (e->kind == Expr::Kind::name) e->kind = Expr::Kind::call;
                    else error(source,e->token,Code::not_callable,"Only named functions, value types and receiver methods are callable.");
                    if (!at(")")) do {
                        Token named{TokenKind::name,"",peek().offset};
                        if (p + 1 < tokens.size() && tokens[p+1].text == "=") { named = name(); need("="); }
                        e->names.push_back(named); e->parts.push_back(expression());
                    } while (accept(","));
                    need(")");
                } else break;
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
                auto token = take();
                if (at("?")) error(source,peek(),Code::character,"The as? conversion is not supported.","Use a checked as conversion; fallible conversion syntax is deferred.");
                auto e = std::make_unique<Expr>(Expr::Kind::conversion, token);
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
            auto end = p + 1;
            while (end + 1 < tokens.size() && tokens[end].text == "." && tokens[end+1].kind == TokenKind::name) end += 2;
            auto op = tokens[end].text;
            if (op == "=" || op == "+=" || op == "-=" || op == "*=" || op == "/=" || op == "%=" || op == "++" || op == "--") {
                Statement s(Statement::Kind::assignment, name()); s.name = s.token.text; while (accept(".")) s.fields.push_back(name()); s.op = take().text;
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
            bool value_declaration = accept("value");
            if (value_declaration || at("struct")) {
                need("struct"); Structure structure; structure.token = name(); structure.visibility = f.visibility;
                structure.definition = std::make_shared<ir::ValueType>(); structure.definition->name = structure.token.text; structure.definition->managed = !value_declaration;
                if (accept("extends")) { structure.base_token=peek(); structure.base_type=type(); }
                need(":"); newline();
                bool fields=accept("<indent>");
                if (!fields && !structure.definition->managed) need("<indent>");
                while (fields && !at("<dedent>") && !at("<end of file>")) {
                    bool composition = at("using") && peek(1).text != ":";
                    if (composition) take();
                    auto start = p;
                    Statement field(Statement::Kind::variable, name()); field.name = field.token.text;
                    field.using_field = composition;
                    if (composition && !at(":")) {
                        p = start; field.annotation = type();
                        field.token = tokens[p - (field.annotation->nullable ? 2 : 1)];
                        field.name = field.token.text;
                    } else { need(":"); field.annotation = type(); }
                    auto begin = p; nodes = 0;
                    if (accept("=")) field.expression = expression();
                    for (auto n = begin; n < p; ++n) field.constant_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
                    newline(); structure.fields.push_back(std::move(field));
                }
                if (fields) need("<dedent>"); unit.structures.push_back(std::move(structure)); continue;
            }
            f.virtual_method=accept("virtual");
            f.overriding=!f.virtual_method && accept("override");
            need("func");
            if (accept("(")) {
                auto receiver = name(); auto receiver_type = type(); need(")");
                f.receiver = Parameter{receiver,receiver_type};
            }
            f.token = name(); need("(");
            if (!at(")")) do {
                bool readonly_parameter=accept("const");
                auto token = name(); need(":"); Parameter parameter{token,type()}; parameter.type.readonly=readonly_parameter;
                if (accept("=")) {
                    auto begin = p; parameter.default_begin = peek().offset; nodes = 0; parameter.default_expression = expression();
                    parameter.default_end = peek().offset;
                    for (auto n = begin; n < p; ++n) parameter.default_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
                }
                f.parameters.push_back(std::move(parameter));
            } while (accept(","));
            need(")"); if (accept("->")) f.result = type();
            auto body_begin = p; f.body = block();
            for (auto n = body_begin; n < p; ++n) f.body_syntax += std::to_string(tokens[n].text.size()) + ":" + tokens[n].text;
            f.parse_failed = diagnostics.within(source, f.token.offset, peek().offset); unit.functions.push_back(std::move(f));
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
    if (t.kind != Type::unresolved) return t;
    auto [owner, structure] = structure_lookup(unit,root,units,t.definition->name,token); (void)owner;
    auto location = token; location.offset = t.definition->source_offset; location.text = t.definition->name;
    if (!structure) error(unit.source,location,Code::unsupported_type,"Unsupported type '" + t.definition->name + "'.","Use a numeric type, bool or a visible value/managed struct.");
    if (structure->layout_state == 3) throw Poison{};
    Type result(structure->definition, structure->definition->managed ? Type::managed_ref : Type::aggregate);
    result.nullable = t.nullable; result.readonly=t.readonly;
    if (result.nullable && result.kind != Type::managed_ref) error(unit.source,token,Code::unsupported_type,"Only managed references can be nullable in this phase.");
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
        if (t.kind!=Type::aggregate && t.kind!=Type::managed_ref)
            error(source,token,calling ? Code::receiver : Code::type_mismatch,"Member access requires a struct.");
        auto visibility=[&] { error(source,token,Code::visibility,"Member '"+token.text+"' is not accessible from this module.","Import its owning module and use a public method."); };
        // All real fields/methods on the static ancestry outrank composition.
        for (unsigned n=0;n<t.definition->fields.size();++n)
            if (t.definition->fields[n].name==token.text) return Member{{n},nullptr};
        for (auto current=t.definition;current;current=current->base.lock()) {
            if (auto* f=direct_method(Type(current,t.kind),token)) {
                if (!accessible(*f)) visibility();
                return Member{{},f};
            }
        }
        auto [owner,structure]=owned_structure(t);
        std::vector<Member> candidates; std::vector<RelatedLocation> related;
        std::string routes; bool hidden=false;
        for (unsigned n=0;n<structure->effective_fields.size();++n) if (structure->effective_fields[n].second->using_field) {
            auto& embedded=*structure->effective_fields[n].second; auto& embedded_owner=*structure->effective_fields[n].first;
            auto child=ir::field_type(t,n);
            if (auto found=direct_member(child,token)) {
                if (found->function && !accessible(*found->function)) { hidden=true; continue; }
                found->path.insert(found->path.begin(),n); candidates.push_back(*found);
                auto route=embedded.name+"."+token.text;
                if (!routes.empty()) routes+=", "; routes+=route;
                related.push_back({span(embedded_owner.source,embedded.token),"Candidate through '"+route+"'."});
                auto [child_owner,child_structure]=owned_structure(child);
                const auto& declaration=found->function ? found->function->token : child_structure->effective_fields.at(found->path.back()).second->token;
                related.push_back({span(child_owner->source,declaration),"Promoted member declared here."});
            }
        }
        if (candidates.size()>1) error(source,token,Code::ambiguous,"Ambiguous promoted member '"+token.text+"': "+routes+".","Qualify the intended embedded field explicitly; types and signatures do not select a component.",std::move(related));
        if (!candidates.empty()) return candidates.front();
        if (hidden) visibility();
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
        if (e.token.text=="deinit") error(source,e.token,Code::initializer_deferred,"Resource lifecycle deinit is not implemented.");
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
    Type method_expression(Expr& e, std::optional<Type> expected) {
        bool super=e.parts[0]->kind==Expr::Kind::name && e.parts[0]->token.text=="super" && !lookup("super") && !unit.qualifiers.contains("super") && !unit.failed_qualifiers.contains("super");
        if (super) return super_expression(e);
        if ((e.token.text == "init" || e.token.text == "deinit") && !(e.parts[0]->kind==Expr::Kind::name && unit.qualifiers.contains(e.parts[0]->token.text)))
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        if (module_call(e)) {
            e.qualifier = e.parts[0]->token.text; e.qualifier_token = e.parts[0]->token;
            e.parts.erase(e.parts.begin()); e.kind = Expr::Kind::call;
            return call_expression(e);
        }
        auto receiver = expression(*e.parts[0]);
        if (!current) error(source,e.token,Code::constant_call,"A compile-time constant cannot call a method.","Use a local const for runtime values.");
        if (e.token.text == "init" || e.token.text == "deinit")
            error(source,e.token,Code::initializer_deferred,"Lifecycle methods cannot be called directly; construct a new value using its type.");
        const auto& target = [&]() -> const Function& {
            auto check_arguments = [&] { for (std::size_t n=1; n<e.parts.size(); ++n) expression(*e.parts[n]); };
            try {
                auto selected=member(receiver,e.token,true);
                if (!selected.function) error(source,e.token,Code::not_callable,"Field '"+e.token.text+"' is not callable.");
                for (auto index:selected.path) embedded_receiver(e.parts[0],index,e.token);
                receiver=e.parts[0]->type;
                return *selected.function;
            }
            catch (const CompilationError&) { check_arguments(); throw; }
            catch (const Poison&) { check_arguments(); throw; }
        }();
        e.target = target.symbol; e.virtual_slot=target.virtual_slot;
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
        if (receiver.kind==Type::managed_ref) e.receiver_addressable=true;
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
        (void)expected;
        return e.type = call_arguments(e,target,1);
    }
    Type call_arguments(Expr& e, const Function& target, std::size_t first, bool constructor = false) {
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
        return invalid ? error_type : target.result;
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
                if (field.expression || field.annotation->kind != Type::aggregate) continue;
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
        defaults(owner,structure);
        if (structure.definition->managed && !runtime) error(source,e.token,Code::constant_call,"Managed allocation is not a compile-time expression.");
        e.kind = Expr::Kind::construction; e.type = Type(structure.definition,structure.definition->managed ? Type::managed_ref : Type::aggregate); e.target = structure.definition->identity;
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
            if (t.kind == Type::managed_ref && !t.nullable && std::find(e.field_indexes.begin(),e.field_indexes.end(),n)==e.field_indexes.end())
                error(source,e.token,Code::argument_count,"A non-nullable reference field requires an explicit value: " + field.name + ".",{},{{span(field_owner.source,field.token),"Required field declared here."}});
        }
        if (!structure.initializer || !structure.definition->managed) construction_fields(e,owner,structure,runtime);
    }
    void expect(Type actual, Type expected, const Token& token) {
        if (actual == error_type || expected == error_type) return;
        if (actual.kind == Type::managed_ref && expected.kind == Type::managed_ref && ir::subtype(actual,expected) && (!actual.nullable || expected.nullable)) return;
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
        if (e.kind == K::method_call) {
            if (module_call(e)) {
                Expr qualified(K::call,e.token); qualified.qualifier = e.parts[0]->token.text; qualified.qualifier_token = e.parts[0]->token;
                return anchor(qualified);
            }
            auto receiver = anchor(*e.parts[0]);
            if (!receiver) return std::nullopt;
            auto selected=member(*receiver,e.token,true);
            return selected.function ? std::optional<Type>(selected.function->result) : std::nullopt;
        }
        if (e.kind == K::call) {
            auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
            if (structure) return Type(structure->definition,structure->definition->managed ? Type::managed_ref : Type::aggregate);
            return resolve(e).result;
        }
        if (e.kind == K::field) {
            const auto& operand = *e.parts[0];
            if (operand.kind == K::name && !lookup(operand.token.text) && unit.qualifiers.contains(operand.token.text)) {
                Expr qualified(K::name,e.token); qualified.qualifier = operand.token.text; qualified.qualifier_token = operand.token;
                auto [owner,c] = constant(qualified); if (c) { evaluate(*owner,*c); return c->type; }
                return {};
            }
            auto base = anchor(*e.parts[0]);
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
    Type contextual_conversion(Expr& e, Type type, const std::optional<Type>& expected) {
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
            auto type = expression_impl(e, expected);
            type = contextual_conversion(e,type,expected);
            if (type == error_type) { e.type = error_type; invalid_function = true; }
            return type;
        } catch (const CompilationError& failure) { diagnostics.add(failure); }
          catch (const Poison&) {}
        invalid_function = true;
        return e.type = error_type;
    }
    // Keep the recursive dispatch frame small: reserving every handler's
    // temporaries here exhausts a 1 MiB Debug stack before the depth guard.
    Type expression_impl(Expr& e, std::optional<Type> expected) {
        switch (e.kind) {
        case Expr::Kind::literal: return literal_expression(e, expected);
        case Expr::Kind::name: return name_expression(e);
        case Expr::Kind::field: return field_expression(e, expected);
        case Expr::Kind::call: return call_expression(e);
        case Expr::Kind::method_call: return method_expression(e, expected);
        case Expr::Kind::conversion: return conversion_expression(e);
        case Expr::Kind::unary: return unary_expression(e, expected);
        default: return binary_expression(e, expected);
        }
    }
    Type literal_expression(Expr& e, std::optional<Type> expected) {
        if (e.token.text == "null") {
            if (!expected || expected->kind != Type::managed_ref || !expected->nullable) error(source,e.token,Code::type_mismatch,"null requires a nullable managed-reference context.","Use a type annotation such as node?.");
            return e.type = *expected;
        }
        if (e.literal_text.empty()) return e.type;
        Type target = anchor(e).value_or(expected && numeric::number(*expected) && (!decimal(e) || numeric::floating(*expected)) ? *expected : decimal(e) ? Type::f32 : Type::i32);
        try { e.value = numeric::literal(e.literal_text,target); }
        catch (const std::invalid_argument& failure) { error(source,e.token,numeric::floating(target) ? Code::numeric_representability : Code::integer_range,failure.what()); }
        return e.type = target;
    }
    Type name_expression(Expr& e) {
        using K = Expr::Kind;
        auto* v = e.qualifier.empty() ? lookup(e.token.text) : nullptr;
        if (!v) {
            auto [owner, c] = constant(e);
            if (c) {
                evaluate(*owner, *c); e.target = owner->identity + "::" + c->declaration.name; e.kind = K::literal; e.value = c->value;
                e.aggregate_value = c->aggregate_value;
                return e.type = c->type;
            }
        }
        if (!v && unit.failed_import) throw Poison{};
        if (!v) error(source, e.token, Code::unknown_identifier, "Unknown identifier '" + e.token.text + "'.", "Declare it with var before use.");
        e.slot = v->slot; return e.type = v->type;
    }
    Type field_expression(Expr& e, std::optional<Type> expected) {
        using K = Expr::Kind;
        auto& base = *e.parts[0];
        if (base.kind == K::name && !lookup(base.token.text) && unit.qualifiers.contains(base.token.text)) {
            e.qualifier = base.token.text; e.qualifier_token = base.token; e.kind = K::name; e.parts.clear();
            return expression_impl(e,expected);
        }
        auto t = expression(base);
        if (t == error_type) return error_type;
        if (t.nullable) error(source,e.token,Code::type_mismatch,"Unwrap the nullable reference before field access.");
        if (t.kind != Type::aggregate && t.kind != Type::managed_ref) error(source,e.token,Code::type_mismatch,"Field access requires a struct.");
        auto selected=member(t,e.token);
        if (selected.function) error(source,e.token,Code::not_callable,"Method '"+e.token.text+"' requires a call.");
        for (unsigned n=0;n+1<selected.path.size();++n) embedded_receiver(e.parts[0],selected.path[n],e.token);
        auto owner=e.parts[0]->type;
        e.slot=selected.path.back(); e.target=owner.definition->fields[e.slot].owner+"::field::"+e.token.text;
        return e.type=ir::field_type(owner,e.slot);
    }
    Type call_expression(Expr& e) {
        auto [owner, structure] = structure_lookup(unit,root,units,e.qualifier.empty() ? e.token.text : e.qualifier + "." + e.token.text,e.qualifier.empty() ? e.token : e.qualifier_token);
        if (structure && !(e.qualifier.empty() && lookup(e.token.text))) {
            construction(e,*owner,*structure,current != nullptr,unit);
            return e.type;
        }
        if (std::any_of(e.names.begin(),e.names.end(),[](const auto& t) { return !t.text.empty(); })) error(source,e.token,Code::argument_count,"Named arguments are currently supported only for value construction.");
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
        return e.type = call_arguments(e,target,0);
    }
    Type conversion_expression(Expr& e) {
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
        auto from = expression(operand);
        if (from == error_type) return error_type;
        if (from.kind == Type::managed_ref && target.kind == Type::managed_ref && Type::key(from)==Type::key(target) && !target.nullable) {
            target.readonly=from.readonly; return e.type=target;
        }
        if (ir::subtype(from,target) && (!from.nullable || target.nullable)) {
            e.op="upcast"; target.readonly=from.readonly; return e.type=target;
        }
        if (from.kind==Type::managed_ref || target.kind==Type::managed_ref) error(source,e.token,Code::numeric_conversion,"Checked reference conversion requires the same nominal type or an ancestor without nullability strengthening.","Unwrap nullable references before upcasting; source downcasts and unrelated reference casts are not supported.");
        if (!numeric::number(from) || !numeric::number(target)) error(source,e.token,Code::numeric_conversion,"Numeric as requires numeric source and destination types.");
        return target;
    }
    Type unary_expression(Expr& e, std::optional<Type> expected) {
        auto left = expression(*e.parts[0], e.op == "not" ? std::optional<Type>(Type::boolean) : expected);
        if (left == error_type) return error_type;
        if (e.op == "not") expect(left,Type::boolean,e.token);
        else if (!numeric::number(left)) error(source,e.token,Code::numeric_operator,"Type mismatch: unary arithmetic requires a numeric operand.");
        return e.type = left;
    }
    Type binary_expression(Expr& e, std::optional<Type> expected) {
        using K = Expr::Kind;
        bool logical = e.op == "and" || e.op == "or";
        auto left_hint = anchor(*e.parts[0]), right_hint = anchor(*e.parts[1]);
        auto context = logical ? std::optional<Type>(Type::boolean) : left_hint ? left_hint : right_hint ? right_hint :
            expected && numeric::number(*expected) ? expected : std::optional<Type>(decimal(e) ? Type::f32 : Type::i32);
        if (context && context->kind == Type::managed_ref && (e.op=="==" || e.op=="!=")) context->nullable=true;
        auto left = expression(*e.parts[0],context);
        bool previous = suppress_zero;
        if (e.parts[0]->kind == K::literal && ((e.op == "and" && !e.parts[0]->value) || (e.op == "or" && e.parts[0]->value))) suppress_zero = true;
        auto right = expression(*e.parts[1],context); suppress_zero = previous;
        if (left == error_type || right == error_type) return error_type;
        if (logical) {
            expect(left,Type::boolean,e.parts[0]->token); expect(right,Type::boolean,e.parts[1]->token); return e.type = Type::boolean;
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
        } else expect(right,left,e.token);
        if (e.op == "==" || e.op == "!=") {
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
            if (!s.expression && t.kind == Type::managed_ref && !t.nullable) error(source,s.token,Code::initializer,"A non-nullable reference requires an initializer.","Construct the object explicitly using its type.");
            if (!s.expression && t.kind == Type::aggregate) {
                auto [owner,structure] = owned_structure(t);
                s.expression = std::make_unique<Expr>(Expr::Kind::construction,s.token);
                construction(*s.expression,*owner,*structure,true,unit);

            }
            if (t.kind!=Type::managed_ref) t.readonly=false;
            s.slot = declare(s.token, t, s.readonly); break;
        }
        case K::assignment: {
            auto* variable = lookup(s.name);
            if ((variable && variable->readonly) || (!variable && visible_names.contains(s.name)))
                error(source, s.token, Code::readonly, "Cannot mutate readonly const or declaration '" + s.name + "'.", "Use var for a mutable local binding.");
            if (!variable) error(source, s.token, Code::unknown_identifier, "Unknown identifier '" + s.name + "'.", "Declare it with var before assignment.");
            auto t = variable->type; s.slot = variable->slot;
            std::vector<Token> expanded;
            for (const auto& field : s.fields) {
                auto selected=member(t,field);
                if (selected.function) error(source,field,Code::invalid_value,"Cannot assign to method '"+field.text+"'.");
                for (unsigned n=0;n<selected.path.size();++n) {
                    auto index=selected.path[n]; auto token=field;
                    token.text=t.definition->fields[index].name;
                    expanded.push_back(token); s.implicit_fields.push_back(n+1<selected.path.size());
                    s.field_indexes.push_back(index); t=ir::field_type(t,index);
                }
            }
            s.fields=std::move(expanded);
            s.assigned_type = t;
            expect(expression(*s.expression, t), t, s.expression->token);
            if (s.op != "=" && t != error_type && !numeric::number(t)) expect(t,Type::i32,s.token);
            if (s.op == "%=" && numeric::floating(t)) error(source,s.token,Code::numeric_operator,"Illegal compound floating remainder operator.");
            if (numeric::integer(t) && (s.op == "/=" || s.op == "%=") && s.expression->kind == Expr::Kind::literal && s.expression->value == 0)
                error(source, s.expression->token, Code::zero_divisor, "Division or remainder by zero.");
            break;
        }
        case K::expression:
            if (expression(*s.expression) == error_type) break;
            if (s.expression->kind != Expr::Kind::call && s.expression->kind != Expr::Kind::method_call) error(source, s.token, Code::invalid_value, "Only function calls can be standalone expressions.", "Assign this value with var or return it.");
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
    void analyze_constant(Constant& c) {
        if (c.declaration.annotation) c.declaration.annotation = resolve_type(*c.declaration.annotation,unit,root,units,c.declaration.token);
        c.type = expression(*c.declaration.expression, c.declaration.annotation);
        if (c.type == error_type) throw Poison{};
        if (c.declaration.annotation) expect(c.type, *c.declaration.annotation, c.declaration.token);
    }
    void parameter_defaults(Function& f) {
        bool optional = false;
        for (auto& p : f.parameters) {
            if (p.default_expression) {
                if (!f.initializer) error(source,p.token,Code::initializer_deferred,"Default parameters are supported on initializers only in this phase.");
                optional = true; expect(expression(*p.default_expression,p.type),p.type,p.token);
                if (p.default_expression->type == error_type) throw Poison{};
                p.default_value = constant_value(source,*p.default_expression);
            } else if (optional) error(source,p.token,Code::argument_count,"Required parameters must precede defaulted parameters.");
        }
    }
    void initializer_fields(Function& f) {
        if (!f.initializer) return;
        auto [owner,structure] = owned_structure(f.receiver->type);
        if (f.receiver->type.kind==Type::aggregate) for (const auto& field:structure->fields) {
            auto type=*field.annotation;
            if (type.kind==Type::managed_ref && !type.nullable)
                error(source,f.token,Code::initializer,"Value init requires every field to be valid before its body; a non-nullable reference has no safe default.","Use named-field construction without init for a value with required reference fields.",{{span(owner->source,field.token),"Required reference field declared here."}});
        }
        Expr plan(Expr::Kind::construction,f.token); plan.initializer=f.symbol;
        construction_fields(plan,*owner,*structure,true,true);
        f.initialization_defaults=std::move(plan.omitted); f.initialization_value=structure->defaults;
    }
    ir::ConstantValue field_default(Statement& field) {
        auto t = *field.annotation;
        if (field.expression) {
            expect(expression(*field.expression,t),t,field.token);
            if (field.expression->type == error_type) throw Poison{};
            return constant_value(source,*field.expression);
        }
        if (t.kind != Type::aggregate) return {};
        for (const auto& [id,owner] : units) for (auto& nested : owner->structures) if (nested.definition == t.definition) {
            defaults(*owner,nested); return nested.defaults;
        }
        throw std::logic_error("Missing owned value type");
    }
    void analyze() {
        for (auto& f : unit.functions) {
            if (diagnostics.full()) break;
            current = &f; scopes = {{}}; invalid_function = false;
            auto before = diagnostics.size();
            if (f.receiver) {
                try { f.receiver->slot = declare(f.receiver->token,f.receiver->type); }
                catch (const CompilationError& failure) { diagnostics.add(failure); }
            }
            for (auto& p : f.parameters) {
                try { p.slot = declare(p.token, p.type, p.type.readonly); }
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
    const ConstructionContext* context=nullptr;
    std::vector<std::pair<unsigned, unsigned>> loops; // continue, break destinations
    unsigned block() {
        auto id = static_cast<unsigned>(output.blocks.size());
        output.blocks.push_back({{}, {ir::Terminator::Kind::unreachable}}); return id;
    }
    void terminate(ir::Terminator end) { output.blocks[*current].end = end; current.reset(); }
    void jump(unsigned to) { terminate({ir::Terminator::Kind::jump, 0, to}); }
    unsigned emit(ir::Instruction i, const Token& token) {
        i.source = source.location(token.offset);
        if (i.op == ir::Op::call || i.op == ir::Op::divide || i.op == ir::Op::remainder || i.op == ir::Op::convert || i.op == ir::Op::gc_construct || i.op == ir::Op::gc_read || i.op == ir::Op::gc_write)
            i.source_excerpt = source.excerpt(token.offset);
        auto& instructions = output.blocks[*current].instructions;
        auto id = static_cast<unsigned>(instructions.size()); instructions.push_back(std::move(i)); return id;
    }
    unsigned constant(Type t, const ir::ConstantValue& value, const Token& token) {
        ir::Instruction i{t.kind == Type::aggregate ? ir::Op::aggregate : ir::Op::constant}; i.type = t;
        if (t.kind == Type::aggregate) for (unsigned n = 0; n < t.definition->fields.size(); ++n)
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
    unsigned expression(const Expr& e) {
        using K = Expr::Kind;
        switch (e.kind) {
        case K::base_initialization: throw std::logic_error("base initialization is a dedicated prelude");
        case K::literal: {
            return constant(e.type,e.type.kind == Type::aggregate ? e.aggregate_value : ir::ConstantValue{e.value,{}},e.token);
        }
        case K::field: { auto value=expression(*e.parts[0]); ir::Instruction i{e.parts[0]->type.kind==Type::managed_ref ? ir::Op::gc_read : ir::Op::extract,value}; i.immediate=e.slot; return emit(i,e.token); }
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
                    if (field_type.kind==Type::managed_ref && !field_type.nullable) continue;
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
        case K::name: return emit({ir::Op::load, e.slot}, e.token);
        case K::method_call: case K::call: {
            std::optional<ir::Place> receiver;
            std::size_t first = 0;
            if (e.kind == K::method_call) {
                first = 1;
                if (e.parts[0]->type.kind==Type::managed_ref) {
                    auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}};
                } else if (e.receiver_addressable) receiver=place(*e.parts[0]);
                else { auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}}; }

            }
            // Materialize each argument in a slot: later short-circuit arguments
            // may create new blocks, while IR instruction values stay block-local.
            std::vector<unsigned> slots;
            for (std::size_t n = first; n < e.parts.size(); ++n) {
                const auto& arg = e.parts[n];
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(expression_type(*arg));
                auto value = expression(*arg);
                if (expression_type(*arg).construction) {
                    output.construction_origins[slot]=view_origin(value);
                }
                emit({ir::Op::store, slot, value}, arg->token); slots.push_back(slot);
            }
            ir::Instruction i{!e.virtual_slot.empty() && !e.super_call ? ir::Op::virtual_call : ir::Op::call}; i.callee = e.target; i.receiver = receiver;
            if (context) if (auto found=context->calls.find(&e);found!=context->calls.end())
                i.callee=ir::construction_name(e.target,{found->second.begin(),found->second.end()});
            for (auto slot : slots) i.arguments.push_back(emit({ir::Op::load, slot}, e.token));
            return emit(i, e.token);
        }
        case K::conversion: {
            auto value = expression(*e.parts[0]); ir::Instruction i{e.op=="upcast" ? (expression_type(e).construction ? ir::Op::stage_prefix : ir::Op::upcast) : ir::Op::convert,value}; i.type = expression_type(e); return emit(i,e.token);
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
                value = emit({operation(s.op.substr(0, 1)), old, value}, s.token);
            }
            write_place(selected,value,s.token); break;
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
    ir::Function lower(const Function& f, const ConstructionContext* construction=nullptr) {
        context=construction;
        output = {}; output.name = f.symbol; output.source_name = f.token.text; output.source = source.location(f.token.offset);
        output.parameters.clear(); output.result = f.result; output.locals = f.locals; current = block();
        output.virtual_slot=f.virtual_slot; output.overriding=f.overriding; output.visibility=f.visibility;
        if(f.receiver) output.method_name=f.token.text;
        output.receiver = f.receiver.has_value(); output.receiver_mutates = f.receiver_mutates; output.initializer = f.initializer;
        if (f.receiver) {
            output.source_name = f.receiver->type.definition->name + "::method::" + f.token.text;
            output.parameters.push_back(f.receiver->type);
        }
        for (std::size_t n = 0; n < f.parameters.size(); ++n) {
            const auto& p = f.parameters[n]; output.parameters.push_back(p.type);
            auto value = emit({ir::Op::argument, static_cast<unsigned>(n + (f.receiver ? 1 : 0))}, p.token); emit({ir::Op::store, p.slot, value}, p.token);
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
                if (t.kind==Type::managed_ref && !t.nullable) continue;
                std::optional<unsigned> selected;
                for(const auto& child:f.initialization_defaults) if(child->slot==field) selected=expression(*child);
                auto value=selected ? *selected : constant(t,f.initialization_value.fields.at(field),f.token);
                auto view=emit({ir::Op::load,0},f.token);
                ir::Instruction write{ir::Op::gc_write,view,value}; write.immediate=field; emit(write,f.token);
            }
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
    if (e.kind == Expr::Kind::literal) return e.type.kind == Type::aggregate ? e.aggregate_value : ir::ConstantValue{e.value,{}};
    if (e.kind == Expr::Kind::construction) {
        auto result = e.aggregate_value;
        for (unsigned n = 0; n < e.parts.size(); ++n) result.fields[e.field_indexes[n]] = constant_value(source,*e.parts[n]);
        for (const auto& child : e.omitted) result.fields[child->slot] = constant_value(source,*child);
        return result;
    }
    if (e.kind == Expr::Kind::field) return constant_value(source,*e.parts[0]).fields.at(e.slot);
    auto a = constant_value(source,*e.parts[0]).bits;
    try {
        if (e.kind == Expr::Kind::conversion) {
            if (e.type.kind==Type::managed_ref) { if (!a) throw std::invalid_argument("Cannot unwrap null in a compile-time expression"); return {a,{}}; }
            return {numeric::convert(a,e.parts[0]->type,e.type),{}};
        }
        if (e.kind == Expr::Kind::unary) return {e.op == "not" ? !a : e.op == "-" ? numeric::negate(a,e.type) : a,{}};
        if (e.op == "and" && !a) return {};
        if (e.op == "or" && a) return {1,{}};
        auto b = constant_value(source,*e.parts[1]).bits;
        if (e.op == "and" || e.op == "or") return {b,{}};
        if (e.parts[0]->type.kind==Type::managed_ref) return {static_cast<std::uint64_t>(e.op=="==" ? a==b : a!=b),{}};
        return {numeric::binary(operation(e.op),a,b,e.parts[0]->type),{}};
    } catch (const CompilationError&) { throw; }
      catch (const std::invalid_argument& failure) {
        error(source,e.token,e.kind == Expr::Kind::conversion ? Code::numeric_conversion : Code::zero_divisor,failure.what());
    }
}
#include "capabilities.inc"
#include "construction.inc"
#include "inheritance.inc"
void validate_graph(std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry, Diagnostics& diagnostics) {
    auto arena = std::make_shared<ir::TypeArena>();
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        structure.definition->self = structure.definition;
        structure.definition->arena = arena;
        arena->definitions.push_back(structure.definition);
    }
    // Every external resolved Type aliases this table; field edges are nonowning.

    // Discover every signature before checking any body, including cyclic imports.
    for (auto& [id, unit] : units) for (auto& f : unit->functions) {
        if (f.receiver) continue;
        f.symbol = id + "::" + f.token.text;
        auto [previous, inserted] = unit->declarations.emplace(f.token.text, &f);
        if (!inserted) {
            unit->invalid_declarations.insert(f.token.text);
            try { error(unit->source, f.token, Code::duplicate, "Duplicate function '" + f.token.text + "'.", {}, {{span(unit->source, previous->second->token), "Previously declared"}}); }
            catch (const CompilationError& failure) { diagnostics.add(failure); }
        }
    }
    auto& root = *units.at(entry);
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        auto name = structure.token.text; structure.definition->identity = id + "::" + name;
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
    std::function<void(Unit&,Structure&)> layout = [&](Unit& owner, Structure& structure) {
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
                if (field.using_field && ((t.kind!=Type::aggregate && t.kind!=Type::managed_ref) || t.nullable))
                    error(owner.source,field.token,Code::unsupported_type,"A using field requires a non-nullable nominal struct type.","Use an ordinary field for primitives or nullable references.");
                std::size_t expanded = 1;
                if (t.kind == Type::aggregate) for (auto& [id,u] : units) for (auto& child : u->structures) if (child.definition == t.definition) {
                    layout(*u,child); expanded += child.expanded_fields; structure.layout_depth=std::max(structure.layout_depth,child.layout_depth+1);
                    if(structure.layout_depth>128) error(owner.source,field.token,Code::nesting,"Physical layout depth limit exceeded.","Reduce the combined inheritance and inline-value depth; the current limit is 128.");
                }
                if (expanded > 65536 - structure.expanded_fields) error(owner.source,field.token,Code::nesting,"Expanded value layout limit exceeded.","Reduce nested value duplication; the compiler currently permits 65536 expanded fields per value.");
                structure.expanded_fields += expanded;
                field.annotation = t; structure.definition->fields.push_back({field.name,t,structure.definition->identity});
                structure.effective_fields.emplace_back(&owner,&field);
            }
            structure.layout_state = 2;
        } catch (...) { structure.layout_state = 3; throw; }
    };
    for (auto& [id,unit] : units) for (auto& structure : unit->structures) {
        try { layout(*unit,structure); }
        catch (const CompilationError& failure) { diagnostics.add(failure); }
        catch (const Poison&) {}
    }
    for (auto& [id,unit] : units) for (auto& f : unit->functions) {
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
                if ((t.kind != Type::aggregate && t.kind != Type::managed_ref) || t.nullable) error(unit->source,f.receiver->token,Code::receiver,"Receiver declarations require a non-nullable struct type.");
                if (!unit->structure_names.contains(t.definition->name) || unit->structure_names.at(t.definition->name)->definition != t.definition)
                    error(unit->source,f.receiver->token,Code::receiver,"Methods must be declared in their value type's owning module; extension methods are deferred.");
                f.initializer = f.token.text == "init";
                if (f.token.text == "deinit") error(unit->source,f.token,Code::initializer_deferred,"Resource lifecycle deinit is not implemented.");
                if (f.initializer && f.result != Type::none) error(unit->source,f.token,Code::type_mismatch,"An initializer must not declare a result; fallible construction is deferred.");
                for (const auto& field : t.definition->fields) if (field.name == f.token.text)
                    error(unit->source,f.token,Code::duplicate,"Method name conflicts with field '" + f.token.text + "'.");
                f.symbol = t.definition->identity + "::method::" + f.token.text;
                if (!unit->methods.emplace(f.symbol,&f).second) error(unit->source,f.token,Code::duplicate,"Duplicate receiver method '" + f.token.text + "'.");
                if (f.initializer) unit->structure_names.at(t.definition->name)->initializer = &f;
            } catch (const CompilationError& failure) { diagnostics.add(failure); f.parse_failed = true; }
              catch (const Poison&) { f.parse_failed = true; }
        }
    }
    validate_inheritance(units,root,diagnostics);
    // Failed layouts are poisoned and never form owning descriptor cycles.
    std::function<void(Unit&, Structure&)> defaults;
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
            Semantics(owner, units, root, evaluate, diagnostics, defaults).analyze_constant(c);
            c.aggregate_value = constant_value(owner.source, *c.declaration.expression); c.value = c.aggregate_value.bits;
            c.state = 2; dependency.pop_back();
        } catch (...) { c.state = 3; dependency.pop_back(); throw; }
    };
    defaults = [&](Unit& owner, Structure& structure) {
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
    for (auto& [id, unit] : units) Semantics(*unit, units, root, evaluate, diagnostics, defaults).analyze();
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
            std::vector<const Expr*> expressions; if (statement->expression) expressions.push_back(statement->expression.get());
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
}
ir::Module lower_graph(const std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry) {
    ir::Module module;
    for (const auto& [id,unit] : units) for (const auto& structure : unit->structures) {
        module.nominal_types.emplace_back(structure.definition,structure.definition->managed ? Type::managed_ref : Type::aggregate);
        if (structure.definition->managed) module.coordinated=true;
    }
    module.identity = entry; module.entry_symbol = entry + "::entry";
    module.source_path = units.at(entry)->source.path();
    for (auto& [id, unit] : units) {
        Lowering lowering(unit->source);
        for (const auto& f : unit->functions) {
            if (!f.initializer || f.receiver->type.kind!=Type::managed_ref) {
                auto lowered = lowering.lower(f); lowered.module_identity = id;
                module.functions.push_back(std::move(lowered));
            }
            for (const auto& [mask,context]:f.construction_contexts) {
                auto lowered=lowering.lower(f,&context); lowered.module_identity=id;
                module.functions.push_back(std::move(lowered));
            }
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
            if (d.kind != AnalyzedDeclaration::Kind::function && d.kind != AnalyzedDeclaration::Kind::receiver_method && d.kind != AnalyzedDeclaration::Kind::initializer) {
                field(result->constants_, d.identity); field(result->constants_, d.visibility);
                field(result->constants_,d.using_field ? "using" : "ordinary-field");
                field(result->constants_, d.type.kind == Type::aggregate ? d.type.definition->identity : ir::type_name(d.type)); field(result->constants_, d.constant_syntax);
            }
            result->declarations_.push_back(std::move(d));
        };
        auto reference = [&](const std::string& from, const std::string& to, const Expr& e, const Unit& u, bool implicit = false) {
            auto location = span(u.source,e.token); location.module_identity = u.identity;
            auto spelling = e.qualifier + "::" + e.token.text;
            result->references_.push_back({from,to,spelling,std::move(location),implicit});
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
            std::function<void(const Expr&,const std::string&,const std::map<unsigned,std::string>&,bool)> visit_expression;
            auto expression = [&](const Expr& e, const std::string& owner, const std::map<unsigned,std::string>& locals) { visit_expression(e,owner,locals,false); };
            visit_expression = [&](const Expr& e, const std::string& owner, const auto& locals, bool implicit) {
                if (!e.target.empty()) reference(owner,e.target,e,*unit,implicit || e.implicit);
                else if (e.kind == Expr::Kind::name && locals.contains(e.slot)) reference(owner,locals.at(e.slot),e,*unit,implicit || e.implicit);
                if(!e.virtual_slot.empty() && !e.super_call) for(const auto& [id,target_unit]:*graph) for(const auto& target:target_unit->functions)
                    if(target.virtual_slot==e.virtual_slot) reference(owner,target.symbol,e,*unit,true);
                if (!e.initializer.empty()) reference(owner,e.initializer,e,*unit,implicit);
                if ((e.kind == Expr::Kind::construction || e.kind==Expr::Kind::base_initialization) && e.initializer.empty()) for (unsigned n = 0; n < e.names.size(); ++n) {
                    Expr use(Expr::Kind::name,e.names[n]); reference(owner,e.type.definition->fields.at(e.field_indexes[n]).owner + "::field::" + e.names[n].text,use,*unit);
                }
                for (const auto& part : e.parts) visit_expression(*part,owner,locals,implicit);
                for (const auto& part : e.omitted) visit_expression(*part,owner,locals,true);
            };
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
                for (const auto& member : structure.fields) {
                    auto t = *member.annotation; auto identity = key + "::field::" + member.name;
                    field(layout,member.using_field ? "using" : "ordinary-field"); field(layout,member.name); field(layout,t.kind == Type::aggregate ? t.definition->identity : ir::type_name(t)); field(layout,member.constant_syntax);
                    AnalyzedDeclaration record{structure.definition->managed ? AnalyzedDeclaration::Kind::managed_field : AnalyzedDeclaration::Kind::value_field,identity,id,structure.visibility,location(member.token),{},t,member.constant_syntax,{},key};
                    record.using_field=member.using_field; declaration(std::move(record));
                    if (member.expression) expression(*member.expression,identity,{});
                }
                AnalyzedDeclaration record{structure.definition->managed ? AnalyzedDeclaration::Kind::managed_structure : AnalyzedDeclaration::Kind::value_structure,key,id,structure.visibility,location(structure.token),{},Type(structure.definition,structure.definition->managed ? Type::managed_ref : Type::aggregate),layout,{},id};
                if(auto base=structure.definition->base.lock()) record.base_identity=base->identity;
                declaration(std::move(record));
            }
            for (const auto& c : unit->constants) {
                auto key = id + "::" + c.declaration.name;
                declaration({AnalyzedDeclaration::Kind::module_constant,key,id,c.visibility,location(c.declaration.token),{},c.type,c.declaration.constant_syntax,c.type.kind == Type::aggregate ? std::optional<std::uint64_t>{} : std::optional<std::uint64_t>{c.value}});
                expression(*c.declaration.expression,key,{});
            }
            for (const auto& f : unit->functions) {
                std::vector<Type> parameters; for (const auto& p : f.parameters) parameters.push_back(p.type);
                AnalyzedDeclaration method_record{f.initializer ? AnalyzedDeclaration::Kind::initializer : f.receiver ? AnalyzedDeclaration::Kind::receiver_method : AnalyzedDeclaration::Kind::function,f.symbol,id,f.visibility,location(f.token),parameters,f.result,{}};
                if (f.receiver) { method_record.owner = f.receiver->type.definition->identity; method_record.receiver = f.receiver->type; method_record.receiver_mutates = f.receiver_mutates; }
                method_record.initialization_contract=f.initialization_contract;
                method_record.virtual_slot=f.virtual_slot;
                if(f.overriding) method_record.override_target=f.virtual_slot;
                method_record.capability_contract=f.capability_contract;
                for (const auto& [mask,context]:f.construction_contexts)
                    method_record.construction_parameters.emplace_back(mask.begin(),mask.end());
                if (f.initializer && f.receiver->type.kind==Type::managed_ref)
                    for (const auto& member:f.receiver->type.definition->fields) {
                        Type t=member.type;
                        if (t.kind==Type::managed_ref && !t.nullable) method_record.required_initialization_fields.push_back(member.name);
                    }
                if (f.initializer) for (const auto& p : f.parameters) {
                    AnalyzedParameter parameter{p.token.text,p.type,location(p.token)};
                    if (p.default_expression) { auto range=location(p.default_expression->token); range.begin=p.default_begin; range.end=p.default_end; parameter.default_source=std::move(range); }
                    method_record.initializer_parameters.push_back(std::move(parameter));
                }
                declaration(std::move(method_record));
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
                        if (s.expression) expression(*s.expression,owner,locals);
                        if (s.readonly) locals.emplace(s.slot,owner);
                        body(s.body); body(s.otherwise);
                    }
                };
                if(f.base_prelude) expression(*f.base_prelude,f.symbol,locals);
                for(const auto& child:f.initialization_defaults) visit_expression(*child,f.symbol,locals,true);
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
    return AnalysisAccess::build(std::move(units), identity);
}
std::shared_ptr<const ValidatedAnalysis> validated_project(const std::filesystem::path& entry, const SourceOverrides& overrides, bool verify_disk) {
    for (const auto& [id, source] : overrides) { (void)source; require_module_identity(id); }
    std::error_code ec;
    auto absolute = std::filesystem::absolute(entry, ec);
    if (ec) error(SourceFile(path_utf8(entry), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the entry source path.");
    auto root = std::filesystem::canonical(absolute.parent_path(), ec);
    if (ec) error(SourceFile(path_utf8(entry), ""), {TokenKind::end, "", 0}, Code::import_read, "Cannot resolve the project root directory.", "Check that the entry's parent directory exists and is readable.");
    // A diagnostic anchor is needed even before the entry file can be opened.
    Unit anchor(SourceFile(path_utf8(absolute), ""), "/");
    Diagnostics diagnostics; diagnostics.bind(anchor.source, "/" + path_utf8(absolute.filename()));
    SourceGraph graph(root, diagnostics, overrides);
    auto& first = [&]() -> Unit& {
        try { return graph.load(anchor, {TokenKind::path, "/" + path_utf8(absolute.filename()), 0}); }
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
