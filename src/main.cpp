#include "void/backend.hpp"
#include "void/private_diagnostics.hpp"
#include "void/crash_diagnostics.hpp"
#include "void/c_primitive.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/native_build.hpp"
#include "void/publish.hpp"
#include "target_pack.hpp"
#include "void/binary_audit.hpp"
#include "frontend/project_digest.hpp"
#include <llvm/TargetParser/Triple.h>
#include "void/native_import.hpp"
#include "void/project_creation.hpp"
#include "void/project_resolution.hpp"
#include "frontend/project_files.hpp"
#include "frontend/project_templates.hpp"
#include "native_approval.hpp"
#include "void/runtime.hpp"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/JSON.h>
#include <llvm/Config/llvm-config.h>
#include "cli_help.hpp"
#include "cli_presentation.hpp"
#include <llvm/Support/raw_ostream.h>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <vector>
namespace {
struct UsageError : std::invalid_argument {using std::invalid_argument::invalid_argument;};
using vd::cli::known_command;
using vd::cli::usage;
using vd::cli::help;
std::int32_t input(const char* text) {
    char* end = nullptr; errno = 0;
    auto value = std::strtoll(text, &end, 10);
    if (end == text || *end || errno == ERANGE || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw UsageError("Expected a decimal i32 input (-2147483648 through 2147483647).");
    return static_cast<std::int32_t>(value);
}
std::filesystem::path bundled_raylib_source(const std::filesystem::path& executable) {
    const auto installed=(executable.parent_path()/"../share/void/verified-packages/raylib").lexically_normal();
    const auto complete=[](const std::filesystem::path& source) {
        return std::filesystem::is_regular_file(source/"SOURCE_ORIGIN.txt") &&
               std::filesystem::is_regular_file(source.parent_path()/"raylib-prepare.void");
    };
    if(complete(installed))return installed;
    const auto build=vd::utf8_path(VOID_RAYLIB_BUILD_ROOT);
    // A build-tree compiler can use its CMake-staged resources. An installed
    // compiler must use its own resources, even if a checkout still exists.
    if(complete(build) &&
       (executable.parent_path()==build.parent_path().parent_path() ||
        executable.parent_path().parent_path()==build.parent_path().parent_path()))return build;
    return {};
}
int discover_packages(const std::filesystem::path& executable, int argc, char** argv) {
    const bool search=std::string_view(argv[2])=="search";
    const bool json=!search && argc==4 && std::string_view(argv[3])=="--json";
    if(argc!=(search||json?4:3) || (search && (!*argv[3] || argv[3][0]=='-')))
        throw UsageError("Expected package list [--json] or package search NAME.");
    const auto registrations=vd::project::list_user_packages();
    const bool raylib=!bundled_raylib_source(executable).empty() && vd::native_source_debug_supported();
    if(json) {
        llvm::json::Array packages;
        if(raylib)packages.emplace_back(llvm::json::Object{{"kind","bundled"},{"alias","raylib"},
            {"version",VOID_RAYLIB_VERSION},{"id","org.voidlang.raylib"}});
        for(const auto& package:registrations)packages.emplace_back(llvm::json::Object{{"kind","registered"},
            {"alias",package.alias},{"version",package.version},{"id",package.id},{"identity",package.identity}});
        llvm::outs()<<llvm::json::Value(llvm::json::Object{{"schema",1},{"packages",std::move(packages)}})<<'\n';
        return 0;
    }
    bool listed=false;
    for(const auto& package:registrations) {
        if(search && package.alias.find(argv[3])==std::string::npos)continue;
        std::cout<<"Installed: "<<package.alias<<' '<<package.version<<" | "<<package.id;
        if(package.git)std::cout<<" | "<<package.git->pin.origin<<" @ "<<package.git->pin.revision;
        std::cout<<'\n';
        listed=true;
    }
    if(!raylib ||
       (search && std::string_view("raylib").find(argv[3])==std::string_view::npos)) {
        if(!listed)std::cout<<"No matching bundled packages available for this compiler and target.\n";
        return 0;
    }
    std::cout<<"raylib " VOID_RAYLIB_VERSION " | org.voidlang.raylib\n"
              "  Source origin: " VOID_RAYLIB_ORIGIN " (" VOID_RAYLIB_SUBDIR ")\n"
              "  Base revision: " VOID_RAYLIB_BASE_REVISION "\n"
              "  Shipped snapshot SHA-256: " VOID_RAYLIB_SNAPSHOT_SHA256 "\n"
              "  Native raylib 6.0.0 (ABI 3); GNU/Linux x86-64, Windows x64 MSVC\n";
    const auto catalog=std::filesystem::current_path()/"libs/raylib/.void/native/void.native.toml";
    if(std::filesystem::is_regular_file(catalog))
        std::cout<<"  Local catalog present; run voidc resolve --project . --locked to verify it.\n";
    else std::cout<<"  Install with voidc add raylib, then approve the verified provider with voidc run .\n";
    return 0;
}
int maintain_project(const std::string& command,int argc,char** argv,const std::filesystem::path& executable,
                     bool registration=false) {
    using namespace vd::project;
    std::filesystem::path root=".";
    AddOptions options;
    std::optional<std::string> origin,rev,subdir,registered;
    std::set<std::string> seen;
    bool check=false,bundled=false,suggest_alias=false;
    for(int i=registration?3:2;i<argc;++i) {
        std::string option=argv[i];
        if(option=="--check" && command=="tidy") {
            if(check)throw UsageError("Repeated --check.");check=true;
        } else if(!registration && command=="add" && (option=="--copy" || option=="--bundled" || option=="--suggest-alias")) {
            if(!seen.insert(option).second)throw UsageError("Repeated "+option+".");
            if(option=="--copy")options.copy=true;
            else if(option=="--bundled")bundled=true;
            else suggest_alias=true;
        } else if((!registration && (option=="--project" || (command!="tidy" && option=="--edit-token") ||
                                      (command=="add" && option=="--registered"))) ||
                  (command=="add" && (option=="--path" || option=="--dir" || option=="--git" || option=="--rev" || option=="--subdir"))) {
            if(option=="--dir")option="--path";
            if(!seen.insert(option).second || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one value after "+option);
            if(option=="--project")root=vd::utf8_path(argv[i]);
            else if(option=="--path")options.path=vd::utf8_path(argv[i]);
            else if(option=="--git")origin=argv[i];
            else if(option=="--rev")rev=argv[i];
            else if(option=="--edit-token")options.edit_token=argv[i];
            else if(option=="--registered")registered=argv[i];
            else subdir=argv[i];
        } else if((command=="add" || command=="remove") && !option.starts_with('-') && options.alias.empty())options.alias=std::move(option);
        else throw UsageError("Unknown or repeated "+command+" option: "+option);
    }
    auto print_report=[](const MaintenanceReport& report) {
        std::cout<<(report.changed?"Updated source dependencies and installed views.":"Source dependencies unchanged.")<<'\n';
        for(const auto& advice:report.advice)std::cout<<"Advice: "<<advice<<'\n';
        return 0;
    };
    if(registration && command=="remove") {
        if(options.alias.empty())throw UsageError("Expected one package alias.");
        uninstall_user_package(options.alias);
        std::cout<<"Uninstalled user package "<<options.alias<<".\n";
        return 0;
    }
    if(command=="add") {
        const bool explicit_source=options.path || origin || rev || subdir;
        if(suggest_alias) {
            if(!options.alias.empty() || !options.path || origin || rev || subdir || bundled || registered ||
               options.copy || options.edit_token)
                throw UsageError("Expected add --suggest-alias --dir DIR [--project DIR].");
            const auto alias=suggest_local_package_alias(root,*options.path);
            llvm::outs()<<llvm::json::Value(llvm::json::Object{{"schema",1},
                {"alias",alias?llvm::json::Value(*alias):llvm::json::Value(nullptr)}})<<'\n';
            return 0;
        }
        if((options.copy && !options.path) || ((bundled || registered) && explicit_source) || (bundled && registered))
            throw UsageError("Select one source; --copy requires a local --dir.");
        if(!registration && !bundled && !options.alias.empty() && !explicit_source) {
            if(const auto report=add_user_package(root,options.alias,options.edit_token,registered))return print_report(*report);
        }
        if(registered)throw std::runtime_error("No selected user package registration for '"+options.alias+"'.");
        if(bundled && options.alias!="raylib")throw UsageError("No bundled library named '"+options.alias+"'.");
        if(options.alias=="raylib" && !explicit_source) {
            const auto source=bundled_raylib_source(executable);
            if(source.empty() || !vd::native_source_debug_supported())
                throw std::runtime_error("Bundled raylib is unavailable for this compiler or target. Use an explicit --path or --git source.");
            const BundledRaylib bundle{source,VOID_RAYLIB_INVENTORY,VOID_RAYLIB_SNAPSHOT_SHA256,
                VOID_RAYLIB_RECEIPT_SHA256,VOID_RAYLIB_PREPARER_SHA256};
            const auto report=add_verified_raylib(root,bundle,executable,vd::native_clang_driver(argv[0]),VOID_RAYLIB_SANITIZE,options.edit_token);
            std::cout<<(report.changed?"Installed and locked bundled raylib.":"Bundled raylib and native lock verified; reused the prepared catalog.")
                     <<"\nNative execution still needs explicit consent: review with voidc run .\n";
            return 0;
        }
        if(options.alias.empty() || bool(options.path)==bool(origin) || (!origin && (rev || subdir)))
            throw UsageError(registration?"Package install needs ALIAS with --dir or --git.":
                "Add a registered or bundled package by alias, or supply --dir or --git.");
        if(origin)options.git=GitDependency{{*origin,rev.value_or("")},subdir.value_or("")};
        if(registration) {
            const auto report=install_user_package(options,[&]{return vd::native_clang_driver(argv[0]);},VOID_RAYLIB_SANITIZE);
            std::cout<<(report.changed?"Installed user package ":"Verified existing user package ")<<options.alias<<".\n";
            return 0;
        }
    }
    if(command=="remove" && options.alias.empty())throw UsageError("Expected one dependency alias.");
    auto report=command=="add"?add_dependency(root,options,[&]{return vd::native_clang_driver(argv[0]);},
                                              VOID_RAYLIB_SANITIZE):
        command=="remove"?remove_dependency(root,options.alias,options.edit_token):tidy_dependencies(root,check);
    return print_report(report);
}
int resolve_project(int argc,char** argv) {
    using namespace vd::project;
    std::optional<std::filesystem::path> root;
    auto mode=ResolveMode::create;
    for(int i=2;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--project") {
            if(root || ++i==argc || !*argv[i] || argv[i][0]=='-')
                throw UsageError("Expected one directory after --project.");
            root=vd::utf8_path(argv[i]);
        } else if(option=="--locked" || option=="--refresh") {
            if(mode!=ResolveMode::create)throw UsageError("Use at most one of --locked and --refresh.");
            mode=option=="--locked"?ResolveMode::locked:ResolveMode::refresh;
        } else throw UsageError("Unknown resolve option: "+std::string(option));
    }
    if(!root)throw UsageError("resolve requires --project DIR.");
    switch(resolve_dependencies(*root,mode)) {
    case ResolveResult::no_dependencies: std::cout<<"No project dependencies.\n"; break;
    case ResolveResult::verified: std::cout<<"Verified void.lock.\n"; break;
    case ResolveResult::created: std::cout<<"Created void.lock.\n"; break;
    case ResolveResult::refreshed: std::cout<<"Refreshed void.lock.\n"; break;
    }
    return 0;
}
constexpr std::size_t dependency_output_limit = 4 * 1024 * 1024;

std::string dependency_json(const vd::project::DependencyExplanation* report, std::string_view error = {}) {
    std::string text;
    llvm::raw_string_ostream out(text);
    llvm::json::OStream json(out);
    auto source = [&](const std::string& path, const std::optional<vd::project::GitDependency>& git) {
        json.attributeObject("source", [&] {
            if (git) {
                json.attribute("kind", "git");
                json.attribute("origin", git->pin.origin);
                json.attribute("revision", git->pin.revision);
                json.attribute("subdir", git->subdir);
            } else {
                json.attribute("kind", "local");
                json.attribute("path", path);
            }
        });
    };
    json.object([&] {
        json.attribute("schema", 1);
        json.attribute("status", report ? "ok" : "error");
        if (!report) {
            json.attribute("message", llvm::json::fixUTF8(llvm::StringRef(error)));
            return;
        }
        json.attribute("project_id", report->project_id);
        if(report->edit_token)json.attribute("edit_token", *report->edit_token);
        json.attributeObject("lock", [&] {
            json.attribute("status", report->lock_sha256 ? "verified" : "not_required");
            if (report->lock_sha256) json.attribute("sha256", *report->lock_sha256);
        });
        json.attributeArray("packages", [&] {
            for (const auto& package : report->packages) json.object([&] {
                json.attribute("id", package.id);
                json.attribute("version", package.version);
                json.attribute("entry", package.entry);
                json.attribute("manifest_sha256", package.manifest_sha256);
                json.attribute("tree_sha256", package.tree_sha256);
                source(package.path, package.git);
            });
        });
        json.attributeArray("relationships", [&] {
            for (const auto& edge : report->relationships) json.object([&] {
                json.attribute("direct", edge.direct);
                json.attribute("parent", edge.parent);
                json.attribute("alias", edge.alias);
                json.attribute("target", edge.target);
                json.attribute("version_requirement", edge.version_requirement);
                source(edge.path, edge.git);
            });
        });
    });
    out << '\n';
    return text;
}

std::string dependency_text(const vd::project::DependencyExplanation& report) {
    std::ostringstream out;
    auto source = [&](const std::string& path, const std::optional<vd::project::GitDependency>& git) {
        if (!git) out << "local " << path;
        else {
            out << "Git " << git->pin.origin << " at " << git->pin.revision;
            if (!git->subdir.empty()) out << " subdir " << git->subdir;
        }
    };
    out << "Project " << report.project_id << '\n';
    if (report.lock_sha256) out << "Verified void.lock SHA-256 " << *report.lock_sha256 << '\n';
    else out << "No void.lock required.\n";
    if (report.packages.empty()) out << "No source package dependencies.\n";
    for (const auto& package : report.packages) {
        out << "Package " << package.id << ' ' << package.version << " [";
        source(package.path, package.git);
        out << "]\n  entry " << package.entry << "\n  manifest SHA-256 " << package.manifest_sha256
            << "\n  tree SHA-256 " << package.tree_sha256 << '\n';
    }
    for (const auto& edge : report.relationships) {
        out << (edge.direct ? "Direct     " : "Transitive ") << edge.parent << " --" << edge.alias
            << "--> " << edge.target << " (" << edge.version_requirement << "; ";
        source(edge.path, edge.git);
        out << ")\n";
    }
    return out.str();
}

int explain_project(int argc, char** argv) {
    std::optional<std::filesystem::path> root;
    bool json = false, for_edit = false;
    for (int i = 3; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--project") {
            if (root || ++i == argc || !*argv[i] || argv[i][0] == '-')
                throw UsageError("Expected one directory after --project.");
            root = vd::utf8_path(argv[i]);
        } else if (option == "--json" && !json) json = true;
        else if (option == "--for-edit" && !for_edit) for_edit = true;
        else throw UsageError("Unknown or repeated deps explain option: " + std::string(option));
    }
    if (!root || (for_edit && !json)) throw UsageError("deps explain requires --project DIR; --for-edit requires --json.");
    try {
        const auto report = vd::project::explain_dependencies(*root,for_edit);
        const auto text = json ? dependency_json(&report) : dependency_text(report);
        if (text.size() > dependency_output_limit) throw std::runtime_error("Dependency explanation exceeds the output byte limit.");
        std::cout << text;
        return 0;
    } catch (const std::exception& failure) {
        const std::string_view message = failure.what();
        constexpr auto limit_message = "Dependency diagnostic exceeds the output byte limit.";
        if (!json) {
            std::cerr << (message.size() <= dependency_output_limit ? message : limit_message) << '\n';
            return 1;
        }
        const auto text = message.size() <= dependency_output_limit ? dependency_json(nullptr, message) : std::string{};
        std::cout << (text.empty() || text.size() > dependency_output_limit
            ? dependency_json(nullptr, limit_message) : text);
        return 1;
    }
}

int fetch_project(int argc, char** argv) {
    using namespace vd::project;
    if (argc < 3 || std::string_view(argv[2]) != "fetch") throw UsageError("Expected deps explain or deps fetch.");
    std::optional<std::filesystem::path> root;
    std::vector<GitPin> allowed;
    for (int i = 3; i < argc; ++i) {
        const std::string_view option = argv[i];
        if (option == "--project") {
            if (root || ++i == argc || !*argv[i] || argv[i][0] == '-')
                throw UsageError("Expected one directory after --project.");
            root = vd::utf8_path(argv[i]);
        } else if (option == "--allow-git") {
            if (i + 2 >= argc || allowed.size() == dependency_edge_limit)
                throw UsageError("Expected --allow-git ORIGIN REV.");
            GitPin pin{argv[i + 1], argv[i + 2]};
            i += 2;
            if (!valid_git_pin(pin)) throw UsageError("Invalid --allow-git origin or revision.");
            allowed.push_back(std::move(pin));
        } else throw UsageError("Unknown deps fetch option: " + std::string(option));
    }
    if (!root) throw UsageError("deps fetch requires --project DIR.");
    const auto count = fetch_dependencies(*root, allowed);
    std::cout << "Verified Git source snapshots: " << count << '\n';
    return 0;
}
int list_templates(bool machine = false) {
    const auto installed=vd::project::list_project_templates();
    if(machine) {
        std::string text;
        llvm::raw_string_ostream out(text);
        llvm::json::OStream json(out);
        auto entry=[&](llvm::StringRef name,const auto& short_names,const auto& tags,bool builtin) {
            json.object([&] {
                json.attribute("name",name);
                json.attributeArray("shortNames",[&]{for(const auto& value:short_names)json.value(value);});
                json.attributeArray("tags",[&]{for(const auto& value:tags)json.value(value);});
                json.attribute("inPlace",builtin);
            });
        };
        json.object([&] {
            json.attribute("schema",1);
            json.attributeArray("templates",[&] {
                entry("Console Application",std::array{"app"},std::array{"Common/Console"},true);
                entry("VOID Library",std::array{"classlib","lib"},std::array{"Common/Library"},true);
                for(const auto& info:installed)entry(info.name,info.short_names,info.tags,false);
            });
        });
        std::cout<<text<<'\n';
        return 0;
    }
    std::cout<<"Template Name         Short Name  Tags\n"
        "Console Application   app         Common/Console\n"
        "VOID Library          classlib, lib  Common/Library\n";
    auto display=[](const auto& values) {
        for(std::size_t i=0;i<values.size();++i)std::cout<<(i?", ":"")<<values[i];
    };
    for(const auto& info:installed) {
        std::cout<<info.name<<"   ";
        display(info.short_names);
        std::cout<<"   ";
        display(info.tags);
        std::cout<<'\n';
    }
    std::cout<<"Use voidc new app --name NAME, voidc new classlib --name NAME or voidc new SHORT-NAME --name NAME.\n";
    return 0;
}

vd::project::CreationOptions starter_options(std::string_view starter) {
    vd::project::CreationOptions options;
    options.new_directory=true;
    options.kind=starter=="app"?vd::project::Kind::application:vd::project::Kind::library;
#if (defined(__linux__) && defined(__x86_64__)) || (defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__)))
    options.app_launch=starter=="app";
#endif
    return options;
}

vd::project::CreationPlan checked_creation_plan(const std::filesystem::path& root,const vd::project::CreationOptions& options) {
    vd::project::require_outside_user_package_store(root);
    vd::project::require_outside_native_approval_store(root,true);
    return vd::project::plan_creation(root,options);
}

int create_selected_template(int argc,char** argv) {
    if((argc!=6 && argc!=7) || std::string_view(argv[4])!="--name" ||
       (argc==7 && std::string_view(argv[6])!="--in-place"))
        throw UsageError("Use voidc new --template SHORT-NAME --name NAME [--in-place].");
    const std::string_view starter=argv[3];
    const std::string name=argv[5];
    const bool in_place=argc==7;
    if(!vd::project::valid_template_project_name(name))
        throw UsageError("Template project name must be a portable identifier beginning with a letter.");
    std::filesystem::path root;
    if(starter=="app" || starter=="classlib" || starter=="lib") {
        auto options=starter_options(starter);
        options.name=name;
        options.in_place=in_place;
        const auto plan=checked_creation_plan(in_place?vd::utf8_path("."):vd::utf8_path(name),options);
        vd::project::publish_creation(plan);
        root=plan.root;
    } else {
        if(!vd::project::valid_template_short_name(starter))throw UsageError("Invalid template short name.");
        if(in_place)throw UsageError("Installed templates require a new project directory.");
        const auto generated=vd::project::create_from_project_template(starter,name,false);
        if(!generated)throw UsageError("No installed template named "+std::string(starter)+". Run voidc new list and select again.");
        root=*generated;
    }
    std::string text;
    llvm::raw_string_ostream out(text);
    llvm::json::OStream json(out);
    json.object([&] {
        json.attribute("schema",1);
        json.attribute("root",vd::path_utf8(root));
    });
    std::cout<<text<<'\n';
    return 0;
}

std::optional<int> custom_template_command(int argc,char** argv) {
    using namespace vd::project;
    const std::string_view command=argv[2];
    if(command=="install" && argc>=4) {
        const std::string_view source=argv[3];
        if(argc==4 && (source=="--help" || source=="-h")) {help("new", command);return 0;}
        if(source=="--dir" || source=="--git") {
            if(argc!=5 || !*argv[4] || argv[4][0]=='-')
                throw UsageError("Use voidc new install with exactly one --dir PATH or --git URL.");
            auto template_info=source=="--dir"?install_project_template(vd::utf8_path(argv[4])):
                install_project_template_git(argv[4]);
            std::cout<<"Installed template "<<template_info.name<<" ("<<template_info.short_names.front()<<").\n";
            return 0;
        }
    }
    if(command=="uninstall" && argc==4) {
        const std::string_view name=argv[3];
        if(name=="--help" || name=="-h") {help("new", command);return 0;}
        if(!name.empty() && !name.starts_with('-')) {
            if(name=="app" || name=="classlib" || name=="lib")
                throw UsageError("Built-in starters cannot be uninstalled.");
            uninstall_project_template(name);
            std::cout<<"Uninstalled template "<<name<<".\n";
            return 0;
        }
    }

    if(!valid_template_short_name(command))return {};
    // Only the template grammar can select a short name. Explicit paths and
    // the ordinary project's other options retain their existing meaning.
    std::optional<std::string> name;
    bool nested=false;
    for(int i=3;i<argc;++i) {
        const std::string_view option=argv[i];
        if(option=="--name" && !name && i+1<argc && !std::string_view(argv[i+1]).starts_with("--"))
            name=argv[++i];
        else if(option=="--nested" && !nested)nested=true;
        else return {};
    }
    auto project=create_from_project_template(command,name,nested);
    if(!project)return {};
    std::cout<<"Manifest: "<<vd::path_utf8(*project/"void.project.toml")<<'\n';
    return 0;
}

int create_project(const std::string& command,int argc,char** argv) {
    vd::project::CreationOptions options;options.new_directory=command=="new";
    std::optional<std::filesystem::path> destination;
    std::set<std::string> seen;
    bool operands=false;
    int first=2;
    std::string_view starter;
    if(options.new_directory && argc>2) {
        const std::string_view word=argv[2], next=argc>3?argv[3]:"";
        if(word=="classlib" || ((word=="app" || word=="lib") &&
           (next.empty() || !next.starts_with('-') || next=="--" || next=="--name"))) {
            starter=word;
            options=starter_options(starter);
            seen.insert("--kind");
            ++first;
        }
    }
    for(int i=first;i<argc;++i) {
        std::string arg=argv[i];
        if(!operands && arg=="--"){operands=true;continue;}
        if(operands || !arg.starts_with('-')) {
            if(destination || arg.empty())throw UsageError("Expected one project destination.");
            destination=vd::utf8_path(arg);continue;
        }
        if(!seen.insert(arg).second)throw UsageError("Duplicate creation option "+arg+".");
        if(arg=="--nested"){options.nested=true;continue;}
        if(arg!="--kind" && arg!="--name" && arg!="--version" && arg!="--project-id" &&
           arg!="--app-id" && arg!="--package-id" && arg!="--package-name" && arg!="--source")
            throw UsageError("Unknown creation option "+arg+".");
        if(++i==argc || std::string_view(argv[i]).starts_with("--"))throw UsageError("Missing value for "+arg+".");
        std::string value=argv[i];
        if(arg=="--kind") {
            if(value!="application" && value!="library")throw UsageError("Expected --kind application or library.");
            options.kind=value=="application"?vd::project::Kind::application:vd::project::Kind::library;
        } else if(arg=="--name")options.name=value;
        else if(arg=="--version")options.version=value;
        else if(arg=="--project-id")options.project_id=value;
        else if(arg=="--app-id")options.application_id=value;
        else if(arg=="--package-id")options.package_id=value;
        else if(arg=="--package-name")options.package_name=value;
        else options.source=value;
    }
    const bool app=options.kind==vd::project::Kind::application;
    if(!destination && !starter.empty()) {
        destination=vd::utf8_path(options.name.value_or(std::string(starter)));
        if(destination->has_parent_path() || destination->has_root_path() ||
           destination->empty() || *destination=="." || *destination=="..")
            throw UsageError("Use a directory name with --name or supply a separate destination PATH.");
    }
    if(!destination && options.new_directory)throw UsageError("new requires a destination PATH.");
    options.in_place=options.new_directory && (*destination=="." || *destination=="./");
    if(options.source && !options.new_directory)throw UsageError("--source is only available with new.");
    if((app && (options.package_name || options.package_id)) || (!app && options.application_id))
        throw UsageError("Identity options must match the selected project kind.");
    const auto root=destination?*destination:std::filesystem::current_path();
    const auto plan=checked_creation_plan(root,options);
    const auto manifest=plan.root/"void.project.toml";
    std::cout<<"Manifest: "<<vd::path_utf8(manifest)<<"\nProject ID: "<<plan.manifest->project().id<<'\n'
             <<(app?"Application ID: ":"Package ID: ")<<(app?plan.manifest->application()->id:plan.manifest->package()->id)
             <<"\nNext: from the displayed manifest directory, run voidc check --project .\n"<<std::flush;
    vd::project::publish_creation(plan);
    std::cout<<"Created project manifest.\n";
    if(app)std::cout<<"Automatic application selection requires one eligible main(). Use --entry for a deliberate override.\n";
    if(starter=="app") {
        std::cout<<"Run the starter from its directory with voidc run . or use VOID: Run Project in VS Code.\n";
        if(options.app_launch) {
#ifdef _WIN32
            std::cout<<"For F5 on Windows x64, install Microsoft C/C++ in VS Code and put native GDB 17.2 on PATH. F5 builds the Debug executable before launching.\n";
#else
            std::cout<<"For F5 on Linux x64, configure the LLVM LLDB DAP extension. F5 builds the Debug executable before launching.\n";
#endif
        }
    }
    return 0;
}
void generate_native_binding(int argc, char** argv) {
    if (argc != 6 || std::string_view(argv[4]) != "-o")
        throw UsageError("Expected native-bind <descriptor> <C-header> -o <adapter.c>.");
    const auto descriptor = vd::utf8_path(argv[2]);
    const auto output = vd::utf8_path(argv[5]);
    vd::project::require_outside_user_package_store(output);
    if (output.extension() != ".c" && output.extension() != ".void")
        throw UsageError("Expected a new .c or .void output path.");
    if (!std::filesystem::is_regular_file(descriptor))
        throw std::runtime_error("Missing native descriptor: " + vd::path_utf8(descriptor));
    std::ifstream source(descriptor, std::ios::binary);
    std::string bytes(vd::c_conventional_descriptor_limit + 1, '\0');
    source.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (source.bad() || !source.eof() || source.gcount() == 0)
        throw std::runtime_error("Cannot read native descriptor (outside conventional descriptor bounds).");
    bytes.resize(static_cast<std::size_t>(source.gcount()));
    std::string generated;
    if(output.extension()==".void") {
        generated=vd::CPrimitiveAdmission::generate_declarations(bytes);
    } else generated=vd::CPrimitiveAdmission::generate_adapter(bytes, argv[3]);
    int fd;
    const auto name = vd::path_utf8(output);
    if (auto error = llvm::sys::fs::openFileForWrite(name, fd, llvm::sys::fs::CD_CreateNew))
        throw std::runtime_error("Cannot create C adapter " + name + ": " + error.message());
    llvm::raw_fd_ostream file(fd, true);
    file << generated;
    file.close();
    if (file.has_error()) {
        file.clear_error();
        std::error_code ignored;
        std::filesystem::remove(output, ignored);
        throw std::runtime_error("Cannot write C adapter: " + name);
    }
}
}
static int execute_cli(int argc, char** argv, vd::cli::Presentation& presentation) {
    std::string command;
    try {
        if(argc>=2 && (std::string_view(argv[1])=="init" || std::string_view(argv[1])=="new")) {
            command=argv[1];
            if(argc==3 && (std::string_view(argv[2])=="--help" || std::string_view(argv[2])=="-h")){help(command);return 0;}
            if(command=="new") {
                if(argc==2 || (argc==3 && std::string_view(argv[2])=="list"))return list_templates();
                if(std::string_view(argv[2])=="list" && argc==4 && std::string_view(argv[3])=="--json")
                    return list_templates(true);
                if(std::string_view(argv[2])=="--template")return create_selected_template(argc,argv);
                if(const auto result=custom_template_command(argc,argv))return *result;
            }
            return create_project(command,argc,argv);
        }
        // Preserve the established early removed-option diagnostic, but never
        // scan the explicit application's forwarded argument region.
        bool source_seen=false, source_only=false, output_operand=false;
        for(int i=1;i<argc;++i) {
            const std::string argument=argv[i];
            if(i>=2 && std::string(argv[1])=="run") {
                if(argument=="--" && source_seen)break;
                if(argument=="--"){source_only=true;continue;}
                if(argument=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
                if(output_operand){output_operand=false;continue;}
                if(argument=="-o" && !source_only){output_operand=true;continue;}
                if(source_only || !argument.starts_with("-") ||
                   (argument.size()>1 && argument[1]>='0' && argument[1]<='9'))source_seen=true;
            }
            if(argument=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        }
        if(argc==1 || (argc==2 && (std::string(argv[1])=="--help" || std::string(argv[1])=="-h"))) {help();return 0;}
        if(argc==2 && std::string(argv[1])=="--version") {
            std::cout<<"VOID Language Compiler v"<<VOID_COMPILER_VERSION<<" (development)\nLLVM "<<LLVM_VERSION_STRING
              <<"\nTarget: "<<LLVM_DEFAULT_TARGET_TRIPLE<<"\nBuild identity: "<<vd::compiler_build_identity()<<'\n';
            return 0;
        }
        if(argc==2 && std::string_view(argv[1])=="--capabilities") {
            std::cout << "{\"schema\":1,\"cliPresentation\":1,\"statusPlain\":true,\"nativeApprovalStdio\":1,\"debugInfoOverride\":1,\"timingsSchema\":1,\"publishContract\":1,\"publishEmission\":true}\n";
            return 0;
        }
        if(argc>=2 && std::string(argv[1])=="help") {
            if(argc==2) {help();return 0;}
            if(argc!=3 || !known_command(argv[2]))throw UsageError("Expected help [crashes|check|run|build|package|emit|init|new|add|remove|tidy|resolve|deps|native-bind|native].");
            help(argv[2]);return 0;
        }
        if (argc>=2 && (std::string_view(argv[1])=="check" || std::string_view(argv[1])=="run" ||
            std::string_view(argv[1])=="build" || std::string_view(argv[1])=="emit" || std::string_view(argv[1])=="package"))
            presentation.begin(argv[1]);
        const auto executable=vd::utf8_path(llvm::sys::fs::getMainExecutable(argv[0],reinterpret_cast<void*>(&execute_cli)));
        vd::configure_standard_library_root(executable);
        if(argc==3 && std::string(argv[1])=="--emit-proof") {
            vd::project::require_outside_user_package_store(vd::utf8_path(argv[2]));
            vd::emit_object(vd::ir::architecture_proof(),vd::utf8_path(argv[2]),true);
            std::cout<<"Wrote native architecture proof object\n";return 0;
        }
        if(argc==2 && std::string(argv[1])=="--jit-proof") {
            vd::crash::context(vd::crash::Operation::jit_compile);
            vd::Jit jit;auto code=jit.compile(vd::ir::architecture_proof(),"entry");
            vd::crash::context(vd::crash::Operation::jit_execute);
            auto result=code->invoke(0);vd::crash::context(vd::crash::Operation::finished);std::cout<<result<<'\n';return result==42?0:1;
        }
        command=argv[1];
        if(command=="--app")throw UsageError("--app has been removed; entry selection is automatic.");
        if(!known_command(command))
            throw UsageError("Unknown command: "+command);
        if(argc==3 && (std::string(argv[2])=="--help" || std::string(argv[2])=="-h")) {help(command);return 0;}
        if(command=="crashes")throw UsageError("Expected crashes run ABSOLUTE-EXECUTABLE [arguments...].");
        if (command == "target") {
            std::vector<std::string_view> args;
            for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
            return vd::publish::pack_command(args);
        }
        if(command=="publish") {
            std::vector<std::string_view> args;
            for(int i=2;i<argc;++i)args.emplace_back(argv[i]);
            if (args.size() == 4 && args[0] == "--resolve-diagnostic") {
                const auto map = vd::project::read_project_input(vd::utf8_path(args[1]), vd::diagnostics::map_byte_limit);
                const auto image = vd::project::read_project_input(vd::utf8_path(args[2]), vd::audit::byte_limit);
                std::string token(args[3]);
                if (!token.ends_with('\n')) token += '\n';
                const auto site = vd::diagnostics::PrivateMap::resolve(map, image, token);
                std::cout << site.path << ':' << site.line << ':' << site.column << ": " << site.description
                          << '\n' << site.function << '\n' << site.excerpt << '\n';
                return 0;
            }
            const llvm::Triple host(LLVM_DEFAULT_TARGET_TRIPLE);
            const auto arch = host.getArch();
            const auto system = host.isOSLinux() ? std::optional{vd::publish::System::linux_gnu} :
                host.isOSWindows() ? std::optional{vd::publish::System::windows_msvc} :
                host.isMacOSX() ? std::optional{vd::publish::System::macos} : std::nullopt;
            std::string_view target = "unqualified-host";
            if (system && (arch == llvm::Triple::x86_64 || arch == llvm::Triple::aarch64)) {
                for (const auto& candidate : vd::publish::targets)
                    if (candidate.system == system && ((arch == llvm::Triple::aarch64) ==
                        (candidate.architecture == vd::publish::Architecture::arm64))) target = candidate.id;
            }
            if (args.size() == 2 && args[0] == "--verify") {
                vd::verify_native_publication(vd::utf8_path(args[1]));
                std::cout << "Native publication verified.\n";
                return 0;
            }
            if (args.size() == 4 && args[0] == "--bundle" && args[2] == "-o") {
                vd::archive_macos_application(vd::utf8_path(args[1]), vd::utf8_path(args[3]));
                std::cout << "Published macOS bundle ZIP.\n";
                return 0;
            }
            auto request=vd::publish::parse_arguments(args);
            vd::project::require_outside_user_package_store(request.project);
            const auto plan = vd::publish::preflight(request, target);
            const auto pack = plan.target() == target ? std::optional<vd::publish::Pack>{} :
                std::optional{vd::publish::load_pack(plan.target())};
            auto result = vd::analyze_manifest_project_with_native_approval(request.project, plan.entry(),
                [&](const vd::ManagedNativeApprovalSubject& subject) -> std::filesystem::path {
                    if (pack)
                        throw vd::NativeApprovalRequired("Managed native dependencies require a destination-approved catalog variant. Target packs currently publish applications without managed native dependencies.");
                    const auto& identity = subject.dependencies.native_catalog_identity();
                    if (request.native_catalog != identity)
                        throw vd::NativeApprovalRequired("Review redistribution rights and native catalog contents. "
                            "Publish requires --native-catalog " + identity + " for this exact catalog.");
                    return {};
                });
            if (!result.succeeded()) throw vd::CompilationError(std::move(result.diagnostics));
            vd::project::Digest manifest;
            manifest.bytes(result.manifest->source().text());
            if (manifest.finish() != plan.manifest_sha256())
                throw std::runtime_error("Project manifest changed after publishing preflight. Retry.");
            const auto& program = *result.project->program();
            vd::require_source_entry(program);
            for (const auto& warning : program.warnings()) std::cerr << vd::render_diagnostic(warning) << '\n';
            vd::crash::context(vd::crash::Operation::aot_build);
            const auto archive = vd::publish_native_application(*result.project, plan, argv[0], pack ? &*pack : nullptr);
            std::cout << "Private diagnostics: " << vd::path_utf8(archive) << '\n';
            return 0;
        }
        if(command=="add" || command=="remove" || command=="tidy")return maintain_project(command,argc,argv,executable);
        if(command=="resolve")return resolve_project(argc,argv);
        if(command=="deps") {
            if(argc==4 && (std::string_view(argv[2])=="explain" || std::string_view(argv[2])=="fetch") &&
               (std::string_view(argv[3])=="--help" || std::string_view(argv[3])=="-h")) { help(command, argv[2]); return 0; }
            if(argc>=3 && std::string_view(argv[2])=="explain")return explain_project(argc,argv);
            return fetch_project(argc,argv);
        }
        if(command=="native") {
            if(argc==3 && std::string_view(argv[2])=="approvals") {
                const auto approvals=vd::native_approvals();
                if(approvals.empty())std::cout<<"No native approvals.\n";
                for(const auto& record:approvals)std::cout<<record<<'\n';
                return 0;
            }
            if(argc==4 && std::string_view(argv[2])=="revoke"){vd::revoke_native_approval(argv[3]);return 0;}
            if(argc==4 && std::string_view(argv[2])=="import" && std::string_view(argv[3])=="--help") {help(command, argv[2]);return 0;}
            if(argc<4 || std::string_view(argv[2])!="import")throw UsageError("Expected native import <header> --provider ID --out NEW-DIR.");
            vd::NativeImportOptions options;options.header=vd::utf8_path(argv[3]);std::set<std::string> seen;
            for(int i=4;i<argc;++i) {
                std::string key=argv[i];if(key=="--") {while(++i<argc)options.preprocessing.emplace_back(argv[i]);break;}
                if(!seen.insert(key).second || i+1==argc)throw UsageError("Duplicate option or missing native import operand: "+key);
                std::string value=argv[++i];
                if(key=="--provider")options.provider=value;
                else if(key=="--out")options.output=vd::utf8_path(value);
                else if(key=="--source")options.source=value;
                else if(key=="--clang")options.clang=vd::utf8_path(value);
                else if(key=="--language")options.language=value;
                else throw UsageError("Unknown native import option: "+key);
            }
            if(options.provider.empty() || options.output.empty())throw UsageError("Native import requires --provider and --out.");
            vd::project::require_outside_user_package_store(options.output);
            if(options.clang.empty())options.clang=vd::native_clang_driver(argv[0]);
            vd::import_native_header(options);return 0;
        }
        if(command=="native-bind") {generate_native_binding(argc,argv);return 0;}
        if(command=="package" && argc>=3 &&
           (std::string_view(argv[2])=="list" || std::string_view(argv[2])=="search"))
            return discover_packages(executable,argc,argv);
        if(command=="package" && argc>=3 &&
           (std::string_view(argv[2])=="install" || std::string_view(argv[2])=="uninstall")) {
            if(argc==4 && std::string_view(argv[3])=="--help") {help(command, argv[2]);return 0;}
            return maintain_project(std::string_view(argv[2])=="install"?"add":"remove",argc,argv,executable,true);
        }
        if(command=="package" && argc>=3 && std::string_view(argv[2])=="--verify") {
            if(argc!=4 || !*argv[3] || argv[3][0]=='-')throw UsageError("Expected one directory after package --verify.");
            vd::verify_application_package(vd::utf8_path(argv[3]));
            std::cout<<"Application package verified\n";
            return 0;
        }
        vd::cli::PresentationOptions presentation_options;
        bool optimize=true,cache=false,no_cache=false,no_opt=false;
        bool allow_native=false,reuse_native_approval=false,editor_native_approval=false;
        bool replace_package=false;
        std::string editor_approval_session;
        bool positional_only=false;
        bool forwarded=false;
        std::vector<std::string> application_arguments;
        std::vector<std::string> operands;std::optional<std::filesystem::path> output;
        std::optional<std::filesystem::path> project_root;
        std::optional<std::string> project_entry;
        std::optional<vd::project::ProfileName> profile;
        std::string profile_key;
        std::optional<vd::project::DebugInfo> source_debug,debug_override;
        for(int i=2;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--" && command=="run" && (!operands.empty() || project_root)) {
                if(operands.size()>1)throw UsageError("Application arguments require one explicit source or project selector.");
                forwarded=true;
                for(++i;i<argc;++i)application_arguments.emplace_back(argv[i]);
                break;
            }
            if(arg=="--app" && !positional_only)throw UsageError("--app has been removed; entry selection is automatic.");
            if(positional_only)operands.push_back(arg);
            else if(arg=="--")positional_only=true;
            else if(arg=="--timings") {
                if(presentation_options.timings)throw UsageError("Duplicate --timings.");
                presentation_options.timings=true;
            }
            else if(arg=="--status" || arg=="--timings-format" || arg=="--timings-output") {
                if(++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one value after "+arg+".");
                if(arg=="--status") {
                    if(presentation_options.status_seen)throw UsageError("Duplicate --status.");
                    presentation_options.status_seen=true;presentation_options.status=argv[i];
                } else if(arg=="--timings-format") {
                    if(presentation_options.format_seen)throw UsageError("Duplicate --timings-format.");
                    presentation_options.format_seen=true;presentation_options.format=argv[i];
                } else {
                    if(presentation_options.output)throw UsageError("Duplicate --timings-output.");
                    presentation_options.output=vd::utf8_path(argv[i]);
                }
            }
            else if(arg=="--portable" && command=="build")
                throw UsageError("Use voidc publish --portable --project DIR to produce a portable distribution.");
            else if(arg=="--no-opt") {if(no_opt)throw UsageError("Duplicate --no-opt.");no_opt=true;optimize=false;}
            else if(arg=="--cache") {if(cache)throw UsageError("Duplicate --cache.");cache=true;optimize=false;}
            else if(arg=="--no-cache") {if(no_cache)throw UsageError("Duplicate --no-cache.");no_cache=true;}
            else if(arg=="--allow-native") {if(allow_native)throw UsageError("Duplicate --allow-native.");allow_native=true;}
            else if(arg=="--debug-info") {
                if(debug_override || ++i==argc)throw UsageError("Expected one --debug-info none|line|full.");
                const std::string_view mode=argv[i];
                if(mode=="none")debug_override=vd::project::DebugInfo::none;
                else if(mode=="line")debug_override=vd::project::DebugInfo::line;
                else if(mode=="full")debug_override=vd::project::DebugInfo::full;
                else throw UsageError("Expected --debug-info none|line|full.");
            }
            else if(arg=="--native-approval-stdio") {
                if(editor_native_approval || ++i==argc)throw UsageError("Expected one --native-approval-stdio session token.");
                editor_approval_session=argv[i];
                if(editor_approval_session.size()!=64 || !std::ranges::all_of(editor_approval_session,[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
                    throw UsageError("Expected a 64-character lowercase hexadecimal editor session token.");
                editor_native_approval=true;
            }
            else if(arg=="--reuse-native-approval") {
                if(reuse_native_approval)throw UsageError("Duplicate --reuse-native-approval.");
                reuse_native_approval=true;
            }
            else if(arg=="--replace" && command=="package") {
                if(replace_package)throw UsageError("Duplicate --replace.");
                replace_package=true;
            }
            else if(arg=="--project") {
                if(project_root || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one directory after --project.");
                project_root=vd::utf8_path(argv[i]);
            }
            else if(arg=="--profile") {
                if(profile || ++i==argc)throw UsageError("Expected one debug|release value after --profile.");
                const std::string value=argv[i];
                if(value=="debug")profile=vd::project::ProfileName::debug;
                else if(value=="release")profile=vd::project::ProfileName::release;
                else throw UsageError("--profile requires exactly debug or release.");
            }
            else if(arg=="--entry") {
                if(project_entry || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one manifest-relative source after --entry.");
                project_entry=argv[i];
            }
            else if(arg=="-o") {
                if(output || ++i==argc || !*argv[i] || argv[i][0]=='-')throw UsageError("Expected one output path after -o.");
                output=vd::utf8_path(argv[i]);
            } else if(arg.starts_with("--") || (arg.starts_with("-") && (arg.size()<2 || arg[1]<'0' || arg[1]>'9')))
                throw UsageError("Unknown option: "+arg);
            else operands.push_back(arg);
        }
        if(cache && command!="run")throw UsageError("--cache is available only for Debug JIT runs.");
        if(no_cache && command!="run")throw UsageError("--no-cache is available only for JIT runs.");
        if(cache && no_cache)throw UsageError("--cache and --no-cache cannot be combined.");
        if(output && command!="emit" && command!="build" && command!="package")throw UsageError("-o is available only for emit/build/package.");
        if(command=="emit" && !output)throw UsageError("emit requires -o <object>.");
        if(project_root && !operands.empty())throw UsageError("--project cannot accompany a positional file or directory selector.");
        if((command=="check" || command=="emit") && operands.empty() && !project_root)throw UsageError("Expected a file or directory.");
        if(operands.size()>(command=="run"?2u:1u))throw UsageError("Too many operands.");
        // Parse legacy input syntax before analysis, but its admission follows classification.
        auto value=operands.size()==2?input(operands[1].c_str()):0;
        auto path=project_root?*project_root:operands.empty()?std::filesystem::current_path():vd::utf8_path(operands[0]);
        vd::project::require_outside_user_package_store(path);
        if(output) {
            vd::project::require_outside_user_package_store(*output);
            if(command=="package")vd::project::require_outside_native_approval_store(*output,true);
        }
        const bool project_mode=project_root || (std::filesystem::is_directory(path) && vd::project_manifest_present(path));
        if(command=="package" && !project_mode)throw UsageError("Package requires a manifest-selected application project.");
        if(allow_native && (!project_mode || command=="check"))
            throw UsageError("--allow-native requires a manifest-selected run, build or emit.");
        if(editor_native_approval && (allow_native || reuse_native_approval || !project_mode || (command!="run" && command!="build")))
            throw UsageError("--native-approval-stdio requires a manifest-selected run or build without other native authorization flags.");
        if(reuse_native_approval && (allow_native || !project_mode || (command!="run" && command!="build")))
            throw UsageError("--reuse-native-approval requires a manifest-selected run or build without --allow-native.");
        if(debug_override && (!project_mode || (command!="build" && command!="emit")))
            throw UsageError("--debug-info requires a manifest-selected build or emit.");
        if(profile && !project_mode)throw UsageError("--profile requires a manifest-selected project context.");
        if(project_entry && !project_mode)throw UsageError("--entry requires a manifest-selected project context.");
        try { presentation.configure(std::move(presentation_options),path,output); }
        catch(const std::exception& error) {throw UsageError(error.what());}
        presentation.project_context(project_mode);
        if(!project_mode)presentation.configuration(false,{},optimize,"none");
        vd::crash::context(vd::crash::Operation::frontend);
        vd::observation::Scope frontend(vd::observation::Phase::frontend);
        std::shared_ptr<const vd::ProjectAnalysis> project;
        std::filesystem::path selected;
        vd::AnalysisResult analysis;
        vd::NativeApproval approval(editor_native_approval?vd::NativeApproval::Mode::editor:reuse_native_approval?vd::NativeApproval::Mode::reuse_only:vd::NativeApproval::Mode::terminal,editor_approval_session);
        if(project_mode) {
            const bool durable=!allow_native && (command=="run" || command=="build");
            auto result=durable?vd::analyze_manifest_project_with_native_approval(path,project_entry,
                [&](const auto& subject){return approval.authorize(subject,command=="run");}):
                vd::analyze_manifest_project(path,project_entry,{},{},allow_native);
            if(!result.succeeded())frontend.fail();
            frontend.finish();
            if(result.manifest && result.manifest->kind()==vd::project::Kind::library && project_entry)throw UsageError("--entry cannot select an application entry in a library project.");
            if(result.manifest) {
                vd::project::EffectiveProfile selected_profile;
                const bool supports_debug=vd::native_source_debug_supported() &&
                    (command=="check" || command=="build" || command=="emit");
                try{selected_profile=vd::project::resolve_profile(*result.manifest,profile,no_opt,cache,
                    supports_debug?vd::project::ProfileDebugSupport::available:vd::project::ProfileDebugSupport::unavailable);}
                catch(const std::invalid_argument& error){throw UsageError(error.what());}
                if(debug_override) {
                    if(!supports_debug)throw UsageError("Native debug information is unavailable for this target.");
                    selected_profile.debug_info=*debug_override;
                    selected_profile.cache_key+=":debug-info-override:"+std::to_string(static_cast<int>(*debug_override));
                }
                optimize=selected_profile.optimize;profile_key=selected_profile.cache_key;
                presentation.configuration(true,selected_profile.name==vd::project::ProfileName::debug?"debug":"release",optimize,
                    selected_profile.debug_info==vd::project::DebugInfo::none?"none":!supports_debug?"unavailable":
                    selected_profile.debug_info==vd::project::DebugInfo::line?"line":"full");
                if(supports_debug && command!="check")source_debug=selected_profile.debug_info;
                for(const auto& warning:selected_profile.diagnostics)std::cerr<<vd::render_diagnostic(warning)<<'\n';
            }
            if(!result.succeeded()) {
                if(reuse_native_approval)for(auto& diagnostic:result.diagnostics) {
                    if(diagnostic.code!=vd::DiagnosticCode::project_changed)continue;
                    if(!diagnostic.hint.empty())diagnostic.hint+=' ';
                    diagnostic.hint+="After verifying an intentional native provider change, run 'voidc run .' in an ordinary interactive terminal to review approval.";
                }
                throw vd::CompilationError(std::move(result.diagnostics));
            }
            if((!result.manifest->native().empty() || result.manifest->native_requirement()) &&
                command!="check" && !allow_native && !approval.authorized())
                throw UsageError("This project declares native code. Pass --allow-native to authorize its execution.");
            project=std::move(result.project);
            if(project->manifest()->kind()==vd::project::Kind::library) {
                if(command=="package")throw UsageError("Package requires an application project.");
                if(command!="check")throw std::invalid_argument("Library project run/build/emit is unsupported; use check for source analysis.");
                for(const auto& warning:result.diagnostics)std::cerr<<vd::render_diagnostic(warning)<<'\n';
                frontend.finish();presentation.build_finished();std::cout<<"Library project checked successfully\n";return 0;
            }
            selected=*project->entry();analysis={project->program(),std::move(result.diagnostics)};
        } else {
            selected=std::filesystem::is_directory(path)?vd::discover_project(path).entry:path;
            analysis=vd::analyze_project(selected);
            if(!analysis.succeeded())frontend.fail();
            frontend.finish();
        }
        if(!analysis.succeeded())throw vd::CompilationError(std::move(analysis.diagnostics));
        for(const auto& warning:analysis.program->warnings())std::cerr<<vd::render_diagnostic(warning)<<'\n';
        frontend.finish();
        if(command=="check") {presentation.build_finished();std::cout<<"Source checked successfully\n";return 0;}
        const bool application=analysis.program->application();
        if(forwarded && !application) {
            if(application_arguments.size()>1)throw UsageError("Legacy entry accepts one optional decimal i32 input.");
            value=application_arguments.empty()?0:input(application_arguments[0].c_str());
            application_arguments.clear();
        }
        if(application && operands.size()==2)throw UsageError("Application entries do not accept arguments.");
        vd::require_source_entry(*analysis.program);
        approval.remember();
        approval.finish_editor_review();
        if(command=="package") {
            if(!output)output=project->root()/"dist";
            vd::project::require_outside_native_approval_store(*output,true);
            vd::build_application_package(*project,*output,optimize,argv[0],profile_key,replace_package);
            presentation.build_finished();
            std::cout<<"Packaged application: "<<vd::path_utf8(*output)<<'\n';
            return 0;
        }
        if(command=="run") {
            struct ApplicationDirectory {
                std::filesystem::path previous;
                explicit ApplicationDirectory(const std::filesystem::path& root) {
                    if(!root.empty()) {previous=std::filesystem::current_path();std::filesystem::current_path(root);}
                }
                ~ApplicationDirectory() {
                    if(!previous.empty()) {std::error_code ignored;std::filesystem::current_path(previous,ignored);}
                }
            } working_directory(project?project->root():std::filesystem::path{});
            vd::crash::context(vd::crash::Operation::jit_compile);
            std::unique_ptr<vd::Jit> jit;
            if(!no_cache)jit=std::make_unique<vd::Jit>(vd::DebugCache{project?project->root():std::filesystem::canonical(selected.parent_path().empty()?std::filesystem::path("."):selected.parent_path()),std::string(vd::compiler_build_identity())+(project?":project-selection-v1:"+project->selection_key()+":"+profile_key:"")},vd::ir::Emission::entry);
            else jit=std::make_unique<vd::Jit>(vd::DebugCache{{},std::string(vd::compiler_build_identity())+(project?":"+profile_key:"")},vd::ir::Emission::entry);
            auto code=jit->compile(*analysis.program,optimize);
            if(auto runtime=code->runtime_owner())
                runtime->application_arguments(vd::checked_application_arguments(std::move(application_arguments)));
            presentation.cache_status(jit->cache_status());
            if(cache)std::cerr<<"Debug cache: "<<jit->cache_status()<<'\n';
            presentation.build_finished();
            presentation.application_begin(application);
            vd::crash::context(vd::crash::Operation::jit_execute);
            auto status=code->invoke(value);
            vd::crash::context(vd::crash::Operation::finished);
            presentation.application_finished(status);
            if(application)return status;
            std::cout<<status<<'\n';return 0;
        }
        vd::crash::context(vd::crash::Operation::aot_build);
        if(command=="build") {
            if(!output) {
                auto entry=std::filesystem::absolute(selected);
                auto name=entry.stem();
#ifdef _WIN32
                name+=".exe";
#endif
                output=project?vd::project_build_directory(project->root())/name:entry.parent_path()/"build"/name;
            }
            vd::project::require_outside_native_approval_store(*output);
            vd::build_executable(*analysis.program,*output,optimize,argv[0],profile_key,source_debug);
            presentation.build_finished();
            std::cout<<"Built native executable: "<<vd::path_utf8(*output)<<'\n';
        } else {
            vd::publish_object(*analysis.program,*output,optimize,profile_key,source_debug);
            presentation.build_finished();
            std::cout<<"Wrote native source object: "<<vd::path_utf8(*output)<<'\n';
        }
        return 0;
    } catch(const vd::NativeApprovalRequired& error) {
        presentation.failure("approval_required");
        std::cerr<<error.what()<<'\n';return 2;
    } catch(const UsageError& error) {
        presentation.failure("invocation_error");
        std::cerr<<"Invalid invocation: "<<error.what()<<'\n';
        usage(std::cerr,known_command(command)?command:std::string{});
        std::cerr<<"Try 'voidc "<<(known_command(command)?command+" ":std::string{})<<"--help' for examples.\n";
        return 2;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}

static int run_cli(int argc, char** argv) {
    const bool external=argc>=4 && std::string_view(argv[1])=="crashes" && std::string_view(argv[2])=="run";
    if(auto status=vd::crash::start(external?"aot-program":"voidc",external?argv[3]:argv[0],vd::compiler_build_identity(),external))return *status;
    vd::crash::context(vd::crash::Operation::cli);
    if(external) {
        try{return vd::crash::execute_external(argc-3,argv+3);}
        catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
    }
    vd::cli::Presentation presentation;
    auto code=execute_cli(argc,argv,presentation);
    presentation.finish(code);
    return code;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    try {
        // Keep the shared CLI in UTF-8; never pass native arguments through ACP.
        std::vector<std::string> arguments;
        arguments.reserve(argc);
        for(int i=0;i<argc;++i) {
            auto text=std::filesystem::path(argv[i]).u8string();
            arguments.emplace_back(text.begin(),text.end());
        }
        std::vector<char*> pointers;
        for(auto& argument:arguments)pointers.push_back(argument.data());
        pointers.push_back(nullptr);
        return run_cli(argc,pointers.data());
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
#else
int main(int argc, char** argv) { return run_cli(argc,argv); }
#endif
