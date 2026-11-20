#include "void/tooling.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool evaluate(std::string text,const vd::BuildConditions& c){vd::SourceFile source("<predicate>",text);return vd::evaluate_build_condition(source,0,text.size(),c);}
void targets(){
    auto windows=vd::target_build_conditions("x86_64-pc-windows-msvc");
    auto mac=vd::target_build_conditions("arm64-apple-darwin25.0.0");
    auto linux=vd::target_build_conditions("x86_64-unknown-linux-gnu");
    check(evaluate("target_os == \"windows\" and target_arch == \"x86_64\"",windows),"Windows target predicates");
    check(evaluate("target_os == \"macos\" and target_arch == \"aarch64\"",mac),"Darwin/ARM64 target normalization");
    check(!evaluate("target_os == \"windows\"",linux),"Target selection independent of host");
    check(windows.key()!=linux.key() && mac.key()!=linux.key(),"target cache identity");
    linux.features={{"one",true},{"two",false}};
    check(evaluate("not feature.two and feature.one or false",linux),"logical precedence");
    check(!evaluate("not (feature.one or feature.two)",linux),"grouping");
    bool rejected=false;try{evaluate("true or feature.unknown",linux);}catch(const vd::CompilationError&){rejected=true;}
    check(rejected,"Unknown operands validated without short circuit");
    auto source=vd::SourceFile("bad.void","compile if true:\n    compile if false:\n        pass\ncompile else:\n    pass\nfunc main():\n    pass\n");
    check(vd::analyze_source(source).succeeded(),"nested source selection");
}
const std::string manifest="schema=1\n[project]\nid='org.test.conditions'\nname='Conditions'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.conditions.app'\n[build.conditions]\nextra=";
const std::string source="func selected() -> i32:\n    compile if feature.extra:\n        return 1\n    compile else:\n        return 2\nfunc main():\n    print(selected())\n";
void sessions(){
    namespace fs=std::filesystem;const auto root=fs::temp_directory_path()/("void-g87-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);}} cleanup{root};
    {std::ofstream(root/"void.project.toml")<<manifest+"true\n";std::ofstream(root/"launcher.void")<<source;}
    vd::ToolingSession session(vd::ProjectSession{root});
    auto first=vd::ToolingSession::analyze(session.select());check(first->succeeded() && session.accept(first),"initial analysis");
    const auto before=first->result().program;const auto snapshot=session.select();
    session.open("/void.project.toml",1,manifest+"false\n");
    auto next=vd::ToolingSession::analyze(session.select());check(next->succeeded() && session.accept(next),"dirty manifest condition selection");
    const auto after=next->result().program;
    check(!session.requires_reselection(),"feature edits retain the selected project session");
    check(before->compatibility_contract()!=after->compatibility_contract(),"same signature/source, new conditions require restart");
    check(first->project()->project->selection_key()!=next->project()->project->selection_key(),"cache selection changes");
    auto plan=vd::plan_invalidation(*before,{},after.get());
    check(plan.source_compatibility==vd::SourceCompatibility::restart_required,"reload condition invalidation");
    check(!session.accept(vd::ToolingSession::analyze(snapshot)),"reject late old-configuration analysis");
    vd::ToolingIndex index(after);const auto tokens=index.semantic_tokens("/launcher.void");
    bool inactive=false,directive=false;
    for(const auto& token:tokens){inactive|=bool(token.modifiers & vd::SemanticToken::inactive);directive|=token.kind==vd::SemanticToken::Kind::keyword;}
    check(inactive && directive,"inactive source and directive semantic metadata");
    const auto at=source.find("return 1");
    check(index.definitions_at("/launcher.void",at).empty(),"inactive reference has no navigation");
    const auto assistance=vd::project_editor_assistance(root,{}, {},{{"/void.project.toml",vd::SourceFile("manifest",manifest+"false\n")}},"/launcher.void",at+2);
    check(assistance.candidates.empty(),"inactive source has no completion");
    check(before->modules().front().conditions.features.at("extra"),"old snapshot remains immutable");
    const auto native=root/"native.void";
    {std::ofstream(native)<<"compile if true:\n    use \"/native_bridge.void\" as bridge\ncompile else:\n    use \"/unavailable_native.void\"\nfunc entry(value: i32) -> i32:\n    return bridge.scalar_i32(value)\n";}
    auto admitted=vd::analyze_project(native,vd::native_fixture_project());
    check(admitted.succeeded(),"selected native import uses existing explicit admission");
    auto lowered=admitted.program->lower();check(lowered.source_imports.at("/native.void").contains("/native_bridge.void"),"selected native dependency retained in typed IR");
    {std::ofstream(native)<<"compile if false:\n    use \"/native_bridge.void\" as bridge\nfunc entry(value: i32) -> i32:\n    return value\n";}
    auto excluded=vd::analyze_project(native,vd::native_fixture_project());
    check(excluded.succeeded() && excluded.program->modules().size()==1,"inactive native import contributes no module or binding");

}
}
int main(){try{targets();sessions();std::cout<<"G87 target/configuration/session/reload checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
