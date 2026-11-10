#include "void/backend.hpp"
#include "void/path.hpp"
#include "void/project_resolution.hpp"
#include "void/runtime.hpp"
#include "void/tooling.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void rejected(const vd::ir::Module& module, std::string_view diagnostic) {
    try {
        vd::ir::validate(module);
    } catch (const std::invalid_argument& failure) {
        if (std::string_view(failure.what()).find(diagnostic) == std::string_view::npos) throw;
        return;
    }
    throw std::runtime_error("independent IR validator accepted " + std::string(diagnostic));
}

std::vector<std::string> package_ids(const vd::ValidatedAnalysis& program) {
    std::vector<std::string> ids;
    for (const auto& module : program.modules())
        if (vd::package_source(module.identity)) ids.push_back(module.identity);
    return ids;
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 3, "pass fixture and relocated fixture paths");
        const auto root = std::filesystem::absolute(vd::utf8_path(argv[1]));
        const auto moved = std::filesystem::absolute(vd::utf8_path(argv[2]));
        const std::string app = "/src/app.void";
        const std::string geo = "/@pkg/org.example.geometry/src/api.void";
        const std::string algebra = "/@pkg/org.example.algebra/src/api.void";
        const std::string units = "/@pkg/org.example.units/src/api.void";
        vd::ToolingSession session(vd::ProjectSession{root});
        auto analyze = [&](bool success) {
            auto result = vd::ToolingSession::analyze(session.select());
            check(session.accept(result) && result->succeeded() == success, "package session status");
            check(bool(session.current_index()) == success, "package session index freshness");
            return result;
        };
        const auto initial = analyze(true);
        const auto& program = *initial->result().program;
        const auto relocated = vd::analyze_manifest_project(moved);
        check(relocated.succeeded(), "relocated application failed analysis");
        const auto ids = package_ids(program);
        check(ids.size() == 6 && ids == package_ids(*relocated.project->program()), "unstable or duplicate package modules");
        check(program.entry_identity() == app, "a package main participated in discovery");

        auto module = program.lower();
        check(module.locked_dependencies && module.globals.size() == 2 &&
              module.globals[0].module_identity == units && module.globals[1].module_identity == geo,
              "diamond globals must initialize once and in dependency order");
        check(module.source_imports.at(geo).contains(units) && module.source_imports.at(algebra).contains(units),
              "diamond branches did not converge");
        module.analysis.reset();
        vd::ir::validate(module);

        auto forged = module;
        forged.locked_dependencies.reset();
        rejected(forged, "verified package snapshot");
        forged = module;
        forged.source_imports.at(app).insert(units);
        rejected(forged, "verified direct edge");
        forged = module;
        forged.source_imports.at(geo).insert(app);
        rejected(forged, "escapes its source domain");
        forged = module;
        forged.source_imports.at(app).insert("/@pkg/org.example.geometry/src/helper shared.void");
        rejected(forged, "verified direct edge");
        for (const auto* inaccessible : {"hidden", "secret"}) {
            forged = module;
            auto main = std::find_if(forged.functions.begin(), forged.functions.end(),
                [&](const auto& function) { return function.name == app + "::main"; });
            check(main != forged.functions.end(), "application function missing");
            bool replaced = false;
            for (auto& block : main->blocks) for (auto& instruction : block.instructions)
                if (instruction.op == vd::ir::Op::call && instruction.callee == geo + "::answer") {
                    instruction.callee = geo + "::" + inaccessible;
                    replaced = true;
                }
            check(replaced, "package call missing");
            rejected(forged, "inaccessible package callable");
        }

        vd::Jit jit;
        auto code = jit.compile(program, false);
        auto runtime = code->runtime_owner();
        vd::ReloadSlot slot(code);
        check(slot.invoke(0) == 0, "initial locked generation");
        {
            vd::Runtime::Attachment attachment(*runtime);
            vd::Runtime::Managed managed(*runtime);
            check(runtime->collect() == vd::Runtime::Collection::completed && runtime->snapshot().objects >= 2,
                  "collection lost the package global or its managed value");
        }
        const auto changed = root / "deps/units/src/api.void";
        auto edited = vd::read_source(changed).text();
        const auto at = edited.find("value = 4");
        check(at != edited.npos, "fixture source changed");
        edited.replace(at, 9, "value = 5");
        {
            std::ofstream output(changed, std::ios::binary | std::ios::trunc);
            output << edited;
            check(bool(output), "package edit failed");
        }
        session.refresh();
        analyze(false);
        check(!jit.replace(slot, session, *initial, false), "stale package generation published");
        check(vd::project::resolve_dependencies(root, vd::project::ResolveMode::refresh) ==
              vd::project::ResolveResult::refreshed, "explicit refresh failed");
        session.refresh();
        const auto next = analyze(true);
        check(next->result().program->compatibility_contract() != program.compatibility_contract(),
              "package edit did not change the generation contract");
        check(session.invalidation().source_compatibility == vd::SourceCompatibility::restart_required &&
              !jit.replace(slot, session, *next, false), "changed package plan reused an old generation");
        check(slot.invoke(0) == 0, "rejected replacement damaged the current generation");
        runtime->close();
        check(runtime->memory_snapshot().handles == 0, "package globals survived Runtime shutdown");
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << failure.what() << '\n';
        return 1;
    }
}
