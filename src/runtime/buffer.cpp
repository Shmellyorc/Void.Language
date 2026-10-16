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
std::size_t BufferLayout::element_offset(std::uint64_t index) const {
    if(index>=capacity_) throw std::out_of_range("buffer element outside capacity");
    return sum(base_,product(stride_,index));
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
        } else if(!std::holds_alternative<SchemaBytes>(node) && !std::holds_alternative<SchemaRecord>(node) && !std::holds_alternative<SchemaRepeat>(node))
            throw std::invalid_argument("reference-bearing construction awaits Stage 2D");
    }
    stack_.reserve(schema->depth());schema_=std::move(schema);reset();
}
bool DataCursor::has_data() const {return schema_->summaries_[schema_->spec_.root].data;}
bool DataCursor::dense_u8() const {
    auto* scalar=std::get_if<SchemaScalar>(&schema_->spec_.nodes[schema_->spec_.root]);
    return scalar && scalar->encoding==ScalarEncoding::u8;
}
void DataCursor::reset() noexcept {stack_.clear();if(has_data()) stack_.push_back({schema_->spec_.root,0,0});}
std::optional<DataCursor::Event> DataCursor::next(std::size_t& work) {
    while(work && !stack_.empty()) {
        --work;auto& frame=stack_.back();const auto& node=schema_->spec_.nodes[frame.node];
        if(auto* scalar=std::get_if<SchemaScalar>(&node)) {
            Event e{frame.base,schema_->summaries_[frame.node].bytes,true,scalar->encoding};stack_.pop_back();return e;
        }
        if(auto* bytes=std::get_if<SchemaBytes>(&node)) {Event e{frame.base,bytes->bytes,false};stack_.pop_back();return e;}
        if(auto* record=std::get_if<SchemaRecord>(&node)) {
            const auto& order=schema_->summaries_[frame.node].physical;
            if(frame.next==order.size()) {stack_.pop_back();continue;}
            const auto& field=record->fields[order[static_cast<std::size_t>(frame.next++)]];
            auto base=sum(frame.base,field.offset);
            if(schema_->summaries_[field.child].data) stack_.push_back({field.child,base,0});
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
void Runtime::require_buffer_output(void** output) const {
    require_output(output);
    for(auto* root=thread().private_roots;root;root=root->previous_) if(output==&root->value_) throw std::invalid_argument("publication into private builder root");
}
Runtime::BufferBuilder::BufferBuilder(Runtime& r,BufferLayoutRef layout,std::uint64_t live,BufferWorkQuota quota)
    :runtime_(r),layout_(std::move(layout)),quota_(quota),root_(r),owner_(std::this_thread::get_id()),requested_(live) {
    {std::lock_guard lock(r.mutex_);r.validate_buffer_layout(layout_);}
    if(live>layout_->capacity() || !quota.bytes || !quota.node_events || !quota.reference_events) throw std::invalid_argument("invalid buffer live extent/work quota");
    bool due;{std::lock_guard lock(r.mutex_);due=layout_->capacity() && r.allocation_budget_ && r.allocations_>=r.allocation_budget_;}
    if(due && r.collect()!=Collection::completed) throw std::bad_alloc();
    Heap::PendingBuffer pending;std::shared_ptr<const Heap::BufferState> state;
    {
        Native native(r);cursor_.prepare(layout_->schema());
        if(quota.bytes<cursor_.atomic_bytes()) throw std::invalid_argument("byte quota cannot hold atomic scalar");
        if(layout_->capacity()) {state=std::make_shared<const Heap::BufferState>(Heap::BufferState{layout_,&root_});pending=Heap::PendingBuffer(layout_);}
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
        if(!event_ || (!event_->scalar && !event_->u8_run) || (event_->scalar && consumed_)) throw std::invalid_argument("expected an ordered scalar event");
        auto width=event_->u8_run ? 1 : event_->bytes;
        if((width<8 && (bits>>(width*8))) || (event_->encoding==ScalarEncoding::boolean && bits>1)) throw std::invalid_argument("scalar bits exceed encoding");
        runtime_.poll();
        {std::lock_guard lock(runtime_.mutex_);auto* out=address(sum(event_->offset,consumed_),width);
            switch(width) {
                case 1: {auto v=static_cast<std::uint8_t>(bits);std::memcpy(out,&v,1);break;}
                case 2: {auto v=static_cast<std::uint16_t>(bits);std::memcpy(out,&v,2);break;}
                case 4: {auto v=static_cast<std::uint32_t>(bits);std::memcpy(out,&v,4);break;}
                case 8: std::memcpy(out,&bits,8);break;
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
                    std::memcpy(static_cast<char*>(root_.value_)+offset,input.data(),count);prefix_+=count;}
                input=input.subspan(count);
            }
            event_.reset();consumed_=0;cursor_.reset();return;
        }
        while(!input.empty()) {
            normalize();
            if(!event_ || (event_->scalar && event_->encoding!=ScalarEncoding::u8))throw std::invalid_argument("expected byte/u8 construction event");
            auto count=std::min({input.size(),quota_.bytes,event_->bytes-consumed_});runtime_.poll();
            {std::lock_guard lock(runtime_.mutex_);auto* out=address(sum(event_->offset,consumed_),count);
                std::memcpy(out,input.data(),count);in_element_=true;consumed_+=count;
                if(consumed_==event_->bytes) {consumed_=0;event_.reset();}}
            input=input.subspan(count);normalize();
        }
    } catch(...) {failed_=true;throw;}
}
void Runtime::BufferBuilder::zero_elements(std::uint64_t count) {
    try {
        require_active();if(in_element_ || count>requested_-prefix_)throw std::invalid_argument("zero data must append whole elements");
        runtime_.poll();{std::lock_guard lock(runtime_.mutex_);validate_private();prefix_+=count;}
        event_.reset();consumed_=0;cursor_.reset();
    } catch(...) {failed_=true;throw;}
}
void Runtime::BufferBuilder::finish_into(void** output) {
    try {
        require_active();runtime_.require_buffer_output(output);normalize();
        if(prefix_!=requested_ || in_element_ || event_)throw std::logic_error("incomplete buffer construction");
        runtime_.poll();
        std::lock_guard lock(runtime_.mutex_);runtime_.require_buffer_output(output);validate_private();
        // All fallible checks precede this commit. No payload scan/copy, allocation,
        // safepoint or callback can occur between sealing and output publication.
        if(layout_->capacity())runtime_.heap_.seal_buffer(root_.value_,&root_,prefix_);
        *output=root_.value_;root_.value_=nullptr;finished_=true;
    } catch(...) {failed_=true;throw;}
}
}
