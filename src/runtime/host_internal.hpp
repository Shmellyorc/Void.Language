#pragma once
#include "void/host_ops.hpp"
#include "void/native_resource.hpp"
#include "void/filesystem_abi.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace vd::host {
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
Result error(std::int64_t kind,std::int64_t code=0);
Result native_error(int code);
Result crypto_execute(Request&);
Result zip_execute(Request&);
Result directory_execute(Request&);
Result windows_directory_execute(Request&);
Result process_execute(Request&);
Result windows_process_execute(Request&);
}
