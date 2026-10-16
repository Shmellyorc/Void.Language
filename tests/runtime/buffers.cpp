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
long reference_transaction(long ordinal,bool measure) {
    Runtime r;auto w=sizeof(void*);NominalDescriptor type{"Target","","plain",{w,{}}};r.admit_types({&type,1});
    auto schema=admit(r,spec("two-required",{SchemaManaged{"Target",false},SchemaRepeat{0,w,2,0,2*w,alignof(void*)}}));auto layout=r.buffer_layout(schema,2);
    Runtime::Attachment a(r);Runtime::Managed m(r);Output source(r),out(r);allocate_nominal(r,source,"Target");r.allocate_into(&out.value,{8,{}});auto* sentinel=out.value;auto token=r.root_word(&source.value);
    check(!stats[0].live,"prior reference sweep retained measured storage");stats[0].calls=0;stats[0].bytes=0;recording=0;fail_after=ordinal;bool failed=false,released_source=false;
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
    check(!stats[0].live && !stats[0].live_calls,"reference failure sweep leaked native storage");
    return sites;
}
void reference_failures() {
    auto transaction=[](long ordinal,bool baseline) {
        try {return reference_transaction(ordinal,baseline);}
        catch(...) {
            // All transaction owners (including Runtime's containers) have now
            // unwound. Keep the original failure, but distinguish owner-retained
            // capacity from allocations that actually outlive those owners.
            std::fprintf(stderr,"reference sweep after owner teardown: baseline=%d ordinal=%ld native_count=%zu native_bytes=%zu\n",
                baseline,ordinal,stats[0].live_calls.load(),stats[0].live.load());
            std::fflush(stderr);
            throw;
        }
    };
    auto sites=transaction(-1,true);check(sites>4,"reference cursor/mark work not measured");
    for(long ordinal=0;ordinal<sites;++ordinal)transaction(ordinal,false);
    std::cout<<"reference construction/partial-mark failure sweep: "<<sites<<" sites\n";
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
        if(argc>1) {if(std::string_view(argv[1])=="--measure")measure();else if(std::string_view(argv[1])=="--measure-reference")measure_references();else fatal(argv[1]);}
        else {layouts();byte_oracles();scalar_and_deep_oracles();empty_aligned_and_negatives();failures();native_and_chunk_collections();reference_buffers();reference_failures();reference_poll_reacquisition();
#ifdef VD_BUFFER_WRAP_COLLECT
            reference_poll_reacquisition(true);
#endif
            std::cout<<"private rooted byte/reference buffer checks passed\n";}
    }catch(const std::exception& e){recording=-1;fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}
}
