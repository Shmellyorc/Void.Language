// Native provider tests shared by Linux, Windows and macOS. Source/JIT/AOT
// qualification remains separate; passing here does not certify another host.
#include "../../src/runtime/host_internal.hpp"
#include "void/application.hpp"
#include "void/utf8.hpp"
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include "../../src/runtime/host_windows.hpp"
#endif
namespace fs=std::filesystem;
using namespace vd::host;
namespace {
unsigned checks=0;
void check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);++checks;}
Request request(unsigned code){Request q{};q.operation=code;return q;}
std::string unhex(std::string_view text) {
 std::string result;
 auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};
 for(std::size_t n=0;n<text.size();n+=2)result.push_back(static_cast<char>((digit(text[n])<<4)|digit(text[n+1])));
 return result;
}
struct Borrow final : vd::filesystem::FileBorrow {
 std::shared_ptr<vd::filesystem::File> value;
 explicit Borrow(std::shared_ptr<vd::filesystem::File> file):value(std::move(file)){}
 std::shared_ptr<vd::filesystem::File> file() const noexcept override {return value;}
};
std::shared_ptr<Object> child(const std::shared_ptr<Object>& parent,const char* name) {
 auto q=request(dir_child);q.objects={parent};q.texts={name};auto result=directory_execute(q);
 check(!result.kind && q.acquired,"private child");return q.acquired;
}
}
int portable_main(int argc,char** argv) {
 try {
  if(argc==2 && std::string_view(argv[1])=="--exit")return 23;
  if(argc==7 && std::string_view(argv[1])=="--verify-args") {
    bool valid=std::string_view(argv[2]).empty() && std::string_view(argv[3])=="two words" && std::string_view(argv[4])=="\"quote\"" && std::string_view(argv[5])=="tail\\" && std::string_view(argv[6])=="雪😀";
#ifdef _WIN32
    auto value=::_wgetenv(L"VOID_CHILD");valid=valid && value && std::wstring_view(value)==L"雪=value";
#else
    auto value=std::getenv("VOID_CHILD");valid=valid && value && std::string_view(value)=="雪=value";
#endif
    return valid?23:99;
  }
  if(argc!=3)return 2;
  auto work=fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(argv[1])));auto executable=fs::path(std::u8string_view(reinterpret_cast<const char8_t*>(argv[2])));
  auto path=(work/"capability-private").u8string();std::string root(path.begin(),path.end());
  auto hash_request=request(hash_open);auto hash_result=crypto_execute(hash_request);
  check(!hash_result.kind && hash_request.acquired,"SHA provider acquisition");auto hash=hash_request.acquired;
  hash_request=request(hash_update);hash_request.objects={hash};hash_request.bytes={"abc"};check(!crypto_execute(hash_request).kind,"SHA provider update");
  hash_request=request(hash_final);hash_request.objects={hash};hash_result=crypto_execute(hash_request);
  check(!hash_result.kind && hash_result.bytes==unhex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"SHA known answer");
  check(crypto_execute(hash_request).kind!=0 && !hash->close(),"SHA final/cleanup state");
  auto verify_request=request(verify);verify_request.bytes={unhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"),unhex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"),{}};
  auto verified=crypto_execute(verify_request);check(!verified.kind && verified.flag,"Ed25519 known answer");
  verify_request.bytes[2]="altered";verified=crypto_execute(verify_request);check(!verified.kind && !verified.flag,"Ed25519 altered message");
  verify_request.bytes[0].clear();check(crypto_execute(verify_request).kind!=0,"Ed25519 malformed key");
  for(bool hostile:{false,true}) {
   auto zip_path=(work/(hostile?"hostile.zip":"package.zip")).u8string();vd::filesystem::Result opened;
   auto input=vd::filesystem::open(std::string(zip_path.begin(),zip_path.end()),vd::filesystem::OpenMode::read,opened);
   check(input && opened.error==vd::filesystem::ErrorKind::success,"open ZIP fixture");
   auto zip_request=request(zip_open);zip_request.file=std::make_shared<Borrow>(input);zip_request.numbers={1048576,100,1048576};auto zip_result=zip_execute(zip_request);
   if(hostile)check(zip_result.kind!=0 && !zip_request.acquired,"hostile ZIP rejected");
   else {
    check(!zip_result.kind && zip_request.acquired,"ZIP acquisition");auto zip=zip_request.acquired;
    zip_request=request(zip_next);zip_request.objects={zip};zip_result=zip_execute(zip_request);check(!zip_result.kind && zip_result.text=="data" && zip_result.number==3,"ZIP metadata");
    zip_request=request(zip_read);zip_request.objects={zip};zip_request.numbers={2};std::string content;
    for(;;){zip_result=zip_execute(zip_request);check(!zip_result.kind,"ZIP streaming");content+=zip_result.bytes;if(zip_result.flag)break;}
    check(content=="abc","ZIP streaming bytes");check(!zip->close(),"ZIP close");
   }
   check(vd::filesystem::close(*input).error==vd::filesystem::ErrorKind::success,"ZIP input cleanup");
  }
  auto q=request(dir_create);q.texts={root};auto r=directory_execute(q);check(!r.kind && q.acquired,"private root");auto directory=q.acquired;
  q=request(dir_admit);q.texts={root};r=directory_execute(q);check(!r.kind,"admit root");check(!q.acquired->close(),"close admitted alias");
  auto stage=child(directory,"stage");
  q=request(dir_child);q.objects={directory};q.texts={"../escape"};r=directory_execute(q);check(r.kind!=0,"reject traversal");
  q=request(dir_file);q.objects={stage};q.texts={"data"};q.numbers={1};r=directory_execute(q);check(!r.kind && q.acquired_file,"private file");auto file=q.acquired_file;
  vd::filesystem::Request write(vd::filesystem::Operation::write_bytes);write.file=file;write.bytes="payload";
  check(vd::filesystem::run(write).completed==7,"write private file");
  check(vd::filesystem::private_mode(*file,true).error==vd::filesystem::ErrorKind::success,"private executable classification");
  char bytes[7]{};auto read=vd::filesystem::read_at(*file,0,bytes,7);check(read.error==vd::filesystem::ErrorKind::success && std::string_view(bytes,7)=="payload","positioned read");
  vd::filesystem::Request position(vd::filesystem::Operation::position);position.file=file;
  check(vd::filesystem::run(position).number==7,"positioned read preserves cursor");
  check(vd::filesystem::regular_length(*file).number==7,"held regular extent");
  check(vd::filesystem::close(*file).error==vd::filesystem::ErrorKind::success,"close private file");
  q=request(dir_lock);q.objects={directory};q.texts={"writer.lock"};r=directory_execute(q);check(!r.kind && q.acquired,"first lock");auto lock=q.acquired;
  q=request(dir_lock);q.objects={directory};q.texts={"writer.lock"};r=directory_execute(q);check(r.kind!=0,"second lock denied");check(!lock->close(),"release lock");
  q=request(dir_lock);q.objects={directory};q.texts={"writer.lock"};r=directory_execute(q);check(!r.kind,"released lock reacquired");check(!q.acquired->close(),"release second lock");
  q=request(dir_publish);q.objects={stage,directory};q.texts={"release"};r=directory_execute(q);check(r.flag,"exclusive publication");
  // Windows directory FlushFileBuffers can fail. Mutation must remain visible,
  // with the actual durability error, rather than becoming unpublished/success.
  check(!r.kind || (r.flag && r.code!=0),"uncertain publication preserves native code");
  q=request(dir_open);q.objects={directory};q.texts={"release"};r=directory_execute(q);check(!r.kind,"published directory available");check(!q.acquired->close(),"close published alias");
  auto collision=child(directory,"collision");
  q=request(dir_publish);q.objects={collision,directory};q.texts={"release"};r=directory_execute(q);check(r.kind && !r.flag,"publication collision has no mutation");
  check(!collision->close() && !stage->close() && !directory->close(),"directory cleanup");
  auto executable8=executable.u8string(),cwd8=work.u8string();
  q=request(spawn);q.texts={std::string(executable8.begin(),executable8.end()),std::string(cwd8.begin(),cwd8.end())};q.arrays={{"--exit"},{"VOID_CHILD=owned"}};
  r=process_execute(q);check(!r.kind && q.acquired,"explicit child launch");auto process=q.acquired;
  q=request(wait);q.objects={process};r=process_execute(q);check(!r.kind && r.number==23 && !r.flag,"child exact status");r=process_execute(q);check(!r.kind && r.number==23,"repeat wait");check(!process->close(),"child cleanup");
  q=request(spawn);q.texts={"relative",std::string(cwd8.begin(),cwd8.end())};q.arrays={{},{}};r=process_execute(q);check(r.kind!=0,"reject relative executable");
  auto arguments=vd::checked_application_arguments({"","two words","\"quote\"","tail\\","雪😀"});check(!arguments.error && arguments.values.size()==5,"owned Unicode arguments");
  check(vd::checked_application_arguments({std::string("a\0b",3)}).error!=0,"reject embedded NUL");
#ifdef _WIN32
  wchar_t image[]=L"image",bad[]={static_cast<wchar_t>(0xd800),0};wchar_t* input[]={image,bad};
  check(vd::capture_application_arguments(2,input,true).error==4,"reject invalid UTF16");
  check(!win::component(L"CON.txt") && !win::component(L"file:stream") && !win::component(L"data. ") && !win::component(L"FOO~1"),"reject DOS aliases and streams");
#endif
  std::cout<<"Portable host providers: "<<checks<<" checks passed\n";return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    std::vector<std::string> values;std::vector<char*> pointers;values.reserve(argc);pointers.reserve(argc);
    for(int n=0;n<argc;++n) {
        int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[n],-1,nullptr,0,nullptr,nullptr);
        if(!count)return 3;std::string value(count,0);
        if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[n],-1,value.data(),count,nullptr,nullptr))return 3;
        value.pop_back();values.push_back(std::move(value));
    }
    for(auto& value:values)pointers.push_back(value.data());return portable_main(argc,pointers.data());
}
#else
int main(int argc,char** argv) {return portable_main(argc,argv);}
#endif
