#pragma once
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>
namespace vd {
// Single-threaded proof. Type descriptors list every pointer field, including
// references nested inside inline values. Objects contain zeroed payload bytes.
struct TypeLayout { std::size_t size; std::vector<std::size_t> references; };
class Heap {
public:
    ~Heap();
    void* allocate(const TypeLayout&);
    void collect();
    bool contains(void* object) const;
    std::size_t size() const { return objects_.size(); }
    void add_root(void** slot);
    void remove_root(void** slot);
    void store(void* object, std::size_t offset, void* value);
private:
    struct Object { TypeLayout layout; bool marked = false; };
    std::unordered_map<void*, Object> objects_;
    std::vector<void**> roots_;
};
class Root {
public:
    Root(Heap& heap, void* object = nullptr);
    ~Root();
    Root(const Root&) = delete;
    Root& operator=(const Root&) = delete;
    void* get() const { return value_; }
    void reset(void* value) { value_ = value; }
private:
    Heap& heap_;
    void* value_;
};
}
