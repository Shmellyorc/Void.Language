#include "void/project_creation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
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
}

int main() {
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
        std::cout<<"creation: entropy, reviewed directory, external collision, rollback ownership, staged content passed\n";
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
