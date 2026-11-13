#include "void/project_creation.hpp"
#include "../../src/frontend/project_files.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
using namespace vd::project;

namespace {
void check(bool condition,const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
template<class Error = std::exception, class Operation>
void fails(Operation operation) {
    try {operation();}
    catch (const Error&) {return;}
    throw std::runtime_error("Expected failure");
}
struct Fixture {
    fs::path root;
    Fixture() {
        auto base=fs::canonical(fs::temp_directory_path());
        for (unsigned i=0;i<1000;++i) {
            auto path=base/("void-creation-test-"+std::to_string(i));
            if (fs::create_directory(path)) {root=path;return;}
        }
        throw std::runtime_error("Cannot create fixture");
    }
    ~Fixture() {std::error_code error;fs::remove_all(root,error);}
};
std::error_code fail_entropy(void*,std::size_t) {return std::make_error_code(std::errc::io_error);}
bool modified_starter=false;
void alter_starter(const fs::path& stage) {
    const auto path=stage/"src/app.void";
    constexpr std::string_view alteration="// changed";
#ifdef _WIN32
    // CRT streams do not share DELETE access with the held staging handle.
    auto file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written=0;
    modified_starter=WriteFile(file,alteration.data(),static_cast<DWORD>(alteration.size()),&written,nullptr) &&
        written==alteration.size();
    CloseHandle(file);
#else
    std::ofstream file(path,std::ios::app);
    file<<alteration;
    file.close();
    modified_starter=bool(file);
#endif
}
std::string read(const fs::path& file) {
    std::ifstream input(file,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),{}};
}

#if defined(_WIN32) || defined(__linux__)
void application_publication(const fs::path& root) {
    const auto parent=inspect_creation_root(root);
    const auto published=root/"distribution";
    auto prepare=[&](const fs::path& stage,const ProjectDirectoryReader& reader) {
#ifdef _WIN32
        fails([&]{reader.inspect(stage.parent_path());});
#endif
        std::ofstream(stage/"app")<<"application bytes";
        publish_project_directory(reader.inspect(stage),"native",{{"payload","native bytes"}},
            [&](const fs::path& nested,const ProjectDirectoryReader& nested_reader) {
                check(nested_reader.read(nested/"payload",32)=="native bytes","Nested staged payload missing");
#ifdef _WIN32
                for (const auto& pinned : {stage,nested}) {
                    const auto moved=pinned.parent_path()/"unexpected rename";
                    const auto renamed=MoveFileExW(pinned.c_str(),moved.c_str(),0);
                    const auto error=GetLastError();
                    check(!renamed && error==ERROR_SHARING_VIOLATION,"Active stage did not exclude a competing rename");
                }
#endif
            },{},reader);
        check(reader.read(stage/"native/payload",32)=="native bytes","Published nested payload missing from stage");
    };
    fails([&]{publish_application_directory(parent,"distribution",[&](const auto& stage,const auto& reader) {
        prepare(stage,reader);
        throw std::runtime_error("Injected package verification failure");
    },{});});
    check(fs::is_empty(root),"Package verification rollback left output");
    fails<CreationCollision>([&]{publish_application_directory(parent,"distribution",[&](const auto& stage,const auto& reader) {
        prepare(stage,reader);
        fs::create_directory(published);
        std::ofstream(published/"external")<<"existing winner";
    },{});});
    check(read(published/"external")=="existing winner","Late collision altered external output");
    check(std::distance(fs::directory_iterator(root),fs::directory_iterator{})==1,"Late collision left package staging");
    fs::remove_all(published);
    publish_application_directory(parent,"distribution",prepare,{});
    check(read(published/"app")=="application bytes" && read(published/"native/payload")=="native bytes",
        "Exclusive publication lost verified content");
#ifdef _WIN32
    const auto locked=CreateFileW((published/"app").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(locked!=INVALID_HANDLE_VALUE,"Cannot lock published executable fixture");
    const auto unlock=[](void* handle) {CloseHandle(handle);};
    std::unique_ptr<void,decltype(unlock)> lock(locked,unlock);
    fails<CreationCollision>([&]{publish_application_directory(parent,"distribution",
        [&](const auto&,const auto&) {throw std::logic_error("Prepared replacement");},
        [&](const auto&) {throw std::logic_error("Admitted replacement");});});
    lock.reset();
    check(read(published/"app")=="application bytes","Windows replacement refusal altered output");
#endif
    fs::rename(published,root/"renamed after release");
}
#endif
}

int main(int argc,char** argv) {
    if(argc==4 && std::string_view(argv[1])=="--package-interrupt") {
        const auto root=inspect_creation_root(fs::path(argv[2]));
        const auto before=read(root.path/"void.project.toml");
        static std::string phase;
        phase=argv[3];
        std::map<std::string,ProjectFiles> replacements;
        if(phase.starts_with("replace"))replacements.emplace("interrupted",ProjectFiles{{"owned.txt","previous source"}});
        publish_package_edit(root,before,read_dependency_lock(root.path),{{"interrupted",{{"owned.txt","staged source"}}}},
            [&]{return std::pair{before+"\n# completed edit\n",std::optional<std::string>("interrupted lock")};},
            [](const fs::path& checkpoint){
                bool displaced=checkpoint.parent_path().filename()=="displaced";
                bool before=checkpoint.filename()=="package-edit";
                if((phase=="replace-displaced" && displaced) ||
                   ((phase=="before" || phase=="replace-before") && before) ||
                   ((phase=="after" || phase=="replace-after") && !before && !displaced))std::_Exit(86);
            },replacements);
        return 3;
    }

    try {
        Fixture fixture;
        auto root=fixture.root/"existing";
        fs::create_directory(root);
        fails<std::system_error>([&]{plan_creation(root,{},fail_entropy);});
        check(fs::is_empty(root),"Entropy failure wrote files");
        auto original=plan_creation(root,{});
        fs::rename(root,fixture.root/"original");
        fs::create_directory(root);
        fails([&]{publish_creation(original);});
        check(fs::is_empty(root),"Replaced root was modified");

        auto plan=plan_creation(root,{});
        fails<CreationCollision>([&]{publish_creation(plan,[](const fs::path& stage){
            std::ofstream(stage.parent_path()/"void.project.toml")<<"external manifest";
        });});
        check(read(root/"void.project.toml")=="external manifest","Collision altered external file");
        check(std::distance(fs::directory_iterator(root),fs::directory_iterator{})==1,"Collision left owned staging files");

        auto fresh=CreationOptions{};fresh.new_directory=true;
        auto target=fixture.root/"new-project";
        auto create=plan_creation(target,fresh);
        fails([&]{publish_creation(create,[](const fs::path& stage) {
            std::ofstream(stage/"foreign.txt")<<"foreign data";
        });});
        check(!fs::exists(target),"Unreviewed content was published");
        unsigned retained=0;
        for(const auto& item:fs::directory_iterator(fixture.root)) {
            if(!item.path().filename().string().starts_with(".void-create-"))continue;
            ++retained;
            check(read(item.path()/"foreign.txt")=="foreign data","Rollback removed a foreign file");
            check(std::distance(fs::directory_iterator(item),fs::directory_iterator{})==1,"Owned files remained after rollback");
        }
        check(retained==1,"Rollback discarded uncertain directory");
        try {
            publish_creation(create,alter_starter);
            throw std::logic_error(modified_starter?"Changed staged bytes were published":"Stage tampering was never injected");
        } catch(const std::runtime_error& error) {
            check(modified_starter,"Stage tampering was never injected");
            check(std::string_view(error.what()).find("staging contents changed")!=std::string_view::npos,
                "Unexpected staging failure after tampering");
        }
        check(!fs::exists(target),"Changed staged bytes were published");
#if defined(_WIN32) || defined(__linux__)
        const auto package_parent=fixture.root/"package parent";
        fs::create_directory(package_parent);
        application_publication(package_parent);
#endif
        std::cout<<"creation: entropy, reviewed directory, external collision, rollback ownership, staged content passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
