#pragma once
#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace vd::cli {
using HelpRow = std::pair<std::string_view, std::string_view>;
inline constexpr std::array<HelpRow, 17> commands{{
    {"crashes", "Diagnose an explicitly launched native program."},
    {"check", "Check source and imports."},
    {"run", "Compile and run through the JIT."},
    {"build", "Build a native executable."},
    {"publish", "Publish a native application distribution."},
    {"target", "Manage publishing target packs."},
    {"emit", "Write a native object."},
    {"init", "Add a project manifest."},
    {"new", "Create a project or manage templates."},
    {"add", "Add a source dependency."},
    {"remove", "Remove a direct dependency."},
    {"tidy", "Check or reconcile dependencies."},
    {"resolve", "Verify or create a dependency lock."},
    {"deps", "Explain or fetch dependencies."},
    {"package", "Assemble an app or manage source packages."},
    {"native", "Import headers or manage native approvals."},
    {"native-bind", "Generate an adapter or native declarations."},
}};
inline bool known_command(const std::string& command) {
    return std::any_of(commands.begin(), commands.end(), [&](const auto& row){return row.first == command;});
}
template<class Rows> void help_rows(std::ostream& out, const Rows& rows) {
    std::size_t width = 0;
    for (const auto& row : rows) width = std::max(width, row.first.size());
    // CLI spellings are ASCII; do not apply byte-width alignment to arbitrary UTF-8.
    for (const auto& row : rows)
        out << "  " << row.first << std::string(width - row.first.size() + 2, ' ') << row.second << '\n';
}
inline void usage(std::ostream& out, const std::string& command={}) {
    if(command.empty() || command=="crashes")out<<"Usage: voidc crashes run ABSOLUTE-EXECUTABLE [arguments...]\n";
    if(command.empty() || command=="check")out<<"Usage: voidc check <file-or-directory> [--no-opt]\n";
    if(command.empty() || command=="run")out<<"Usage: voidc run [file-or-directory] [legacy-i32-input] [--no-opt] [--no-cache | --cache] [-- application-arguments...]\n";
    if(command.empty() || command=="build")out<<"Usage: voidc build [file-or-directory] [-o executable] [--no-opt]\n";
    if(command.empty() || command=="publish")out<<"Usage: voidc publish [--project DIR] [--entry RELATIVE.void] [--target TARGET] [--profile release|performance|protected] [-o NEW-DIRECTORY]\n"
      "       [--optimization none|size|speed] [--resources embed|files] [--symbols private|none] [--native-catalog SHA256] [--lto none|thin] [--format directory|app] [--icon RELATIVE.icns] [--portable]\n"
      "       voidc publish --verify DIR\n"
      "       voidc publish --bundle DIR -o NEW.zip\n"
      "       voidc publish --resolve-diagnostic MAP IMAGE TOKEN\n";
    if(command.empty() || command=="target")out<<"Usage: voidc target list [--available] | add DIRECTORY | inspect ID | verify ID | remove ID\n";
    if(command.empty() || command=="package")out<<"Usage: voidc package [--project DIR] [--profile debug|release] [-o directory] [--allow-native] [--replace]\n"
      "       voidc package --verify DIR\n"
      "       voidc package list | voidc package search NAME\n"
      "       voidc package list --json\n"
      "       voidc package install ALIAS (--dir DIR | --git ORIGIN [--rev FULL_SHA] [--subdir DIR])\n"
      "       voidc package uninstall ALIAS\n";
    if(command.empty() || command=="emit")out<<"Usage: voidc emit <file-or-directory> -o <object> [--no-opt]\n";
    if(command.empty() || command=="init")out<<"Usage: voidc init [DIR] [project options]\n";
    if(command.empty() || command=="new")out<<"Usage: voidc new [list]\n"
      "       voidc new list --json\n"
      "       voidc new (app | classlib | lib) [PATH | .] [--name NAME] [project options]\n"
      "       voidc new PATH [project options] [--source PATH]\n"
      "       voidc new SHORT-NAME --name NAME [--nested]\n"
      "       voidc new --template SHORT-NAME --name NAME [--in-place]\n"
      "       voidc new install (--dir PATH | --git URL)\n"
      "       voidc new uninstall SHORT-NAME\n";
    if(command.empty() || command=="add")out<<"Usage: voidc add ALIAS [--project DIR]\n"
      "       voidc add ALIAS [--project DIR] (--dir DIR [--copy] | --git ORIGIN [--rev FULL_SHA] [--subdir DIR])\n"
      "       voidc add --suggest-alias --dir DIR [--project DIR]\n";
    if(command.empty() || command=="tidy")out<<"Usage: voidc tidy [--project DIR] [--check]\n";
    if(command.empty() || command=="remove")out<<"Usage: voidc remove ALIAS [--project DIR]\n";
    if(command.empty() || command=="resolve")out<<"Usage: voidc resolve --project DIR [--locked | --refresh]\n";
    if(command.empty() || command=="deps")out<<"Usage: voidc deps explain --project DIR [--json]\n"
      "       voidc deps fetch --project DIR [--allow-git ORIGIN REV]...\n";
    if(command.empty() || command=="native")out<<"Usage: voidc native import <header> --provider ID --out NEW-DIR [--source /native.void] [--clang PATH] [--language c|c++] [-- -Ipath -Dname=value -Uname]\n"
      "       voidc native approvals\n"
      "       voidc native revoke APPROVAL_ID\n";
    if(command.empty() || command=="native-bind")out<<"Usage: voidc native-bind <descriptor> <C-header> -o <adapter.c>\n";
}
inline void help(const std::string& command = {}, std::string_view nested = {}) {
    std::cout << "VOID Language Compiler v" << VOID_COMPILER_VERSION << " (development)\nLLVM " << LLVM_VERSION_STRING << "\n\n";
    if (command.empty()) {
        std::cout << "Usage: voidc <command> [arguments] [options]\n\nCommands:\n";
        auto rows = std::array<HelpRow, commands.size() + 1>{};
        std::copy(commands.begin(), commands.end(), rows.begin());
        rows.back() = {"help", "Show command help."};
        help_rows(std::cout, rows);
        std::cout << "\nOptions:\n";
        help_rows(std::cout, std::array<HelpRow,2>{{{"-h, --help", "Show help."}, {"--version", "Show compiler, LLVM, target and build identity."}}});
        std::cout << "\nLocal crash diagnostics: VOID_CRASH_DIAGNOSTICS=1 (disabled by default).\n"
                     "Optional absolute VOID_CRASH_DIRECTORY; see docs/projects/G90_CRASH_DIAGNOSTICS.md.\n";
        std::cout << "\nUse 'voidc <command> --help' for arguments, options and examples.\n";
    } else {
        usage(std::cout, command);
        if (!nested.empty()) std::cout << "\nHelp for " << command << ' ' << nested << ":\n";
        const bool source = command=="check" || command=="run" || command=="build" || command=="emit" || command=="package";
        if (source) {
            std::cout << "\nSource options:\n";
            help_rows(std::cout, std::array<HelpRow,6>{{
                {"--project DIR", "Select exactly DIR/void.project.toml; no positional selector."},
                {"--entry PATH", "Override a manifest application entry with a relative source."},
                {"--profile debug|release", "Project profile; default release. --cache selects implicit debug."},
                {"--no-opt", "Disable O2 without relabeling the profile; no-op for check."},
                {"--", "End options before a dash-leading source path."},
                {"-h, --help", "Show this command's help."}
            }});
            std::cout << "\nPresentation options (source/build operations only):\n";
            help_rows(std::cout, std::array<HelpRow,4>{{
                {"--status auto|plain|off", "Default auto: concise stderr status only in an eligible terminal."},
                {"--timings", "Opt in to inclusive phase timings; text on stderr by default."},
                {"--timings-format text|json", "Requires --timings; JSON requires a fresh explicit report file."},
                {"--timings-output PATH", "JSON report outside source/project and artifact trees; never overwritten."}
            }});
            std::cout << "A directory selects its exact manifest, otherwise main.void; bare files keep their parent root.\n"
                         "No parent-project search or fallback from an invalid manifest. Libraries support check only.\n"
                         "Debug information and optimization are independent and target-dependent.\n";
        }
        if (command=="crashes") std::cout << "\nOpt in for this explicit native-program launch only. No handlers are installed into the program.\n"
            "Reports are local; the program keeps its arguments, streams and exit status.\n"
            "Example: voidc crashes run /absolute/path/to/program argument\n";
        if (command=="check") std::cout << R"help(
Check source and imports without executing startup code; no application entry is required.
A file/directory or --project is required. --no-opt is accepted without affecting checks.
Example: voidc check --project .
)help";
        auto options_title = [] { std::cout << "\nCommand options:\n"; };
        if (command=="run") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,4>{{
                {"--cache", "Select implicit Debug and print cache status; conflicts with --no-cache."},
                {"--no-cache", "Disable persistent caching without changing optimization."},
                {"--allow-native", "Authorize this manifest invocation only."},
                {"--reuse-native-approval", "Reuse exact saved consent without prompting."}
            }});
        }
        if (command=="build" || command=="emit") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,1>{{
                {"--debug-info none|line|full", "Override manifest debug information for this build/emit."}
            }});
            std::cout << '\n'; // The following option table has its own column width.
            if (command=="build") help_rows(std::cout, std::array<HelpRow,3>{{
                {"-o PATH", "Executable output; defaults under build/."},
                {"--allow-native", "Authorize this manifest invocation only."},
                {"--reuse-native-approval", "Reuse exact saved consent without prompting."}
            }});
            else help_rows(std::cout, std::array<HelpRow,2>{{
                {"-o PATH", "Required host-native object output."},
                {"--allow-native", "Authorize this manifest invocation only."}
            }});
        }
        if (command=="init" || command=="new") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,8>{{
                {"--kind application|library", "Default application; identity options must match kind."},
                {"--name TEXT", "Project display name; default destination basename."},
                {"--version SEMVER", "Project version; default 0.1.0."},
                {"--project-id ID", "Override the generated local project ID."},
                {"--app-id ID", "Application identity; application kind only."},
                {"--package-id ID", "Library identity; library kind only."},
                {"--package-name IDENTIFIER", "Library import name."},
                {"--nested", "Explicitly allow creation inside another manifest project."}
            }});
            if (command=="new") {std::cout << "\nTemplate options:\n"; help_rows(std::cout, std::array<HelpRow,6>{{
                {"--source PATH", "Built-in starter source path."},
                {"--template SHORT-NAME", "Editor template route; requires --name NAME in this order."},
                {"--in-place", "Editor template route: create in the admitted current directory."},
                {"--dir PATH", "install: register an admitted local template."},
                {"--git URL", "install: acquire the explicitly selected template origin."},
                {"--json", "list: emit schema-1 template metadata."}
            }});}
        }
        if (command=="add" || command=="remove" || command=="tidy" || command=="resolve" || command=="deps") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,1>{{
                {"--project DIR", command=="resolve" || command=="deps" ? "Required exact manifest root." : "Exact manifest root; default current directory."}
            }});
        }
        if (command=="add" || command=="package") {
            std::cout << "\nSource selection options (package install only for package):\n";
            help_rows(std::cout, std::array<HelpRow,4>{{
                {"--dir DIR, --path DIR", "Explicit local source directory."},
                {"--git ORIGIN", "Explicit Git source origin."},
                {"--rev FULL_SHA", "Immutable Git revision."},
                {"--subdir DIR", "Bounded source subtree in the Git snapshot."}
            }});
        }
        if (command=="add") {std::cout << "\nDependency edit options:\n"; help_rows(std::cout, std::array<HelpRow,5>{{
            {"--copy", "Copy an explicit local source into the project."},
            {"--bundled", "Select the bundled alias."},
            {"--registered IDENTITY", "Require the selected user registration."},
            {"--suggest-alias", "With --dir: emit nullable alias JSON without editing."},
            {"--edit-token TOKEN", "Require the editor-reviewed dependency state."}
        }});}
        if (command=="remove") {std::cout << "\nDependency edit options:\n"; help_rows(std::cout, std::array<HelpRow,1>{{{"--edit-token TOKEN", "Require the editor-reviewed dependency state."}}});}
        if (command=="tidy") {std::cout << "\nReconciliation options:\n"; help_rows(std::cout, std::array<HelpRow,1>{{{"--check", "Check consistency without ordinary reconciliation."}}});}
        if (command=="resolve") {std::cout << "\nLock options:\n"; help_rows(std::cout, std::array<HelpRow,2>{{
            {"--locked", "Require an existing lock when dependencies need it."},
            {"--refresh", "Explicitly replace a stale lock; conflicts with --locked."}
        }});}
        if (command=="deps") {std::cout << "\nOperation options:\n"; help_rows(std::cout, std::array<HelpRow,3>{{
            {"--json", "explain: emit schema-1 dependency metadata."},
            {"--for-edit", "explain: require --json and include the dependency state token."},
            {"--allow-git ORIGIN REV", "fetch: explicitly admit one transitive pin; repeatable."}
        }});}
        if (command=="package") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,5>{{
                {"-o DIR", "Application distribution; default <project>/dist."},
                {"--allow-native", "Authorize this manifest invocation only."},
                {"--replace", "Replace a verified same-application Linux package."},
                {"--verify DIR", "Verify an existing application inventory without compiling."},
                {"--json", "list: emit schema-1 source package metadata."}
            }});
        }
        if (command=="native") {
            std::cout << "\nImport options:\n";
            help_rows(std::cout, std::array<HelpRow,6>{{
                {"--provider ID", "Required native provider identity."},
                {"--out NEW-DIR", "Required fresh candidate output directory."},
                {"--source PATH", "Declaration source identity; default /native.void."},
                {"--clang PATH", "Configured pinned Clang 23.1.4 driver by default."},
                {"--language c|c++", "Header language; default c."},
                {"--", "Begin the bounded -I/-D/-U preprocessing argument vector."}
            }});
        }
        if (command=="publish") std::cout << R"help(
Publish to a fresh distribution. The default profile is release.
Foreign targets require a compatible installed target pack.
Managed native code needs the exact catalog SHA-256 and redistribution rights.
Example: voidc publish --project . --resources embed -o dist/my-app
)help";
        if (command=="target") std::cout << "Example: voidc target add ./custom-target\n";
        if (command=="native-bind") {
            options_title();
            help_rows(std::cout, std::array<HelpRow,1>{{{"-o NEW-OUTPUT", "Required fresh .c adapter or .void declarations."}}});
        }
        if (command=="run") std::cout << R"help(
Compile then invoke through the JIT; omitted selector defaults to the current directory.
--cache selects effective Debug/no optimization and reports actual cache status on stderr.
--no-cache disables persistence, not O2; it conflicts with --cache.
Eligible automatic caching is attempted even without --cache, including optimized project code.
Use an explicit source/project and -- for literal application arguments (including --help).
Legacy entry(i32) -> i32 accepts one decimal i32, default 0, prints its result and exits 0.
Modern application return status is the process status. Native execution retains verification and consent.
--allow-native authorizes this manifest invocation; --reuse-native-approval reuses exact saved consent without prompting.
Examples: voidc run .
          voidc run calculation.void 10 --no-cache
          voidc run main.void -- --help "two words"
)help";
        if (command=="build") std::cout << R"help(
Link and publish an executable using the matching Runtime and void-native.txt beside voidc.
The original Clang/LLD 23.1.4 SDK and host development libraries must remain available.
Omitted selector defaults to cwd. -o PATH defaults to <project-root or entry-parent>/build/<entry-stem>[.exe].
Missing parents are created after collision checks. Existing output is preserved on failed publication.
--allow-native authorizes this manifest invocation; --reuse-native-approval reuses exact saved consent without prompting.
Examples: voidc build .
          voidc build --project . --profile debug -o "output folder/hello"
)help";
        if (command=="emit") std::cout << R"help(
Write a host-native object, not an executable; explicit source/project and -o PATH are required.
Manual linking must supply the matching Runtime and dependencies; prefer build for applications.
--allow-native uses existing manifest invocation-only authorization; no saved approval is created by this flag.
Example: voidc emit main.void -o main.o
)help";
        if (command=="init") std::cout << R"help(
Create a new manifest in an existing directory, default cwd; preserve existing sources.
Options: --kind application|library, --name TEXT, --version SEMVER, --project-id ID,
         --app-id ID or --package-id ID --package-name IDENTIFIER, --nested, -- before a path.
Defaults: application, destination basename, version 0.1.0 and independent local IDs.
Identity options must match kind. Child manifests need --nested; existing manifests are refused.
Example: voidc init . --name Orbit
)help";
        if (command=="new") std::cout << R"help(
No operands or list displays built-in and installed templates; list --json emits schema 1.
Built-ins: app, classlib, and lib (alias for classlib). Use --name NAME or an explicit PATH.
Project options: --kind application|library, --name, --version, --project-id, --app-id,
                 --package-id, --package-name, --source PATH and --nested.
Defaults: application, basename, version 0.1.0, independent local IDs; src/app.void or src/<package-name>.void.
Use . for an empty in-place built-in project; ./PATH or -- PATH keeps a literal starter-looking path.
--template SHORT-NAME --name NAME [--in-place] has exact ordering and emits schema-1 root JSON.
Installed templates require fresh directories; new SHORT-NAME --name NAME [--nested] selects an installed template.
install accepts exactly --dir PATH or --git URL; uninstall accepts one short name, excluding built-ins.
Creation publishes exclusively. Templates and installation never grant native execution consent.
Examples: voidc new app --name Orbit
          voidc new classlib geometry
          voidc new install --dir ./templates/game
)help";
        if (command=="add") std::cout << R"help(
Add ALIAS to --project DIR (default .), choosing a registered/bundled source or an explicit source.
--dir DIR (alias --path) references local sources; --copy copies an explicit local source.
--git ORIGIN [--rev FULL_SHA] [--subdir DIR] uses verified Git snapshots; omitted revisions follow existing source selection.
--bundled selects the bundled alias; --registered IDENTITY checks the selected user registration.
--edit-token TOKEN protects an editor-reviewed dependency edit; it is not native authorization.
--suggest-alias --dir DIR [--project DIR] emits schema-1 JSON with a nullable alias and performs no edit.
Updates lock/installed views through existing integrity checks. Native preparation grants no execution consent.
Examples: voidc add geometry --dir ../geometry
          voidc add raylib
)help";
        if (command=="remove") std::cout << R"help(
Remove one direct ALIAS from --project DIR (default .) and reconcile installed views.
--edit-token TOKEN requires the editor-reviewed dependency state. Native approvals are not removed.
Example: voidc remove geometry --project .
)help";
        if (command=="tidy") std::cout << R"help(
Validate imports and reconcile source dependencies and installed views in --project DIR (default .).
--check checks consistency without applying ordinary reconciliation. Interrupted edits remain explicit errors.
Example: voidc tidy --project . --check
)help";
        if (command=="resolve") std::cout << R"help(
Requires --project DIR. Verify an existing local lock or create one when inputs are available.
--locked requires a lock when dependencies need it. --refresh explicitly replaces a stale lock.
Use at most one; no implicit Git acquisition or native approval occurs.
Example: voidc resolve --project . --locked
)help";
        if (command=="deps") std::cout << R"help(
explain requires --project DIR and verifies the local dependency graph without acquisition or publication.
--json emits bounded schema-1 JSON; --for-edit requires --json and includes a state token for editor edits.
Inspection failures exit 1 with JSON on stdout in JSON mode, otherwise a diagnostic on stderr.
fetch requires --project DIR; repeated --allow-git ORIGIN REV explicitly admit required transitive immutable pins.
Content, origin and revision checks remain mandatory; no native execution consent is implied.
Examples: voidc deps explain --project . --json
          voidc deps fetch --project .
)help";
        if (command=="package") std::cout << R"help(
Without a nested operation, assemble a relocatable manifest application on Linux x86-64 GNU or Windows x64 MSVC.
-o DIR defaults to <project>/dist. --allow-native retains invocation-only authorization.
--replace accepts a verified same-application package on Linux; Windows still requires a fresh destination.
--verify DIR checks the inventory without compiling. Integrity and publication checks are mandatory.
list [--json] discovers installed/bundled sources; search NAME filters aliases, not an online registry.
install ALIAS accepts --dir (alias --path) or --git, optionally --rev FULL_SHA and --subdir DIR.
Package registration takes no --project, --copy or execution consent; uninstall takes one ALIAS.
Examples: voidc package --project . --profile release -o dist
          voidc package --verify dist
          voidc package list --json
          voidc package install geometry --dir ../geometry
)help";
        if (command=="native") std::cout << R"help(
import inspects a header and publishes editable candidate bindings in --out NEW-DIR; --provider ID is required.
--source defaults to /native.void; --clang defaults to the installed configured Clang 23.1.4 driver.
--language c|c++ defaults to c. Validated preprocessing -I/-D/-U tokens follow --.
Generated candidates require review and existing admission; header inspection does not authorize execution.
approvals lists exact saved records; revoke APPROVAL_ID removes the exact selected record through the checked store.
A malformed revoke ID returns 2; a missing record is an operational failure (1).
Examples: voidc native import api.h --provider demo --out bindings -- -Iinclude
          voidc native approvals
)help";
        if (command=="native-bind") std::cout << R"help(
Exact grammar: DESCRIPTOR C-HEADER -o NEW-OUTPUT, with output suffix .c or .void.
.c generates a checked C11 adapter; .void generates native declarations from the bounded descriptor.
Output is created exclusively; generating bindings changes no native ABI or execution authorization.
Example: voidc native-bind provider.toml api.h -o adapter.c
)help";
        std::cout << "\nExit status: 0 success; 1 compilation/operational failure; 2 invocation error or native approval required.\n";
        if (command=="run") std::cout << "run propagates a modern application's status; a legacy numeric result still exits 0.\n";
        std::cout << "Quote paths with spaces. Only the documented command parser controls accepted ordering and separators.\n";
    }
    std::cout << "Documentation: https://github.com/Shmellyorc/Void.Language/wiki\n";
}
}
