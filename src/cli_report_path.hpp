#pragma once
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace vd::cli {
// Compare admitted physical locations, not the spelling of a Windows volume.
// Keep this Windows-only: POSIX paths retain their case-sensitive spelling.
inline std::filesystem::path report_comparison_path(std::filesystem::path path) {
#ifdef _WIN32
    auto text=path.native();
    for(auto& c:text)if(c==L'/')c=L'\\';
    if(text.size()>=8 && CompareStringOrdinal(text.data(),8,L"\\\\?\\UNC\\",8,TRUE)==CSTR_EQUAL)text=L"\\\\"+text.substr(8);
    else if(text.starts_with(L"\\\\?\\"))text.erase(0,4);
    if(text.size()>=2 && text[1]==L':' && text[0]>=L'a' && text[0]<=L'z')text[0]-=L'a'-L'A';
    return std::filesystem::path(text);
#else
    return path;
#endif
}
inline bool report_within(const std::filesystem::path& root,const std::filesystem::path& child) {
    auto normalized_root=report_comparison_path(root), normalized_child=report_comparison_path(child);
    auto a=normalized_root.begin(), b=normalized_child.begin();
    for(;a!=normalized_root.end() && b!=normalized_child.end();++a,++b) {
#ifdef _WIN32
        auto comparison=CompareStringOrdinal(a->c_str(),-1,b->c_str(),-1,TRUE);
        if(!comparison)throw std::runtime_error("Cannot compare timing report locations.");
        if(comparison!=CSTR_EQUAL)return false;
#else
        if(*a!=*b)return false;
#endif
    }
    return a==normalized_root.end();
}
}
