#include "void/frontend.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
namespace vd {
namespace {
using Type = ir::Type;
enum class TokenKind { name, number, path, symbol, newline, indent, dedent, end };
struct Token { TokenKind kind; std::string text; std::size_t offset; };
[[noreturn]] void error(const SourceFile& source, const Token& token, const std::string& message,
                        const std::string& hint = {}) {
    throw std::invalid_argument(source.diagnostic(token.offset, message + (hint.empty() ? "" : " Hint: " + hint)));
}
bool letter(char c) { return (c >= 'a' && c <= 'z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
const std::set<std::string> reserved = {"func", "var", "return", "if", "else", "for", "break", "continue",
    "and", "or", "not", "true", "false", "while", "const", "use", "struct", "in", "reverse", "i32", "bool", "as", "private", "public", "internal"};
std::vector<Token> lex(const SourceFile& source) {
    const auto& text = source.text();
    std::vector<Token> tokens;
    std::vector<std::size_t> indents{0};
    std::size_t p = text.starts_with("\xef\xbb\xbf") ? 3 : 0;
    bool beginning = true;
    auto emit = [&](TokenKind kind, std::string value, std::size_t offset) { tokens.push_back({kind, std::move(value), offset}); };
    while (p < text.size()) {
        if (beginning) {
            auto start = p;
            while (p < text.size() && text[p] == ' ') ++p;
            if (p < text.size() && text[p] == '\t') error(source, {TokenKind::symbol, "", p}, "Tabs are not supported in source indentation.", "Use spaces consistently (four spaces recommended).");
            bool blank = p == text.size() || text[p] == '#' || text[p] == '\r' || text[p] == '\n';
            if (!blank) {
                auto width = p - start;
                if (width > indents.back()) { indents.push_back(width); emit(TokenKind::indent, "<indent>", p); }
                else if (width < indents.back()) {
                    while (width < indents.back()) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
                    if (width != indents.back()) error(source, {TokenKind::symbol, "", p}, "Indentation does not match an enclosing block.", "Align this statement with an earlier block level.");
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
            if (p == text.size() || text[p] != '"') error(source, {TokenKind::path, "", start}, "Unterminated import path.", "Close the path with a double quote on this line.");
            emit(TokenKind::path, text.substr(start + 1, p - start - 1), start); ++p; continue;
        }
        if (letter(c)) {
            while (p < text.size() && (letter(text[p]) || digit(text[p]))) ++p;
            emit(TokenKind::name, text.substr(start, p - start), start); continue;
        }
        if (digit(c)) {
            while (p < text.size() && digit(text[p])) ++p;
            if (p < text.size() && (letter(text[p]) || text[p] == '.' || (text[p] >= 'A' && text[p] <= 'Z')))
                error(source, {TokenKind::number, "", start}, "Unsupported numeric literal.", "This subset accepts decimal i32 integers without suffixes or separators; ranges and floating values are not implemented.");
            emit(TokenKind::number, text.substr(start, p - start), start); continue;
        }
        std::string pair = text.substr(p, 2);
        if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" || pair == ">=" || pair == "+=" ||
            pair == "-=" || pair == "*=" || pair == "/=" || pair == "%=" || pair == "++" || pair == "--") {
            p += 2; emit(TokenKind::symbol, pair, start); continue;
        }
        if (std::string("():,.+-*/%=<>").find(c) != std::string::npos) { ++p; emit(TokenKind::symbol, std::string(1, c), start); continue; }
        error(source, {TokenKind::symbol, "", p}, "Unexpected character in source.",
              c == '\t' ? "Use spaces instead of tabs." : "Use the supported i32/bool syntax and lowercase ASCII identifiers; Unicode identifiers and strings are not implemented yet.");
    }
    if (!tokens.empty() && tokens.back().kind != TokenKind::newline) emit(TokenKind::newline, "<newline>", p);
    while (indents.size() > 1) { indents.pop_back(); emit(TokenKind::dedent, "<dedent>", p); }
    emit(TokenKind::end, "<end of file>", p);
    return tokens;
}
struct Expr {
    enum class Kind { literal, name, call, unary, binary } kind;
    Token token;
    Token qualifier_token{TokenKind::name, "", 0};
    std::string op, qualifier, target;
    std::int64_t value = 0;
    std::vector<std::unique_ptr<Expr>> parts;
    Type type = Type::none;
    unsigned slot = 0;
    Expr(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Statement {
    enum class Kind { variable, assignment, expression, returned, conditional, loop, broken, continued } kind;
    Token token;
    std::string name, op;
    std::optional<Type> annotation;
    std::unique_ptr<Expr> expression;
    std::vector<Statement> body, otherwise;
    unsigned slot = 0, flow = 0;
    Statement(Kind k, Token t) : kind(k), token(std::move(t)) {}
};
struct Parameter { Token token; Type type; unsigned slot = 0; };
struct Function {
    Token token;
    bool file_private = false;
    std::string symbol;
    std::vector<Parameter> parameters;
    Type result = Type::none;
    std::vector<Statement> body;
    std::vector<Type> locals;
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
    std::map<std::string, Function*> declarations;
    std::map<std::string, std::vector<Unit*>> qualifiers;
    std::set<std::string> explicit_qualifiers;
    Unit(SourceFile s, std::string id) : source(std::move(s)), identity(std::move(id)) {}
};
class Parser {
    const SourceFile& source;
    std::vector<Token> tokens;
    std::size_t p = 0;
    const Token& peek() const { return tokens[p]; }
    bool at(const std::string& text) const { return peek().text == text; }
    Token take() { return tokens[p++]; }
    bool accept(const std::string& text) { if (!at(text)) return false; ++p; return true; }
    Token need(const std::string& text) {
        if (!at(text)) {
            if (text == "<indent>") error(source, peek(), "Expected an indented block.", "Indent the body with spaces beneath its declaration or control-flow statement.");
            if (text == "<newline>") error(source, peek(), "Expected end of statement, found '" + peek().text + "'.", "Put each statement on its own line; postfix ++/-- cannot be used as values.");
            error(source, peek(), "Expected '" + text + "', found '" + peek().text + "'.");
        }
        return take();
    }
    Token name() {
        if (peek().kind != TokenKind::name || reserved.contains(peek().text))
            error(source, peek(), "Expected a lowercase identifier.");
        return take();
    }
    Type type() {
        if (accept("i32")) return Type::i32;
        if (accept("bool")) return Type::boolean;
        error(source, peek(), "Unsupported type '" + peek().text + "'.", "Use i32 or bool in this compiler subset; omit the return annotation for no result.");
    }
    void newline() { need("<newline>"); }
    std::vector<Statement> block() {
        need(":"); newline(); need("<indent>");
        std::vector<Statement> body;
        while (peek().kind != TokenKind::dedent && peek().kind != TokenKind::end) body.push_back(statement());
        if (body.empty()) error(source, peek(), "Expected an indented statement.");
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
        auto token = take();
        if (token.kind == TokenKind::number || token.text == "true" || token.text == "false") {
            auto e = std::make_unique<Expr>(Expr::Kind::literal, token);
            if (token.kind == TokenKind::number) {
                auto result = std::from_chars(token.text.data(), token.text.data() + token.text.size(), e->value);
                if (result.ec != std::errc{}) error(source, token, "Integer literal is too large.", "Use a value in the i32 range.");
                e->type = Type::i32;
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
                if (!at("(")) error(source, e->token, "Only module-qualified function calls are supported.");
            }
            if (accept("(")) {
                e->kind = Expr::Kind::call;
                if (!at(")")) do { e->parts.push_back(expression()); } while (accept(","));
                need(")");
            }
            return e;
        }
        error(source, token, "Expected an expression, found '" + token.text + "'.");
    }
    std::unique_ptr<Expr> unary() {
        if (at("not") || at("-") || at("+")) {
            auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::unary, token); e->op = token.text;
            // not binds below comparisons: not a < b means not (a < b).
            e->parts.push_back(token.text == "not" ? expression(3) : unary());
            if (token.text == "-" && e->parts[0]->kind == Expr::Kind::literal && e->parts[0]->type == Type::i32) {
                e->kind = Expr::Kind::literal; e->type = Type::i32; e->value = -e->parts[0]->value; e->parts.clear();
            }
            return e;
        }
        return primary();
    }
    std::unique_ptr<Expr> expression(int minimum = 1) {
        auto left = unary();
        while (precedence(peek().text) >= minimum) {
            auto token = take(); auto e = std::make_unique<Expr>(Expr::Kind::binary, token); e->op = token.text;
            e->parts.push_back(std::move(left)); e->parts.push_back(expression(precedence(token.text) + 1));
            left = std::move(e);
        }
        return left;
    }
    Statement conditional(Token token) {
        Statement s(Statement::Kind::conditional, std::move(token));
        s.expression = expression(); s.body = block();
        if (accept("else")) {
            if (at("if")) { auto next = take(); s.otherwise.push_back(conditional(next)); }
            else s.otherwise = block();
        }
        return s;
    }
    Statement statement() {
        if (at("while")) error(source, peek(), "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:'.");
        if (at("if")) return conditional(take());
        if (at("for")) {
            Statement s(Statement::Kind::loop, take());
            if (!at(":")) s.expression = expression();
            if (at("in") || at(",")) error(source, peek(), "Collection/range iteration is not implemented.", "Use 'for condition:' or 'for:' for this subset.");
            s.body = block(); return s;
        }
        if (at("var")) {
            Statement s(Statement::Kind::variable, take()); auto n = name(); s.name = n.text; s.token = n;
            if (accept(":")) s.annotation = type();
            if (accept("=")) s.expression = expression();
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
                    s.expression = std::make_unique<Expr>(Expr::Kind::literal, s.token); s.expression->value = 1; s.expression->type = Type::i32;
                } else s.expression = expression();
                newline(); return s;
            }
        }
        Statement s(Statement::Kind::expression, peek()); s.expression = expression(); newline(); return s;
    }
public:
    explicit Parser(const SourceFile& s) : source(s), tokens(lex(s)) {}
    void parse(Unit& unit) {
        while (peek().kind != TokenKind::end) {
            if (accept("<newline>")) continue;
            if (at("while")) error(source, peek(), "VOID uses 'for' for all loops.", "Replace 'while condition:' with 'for condition:' inside a function.");
            if (accept("use")) {
                if (!unit.functions.empty()) error(source, peek(), "Imports must precede function declarations.", "Move use declarations to the start of this file.");
                if (peek().kind != TokenKind::path) error(source, peek(), "Expected a quoted import path.", "Use \"/folder/file.void\" or \"./file.void\".");
                Import import; import.path = take(); import.alias = import.path;
                if (accept("as")) { import.alias = name(); import.explicit_alias = true; }
                newline(); unit.imports.push_back(std::move(import)); continue;
            }
            Function f;
            if (accept("private")) f.file_private = true;
            else if (!accept("public")) accept("internal"); // All graph files belong to this project.
            need("func"); f.token = name(); need("(");
            if (!at(")")) do { auto token = name(); need(":"); f.parameters.push_back({token, type()}); } while (accept(","));
            need(")"); if (accept("->")) f.result = type(); f.body = block(); unit.functions.push_back(std::move(f));
        }
    }
};
// Flow sets describe possible exits from a statement, independently of LLVM.
constexpr unsigned next_flow = 1, return_flow = 2, break_flow = 4, continue_flow = 8;
bool always(const Expr* e) { return !e || (e->kind == Expr::Kind::literal && e->type == Type::boolean && e->value == 1); }
class Semantics {
    const SourceFile& source;
    Unit& unit;
    const std::map<std::string, std::unique_ptr<Unit>>& units;
    std::set<std::string> visible_names;
    struct Variable { Type type; unsigned slot; };
    std::vector<std::map<std::string, Variable>> scopes;
    Function* current = nullptr;
    unsigned loops = 0;
    Variable* lookup(const std::string& name) {
        for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
            auto found = scope->find(name); if (found != scope->end()) return &found->second;
        }
        return nullptr;
    }
    const Function& resolve(const Expr& e) {
        if (!e.qualifier.empty()) {
            auto module = unit.qualifiers.find(e.qualifier);
            if (module == unit.qualifiers.end()) error(source, e.qualifier_token, "Unknown import qualifier '" + e.qualifier + "'.", "Import the module with this alias before calling it.");
            if (module->second.size() != 1) error(source, e.qualifier_token, "Ambiguous import qualifier '" + e.qualifier + "'.", "Assign distinct explicit aliases with as to modules sharing a filename stem.");
            auto* target = module->second.front();
            auto found = target->declarations.find(e.token.text);
            if (found == target->declarations.end()) error(source, e.token, "Unknown imported function '" + e.qualifier + "." + e.token.text + "'.");
            if (found->second->file_private && target != &unit)
                error(source, e.token, "Function '" + e.token.text + "' is private to " + target->identity + ".", "Call a public function from that file instead.");
            return *found->second;
        }
        auto own = unit.declarations.find(e.token.text);
        if (own != unit.declarations.end()) return *own->second;
        std::map<std::string, Function*> candidates;
        bool hidden = false;
        for (const auto& import : unit.imports) if (!import.explicit_alias) {
            auto found = units.at(import.target)->declarations.find(e.token.text);
            if (found != units.at(import.target)->declarations.end()) {
                if (found->second->file_private) hidden = true;
                else candidates.emplace(found->second->symbol, found->second);
            }
        }
        if (candidates.size() > 1) {
            std::string owners;
            for (const auto& [id, f] : candidates) { (void)f; if (!owners.empty()) owners += ", "; owners += id.substr(0, id.rfind("::")); }
            error(source, e.token, "Ambiguous imported function '" + e.token.text + "' from " + owners + ".", "Use a module qualifier or import as an explicit alias.");
        }
        if (!candidates.empty()) return *candidates.begin()->second;
        if (hidden) error(source, e.token, "Function '" + e.token.text + "' is private to its declaring file.", "Call a public function instead.");
        error(source, e.token, "Unknown function '" + e.token.text + "'.", "Declare it here or import its declaring file; imports are not transitive exports.");
    }
    void expect(Type actual, Type expected, const Token& token) {
        if (actual != expected) error(source, token, "Type mismatch: expected " + std::string(ir::type_name(expected)) + ", received " + ir::type_name(actual) + ".");
    }
    unsigned declare(const Token& token, Type type) {
        if (lookup(token.text) || visible_names.contains(token.text)) error(source, token, "Duplicate declaration or shadowing of '" + token.text + "'.", "Assign to the existing variable, or choose a distinct name.");
        auto slot = static_cast<unsigned>(current->locals.size()); current->locals.push_back(type);
        scopes.back().emplace(token.text, Variable{type, slot}); return slot;
    }
    Type expression(Expr& e) {
        using K = Expr::Kind;
        if (e.kind == K::literal) {
            if (e.type == Type::i32 && (e.value < std::numeric_limits<std::int32_t>::min() || e.value > std::numeric_limits<std::int32_t>::max()))
                error(source, e.token, "Integer literal is outside the i32 range.", "Use a value from -2147483648 through 2147483647.");
            return e.type;
        }
        if (e.kind == K::name) {
            auto* v = lookup(e.token.text);
            if (!v) error(source, e.token, "Unknown identifier '" + e.token.text + "'.", "Declare it with var before use.");
            e.slot = v->slot; return e.type = v->type;
        }
        if (e.kind == K::call) {
            const auto& target = resolve(e);
            e.target = target.symbol;
            if (e.parts.size() != target.parameters.size()) error(source, e.token, "Incorrect argument count for '" + e.token.text + "': expected " + std::to_string(target.parameters.size()) + ", received " + std::to_string(e.parts.size()) + ".");
            for (std::size_t i = 0; i < e.parts.size(); ++i) expect(expression(*e.parts[i]), target.parameters[i].type, e.parts[i]->token);
            return e.type = target.result;
        }
        auto left = expression(*e.parts[0]);
        if (e.kind == K::unary) {
            expect(left, e.op == "not" ? Type::boolean : Type::i32, e.token); return e.type = left;
        }
        auto right = expression(*e.parts[1]);
        if (e.op == "and" || e.op == "or") {
            expect(left, Type::boolean, e.parts[0]->token); expect(right, Type::boolean, e.parts[1]->token); return e.type = Type::boolean;
        }
        if (e.op == "==" || e.op == "!=") {
            if (left == Type::none) error(source, e.token, "Cannot compare a no-result call.");
            expect(right, left, e.token); return e.type = Type::boolean;
        }
        expect(left, Type::i32, e.parts[0]->token); expect(right, Type::i32, e.parts[1]->token);
        if ((e.op == "/" || e.op == "%") && e.parts[1]->kind == K::literal && e.parts[1]->value == 0)
            error(source, e.parts[1]->token, "Division or remainder by zero.", "Use a nonzero divisor.");
        return e.type = (e.op == "<" || e.op == "<=" || e.op == ">" || e.op == ">=") ? Type::boolean : Type::i32;
    }
    unsigned sequence(std::vector<Statement>& body, bool nested = true) {
        if (nested) scopes.emplace_back();
        unsigned flow = next_flow;
        for (auto& s : body) {
            auto result = statement(s);
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
            Type t = s.expression ? expression(*s.expression) : s.annotation.value_or(Type::none);
            if (t == Type::none) error(source, s.token, "Variable requires a value type.", "Supply an i32/bool annotation or a value-producing initializer.");
            if (s.annotation) expect(t, *s.annotation, s.token);
            s.slot = declare(s.token, t); break;
        }
        case K::assignment: {
            auto* variable = lookup(s.name);
            if (!variable) error(source, s.token, "Unknown identifier '" + s.name + "'.", "Declare it with var before assignment.");
            auto t = variable->type; s.slot = variable->slot;
            expect(expression(*s.expression), t, s.expression->token);
            if (s.op != "=") expect(t, Type::i32, s.token);
            if ((s.op == "/=" || s.op == "%=") && s.expression->kind == Expr::Kind::literal && s.expression->value == 0)
                error(source, s.expression->token, "Division or remainder by zero.");
            break;
        }
        case K::expression:
            expression(*s.expression);
            if (s.expression->kind != Expr::Kind::call) error(source, s.token, "Only function calls can be standalone expressions.", "Assign this value with var or return it.");
            break;
        case K::returned:
            expect(s.expression ? expression(*s.expression) : Type::none, current->result, s.token);
            // A no-result call is a statement, not a return value.
            if (s.expression && current->result == Type::none) error(source, s.token, "A no-result function cannot return an expression.", "Call the function, then use an empty return.");
            flow = return_flow; break;
        case K::conditional:
            expect(expression(*s.expression), Type::boolean, s.expression->token);
            flow = sequence(s.body) | (s.otherwise.empty() ? next_flow : sequence(s.otherwise)); break;
        case K::loop: {
            if (s.expression) expect(expression(*s.expression), Type::boolean, s.expression->token);
            ++loops; auto body = sequence(s.body); --loops;
            flow = body & return_flow;
            if (!always(s.expression.get()) || (body & break_flow)) flow |= next_flow;
            break;
        }
        case K::broken: case K::continued:
            if (!loops) error(source, s.token, s.token.text + " is only valid inside a for loop.");
            flow = s.kind == K::broken ? break_flow : continue_flow; break;
        }
        return s.flow = flow;
    }
public:
    Semantics(Unit& u, const std::map<std::string, std::unique_ptr<Unit>>& graph)
        : source(u.source), unit(u), units(graph) {
        for (const auto& [name, f] : unit.declarations) { (void)f; visible_names.insert(name); }
        for (const auto& [name, target] : unit.qualifiers) { (void)target; visible_names.insert(name); }
        for (const auto& import : unit.imports) if (!import.explicit_alias)
            for (const auto& [name, f] : units.at(import.target)->declarations)
                if (!f->file_private) visible_names.insert(name);
    }
    void analyze() {
        for (auto& f : unit.functions) {
            current = &f; scopes = {{}};
            for (auto& p : f.parameters) p.slot = declare(p.token, p.type);
            auto flow = sequence(f.body, false);
            if (f.result != Type::none && (flow & next_flow)) error(source, f.token, "Missing return path in '" + f.token.text + "'.", "Return a " + std::string(ir::type_name(f.result)) + " on every path that reaches the end.");
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
        auto& instructions = output.blocks[*current].instructions;
        auto id = static_cast<unsigned>(instructions.size()); instructions.push_back(std::move(i)); return id;
    }
    static ir::Op operation(const std::string& op) {
        static const std::map<std::string, ir::Op> ops = {{"+", ir::Op::add}, {"-", ir::Op::subtract}, {"*", ir::Op::multiply},
            {"/", ir::Op::divide}, {"%", ir::Op::remainder}, {"<", ir::Op::less}, {"<=", ir::Op::less_equal},
            {">", ir::Op::greater}, {">=", ir::Op::greater_equal}, {"==", ir::Op::equal}, {"!=", ir::Op::not_equal}};
        return ops.at(op);
    }
    unsigned expression(const Expr& e) {
        using K = Expr::Kind;
        switch (e.kind) {
        case K::literal: {
            ir::Instruction i{ir::Op::constant}; i.immediate = static_cast<std::int32_t>(e.value); i.type = e.type; return emit(i, e.token);
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
        case K::unary: {
            auto value = expression(*e.parts[0]);
            if (e.op == "not") return emit({ir::Op::logical_not, value}, e.token);
            if (e.op == "+") return value;
            auto zero = emit({ir::Op::constant}, e.token); return emit({ir::Op::subtract, zero, value}, e.token);
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
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(Type::i32); old_slot = slot;
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
    if (invalid) error(source, at, "Non-portable module path component '" + component + "'.",
        "Use printable ASCII filenames without Windows-reserved characters/names or trailing dots/spaces; Unicode module filenames are deferred.");
}
std::string normalize(const Unit& from, const Token& path) {
    const auto& text = path.text;
    if (text.empty() || text.find('\\') != std::string::npos)
        error(from.source, path, "Invalid import path.", "Use a nonempty VFS path with / separators on every platform.");
    bool absolute = text[0] == '/';
    if (text.starts_with("//")) error(from.source, path, "Invalid root-relative import path.", "A single leading / denotes the virtual project root, not an OS or network path.");
    if (!absolute && !text.starts_with("./") && !text.starts_with("../") && text.find(':') != std::string::npos)
        error(from.source, path, "OS filesystem paths are not valid VOID imports.", "Use a project-root /path.void or importer-relative ./path.void instead of a drive path.");
    if (!absolute && !text.starts_with("./") && !text.starts_with("../"))
        error(from.source, path, "Package imports are not implemented.", "Use /project/file.void, ./file.void or ../file.void; OS drive paths are not VFS paths.");
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
            if (parts.empty()) error(from.source, path, "Import escapes the project root.", "External imports require an explicit permission/identity contract and are not enabled in this phase.");
            parts.pop_back();
        } else if (!component.empty() && component != ".") {
            portable_component(from.source, path, component); parts.push_back(std::move(component));
        }
        if (end == text.size()) break;
        begin = end + 1;
    }
    if (parts.empty() || text.back() == '/' || !parts.back().ends_with(".void"))
        error(from.source, path, "Import must name a .void source file.", "Supply an exact-case filename, not a directory.");
    std::string id;
    for (const auto& component : parts) id += "/" + component;
    return id;
}
class SourceReadError : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};
class SourceGraph {
    std::filesystem::path root;
public:
    std::map<std::string, std::unique_ptr<Unit>> units;
    explicit SourceGraph(std::filesystem::path project_root) : root(std::move(project_root)) {}
    Unit& load(const Unit& from, const Token& path) {
        auto id = normalize(from, path);
        if (auto found = units.find(id); found != units.end()) return *found->second;
        auto physical = root;
        std::error_code ec;
        for (const auto& part : std::filesystem::path(id).relative_path()) {
            auto spelling = part.generic_string();
            std::vector<std::string> matches;
            for (std::filesystem::directory_iterator it(physical, ec), end; !ec && it != end; it.increment(ec)) {
                auto candidate = it->path().filename().generic_string();
                if (folded(candidate) == folded(spelling)) matches.push_back(candidate);
            }
            if (ec) error(from.source, path, "Cannot inspect import directory for '" + id + "'.", "Check that each parent exists, is a readable directory and uses the exact filename case.");
            if (matches.size() > 1) error(from.source, path, "Filesystem case collision for '" + spelling + "'.", "Rename case-only siblings so this project has the same identity on every platform.");
            if (matches.empty()) error(from.source, path, "Cannot find imported file '" + id + "'.", "Check this path relative to the importing file or virtual project root.");
            if (matches.front() != spelling) error(from.source, path, "Import path case mismatch: requested '" + spelling + "', found '" + matches.front() + "'.", "Use the exact on-disk spelling on every platform.");
            physical /= part;
            auto status = std::filesystem::symlink_status(physical, ec);
            if (ec) error(from.source, path, "Cannot inspect imported path '" + id + "'.");
            if (std::filesystem::is_symlink(status)) error(from.source, path, "Symbolic links are not supported in module paths.", "Import a regular project file; symlink identity and external permissions are deferred.");
        }
        if (!std::filesystem::is_regular_file(physical, ec) || ec)
            error(from.source, path, "Imported path is not a regular .void source file.");
        auto resolved = std::filesystem::canonical(physical, ec);
        if (ec || resolved != physical)
            error(from.source, path, "Module path contains a symbolic link or filesystem alias.", "Use regular project paths; junctions and redirected module paths are not supported.");
        for (const auto& [other_id, other] : units) {
            if (std::filesystem::equivalent(physical, other->source.path(), ec) && !ec)
                error(from.source, path, "Hard-link aliases have conflicting module identities: '" + id + "' and '" + other_id + "'.", "Use one VFS filename for this physical source.");
            ec.clear();
        }
        // Publish before traversing imports: function-only declaration cycles are safe.
        std::unique_ptr<Unit> unit;
        try { unit = std::make_unique<Unit>(read_source(physical), id); }
        catch (const SourceReadError&) { error(from.source, path, "Cannot read imported file '" + id + "'.", "Check source file permissions and storage availability."); }
        auto* result = unit.get(); units.emplace(id, std::move(unit));
        Parser(result->source).parse(*result);
        for (auto& import : result->imports) import.target = load(*result, import.path).identity;
        return *result;
    }
};
ir::Module compile_graph(std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry) {
    // Discover every signature before checking any body, including cyclic imports.
    for (auto& [id, unit] : units) for (auto& f : unit->functions) {
        f.symbol = id + "::" + f.token.text;
        if (!unit->declarations.emplace(f.token.text, &f).second)
            error(unit->source, f.token, "Duplicate function '" + f.token.text + "'.");
    }
    for (auto& [id, unit] : units) {
        (void)id;
        for (const auto& import : unit->imports) {
            auto alias = import.explicit_alias ? import.alias.text : std::filesystem::path(import.target).stem().generic_string();
            if (!import.explicit_alias && !identifier(alias)) continue;
            if (import.explicit_alias && unit->declarations.contains(alias))
                error(unit->source, import.alias, "Import alias '" + alias + "' conflicts with a function declaration.", "Choose a distinct explicit alias with as.");
            auto* target = units.at(import.target).get();
            auto& targets = unit->qualifiers[alias];
            bool repeated = std::find(targets.begin(), targets.end(), target) != targets.end();
            bool different = std::any_of(targets.begin(), targets.end(), [&](Unit* previous) { return previous != target; });
            if (different && (import.explicit_alias || unit->explicit_qualifiers.contains(alias)))
                error(unit->source, import.alias, "Conflicting import alias '" + alias + "'.", "Give these modules different aliases using as.");
            if (import.explicit_alias) unit->explicit_qualifiers.insert(alias);
            if (!repeated) targets.push_back(target);
        }
    }
    ir::Module module; module.identity = entry; module.entry_symbol = entry + "::entry";
    module.source_path = units.at(entry)->source.path();
    for (auto& [id, unit] : units) {
        Semantics(*unit, units).analyze();
        Lowering lowering(unit->source);
        for (const auto& f : unit->functions) {
            auto lowered = lowering.lower(f); lowered.module_identity = id;
            module.functions.push_back(std::move(lowered));
        }
    }
    ir::validate(module); return module;
}

}
SourceFile read_source(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw SourceReadError(path.generic_string() + ": error: Cannot read source file.\n");
    std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) throw SourceReadError(path.generic_string() + ": error: Cannot finish reading source file.\n");
    return {path.generic_string(), std::move(text)};
}
ir::Module compile_source(const SourceFile& source) {
    std::map<std::string, std::unique_ptr<Unit>> units;
    auto unit = std::make_unique<Unit>(source, source.path());
    Parser(source).parse(*unit);
    if (!unit->imports.empty()) error(source, unit->imports.front().path, "Imports need a filesystem project context.", "Use compile_project or the check/run/emit CLI with a source filename.");
    units.emplace(source.path(), std::move(unit));
    return compile_graph(units, source.path());
}
SourceProgram compile_project(const std::filesystem::path& entry) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(entry, ec);
    if (ec) throw std::invalid_argument("Cannot resolve the entry source path.");
    auto root = std::filesystem::canonical(absolute.parent_path(), ec);
    if (ec) throw std::invalid_argument(entry.generic_string() + ": error: Cannot resolve the project root directory.\n");
    // A diagnostic anchor is needed even before the entry file can be opened.
    Unit anchor(SourceFile(absolute.generic_string(), ""), "/");
    SourceGraph graph(root);
    auto& first = graph.load(anchor, {TokenKind::path, "/" + absolute.filename().generic_string(), 0});
    auto module = compile_graph(graph.units, first.identity);
    return {first.source, std::move(module)};
}
void require_source_entry(const SourceFile& source, const ir::Module& module) {
    for (const auto& f : module.functions) if (f.name == module.entry_symbol) {
        if (f.parameters == std::vector<Type>{Type::i32} && f.result == Type::i32) return;
        std::size_t offset = 0;
        while (offset < source.text().size() && source.location(offset).line < f.source.line) ++offset;
        while (offset < source.text().size() && source.location(offset).column < f.source.column) ++offset;
        error(source, {TokenKind::name, "entry", offset}, "The CLI entry point must have signature entry(value: i32) -> i32.", "Other functions can have any supported parameter list; choose an i32 adapter for the CLI.");
    }
    error(source, {TokenKind::end, "", 0}, "No entry function found.", "Define func entry(value: i32) -> i32 to run or emit this file; check accepts libraries without an entry.");
}
}
