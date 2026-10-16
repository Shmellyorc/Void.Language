#include "void/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <new>
#include <stdexcept>
// Same allocation family and injection pattern as runtime/failures.cpp.
namespace { thread_local long fail_after=-1; thread_local bool count_new=false; thread_local long allocation_count=0; thread_local std::size_t allocation_bytes=0; }
void* operator new(std::size_t size) {
    if(count_new) {++allocation_count;allocation_bytes+=size;}
    if(fail_after==0) throw std::bad_alloc();
    if(fail_after>0) --fail_after;
    if(void* p=std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace vd;
constexpr auto P=sizeof(void*), A=alignof(void*);
void check(bool b,const char* m) {if(!b) throw std::runtime_error(m);}
template<class F> void reject(F f) {try{f();} catch(const std::logic_error&){return;}throw std::runtime_error("forged schema/input accepted");}
ElementSchemaSpec spec(std::string name,std::vector<SchemaNode> nodes) {
    ElementSchemaSpec s;s.identity=std::move(name);s.capability_profile="mutable";s.lowering_layout_key="native-cpp-fixture-v1";
    s.storage_domain=StorageDomain::native();s.root=nodes.size()-1;s.nodes=std::move(nodes);return s;
}
NominalDescriptor nominal(std::string name) {return {std::move(name),"","native-pointer-free",{P,{}}};}
ElementSchemaSpec references(std::string name,std::uint64_t n) {
    return spec(std::move(name),{SchemaManaged{"owner",false},SchemaRepeat{0,P,n,0,static_cast<std::size_t>(n)*P,A}});
}
void admit(Runtime& r,const ElementSchemaSpec& s) {r.admit_buffer_schemas({&s,1});}
void oracles_and_compactness() {
    Runtime r;auto owner=nominal("owner");r.admit_types({&owner,1});
    // Independent explicit layout oracle: records have two refs, repeated twice,
    // then repeated three times with leading padding. No cursor-derived oracle.
    auto nested=spec("nested",{SchemaManaged{"owner",true},SchemaString{},
        SchemaRecord{3*P,A,{{10,2*P,1},{20,0,0}}},
        SchemaRepeat{P,3*P,2,2,7*P,A},SchemaRepeat{P,7*P,3,3,22*P,A}});
    admit(r,nested);auto schema=r.buffer_schema("nested");auto shape=r.reference_shape(schema);
    std::vector<std::size_t> expected;
    for(std::size_t outer=0;outer<3;++outer) for(std::size_t inner=0;inner<2;++inner)
        for(auto field:{std::size_t{0},2*P}) expected.push_back(P+outer*7*P+P+inner*3*P+field);
    ReferenceCursor cursor(shape);std::vector<std::size_t> actual;
    while(auto e=cursor.next()) actual.push_back(e->offset);
    check(actual==expected,"nested cursor differs from independent oracle");
    for(std::size_t byte=0;byte<22*P+1;++byte) {
        bool present=std::find(expected.begin(),expected.end(),byte)!=expected.end();
        check(shape.at(byte).has_value()==present,"byte lookup disagrees with oracle");
    }
    std::array<SchemaStep,3> path{{{SchemaStep::Kind::element,2},{SchemaStep::Kind::element,1},{SchemaStep::Kind::field,10}}};
    check(shape.path(path)->offset==P+2*7*P+P+3*P+2*P,"logical path wrong");
    path[0].value=3;check(!shape.path(path),"out-of-range path accepted");path[0].value=0;path[2].value=123;check(!shape.path(path),"unknown logical field accepted");
    auto million=references("million",1000000);admit(r,million);auto large=r.buffer_schema("million");
    check(large->node_count()==2 && large->edge_count()==1 && large->depth()==2,"metadata expanded with multiplicity");
    cursor.reset(r.reference_shape(large));std::size_t count=0;
    while(auto e=cursor.next()) {check(e->offset==count*P && e->kind==ReferenceEvent::Kind::managed,"million stream geometry");++count;}
    check(count==1000000,"million stream incomplete");
    // Measure actual descriptor allocations, not just public node counters. Each
    // run has its own identically seeded catalog; only repetition count differs.
    auto metadata_bytes=[&](std::uint64_t extent) {
        Runtime measured;measured.admit_types({&owner,1});auto input=references("measured",extent);
        allocation_bytes=0;count_new=true;admit(measured,input);count_new=false;return allocation_bytes;
    };
    auto small_bytes=metadata_bytes(1), million_bytes=metadata_bytes(1000000);
    check(million_bytes<2*small_bytes,"descriptor allocation grew with repeated element count");
    cursor.reset(r.reference_shape(large));allocation_count=0;count_new=true;
    while(cursor.next()) {} // No per-reference work allocation, including first event.
    count_new=false;check(allocation_count==0,"streaming expanded/allocated reference metadata");
    std::cout<<"schema admission requested bytes: one="<<small_bytes<<", million="<<million_bytes<<"; cursor next allocations=0\n";
    auto bytes=spec("bytes",{SchemaScalar{ScalarEncoding::u8},SchemaRepeat{0,1,1000000000,0,1000000000,1}});
    admit(r,bytes);cursor.reset(r.reference_shape(r.buffer_schema("bytes")));check(!cursor.next(),"nonreference buffer scanned");
    auto empty=references("empty",0);admit(r,empty);cursor.reset(r.reference_shape(r.buffer_schema("empty")));check(!cursor.next(),"empty repeat emitted refs");
    auto zero=spec("zero-sized",{SchemaBytes{0,1},SchemaRepeat{0,1,1000000,0,1000000,1}});admit(r,zero);
    // Empty fields inside another child's span must not shadow its references.
    auto empty_field=spec("empty-field",{SchemaString{},SchemaRepeat{0,P,2,0,2*P,A},SchemaBytes{0,1},SchemaRecord{2*P,A,{{1,0,1},{2,1,2}}}});
    admit(r,empty_field);check(r.reference_shape(r.buffer_schema("empty-field")).at(P).has_value(),"empty field shadowed reference lookup");
    auto duplicate=spec("deduplicated",{SchemaString{},SchemaString{},SchemaRecord{2*P,A,{{1,0,0},{2,P,1}}}});
    admit(r,duplicate);check(r.buffer_schema("deduplicated")->node_count()==2,"identical child nodes not shared");
    auto alternative=spec("nested",{SchemaString{},SchemaManaged{"owner",true},
        SchemaRecord{3*P,A,{{10,2*P,0},{20,0,1}}},SchemaRepeat{P,3*P,2,2,7*P,A},SchemaRepeat{P,7*P,3,3,22*P,A}});
    admit(r,alternative);check(r.buffer_schema("nested")==schema,"equivalent topology did not retain canonical admission");
    for(auto encoding:{ScalarEncoding::i8,ScalarEncoding::i16,ScalarEncoding::i32,ScalarEncoding::i64,ScalarEncoding::u8,ScalarEncoding::u16,ScalarEncoding::u32,ScalarEncoding::u64,ScalarEncoding::f32,ScalarEncoding::f64,ScalarEncoding::boolean}) {
        auto s=spec("scalar"+std::to_string(static_cast<int>(encoding)),{SchemaScalar{encoding}});admit(r,s);
        auto value=r.buffer_schema(s.identity);check(value->bytes()==s.storage_domain.scalars[static_cast<std::size_t>(encoding)].bytes && !value->has_references(),"scalar domain mismatch");
    }
    check(r.snapshot().objects==0,"schema admission allocated a GC object");
}
void negatives_and_ownership() {
    Runtime r;auto owner=nominal("owner");r.admit_types({&owner,1});auto base=references("reference",3);admit(r,base);auto retained=r.buffer_schema(base.identity);
    auto negative=[&](auto mutate) {auto bad=base;mutate(bad);reject([&]{admit(r,bad);});check(r.buffer_schema(base.identity)==retained,"failed admission changed existing schema");};
    negative([](auto& s){s.root=99;});negative([](auto& s){s.nodes.clear();});
    negative([](auto& s){s.identity.clear();});negative([](auto& s){s.capability_profile=std::string("x\0y",3);});
    negative([](auto& s){s.lowering_layout_key.clear();});negative([](auto& s){s.storage_domain.pointer_bytes*=2;});
    negative([](auto& s){s.storage_domain.pointer_alignment*=2;});negative([](auto& s){s.storage_domain.revision++;});
    negative([](auto& s){s.storage_domain.byte_order=static_cast<StorageDomain::ByteOrder>(55);});
    negative([](auto& s){s.storage_domain.scalars[0].alignment*=2;});
    negative([](auto& s){std::get<SchemaManaged>(s.nodes[0]).target="absent";});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).child=1;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).stride=P-1;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).stride=0;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).base=1;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).alignment=3;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).bytes--;});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).extent=std::numeric_limits<std::uint64_t>::max();});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).extent=std::numeric_limits<std::int64_t>::max();});
    negative([](auto& s){std::get<SchemaRepeat>(s.nodes[1]).base=std::numeric_limits<std::size_t>::max()-(A-1);});
    negative([](auto& s){s.nodes[0]=SchemaScalar{static_cast<ScalarEncoding>(999)};});
    negative([](auto& s){s.nodes[0]=SchemaOwnedFixed{};});negative([](auto& s){s.nodes[0]=SchemaGrowable{};});
    // Same bytes but different semantics are incompatible, not size-based aliases.
    negative([](auto& s){std::get<SchemaManaged>(s.nodes[0]).nullable=true;});
    negative([](auto& s){s.capability_profile="readonly";});negative([](auto& s){s.lowering_layout_key="another-layout";});
    auto broken=spec("broken",{SchemaString{},SchemaRecord{2*P,A,{{1,0,0},{2,0,0}}}});reject([&]{admit(r,broken);});
    std::get<SchemaRecord>(broken.nodes[1]).fields={{1,0,0},{1,P,0}};reject([&]{admit(r,broken);});
    std::get<SchemaRecord>(broken.nodes[1]).fields={{1,2*P,0}};reject([&]{admit(r,broken);});
    auto unused=base;unused.nodes.emplace_back(SchemaBytes{1,1});reject([&]{admit(r,unused);});
    auto align_overflow=spec("align-overflow",{SchemaBytes{1,1},SchemaRepeat{std::numeric_limits<std::size_t>::max()-1,1,1,0,0,2}});reject([&]{admit(r,align_overflow);});
    auto empty_bad=references("empty-bad",0);std::get<SchemaRepeat>(empty_bad.nodes[1]).base=P;reject([&]{admit(r,empty_bad);});
    Runtime other;other.admit_types({&owner,1});admit(other,base);
    reject([&]{other.reference_shape(retained);});reject([&]{r.reference_shape({});});
    std::array<ElementSchemaSpec,2> duplicates{base,base};reject([&]{r.admit_buffer_schemas(duplicates);});
    {Runtime::Attachment a(r);Runtime::Managed m(r);reject([&]{admit(r,base);});}
    r.close();auto fresh=base;fresh.identity="closed";reject([&]{admit(r,fresh);});
}
void legacy_consumers() {
    Heap heap;std::vector<std::size_t> offsets{P+1,1};TypeLayout layout{2*P+2,offsets};
    auto shape=ReferenceShape::legacy(layout.size,offsets);check(shape.legacy_offsets()==std::vector<std::size_t>({1,P+1}),"legacy normalization");
    Root root(heap,heap.allocate(layout));void* child=heap.allocate({P,{}});
    for(std::size_t byte=0;byte<layout.size;++byte) {
        bool allowed=byte==1 || byte==P+1;check(shape.at(byte).has_value()==allowed,"legacy offset oracle");
        if(allowed) heap.store(root.get(),byte,child);else reject([&]{heap.store(root.get(),byte,child);});
    }
    heap.collect();check(heap.size()==2,"legacy unaligned tracing lost referent");
    for(auto bad: {std::vector<std::size_t>{1,1},std::vector<std::size_t>{1,2},std::vector<std::size_t>{layout.size},std::vector<std::size_t>{std::numeric_limits<std::size_t>::max()}})
        reject([&]{ReferenceShape::legacy(layout.size,bad);});
    Runtime r;Runtime::Attachment a(r);Runtime::Managed m(r);void* output=nullptr;constexpr std::size_t zero=0;
    Runtime::Slot slot{&output,sizeof(output),&zero,1};Runtime::Frame frame(r,&slot,1);
    r.allocate_into(&output,{P,{}});void* old=output;std::vector<std::byte> payload(layout.size);
    // Keep prepared child root precise across construct_into's poll.
    void* target=output;Runtime::Slot target_slot{&target,sizeof(target),&zero,1};Runtime::Frame target_frame(r,&target_slot,1);
    for(auto off:offsets) std::memcpy(payload.data()+off,&target,P);
    r.construct_into(&output,layout,payload.data());r.collect();check(r.contains(target),"prepared adapter lost child");
    auto malformed=layout;malformed.references={1,2};reject([&]{r.construct_into(&output,malformed,payload.data());});check(output!=old,"construction fixture did not publish");
    void* sentinel=output;int foreign=0;void* invalid=&foreign;std::memcpy(payload.data()+1,&invalid,P);
    reject([&]{r.construct_into(&output,layout,payload.data());});check(output==sentinel,"bad prepared payload changed output");
    reject([&]{r.construct_into(&output,{0,{}},payload.data());});check(output==sentinel,"zero layout changed output");
}
void flat_fast_path_regressions() {
    // Public cursors own temporary adapters; copying/moving a cursor must retain
    // that ownership. Mixed offset/event calls advance exactly one common stream.
    ReferenceCursor detached(ReferenceShape::legacy(3*P+1,std::array<std::size_t,3>{2*P+1,1,P+1}));
    auto copy=detached;auto moved=std::move(copy);
    check(moved.next_offset()==1 && moved.next()->offset==P+1 && moved.next_offset()==2*P+1 && !moved.next(),"flat cursor ownership/position");
    check(!copy.next(),"moved-from flat cursor retained a borrowed owner");
    check(detached.next()->offset==1,"copied flat cursor changed original position");
    auto shape=ReferenceShape::legacy(3*P+1,std::array<std::size_t,3>{2*P+1,1,P+1});
    for(std::uint64_t i=0;i<3;++i) {
        SchemaStep step{SchemaStep::Kind::field,i};
        check(shape.path({&step,1})->offset==1+i*P,"legacy field path changed");
    }
    Runtime r;auto scalar=spec("offset-events",{SchemaString{},SchemaRepeat{0,P,3,0,3*P,A}});admit(r,scalar);
    ReferenceCursor typed(r.reference_shape(r.buffer_schema("offset-events")));
    check(typed.next_offset()==0 && typed.next()->offset==P && typed.next_offset()==2*P && !typed.next_offset(),"typed offset/event stream diverged");
    Heap heap;TypeLayout input{3*P+1,{2*P+1,1}};
    Root root(heap,heap.allocate(input));auto* child=heap.allocate({P,{}});
    // Caller mutation, reallocations and an alternating shape cannot change an
    // existing object's immutable map. There is no cache key or borrowed input.
    input.references={P+1};heap.allocate(input);heap.store(root.get(),1,child);
    reject([&]{heap.store(root.get(),P+1,child);});
    heap.collect();check(heap.size()==2,"flat map borrowed/misidentified caller offsets");
    // The fast allocation transaction has fewer sites, but must still release
    // payload/map nodes and preserve the old graph at every actual failing site.
    TypeLayout fresh{3*P+1,{2*P+1,1}};
    allocation_count=0;count_new=true;auto* measured=heap.allocate(fresh);count_new=false;
    auto sites=allocation_count;check(sites>=3,"allocation-site fixture missed payload/map/offsets");
    heap.collect();check(!heap.contains(measured),"unrooted measured object retained");
    for(long ordinal=0;ordinal<sites;++ordinal) {
        fail_after=ordinal;bool failed=false;
        try {heap.allocate(fresh);} catch(const std::bad_alloc&) {failed=true;}
        fail_after=-1;check(failed && heap.size()==2,"fast allocation failed to roll back");
        heap.collect();check(heap.size()==2 && heap.contains(child),"failed fast allocation damaged roots");
    }
}
void lifetime_and_depth() {
    ElementSchemaRef retained;std::weak_ptr<const NominalDescriptor> nominal_owner;
    {
        Runtime r;auto owner=nominal("owner");r.admit_types({&owner,1});nominal_owner=r.descriptor("owner");
        auto s=references("retained",2);admit(r,s);retained=r.buffer_schema(s.identity);
        s.nodes.clear();s.identity.clear();
    }
    check(!nominal_owner.expired(),"schema lost retained runtime-owned nominal metadata");
    {ReferenceCursor cursor{ReferenceShape(retained)};check(cursor.next()->offset==0 && cursor.next()->offset==P && !cursor.next(),"retired metadata not usable");}
    retained.reset();check(nominal_owner.expired(),"schema metadata cycle retained runtime");
    Runtime r;auto owner=nominal("owner");r.admit_types({&owner,1});
    auto deep=spec("deep",{SchemaManaged{"owner",false}});
    for(std::size_t n=0;n<2048;++n) deep.nodes.emplace_back(SchemaRepeat{0,P,1,n,P,A});deep.root=deep.nodes.size()-1;
    admit(r,deep);auto shape=r.reference_shape(r.buffer_schema("deep"));ReferenceCursor cursor(shape);
    check(cursor.next()->offset==0 && !cursor.next() && shape.at(0).has_value(),"deep iterative traversal");
    std::vector<SchemaStep> steps(2048,{SchemaStep::Kind::element,0});check(shape.path(steps).has_value(),"deep path lookup");
    admit(r,deep); // Iterative structural comparison too.
    auto shallow=ReferenceShape::legacy(P,std::array<std::size_t,1>{0});
    ReferenceCursor retained_cursor(shallow);fail_after=0;bool failed=false;
    try {retained_cursor.reset(shape);} catch(const std::bad_alloc&) {failed=true;}
    fail_after=-1;check(failed,"cursor reserve failure not exercised");
    check(retained_cursor.next()->offset==0 && !retained_cursor.next(),"failed reset damaged old cursor state");
}
void catalog_failures() {
    auto fresh=nominal("fresh");InterfaceWitness witness{"interface:1:I5:fresh","I","fresh","contract"};
    auto schema=spec("transaction",{SchemaManaged{"fresh",false},SchemaInterface{"I",false,0,P,2*P,A},SchemaRecord{3*P,A,{{1,0,0},{2,P,1}}}});
    long sites=0;
    { Runtime r;auto seed=nominal("seed");r.admit_types({&seed,1});count_new=true;allocation_count=0;
      r.admit_metadata({&fresh,1},{&witness,1},{&schema,1});count_new=false;sites=allocation_count; }
    check(sites>10,"allocation sweep did not measure admission work");
    for(long ordinal=0;ordinal<sites;++ordinal) {
        Runtime r;auto seed=nominal("seed");r.admit_types({&seed,1});auto original=r.descriptor("seed");
        fail_after=ordinal;bool failed=false;
        try {r.admit_metadata({&fresh,1},{&witness,1},{&schema,1});} catch(const std::bad_alloc&){failed=true;}
        fail_after=-1;check(failed,"measured allocation failure did not fail transaction");
        check(r.descriptor("seed")==original,"failed transaction changed old metadata");
        reject([&]{r.descriptor("fresh");});reject([&]{r.buffer_schema("transaction");});
        {Runtime::Attachment a(r);Runtime::Managed m(r);check(!r.witness(witness.key),"failed transaction published witness");}
        r.admit_metadata({&fresh,1},{&witness,1},{&schema,1});
        auto shape=r.reference_shape(r.buffer_schema("transaction"));ReferenceCursor cursor(shape);
        auto first=cursor.next(), second=cursor.next();check(first->offset==0 && second->offset==P && second->witness_offset==2*P && !cursor.next(),"transaction retry/interface cursor");
        check(!shape.at(2*P),"witness word traced as managed object");
    }
    Runtime incompatible;auto owner=nominal("owner");incompatible.admit_types({&owner,1});
    auto existing=references("transaction",1);admit(incompatible,existing);
    auto incompatible_shape=schema; // Valid geometry, incompatible existing identity.
    reject([&]{incompatible.admit_metadata({&fresh,1},{&witness,1},{&incompatible_shape,1});});
    reject([&]{incompatible.descriptor("fresh");});
    check(incompatible.buffer_schema("transaction")->bytes()==P,"incompatible transaction altered schema");
    {Runtime::Attachment a(incompatible);Runtime::Managed m(incompatible);check(!incompatible.witness(witness.key),"incompatible transaction published witness");}
    Runtime r;auto broken=schema;std::get<SchemaManaged>(broken.nodes[0]).target="missing";
    reject([&]{r.admit_metadata({&fresh,1},{&witness,1},{&broken,1});});reject([&]{r.descriptor("fresh");});
    auto bad_interface=schema;std::get<SchemaInterface>(bad_interface.nodes[1]).target="missing";
    reject([&]{r.admit_metadata({&fresh,1},{&witness,1},{&bad_interface,1});});
    std::get<SchemaInterface>(bad_interface.nodes[1]).witness_offset=0;reject([&]{r.admit_metadata({&fresh,1},{&witness,1},{&bad_interface,1});});
    std::cout<<"schema admission allocation-site rollback/retry: "<<sites<<" sites\n";
}
void concurrent_admission() {
    Runtime r;std::latch start(2);
    auto worker=[&](int id) {start.count_down();start.wait();for(int n=0;n<12;++n) admit(r,spec("parallel"+std::to_string(id)+":"+std::to_string(n),{SchemaString{}}));};
    auto a=std::async(std::launch::async,worker,0), b=std::async(std::launch::async,worker,1);a.get();b.get();
    for(int id=0;id<2;++id) for(int n=0;n<12;++n) check(r.buffer_schema("parallel"+std::to_string(id)+":"+std::to_string(n))->has_references(),"concurrent catalog update lost");
}
}
int main() {
    try {oracles_and_compactness();negatives_and_ownership();legacy_consumers();flat_fast_path_regressions();lifetime_and_depth();catalog_failures();concurrent_admission();std::cout<<"compact GC schemas/shared cursor checks passed\n";}
    catch(const std::exception& e) {fail_after=-1;count_new=false;std::cerr<<e.what()<<'\n';return 1;}
}
