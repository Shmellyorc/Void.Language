#pragma once
#include "void/host_ops.hpp"
#include "void/native_resource.hpp"
#include "void/filesystem_abi.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace vd::host {
inline constexpr std::size_t byte_snapshot_limit = 16 * 1024 * 1024;
// Native snapshots only. No managed pointers may be dereferenced in execute().
struct Result {
    std::int64_t kind=0,code=0,number=0,detail=0;
    bool flag=false;
    std::string text,bytes;
};
struct Object {
    virtual ~Object()=default;
    virtual std::error_code close() noexcept=0;
    virtual void stop() noexcept {}
    virtual std::error_code quiesce() noexcept {return {};}
};
struct Request {
    unsigned operation;
    std::vector<std::string> texts,bytes;
    std::vector<std::vector<std::string>> arrays;
    std::vector<std::int64_t> numbers;
    std::vector<std::shared_ptr<Object>> objects;
    std::shared_ptr<filesystem::FileBorrow> file;
    std::shared_ptr<Object> acquired;
    std::shared_ptr<filesystem::File> acquired_file;
};
using CaptureLimits = std::array<std::int64_t,4>; // stdout, stderr, combined bytes, milliseconds
enum class CaptureEnd : std::int64_t { complete, output_limit, deadline, io_failure, cancelled };
struct CaptureResult {
    Result launch, status;
    CaptureEnd end=CaptureEnd::complete;
    int io_code=0, cleanup_code=0;
    bool exited=false;
    std::array<bool,2> eof{};
    std::array<std::string,2> output;
};
struct PipeRead { std::size_t size=0; int error=0; bool eof=false; };
struct CaptureChild : Object {
    virtual std::optional<Result> poll_exit() noexcept=0;
    virtual PipeRead read(unsigned, std::span<char>) noexcept=0;
    virtual std::error_code close_readers() noexcept=0;
};
Result error(std::int64_t kind,std::int64_t code=0);
Result native_error(int code);
Result crypto_execute(Request&);
Result zip_execute(Request&);
Result encode_zip(Runtime&,void*,const NativeArrayElement*,std::span<const std::int64_t,4>);
Result directory_execute(Request&);
Result windows_directory_execute(Request&);
Result process_execute(Request&,const CaptureLimits* = nullptr);
Result windows_process_execute(Request&,const CaptureLimits*);
CaptureResult capture_execute(Request&,const CaptureLimits&,Runtime&,const std::atomic<bool>&);
}
