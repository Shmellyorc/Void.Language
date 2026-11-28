#include "void/publish.hpp"
#include "void/binary_audit.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <type_traits>

namespace {
unsigned checks=0;
void check(bool value,const char* what) {++checks;if(!value)throw std::runtime_error(what);}
template<class F> void rejected(F action,const char* what) {
    bool failed=false;try {action();}catch(const std::exception&) {failed=true;}check(failed,what);
}
const std::string app="schema=1\n[project]\nid='org.test.sources'\nname='Fixture'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n";
auto parse(std::string bytes) {return vd::project::parse_manifest(vd::SourceFile("/fixture/void.project.toml",std::move(bytes)));}
}
int main(int argc,char** argv) {
    try {
        if(argc>=4 && (std::string_view(argv[1])=="audit" || std::string_view(argv[1])=="audit-stage-b")) {
            std::ifstream input(argv[3],std::ios::binary|std::ios::ate);
            if(!input || input.tellg()<0 || input.tellg()>std::streamoff(vd::audit::byte_limit))return 2;
            std::string bytes(static_cast<std::size_t>(input.tellg()),'\0');input.seekg(0);input.read(bytes.data(),bytes.size());if(!input)return 2;
            vd::audit::Policy policy;policy.target=argv[2];if(std::string_view(argv[1])=="audit-stage-b")policy.revision=2;for(int i=4;i<argc;++i)policy.forbidden_strings.emplace_back(argv[i]);
            std::cout<<vd::audit::json(vd::audit::inspect(bytes,policy))<<'\n';return 0;
        }
        if(argc>=3 && std::string_view(argv[1])=="preflight") {
            std::vector<std::string_view> args;for(int i=2;i<argc;++i)args.emplace_back(argv[i]);
            vd::publish::preflight(vd::publish::parse_arguments(args),"linux-x64");return 99;
        }
        using namespace vd::publish;
        static_assert(!std::is_copy_assignable_v<PublishPlan> && !std::is_move_assignable_v<PublishPlan>);
        auto v1=parse(app);check(v1.succeeded() && v1.manifest->schema()==1,"schema 1 preserved");
        auto v2text=app;v2text.replace(7,1,"2");
        auto v2=parse(v2text+"[publish]\ntarget='windows-x64'\ndefault_profile='protected'\n");
        check(v2.succeeded() && v2.manifest->schema()==2,"schema 2");
        for(const auto& suffix: {"[publish]\nlto='full'\n","[publish.profiles.release]\noptimization='O2'\n","[publish]\ntarget='linux-riscv64'\n","[publish]\ndefault_profile='debug'\n","[publish]\ncrypto='secret'\n","[publish]\ntarget=4\n","[publish]\ntarget='linux-x64'\ntarget='windows-x64'\n","[publish.unknown]\n"})
            check(!parse(v2text+suffix).succeeded(),"unknown schema 2 capability rejected");
        check(!parse(app+"[publish]\ntarget='linux-x64'\n").succeeded(),"schema 1 rejects publish");
        for(const auto& args:std::vector<std::vector<std::string_view>>{{"main.void"},{"."},{"--"},{"--target","linux-riscv64"},{"--target","linux-x64","--target","linux-x64"},{"--profile","debug"},{"--profile","release","--profile","protected"},{"--allow-native"},{"--reuse-native-approval"},{"--project"},{"--entry","../main.void"},{"--entry","/main.void"},{"--entry","C:/main.void"},{"-o","x","-o","y"},{"--project","x","main.void"},{"--entry","a//b.void"}})
            rejected([&]{(void)parse_arguments(args);},"CLI rejection");
        auto defaults=plan_contract({},*v1.manifest,"linux-x64");
        auto relative=parse_arguments(std::vector<std::string_view>{"--entry","src/main.void"});
        check(relative.entry=="src/main.void","project-relative entry preserved on every host");
        check(defaults.target()=="linux-x64" && defaults.profile()=="release" && !defaults.admitted(),"contract defaults unavailable");
        check(defaults.manifest_sha256().size()==64 && defaults.project_id()=="org.test.sources","manifest identity");
        check(defaults.output()=="./dist/org.test.app/linux-x64/release","default fresh output contract");
        auto configured=plan_contract({},*v2.manifest,"linux-x64");check(configured.target()=="windows-x64" && configured.profile()=="protected","manifest defaults");
        auto request=parse_arguments(std::vector<std::string_view>{"--target","macos-arm64","--profile","performance","--entry","src/main.void","-o","new output"});
        auto overridden=plan_contract(request,*v2.manifest,"linux-x64");
        check(overridden.target()=="macos-arm64" && overridden.profile()=="performance" && overridden.entry()=="src/main.void","CLI overrides");
        auto configured_settings = parse(v2text + "[publish]\noutput='dist/custom'\noptimization='none'\nresources='embed'\nsymbols='none'\n");
        check(configured_settings.succeeded(), "schema 2 production settings");
        auto settings = plan_contract(parse_arguments(std::vector<std::string_view>{"--portable", "--optimization", "size"}),
                                      *configured_settings.manifest, "linux-x64");
        check(settings.output() == "./dist/custom" && settings.optimization() == "size" &&
              settings.resources() == "embed" && settings.symbols() == "none", "CLI settings override manifest defaults");
        check(overridden.optimization() == "speed", "performance profile defaults to speed");
        const auto lib="schema=1\n[project]\nid='org.test.sources'\nname='Fixture'\nversion='1.0.0'\nkind='library'\n[package]\nid='org.test.lib'\nname='fixture'\n";
        auto library=parse(lib);check(library.succeeded(),"valid library");rejected([&]{(void)plan_contract({},*library.manifest,"linux-x64");},"library ineligible");
        auto native=parse(app+"[native-requirements.raylib]\nid='org.void.raylib'\nversion='=1.0.0'\nabi=3\n");
        check(native.succeeded(),"valid native requirement");
        auto nativeplan=plan_contract({},*native.manifest,"linux-x64");
        check(std::find(nativeplan.required_capabilities().begin(),nativeplan.required_capabilities().end(),"publish-native-authorization")!=nativeplan.required_capabilities().end(),"separate native publish authority");
        check(!parse(app+"[native-requirements.raylib]\nid='org.void.raylib'\nversion='latest'\nabi=99\n").succeeded(),"invalid native requirement");
        vd::audit::Policy policy;policy.target="linux-x64";
        for(const auto& bytes:{std::string("\x7f" "ELF"),std::string("MZ"),std::string("\xcf\xfa\xed\xfe"),std::string("archive")}) {
            auto report=vd::audit::inspect(bytes,policy);check(!report.inspected && !report.findings.empty(),"malformed/unsupported image rejected");
        }
        policy.revision=3;check(vd::audit::inspect("",policy).findings.front().code=="unsupported-policy","policy fail closed");
        policy.revision=1;policy.target="linux-riscv64";
        check(!vd::audit::inspect("",policy).inspected,"unknown policy target fails closed");
        policy.target="linux-x64";
        for(const auto& canaries:std::vector<std::vector<std::string>>{{""}, {std::string(257,'x')}, std::vector<std::string>(65,"x"), std::vector<std::string>(17,std::string(256,'x'))}) {
            policy.forbidden_strings=canaries;
            auto report=vd::audit::inspect("",policy);
            check(!report.inspected && report.findings.front().code=="unsupported-policy","canary policy bounds");
        }
        policy.forbidden_strings.clear();
        auto excessive=vd::audit::inspect(std::string(vd::audit::byte_limit+1,'x'),policy);
        check(!excessive.inspected && excessive.findings.front().code=="limit" && excessive.sha256.empty(),"input byte limit precedes parser and hash allocation");
        std::cout<<"PASS "<<checks<<" publish contract checks\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
