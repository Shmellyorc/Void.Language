#include "void/runtime.hpp"
#include "void/runtime_abi.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace vd;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class Work> void reject(Work work){bool rejected=false;try{work();}catch(const std::exception&){rejected=true;}require(rejected,"invalid checked operation accepted");}
void geometry(){
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);
    void* object=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&object,sizeof(object),&zero,1};Runtime::Frame roots(r,&slot,1);auto token=r.root_word(&object);
    for(unsigned kind=0;kind<=10;++kind){
        const auto encoding=static_cast<ScalarEncoding>(kind);
        for(std::uint64_t extent:{1u,3u,17u}){
            auto layout=r.fixed_scalar_layout(encoding,extent);r.fixed_default_into(&object,layout,{16384,256,4096});
            require(r.fixed_scalar_layout(token,encoding,extent)==layout,"canonical source geometry lost");
            require(layout->element_offset(extent-1)==(extent-1)*layout->stride(),"last offset mismatch");
            reject([&]{layout->element_offset(extent);});reject([&]{layout->element_offset(~std::uint64_t{});});
            reject([&]{r.fixed_scalar_layout(token,encoding,extent+1);});
            auto padded=r.buffer_layout(layout->schema(),extent,layout->alignment(),layout->stride()*2);
            r.fixed_default_into(&object,padded,{16384,256,4096});reject([&]{r.fixed_scalar_layout(token,encoding,extent);});
            r.fixed_default_into(&object,layout,{16384,256,4096});
            require(r.collect()==Runtime::Collection::completed && r.fixed_scalar(token,layout,extent-1)==0,"geometry GC/recovery failed");
        }
    }
    object=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"geometry retained payload");
}
// Exercise the source-selected ABI against the independent dynamic ABI, including
// raw floating-point payloads. A source-constant encoding grants no root authority.
void source_reads(){
    using Read=std::uint64_t(*)(void**,std::uint64_t,unsigned,RuntimeFailure*) noexcept;
    const Read reads[]={vd_list_read_index_i8,vd_list_read_index_i16,vd_list_read_index_i32,vd_list_read_index_i64,
        vd_list_read_index_u8,vd_list_read_index_u16,vd_list_read_index_u32,vd_list_read_index_u64,
        vd_list_read_index_f32,vd_list_read_index_f64,vd_list_read_index_boolean};
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);
    void* object=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&object,sizeof(object),&zero,1};Runtime::Frame roots(r,&slot,1);
    for(unsigned kind=0;kind<=10;++kind){
        const auto e=static_cast<ScalarEncoding>(kind);r.list_into(&object,e);auto token=r.root_word(&object);
        const auto width=(kind==0 || kind==4 || kind==10)?1u:(kind==1 || kind==5)?2u:(kind==2 || kind==6 || kind==8)?4u:8u;
        const auto bits=kind==10?1ull:width==8?0xfff0123456789abcull:(1ull<<(8*width))-1;
        r.list_push(token,e,bits);r.list_push(token,e,0);r.list_push(token,e,bits);
        for(auto index:{0ull,2ull,~0ull,~1ull,~2ull,3ull,~3ull,1ull<<63})for(unsigned sign:{0u,1u}){
            RuntimeFailure old{},now{};auto expected=vd_list_read_index(&object,kind,index,sign,&old);
            auto actual=reads[kind](&object,index,sign,&now);
            require(actual==expected && bool(now.message)==bool(old.message),"source read result/failure differs");
            if(old.message)require(std::strcmp(old.message,now.message)==0,"source read error ordering differs");
        }
        RuntimeFailure invalid{};reads[kind](&object,0,2,&invalid);require(invalid.message,"invalid index signedness accepted");
        RuntimeFailure pending{"already failed"};require(reads[kind](&object,0,0,&pending)==0 && std::string_view(pending.message)=="already failed","pending failure changed");
        void* unregistered=object;RuntimeFailure authority{};reads[kind](&unregistered,0,0,&authority);require(authority.message,"source constant bypassed root authority");
        RuntimeFailure mismatch{};reads[(kind+1)%11](&object,0,0,&mismatch);require(mismatch.message,"source constant bypassed schema identity");
        require(r.collect()==Runtime::Collection::completed,"source read collection failed");
        RuntimeFailure recovered{};require(reads[kind](&object,0,0,&recovered)==bits && !recovered.message,"source read GC/recovery failed");
        // Publishing an unrelated catalog generation cannot invalidate a read,
        // and does not authorize a mismatched source encoding.
        NominalDescriptor added{"g37.catalog."+std::to_string(kind),"","v1",{8,{}}};
        {Runtime::Native n(r);r.admit_types({&added,1});}
        RuntimeFailure fresh{};require(reads[kind](&object,~0ull,1,&fresh)==bits && !fresh.message,"catalog publication changed read");
    }
    object=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"source read retained payload");
}
void abi_failure_order(){
    auto message=[](const RuntimeFailure& f,const char* text){require(f.message && std::strcmp(f.message,text)==0,"checked ABI failure translation changed");};
    RuntimeFailure unattached{};vd_list_count(nullptr,2,&unattached);message(unattached,"Invalid fixed root or runtime state");
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);void* object=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&object,sizeof(object),&zero,1};Runtime::Frame roots(r,&slot,1);
    auto layout=r.fixed_scalar_layout(ScalarEncoding::i32,3);r.fixed_default_into(&object,layout,{16384,256,4096});
    RuntimeFailure fixed_index{};vd_array_read(&object,3,2,3,&fixed_index);message(fixed_index,"Fixed index outside extent");
    RuntimeFailure geometry{};vd_array_default(&object,2,~std::uint64_t{},0,&geometry);message(geometry,"Fixed layout is not representable");
    RuntimeFailure encoding{};vd_array_default(&object,255,3,0,&encoding);message(encoding,"Invalid fixed value contract");
    {
        Runtime::BufferReadLease lease(r,r.root_word(&object),layout);
        RuntimeFailure busy{};vd_array_write(&object,0,7,2,3,&busy);message(busy,"Fixed buffer is busy");
    }
    r.list_into(&object,ScalarEncoding::i32,3);
    RuntimeFailure list_index{};vd_list_read_index_i32(&object,3,0,&list_index);message(list_index,"List index outside extent");
    RuntimeFailure version{};vd_list_write(&object,2,0,7,1,1,&version);message(version,"List structurally modified during selected operation");
    RuntimeFailure pending{"preserve prior error"};auto* before=object;vd_array_default(&object,255,~std::uint64_t{},0,&pending);message(pending,"preserve prior error");require(before==object,"pending failure changed output");
    RuntimeFailure recovered{};require(vd_list_read_index_i32(&object,0,0,&recovered)==0 && !recovered.message,"translated failures prevented recovery");
    object=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"failure translation retained payload");
}
void policies(){
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);void* dict=nullptr;constexpr std::size_t zero=0;Runtime::Slot slot{&dict,sizeof(dict),&zero,1};Runtime::Frame roots(r,&slot,1);
    struct Entry{std::uint64_t key,value,hash;};
    NativeArrayField k{"",0,0,7,0};NativeArrayElement key{"void.fixed.scalar.v1/7",8,8,1,&k};
    NativeArrayField fields[]={{"",0,0,7,0},{"",8,0,7,0},{"",16,0,7,0}};
    NativeArrayElement entry{"g36.entry",sizeof(Entry),alignof(Entry),3,fields};NativeDictionary d{&entry,&key,8,16};
    r.dictionary_new(&dict,d);auto token=r.root_word(&dict);Entry value{7,42,0};r.dictionary_write(token,d,&value,0);
    std::uint64_t selected=7;Entry out{};r.dictionary_read(&out,token,d,&selected);require(out.value==42,"dictionary warm oracle");
    // Borrowed descriptors remain mutable, even at the same address.
    fields[1].offset=0;reject([&]{r.dictionary_count(token,d);});fields[1].offset=8;
    require(r.dictionary_count(token,d)==1,"descriptor restoration failed");
    NominalDescriptor unrelated{"g36.unrelated","","v1",{8,{}}};
    {Runtime::Native n(r);r.admit_types({&unrelated,1});}
    require(r.dictionary_count(token,d)==1,"unrelated catalog publication changed dictionary");
    NominalDescriptor forbidden{"g36.policy","","v1",{8,{}},"void.fixed.scalar.v1/7",true};
    {Runtime::Native n(r);r.admit_types({&forbidden,1});}
    // Admission owns the canonical key; caller input storage may be reused.
    forbidden.dictionary_key_schema.assign(4096,'x');forbidden.dictionary_key_forbidden=false;
    reject([&]{r.dictionary_count(token,d);});reject([&]{r.dictionary_read(&out,token,d,&selected);});
    // Failed/conflicting admission cannot erase the published policy index.
    NominalDescriptor conflict{"g36.conflict","","v1",{8,{}},"void.fixed.scalar.v1/7",false};
    {Runtime::Native n(r);reject([&]{r.admit_types({&conflict,1});});r.admit_types({&unrelated,1});}
    reject([&]{r.dictionary_count(token,d);});
    // The legacy policy also covers the nullable canonical spelling.
    key.key="g36.key?";entry.key="g36.nullable-entry";r.dictionary_new(&dict,d);
    require(r.dictionary_count(token,d)==0,"nullable policy warmup");
    NominalDescriptor nullable{"g36.nullable-policy","","v1",{8,{}},"g36.key",true};
    {Runtime::Native n(r);r.admit_types({&nullable,1});}
    nullable.dictionary_key_schema.assign(4096,'x');
    reject([&]{r.dictionary_count(token,d);});
    dict=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"policy payload retained");
}
void strings(){
    Runtime r(1);Runtime::Attachment a(r);Runtime::Managed m(r);void* source=nullptr;void* out=nullptr;constexpr std::size_t zero=0;Runtime::Slot slots[]={{&source,sizeof(source),&zero,1},{&out,sizeof(out),&zero,1}};Runtime::Frame roots(r,slots,2);
    const std::string scalars[]={"a","\xc2\xa2","\xe9\x9b\xaa","\xf0\x9f\x98\x80",std::string(1,'\0')};
    std::string text;for(int i=0;i<257;++i)for(const auto& s:scalars)text+=s;
    r.string_into(&source,text);require(r.string_count(source,false)==1285 && r.string_count(source,true)==text.size(),"Unicode scalar count");
    for(unsigned encoding=0;encoding<=7;++encoding){
        const auto kind=static_cast<ScalarEncoding>(encoding);const auto index=encoding==0||encoding==4?17u:1284u;
        r.string_index_into(&out,source,kind,index);require(r.string_bytes(out)==scalars[index%5],"nonnegative Unicode index");
        if(encoding<4){r.string_index_into(&out,source,kind,~std::uint64_t{});require(r.string_bytes(out)==scalars[4],"negative Unicode index");}
    }
    reject([&]{r.string_index_into(&out,source,ScalarEncoding::u64,~std::uint64_t{});});
    reject([&]{r.string_index_into(&out,source,ScalarEncoding::i64,std::uint64_t{1}<<63);});
    reject([&]{r.string_index_into(&out,source,ScalarEncoding::u64,1285);});
    reject([&]{r.string_index_into(&out,source,ScalarEncoding::f32,0);});
    reject([&]{r.string_into(&out,"\xc0\x80");});require(r.string_count(source,false)==1285,"invalid input changed source");
    r.string_index_into(&out,source,ScalarEncoding::i64,0);require(r.collect()==Runtime::Collection::completed && r.string_bytes(out)=="a","Unicode failure recovery/GC");
    source=out=nullptr;require(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"strings retained");
}
}
int main(){try{abi_failure_order();geometry();source_reads();policies();strings();std::cout<<"checked collection geometry, live policies and Unicode passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
