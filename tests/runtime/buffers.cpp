#include "void/runtime.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <new>
#include <thread>
#include <utility>
#include <unordered_map>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace {
struct Stats {std::atomic<std::size_t> calls{0},bytes{0},live{0},live_calls{0};};
Stats stats[4];thread_local int recording=-1;thread_local long fail_after=-1;
struct alignas(std::max_align_t) Header {void* base;std::size_t alignment,bytes;int record;};
thread_local std::size_t capture_size=0;
thread_local void* captured=nullptr;
thread_local bool poison=false;
thread_local std::latch* allocation_entered=nullptr;
thread_local std::latch* allocation_resume=nullptr;
thread_local vd::Runtime* allocating_runtime=nullptr;
thread_local std::latch* zeroing_entered=nullptr;
thread_local std::latch* zeroing_resume=nullptr;
std::atomic<bool> allocation_was_native{false};
// Linux test-only Heap collection-entry witness, after Runtime's rendezvous and
// root enumeration under its mutex. Both candidate values are already rooted.
#ifdef VD_BUFFER_WRAP_COLLECT
thread_local void** redirect_slot=nullptr;
thread_local void* redirect_value=nullptr;
#endif
void* allocate(std::size_t size,std::size_t alignment) {
    if(fail_after==0)throw std::bad_alloc();if(fail_after>0)--fail_after;
    auto prefix=alignment ? (sizeof(Header)+alignment-1)&~(alignment-1) : sizeof(Header);
    if(size>std::numeric_limits<std::size_t>::max()-prefix)throw std::bad_alloc();
    auto bytes=prefix+(size ? size : 1);void* base=nullptr;
    if(alignment) {
#ifdef _WIN32
        base=_aligned_malloc(bytes,alignment);
#else
        if(posix_memalign(&base,alignment,bytes))base=nullptr;
#endif
    } else base=std::malloc(bytes);
    if(!base)throw std::bad_alloc();
    auto* data=static_cast<char*>(base)+prefix;auto* header=reinterpret_cast<Header*>(data)-1;
    *header={base,alignment,size,recording};
    if(recording>=0) {++stats[recording].calls;stats[recording].bytes+=size;stats[recording].live+=size;++stats[recording].live_calls;}
    if(capture_size && capture_size==size) {
        captured=data;
        if(allocating_runtime)allocation_was_native=!allocating_runtime->is_managed();
        if(allocation_entered) {allocation_entered->count_down();allocation_resume->wait();}
        if(poison)std::memset(data,0xa5,size);
    }
    return data;
}
void deallocate(void* data,std::size_t alignment) noexcept {
    if(!data)return;
    auto* header=static_cast<Header*>(data)-1;
    if(header->alignment!=alignment)std::terminate(); // independently checks delete family/alignment
    if(header->record>=0) {stats[header->record].live-=header->bytes;--stats[header->record].live_calls;}
    auto* base=header->base;
#ifdef _WIN32
    if(alignment) {_aligned_free(base);return;}
#endif
    std::free(base);
}
}
void* operator new(std::size_t n) {return allocate(n,0);}
void operator delete(void* p) noexcept {deallocate(p,0);}
void operator delete(void* p,std::size_t) noexcept {deallocate(p,0);}
void* operator new[](std::size_t n) {return allocate(n,0);}
void operator delete[](void* p) noexcept {deallocate(p,0);}
void operator delete[](void* p,std::size_t) noexcept {deallocate(p,0);}
void* operator new(std::size_t n,std::align_val_t a) {return allocate(n,static_cast<std::size_t>(a));}
void operator delete(void* p,std::align_val_t a) noexcept {deallocate(p,static_cast<std::size_t>(a));}
void operator delete(void* p,std::size_t,std::align_val_t a) noexcept {deallocate(p,static_cast<std::size_t>(a));}
void* operator new[](std::size_t n,std::align_val_t a) {return allocate(n,static_cast<std::size_t>(a));}
void operator delete[](void* p,std::align_val_t a) noexcept {deallocate(p,static_cast<std::size_t>(a));}
void operator delete[](void* p,std::size_t,std::align_val_t a) noexcept {deallocate(p,static_cast<std::size_t>(a));}
#ifdef VD_BUFFER_WRAP_MEMSET
extern "C" void* __real_memset(void*,int,std::size_t);
#ifdef VD_BUFFER_WRAP_ASAN_MEMSET
extern "C" void* __real___asan_memset(void*,int,std::size_t);
#endif
template<bool Asan> void* zeroing_witness(void* data,int value,std::size_t bytes) {
    auto real=[](void* p,int v,std::size_t n) {
#ifdef VD_BUFFER_WRAP_ASAN_MEMSET
        if constexpr(Asan)return __real___asan_memset(p,v,n);
#endif
        return __real_memset(p,v,n);
    };
    if(!value && zeroing_entered && bytes==capture_size) {
        auto half=bytes/2;real(data,0,half);
        zeroing_entered->count_down();zeroing_resume->wait();
        real(static_cast<char*>(data)+half,0,bytes-half);return data;
    }
    return real(data,value,bytes);
}
extern "C" void* __wrap_memset(void* data,int value,std::size_t bytes) {return zeroing_witness<false>(data,value,bytes);}
#ifdef VD_BUFFER_WRAP_ASAN_MEMSET
extern "C" void* __wrap___asan_memset(void* data,int value,std::size_t bytes) {return zeroing_witness<true>(data,value,bytes);}
#endif
#endif

#ifdef VD_BUFFER_WRAP_COLLECT
// Verified Linux x64 native C++ ABI symbol for Heap::collect(span<void* const>).
// The first explicit argument is the method's existing this parameter. This is
// only a test link wrapper; Runtime and the production library stay unmodified.
extern "C" void __real__ZN2vd4Heap7collectESt4spanIKPvLm18446744073709551615EE(vd::Heap*,std::span<void* const>);
extern "C" void __wrap__ZN2vd4Heap7collectESt4spanIKPvLm18446744073709551615EE(vd::Heap* heap,std::span<void* const> roots) {
    if(redirect_slot) {*redirect_slot=redirect_value;redirect_slot=nullptr;}
    __real__ZN2vd4Heap7collectESt4spanIKPvLm18446744073709551615EE(heap,roots);
}
#endif

namespace {
using namespace vd;
using Clock=std::chrono::steady_clock;
constexpr BufferWorkQuota quota{65536,1,64}; // explicit fixture scheduling choice, not a production default
void check(bool value,const char* what) {if(!value)throw std::runtime_error(what);}
template<class F> void reject(F f) {try{f();}catch(const std::logic_error&){return;}throw std::runtime_error("invalid buffer operation accepted");}
ElementSchemaSpec spec(std::string name,std::vector<SchemaNode> nodes) {
    auto root=nodes.size()-1;return {std::move(name),"mutable","native-cpp-buffer-fixture-v1",StorageDomain::native(),root,std::move(nodes)};
}
ElementSchemaRef admit(Runtime& r,ElementSchemaSpec s) {r.admit_buffer_schemas({&s,1});return r.buffer_schema(s.identity);}
struct Output {
    void* value=nullptr;static constexpr std::size_t zero=0;Runtime::Slot slot{&value,sizeof(value),&zero,1};Runtime::Frame frame;
    explicit Output(Runtime& r):frame(r,&slot,1){}
};
void layouts() {
    Runtime r;auto u8=admit(r,spec("u8",{SchemaScalar{ScalarEncoding::u8}}));
    auto empty=r.buffer_layout(u8,0);check(!empty->bytes() && empty->stride()==1,"typed empty geometry");
    auto many=r.buffer_layout(u8,8*1024*1024);check(many->bytes()==8*1024*1024,"multi-megabyte layout");
    reject([&]{r.buffer_layout(u8,0,1);});reject([&]{r.buffer_layout(u8,std::numeric_limits<std::uint64_t>::max());});
    reject([&]{r.buffer_layout(u8,std::numeric_limits<std::int64_t>::max(),0,2);});
    reject([&]{r.buffer_layout(u8,std::numeric_limits<std::int64_t>::max(),0,3);});
    reject([&]{r.buffer_layout(u8,2,std::numeric_limits<std::size_t>::max()-1);});
    reject([&]{many->element_offset(many->capacity());});
    auto aligned=admit(r,spec("aligned",{SchemaBytes{3,256}}));
    auto pad=r.buffer_layout(aligned,4,256,512);check(pad->bytes()==2304 && pad->element_offset(3)==1792,"independent padded geometry");
    reject([&]{r.buffer_layout(aligned,1,1);});reject([&]{r.buffer_layout(aligned,1,0,3);});
    auto zero=admit(r,spec("zero",{SchemaBytes{0,64}}));auto zeros=r.buffer_layout(zero,1000000);
    check(zeros->stride()==64 && zeros->bytes()==64000000,"zero-footprint must retain positive stride");
    Runtime other;auto foreign=admit(other,spec("u8",{SchemaScalar{ScalarEncoding::u8}}));reject([&]{r.buffer_layout(foreign,1);});
    std::weak_ptr<const BufferLayout> capacity_layout;
    {auto temporary=r.buffer_layout(u8,123);capacity_layout=temporary;}
    check(capacity_layout.expired(),"capacity layout permanently interned");
}
void byte_oracles() {
    Runtime r;auto u8=admit(r,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));
    auto alias=admit(r,spec("different-byte-identity",{SchemaScalar{ScalarEncoding::u8}}));
    auto record=admit(r,spec("record",{SchemaScalar{ScalarEncoding::u16},SchemaScalar{ScalarEncoding::boolean},SchemaBytes{7,1},SchemaRecord{16,2,{{20,8,2},{10,0,0},{30,2,1}}}}));
    auto run=admit(r,spec("run",{SchemaScalar{ScalarEncoding::u8},SchemaRepeat{1,1,5,0,6,1}}));
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
    const std::size_t n=4*1024*1024;auto layout=r.buffer_layout(u8,n+3,3);
    std::vector<std::byte> source(n);for(std::size_t i=0;i<n;++i)source[i]=std::byte((i*29+7)%251);
    capture_size=layout->bytes();poison=true;
    {Runtime::BufferBuilder builder(r,layout,n,quota);
        check(captured && !r.contains(captured),"private object exposed through contains");
        reject([&]{Runtime::Handle h(r,captured);});reject([&]{r.buffer_byte(captured,layout,0,0);});
        check(r.collect()==Runtime::Collection::completed && r.snapshot().objects==1,"adopted private root not retained");
        builder.write_bytes(source);check(r.collect()==Runtime::Collection::completed,"collection between construction/publication");
        builder.finish_into(&out.value);
    }
    poison=false;capture_size=0;check(out.value==captured && r.contains(out.value),"complete output missing");
    auto* raw=static_cast<const std::byte*>(captured);
    for(std::size_t i=0;i<3;++i)check(raw[i]==std::byte{0},"leading padding not zeroed");
    for(std::size_t i=0;i<n;++i)check(r.buffer_byte(out.value,layout,i,0)==source[i],"independent byte oracle mismatch");
    for(std::size_t i=3+n;i<layout->bytes();++i)check(raw[i]==std::byte{0},"unused capacity not zero");
    reject([&]{r.buffer_byte(out.value,layout,n,0);});
    reject([&]{r.buffer_byte(out.value,r.buffer_layout(alias,n+3,3),0,0);});check(r.buffer_byte(out.value,r.buffer_layout(u8,n+3,3),0,0)==source[0],"equivalent capacity layout rejected");
    reject([&]{r.buffer_byte(out.value,r.buffer_layout(u8,n+2,3),0,0);});
    auto records=r.buffer_layout(record,2);
    {Runtime::BufferBuilder b(r,records,2,{3,1,1});b.write_scalar(0x1234);b.write_scalar(1);
        std::array<std::byte,7> bytes;bytes.fill(std::byte{0x91});b.write_bytes({bytes.data(),2});r.collect();b.write_bytes({bytes.data()+2,5});b.zero_elements(1);b.finish_into(&out.value);}
    std::uint16_t number=0x1234;std::array<std::byte,16> expected{};std::memcpy(expected.data(),&number,2);expected[2]=std::byte{1};
    for(std::size_t i=8;i<15;++i)expected[i]=std::byte{0x91};
    for(std::size_t i=0;i<16;++i)check(r.buffer_byte(out.value,records,0,i)==expected[i] && r.buffer_byte(out.value,records,1,i)==std::byte{0},"nested scalar/byte/padding oracle");
    auto runs=r.buffer_layout(run,1);
    {Runtime::BufferBuilder b(r,runs,1,{1,1,1});b.write_scalar(17);std::array<std::byte,4> tail{std::byte{18},std::byte{19},std::byte{20},std::byte{21}};b.write_bytes(tail);b.finish_into(&out.value);}
    check(r.buffer_byte(out.value,runs,0,0)==std::byte{0},"repeat base padding changed");
    for(std::size_t i=1;i<6;++i)check(r.buffer_byte(out.value,runs,0,i)==std::byte(16+i),"repeat coalescing lost scalar identity/order");
    out.value=nullptr;r.collect();check(!r.snapshot().objects,"complete buffer not reclaimed");captured=nullptr;
}
void scalar_and_deep_oracles() {
    Runtime r;std::vector<BufferLayoutRef> layouts;
    for(auto encoding:{ScalarEncoding::i8,ScalarEncoding::i16,ScalarEncoding::i32,ScalarEncoding::i64,ScalarEncoding::u8,ScalarEncoding::u16,ScalarEncoding::u32,ScalarEncoding::u64,ScalarEncoding::f32,ScalarEncoding::f64,ScalarEncoding::boolean})
        layouts.push_back(r.buffer_layout(admit(r,spec("scalar"+std::to_string(static_cast<int>(encoding)),{SchemaScalar{encoding}})),1));
    auto deep=spec("deep-data",{SchemaScalar{ScalarEncoding::u8}});
    for(std::size_t i=0;i<2048;++i)deep.nodes.emplace_back(SchemaRepeat{0,1,1,i,1,1});deep.root=deep.nodes.size()-1;
    auto nested=r.buffer_layout(admit(r,std::move(deep)),1);
    auto impossible=admit(r,spec("unrepresentable",{SchemaBytes{std::numeric_limits<std::size_t>::max(),1}}));
    auto round_overflow=admit(r,spec("round-overflow",{SchemaBytes{std::numeric_limits<std::size_t>::max(),64}}));
    reject([&]{r.buffer_layout(round_overflow,1);});
    reject([&]{r.buffer_layout(impossible,1);});
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
    for(std::size_t i=0;i<layouts.size();++i) {
        auto& layout=layouts[i];auto width=layout->schema()->bytes();auto bits=i==10 ? std::uint64_t{1} : width==1 ? 0xf1ULL : width==2 ? 0x8123ULL : width==4 ? 0x7fc01234ULL : 0x7ff8000000001234ULL;
        std::array<std::byte,8> oracle{};
        if(width==1) {auto v=static_cast<std::uint8_t>(bits);std::memcpy(oracle.data(),&v,1);}
        if(width==2) {auto v=static_cast<std::uint16_t>(bits);std::memcpy(oracle.data(),&v,2);}
        if(width==4) {auto v=static_cast<std::uint32_t>(bits);std::memcpy(oracle.data(),&v,4);}
        if(width==8)std::memcpy(oracle.data(),&bits,8);
        {Runtime::BufferBuilder b(r,layout,1,quota);b.write_scalar(bits);b.finish_into(&out.value);}
        for(std::size_t n=0;n<width;++n)check(r.buffer_byte(out.value,layout,0,n)==oracle[n],"native scalar bit oracle");
    }
    {Runtime::BufferBuilder b(r,nested,1,{1,1,1});b.write_scalar(0x71);b.finish_into(&out.value);}
    check(r.buffer_byte(out.value,nested,0,0)==std::byte{0x71},"deep construction lost scalar");out.value=nullptr;r.collect();
}
void empty_aligned_and_negatives() {
    Runtime r;auto bytes=admit(r,spec("bytes",{SchemaBytes{3,256}}));auto u8=admit(r,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));
    auto boolean=admit(r,spec("bool",{SchemaScalar{ScalarEncoding::boolean}}));auto i64=admit(r,spec("i64",{SchemaScalar{ScalarEncoding::i64}}));
    auto zero=admit(r,spec("zero",{SchemaBytes{0,1}}));
    NominalDescriptor nominal{"owner","","plain",{8,{}}};r.admit_types({&nominal,1});
    auto refs=admit(r,spec("ref",{SchemaManaged{"owner",false}}));
    auto empty_refs=admit(r,spec("empty-ref",{SchemaManaged{"owner",false},SchemaRepeat{0,sizeof(void*),0,0,0,alignof(void*)}}));
    Runtime foreign;auto foreign_schema=admit(foreign,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));auto foreign_layout=foreign.buffer_layout(foreign_schema,1);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);r.allocate_into(&out.value,{8,{}});auto* sentinel=out.value;
    Output parent(r);r.allocate_into(&parent.value,{sizeof(void*),{0}});
    auto aligned=r.buffer_layout(bytes,4,256,512);capture_size=aligned->bytes();poison=true;
    {Runtime::BufferBuilder b(r,aligned,3,quota);check(reinterpret_cast<std::uintptr_t>(captured)%256==0,"over-aligned reservation");reject([&]{r.store(parent.value,0,captured);});reject([&]{r.store(captured,0,parent.value);});b.zero_elements(3);b.finish_into(&out.value);}
    for(std::size_t i=0;i<aligned->bytes();++i)check(static_cast<const std::byte*>(captured)[i]==std::byte{0},"aligned full reservation not zeroed");
    capture_size=0;poison=false;parent.value=nullptr;
    auto empty=r.buffer_layout(u8,0);auto before=r.snapshot().objects;
    {Runtime::BufferBuilder b(r,empty,0,quota);b.finish_into(&out.value);}check(!out.value && r.snapshot().objects==before,"empty capacity allocated object");
    {Runtime::BufferBuilder b(r,r.buffer_layout(zero,1000000),1000000,quota);b.zero_elements(1000000);b.finish_into(&out.value);}check(r.contains(out.value),"zero-footprint nonempty buffer absent");
    out.value=sentinel;r.collect();check(r.snapshot().objects==1,"old sentinel lost during buffer collection");
    auto one=r.buffer_layout(u8,1);
    reject([&]{Runtime::BufferBuilder b(r,foreign_layout,1,quota);});reject([&]{Runtime::BufferBuilder b(r,{},1,quota);});
    reject([&]{Runtime::BufferBuilder b(r,one,2,quota);});reject([&]{Runtime::BufferBuilder b(r,one,1,{0,1,1});});
    reject([&]{Runtime::BufferBuilder b(r,one,1,{1,0,1});});reject([&]{Runtime::BufferBuilder b(r,one,1,{1,1,0});});
    reject([&]{Runtime::BufferBuilder b(r,r.buffer_layout(i64,1),1,{1,1,1});});
    {Runtime::BufferBuilder b(r,r.buffer_layout(refs,1),1,quota);reject([&]{b.finish_into(&out.value);});}
    // Stage 2D now supports the empty repeated shape, without inventing a null
    // required value: its logical repetition contains no reference occurrence.
    {Output empty_result(r);Runtime::BufferBuilder b(r,r.buffer_layout(empty_refs,1),1,quota);b.finish_into(&empty_result.value);}
    {Runtime::BufferBuilder b(r,one,1,quota);reject([&]{b.finish_into(&out.value);});reject([&]{b.write_scalar(1);});}
    {Runtime::BufferBuilder b(r,one,1,quota);reject([&]{b.write_scalar(256);});reject([&]{b.finish_into(&out.value);});}
    {Runtime::BufferBuilder b(r,r.buffer_layout(boolean,1),1,quota);reject([&]{b.write_scalar(2);});}
    {Runtime::BufferBuilder b(r,r.buffer_layout(boolean,1),1,quota);std::byte v{1};reject([&]{b.write_bytes({&v,1});});}
    {Runtime::BufferBuilder b(r,r.buffer_layout(bytes,1),1,quota);std::byte v{1};b.write_bytes({&v,1});reject([&]{b.zero_elements(1);});}
    {Runtime::BufferBuilder b(r,one,1,quota);b.zero_elements(1);void* unregistered=nullptr;reject([&]{b.finish_into(&unregistered);});reject([&]{b.finish_into(&out.value);});}
    {Runtime::BufferBuilder b(r,empty,0,quota);std::byte v{1};reject([&]{b.write_bytes({&v,1});});}
    TypeLayout forbidden{8,{}};forbidden.kind=TypeLayout::Kind::buffer;
    reject([&]{r.allocate_into(&out.value,forbidden);});std::uint64_t raw=0;reject([&]{r.construct_into(&out.value,forbidden,&raw);});
    check(out.value==sentinel,"failed construction changed output");r.collect();check(r.snapshot().objects==1,"abandoned private buffers not reclaimed");
    out.value=nullptr;r.collect();
    std::weak_ptr<const BufferLayout> owned_layout;
    {auto temporary=r.buffer_layout(zero,2);owned_layout=temporary;
        {Runtime::BufferBuilder b(r,temporary,2,quota);b.zero_elements(2);b.finish_into(&out.value);}}
    check(!owned_layout.expired(),"published buffer lost capacity metadata");out.value=nullptr;r.collect();
    check(owned_layout.expired(),"collected buffer retained historical capacity metadata");captured=nullptr;
}
long transaction(long ordinal,std::size_t budget,bool measure) {
    Runtime r(budget);auto s=admit(r,spec("aligned",{SchemaBytes{4096,256}}));auto layout=r.buffer_layout(s,1);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
    {Runtime::BufferBuilder b(r,layout,1,quota);b.zero_elements(1);b.finish_into(&out.value);}
    auto* sentinel=out.value;
    stats[0].calls=0;stats[0].bytes=0;check(!stats[0].live,"prior measurement retained storage");recording=0;fail_after=ordinal;bool failed=false;
    try {Runtime::BufferBuilder b(r,layout,1,quota);b.zero_elements(1);b.finish_into(&out.value);}catch(const std::bad_alloc&){failed=true;}
    recording=-1;fail_after=-1;auto sites=static_cast<long>(stats[0].calls.load());
    if(!measure)check(failed && out.value==sentinel,"failed allocation changed output or did not fail");
    check(!r.snapshot().requested && r.collect()==Runtime::Collection::completed,"failed allocation poisoned collection epoch");
    check(!failed || r.snapshot().objects==1,"failed reservation/adoption leaked managed object");
    out.value=nullptr;r.collect();check(!stats[0].live,"failed transaction leaked metadata/reservation/map allocation");return sites;
}
void failures() {
    for(auto budget:{std::size_t{0},std::size_t{1}}) {
        auto sites=transaction(-1,budget,true);check(sites>=4,"reservation/adoption failure work not measured");
        for(long ordinal=0;ordinal<sites;++ordinal)transaction(ordinal,budget,false);
        std::cout<<"buffer allocation-site sweep budget="<<budget<<": "<<sites<<" sites\n";
    }
}
void native_and_chunk_collections() {
    Runtime r;auto s=admit(r,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));
    auto layout=r.buffer_layout(s,64*1024*1024);
    std::latch entered(1),resume(1),prepared(1),finish(1),half_zeroed(1),zero_resume(1);std::atomic<void*> result{nullptr};
    auto worker=std::async(std::launch::async,[&] {
        Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
        capture_size=layout->bytes();poison=true;allocation_entered=&entered;allocation_resume=&resume;allocating_runtime=&r;
#ifdef VD_BUFFER_WRAP_MEMSET
        zeroing_entered=&half_zeroed;zeroing_resume=&zero_resume;
#endif
        {Runtime::BufferBuilder b(r,layout,layout->capacity(),quota);
            capture_size=0;allocation_entered=nullptr;allocation_resume=nullptr;allocating_runtime=nullptr;zeroing_entered=nullptr;zeroing_resume=nullptr;
            prepared.count_down();{Runtime::Native native(r);finish.wait();}
            b.zero_elements(layout->capacity());b.finish_into(&out.value);result=out.value;
            check(r.buffer_byte(out.value,layout,layout->capacity()-1,0)==std::byte{0},"concurrent zeroing incomplete");}
        // Keep published value rooted while main observes completion.
    });
    entered.wait();
    {Runtime::Attachment a(r);Runtime::Managed m(r);
        check(allocation_was_native,"raw allocation was Managed");
        check(r.collect(std::chrono::seconds(5))==Runtime::Collection::completed && !r.snapshot().objects,"collector blocked on NativeSafe raw reservation");
        resume.count_down();
#ifdef VD_BUFFER_WRAP_MEMSET
        {Runtime::Native native(r);half_zeroed.wait();}
        check(r.collect(std::chrono::seconds(5))==Runtime::Collection::completed && !r.snapshot().objects,"collector observed/blocked on half-zeroed raw reservation");
        zero_resume.count_down();
#else
        // No linker wrapper on this platform: concurrent NativeSafe-phase witness,
        // not a claim of instruction-level observation during memset.
        check(r.collect(std::chrono::seconds(5))==Runtime::Collection::completed,"collector could not rendezvous with NativeSafe zeroing phase");
#endif
        {Runtime::Native native(r);prepared.wait();}
        check(r.collect()==Runtime::Collection::completed && r.snapshot().objects==1,"private adopted buffer not rooted");
        finish.count_down();{Runtime::Native native(r);worker.get();}
        check(result.load()!=nullptr,"concurrent buffer not published");r.collect();check(!r.snapshot().objects,"retired worker buffer not collected");}
    // A request-ready signal is insufficient: the collector may not run again
    // until after the writer finishes. Instead hold a PARTIALLY populated builder
    // Managed, observe the actual pending epoch from an unattached thread, then
    // let the next production write_bytes poll perform the rendezvous.
    auto many=r.buffer_layout(s,4*1024*1024);std::vector<std::byte> input(many->capacity(),std::byte{0x71});
    {Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
        Runtime::BufferBuilder b(r,many,many->capacity(),{4096,1,8});
        constexpr std::size_t prefix=4096;
        auto bytes=std::span<const std::byte>(input);
        b.write_bytes(bytes.first(prefix));
        const auto before=r.snapshot();
        auto observer=std::async(std::launch::async,[&] {
            // The writer must stay Managed; wait_for_request deliberately rejects
            // that state. This observer has no attachment or root responsibility.
            // A watchdog can fail the test; elapsed time never satisfies it.
            const bool observed=r.wait_for_request(std::chrono::seconds(5));
            return std::pair{observed,r.snapshot()};
        });
        auto collector=std::async(std::launch::async,[&] {
            Runtime::Attachment attached(r);Runtime::Managed managed(r);
            return r.collect();
        });
        const auto [observed,pending]=observer.get(); // no Native transition or writer poll
        const bool writer_managed=r.is_managed();
        const bool output_unchanged=out.value==nullptr;
        b.write_bytes(bytes.subspan(prefix));
        const auto after_poll=r.snapshot(); // before finish or a Native transition
        b.finish_into(&out.value);
        Runtime::Collection collected;
        {Runtime::Native native(r);collected=collector.get();}
        // Distinct assertions identify request, completion and poll/park failures.
        check(writer_managed && output_unchanged,"partial builder did not remain private and Managed");
        check(observed && pending.requested && pending.epoch==before.epoch+1 && pending.completed_epoch==before.completed_epoch && pending.parked==1 && pending.threads==2 && pending.objects==1,
              "collection request did not overlap the Managed partial builder");
        check(collected==Runtime::Collection::completed,"partial-buffer collection did not complete");
        check(!after_poll.requested && after_poll.completed_epoch==pending.epoch && after_poll.objects==1,
              "chunk write did not complete the pending collection");
        check(after_poll.cooperative_parks==before.cooperative_parks+1,
              "chunk write did not cooperatively park for the pending collection");
        check(r.buffer_byte(out.value,many,0,0)==std::byte{0x71} &&
              r.buffer_byte(out.value,many,prefix-1,0)==std::byte{0x71} &&
              r.buffer_byte(out.value,many,prefix,0)==std::byte{0x71} &&
              r.buffer_byte(out.value,many,many->capacity()-1,0)==std::byte{0x71},"chunk write content");
        std::cout<<"Managed partial-buffer request: completed_collections="<<(after_poll.completed_epoch-before.completed_epoch)
                 <<" cooperative_parks="<<(after_poll.cooperative_parks-before.cooperative_parks)<<"\n";
        out.value=nullptr;r.collect();}
    std::cout<<"NativeSafe reservation/zeroing, private adoption and deterministic chunk collection passed\n";
}
void allocate_nominal(Runtime& r,Output& out,std::string_view name) {
    auto descriptor=r.descriptor(name);auto layout=descriptor->layout;layout.descriptor=descriptor;r.allocate_into(&out.value,layout);
}
void reference_buffers() {
    Runtime r;const auto w=sizeof(void*),a=alignof(void*);
    std::array<NominalDescriptor,3> types{{{"Base","","base",{w,{0}}},{"Derived","Base","derived",{2*w,{0}}},{"Other","","other",{w,{}}}}};
    std::array<InterfaceWitness,3> witnesses{{{"interface:1:I4:Base","I","Base","read"},{"interface:1:I7:Derived","I","Derived","read"},{"interface:1:J5:Other","J","Other","read"}}};
    r.admit_metadata(types,witnesses);
    auto managed=admit(r,spec("required",{SchemaManaged{"Base",false}}));
    auto nullable=admit(r,spec("nullable",{SchemaManaged{"Base",true}}));
    auto string=admit(r,spec("string",{SchemaString{}}));
    auto interface=admit(r,spec("interface",{SchemaInterface{"I",false,2*w,0,3*w,a}}));
    auto optional_interface=admit(r,spec("optional-interface",{SchemaInterface{"I",true,2*w,0,3*w,a}}));
    auto nested=admit(r,spec("nested-refs",{SchemaScalar{ScalarEncoding::u8},SchemaManaged{"Base",false},SchemaString{},SchemaInterface{"I",false,2*w,0,3*w,a},
        SchemaRecord{8*w,a,{{40,3*w,3},{20,w,1},{10,0,0},{30,2*w,2}}},SchemaRepeat{w,9*w,3,4,28*w,a}}));
    auto large=admit(r,spec("million-refs",{SchemaManaged{"Base",true},SchemaRepeat{0,w,1000000,0,1000000*w,a}}));
    auto single_repeat=admit(r,spec("single-repeat-ref",{SchemaManaged{"Base",true},SchemaRepeat{0,w,1,0,w,a}}));
    auto layout=r.buffer_layout(nested,5,2*w,30*w);
    // Independently enumerate physical offsets, including the full unused capacity.
    reject([&]{BufferReferenceCursor invalid({});});
    BufferReferenceCursor cursor(layout);std::size_t events=0;
    for(std::size_t outer=0;outer<5;++outer)for(std::size_t inner=0;inner<3;++inner)for(std::size_t leaf=0;leaf<3;++leaf) {
        auto e=cursor.next();check(e && e->element==outer,"capacity cursor lost element");
        const auto base=2*w+outer*30*w+w+inner*9*w;
        const auto expected=base+(leaf==0 ? w : leaf==1 ? 2*w : 5*w);
        check(e->reference.offset==expected,"independent repeated reference offset mismatch");
        check(e->reference.kind==(leaf==0 ? ReferenceEvent::Kind::managed : leaf==1 ? ReferenceEvent::Kind::string : ReferenceEvent::Kind::interface),"reference event kind mismatch");
        if(leaf==2)check(e->reference.witness_offset==base+3*w,"independent witness offset mismatch");
        auto local=ReferenceShape(nested).at(expected-(2*w+outer*30*w));
        check(local && local->kind==e->reference.kind,"construction/lookup/capacity stream disagreement");++events;
    }
    check(events==45 && !cursor.next(),"capacity cursor did not terminate");
    Heap foreign;Root foreign_root(foreign,foreign.allocate({w,{}}));
    Runtime::Attachment attached(r);Runtime::Managed active(r);Output object(r),text(r),child(r),other(r),out(r),source(r);
    allocate_nominal(r,object,"Derived");allocate_nominal(r,child,"Other");allocate_nominal(r,other,"Other");r.store(object.value,0,child.value);r.string_into(&text.value,"GC string 🎮");
    const auto object_token=r.root_word(&object.value),text_token=r.root_word(&text.value),source_token=r.root_word(&source.value);
    const auto* witness=r.witness("interface:1:I7:Derived");const auto* base_witness=r.witness("interface:1:I4:Base");
    capture_size=layout->bytes();
    {Runtime::BufferBuilder b(r,layout,2,quota);
        for(std::size_t outer=0;outer<2;++outer)for(std::size_t inner=0;inner<3;++inner) {
            b.write_scalar(outer*3+inner+1);b.write_reference(object_token);
            check(r.collect()==Runtime::Collection::completed,"partial required-reference element unsafe to trace");
            b.write_reference(text_token);b.write_interface({object_token,inner%2 ? base_witness : witness});
        }
        b.finish_into(&out.value);}
    capture_size=0;auto* raw=static_cast<const char*>(out.value);
    for(std::size_t outer=0;outer<2;++outer)for(std::size_t inner=0;inner<3;++inner) {
        const auto base=2*w+outer*30*w+w+inner*9*w;void* value=nullptr;const void* observed=nullptr;
        std::memcpy(&value,raw+base+w,w);check(value==object.value,"managed pointer byte oracle");
        std::memcpy(&value,raw+base+2*w,w);check(value==text.value,"string pointer byte oracle");
        std::memcpy(&value,raw+base+5*w,w);check(value==object.value,"interface object byte oracle");
        std::memcpy(&observed,raw+base+3*w,w);check(observed==(inner%2 ? base_witness : witness),"atomic interface pair oracle");
    }
    for(std::size_t i=layout->element_offset(2);i<layout->bytes();++i)check(raw[i]==0,"spare reference capacity was populated");
    object.value=text.value=child.value=nullptr;r.collect();check(r.snapshot().objects==5,"buffer references failed to retain graph/string");
    out.value=nullptr;r.collect();check(r.snapshot().objects==1,"dead reference buffer retained graph");
    allocate_nominal(r,object,"Derived");r.string_into(&text.value,"source");
    auto attempt=[&](ElementSchemaRef schema,auto action) {
        Runtime::BufferBuilder b(r,r.buffer_layout(schema,1),1,quota);action(b);reject([&]{b.finish_into(&out.value);});
    };
    source.value=nullptr;attempt(managed,[&](auto& b){reject([&]{b.write_reference(source_token);});});
    attempt(managed,[&](auto& b){reject([&]{b.zero_elements(1);});});
    attempt(managed,[&](auto& b){reject([&]{b.write_scalar(0);});});
    attempt(managed,[&](auto& b){std::array<std::byte,sizeof(void*)> bytes{};reject([&]{b.write_bytes(bytes);});});
    source.value=other.value;attempt(managed,[&](auto& b){reject([&]{b.write_reference(source_token);});});
    attempt(string,[&](auto& b){reject([&]{b.write_reference(object_token);});});
    attempt(managed,[&](auto& b){reject([&]{b.write_reference(text_token);});});
    source.value=foreign_root.get();attempt(nullable,[&](auto& b){reject([&]{b.write_reference(source_token);});});source.value=nullptr;
    attempt(interface,[&](auto& b){reject([&]{b.write_reference(object_token);});});
    attempt(managed,[&](auto& b){reject([&]{b.write_interface({object_token,witness});});});
    attempt(interface,[&](auto& b){reject([&]{b.write_interface({object_token,reinterpret_cast<const InterfaceWitness*>(std::uintptr_t{1})});});});
    InterfaceWitness forged=*witness;attempt(interface,[&](auto& b){reject([&]{b.write_interface({object_token,&forged});});});
    attempt(interface,[&](auto& b){reject([&]{b.write_interface({object_token,r.witness("interface:1:J5:Other")});});});
    attempt(interface,[&](auto& b){reject([&]{b.write_interface({r.root_word(&other.value),witness});});});
    attempt(optional_interface,[&](auto& b){reject([&]{b.write_interface({source_token,witness});});});
    attempt(interface,[&](auto& b){reject([&]{b.write_interface({object_token,nullptr});});});
    void* unregistered=nullptr;reject([&]{r.root_word(&unregistered);});
    auto foreign_token=std::async(std::launch::async,[&]{Runtime::Attachment attachment(r);Runtime::Managed managed_scope(r);Output temporary(r);return r.root_word(&temporary.value);}).get();
    attempt(nullable,[&](auto& b){reject([&]{b.write_reference(foreign_token);});});
    auto expired=[&]{Output temporary(r);return r.root_word(&temporary.value);}();
    attempt(nullable,[&](auto& b){reject([&]{b.write_reference(expired);});});
    // A registered public source word is still forbidden to name private storage.
    {auto one=r.buffer_layout(nullable,1);capture_size=one->bytes();Runtime::BufferBuilder b(r,one,1,quota);capture_size=0;
        source.value=captured;reject([&]{b.write_reference(source_token);});source.value=nullptr;}
    {Runtime::BufferBuilder b(r,r.buffer_layout(optional_interface,2),1,quota);b.write_interface({source_token,nullptr});b.finish_into(&out.value);}
    {Runtime::BufferBuilder b(r,r.buffer_layout(string,2),1,quota);b.write_reference(source_token);b.finish_into(&out.value);}
    {Runtime::BufferBuilder b(r,r.buffer_layout(nullable,2),1,quota);b.zero_elements(1);b.finish_into(&out.value);}
    // Compare actual constructor allocation requests for equal-complexity shapes;
    // repetition count must only change payload bytes, never metadata allocation.
    r.collect();std::size_t fixed_calls=0,fixed_metadata=0,index=0;
    for(auto shape:{r.buffer_layout(nullable,1),r.buffer_layout(nullable,1000000),r.buffer_layout(single_repeat,1),r.buffer_layout(large,1)}) {
        check(!stats[2].live,"previous compactness measurement retained work");stats[2].calls=0;stats[2].bytes=0;recording=2;
        {Runtime::BufferBuilder b(r,shape,shape->capacity(),quota);b.zero_elements(shape->capacity());b.finish_into(&out.value);}
        recording=-1;auto calls=stats[2].calls.load(),metadata=stats[2].bytes.load()-shape->bytes();
        if(!(index%2)) {fixed_calls=calls;fixed_metadata=metadata;}
        else check(calls==fixed_calls && metadata==fixed_metadata,"repetition count expanded compiler/runtime metadata");
        ++index;check(r.collect()==Runtime::Collection::completed,"million reference scan failed");
        out.value=nullptr;r.collect();check(!stats[2].live,"compact buffer metadata was not reclaimed");
    }
    check(large->node_count()==2 && large->edge_count()==1,"million repetitions expanded metadata");
    {Output late(r);Runtime::BufferBuilder b(r,r.buffer_layout(interface,1),1,quota);
        NominalDescriptor type{"Late","Base","late",{2*w,{0}}};InterfaceWitness added{"interface:1:I4:Late","I","Late","read"};
        {Runtime::Native native(r);r.admit_metadata({&type,1},{&added,1});}
        allocate_nominal(r,late,"Late");b.write_interface({r.root_word(&late.value),r.witness(added.key)});b.finish_into(&out.value);
        late.value=nullptr;check(r.collect()==Runtime::Collection::completed,"buffer lost newly admitted descriptor/witness ownership");}
    object.value=text.value=other.value=out.value=nullptr;r.collect();check(!r.snapshot().objects,"reference tests leaked graph");
    std::cout<<"typed managed/string/interface buffers, capacity oracle and million-reference scans passed\n";
}
// Isolate object-table ownership independently of Runtime/builder lifetimes.
// All payloads/nodes are collected, roots_ is never populated, and empty legacy
// layouts allocate no metadata. The actual platform's unordered_map supplies
// the bucket control; no expected libc++ size or allowance is hardcoded.
void object_table_lifetime() {
    struct Retained {std::size_t count,bytes;};
    auto retained=[] {return Retained{stats[1].live_calls.load(),stats[1].live.load()};};
    auto empty=[] {check(!stats[1].live_calls && !stats[1].live,"object table owner leaked native storage");};
    // First insertion exercises libstdc++'s bucket allocation; two-to-three
    // insertion isolates the transition in the original macOS transaction.
    for(auto initial:{std::size_t{0},std::size_t{2}}) {
        empty();Retained heap_storage{};
        {
            Heap heap;for(std::size_t i=0;i<initial;++i)heap.allocate({8,{}});
            recording=1;heap.allocate({8,{}});recording=-1;
            heap.collect();check(!heap.size(),"isolated object table retained GC objects");
            heap_storage=retained();
        }
        empty();Retained bucket_storage{};std::size_t before=0,after=0;
        {
            int keys[3]{};std::unordered_map<void*,std::size_t> table;
            for(std::size_t i=0;i<initial;++i)table.emplace(&keys[i],i);
            before=table.bucket_count();
            recording=1;table.emplace(&keys[initial],initial);recording=-1;after=table.bucket_count();
            table.clear();check(table.empty(),"bucket control retained entries");
            bucket_storage=retained();
        }
        empty();
        check(heap_storage.count==bucket_storage.count && heap_storage.bytes==bucket_storage.bytes,
              "isolated Heap retention differs from object-table bucket control");
        std::cout<<"object-table ownership witness: initial="<<initial<<" buckets="<<before<<"->"<<after
                 <<" heap_retained_count="<<heap_storage.count<<" heap_retained_bytes="<<heap_storage.bytes
                 <<" bucket_retained_count="<<bucket_storage.count<<" bucket_retained_bytes="<<bucket_storage.bytes
                 <<" after_owners_count="<<stats[1].live_calls<<" after_owners_bytes="<<stats[1].live<<'\n';
    }
}
void reference_native_empty() {
    check(!stats[0].live && !stats[0].live_calls,"reference failure sweep leaked native storage");
}
long reference_transaction(long ordinal,bool measure) {
    Runtime r;auto w=sizeof(void*);NominalDescriptor type{"Target","","plain",{w,{}}};r.admit_types({&type,1});
    auto schema=admit(r,spec("two-required",{SchemaManaged{"Target",false},SchemaRepeat{0,w,2,0,2*w,alignof(void*)}}));auto layout=r.buffer_layout(schema,2);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);allocate_nominal(r,source,"Target");r.allocate_into(&out.value,{8,{}});auto* sentinel=out.value;auto token=r.root_word(&source.value);
    reference_native_empty();stats[0].calls=0;stats[0].bytes=0;recording=0;fail_after=ordinal;bool failed=false,released_source=false;
    try {
        Runtime::BufferBuilder b(r,layout,1,quota);b.write_reference(token);b.write_reference(token);
        source.value=nullptr;released_source=true;
        const auto before=r.snapshot();auto collected=r.collect();
        if(collected==Runtime::Collection::allocation_failure) {
            check(r.snapshot().objects==before.objects && !r.snapshot().requested,"failed reference marking swept or stuck epoch");throw std::bad_alloc();
        }
        check(collected==Runtime::Collection::completed,"reference preparation collection failed");
        b.finish_into(&out.value);
    } catch(const std::bad_alloc&) {failed=true;}
    recording=-1;fail_after=-1;auto sites=static_cast<long>(stats[0].calls.load());
    if(!measure)check(failed && out.value==sentinel,"failed reference construction published or missed ordinal");
    check(r.collect()==Runtime::Collection::completed,"reference mark rollback retry failed");
    if(failed) {
        check(r.snapshot().objects==(released_source ? 1 : 2),"abandoned reference buffer not reclaimed");
        if(!source.value)allocate_nominal(r,source,"Target");
        Runtime::BufferBuilder retry(r,layout,1,quota);retry.write_reference(token);retry.write_reference(token);retry.finish_into(&out.value);
    }
    out.value=source.value=nullptr;
    check(r.collect()==Runtime::Collection::completed,"reference final collection failed");
    const auto objects=r.snapshot().objects;
    // Failure injection/recording are off. stdio diagnostics do not allocate via
    // this fixture's operator new or change the measured failure ordinals.
    std::fprintf(stderr,"reference sweep before owner teardown: baseline=%d ordinal=%ld sites=%ld failed=%d released_source=%d native_count=%zu native_bytes=%zu heap_objects=%zu\n",
        measure,ordinal,sites,failed,released_source,stats[0].live_calls.load(),stats[0].live.load(),objects);
    std::fflush(stderr);
    check(!objects,"reference failure sweep leaked GC objects");
    // Runtime owns object-table bucket capacity until its destructor. Native
    // leak detection belongs in the caller, after every transaction owner dies.
    return sites;
}
long checked_reference_transaction(long ordinal,bool baseline,std::optional<std::size_t> deliberate_leak={}) {
    void* leaked=nullptr;
    try {
        auto sites=reference_transaction(ordinal,baseline); // all owners destroyed on return
        // Negative controls deliberately keep a measured allocation past owner
        // teardown. Release it only after the strict check has rejected it.
        if(deliberate_leak) {recording=0;leaked=::operator new(*deliberate_leak);recording=-1;}
        std::fprintf(stderr,"reference sweep after owner teardown: baseline=%d ordinal=%ld native_count=%zu native_bytes=%zu\n",
            baseline,ordinal,stats[0].live_calls.load(),stats[0].live.load());
        std::fflush(stderr);
        reference_native_empty();
        return sites;
    } catch(...) {
        recording=-1;fail_after=-1;
        std::fprintf(stderr,"reference sweep rejected after owner teardown: baseline=%d ordinal=%ld native_count=%zu native_bytes=%zu\n",
            baseline,ordinal,stats[0].live_calls.load(),stats[0].live.load());
        std::fflush(stderr);
        ::operator delete(leaked); // negative-control cleanup, never converts failure to success
        throw;
    }
}
void reference_failures() {
    auto sites=checked_reference_transaction(-1,true);check(sites>4,"reference cursor/mark work not measured");
    for(long ordinal=0;ordinal<sites;++ordinal)checked_reference_transaction(ordinal,false);
    std::cout<<"reference construction/partial-mark failure sweep: "<<sites<<" sites\n";
}


void busy(auto&& operation) {
    try {operation();}catch(const BufferBusy&) {return;}
    throw std::runtime_error("leased write did not report BufferBusy");
}
void buffer_copy_bytes() {
    Runtime r;auto schema=admit(r,spec("copy-u8",{SchemaScalar{ScalarEncoding::u8}}));
    constexpr std::size_t size=3*1024*1024;
    auto layout=r.buffer_layout(schema,size,16),destination=r.buffer_layout(schema,size+8,32);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);
    std::vector<std::byte> input(size);for(std::size_t i=0;i<size;++i)input[i]=std::byte((i*17+i/37)%256);
    {Runtime::BufferBuilder b(r,layout,size,quota);b.write_bytes(input);b.finish_into(&source.value);}
    auto source_token=r.root_word(&source.value);void* original=source.value;
    auto start=Clock::now();
    {
        Runtime::BufferReadLease first(r,source_token,layout),second(r,source_token,layout);
        check(first.count()==size && second.count()==size,"lease live prefix");
        busy([&]{r.buffer_store_scalar(source_token,layout,0,{},99);});
        busy([&]{r.store(source.value,16,nullptr);});
        Runtime::Handle shared(r,source.value);
        auto writer=std::async(std::launch::async,[&] {
            Runtime::Attachment attached(r);Runtime::Managed managed(r);Output alias(r);alias.value=shared.get();
            busy([&]{r.buffer_store_scalar(r.root_word(&alias.value),layout,0,{},55);});
        });
        {Runtime::Native native(r);writer.get();}
        source.value=nullptr;check(r.collect()==Runtime::Collection::completed,"lease did not root source");
        check(r.contains(original),"source reclaimed during lease");
        stats[3].calls=0;stats[3].bytes=0;recording=3;
        Runtime::BufferBuilder b(r,destination,size,quota);auto copy_start=Clock::now();b.copy_elements(first,0,size);b.finish_into(&out.value);auto copy_end=Clock::now();recording=-1;
        std::cout<<"byte copy allocation_calls="<<stats[3].calls<<" requested_bytes="<<stats[3].bytes
                 <<" copy_finish_ms="<<std::chrono::duration<double,std::milli>(copy_end-copy_start).count()<<'\n';
        check(out.value!=original,"copy reused source storage");
        for(std::size_t i=0;i<size;++i)check(r.buffer_byte(out.value,destination,i,0)==input[i],"large independent byte oracle");
        r.buffer_store_scalar(r.root_word(&out.value),destination,size-1,{},71);
        check(r.buffer_byte(original,layout,size-1,0)==input.back(),"destination mutation changed leased source");
        source.value=original;
    }
    auto end=Clock::now();
    r.buffer_store_scalar(source_token,layout,0,{},42);
    check(r.buffer_byte(out.value,destination,0,0)==input[0],"source mutation changed independent copy");
    check(r.buffer_byte(out.value,destination,size-1,0)==std::byte{71},"destination mutation lost");
    std::cout<<"independent byte copy bytes="<<size<<" elapsed_ms="<<std::chrono::duration<double,std::milli>(end-start).count()
             <<" (includes oracle/GC/conflict checks; no latency claim)\n";
    source.value=out.value=nullptr;r.collect();check(!r.snapshot().objects,"byte copy leaked objects");
    ElementSchemaRef blob;{Runtime::Native native(r);blob=admit(r,spec("blob-copy",{SchemaBytes{size,64}}));}
    auto blob_layout=r.buffer_layout(blob,2,64);
    {Runtime::BufferBuilder b(r,blob_layout,1,quota);b.write_bytes(input);b.finish_into(&source.value);}
    {Runtime::BufferReadLease lease(r,source_token,blob_layout);Runtime::BufferBuilder b(r,blob_layout,1,quota);b.copy_elements(lease,0,1);b.finish_into(&out.value);}
    check(out.value!=source.value && !std::memcmp(static_cast<char*>(out.value)+64,input.data(),size),"multi-megabyte blob event oracle");
    source.value=out.value=nullptr;r.collect();check(!r.snapshot().objects,"blob copy leaked objects");
}
ElementSchemaRef copy_schema(Runtime& r) {
    const auto w=sizeof(void*);
    return admit(r,spec("nested-copy",{SchemaScalar{ScalarEncoding::i32},SchemaManaged{"T",false},SchemaString{},
        SchemaInterface{"I",false,0,w,2*w,alignof(void*)},
        SchemaRecord{5*w,alignof(void*),{{0,0,0},{1,w,1},{2,2*w,2},{3,3*w,3}}},
        SchemaRepeat{w,5*w,3,4,16*w,alignof(void*)}}));
}
void fill_copy_source(Runtime& r,Output& source,Output& object,Output& text,const BufferLayoutRef& layout,const InterfaceWitness* witness) {
    Runtime::BufferBuilder b(r,layout,2,quota);
    for(unsigned element=0;element<2;++element)for(unsigned repeat=0;repeat<3;++repeat) {
        b.write_scalar(100*element+repeat);b.write_reference(r.root_word(&object.value));b.write_reference(r.root_word(&text.value));
        b.write_interface({r.root_word(&object.value),witness});
    }
    b.finish_into(&source.value);
}
void copy_oracle(void* payload,const BufferLayoutRef& layout,void* target,void* text,const InterfaceWitness* witness) {
    // Independent physical offsets, never DataCursor or ReferenceCursor.
    const auto w=sizeof(void*);
    for(unsigned element=0;element<2;++element)for(unsigned repeat=0;repeat<3;++repeat) {
        auto* base=static_cast<const char*>(payload)+layout->base()+element*layout->stride()+w+repeat*5*w;
        std::uint32_t scalar=0;void* managed=nullptr;void* string=nullptr;void* interface=nullptr;const InterfaceWitness* actual=nullptr;
        std::memcpy(&scalar,base,4);std::memcpy(&managed,base+w,w);std::memcpy(&string,base+2*w,w);
        std::memcpy(&interface,base+3*w,w);std::memcpy(&actual,base+4*w,w);
        check(scalar==100*element+repeat && managed==target && string==text && interface==target && actual==witness,"nested copy identity/content oracle");
    }
}
void nested_copy_and_collections() {
    Runtime r;const auto w=sizeof(void*);NominalDescriptor type{"T","","copy",{w,{}}};
    InterfaceWitness wi{"interface:1:I1:T","I","T","read"};r.admit_metadata({&type,1},{&wi,1});auto schema=copy_schema(r);
    auto layout=r.buffer_layout(schema,3,2*w,17*w),dest=r.buffer_layout(schema,4,4*w,18*w);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r),object(r),text(r),other(r);
    allocate_nominal(r,object,"T");allocate_nominal(r,other,"T");r.string_into(&text.value,"shared immutable text");
    auto* witness=r.witness(wi.key);fill_copy_source(r,source,object,text,layout,witness);auto token=r.root_word(&source.value);
    {
        Runtime::BufferReadLease lease(r,token,layout);
        busy([&]{r.store(source.value,layout->element_offset(0)+2*w,other.value);});
        {
            auto** interior=reinterpret_cast<void**>(static_cast<char*>(source.value)+layout->element_offset(0)+2*w);
            constexpr std::size_t offset=0;Runtime::Slot slot{interior,sizeof(void*),&offset,1};Runtime::Frame frame(r,&slot,1);
            busy([&]{r.allocate_into(interior,{w,{}});});
            std::uint64_t prepared=0;busy([&]{r.construct_into(interior,{w,{}},&prepared);});
            busy([&]{r.string_into(interior,"");});busy([&]{r.string_into(interior,"text");});
            Runtime::BufferBuilder attempted(r,dest,1,quota);attempted.copy_elements(lease,0,1);
            busy([&]{attempted.finish_into(interior);});
            check(*interior==object.value,"output alias bypassed lease");
        }
        Runtime::BufferBuilder b(r,dest,2,quota);b.copy_elements(lease,0,1);
        auto before=r.snapshot();
        auto observed=std::async(std::launch::async,[&]{return r.wait_for_request(std::chrono::seconds(5));});
        auto collector=std::async(std::launch::async,[&]{Runtime::Attachment at(r);Runtime::Managed managed(r);return r.collect();});
        check(observed.get() && r.snapshot().requested && r.is_managed(),"copy request did not overlap Managed partial destination");
        b.copy_elements(lease,1,1);b.finish_into(&out.value);
        Runtime::Collection result;{Runtime::Native native(r);result=collector.get();}
        check(result==Runtime::Collection::completed && r.snapshot().cooperative_parks>before.cooperative_parks,"copy missed genuine poll/park");
        copy_oracle(out.value,dest,object.value,text.value,witness);
        // Trusted corruption fixtures deliberately bypass write APIs, then restore
        // the public graph before any explicit collection. Copy must independently
        // reject each bad typed word without publishing its partial destination.
        InterfaceWitness forged{"forged","I","T","read"};
        for(auto [offset,bad_word]:std::array<std::pair<std::size_t,const void*>,4>{{
                {2*w,nullptr},{2*w,text.value},{3*w,object.value},{5*w,&forged}}}) {
            Runtime::BufferBuilder bad(r,dest,1,quota);void* previous=nullptr;
            auto* address=static_cast<char*>(source.value)+layout->element_offset(0)+offset;
            std::memcpy(&previous,address,w);std::memcpy(address,&bad_word,w);
            auto* sentinel=out.value;
            try {reject([&]{bad.copy_elements(lease,0,1);});}
            catch(...) {std::memcpy(address,&previous,w);throw;}
            std::memcpy(address,&previous,w);
            reject([&]{bad.finish_into(&out.value);});check(out.value==sentinel,"invalid copied reference published");
        }
        // Abort a second independent partial copy; its root must unwind.
        {Runtime::BufferBuilder abort(r,dest,2,quota);abort.copy_elements(lease,0,1);}
        check(r.collect()==Runtime::Collection::completed,"aborted copy collection");
    }
    {
        auto** interior=reinterpret_cast<void**>(static_cast<char*>(source.value)+layout->element_offset(0)+2*w);
        constexpr std::size_t zero=0;Runtime::Slot slot{interior,sizeof(void*),&zero,1};Runtime::Frame frame(r,&slot,1);
        reject([&]{r.string_into(interior,"");}); // even unleased outputs cannot bypass typed stores
    }
    r.store(source.value,layout->element_offset(0)+2*w,other.value);
    std::array<SchemaStep,2> path{{{SchemaStep::Kind::element,1},{SchemaStep::Kind::field,0}}};
    r.buffer_store_scalar(token,layout,0,path,777);
    copy_oracle(out.value,dest,object.value,text.value,witness);
    source.value=object.value=text.value=other.value=nullptr;
    check(r.collect()==Runtime::Collection::completed && r.snapshot().objects==3,"copy lost shared managed/string referents or retained abandoned buffers");
    out.value=nullptr;r.collect();check(!r.snapshot().objects,"nested copies leaked graph");
}
void copy_negatives() {
    Runtime r,foreign;auto foreign_schema=admit(foreign,spec("copy-neg-u8",{SchemaScalar{ScalarEncoding::u8}}));
    std::optional<Runtime::RegisteredRootWord> foreign_token;
    {Runtime::Attachment a(foreign);Runtime::Managed m(foreign);Output root(foreign);foreign_token=foreign.root_word(&root.value);}
    auto schema=admit(r,spec("copy-neg-u8",{SchemaScalar{ScalarEncoding::u8}}));
    auto zero=admit(r,spec("copy-zero",{SchemaBytes{0,64}}));auto layout=r.buffer_layout(schema,2),empty=r.buffer_layout(schema,0),foreign_layout=foreign.buffer_layout(foreign_schema,2);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r),alias(r);
    {Runtime::BufferBuilder b(r,layout,1,quota);b.write_scalar(7);b.finish_into(&source.value);}
    auto token=r.root_word(&source.value);
    reject([&]{Runtime::BufferReadLease invalid(r,*foreign_token,layout);});
    reject([&]{Runtime::BufferReadLease invalid(r,token,foreign_layout);});
    void* unregistered=source.value;reject([&]{(void)r.root_word(&unregistered);});
    std::optional<Runtime::RegisteredRootWord> expired;
    {Output temporary(r);temporary.value=source.value;expired=r.root_word(&temporary.value);}
    reject([&]{Runtime::BufferReadLease invalid(r,*expired,layout);});
    {Runtime::BufferReadLease lease(r,token,layout);Runtime::BufferBuilder b(r,layout,1,quota);
        reject([&]{b.copy_elements(lease,1,1);});reject([&]{b.finish_into(&out.value);});}
    {Runtime::BufferReadLease lease(r,token,layout);Runtime::BufferBuilder b(r,layout,1,quota);
        reject([&]{b.copy_elements(lease,0,2);});}
    {capture_size=layout->bytes();Runtime::BufferBuilder b(r,layout,1,quota);capture_size=0;
        alias.value=captured;reject([&]{Runtime::BufferReadLease private_alias(r,r.root_word(&alias.value),layout);});alias.value=nullptr;}
    reject([&]{Runtime::BufferReadLease mismatch(r,token,r.buffer_layout(schema,1));});
    {
        Runtime::BufferReadLease lease(r,token,layout);
        auto other_thread=std::async(std::launch::async,[&]{
            Runtime::Attachment a(r);Runtime::Managed m(r);Runtime::BufferBuilder b(r,layout,1,quota);
            reject([&]{b.copy_elements(lease,0,1);});
        });
        {Runtime::Native native(r);other_thread.get();}
    }
    {Runtime::BufferReadLease lease(r,r.root_word(&alias.value),empty);Runtime::BufferBuilder b(r,empty,0,quota);b.copy_elements(lease,0,0);b.finish_into(&out.value);check(!out.value,"empty copy allocated");}
    auto zlayout=r.buffer_layout(zero,5);
    {Runtime::BufferBuilder b(r,zlayout,3,quota);b.zero_elements(3);b.finish_into(&alias.value);}
    {Runtime::BufferReadLease lease(r,r.root_word(&alias.value),zlayout);Runtime::BufferBuilder b(r,zlayout,3,quota);b.copy_elements(lease,1,2);b.copy_elements(lease,0,1);b.finish_into(&out.value);}
    {Runtime::BufferReadLease lease(r,token,layout);Runtime::BufferBuilder b(r,zlayout,1,quota);reject([&]{b.copy_elements(lease,0,1);});}
    for(auto node:{SchemaNode{SchemaOwnedFixed{}},SchemaNode{SchemaGrowable{}}}) {Runtime::Native native(r);reject([&]{admit(r,spec("unsupported-copy",{node}));});}
    auto readonly=spec("readonly-copy",{SchemaScalar{ScalarEncoding::u8}});readonly.capability_profile="readonly";
    ElementSchemaRef ro_schema;{Runtime::Native native(r);ro_schema=admit(r,std::move(readonly));}
    auto ro=r.buffer_layout(ro_schema,1);
    {Runtime::BufferBuilder b(r,ro,1,quota);b.write_scalar(9);b.finish_into(&alias.value);}
    reject([&]{r.buffer_store_scalar(r.root_word(&alias.value),ro,0,{},3);});
    {Runtime::BufferReadLease lease(r,r.root_word(&alias.value),ro);Runtime::BufferBuilder b(r,ro,1,quota);b.copy_elements(lease,0,1);b.finish_into(&out.value);}
    check(r.buffer_byte(out.value,ro,0,0)==std::byte{9},"readonly-profile copy failed");
    {Runtime::BufferReadLease lease(r,r.root_word(&alias.value),ro);Runtime::BufferBuilder b(r,layout,1,quota);reject([&]{b.copy_elements(lease,0,1);});}
    r.buffer_store_scalar(token,layout,0,{},8); // aborted copies left no lease.
    source.value=out.value=alias.value=nullptr;r.collect();check(!r.snapshot().objects,"copy negatives leaked objects");
}
void copy_compactness() {
    std::size_t fixed_calls=0,fixed_metadata=0;
    for(auto capacity:{std::uint64_t{1},std::uint64_t{131072}}) {
        {
            Runtime r;NominalDescriptor type{"T","","copy-metadata",{sizeof(void*),{}}};r.admit_types({&type,1});
            auto schema=admit(r,spec("copy-nullable",{SchemaManaged{"T",true}}));auto layout=r.buffer_layout(schema,capacity);
            Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);
            {Runtime::BufferBuilder b(r,layout,capacity,quota);b.zero_elements(capacity);b.finish_into(&source.value);}
            check(!stats[2].live && !stats[2].live_calls,"previous copy measurement retained storage");
            stats[2].calls=0;stats[2].bytes=0;recording=2;
            auto start=Clock::now();
            {Runtime::BufferReadLease lease(r,r.root_word(&source.value),layout);Runtime::BufferBuilder b(r,layout,capacity,quota);
                b.copy_elements(lease,0,capacity);b.finish_into(&out.value);}
            auto end=Clock::now();recording=-1;
            auto calls=stats[2].calls.load(),metadata=stats[2].bytes.load()-layout->bytes();
            if(capacity==1) {fixed_calls=calls;fixed_metadata=metadata;}
            else check(calls==fixed_calls && metadata==fixed_metadata,"copy metadata expanded with repetition count");
            auto gc_start=Clock::now();check(r.collect()==Runtime::Collection::completed,"reference copy live collection");auto gc_end=Clock::now();
            std::cout<<"reference copy capacity="<<capacity<<" payload_bytes="<<layout->bytes()<<" allocation_calls="<<calls<<" metadata_requested="<<metadata
                     <<" copy_finish_ms="<<std::chrono::duration<double,std::milli>(end-start).count()
                     <<" two_live_buffers_gc_ms="<<std::chrono::duration<double,std::milli>(gc_end-gc_start).count()<<'\n';
            source.value=out.value=nullptr;r.collect();check(!r.snapshot().objects,"reference copy measurement retained objects");
        }
        check(!stats[2].live && !stats[2].live_calls,"reference copy storage outlived Runtime");
    }
}
long copy_failure_transaction(long ordinal,bool measure,std::size_t budget) {
    long sites=0;
    {
        Runtime r(budget);const auto w=sizeof(void*);NominalDescriptor type{"T","","copy",{w,{}}};
        InterfaceWitness wi{"interface:1:I1:T","I","T","read"};r.admit_metadata({&type,1},{&wi,1});auto schema=copy_schema(r);
        auto layout=r.buffer_layout(schema,3);Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r),object(r),text(r);
        allocate_nominal(r,object,"T");r.string_into(&text.value,"copy failure target");auto* witness=r.witness(wi.key);
        fill_copy_source(r,source,object,text,layout,witness);r.allocate_into(&out.value,{8,{}});auto* sentinel=out.value;
        reference_native_empty();stats[0].calls=0;stats[0].bytes=0;recording=0;fail_after=ordinal;bool failed=false;
        try {
            Runtime::BufferReadLease lease(r,r.root_word(&source.value),layout);Runtime::BufferBuilder b(r,layout,2,quota);
            b.copy_elements(lease,0,1);auto before=r.snapshot();auto result=r.collect();
            if(result==Runtime::Collection::allocation_failure) {
                check(r.snapshot().objects==before.objects && !r.snapshot().requested,"copy marking failure swept or stuck epoch");throw std::bad_alloc();
            }
            check(result==Runtime::Collection::completed,"copy failure witness collection");
            b.copy_elements(lease,1,1);b.finish_into(&out.value);
        } catch(const std::bad_alloc&) {failed=true;}
        recording=-1;fail_after=-1;sites=static_cast<long>(stats[0].calls.load());
        if(!measure)check(failed && out.value==sentinel,"copy failure published or missed ordinal");
        check(r.collect()==Runtime::Collection::completed,"copy rollback retry collection");
        if(failed) {
            check(r.snapshot().objects==4,"failed copy left private objects");
            Runtime::BufferReadLease lease(r,r.root_word(&source.value),layout);Runtime::BufferBuilder retry(r,layout,2,quota);
            retry.copy_elements(lease,0,2);retry.finish_into(&out.value);
        }
        copy_oracle(out.value,layout,object.value,text.value,witness);
        r.store(source.value,layout->element_offset(0)+2*w,object.value); // no leaked lease after success/failure
        source.value=out.value=object.value=text.value=nullptr;r.collect();check(!r.snapshot().objects,"copy sweep leaked GC objects");
    }
    reference_native_empty();return sites;
}
void copy_failures() {
    for(auto budget:{std::size_t{0},std::size_t{1}}) {
        auto sites=copy_failure_transaction(-1,true,budget);check(sites>4,"copy allocation work not measured");
        for(long ordinal=0;ordinal<sites;++ordinal)copy_failure_transaction(ordinal,false,budget);
        std::cout<<"copy/partial-mark allocation failure sweep budget="<<budget<<": "<<sites<<" sites\n";
    }
}
// Stage 2F hosted acceptance: deterministic work, not retries/timing-based
// overlap. Existing CTest suite/workflow picks this up on every native runner.
void foundation_acceptance_stress() {
    Runtime r(1);const auto w=sizeof(void*);NominalDescriptor type{"acceptance-target","","plain",{w,{}}};r.admit_types({&type,1});
    auto schema=admit(r,spec("acceptance-record",{SchemaScalar{ScalarEncoding::u64},SchemaManaged{type.identity,false},
        SchemaRecord{2*w,alignof(void*),{{0,0,0},{1,w,1}}}}));
    auto layout=r.buffer_layout(schema,32);const BufferWorkQuota work{64,1,1};
    Runtime::Attachment a(r);Runtime::Managed m(r);Output object(r),source(r),out(r),sentinel(r);
    allocate_nominal(r,object,type.identity.c_str());
    {Runtime::BufferBuilder b(r,layout,17,work);for(unsigned i=0;i<17;++i) {b.write_scalar(i);b.write_reference(r.root_word(&object.value));}b.finish_into(&source.value);}
    const auto source_token=r.root_word(&source.value);
    const std::array<SchemaStep,1> scalar_path{{{SchemaStep::Kind::field,0}}};
    for(unsigned cycle=0;cycle<32;++cycle) {
        r.allocate_into(&sentinel.value,{w,{}});out.value=sentinel.value;
        {
            Runtime::BufferReadLease lease(r,source_token,layout);
            Runtime::BufferBuilder b(r,layout,17,work);b.copy_elements(lease,0,8);
            check(r.collect()==Runtime::Collection::completed,"acceptance partial-copy GC failed");
            busy([&]{r.store(source.value,layout->element_offset(0)+w,object.value);});
            busy([&]{r.buffer_store_scalar(source_token,layout,0,scalar_path,999);});
            {
                // Its budget-triggered collection sees the outer partial copy.
                Runtime::BufferBuilder aborted(r,layout,17,work);aborted.copy_elements(lease,0,1);
                reject([&]{aborted.copy_elements(lease,17,1);});
                reject([&]{aborted.finish_into(&out.value);});
                check(out.value==sentinel.value,"acceptance failed copy replaced old output");
            }
            b.copy_elements(lease,8,9);b.finish_into(&out.value);
        }
        r.buffer_store_scalar(source_token,layout,0,scalar_path,cycle+1);
        r.store(source.value,layout->element_offset(0)+w,object.value); // no leaked lease
        for(int pass=0;pass<2;++pass) {
            check(r.collect()==Runtime::Collection::completed,"acceptance repeated GC failed");
            for(unsigned i=0;i<17;++i) {
                std::uint64_t bits=0;void* target=nullptr;
                auto* element=static_cast<const char*>(out.value)+i*2*w;
                std::memcpy(&bits,element,sizeof(bits));std::memcpy(&target,element+w,w);
                check(bits==(i ? i : cycle) && target==object.value,"acceptance snapshot/referent changed");
            }
        }
    }
    object.value=source.value=out.value=sentinel.value=nullptr;
    check(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"acceptance stress leaked objects");
    std::cout<<"foundation acceptance: 32 budgeted lease/copy/abort/recovery cycles, two post-copy collections each passed\n";
}
// Stage 3A uses the same buffer representation/transactions, with exact fixed
// extent checks rather than a second object kind or memory owner.
void fixed_values() {
    Runtime r(1);auto byte=admit(r,spec("fixed-u8",{SchemaScalar{ScalarEncoding::u8}}));
    auto layout=r.buffer_layout(byte,4*1024*1024);Runtime::Attachment a(r);Runtime::Managed m(r);
    Output source(r),copy(r);auto token=r.root_word(&source.value),copied=r.root_word(&copy.value);
    r.fixed_default_into(&source.value,layout,quota);
    r.fixed_store_scalar(token,layout,0,19);r.fixed_store_scalar(token,layout,layout->capacity()-1,217);
    r.fixed_copy_into(&copy.value,token,layout,{4096,1,8});
    check(source.value!=copy.value,"fixed copy shared mutable payload");
    for(std::uint64_t i=0;i<layout->capacity();++i)
        check(r.fixed_scalar(copied,layout,i)==(i==0 ? 19 : i==layout->capacity()-1 ? 217 : 0),"large fixed independent content oracle");
    r.fixed_store_scalar(token,layout,0,71);r.fixed_store_scalar(copied,layout,layout->capacity()-1,33);
    check(r.fixed_scalar(copied,layout,0)==19 && r.fixed_scalar(token,layout,layout->capacity()-1)==217,"fixed mutations escaped independent storage");
    auto* old=source.value;r.fixed_copy_into(&source.value,token,layout,quota);
    check(source.value!=old && r.fixed_scalar(token,layout,0)==71,"fixed self-copy lost snapshot");
    {Runtime::BufferReadLease lease(r,token,layout);busy([&]{r.fixed_store_scalar(token,layout,0,1);});}
    reject([&]{r.fixed_scalar(token,layout,layout->capacity());});
    reject([&]{r.fixed_store_scalar(token,layout,std::numeric_limits<std::uint64_t>::max(),1);});
    auto wrong=r.buffer_layout(byte,layout->capacity()-1);
    reject([&]{r.fixed_scalar(token,wrong,0);});reject([&]{r.fixed_store_scalar(token,wrong,0,1);});
    reject([&]{r.fixed_copy_into(&copy.value,token,wrong,quota);});
    auto* sentinel=copy.value;
    void* unregistered=nullptr;reject([&]{r.fixed_default_into(&unregistered,layout,quota);});
    auto fake=r.root_word(&source.value); // Later-unlinked Frame tokens reject too.
    {Output temporary(r);fake=r.root_word(&temporary.value);}
    reject([&]{r.fixed_copy_into(&copy.value,fake,layout,quota);});
    Runtime foreign;ElementSchemaRef other;{Runtime::Native native(r);other=admit(foreign,spec("fixed-u8",{SchemaScalar{ScalarEncoding::u8}}));}
    reject([&]{r.fixed_default_into(&copy.value,foreign.buffer_layout(other,4),quota);});
    check(copy.value==sentinel,"fixed invalid operation changed output");
    for(unsigned encoding=0;encoding<11;++encoding) {
        auto e=static_cast<ScalarEncoding>(encoding);
        ElementSchemaRef schema;{Runtime::Native native(r);schema=admit(r,spec("fixed-scalar-"+std::to_string(encoding),{SchemaScalar{e}}));}auto small=r.buffer_layout(schema,3);
        std::array<std::uint64_t,3> bits{0,1,e==ScalarEncoding::boolean ? 1u : 127u};
        r.fixed_literal_into(&source.value,small,bits,quota);
        for(unsigned i=0;i<3;++i)check(r.fixed_scalar(token,small,i)==bits[i],"scalar bits/width literal oracle");
        r.fixed_copy_into(&copy.value,token,small,quota);
        auto* before=copy.value;
        reject([&]{r.fixed_literal_into(&copy.value,small,std::span(bits).first(2),quota);});
        if(schema->bytes()<8) {
            bits[2]=std::uint64_t{1}<<(schema->bytes()*8);
            reject([&]{r.fixed_literal_into(&copy.value,small,bits,quota);});
            reject([&]{r.fixed_store_scalar(token,small,0,bits[2]);});
        }
        if(e==ScalarEncoding::boolean) {bits[2]=2;reject([&]{r.fixed_literal_into(&copy.value,small,bits,quota);});}
        check(copy.value==before,"bad fixed literal published partial output");
    }
    auto empty=r.buffer_layout(byte,0);r.fixed_default_into(&source.value,empty,quota);
    r.fixed_copy_into(&copy.value,token,empty,quota);check(!source.value && !copy.value,"typed fixed empty allocated object");
    reject([&]{r.fixed_scalar(token,empty,0);});reject([&]{r.fixed_store_scalar(token,empty,0,1);});
    ElementSchemaRef zero;{Runtime::Native native(r);zero=admit(r,spec("fixed-zero",{SchemaBytes{0,64}}));}auto zeros=r.buffer_layout(zero,32768);
    r.fixed_default_into(&source.value,zeros,quota);r.fixed_copy_into(&copy.value,token,zeros,quota);
    check(source.value!=copy.value && zeros->bytes()==2*1024*1024 && zeros->stride()==64,"fixed zero-footprint stride/ownership");
    check(reinterpret_cast<std::uintptr_t>(source.value)%64==0,"fixed over-alignment");
    reject([&]{r.fixed_scalar(token,zeros,0);});
    ElementSchemaRef record;{Runtime::Native native(r);record=admit(r,spec("fixed-nonscalar",{SchemaBytes{8,8}}));}
    reject([&]{r.fixed_default_into(&copy.value,r.buffer_layout(record,1),quota);});
    // Complete buffer with spare capacity is not a complete fixed value.
    auto short_layout=r.buffer_layout(byte,4);
    {Runtime::BufferBuilder b(r,short_layout,2,quota);b.zero_elements(2);b.finish_into(&source.value);}
    reject([&]{r.fixed_copy_into(&copy.value,token,short_layout,quota);});
    reject([&]{r.fixed_scalar(token,short_layout,0);});reject([&]{r.fixed_store_scalar(token,short_layout,0,1);});
    r.buffer_store_scalar(token,short_layout,0,{},1); // rejected lease did not leak
    source.value=copy.value=nullptr;check(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"fixed values leaked GC storage");
    std::cout<<"fixed scalar values: 4 MiB content oracle, independent/self copy, all encodings, typed empty/zero-footprint and negatives passed\n";
}
void fixed_poll_and_roots() {
    Runtime r;auto schema=admit(r,spec("fixed-poll",{SchemaScalar{ScalarEncoding::u64}}));auto layout=r.buffer_layout(schema,8);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);auto token=r.root_word(&source.value);
    r.fixed_default_into(&source.value,layout,quota);auto before=r.snapshot();
    auto observer=std::async(std::launch::async,[&]{return r.wait_for_request(std::chrono::seconds(5));});
    auto collector=std::async(std::launch::async,[&]{Runtime::Attachment a(r);Runtime::Managed m(r);return r.collect();});
    const bool requested=observer.get(); // remain Managed until the production store poll
    r.fixed_store_scalar(token,layout,7,0xfedcba9876543210ULL);
    auto after=r.snapshot();Runtime::Collection collected;
    {Runtime::Native native(r);collected=collector.get();}
    check(requested && collected==Runtime::Collection::completed && after.completed_epoch==before.epoch+1 &&
          after.cooperative_parks==before.cooperative_parks+1 && !after.requested,"fixed store failed deterministic poll/park");
    r.fixed_copy_into(&out.value,token,layout,quota);
    check(r.fixed_scalar(r.root_word(&out.value),layout,7)==0xfedcba9876543210ULL,"fixed scalar bits changed across GC/copy");
    auto* sentinel=out.value;
    reject([&]{r.fixed_default_into(&out.value,layout,{1,1,1});});
    check(out.value==sentinel,"fixed invalid quota changed output");
    source.value=out.value=nullptr;check(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"fixed poll roots leaked");
    std::cout<<"fixed Managed store: genuine request/poll/park, rooted source and post-GC copy passed\n";
}
long fixed_failure_transaction(long ordinal,bool measure,unsigned operation,std::size_t budget) {
    long sites=0;
    {
        Runtime r(budget);auto schema=admit(r,spec("fixed-failure",{SchemaScalar{ScalarEncoding::u32}}));auto layout=r.buffer_layout(schema,3);
        Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);
        std::array<std::uint64_t,3> bits{3,5,8};auto token=r.root_word(&source.value);
        r.fixed_literal_into(&source.value,layout,bits,quota);r.allocate_into(&out.value,{8,{}});auto* sentinel=out.value;
        reference_native_empty();stats[0].calls=0;stats[0].bytes=0;recording=0;fail_after=ordinal;bool failed=false;
        auto run=[&] {
            if(operation==0)r.fixed_default_into(&out.value,layout,quota);
            else if(operation==1)r.fixed_literal_into(&out.value,layout,bits,quota);
            else r.fixed_copy_into(&out.value,token,layout,quota);
        };
        try {run();}catch(const std::bad_alloc&) {failed=true;}
        recording=-1;fail_after=-1;sites=static_cast<long>(stats[0].calls.load());
        if(!measure)check(failed && out.value==sentinel,"fixed failure ordinal missed or published output");
        check(r.collect()==Runtime::Collection::completed && !r.snapshot().requested,"fixed failure left collection epoch");
        if(failed) {check(r.snapshot().objects==2,"fixed failure retained private storage");run();}
        for(unsigned i=0;i<3;++i)check(r.fixed_scalar(r.root_word(&out.value),layout,i)==(operation ? bits[i] : 0),"fixed failure retry content");
        r.fixed_store_scalar(token,layout,0,13); // all copy leases released
        source.value=out.value=nullptr;check(r.collect()==Runtime::Collection::completed && !r.snapshot().objects,"fixed failure GC leak");
    }
    reference_native_empty();return sites; // strict native check after all owners die
}
void fixed_failures() {
    for(auto budget:{std::size_t{0},std::size_t{1}})for(unsigned operation=0;operation<3;++operation) {
        auto sites=fixed_failure_transaction(-1,true,operation,budget);check(sites>=3,"fixed allocation work not measured");
        for(long ordinal=0;ordinal<sites;++ordinal)fixed_failure_transaction(ordinal,false,operation,budget);
        std::cout<<"fixed value allocation sweep operation="<<operation<<" budget="<<budget<<": "<<sites<<" sites\n";
    }
}
void reference_poll_reacquisition(bool invalid_after_poll=false) {
    Runtime r;const auto w=sizeof(void*);std::array<NominalDescriptor,2> types{{{"T","","plain",{w,{}}},{"Other","","other",{w,{}}}}};
    InterfaceWitness witness{"interface:1:I1:T","I","T","read"};r.admit_metadata(types,{&witness,1});
    auto schema=admit(r,spec("poll-pair",{SchemaManaged{"T",false},SchemaInterface{"I",false,0,w,2*w,alignof(void*)},SchemaRecord{3*w,alignof(void*),{{1,0,0},{2,w,1}}}}));
    auto layout=r.buffer_layout(schema,2);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output first(r),second(r),source(r),out(r);
    allocate_nominal(r,first,"T");allocate_nominal(r,second,invalid_after_poll ? "Other" : "T");source.value=out.value=first.value;auto token=r.root_word(&source.value);auto* admitted=r.witness(witness.key);
    Runtime::BufferBuilder b(r,layout,1,quota);b.write_reference(token);const auto before=r.snapshot();
    auto observer=std::async(std::launch::async,[&]{return r.wait_for_request(std::chrono::seconds(5));});
    auto collector=std::async(std::launch::async,[&] {
        Runtime::Attachment attached(r);Runtime::Managed managed(r);
#ifdef VD_BUFFER_WRAP_COLLECT
        redirect_slot=&source.value;redirect_value=second.value;
#endif
        auto result=r.collect();
#ifdef VD_BUFFER_WRAP_COLLECT
        check(!redirect_slot,"collector root-redirection witness not exercised");
#endif
        return result;
    });
    const bool observed=observer.get();const auto pending=r.snapshot();const bool managed=r.is_managed();
    bool rejected=false;
    try {b.write_interface({token,admitted});}catch(const std::invalid_argument&) {rejected=true;}
    const auto after=r.snapshot();
    if(invalid_after_poll)reject([&]{b.finish_into(&out.value);});else b.finish_into(&out.value);
    Runtime::Collection result;{Runtime::Native native(r);result=collector.get();}
    check(observed && managed && pending.requested && pending.completed_epoch==before.completed_epoch,"reference request did not overlap a Managed partial element");
    check(result==Runtime::Collection::completed && !after.requested && after.completed_epoch==pending.epoch && after.cooperative_parks==before.cooperative_parks+1,"reference chunk did not genuinely poll/park");
    if(invalid_after_poll) {
        check(rejected && out.value==first.value && source.value==second.value,"post-poll target change was not rejected transactionally");
        std::cout<<"reference post-poll wrong-type rejection preserved previous output\n";return;
    }
    check(!rejected,"valid post-poll source rejected");
    void* actual=nullptr;const void* actual_witness=nullptr;std::memcpy(&actual,static_cast<char*>(out.value)+w,w);std::memcpy(&actual_witness,static_cast<char*>(out.value)+2*w,w);
#ifdef VD_BUFFER_WRAP_COLLECT
    check(actual==second.value && actual_witness==admitted && source.value==second.value,"reference source was cached across poll or interface pair torn");
    std::cout<<"reference partial-element poll: one collection/park, source slot reacquired at verified Heap boundary\n";
#else
    check(actual==first.value && actual_witness==admitted && source.value==first.value,"reference pair changed across collection");
    std::cout<<"reference partial-element poll: one collection/park; source-redirection instrumentation unavailable on this platform\n";
#endif
}
void fatal_reference(std::string_view mode) {
    Runtime r;const auto w=sizeof(void*);std::array<NominalDescriptor,2> types{{{"T","","plain",{w,{}}},{"Other","","other",{w,{}}}}};
    InterfaceWitness witness{"interface:1:I1:T","I","T","read"};r.admit_metadata(types,{&witness,1});
    const bool pair=mode=="ref-spare-witness" || mode=="ref-forged-witness";
    auto schema=admit(r,spec("fatal-ref",pair ? std::vector<SchemaNode>{SchemaInterface{"I",true,0,w,2*w,alignof(void*)}} : mode=="ref-wrong-string" ? std::vector<SchemaNode>{SchemaString{}} : std::vector<SchemaNode>{SchemaManaged{"T",false}}));
    auto layout=r.buffer_layout(schema,3);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),other(r),out(r);allocate_nominal(r,source,"T");allocate_nominal(r,other,"Other");
    auto* admitted=r.witness(witness.key);
    {Runtime::BufferBuilder b(r,layout,1,quota);
        if(pair)b.write_interface({r.root_word(&source.value),admitted});
        else if(mode=="ref-wrong-string") {Output empty(r);b.write_reference(r.root_word(&empty.value));}
        else b.write_reference(r.root_word(&source.value));
        b.finish_into(&out.value);}
    void* value=nullptr;const void* bad_witness=reinterpret_cast<const void*>(std::uintptr_t{1});
    if(mode=="ref-private-spare") {
        capture_size=layout->bytes();Runtime::BufferBuilder b(r,layout,1,quota);capture_size=0;
        check(captured,"private spare witness absent");value=source.value;std::memcpy(static_cast<char*>(captured)+layout->element_offset(2),&value,w);
        r.collect();throw std::runtime_error("private spare reference survived");
    }
    if(mode=="ref-spare") {value=source.value;std::memcpy(static_cast<char*>(out.value)+layout->element_offset(2),&value,w);}
    else if(mode=="ref-spare-witness")std::memcpy(static_cast<char*>(out.value)+layout->element_offset(2)+w,&admitted,w);
    else if(mode=="ref-required-null")std::memcpy(out.value,&value,w);
    else if(mode=="ref-wrong-target") {value=other.value;std::memcpy(out.value,&value,w);}
    else if(mode=="ref-wrong-string") {value=source.value;std::memcpy(out.value,&value,w);}
    else if(mode=="ref-forged-witness")std::memcpy(static_cast<char*>(out.value)+w,&bad_witness,w);
    else if(mode=="ref-private-edge") {
        auto private_layout=r.buffer_layout(schema,2);capture_size=private_layout->bytes();Runtime::BufferBuilder private_builder(r,private_layout,1,quota);capture_size=0;
        check(captured,"private target witness missing");std::memcpy(out.value,&captured,w);r.collect();throw std::runtime_error("private reference buffer edge survived");
    } else throw std::runtime_error("unknown fatal reference mode");
    r.collect();throw std::runtime_error("corrupt buffer graph survived collection");
}
void fatal(std::string_view mode) {
    std::set_terminate([]{std::_Exit(79);});
    if(mode.starts_with("ref-")) {fatal_reference(mode);return;}
    Runtime r;auto s=admit(r,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));auto layout=r.buffer_layout(s,4096);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);r.allocate_into(&out.value,{sizeof(void*),{0}});
    capture_size=4096;Runtime::BufferBuilder b(r,layout,4096,quota);capture_size=0;check(captured,"fatal fixture capture absent");
    if(mode=="private-root") {void* leaked=captured;constexpr std::size_t zero=0;Runtime::Slot slot{&leaked,sizeof(leaked),&zero,1};Runtime::Frame frame(r,&slot,1);r.collect();}
    else if(mode=="private-edge") {std::memcpy(out.value,&captured,sizeof(captured));r.collect();}
    else throw std::runtime_error("unknown fatal mode");
    throw std::runtime_error("private escape resumed execution");
}
void measure_references() {
    Runtime r;const auto w=sizeof(void*);NominalDescriptor nominal{"T","","plain",{w,{}}};r.admit_types({&nominal,1});
    auto schema=admit(r,spec("reference-measure",{SchemaManaged{"T",true}}));
    std::array<BufferLayoutRef,2> layouts{r.buffer_layout(schema,1),r.buffer_layout(schema,1000000)};
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);auto token=r.root_word(&source.value);
    r.allocate_into(&out.value,{w,{}});out.value=nullptr;r.collect();
    for(const auto& layout:layouts)for(bool duplicate:{false,true})for(int sample=0;sample<3;++sample) {
        if(duplicate)allocate_nominal(r,source,"T");
        check(!stats[2].live && !stats[3].live,"reference measurement retained work");
        stats[2].calls=0;stats[2].bytes=0;stats[3].calls=0;stats[3].bytes=0;
        recording=2;auto start=Clock::now();
        {Runtime::BufferBuilder b(r,layout,layout->capacity(),quota);
            if(duplicate)for(std::uint64_t i=0;i<layout->capacity();++i)b.write_reference(token);
            else b.zero_elements(layout->capacity());
            b.finish_into(&out.value);}
        auto built=Clock::now();recording=-1;source.value=nullptr;
        recording=3;auto gc_start=Clock::now();auto result=r.collect();auto gc_end=Clock::now();recording=-1;
        check(result==Runtime::Collection::completed && r.snapshot().objects==(duplicate ? 2 : 1),"reference measurement reachability");
        auto live_calls=stats[3].calls.load(),live_bytes=stats[3].bytes.load();
        auto dead_start=Clock::now();out.value=nullptr;r.collect();auto dead_end=Clock::now();
        check(!stats[2].live && !stats[3].live,"reference measurement leaked storage");
        auto us=[](auto start,auto end){return std::chrono::duration<double,std::micro>(end-start).count();};
        std::cout<<"reference_measure capacity="<<layout->capacity()<<" duplicate="<<duplicate<<" sample="<<sample<<" payload="<<layout->bytes()
                 <<" build_calls="<<stats[2].calls<<" build_requested="<<stats[2].bytes<<" build_us="<<us(start,built)
                 <<" live_gc_calls="<<live_calls<<" live_gc_requested="<<live_bytes<<" live_gc_us="<<us(gc_start,gc_end)
                 <<" dead_gc_us="<<us(dead_start,dead_end)<<"\n";
    }
}
void measure() {
    Runtime r;auto s=admit(r,spec("byte",{SchemaScalar{ScalarEncoding::u8}}));
    std::array<BufferLayoutRef,2> layouts{r.buffer_layout(s,1024*1024),r.buffer_layout(s,32*1024*1024)};
    std::vector<std::byte> input(layouts[1]->bytes(),std::byte{0x61});
    Runtime::Attachment a(r);Runtime::Managed m(r);Output out(r);
    r.allocate_into(&out.value,{8,{}});out.value=nullptr;r.collect(); // Warm buckets outside measurement.
    std::cout<<"native embedding sizes: Frame="<<sizeof(Runtime::Frame)<<" Slot="<<sizeof(Runtime::Slot)<<" BufferBuilder="<<sizeof(Runtime::BufferBuilder)<<'\n';
    auto seconds=[](auto a,auto b){return std::chrono::duration<double>(b-a).count();};
    for(const auto& layout:layouts) for(int run=0;run<3;++run) {
        check(!stats[1].live,"measurement objects retained");stats[1].calls=0;stats[1].bytes=0;recording=1;
        auto start=Clock::now();
        {Runtime::BufferBuilder b(r,layout,layout->capacity(),quota);auto allocated=Clock::now();
            b.write_bytes(std::span<const std::byte>(input).first(layout->bytes()));auto populated=Clock::now();b.finish_into(&out.value);auto published=Clock::now();
            recording=-1;
            check(r.buffer_byte(out.value,layout,0,0)==std::byte{0x61} && r.buffer_byte(out.value,layout,layout->capacity()-1,0)==std::byte{0x61},"measurement witness");
            auto mib=static_cast<double>(layout->bytes())/(1024*1024);
            std::cout<<"buffer_measure run="<<run<<" bytes="<<layout->bytes()<<" allocation_calls="<<stats[1].calls<<" requested_bytes="<<stats[1].bytes
                     <<" allocate_zero_MiB_s="<<(mib/seconds(start,allocated))<<" populate_MiB_s="<<(mib/seconds(allocated,populated))<<" finish_us="<<(1e6*seconds(populated,published));}
        auto live_start=Clock::now();r.collect();auto live_end=Clock::now();
        out.value=nullptr;r.collect();auto dead_end=Clock::now();
        check(!stats[1].live,"measurement reservation/metadata retained after collection");
        std::cout<<" live_collect_us="<<1e6*seconds(live_start,live_end)<<" dead_collect_us="<<1e6*seconds(live_end,dead_end)<<'\n';
    }
    recording=-1;
}
}
int main(int argc,char** argv) {
    try {
        if(argc>1) {if(std::string_view(argv[1])=="--measure")measure();else if(std::string_view(argv[1])=="--measure-reference")measure_references();else if(std::string_view(argv[1])=="reference-leak-bytes")checked_reference_transaction(-1,true,37);else if(std::string_view(argv[1])=="reference-leak-zero")checked_reference_transaction(-1,true,0);else fatal(argv[1]);}
        else {layouts();byte_oracles();scalar_and_deep_oracles();empty_aligned_and_negatives();failures();native_and_chunk_collections();reference_buffers();object_table_lifetime();reference_failures();buffer_copy_bytes();check(!stats[3].live && !stats[3].live_calls,"byte copy native allocations outlived Runtime");nested_copy_and_collections();copy_negatives();copy_compactness();copy_failures();foundation_acceptance_stress();fixed_values();fixed_poll_and_roots();fixed_failures();reference_poll_reacquisition();
#ifdef VD_BUFFER_WRAP_COLLECT
            reference_poll_reacquisition(true);
#endif
            std::cout<<"private rooted byte/reference buffer checks passed\n";}
    }catch(const std::exception& e){recording=-1;fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}
}
