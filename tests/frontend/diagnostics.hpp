#pragma once
#include <algorithm>
#include <fstream>
#include <set>
namespace diagnostic_acceptance {
using Code = vd::DiagnosticCode;
void check(bool condition, const std::string& message) { if (!condition) throw std::runtime_error("Diagnostic acceptance: " + message); }
void write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary); stream << text;
    check(bool(stream), "write real source");
}
std::vector<vd::Diagnostic> errors(const std::filesystem::path& path) {
    try { (void)vd::compile_project(path); }
    catch (const vd::CompilationError& failure) {
        auto result = failure.diagnostics();
        check(!result.empty(), "empty failure");
        for (const auto& d : result) {
            check(d.severity == vd::Severity::error, "wrong severity");
            check(d.primary.source && d.primary.end >= d.primary.begin && d.primary.end <= d.primary.source->text().size(), "invalid span");
            check(!d.primary.module_identity.empty(), "lost VFS identity");
            check(vd::render_diagnostic(d).find(" | ") != std::string::npos, "missing numbered gutter");
        }
        return result;
    }
    throw std::runtime_error("Invalid diagnostic fixture reached IR: " + path.generic_string());
}
bool has(const std::vector<vd::Diagnostic>& diagnostics, Code code) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const auto& d) { return d.code == code; });
}
void run(const std::filesystem::path& root) {
    const auto fixtures = root / "tests/fixtures/diagnostics";
    auto several = errors(fixtures / "several.void");
    check(several.size() == 8, "independent errors lost or cascaded: " + std::to_string(several.size()));
    for (auto code : {Code::unknown_identifier, Code::type_mismatch, Code::readonly, Code::argument_count, Code::unknown_function, Code::missing_return, Code::duplicate})
        check(has(several, code), "missing independent code " + vd::diagnostic_code(code));
    const auto& first = several.front();
    check(first.primary.source->text().substr(first.primary.begin, first.primary.end - first.primary.begin) == "missing", "primary identifier range");
    check(first.primary.source->location(first.primary.begin).line == 5 && first.primary.source->location(first.primary.begin).column == 18, "identifier position");
    auto duplicate = std::find_if(several.begin(), several.end(), [](const auto& d) { return d.code == Code::duplicate; });
    check(duplicate != several.end() && duplicate->related.size() == 1, "previous local declaration missing");
    check(duplicate->related.front().span.source->location(duplicate->related.front().span.begin).line == 20, "previous local declaration position");
    auto mismatch = std::find_if(several.begin(), several.end(), [](const auto& d) { return d.code == Code::type_mismatch; });
    check(mismatch->expected_type == "bool" && mismatch->received_type == "i32", "structured types");
    auto recovery = errors(fixtures / "recovery.void");
    check(recovery.size() == 3 && has(recovery, Code::unknown_identifier), "later function swallowed during recovery");
    auto multi = errors(fixtures / "multi/main.void");
    check(multi.size() == 4 && has(multi, Code::import_read) && has(multi, Code::unknown_identifier) && has(multi, Code::type_mismatch) && has(multi, Code::duplicate), "independent module errors / failed import cascade");
    check(multi.front().primary.module_identity == "/a.void", "stable VFS ordering");
    auto cross = std::find_if(multi.begin(), multi.end(), [](const auto& d) { return d.code == Code::duplicate; });
    check(!cross->related.empty() && cross->related.front().span.module_identity == "/main.void", "cross-file previous declaration owner");
    for (unsigned repeat = 0; repeat < 15; ++repeat) {
        auto again = errors(fixtures / "multi/main.void");
        check(again.size() == multi.size(), "unstable diagnostic count");
        for (std::size_t i = 0; i < again.size(); ++i) check(vd::render_diagnostic(again[i]) == vd::render_diagnostic(multi[i]), "unstable output order");
    }
    auto output = std::filesystem::current_path() / "diagnostic fixtures with spaces";
    auto source = output / "main.void";
    std::filesystem::create_directories(output);
    check(has(errors(output / "nonexistent entry.void"), Code::import_read), "missing entry structured diagnostic");
    try { (void)vd::read_source(output / "nonexistent entry.void"); check(false, "missing read accepted"); }
    catch (const vd::CompilationError& failure) { check(failure.diagnostics().front().code == Code::import_read, "read_source structured diagnostic"); }
    // Each malformed fragment must terminate and retain a later independent error.
    const std::vector<std::string> malformed = {
        "func damaged(:\n    return\n", "func damaged(value: i32) ->:\n    return\n",
        "use bare/\n", "use \"./unterminated\n", "const no_value\n",
        "func damaged():\n    var bad =\n    return\n", "func damaged():\n    if true\n        return\n",
        "func damaged():\n    if true:\n        return\n    else if:\n        return\n",
        "func damaged():\n    for in:\n        return\n", "func damaged():\n    while true:\n        return\n",
        "func damaged():\n\treturn\n", "func damaged():\n    if true:\n        return\n      return\n",
        "func damaged():\n    return @\n", "func damaged():\n    return\n        return\n"
    };
    for (const auto& fragment : malformed) for (unsigned repeat = 0; repeat < 3; ++repeat) {
        write(source, fragment + "\nfunc later() -> i32:\n    return later_unknown\n");
        auto diagnostics = errors(source);
        check(diagnostics.size() <= 6 && has(diagnostics, Code::unknown_identifier), "recovery lost later function or cascaded: " + fragment);
    }
    for (const auto& newline : {"\n", "\r\n", "\r"}) {
        std::string text = "\xef\xbb\xbf# \xc3\xa9\xf0\x9f\x98\x80" + std::string(newline) + "func f():" + newline + "    missing()";
        write(source, text);
        auto diagnostics = errors(source);
        check(diagnostics.size() == 1 && diagnostics[0].primary.source->location(diagnostics[0].primary.begin).line == 3, "BOM/newline/no-final-newline");
    }
    write(source, "# Unicode scalars: \xc3\xa9\xf0\x9f\x98\x80\nfunc f():\n    return \xc3\xa9\nfunc later():\n    missing()\n");
    auto unicode = errors(source);
    check(unicode.size() == 2 && unicode.front().primary.source->location(unicode.front().primary.begin).column == 12, "UTF-8 unexpected scalar position");
    vd::Diagnostic manual{Code::syntax};
    manual.description = "scalar range";
    manual.primary = {std::make_shared<const vd::SourceFile>("unicode.void", "# \xc3\xa9\xf0\x9f\x98\x80!"), "/unicode.void", 4, 8};
    check(manual.primary.source->location(4).column == 4 && vd::render_diagnostic(manual).find(" |    ^\n") != std::string::npos, "Unicode scalar range rendering");
    // Rejected snapshots retain original bytes/spans. Only rendering expands
    // bad bytes; subsequent/related carets must use those expanded widths.
    const std::string damaged = "\xef\xbb\xbf# header\r\n# \xc3\xa9\xf0\x9f\x98\x80 \x80\xffx";
    try { vd::SourceFile invalid("damaged.void", damaged); check(false, "invalid UTF-8 accepted"); }
    catch (const vd::CompilationError& failure) {
        auto d = failure.diagnostics().front();
        const auto offset = damaged.size() - 3;
        check(d.code == Code::utf8 && d.primary.begin == offset && d.primary.end == offset + 1, "invalid UTF-8 byte span changed");
        check(d.primary.source->text() == damaged, "rejected snapshot bytes changed");
        auto position = d.primary.source->location(offset);
        check(position.line == 2 && position.column == 6, "invalid UTF-8 scalar location changed");
        const auto rendered = vd::render_diagnostic(d);
        for (std::size_t i = 0; i < rendered.size();) {
            auto count = vd::utf8_scalar_bytes(rendered, i);
            check(count != 0, "diagnostic output is not UTF-8"); i += count;
        }
        check(rendered.find("# \xc3\xa9\xf0\x9f\x98\x80 \\x80\\xFFx\n   |      ^^^^\n") != std::string::npos, "escaped byte caret width");
        d.primary.begin = damaged.size() - 1; d.primary.end = damaged.size();
        d.related = {{failure.diagnostics().front().primary, "Original invalid byte"}};
        auto later = vd::render_diagnostic(d);
        check(later.find("\n   | " + std::string(13, ' ') + "^\n") != std::string::npos, "caret after escaped bytes");
        check(later.find("Original invalid byte at damaged.void:2:6") != std::string::npos &&
              later.find("\n   |      ^^^^\n") != std::string::npos, "related invalid-byte rendering");
        check(d.primary.source->text() == damaged, "rendering changed retained snapshot");
    }
    write(source, "func f():\n    var value = missing\n    value += 1\n    value += false\n    var copy = value * true\n    return\n");
    check(errors(source).size() == 1, "poisoned local expression cascade");
    write(source, "const first = second\nconst second = first\nconst third = first + 1\nfunc f() -> i32:\n    return third\nfunc later():\n    unknown()\n");
    auto cycle = errors(source);
    check(cycle.size() == 2 && has(cycle, Code::constant_cycle) && has(cycle, Code::unknown_function), "constant cycle cascade or later error lost");
    write(source, "func f():\n    var value = (1 + )\n    value += 1\n    var copy = value * true\n    return\nfunc later():\n    unknown()\n");
    auto syntax_poison = errors(source);
    check(syntax_poison.size() == 2 && has(syntax_poison, Code::syntax) && has(syntax_poison, Code::unknown_function), "malformed local binding cascade");
    write(source, "func damaged(value:):\n    return\nfunc f():\n    damaged(1)\n    damaged(2)\nfunc later():\n    unknown()\n");
    auto signature_poison = errors(source);
    check(signature_poison.size() == 2 && has(signature_poison, Code::unsupported_type) && has(signature_poison, Code::unknown_function), "malformed signature cascade");
    write(output / "one.void", "func first() -> i32:\n    return 1\n");
    write(output / "two.void", "func second() -> i32:\n    return 2\n");
    write(source, "use \"./one.void\" as same\nuse \"./two.void\" as same\nfunc f():\n    same.missing()\nfunc later():\n    unknown()\n");
    auto alias = errors(source);
    check(alias.size() == 2 && has(alias, Code::alias_conflict) && !alias.front().related.empty(), "alias conflict primary/related recovery");
    write(source, "func repeated(value: i32) -> i32:\n    return value\nfunc repeated(value: bool) -> bool:\n    return value\nfunc f():\n    var result = repeated(true)\n    repeated(false)\nfunc later():\n    unknown()\n");
    auto duplicate_signature = errors(source);
    check(duplicate_signature.size() == 2 && has(duplicate_signature, Code::duplicate) && has(duplicate_signature, Code::unknown_function), "duplicate signature selected an arbitrary type");
    write(source, "const amount = true\nconst amount = 1\nfunc f() -> i32:\n    return amount\nfunc later():\n    unknown()\n");
    auto duplicate_constant = errors(source);
    check(duplicate_constant.size() == 2 && has(duplicate_constant, Code::duplicate) && has(duplicate_constant, Code::unknown_function), "duplicate constant selected an arbitrary type");
    write(source, "use \"./one.void\" as two\nuse \"./two.void\"\nfunc f():\n    two.second()\nfunc later():\n    unknown()\n");
    auto implicit_alias = errors(source);
    check(implicit_alias.size() == 2 && has(implicit_alias, Code::alias_conflict), "implicit alias conflict failed to poison the actual qualifier");
    write(source, "func f():\n    var invalid = missing\n    invalid()\n    invalid()\nfunc later():\n    unknown()\n");
    auto poisoned_call = errors(source);
    check(poisoned_call.size() == 2 && has(poisoned_call, Code::unknown_identifier) && !has(poisoned_call, Code::not_callable), "poisoned local call cascade");
    write(source, "func f(value: i32, value: bool) -> bool:\n    return value\nfunc later():\n    unknown()\n");
    auto duplicate_parameter = errors(source);
    check(duplicate_parameter.size() == 2 && has(duplicate_parameter, Code::duplicate) && !has(duplicate_parameter, Code::type_mismatch), "duplicate parameter selected an arbitrary type");
    write(source, "use \"./one.void\"\nuse \"./one.void\"\nfunc f():\n    var first = 1\n");
    auto repeated_note = errors(source);
    check(repeated_note.size() == 1 && repeated_note.front().code == Code::duplicate && repeated_note.front().related.size() == 1, "repeated import duplicated related declaration notes");
    std::string flood = "func f():\n";
    for (int i = 0; i < 140; ++i) flood += "    unknown_" + std::to_string(i) + "()\n";
    write(source, flood);
    auto limited = errors(source);
    check(limited.size() == 101 && limited.back().code == Code::limit, "diagnostic limit notice");
    for (const auto& expression : {std::string(400, '(') + "1" + std::string(400, ')'), [] { std::string value; for (int i = 0; i < 400; ++i) value += "- "; return value + "1"; }()}) {
        write(source, "func f() -> i32:\n    return " + expression + "\nfunc later():\n    unknown()\n");
        auto nesting = errors(source); check(has(nesting, Code::nesting) && has(nesting, Code::unknown_function), "bounded expression recursion");
    }
    std::cerr << "Frontend resource case: combined expression/constant depth" << std::endl;
    std::string dependencies;
    for (int i = 0; i < 9; ++i) {
        dependencies += "const chain_" + std::to_string(i) + " = ";
        for (int unary = 0; unary < 40; ++unary) dependencies += "- ";
        dependencies += i == 8 ? "1\n" : "chain_" + std::to_string(i + 1) + "\n";
    }
    write(source, dependencies + "func later():\n    unknown()\n");
    auto combined_depth = errors(source);
    check(has(combined_depth, Code::nesting) && has(combined_depth, Code::unknown_function), "combined expression/constant depth is not bounded");
    write(source, "# " + std::string(100000, 'a') + "\nfunc f():\n    missing()\n");
    check(errors(source).size() == 1, "long line");
    write(source, "func f():\n    var value = " + std::string(100000, ' ') + "missing\n");
    auto long_line = errors(source);
    check(long_line.size() == 1 && long_line.front().primary.source->location(long_line.front().primary.begin).column == 100017, "long diagnostic line column");
    write(output / "bad.void", "# \xed\xa0\x80\n");
    write(output / "good.void", "func f():\n    missing()\n");
    write(source, "use \"./bad.void\"\nuse \"./bad.void\"\nuse \"./good.void\"\nfunc entry(value: i32) -> i32:\n    return value\n");
    auto utf8 = errors(source);
    check(utf8.size() == 2 && has(utf8, Code::utf8) && has(utf8, Code::unknown_function), "invalid UTF-8 prevents unrelated discovery or duplicates");
    for (const auto& incomplete : {"func", "func damaged(", "func damaged() -> i32:\n    var value =", "func damaged() -> i32:\n    if true:\n"}) {
        write(source, incomplete);
        auto eof = errors(source);
        check(eof.size() == 1 && !has(eof, Code::missing_return), "EOF recovery generated secondary return error");
    }
    std::string flat = "func f() -> i32:\n    return 1";
    for (int i = 0; i < 1000; ++i) flat += " + 1";
    write(source, flat + "\nfunc later():\n    unknown()\n");
    auto large_expression = errors(source);
    check(has(large_expression, Code::nesting) && has(large_expression, Code::unknown_function), "bounded left-associated AST");
    for (int i = 0; i < 130; ++i) {
        auto import = i == 129 ? std::string{} : "use \"./deep_" + std::to_string(i + 1) + ".void\"\n";
        write(output / ("deep_" + std::to_string(i) + ".void"), import + "func helper(value: i32) -> i32:\n    return value\n");
    }
    write(source, "use \"./deep_0.void\"\nfunc entry(value: i32) -> i32:\n    return value\n");
    auto deep_imports = errors(source);
    check(deep_imports.size() == 1 && has(deep_imports, Code::nesting), "bounded import graph recursion");
    // Diagnostic ownership survives subsequent edits and deletion of imported files.
    write(source, "# \xc0\xaf\n");
    auto invalid_entry = errors(source);
    check(invalid_entry.size() == 1 && invalid_entry.front().code == Code::utf8 && invalid_entry.front().primary.begin == 2 && invalid_entry.front().primary.source->text() == "# \xc0\xaf\n", "entry anchor replaced the rejected UTF-8 snapshot");
    auto snapshot = utf8.front().primary.source;
    auto original = snapshot->text();
    std::filesystem::remove(output / "bad.void");
    write(source, "func entry(value: i32) -> i32:\n    const amount = 42\n    if value < 0:\n        return amount / (value + 1)\n    return value + amount\n");
    check(snapshot->text() == original, "diagnostic snapshot changed after file deletion");
    auto valid = vd::compile_project(source);
    vd::Jit jit; auto old = jit.compile(valid.module, "entry"); vd::ReloadSlot slot(old);
    auto contract = old->compatibility().constant_contract;
    write(source, "func entry(value: i32) -> i32:\n    const amount =\n    return unknown\n");
    try { auto broken = vd::compile_project(source); slot.replace(jit.compile(broken.module, "entry")); check(false, "invalid edit published"); }
    catch (const vd::CompilationError&) {}
    check(slot.invoke(10) == 52 && old->invoke(1) == 43 && old->compatibility().constant_contract == contract, "failed edit changed old generation");
    bool retained_failure = false;
    try { (void)slot.invoke(-1); }
    catch (const std::runtime_error& failure) {
        retained_failure = std::string(failure.what()).find("return amount / (value + 1)") != std::string::npos;
    }
    check(retained_failure && snapshot->text() == original, "old runtime/compile diagnostic snapshots lost during failed reload");
    write(source, "func entry(value: i32) -> i32:\n    const amount = 43\n    return value + amount\n");
    auto changed = jit.compile(vd::compile_project(source).module, "entry");
    check(!slot.replace(changed) && slot.invoke(0) == 42 && changed->invoke(0) == 43, "const restart contract weakened");
    std::cout << "Structured diagnostics: independent files, recovery, bounded cascades, snapshots and failed reload verified\n";
}
}
