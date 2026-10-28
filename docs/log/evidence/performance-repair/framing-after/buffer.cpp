#include "performance_metrics.hpp"
#include "void/runtime.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
namespace vd {
namespace {
std::size_t sum(std::size_t a,std::size_t b) {
    if(b>std::numeric_limits<std::size_t>::max()-a) throw std::length_error("buffer offset overflow");
    return a+b;
}
std::size_t product(std::size_t a,std::uint64_t n) {
    if(a && n>std::numeric_limits<std::size_t>::max()/a) throw std::length_error("buffer extent overflow");
    return a*static_cast<std::size_t>(n);
}
std::size_t round_up(std::size_t n,std::size_t alignment) {return sum(n,alignment-1)&~(alignment-1);}
}
BufferLayout::BufferLayout(ElementSchemaRef schema,std::uint64_t capacity,std::size_t base,std::size_t stride)
    : schema_(std::move(schema)),capacity_(capacity),base_(base),stride_(stride),bytes_(0) {
    auto alignment=schema_->alignment();
    if(capacity>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) throw std::length_error("buffer capacity exceeds i64");
    if(!stride_) stride_=round_up(std::max(std::size_t{1},schema_->bytes()),alignment);
    if(!stride_ || stride_<schema_->bytes() || stride_%alignment || base%alignment) throw std::invalid_argument("invalid buffer stride/base");
    if(!capacity && base) throw std::invalid_argument("empty buffer requires zero base");
    if(capacity) bytes_=round_up(sum(base,product(stride_,capacity)),alignment);
    if(bytes_>static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) throw std::length_error("buffer allocation/address extent not representable");
}
void DataCursor::prepare(ElementSchemaRef schema) {
    if(!schema) throw std::invalid_argument("missing data schema");
    atomic_=1;
    for(const auto& node:schema->spec_.nodes) {
        if(auto* scalar=std::get_if<SchemaScalar>(&node)) {
            auto bytes=schema->spec_.storage_domain.scalars[static_cast<std::size_t>(scalar->encoding)].bytes;
            if((bytes!=1 && bytes!=2 && bytes!=4 && bytes!=8) || ((scalar->encoding==ScalarEncoding::f32 || scalar->encoding==ScalarEncoding::f64) &&
                          (!std::numeric_limits<float>::is_iec559 || !std::numeric_limits<double>::is_iec559)))
                throw std::invalid_argument("unsupported native scalar data encoding");
            atomic_=std::max(atomic_,bytes);
        } else if(std::holds_alternative<SchemaManaged>(node) || std::holds_alternative<SchemaString>(node) || std::holds_alternative<SchemaOwnedFixed>(node) || std::holds_alternative<SchemaGrowable>(node)) atomic_=std::max(atomic_,sizeof(void*));
        else if(std::holds_alternative<SchemaInterface>(node)) atomic_=std::max(atomic_,2*sizeof(void*));
        else if(!std::holds_alternative<SchemaBytes>(node) && !std::holds_alternative<SchemaRecord>(node) && !std::holds_alternative<SchemaRepeat>(node))
            throw std::invalid_argument("unsupported buffer construction role");
    }
    stack_.reserve(schema->depth());schema_=std::move(schema);reset();
}
bool DataCursor::has_data() const {const auto& s=schema_->summaries_[schema_->spec_.root];return s.data || s.references;}
bool DataCursor::dense_u8() const {
    auto* scalar=std::get_if<SchemaScalar>(&schema_->spec_.nodes[schema_->spec_.root]);
    return scalar && scalar->encoding==ScalarEncoding::u8;
}
void DataCursor::reset() noexcept {stack_.clear();if(has_data()) stack_.push_back({schema_->spec_.root,0,0});}
std::optional<DataCursor::Event> DataCursor::next(std::size_t& work) {
    while(work && !stack_.empty()) {
        --work;auto& frame=stack_.back();const auto& node=schema_->spec_.nodes[frame.node];
        if(auto ref=schema_->reference_event(frame.node,frame.base)) {
            Event e{frame.base,schema_->summaries_[frame.node].bytes,false};e.reference=*ref;stack_.pop_back();return e;
        }
        if(auto* scalar=std::get_if<SchemaScalar>(&node)) {
            Event e{frame.base,schema_->summaries_[frame.node].bytes,true,scalar->encoding};stack_.pop_back();return e;
        }
        if(auto* bytes=std::get_if<SchemaBytes>(&node)) {Event e{frame.base,bytes->bytes,false};stack_.pop_back();return e;}
        if(auto* record=std::get_if<SchemaRecord>(&node)) {
            const auto& order=schema_->summaries_[frame.node].physical;
            if(frame.next==order.size()) {stack_.pop_back();continue;}
            const auto& field=record->fields[order[static_cast<std::size_t>(frame.next++)]];
            auto base=sum(frame.base,field.offset);
            if(schema_->summaries_[field.child].data || schema_->summaries_[field.child].references) stack_.push_back({field.child,base,0});
        } else {
            const auto& repeat=std::get<SchemaRepeat>(node);
            if(frame.next==repeat.extent) {stack_.pop_back();continue;}
            const auto& child=schema_->spec_.nodes[repeat.child];
            bool bytes=std::holds_alternative<SchemaBytes>(child);
            auto* scalar=std::get_if<SchemaScalar>(&child);
            if(!frame.next && repeat.stride==schema_->summaries_[repeat.child].bytes && (bytes || (scalar && scalar->encoding==ScalarEncoding::u8))) {
                Event e{sum(frame.base,repeat.base),product(repeat.stride,repeat.extent),false,ScalarEncoding::u8,scalar!=nullptr};stack_.pop_back();return e;
            }
            auto base=sum(frame.base,sum(repeat.base,product(repeat.stride,frame.next++)));
            stack_.push_back({repeat.child,base,0});
        }
    }
    return {};
}
BufferLayoutRef Runtime::buffer_layout(ElementSchemaRef schema,std::uint64_t capacity,std::size_t base,std::size_t stride) const {
    reference_shape(schema); // Exact catalog pointer membership, not just equal keys.
    return BufferLayoutRef(new BufferLayout(std::move(schema),capacity,base,stride));
}
void Runtime::validate_buffer_layout(const BufferLayoutRef& layout) const {
    if(!layout || !types_) throw std::invalid_argument("missing buffer layout");
    auto found=types_->schemas.find(layout->schema()->identity());
    if(found==types_->schemas.end() || found->second!=layout->schema()) throw std::invalid_argument("foreign buffer layout");
}
std::byte Runtime::buffer_byte(void* object,const BufferLayoutRef& layout,std::uint64_t index,std::size_t offset) const {
    require_managed();std::lock_guard lock(mutex_);validate_buffer_layout(layout);
    const auto& b=heap_.buffer_state(object);
    if(!b.complete || !b.layout->equivalent(*layout) || index>=b.valid_prefix || offset>=layout->schema()->bytes()) throw std::invalid_argument("invalid complete buffer inspection");
    return *(static_cast<const std::byte*>(object)+sum(layout->element_offset(index),offset));
}
Runtime::PrivateRoot::PrivateRoot(Runtime& r) :runtime_(r),slot_{&value_,sizeof(value_),&zero_,1},frame_(r,&slot_,1),previous_(r.thread().private_roots) {r.thread().private_roots=this;}
Runtime::PrivateRoot::~PrivateRoot() noexcept {
    runtime_.require_managed();auto& t=runtime_.thread();if(t.private_roots!=this)std::terminate();
    value_=nullptr;t.private_roots=previous_;
}
Runtime::Frame* Runtime::require_buffer_output(void** output) const {
    auto* frame=require_output(output);
    for(auto* root=thread().private_roots;root;root=root->previous_) if(output==&root->value_) throw std::invalid_argument("publication into private builder root");
    return frame;
}
Runtime::BufferBuilder::BufferBuilder(Runtime& r,BufferLayoutRef layout,std::uint64_t live,BufferWorkQuota quota)
    :runtime_(r),layout_(std::move(layout)),quota_(quota),root_(r),owner_(std::this_thread::get_id()),requested_(live) {
    std::shared_ptr<const RuntimeMetadata> catalog;
    {std::lock_guard lock(r.mutex_);r.validate_buffer_layout(layout_);catalog=r.types_;}
    if(live>layout_->capacity() || !quota.bytes || !quota.node_events || !quota.reference_events) throw std::invalid_argument("invalid buffer live extent/work quota");
    bool due;{std::lock_guard lock(r.mutex_);due=layout_->capacity() && r.allocation_budget_ && r.allocations_>=r.allocation_budget_;}
    if(due && r.collect()!=Collection::completed) throw std::bad_alloc();
    Heap::PendingBuffer pending;std::shared_ptr<const Heap::BufferState> state;
    {
        Native native(r);cursor_.prepare(layout_->schema());
        if(quota.bytes<cursor_.atomic_bytes()) throw std::invalid_argument("byte quota cannot hold atomic scalar");
        if(layout_->capacity()) {state=std::make_shared<const Heap::BufferState>(Heap::BufferState{layout_,&root_,0,false,std::move(catalog),live});pending=Heap::PendingBuffer(layout_);}
    }
    if(layout_->capacity()) {
        std::lock_guard lock(r.mutex_);
        // No poll/fallible step after successful adoption until the registered
        // Frame word contains the object. Counter saturation cannot fail here.
        root_.value_=r.heap_.adopt_buffer(pending,std::move(state));
        if(r.allocations_!=std::numeric_limits<std::size_t>::max())++r.allocations_;
    }
}
void Runtime::BufferBuilder::require_active() {
    runtime_.require_managed();
    if(owner_!=std::this_thread::get_id() || failed_ || finished_) throw std::logic_error("inactive or foreign-thread buffer builder");
}
void Runtime::BufferBuilder::validate_private() const {
    runtime_.validate_buffer_layout(layout_);
    if(layout_->capacity()) {
        const auto& b=runtime_.heap_.buffer_state(root_.value_);
        if(b.readers)throw BufferBusy("private destination has active read leases");
        if(b.layout!=layout_ || b.complete || b.owner!=&root_) throw std::logic_error("invalid private builder ownership");
    } else if(root_.value_) throw std::logic_error("nonempty typed empty builder");
}
char* Runtime::BufferBuilder::address(std::size_t offset,std::size_t bytes) const {
    validate_private();
    if(prefix_>=requested_ || offset>layout_->schema()->bytes() || bytes>layout_->schema()->bytes()-offset) throw std::out_of_range("buffer write outside live element");
    auto physical=sum(layout_->element_offset(prefix_),offset);
    if(physical>layout_->bytes() || bytes>layout_->bytes()-physical) throw std::out_of_range("buffer write outside allocation");
    return static_cast<char*>(root_.value_)+physical;
}
void Runtime::BufferBuilder::normalize() {
    if(event_ || prefix_==requested_)return;
    if(!cursor_.has_data()) {prefix_=requested_;in_element_=false;return;}
    while(!event_ && prefix_<requested_) {
        auto work=quota_.node_events;event_=cursor_.next(work);
        if(cursor_.done() && !event_) {++prefix_;in_element_=false;if(prefix_<requested_)cursor_.reset();}
        if(!event_ && prefix_<requested_)runtime_.poll();
    }
}
void Runtime::BufferBuilder::write_scalar(std::uint64_t bits) {
    try {
        require_active();normalize();
        if(!event_ || event_->reference || (!event_->scalar && !event_->u8_run) || (event_->scalar && consumed_)) throw std::invalid_argument("expected an ordered scalar event");
        auto width=event_->u8_run ? 1 : event_->bytes;
        if((width<8 && (bits>>(width*8))) || (event_->encoding==ScalarEncoding::boolean && bits>1)) throw std::invalid_argument("scalar bits exceed encoding");
        runtime_.poll();
        {std::lock_guard lock(runtime_.mutex_);auto* out=address(sum(event_->offset,consumed_),width);
            switch(width) {
                case 1: {auto v=static_cast<std::uint8_t>(bits);perf::copy(out,&v,1);break;}
                case 2: {auto v=static_cast<std::uint16_t>(bits);perf::copy(out,&v,2);break;}
                case 4: {auto v=static_cast<std::uint32_t>(bits);perf::copy(out,&v,4);break;}
                case 8: perf::copy(out,&bits,8);break;
                default: std::terminate();
            }
            in_element_=true;consumed_+=width;if(consumed_==event_->bytes) {consumed_=0;event_.reset();}}
        normalize();
    } catch(...) {failed_=true;throw;}
}
void Runtime::BufferBuilder::write_bytes(std::span<const std::byte> input) {
    try {
        require_active();
        if(prefix_==requested_)throw std::out_of_range("byte write beyond requested prefix");
        if(cursor_.dense_u8() && layout_->stride()==1 && !in_element_) {
            if(input.size()>requested_-prefix_)throw std::out_of_range("byte stream beyond requested prefix");
            while(!input.empty()) {
                auto count=std::min(input.size(),quota_.bytes);runtime_.poll();
                {std::lock_guard lock(runtime_.mutex_);validate_private();auto offset=layout_->element_offset(prefix_);
                    perf::copy(static_cast<char*>(root_.value_)+offset,input.data(),count);prefix_+=count;}
                input=input.subspan(count);
            }
            event_.reset();consumed_=0;cursor_.reset();return;
        }
        while(!input.empty()) {
            normalize();
            if(!event_ || event_->reference || (event_->scalar && event_->encoding!=ScalarEncoding::u8))throw std::invalid_argument("expected byte/u8 construction event");
            auto count=std::min({input.size(),quota_.bytes,event_->bytes-consumed_});runtime_.poll();
            {std::lock_guard lock(runtime_.mutex_);auto* out=address(sum(event_->offset,consumed_),count);
                perf::copy(out,input.data(),count);in_element_=true;consumed_+=count;
                if(consumed_==event_->bytes) {consumed_=0;event_.reset();}}
            input=input.subspan(count);normalize();
        }
    } catch(...) {failed_=true;throw;}
}
Runtime::RegisteredRootWord Runtime::root_word(void** slot) const { perf::Timer perf_timer(perf::root_word); 
    require_managed();std::lock_guard lock(mutex_);
    return RegisteredRootWord(this,require_buffer_output(slot),slot);
}
void* Runtime::reference_source(const RegisteredRootWord& token) const { perf::Timer perf_timer(perf::reference_source); 
    require_managed();
    if(token.runtime_!=this || token.owner_!=std::this_thread::get_id()) throw std::invalid_argument("foreign reference root token");
    bool linked=false;for(auto* f=thread().frames;f;f=f->previous_) if(f==token.frame_) {linked=true;break;}
    if(!linked) throw std::invalid_argument("expired reference root token");
    require_buffer_output(token.slot_);
    bool exact=false;
    for(std::size_t i=0;i<token.frame_->count_ && !exact;++i)for(std::size_t j=0;j<token.frame_->slots_[i].count && !exact;++j)
        if(static_cast<const char*>(token.frame_->slots_[i].storage)+token.frame_->slots_[i].offsets[j]==reinterpret_cast<const char*>(token.slot_))exact=true;
    if(!exact)throw std::invalid_argument("reference source registration changed");
    void* value=nullptr;perf::copy(&value,token.slot_,sizeof(value));
    if(value && !heap_.contains(value)) throw std::invalid_argument("untracked or private reference source");
    return value;
}
void Runtime::BufferBuilder::write_reference(RegisteredRootWord source) {write_root(source,nullptr,false);}
void Runtime::BufferBuilder::write_interface(const RootedInterfaceSource& source) {write_root(source.object,source.witness,true);}
void Runtime::BufferBuilder::write_root(const RegisteredRootWord& source,const InterfaceWitness* witness,bool interface) {
    try {
        require_active();
        {std::lock_guard lock(runtime_.mutex_);(void)runtime_.reference_source(source);
            if(witness) {
                bool admitted=false;
                for(const auto& [key,w]:runtime_.types_->witnesses) { (void)key;if(w.get()==witness) {admitted=true;break;} }
                if(!admitted)throw std::invalid_argument("unadmitted interface source witness");
            }}
        normalize();
        if(!event_ || !event_->reference || consumed_ || (event_->reference->kind==ReferenceEvent::Kind::interface)!=interface)
            throw std::invalid_argument("incorrect ordered reference event");
        {std::lock_guard lock(runtime_.mutex_);
            runtime_.heap_.validate_reference(*event_->reference,runtime_.reference_source(source),witness,*runtime_.types_,false);}
        runtime_.poll();
        std::shared_ptr<const RuntimeMetadata> previous_catalog;
        {std::lock_guard lock(runtime_.mutex_);
            void* value=runtime_.reference_source(source);
            runtime_.heap_.validate_reference(*event_->reference,value,witness,*runtime_.types_,false);
            auto* object_word=address(event_->reference->offset,sizeof(value));
            char* witness_word=nullptr;
            if(interface) witness_word=address(*event_->reference->witness_offset,sizeof(witness));
            const auto& state=runtime_.heap_.buffer_state(root_.value_);
            // Catalog ownership and both words commit without allocation or poll.
            if(state.catalog!=runtime_.types_) {
                previous_catalog=std::move(state.catalog);state.catalog=runtime_.types_;
            }
            perf::copy(object_word,&value,sizeof(value));
            if(interface)perf::copy(witness_word,&witness,sizeof(witness));
            in_element_=true;event_.reset();}
        normalize();
    } catch(...) {failed_=true;throw;}
}
void Runtime::BufferBuilder::zero_elements(std::uint64_t count) {
    try {
        require_active();if(in_element_ || count>requested_-prefix_ || (count && !layout_->schema()->zero_data_complete()))throw std::invalid_argument("zero data must append whole elements");
        runtime_.poll();{std::lock_guard lock(runtime_.mutex_);validate_private();prefix_+=count;}
        event_.reset();consumed_=0;cursor_.reset();
    } catch(...) {failed_=true;throw;}
}
void Runtime::BufferBuilder::finish_into(void** output) {
    try {
        require_active();runtime_.require_buffer_output(output);
        {std::lock_guard lock(runtime_.mutex_);runtime_.heap_.validate_output_address(output);}
        normalize();
        if(prefix_!=requested_ || in_element_ || event_)throw std::logic_error("incomplete buffer construction");
        runtime_.poll();
        std::lock_guard lock(runtime_.mutex_);runtime_.require_buffer_output(output);validate_private();runtime_.heap_.validate_output_address(output);
        // All fallible checks precede this commit. No payload scan/copy, allocation,
        // safepoint or callback can occur between sealing and output publication.
        if(layout_->capacity())runtime_.heap_.seal_buffer(root_.value_,&root_,prefix_);
        *output=root_.value_;root_.value_=nullptr;finished_=true;
    } catch(...) {failed_=true;throw;}
}
Runtime::BufferReadLease::BufferReadLease(Runtime& r,RegisteredRootWord source,const BufferLayoutRef& layout)
    :runtime_(r),layout_(layout),owner_(std::this_thread::get_id()),frame_(r,&slot_,1) {
    r.require_managed();std::lock_guard lock(r.mutex_);r.validate_buffer_layout(layout_);
    auto* value=r.reference_source(source);
    if(!layout_->capacity()) {
        if(value)throw std::invalid_argument("typed empty source must be null");
        return;
    }
    const auto& state=r.heap_.buffer_state(value);
    if(!state.complete || !state.layout->equivalent(*layout_))throw std::invalid_argument("lease requires an exact complete buffer");
    if(state.readers==std::numeric_limits<std::size_t>::max())throw std::overflow_error("buffer read lease overflow");
    // All checks precede the infallible count/root commit. No poll or allocation.
    count_=state.valid_prefix;++state.readers;value_=value;
}
void Runtime::BufferReadLease::validate() const {
    runtime_.require_managed();
    if(owner_!=std::this_thread::get_id())throw std::invalid_argument("foreign-thread buffer lease");
    runtime_.validate_buffer_layout(layout_);
    if(!layout_->capacity()) {if(value_ || count_)throw std::logic_error("invalid empty lease");return;}
    const auto& state=runtime_.heap_.buffer_state(value_);
    if(!state.complete || !state.readers || state.valid_prefix!=count_ || !state.layout->equivalent(*layout_))
        throw std::logic_error("invalid buffer read lease");
}
Runtime::BufferReadLease::~BufferReadLease() noexcept {
    std::lock_guard lock(runtime_.mutex_);validate();
    if(value_)--runtime_.heap_.buffer_state(value_).readers;
    value_=nullptr; // Frame unlinks next, without a poll.
}
void Runtime::buffer_store_scalar(RegisteredRootWord owner,const BufferLayoutRef& layout,std::uint64_t index,
                                 std::span<const SchemaStep> path,std::uint64_t bits) {
    store_scalar(owner,layout,index,path,bits,false);
}
void Runtime::store_scalar(RegisteredRootWord owner,const BufferLayoutRef& layout,std::uint64_t index,
                          std::span<const SchemaStep> path,std::uint64_t bits,bool fixed) {
    require_managed();
    // Resolve one admitted logical path, without walking/expanding repetition.
    std::size_t node=0,offset=0,width=0;ScalarEncoding encoding;
    {
        std::lock_guard lock(mutex_);validate_buffer_layout(layout);
        if(fixed)validate_fixed_layout(layout);
        if(!layout->schema()->mutable_data())throw std::invalid_argument("buffer profile does not permit mutation");
        const auto& schema=*layout->schema();node=schema.spec_.root;
        for(const auto& step:path) {
            const auto& shape=schema.spec_.nodes[node];
            if(step.kind==SchemaStep::Kind::field) {
                auto* record=std::get_if<SchemaRecord>(&shape);
                if(!record)throw std::invalid_argument("scalar path requires record");
                auto field=std::find_if(record->fields.begin(),record->fields.end(),[&](const auto& f){return f.id==step.value;});
                if(field==record->fields.end())throw std::invalid_argument("unknown scalar field");
                offset=sum(offset,field->offset);node=field->child;
            } else {
                if(step.kind!=SchemaStep::Kind::element)throw std::invalid_argument("unknown scalar path step");
                auto* repeat=std::get_if<SchemaRepeat>(&shape);
                if(!repeat || step.value>=repeat->extent)throw std::out_of_range("scalar repeat path");
                offset=sum(offset,sum(repeat->base,product(repeat->stride,step.value)));node=repeat->child;
            }
        }
        auto* scalar=std::get_if<SchemaScalar>(&schema.spec_.nodes[node]);
        if(!scalar)throw std::invalid_argument("scalar store requires scalar leaf");
        encoding=scalar->encoding;width=schema.summaries_[node].bytes;
        if((width!=1 && width!=2 && width!=4 && width!=8) || (width<8 && bits>>(width*8)) || (encoding==ScalarEncoding::boolean && bits>1))
            throw std::invalid_argument("invalid scalar store bits/width");
    }
    auto destination=[&]() {
        auto* object=reference_source(owner);const auto& state=heap_.buffer_state(object);
        if(!state.complete || !state.layout->equivalent(*layout) || index>=state.valid_prefix)throw std::invalid_argument("invalid scalar buffer destination");
        if(fixed && state.valid_prefix!=layout->capacity())throw std::invalid_argument("incomplete fixed extent");
        if(state.readers)throw BufferBusy("buffer storage has active read leases");
        return static_cast<char*>(object)+sum(layout->element_offset(index),offset);
    };
    {std::lock_guard lock(mutex_);(void)destination();}
    poll();
    std::lock_guard lock(mutex_);auto* out=destination();
    switch(width) {
        case 1:{auto v=static_cast<std::uint8_t>(bits);perf::copy(out,&v,1);break;}
        case 2:{auto v=static_cast<std::uint16_t>(bits);perf::copy(out,&v,2);break;}
        case 4:{auto v=static_cast<std::uint32_t>(bits);perf::copy(out,&v,4);break;}
        case 8:perf::copy(out,&bits,8);break;
        default:std::terminate();
    }
}
void Runtime::validate_fixed_layout(const BufferLayoutRef& layout) const {
    validate_buffer_layout(layout);
    const auto& schema=*layout->schema();
    if(!std::holds_alternative<SchemaScalar>(schema.spec_.nodes[schema.spec_.root]) &&
       (schema.bytes() || schema.has_references() || !schema.zero_data_complete()))
        throw std::invalid_argument("fixed runtime proof requires scalar or zero-footprint element");
}
ElementSchemaRef Runtime::fixed_scalar_schema(ScalarEncoding encoding) {
    require_managed();
    if(static_cast<unsigned>(encoding)>static_cast<unsigned>(ScalarEncoding::boolean))
        throw std::invalid_argument("invalid fixed scalar encoding");
    const auto key="void.fixed.scalar.v1/"+std::to_string(static_cast<unsigned>(encoding));
    ElementSchemaRef schema;
    {std::lock_guard lock(mutex_);if(types_) {auto i=types_->schemas.find(key);if(i!=types_->schemas.end()) schema=i->second;}}
    if(!schema) {
        Native native(*this);
        ElementSchemaSpec spec{key,"mutable",key,StorageDomain::native(),0,{SchemaScalar{encoding}}};
        admit_buffer_schemas({&spec,1});schema=buffer_schema(key);
    }
    return schema;
}
BufferLayoutRef Runtime::fixed_scalar_layout(ScalarEncoding encoding,std::uint64_t extent) {
    auto layout=buffer_layout(fixed_scalar_schema(encoding),extent);
    validate_fixed_scalar_layout(layout,encoding,extent);
    return layout;
}
BufferLayoutRef Runtime::fixed_scalar_layout(RegisteredRootWord source,ScalarEncoding encoding,std::uint64_t extent) {
    auto schema=fixed_scalar_schema(encoding);
    BufferLayoutRef layout;
    {
        std::lock_guard lock(mutex_);
        auto* object=reference_source(source);
        if(object) {
            const auto& state=heap_.buffer_state(object);
            // Reuse the immutable layout already owned by the buffer. Construct
            // the expected geometry on the stack; retain exact schema, packing
            // and complete fixed-extent checks without allocating metadata.
            BufferLayout expected(schema,extent,0,0);
            if(!state.complete || state.valid_prefix!=extent || !state.layout->equivalent(expected))
                throw std::invalid_argument("invalid fixed scalar source");
            layout=state.layout;
        }
    }
    // Empty arrays have no heap object from which to recover a layout.
    if(!layout) layout=buffer_layout(schema,extent);
    validate_fixed_scalar_layout(layout,encoding,extent);
    return layout;
}
void Runtime::validate_fixed_scalar_layout(const BufferLayoutRef& layout,ScalarEncoding encoding,std::uint64_t extent) const {
    require_managed();std::lock_guard lock(mutex_);validate_fixed_layout(layout);
    const auto& schema=*layout->schema();
    auto* scalar=std::get_if<SchemaScalar>(&schema.spec_.nodes[schema.spec_.root]);
    if(!scalar || scalar->encoding!=encoding || layout->capacity()!=extent)
        throw std::invalid_argument("fixed scalar signature/layout mismatch");
}
void Runtime::validate_fixed_output(void** output,const BufferLayoutRef& layout) const {
    require_buffer_output(output);
    std::lock_guard lock(mutex_);validate_fixed_layout(layout);heap_.validate_output_address(output);
}
void Runtime::fixed_default_into(void** output,const BufferLayoutRef& layout,BufferWorkQuota quota) {
    validate_fixed_output(output,layout);
    BufferBuilder builder(*this,layout,layout->capacity(),quota);
    builder.zero_elements(layout->capacity());
    builder.finish_into(output);
}
void Runtime::fixed_literal_into(void** output,const BufferLayoutRef& layout,
                                std::span<const std::uint64_t> bits,BufferWorkQuota quota) {
    validate_fixed_output(output,layout);
    const auto& schema=*layout->schema();
    if(!std::holds_alternative<SchemaScalar>(schema.spec_.nodes[schema.spec_.root]))
        throw std::invalid_argument("scalar literal requires scalar element");
    if(bits.size()!=layout->capacity())throw std::invalid_argument("fixed literal extent mismatch");
    // Native input has the same stable, non-managed span contract as write_bytes.
    // No payload staging: ordered values feed the existing rooted transaction.
    BufferBuilder builder(*this,layout,layout->capacity(),quota);
    for(auto value:bits)builder.write_scalar(value);
    builder.finish_into(output);
}
void Runtime::fixed_copy_into(void** output,RegisteredRootWord source,const BufferLayoutRef& layout,BufferWorkQuota quota) {
    validate_fixed_output(output,layout);
    BufferReadLease lease(*this,source,layout);
    if(lease.count()!=layout->capacity())throw std::invalid_argument("fixed source live extent mismatch");
    BufferBuilder builder(*this,layout,layout->capacity(),quota);
    builder.copy_elements(lease,0,layout->capacity());
    builder.finish_into(output);
}
std::uint64_t Runtime::fixed_scalar(RegisteredRootWord source,const BufferLayoutRef& layout,std::uint64_t index) const {
    require_managed();std::lock_guard lock(mutex_);validate_fixed_layout(layout);
    const auto& schema=*layout->schema();
    if(!std::holds_alternative<SchemaScalar>(schema.spec_.nodes[schema.spec_.root]))
        throw std::invalid_argument("scalar read requires scalar element");
    if(index>=layout->capacity())throw std::out_of_range("fixed index outside extent");
    auto* object=reference_source(source);const auto& state=heap_.buffer_state(object);
    if(!state.complete || !state.layout->equivalent(*layout) || state.valid_prefix!=layout->capacity())
        throw std::invalid_argument("invalid fixed scalar source");
    auto* address=static_cast<const char*>(object)+layout->element_offset(index);
    switch(schema.bytes()) {
        case 1:{std::uint8_t v;perf::copy(&v,address,1);return v;}
        case 2:{std::uint16_t v;perf::copy(&v,address,2);return v;}
        case 4:{std::uint32_t v;perf::copy(&v,address,4);return v;}
        case 8:{std::uint64_t v;perf::copy(&v,address,8);return v;}
        default:throw std::invalid_argument("unsupported scalar width");
    }
}
void Runtime::fixed_store_scalar(RegisteredRootWord owner,const BufferLayoutRef& layout,std::uint64_t index,std::uint64_t bits) {
    store_scalar(owner,layout,index,{},bits,true);
}
void Runtime::BufferBuilder::copy_elements(BufferReadLease& source,std::uint64_t first,std::uint64_t count) { perf::Timer perf_timer(perf::buffer_copy); 
    try {
        require_active();
        if(&source.runtime_!=&runtime_)throw std::invalid_argument("foreign buffer lease");
        auto validate=[&]() {
            source.validate();validate_private();
            if(source.layout_->schema()!=layout_->schema())throw std::invalid_argument("copy requires exact admitted schema/profile");
            if(source.value_ && source.value_==root_.value_)throw std::invalid_argument("copy storage aliases");
        };
        {std::lock_guard lock(runtime_.mutex_);validate();}
        if(in_element_ || consumed_ || first>source.count_ || count>source.count_-first || count>requested_-prefix_)
            throw std::out_of_range("copy must append complete live elements");
        const auto begin=prefix_,end=prefix_+count;
        if(!count)return;
        if(!cursor_.has_data()) {
            runtime_.poll();std::lock_guard lock(runtime_.mutex_);validate();prefix_=end;return;
        }
        while(prefix_<end) {
            if(cursor_.dense_u8() && layout_->stride()==1 && source.layout_->stride()==1 && !in_element_) {
                auto n=static_cast<std::size_t>(std::min<std::uint64_t>(end-prefix_,quota_.bytes));
                runtime_.poll();
                {std::lock_guard lock(runtime_.mutex_);validate();
                    auto* from=static_cast<const char*>(source.value_)+source.layout_->element_offset(first+prefix_-begin);
                    auto* to=static_cast<char*>(root_.value_)+layout_->element_offset(prefix_);
                    perf::copy(to,from,n);prefix_+=n;}
                continue;
            }
            normalize();if(prefix_==end)break;
            if(!event_)throw std::logic_error("copy data cursor incomplete");
            runtime_.poll();
            std::shared_ptr<const RuntimeMetadata> previous_catalog;
            {
                std::lock_guard lock(runtime_.mutex_);validate();
                auto source_element=first+prefix_-begin;
                auto* from=static_cast<const char*>(source.value_)+sum(source.layout_->element_offset(source_element),sum(event_->offset,consumed_));
                if(event_->reference) {
                    const auto& ref=*event_->reference;void* value=nullptr;const InterfaceWitness* witness=nullptr;
                    // Interface offsets need not begin at the event's base.
                    auto* base=static_cast<const char*>(source.value_)+source.layout_->element_offset(source_element);
                    perf::copy(&value,base+ref.offset,sizeof(value));
                    if(ref.witness_offset)perf::copy(&witness,base+*ref.witness_offset,sizeof(witness));
                    runtime_.heap_.validate_reference(ref,value,witness,*runtime_.types_,false);
                    auto* to=address(ref.offset,sizeof(value));char* witness_to=nullptr;
                    if(ref.witness_offset)witness_to=address(*ref.witness_offset,sizeof(witness));
                    const auto& state=runtime_.heap_.buffer_state(root_.value_);
                    if(state.catalog!=runtime_.types_) {previous_catalog=std::move(state.catalog);state.catalog=runtime_.types_;}
                    perf::copy(to,&value,sizeof(value));if(witness_to)perf::copy(witness_to,&witness,sizeof(witness));
                    in_element_=true;event_.reset();
                } else {
                    auto n=event_->scalar ? event_->bytes : std::min(event_->bytes-consumed_,quota_.bytes);
                    auto* to=address(sum(event_->offset,consumed_),n);perf::copy(to,from,n);
                    in_element_=true;consumed_+=n;
                    if(consumed_==event_->bytes) {consumed_=0;event_.reset();}
                }
            }
            normalize();
        }
        if(cursor_.dense_u8() && layout_->stride()==1 && source.layout_->stride()==1) {event_.reset();consumed_=0;cursor_.reset();}
    } catch(...) {failed_=true;throw;}
}

}
