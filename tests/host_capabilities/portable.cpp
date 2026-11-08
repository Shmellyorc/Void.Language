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
#include <winioctl.h>
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
#ifdef _WIN32
void server_descriptor_regressions() {
 // Exact D:\ root descriptor from CI 42446206797. Never install it on a drive.
 constexpr auto dacl=L"D:(A;OICI;FA;;;BA)(A;OICI;FA;;;SY)(A;OICIIO;GA;;;CO)(A;OICI;0x1200a9;;;BU)(A;CI;LC;;;BU)(A;CIIO;DC;;;BU)(A;;0x1200a9;;;WD)";
 win::PrivateAccess access;
 auto admitted=[&](const std::wstring& sddl,bool ancestor=true) {
  PSECURITY_DESCRIPTOR raw=nullptr;
  check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&raw,nullptr),"controlled descriptor parse");
  struct Free {void* p;~Free(){LocalFree(p);}} memory{raw};
  try {access.verify_descriptor(raw,ancestor);return true;}
  catch(const std::system_error& e) {check(e.code().value()==ERROR_ACCESS_DENIED,"descriptor rejection code");return false;}
 };
 check(!admitted(std::wstring(L"O:NS")+dacl),"hosted Network Service owner rejected before ACE admission");
 check(admitted(std::wstring(L"O:SY")+dacl),"same hosted ACEs with trusted owner admitted");
 check(!admitted(std::wstring(L"O:SY")+dacl,false),"hosted descriptor is not a private leaf");
 // Creating a new sibling alone cannot replace an existing checked object.
 // Inherit-only grants are evaluated on the opened child, not this parent.
 for(auto mask:{L"0x40",L"0x10000",L"0x40000",L"0x80000",L"0x100",L"0x10",L"GW",L"GA"}) {
  check(!admitted(std::wstring(L"O:SYD:(A;;")+mask+L";;;BU)"),"untrusted ancestor mutation rejected");
 }
 check(admitted(L"O:SYD:(A;CIIO;GA;;;BU)"),"inherit-only does not grant parent access");
 check(!admitted(L"O:SYD:(A;CI;GA;;;BU)"),"inherited child mutation is rejected when effective");
 std::cout<<"Server descriptor: original owner NS rejected; owner-only SY correction admits identical ACEs\n";
}
int stage_executable(const char* source,const char* destination,const char* expected) {
 // Explicit trusted test-caller staging, never an implicit process.start bypass.
 // The caller approves a build artifact digest. Pin that regular source against
 // writes/deletion, copy into an exclusive private capability, and rehash the
 // held destination before it can be selected for process execution.
 std::string_view digest(expected);
 check(digest.size()==64 && digest.find_first_not_of("0123456789abcdef")==digest.npos,"approved image digest syntax");
 auto path=win::absolute(source);
 win::Handle input(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
 if(input.value==INVALID_HANDLE_VALUE)win::fail();
 auto before=win::information(input.value,false);
 const auto size=(std::uint64_t(before.nFileSizeHigh)<<32)|before.nFileSizeLow;
 check(size>0 && size<=256*1024*1024,"bounded staged image extent");
 auto q=request(dir_create);q.texts={destination};auto r=directory_execute(q);
 check(!r.kind && q.acquired,"exclusive trusted staging root");auto directory=q.acquired;
 q=request(dir_file);q.objects={directory};q.texts={"child.exe"};q.numbers={1};r=directory_execute(q);
 check(!r.kind && q.acquired_file,"exclusive private staged image");auto output=q.acquired_file;
 auto hash_file=[&](bool copy) {
  auto h=request(hash_open);check(!crypto_execute(h).kind && h.acquired,"staging hash acquisition");auto hash=h.acquired;
  std::array<char,65536> buffer{};std::uint64_t offset=0;
  while(offset<size) {
   auto count=static_cast<DWORD>(std::min<std::uint64_t>(buffer.size(),size-offset));DWORD got=0;
   if(copy) {if(!ReadFile(input.value,buffer.data(),count,&got,nullptr))win::fail();}
   else {auto read=vd::filesystem::read_at(*output,offset,buffer.data(),count);check(read.error==vd::filesystem::ErrorKind::success,"staged image readback");got=static_cast<DWORD>(read.completed);}
   check(got>0 && got<=count,"staging exact progress");
   h=request(hash_update);h.objects={hash};h.bytes={std::string(buffer.data(),got)};check(!crypto_execute(h).kind,"staging digest update");
   if(copy) {
    vd::filesystem::Request write(vd::filesystem::Operation::write_bytes);write.file=output;write.bytes.assign(buffer.data(),got);
    auto written=vd::filesystem::run(write);check(written.error==vd::filesystem::ErrorKind::success && written.completed==got,"staging exact write");
   }
   offset+=got;
  }
  h=request(hash_final);h.objects={hash};auto result=crypto_execute(h);
  check(!result.kind && result.bytes==unhex(digest),"staged image matches caller-approved digest");check(!hash->close(),"staging hash cleanup");
 };
 hash_file(true);
 auto after=win::information(input.value,false);
 check(before.dwVolumeSerialNumber==after.dwVolumeSerialNumber && before.nFileIndexHigh==after.nFileIndexHigh && before.nFileIndexLow==after.nFileIndexLow &&
       before.nFileSizeHigh==after.nFileSizeHigh && before.nFileSizeLow==after.nFileSizeLow &&
       CompareFileTime(&before.ftLastWriteTime,&after.ftLastWriteTime)==0,"pinned source identity and extent unchanged");
 vd::filesystem::Request flush(vd::filesystem::Operation::flush);flush.file=output;
 check(vd::filesystem::run(flush).error==vd::filesystem::ErrorKind::success,"flush staged image");
 check(vd::filesystem::private_mode(*output,true).error==vd::filesystem::ErrorKind::success,"private staged image mode");
 hash_file(false);
 check(vd::filesystem::close(*output).error==vd::filesystem::ErrorKind::success,"staged image cleanup");
 check(!directory->close(),"staging directory cleanup");
 std::cout<<"Staged approved image "<<digest<<" ("<<size<<" bytes)\n";return 0;
}
void owner_rights_ancestors(const fs::path& work,const fs::path& executable) {
 struct Local {void* value;~Local(){if(value)LocalFree(value);}};
 win::PrivateAccess access;LPWSTR owner_text=nullptr;
 check(ConvertSidToStringSidW(access.sid(),&owner_text),"owner SID text");Local owner_memory{owner_text};
 const std::wstring owner(owner_text);
 auto text=[](const fs::path& path){auto bytes=path.u8string();return std::string(bytes.begin(),bytes.end());};
 struct Case {const wchar_t* name;const wchar_t* aces;bool admitted;};
 for(auto test:{Case{L"owner-rights",L"(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;OW)",true},
                Case{L"owner-rights-only",L"(A;OICI;FA;;;OW)",true},
                Case{L"owner-rights-everyone",L"(A;OICI;FA;;;OW)(A;;0x100;;;WD)",false},
                Case{L"owner-rights-other",L"(A;OICI;FA;;;OW)(A;;0x100;;;BU)",false}}) {
  const auto parent=work/test.name,leaf=parent/L"private";
  const auto sddl=L"O:"+owner+L"D:P"+test.aces;PSECURITY_DESCRIPTOR descriptor=nullptr;
  check(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr),"test ACL descriptor");Local memory{descriptor};
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
  check(CreateDirectoryW(parent.c_str(),&security),"exclusive ACL fixture");
  auto q=request(dir_create);q.texts={text(leaf)};auto r=directory_execute(q);
  if(test.admitted) {
   check(!r.kind && q.acquired,"OWNER_RIGHTS ancestor admitted");check(!q.acquired->close(),"OWNER_RIGHTS child cleanup");
   q=request(dir_admit);q.texts={text(parent)};r=directory_execute(q);
   check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && !q.acquired,"OWNER_RIGHTS is not a private leaf ACL");
  } else check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && !q.acquired && !fs::exists(leaf),"untrusted ancestor mutation rejected");
  q=request(spawn);q.texts={text(executable),text(parent)};q.arrays={{"--exit"},{}};r=process_execute(q);
  if(test.admitted) {
   check(!r.kind && q.acquired,"OWNER_RIGHTS working directory admitted");auto process=q.acquired;
   q=request(vd::host::wait);q.objects={process};r=process_execute(q);
   check(!r.kind && r.number==23,"OWNER_RIGHTS child status");check(!process->close(),"OWNER_RIGHTS process cleanup");
  } else check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && !q.acquired,"untrusted working directory rejected");
  // Apply the same hostile-parent condition to the executable, independently
  // of cwd. The copy is a disposable fixture, never the runner's real root.
  auto image=parent/L"child.exe";fs::copy_file(executable,image);
  q=request(spawn);q.texts={text(image),text(work)};q.arrays={{"--exit"},{}};r=process_execute(q);
  if(test.admitted) {
   check(!r.kind && q.acquired,"trusted image ancestry admitted");auto process=q.acquired;
   q=request(vd::host::wait);q.objects={process};r=process_execute(q);
   check(!r.kind && r.number==23,"trusted image exact exit");check(!process->close(),"trusted image cleanup");
  } else check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && r.detail==4 && !q.acquired,"hostile image ancestry rejected before launch");
 }
 // A junction needs no symlink privilege. Keep all changes in this fixture;
 // never change the real drive root or runner security configuration.
 auto junction=work/L"image-junction";check(fs::create_directory(junction),"junction fixture directory");
 {
  win::Handle held(CreateFileW(junction.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(held.value==INVALID_HANDLE_VALUE)win::fail();
  auto target=executable.parent_path().wstring(),substitute=L"\\??\\"+target;
  struct Mount {DWORD tag;WORD length,reserved,substitute_offset,substitute_length,print_offset,print_length;};
  static_assert(sizeof(Mount)==16);
  auto names=substitute+L'\0'+target+L'\0';
  std::vector<unsigned char> buffer(sizeof(Mount)+names.size()*sizeof(wchar_t));
  auto* mount=reinterpret_cast<Mount*>(buffer.data());mount->tag=IO_REPARSE_TAG_MOUNT_POINT;
  mount->length=static_cast<WORD>(buffer.size()-8);mount->substitute_length=static_cast<WORD>(substitute.size()*2);
  mount->print_offset=static_cast<WORD>((substitute.size()+1)*2);mount->print_length=static_cast<WORD>(target.size()*2);
  std::copy(names.begin(),names.end(),reinterpret_cast<wchar_t*>(buffer.data()+sizeof(Mount)));
  DWORD returned=0;check(DeviceIoControl(held.value,FSCTL_SET_REPARSE_POINT,buffer.data(),static_cast<DWORD>(buffer.size()),nullptr,0,&returned,nullptr),"junction fixture creation");
 }
 auto q=request(spawn);q.texts={text(junction/executable.filename()),text(work)};q.arrays={{"--exit"},{}};auto r=process_execute(q);
 check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && r.detail==4 && !q.acquired,"reparse image ancestor rejected");
 q=request(spawn);q.texts={text(executable),text(junction)};q.arrays={{"--exit"},{}};r=process_execute(q);
 check(r.kind==2 && r.code==ERROR_ACCESS_DENIED && r.detail==5 && !q.acquired,"reparse working directory rejected");
 check(fs::remove(junction),"junction fixture cleanup");
}
#endif
}
int portable_main(int argc,char** argv) {
 try {
#ifdef _WIN32
  if(argc==5 && std::string_view(argv[1])=="--stage-executable")return stage_executable(argv[2],argv[3],argv[4]);
  if(argc==2 && std::string_view(argv[1])=="--server-descriptor") {server_descriptor_regressions();return 0;}
  if(argc==2 && std::string_view(argv[1])=="--verify-stdio") {
   for(DWORD selected:{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE}) {
    auto handle=GetStdHandle(selected);
    if(!handle || handle==INVALID_HANDLE_VALUE || GetFileType(handle)==FILE_TYPE_UNKNOWN)return 98;
   }
   std::cout<<"child inherited stdout\n";std::cerr<<"child inherited stderr\n";return 23;
  }
#endif
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
  q=request(dir_open);q.objects={directory};q.texts={"stage"};r=directory_execute(q);check(!r.kind && q.acquired,"independent source alias");auto source_alias=q.acquired;
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
  q=request(dir_admit);q.texts={root};r=directory_execute(q);check(!r.kind && q.acquired,"independent publication target alias");auto target_alias=q.acquired;
  q=request(dir_publish);q.objects={stage,directory};q.texts={"release"};r=directory_execute(q);check(r.flag,"exclusive publication");
  // Windows directory FlushFileBuffers can fail. Mutation must remain visible,
  // with the actual durability error, rather than becoming unpublished/success.
  check(!r.kind || (r.flag && r.code!=0),"uncertain publication preserves native code");
  q=request(dir_open);q.objects={directory};q.texts={"release"};r=directory_execute(q);check(!r.kind,"published directory available");check(!q.acquired->close(),"close published alias");
  auto collision=child(directory,"collision");
  q=request(dir_publish);q.objects={collision,directory};q.texts={"release"};r=directory_execute(q);check(r.kind && !r.flag,"publication collision has no mutation");
  check(fs::exists(work/"capability-private/collision") && fs::exists(work/"capability-private/release/data"),"collision preserves both directory identities");
  auto destination=child(directory,"destination");
  q=request(dir_publish);q.objects={stage,destination};q.texts={"moved"};r=directory_execute(q);
  check(r.flag && !fs::exists(work/"capability-private/release") && fs::exists(work/"capability-private/destination/moved/data"),"held directory cross-parent publication");
  auto impostor=child(directory,"stage");
  q=request(dir_publish);q.objects={source_alias,directory};q.texts={"stale-move"};r=directory_execute(q);
  check(r.kind && !r.flag && !fs::exists(work/"capability-private/stale-move") && fs::exists(work/"capability-private/stage") && fs::exists(work/"capability-private/destination/moved/data"),"stale source alias rejects replaced old name without mutation");
  q=request(dir_open);q.objects={destination};q.texts={"moved"};r=directory_execute(q);check(!r.kind && q.acquired,"fresh source alias");auto fresh_alias=q.acquired;
  q=request(dir_publish);q.objects={fresh_alias,directory};q.texts={"fresh-move"};r=directory_execute(q);
  check(r.flag && fs::exists(work/"capability-private/fresh-move/data") && !fs::exists(work/"capability-private/destination/moved"),"fresh source alias publishes with current parent");
  auto create_file=[&](const char* name,const char* content) {
   auto operation=request(dir_file);operation.objects={directory};operation.texts={name};operation.numbers={1};auto result=directory_execute(operation);
   check(!result.kind && operation.acquired_file,"publication file creation");auto value=operation.acquired_file;
   vd::filesystem::Request output(vd::filesystem::Operation::write_bytes);output.file=value;output.bytes=content;
   check(vd::filesystem::run(output).completed==static_cast<std::int64_t>(output.bytes.size()),"publication file bytes");return value;
  };
  auto original=create_file("original","original");
  q=request(dir_replace);q.objects={directory,destination};q.texts={"original","selected"};q.numbers={0};q.file=std::make_shared<Borrow>(original);r=directory_execute(q);
  check(r.flag && !fs::exists(work/"capability-private/original") && fs::exists(work/"capability-private/destination/selected"),"exclusive file publication");
  check(vd::filesystem::close(*original).error==vd::filesystem::ErrorKind::success,"close original publication file");
  auto replacement=create_file("replacement","replacement");
  q=request(dir_replace);q.objects={directory,destination};q.texts={"replacement","selected"};q.numbers={0};q.file=std::make_shared<Borrow>(replacement);r=directory_execute(q);
  check(r.kind && !r.flag && fs::exists(work/"capability-private/replacement"),"exclusive file collision preserves source");
  q.numbers={1};r=directory_execute(q);check(r.flag && !fs::exists(work/"capability-private/replacement"),"atomic file replacement");
  char replaced[11]{};auto replaced_read=vd::filesystem::read_at(*replacement,0,replaced,sizeof(replaced));
  check(replaced_read.error==vd::filesystem::ErrorKind::success && replaced_read.completed==11 && std::string_view(replaced,11)=="replacement","held replacement identity and bytes");
  check(vd::filesystem::close(*replacement).error==vd::filesystem::ErrorKind::success,"close replacement file");
  q=request(dir_replace);q.objects={destination,directory};q.texts={"selected","closed-source"};q.numbers={0};q.file=std::make_shared<Borrow>(replacement);r=directory_execute(q);
  check(r.kind && !r.flag && fs::exists(work/"capability-private/destination/selected") && !fs::exists(work/"capability-private/closed-source"),"closed file publication fails without mutation");
#ifdef _WIN32
  check(r.code==ERROR_INVALID_HANDLE,"failed identity query preserves native error");
#endif
  check(!fresh_alias->close() && !impostor->close() && !source_alias->close() && !target_alias->close() && !destination->close() && !collision->close() && !stage->close() && !directory->close(),"directory cleanup");
  auto executable8=executable.u8string(),cwd8=work.u8string();
  q=request(spawn);q.texts={std::string(executable8.begin(),executable8.end()),std::string(cwd8.begin(),cwd8.end())};q.arrays={{"--exit"},{"VOID_CHILD=owned"}};
  r=process_execute(q);
  if(r.kind) {
   std::cerr<<"explicit child launch: kind="<<r.kind<<" code="<<r.code<<" stage="<<r.detail<<"\n";
#ifdef _WIN32
   // Read-only failure evidence; never launch a child outside the checked API.
   win::PrivateAccess access;
   auto inspect=[&](const char* stage,const std::wstring& path,DWORD rights,bool ancestry) {
    win::Handle held(CreateFileW(path.c_str(),rights,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
      nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(held.value==INVALID_HANDLE_VALUE){std::cerr<<stage<<" open: native="<<GetLastError()<<"\n";return;}
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(held.value,&info)){std::cerr<<stage<<" metadata: native="<<GetLastError()<<"\n";return;}
    std::cerr<<stage<<" metadata: attributes="<<info.dwFileAttributes<<"\n";
    if(ancestry) {
     try {access.verify(held.value,true);std::cerr<<stage<<" ACL: validated\n";}
     catch(const std::system_error& e) {
      std::cerr<<stage<<" ACL: native="<<e.code().value()<<"\n";
      PSECURITY_DESCRIPTOR raw=nullptr;
      auto status=GetSecurityInfo(held.value,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,
        nullptr,nullptr,nullptr,nullptr,&raw);
      std::unique_ptr<void,decltype(&LocalFree)> descriptor(raw,&LocalFree);
      if(status==ERROR_SUCCESS) {
       LPWSTR text=nullptr;
       if(ConvertSecurityDescriptorToStringSecurityDescriptorW(raw,SDDL_REVISION_1,
          OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION,&text,nullptr)) {
        std::unique_ptr<void,decltype(&LocalFree)> owner(text,&LocalFree);
        auto count=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);
        if(count>0) {
         std::string utf8(count,0);
         if(WideCharToMultiByte(CP_UTF8,0,text,-1,utf8.data(),count,nullptr,nullptr))
          std::cerr<<stage<<" SDDL: "<<utf8.c_str()<<"\n";
        }
       } else std::cerr<<stage<<" SDDL conversion: native="<<GetLastError()<<"\n";
      } else std::cerr<<stage<<" security query: native="<<status<<"\n";
     }
    }
   };
   auto ancestors=[&](const char* stage,const std::wstring& path) {
    inspect(stage,path.substr(0,7),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL,true);
    for(std::size_t start=7;start<path.size();) {
     auto end=path.find(L'\\',start);
     std::cerr<<stage<<" prefix length="<<(end==path.npos?path.size():end)<<"\n";
     inspect(stage,path.substr(0,end),FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES|READ_CONTROL,true);
     if(end==path.npos)break;start=end+1;
    }
   };
   auto image=win::absolute(q.texts[0]),cwd=win::absolute(q.texts[1]);
   ancestors("image ancestors",image.substr(0,std::max<std::size_t>(7,image.find_last_of(L'\\'))));
   ancestors("working directory ancestors",cwd);
   inspect("working directory list-only",cwd,FILE_LIST_DIRECTORY,false);
   inspect("working directory attributes",cwd,FILE_LIST_DIRECTORY|FILE_READ_ATTRIBUTES,false);
   for(DWORD selected:{STD_INPUT_HANDLE,STD_OUTPUT_HANDLE,STD_ERROR_HANDLE}) {
    auto handle=GetStdHandle(selected);DWORD flags=0;SetLastError(0);
    auto ok=GetHandleInformation(handle,&flags);auto native=ok?0:GetLastError();
    std::cerr<<"standard handle "<<selected<<": query="<<ok<<" native="<<native<<" flags="<<flags<<"\n";
    if(handle && handle!=INVALID_HANDLE_VALUE) {
     HANDLE duplicate=INVALID_HANDLE_VALUE;
     if(!DuplicateHandle(GetCurrentProcess(),handle,GetCurrentProcess(),&duplicate,0,TRUE,DUPLICATE_SAME_ACCESS))
      std::cerr<<"standard handle duplication: native="<<GetLastError()<<"\n";
     else {win::Handle owner(duplicate);std::cerr<<"standard handle duplication: validated\n";}
    }
   }
#endif
  }
  check(!r.kind && q.acquired,"explicit child launch");auto process=q.acquired;
  q=request(vd::host::wait);q.objects={process};r=process_execute(q);check(!r.kind && r.number==23 && !r.flag,"child exact status");r=process_execute(q);check(!r.kind && r.number==23,"repeat wait");check(!process->close(),"child cleanup");
#ifdef _WIN32
  q=request(spawn);q.texts={std::string(executable8.begin(),executable8.end()),std::string(cwd8.begin(),cwd8.end())};q.arrays={{"--verify-stdio"},{}};r=process_execute(q);
  check(!r.kind && q.acquired,"explicit standard handle launch");process=q.acquired;
  q=request(vd::host::wait);q.objects={process};r=process_execute(q);check(!r.kind && r.number==23,"inherited standard handles available");check(!process->close(),"standard handle child cleanup");
#endif
  q=request(spawn);q.texts={"relative",std::string(cwd8.begin(),cwd8.end())};q.arrays={{},{}};r=process_execute(q);check(r.kind!=0,"reject relative executable");
  auto arguments=vd::checked_application_arguments({"","two words","\"quote\"","tail\\","雪😀"});check(!arguments.error && arguments.values.size()==5,"owned Unicode arguments");
  check(vd::checked_application_arguments({std::string("a\0b",3)}).error!=0,"reject embedded NUL");
#ifdef _WIN32
  server_descriptor_regressions();
  wchar_t image[]=L"image",bad[]={static_cast<wchar_t>(0xd800),0};wchar_t* input[]={image,bad};
  check(vd::capture_application_arguments(2,input,true).error==4,"reject invalid UTF16");
  check(!win::component(L"CON.txt") && !win::component(L"file:stream") && !win::component(L"data. ") && !win::component(L"FOO~1"),"reject DOS aliases and streams");
  owner_rights_ancestors(work,executable);
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
