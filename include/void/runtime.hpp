#pragma once
#include "void/heap.hpp"
#include "void/buffer.hpp"
#include <optional>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <map>

namespace vd {
// Native element geometry emitted from the shared LLVM DataLayout. This describes
// the existing typed schema, not ownership or executable callbacks.
struct NativeArrayField {const char* target;std::size_t offset;std::uint64_t extent;unsigned kind,nullable;};
struct NativeArrayElement {const char* key;std::size_t bytes,alignment,count;const NativeArrayField* fields;};
class ListModified final : public std::logic_error {public: using std::logic_error::logic_error;};
class CallbackRegistration;
// These are runtime embedding interfaces, not source-language thread features.
struct NativeDictionary {const NativeArrayElement* entry;const NativeArrayElement* key;std::size_t value_offset,hash_offset;};
class Runtime {
public:
    enum class State { native_safe, managed, parked };
    enum class Collection { completed, timed_out, allocation_failure };
    struct Snapshot { std::uint64_t epoch, completed_epoch; std::size_t threads, parked, objects; bool requested; std::uint64_t cooperative_parks; };
    struct Slot { const void* storage; std::size_t size; const std::size_t* offsets; std::size_t count; };
    class Attachment;
    class Managed;
    class Native;
    class Frame;
    class Handle;
    class BufferBuilder;
    class BufferReadLease;
    class RegisteredRootWord;
    struct RootedInterfaceSource;
    explicit Runtime(std::size_t allocation_budget = 0) : allocation_budget_(allocation_budget) {}
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    static Runtime& current(); // Internal generated-code TLS boundary.
    static Runtime* attached() noexcept { return current_runtime_; }
    bool is_managed() const { return current_runtime_ == this && thread().state == State::managed; }
    void poll();
    Collection collect(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    void allocate_into(void** rooted_slot, const TypeLayout&);
    bool begin_release(void*);
    void construct_into(void** rooted_slot, const TypeLayout&, const void* prepared_payload);
    void string_into(void** rooted_slot, std::string_view);
    void concat_into(void** rooted_slot, void* left, void* right);
    static std::size_t string_payload_size(std::size_t left, std::size_t right);
    std::string_view string_bytes(void*) const; // Managed-only immutable borrow.
    void store(void* object, std::size_t offset, void* value);
    bool contains(void*) const;
    void admit_types(std::span<const NominalDescriptor>);
    void admit_metadata(std::span<const NominalDescriptor>,std::span<const InterfaceWitness>);
    void admit_metadata(std::span<const NominalDescriptor>,std::span<const InterfaceWitness>,std::span<const ElementSchemaSpec>);
    void admit_buffer_schemas(std::span<const ElementSchemaSpec>);
    ElementSchemaRef buffer_schema(std::string_view) const;
    ReferenceShape reference_shape(ElementSchemaRef) const;
    RegisteredRootWord root_word(void** registered_slot) const;
    BufferLayoutRef buffer_layout(ElementSchemaRef,std::uint64_t capacity,std::size_t base=0,std::size_t stride=0) const;
    std::byte buffer_byte(void*,const BufferLayoutRef&,std::uint64_t element,std::size_t offset) const;
    void buffer_store_scalar(RegisteredRootWord, const BufferLayoutRef&,std::uint64_t element,
                             std::span<const SchemaStep>,std::uint64_t bits);
    // Runtime-only fixed values: exact admitted scalar type and capacity=live extent.
    // Zero-footprint admitted shapes support default/copy, not scalar access.
    void fixed_default_into(void**,const BufferLayoutRef&,BufferWorkQuota);
    void fixed_literal_into(void**,const BufferLayoutRef&,std::span<const std::uint64_t>,BufferWorkQuota);
    void fixed_copy_into(void**,RegisteredRootWord,const BufferLayoutRef&,BufferWorkQuota);
    std::uint64_t fixed_scalar(RegisteredRootWord,const BufferLayoutRef&,std::uint64_t index) const;
    void fixed_store_scalar(RegisteredRootWord,const BufferLayoutRef&,std::uint64_t index,std::uint64_t bits);
    BufferLayoutRef fixed_scalar_layout(ScalarEncoding,std::uint64_t extent);
    BufferLayoutRef fixed_scalar_layout(RegisteredRootWord,ScalarEncoding,std::uint64_t extent);
    void validate_fixed_scalar_layout(const BufferLayoutRef&,ScalarEncoding,std::uint64_t extent) const;
    BufferLayoutRef native_array_layout(const NativeArrayElement&,std::uint64_t capacity);
    void array_new(void**,const BufferLayoutRef&,bool list,bool zero_live=false);
    std::uint64_t array_count(RegisteredRootWord,const BufferLayoutRef&,bool list) const;
    std::uint64_t array_version(RegisteredRootWord,const BufferLayoutRef&) const;
    void array_read(void*,RegisteredRootWord,const BufferLayoutRef&,bool list,std::uint64_t) const;
    void array_write(RegisteredRootWord,const BufferLayoutRef&,bool list,std::uint64_t,const void*,bool initialize=false,std::optional<std::uint64_t> version={});
    void array_push(RegisteredRootWord,const ElementSchemaRef&,const void*);
    void array_insert(RegisteredRootWord,const ElementSchemaRef&,std::optional<std::uint64_t>,const void*,std::optional<std::uint64_t> version={});
    void array_clear(RegisteredRootWord,const ElementSchemaRef&);
    void dictionary_new(void**,const NativeDictionary&);
    std::uint64_t dictionary_count(RegisteredRootWord,const NativeDictionary&,bool version=false);
    bool dictionary_read(void*,RegisteredRootWord,const NativeDictionary&,const void*,bool contains=false);
    void dictionary_entry(void*,RegisteredRootWord,const NativeDictionary&,std::uint64_t);
    void dictionary_write(RegisteredRootWord,const NativeDictionary&,const void*,std::uint64_t);
    bool dictionary_remove(RegisteredRootWord,const NativeDictionary&,const void*);
    void dictionary_clear(RegisteredRootWord,const NativeDictionary&);
    void array_slice(void**,RegisteredRootWord,const BufferLayoutRef&,bool,std::uint64_t,std::uint64_t,bool);
    void array_remove(void*,RegisteredRootWord,const ElementSchemaRef&,std::uint64_t);
    void array_copy(void**,RegisteredRootWord,const BufferLayoutRef&);
    void list_into(void**,ScalarEncoding,std::uint64_t count=0);
    std::uint64_t list_count(RegisteredRootWord,ScalarEncoding) const;
    std::uint64_t list_version(RegisteredRootWord,ScalarEncoding) const;
    std::uint64_t list_read(RegisteredRootWord,ScalarEncoding,std::uint64_t index) const;
    void list_write(RegisteredRootWord,ScalarEncoding,std::uint64_t index,std::uint64_t bits,std::optional<std::uint64_t> version={});
    void list_push(RegisteredRootWord,ScalarEncoding,std::uint64_t bits);
    std::uint64_t list_remove(RegisteredRootWord,ScalarEncoding,std::uint64_t index);
    const InterfaceWitness* witness(std::string_view) const;
    std::int64_t interface_binding(void* object,const void* witness,std::span<const char* const> keys) const;
    std::shared_ptr<const NominalDescriptor> descriptor(std::string_view) const;
    std::int64_t dynamic_type(void*,std::span<const char* const>) const;
    Snapshot snapshot() const;
    bool wait_for_request(std::chrono::milliseconds);
    void close(); // Reject new attachments; existing participants must drain.
private:
    friend class CallbackRegistration;
    class PrivateRoot;
    struct Thread {
        std::thread::id owner = std::this_thread::get_id();
        State state = State::native_safe;
        Frame* frames = nullptr;
        PrivateRoot* private_roots = nullptr;
        unsigned depth = 0;
        unsigned managed_depth = 0, admissions = 0;
        std::uint64_t epoch = 0;
        std::uint64_t collecting_epoch = 0;
        Collection result = Collection::completed;
    };
    static thread_local Runtime* current_runtime_;
    static thread_local Thread* current_thread_;
    Thread& thread() const;
    void require_managed() const;
    Frame* require_output(void**) const;
    void string_parts_into(void**,std::string_view,std::string_view);
    void validate_descriptor(const TypeLayout&) const; // Caller holds mutex_.
    void validate_buffer_layout(const BufferLayoutRef&) const; // mutex_ held
    ElementSchemaRef fixed_scalar_schema(ScalarEncoding);
    void validate_fixed_layout(const BufferLayoutRef&) const; // mutex_ held
    struct DictionaryHeader {void* entries;void* buckets;std::uint64_t count,capacity,version;};
    struct DictionaryLayout {BufferLayoutRef entries,key;std::shared_ptr<const NominalDescriptor> header;};
    DictionaryLayout dictionary_layout(const NativeDictionary&);
    DictionaryHeader& dictionary_header(RegisteredRootWord,const DictionaryLayout&) const;
    std::uint64_t dictionary_hash(const void*,const NativeArrayElement&) const;
    bool dictionary_equal(const void*,const void*,const NativeArrayElement&) const;
    std::uint64_t dictionary_find(const DictionaryHeader&,const NativeDictionary&,const void*,std::uint64_t) const;
    void dictionary_index(DictionaryHeader&,const NativeDictionary&) const;
    struct ListHeader {void* backing;std::uint64_t count,capacity,version;};
    void array_write_value(RegisteredRootWord,const ElementSchemaRef&,const BufferLayout*,std::uint64_t,const void*,bool,std::optional<std::uint64_t>);
    static std::string list_identity(const ElementSchemaRef&);
    ListHeader& list_header(RegisteredRootWord,const ElementSchemaRef&) const;
    ListHeader& list_header(RegisteredRootWord,ScalarEncoding) const; // mutex_ held
    void validate_native_element(const void*,DataCursor&,const ElementSchemaRef&) const;
    void append_native(BufferBuilder&,const BufferLayoutRef&,const void*);
    void validate_fixed_output(void**,const BufferLayoutRef&) const;
    void store_scalar(RegisteredRootWord,const BufferLayoutRef&,std::uint64_t,
                      std::span<const SchemaStep>,std::uint64_t,bool fixed);
    Frame* require_buffer_output(void**) const;
    void* reference_source(const RegisteredRootWord&) const; // mutex_ held
    void park(Thread&, std::unique_lock<std::mutex>&);
    void enter(Thread&);
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    Heap heap_;
    using NominalCatalog=std::map<std::string,std::shared_ptr<const NominalDescriptor>,std::less<>>;
    using TypeCatalog=RuntimeMetadata;
    std::shared_ptr<const TypeCatalog> types_ = {};
    std::vector<Thread*> threads_;
    std::vector<Handle*> handles_;
    bool closing_ = false;
    bool requested_ = false;
    std::size_t callbacks_ = 0;
    std::size_t allocation_budget_ = 0, allocations_ = 0;
    std::uint64_t epoch_ = 0, completed_epoch_ = 0;
    std::uint64_t cooperative_parks_ = 0;
};

class Runtime::Attachment {
public:
    explicit Attachment(Runtime&);
    ~Attachment();
    Attachment(const Attachment&) = delete;
    Attachment& operator=(const Attachment&) = delete;
private:
    friend class CallbackRegistration;
    Attachment(Runtime&, bool admitted_callback);
    Runtime& runtime_;
    Thread record_;
    bool outer_ = false;
    bool callback_ = false;
};
class Runtime::Managed {
public:
    explicit Managed(Runtime&);
    ~Managed();
    Managed(const Managed&) = delete;
    Managed& operator=(const Managed&) = delete;
private:
    Runtime& runtime_;
    State previous_;
};
class Runtime::Native {
public:
    explicit Native(Runtime&);
    ~Native();
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;
private:
    Runtime& runtime_;
};
class Runtime::Frame {
public:
    Frame(Runtime&, const Slot*, std::size_t count);
    ~Frame();
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
private:
    friend class Runtime;
    Runtime& runtime_;
    Frame* previous_;
    const Slot* slots_;
    std::size_t count_;
};
class Runtime::Handle {
public:
    explicit Handle(Runtime&, void* value = nullptr);
    ~Handle();
    void reset(void*);
    // Only Managed code may dereference this whole-object reference.
    void* get() const;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
private:
    friend class Runtime;
    Runtime& runtime_;
    void* value_;
};
// A private root is still an ordinary Frame/Slot. Its separate registration is
// permission for exactly this word, never permission for another root/heap edge.
class Runtime::PrivateRoot {
public:
    explicit PrivateRoot(Runtime&);
    ~PrivateRoot() noexcept;
private:
    friend class Runtime;
    friend class Runtime::BufferBuilder;
    Runtime& runtime_;
    void* value_=nullptr;
    static constexpr std::size_t zero_=0;
    Slot slot_;
    Frame frame_;
    PrivateRoot* previous_;
};
// A short-lived registration proof, not a cached referent. The originating Frame
// must stay linked for each operation. Tokens cannot be created from raw pointers.
class Runtime::RegisteredRootWord {
private:
    friend class Runtime;
    RegisteredRootWord(const Runtime* r,Frame* f,void** p) : runtime_(r),frame_(f),slot_(p),owner_(std::this_thread::get_id()) {}
    const Runtime* runtime_;
    Frame* frame_;
    void** slot_;
    std::thread::id owner_;
};
struct Runtime::RootedInterfaceSource {
    RegisteredRootWord object;
    const InterfaceWitness* witness;
};
// A complete source snapshot's storage is rooted and frozen until this scoped,
// thread-affine lease dies. Referenced objects retain their ordinary semantics.
class Runtime::BufferReadLease {
public:
    BufferReadLease(Runtime&,RegisteredRootWord,const BufferLayoutRef&);
    ~BufferReadLease() noexcept;
    BufferReadLease(const BufferReadLease&)=delete;
    BufferReadLease& operator=(const BufferReadLease&)=delete;
    BufferReadLease(BufferReadLease&&)=delete;
    BufferReadLease& operator=(BufferReadLease&&)=delete;
    std::uint64_t count() const {return count_;}
private:
    friend class Runtime::BufferBuilder;
    void validate() const; // Runtime mutex held, Managed owner thread
    Runtime& runtime_;
    BufferLayoutRef layout_;
    std::thread::id owner_;
    void* value_=nullptr;
    static constexpr std::size_t zero_=0;
    Slot slot_{&value_,sizeof(value_),&zero_,1};
    Frame frame_;
    std::uint64_t count_=0;
};
class Runtime::BufferBuilder {
public:
    BufferBuilder(Runtime&,BufferLayoutRef,std::uint64_t requested_live,BufferWorkQuota);
    ~BufferBuilder()=default;
    BufferBuilder(const BufferBuilder&)=delete;
    BufferBuilder& operator=(const BufferBuilder&)=delete;
    BufferBuilder(BufferBuilder&&)=delete;
    BufferBuilder& operator=(BufferBuilder&&)=delete;
    void write_scalar(std::uint64_t bits);
    void write_bytes(std::span<const std::byte> native_input);
    void write_reference(RegisteredRootWord source);
    void write_interface(const RootedInterfaceSource& source);
    void zero_elements(std::uint64_t count);
    void copy_elements(BufferReadLease&,std::uint64_t source_first,std::uint64_t count);
    void finish_into(void** registered_output);
private:
    void require_active();
    void normalize();
    void write_root(const RegisteredRootWord&,const InterfaceWitness*,bool interface);
    void validate_private() const; // mutex_ held
    char* address(std::size_t offset,std::size_t bytes) const; // mutex_ held
    Runtime& runtime_;
    BufferLayoutRef layout_;
    BufferWorkQuota quota_;
    DataCursor cursor_;
    PrivateRoot root_;
    std::thread::id owner_;
    std::uint64_t requested_,prefix_=0;
    std::optional<DataCursor::Event> event_;
    std::size_t consumed_=0;
    bool in_element_=false,failed_=false,finished_=false;
};

}
