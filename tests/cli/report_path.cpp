#include "../../src/cli_report_path.hpp"
#include <iostream>

int main() {
    using vd::cli::report_within;
    auto require=[](bool value) {if(!value)throw std::runtime_error("Timing report containment regression.");};
#ifdef _WIN32
    using std::filesystem::path;
    using vd::cli::report_comparison_path;
    require(report_comparison_path(path(L"c:\\source"))==path(L"C:\\source"));
    require(report_comparison_path(path(L"d:\\source"))==path(L"D:\\source"));
    require(report_within(path(L"C:\\source"),path(L"c:\\source\\report.json")));
    require(report_within(path(L"c:\\source"),path(L"C:\\source\\report.json")));
    require(!report_within(path(L"C:\\source"),path(L"D:\\source\\report.json")));
    require(!report_within(path(L"C:\\source"),path(L"c:\\source-other\\report.json")));
    require(report_within(path(L"C:\\output\\app.exe"),path(L"c:\\output\\app.exe")));
    require(report_within(path(L"C:\\source"),path(L"\\\\?\\c:\\source\\report.json")));
    require(report_within(path(L"\\\\?\\c:\\source"),path(L"C:\\source\\report.json")));
    require(report_within(path(L"\\\\server\\share\\source"),path(L"\\\\SERVER\\SHARE\\source\\report.json")));
    require(report_within(path(L"\\\\server\\share\\source"),path(L"\\\\?\\UNC\\SERVER\\share\\source\\report.json")));
    require(report_within(path(L"\\\\server\\share\\source"),path(L"\\\\?\\unc\\SERVER\\share\\source\\report.json")));
    require(!report_within(path(L"\\\\server\\share\\source"),path(L"\\\\server\\other\\source\\report.json")));
    require(report_within(path(L"C:\\雪\\Ä"),path(L"c:\\雪\\ä\\report.json")));
    auto long_root=path(L"C:\\雪")/std::wstring(120,L'a')/std::wstring(120,L'b');
    require(report_within(long_root,path(L"\\\\?\\"+long_root.native())/L"report.json"));
    std::cout << "Windows drive, UNC, extended, Unicode and long lexical containment checks passed\n";
#else
    require(report_within("/source","/source/report.json"));
    require(!report_within("/source","/SOURCE/report.json"));
    require(!report_within("/source","/source-other/report.json"));
    require(vd::cli::report_comparison_path("c:/source")==std::filesystem::path("c:/source"));
    std::cout << "POSIX case-sensitive containment checks passed; Windows native checks UNQUALIFIED\n";
#endif
}
