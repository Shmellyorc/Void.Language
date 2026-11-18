#include "native_approval.hpp"
#include "frontend/project_digest.hpp"
#include "frontend/project_native.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/path.hpp"
#include "void/project_resolution.hpp"
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Process.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#if defined(_WIN32)
#include "native_windows.hpp"
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace vd {
namespace {
namespace fs = std::filesystem;
using namespace project;
constexpr std::string_view record_header = "VOID native approval 1\n";
constexpr std::size_t record_limit = 16 * 1024, approval_limit = 4096;
constexpr auto missing_approval = "This project declares native code. Pass --allow-native to authorize this invocation, or approve the verified provider in a terminal.";

bool valid_id(std::string_view id) {
    return id.size()==64 && std::ranges::all_of(id,[](char c) {
        return (c>='0' && c<='9') || (c>='a' && c<='f');
    });
}

std::string display(std::string_view bytes) {
    // Do not let an untrusted path or label inject terminal controls.
    std::string escaped;
    llvm::raw_string_ostream out(escaped);
    llvm::printEscapedString(llvm::StringRef(bytes.data(),bytes.size()),out);
    return escaped;
}

fs::path store_path() {
    auto path=native_approval_store();
    if(path.empty())throw NativeApprovalRequired("User data is unavailable for native approval. Pass --allow-native for this invocation.");
    return path;
}

#ifndef _WIN32
void require_safe_ancestors(const fs::path& path) {
    // The account or root must own every existing component. A shared
    // directory such as /tmp is safe only if the sticky bit protects children.
    auto current=path.root_path();
    bool private_ancestor=false;
    for(const auto& name:path.relative_path()) {
        current/=name;
        struct stat info{};
        if(lstat(current.c_str(), &info)) {
            if(errno==ENOENT)return; // The controlled publisher creates missing components.
            throw std::system_error(errno,std::generic_category(),"Cannot inspect native approval path");
        }
        if(!S_ISDIR(info.st_mode) || (info.st_uid!=0 && info.st_uid!=geteuid()) ||
           (!private_ancestor && (info.st_mode & 0022) && !(info.st_mode & S_ISVTX)))
            throw std::runtime_error("Native approval path has an untrusted directory.");
        // Mode + ACL mask on a private home exclude other users from all
        // descendants even if a nested directory's group bits are broader.
        private_ancestor |= (info.st_mode & 0077)==0;
    }
}
#endif

void require_private_directory(const fs::path& path) {
#if defined(_WIN32)
    if(GetDriveTypeW(path.root_path().c_str())!=DRIVE_FIXED)
        throw std::runtime_error("Native approvals require a private local user directory.");
    auto pins=native_windows::pin_directories(path);
    native_windows::PrivateAccess{}.verify(pins.back().value);
#else
    const auto root=inspect_creation_root(path);
    struct stat info{};
    if(lstat(root.path.c_str(), &info) || !S_ISDIR(info.st_mode) || info.st_uid!=geteuid() || (info.st_mode & 0077))
        throw std::runtime_error("Native approvals require a private user directory.");
#endif
}

template<class Action>
void with_approval_store(const fs::path& path,bool create,const Action& action) {
#ifndef _WIN32
    require_safe_ancestors(path);
#endif
    if(fs::exists(fs::symlink_status(path)) || project_path_alias(path))require_private_directory(path);
    with_user_package_store(path,create,[&](const CreationRoot& root) {
        require_private_directory(root.path);
        action(root);
    });
}

std::optional<std::string> read_record(const CreationRoot& root,std::string_view id) {
    const auto path=root.path/utf8_path(id);
    if(!fs::exists(fs::symlink_status(path)) && !project_path_alias(path))return {};
    require_private_directory(path);
    auto entries=fs::directory_iterator(path);
    if(entries==fs::directory_iterator{} || entries->path().filename()!="approval")
        throw std::runtime_error("Malformed native approval record.");
    if(++entries!=fs::directory_iterator{})throw std::runtime_error("Malformed native approval record.");
    auto record=read_project_input(path/"approval",record_limit);
    if(!record.starts_with(record_header) || record.size()<=record_header.size()+id.size()+2 ||
       std::string_view(record).substr(record_header.size(),id.size())!=id ||
       record[record_header.size()+id.size()]!='\n' || record.back()!='\n')
        throw std::runtime_error("Malformed native approval record.");
    const auto label=std::string_view(record).substr(record_header.size()+id.size()+1);
    if(!std::ranges::all_of(label.substr(0,label.size()-1),[](char c){return c>=' ' && c<='~';}))
        throw std::runtime_error("Malformed native approval record.");
    return record;
}

template<class Action>
void each_record(const CreationRoot& root,const Action& action) {
    std::size_t count=0;
    for(const auto& entry:fs::directory_iterator(root.path)) {
        const auto id=path_utf8(entry.path().filename());
        if(id==".void")continue;
        if(++count>approval_limit || !valid_id(id))throw std::runtime_error("Invalid native approval store inventory.");
        action(id);
    }
}

bool interactive() {
#if defined(_WIN32)
    DWORD mode;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode) &&
           GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE),&mode);
#else
    return isatty(STDIN_FILENO) && isatty(STDERR_FILENO) && tcgetpgrp(STDIN_FILENO)==getpgrp();
#endif
}

bool confirm(std::string_view label,std::string_view id) {
    std::cerr<<"Verified native provider for this application:\n  "<<label
             <<"\n  Approval ID (SHA-256): "<<id
             <<"\nNative code can act with your user privileges. Type approve to authorize this exact identity: "<<std::flush;
    for(char c:std::string_view("approve"))if(std::cin.get()!=c)return false;
    const auto end=std::cin.get();
    return end=='\n' || (end=='\r' && std::cin.get()=='\n');
}
}

struct NativeApproval::Payload {
#if defined(_WIN32)
    native_windows::Directory directory{native_windows::temporary_root(),true};
    const fs::path& path() const { return directory.directory; }
#else
    fs::path directory;
    Payload() {
        // Do not give a project-influenced TMPDIR control of the loaded path.
        require_safe_ancestors("/tmp");
        char temporary[]="/tmp/void-native-approval-XXXXXX";
        if(!mkdtemp(temporary))throw std::system_error(errno,std::generic_category(),"Cannot stage approved native payload");
        try {
            directory=temporary;
            require_private_directory(directory);
        } catch(...) {
            rmdir(temporary);
            throw;
        }
    }
    ~Payload() {std::error_code ignored;fs::remove_all(directory,ignored);}
    const fs::path& path() const {return directory;}
#endif
};

NativeApproval::NativeApproval() = default;
NativeApproval::~NativeApproval() = default;

fs::path NativeApproval::authorize(const ManagedNativeApprovalSubject& subject,bool jit) {
    if(llvm::sys::Process::GetEnv("CI"))
        throw NativeApprovalRequired("CI requires explicit --allow-native for this invocation.");
    if(!interactive())throw NativeApprovalRequired(missing_approval);
    const auto store=store_path();
    const auto& dependencies=subject.dependencies;
    const auto& native=*dependencies.native();
    require_outside_native_approval_store(subject.root,true);
    require_outside_native_approval_store(native.dynamic_root(),true);

    Digest digest;
    digest.field("void-managed-native-approval-v1");
    digest.field(path_utf8(subject.root));
    digest.field(dependencies.root_identity());
    digest.field(subject.manifest.project().id);
    digest.field(subject.manifest.application()->id);
    digest.field(subject.entry);
    digest.field(dependencies.identity());
    digest.field(dependencies.native_catalog_identity());
    digest.field(native.dynamic_contract()); // Descriptor bytes, runtime hashes, target, transport, canonical path.
    digest.field(std::to_string(native.abi_revision()));
    digest.field(subject.binding);
    digest.field(subject.binding_source);
    digest.field(compiler_build_identity());
    const auto id=digest.finish();
    const auto& requirement=*subject.manifest.native_requirement();
    const auto label=display(requirement.id)+" "+requirement.version.text()+" | "+native.target()+
        " ABI "+std::to_string(native.abi_revision())+" | "+display(path_utf8(subject.root))+
        " | "+display(subject.manifest.application()->id)+" | "+display(subject.entry);
    const auto expected=std::string(record_header)+id+'\n'+label+'\n';
    if(expected.size()>record_limit)throw NativeApprovalRequired("Native approval identity is too long. Pass --allow-native for this invocation.");
    bool found=false;
    with_approval_store(store,false,[&](const CreationRoot& root){
        if(auto record=read_record(root,id)) {
            if(*record!=expected)throw std::runtime_error("Malformed native approval record.");
            found=true;
        }
    });
    if(!found) {
        if(!confirm(label,id))throw NativeApprovalRequired("Native approval declined. No approval was saved.");
        pending_=expected;
    }
    id_=id;
    if(!jit || subject.binding.empty())return {};
    // The project can change during a prompt and during JIT compilation. Load
    // the exact payload from private files, never from the writable project
    // directory after comparing its hashes. The Runtime still verifies it.
    auto files=read_managed_payload(native);
    payload_=std::make_unique<Payload>();
    for(const auto& [name,bytes]:files) {
        std::ofstream out(payload_->path()/utf8_path(name),std::ios::binary);
        out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
        out.close();
        if(!out)throw std::runtime_error("Cannot stage approved native artifact: "+name);
    }
    return payload_->path();
}

void NativeApproval::remember() {
    if(pending_.empty())return;
    with_approval_store(store_path(),true,[&](const CreationRoot& root){
        if(const auto old=read_record(root,id_)) {
            if(*old!=pending_)throw std::runtime_error("Malformed native approval record.");
            return;
        }
        std::size_t count=0;
        each_record(root,[&](const auto&){++count;});
        if(count>=approval_limit)throw std::runtime_error("Native approval record limit reached; revoke an earlier approval.");
        publish_project_directory(root,id_,{{"approval",pending_}},[&](const fs::path& stage,const ProjectDirectoryReader& reader){
            if(reader.read(stage/"approval",record_limit)!=pending_)
                throw std::runtime_error("Native approval record changed before publication.");
        });
    });
    std::cerr<<"Saved native approval "<<id_<<". Revoke with: voidc native revoke "<<id_<<'\n';
    pending_.clear();
}

std::vector<std::string> native_approvals() {
    std::vector<std::string> rows;
    with_approval_store(store_path(),false,[&](const CreationRoot& root){
        each_record(root,[&](const std::string& id) {
            const auto record=read_record(root,id);
            if(!record)throw std::runtime_error("Native approval disappeared during inspection.");
            rows.push_back(id+" | "+record->substr(record_header.size()+id.size()+1,record->size()-record_header.size()-id.size()-2));
        });
    });
    std::sort(rows.begin(),rows.end());
    return rows;
}

void revoke_native_approval(std::string_view id) {
    if(!valid_id(id))throw NativeApprovalRequired("Expected an exact approval ID from 'voidc native approvals'.");
    bool removed=false;
    with_approval_store(store_path(),false,[&](const CreationRoot& root){
        if(!read_record(root,id))return;
        remove_user_registration(root,std::string(id)); // Serialized, atomic removal from the store.
        removed=true;
    });
    if(!removed)throw std::runtime_error("No native approval with that ID.");
}
}
