#include "void/lsp.hpp"
#include "void/diagnostic.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
template<class F> void reject(F f) { bool rejected=false; try { f(); } catch(const std::invalid_argument&) { rejected=true; } check(rejected,"invalid URI accepted"); }
}
int main() {
    try {
        using vd::lsp::Position; using vd::lsp::Positions;
        const vd::SourceFile source("/coordinates.void","a\xc3\xa9\xf0\x9f\x98\x80z\r\n\r\nq\rx\n");
        Positions map(source);
        check(map.position(0)==Position{0,0} && map.position(1)==Position{0,1},"ASCII");
        check(map.position(3)==Position{0,2} && map.position(7)==Position{0,4},"Unicode UTF16 widths");
        check(!map.position(2) && !map.position(4) && !map.position(6),"split UTF8 sequence");
        check(map.offset({0,2})==3 && map.offset({0,4})==7 && !map.offset({0,3}),"surrogate pair");
        check(map.position(8)==Position{0,5} && !map.position(9) && map.position(10)==Position{1,0},"CRLF boundaries");
        check(map.offset({0,500})==8 && map.offset({1,8})==10,"LSP EOL clamping");
        check(map.position(12)==Position{2,0} && map.position(14)==Position{3,0} && map.position(16)==Position{4,0},"CR LF empty lines EOF");
        check(map.offset({4,0})==16 && !map.offset({5,0}) && !map.position(17),"EOF and invalid lines");
        Positions empty(vd::SourceFile("empty","")); check(empty.position(0)==Position{0,0} && empty.offset({0,9})==0,"empty source");
        Positions bom(vd::SourceFile("bom","\xef\xbb\xbfX")); check(bom.position(3)==Position{0,1},"BOM counts exact text units");
        for(std::size_t i=0;i<=source.text().size();++i) if(auto p=map.position(i)) check(map.offset(*p)==i,"byte-position roundtrip");
        // SourceFile snapshots rejected by validation still own original disk bytes.
        try { vd::SourceFile invalid("bad",std::string("a\xffz",3)); }
        catch(const vd::CompilationError& failure) {
            Positions bad(*failure.diagnostics().front().primary.source);
            check(bad.position(1)==Position{0,1} && bad.position(2)==Position{0,2},"invalid disk UTF8 replacement positions");
        }
        auto path=std::filesystem::current_path()/vd::utf8_path("uri space # % \xc3\xa9 \xf0\x9f\x98\x80.void");
        auto uri=vd::lsp::file_uri(path); check(uri.find("%20")!=std::string::npos && uri.find("%23")!=std::string::npos && uri.find("%25")!=std::string::npos,"URI escapes");
        check(vd::lsp::uri_path(uri)==path,"Unicode file URI roundtrip");
        reject([]{vd::lsp::uri_path("https://host/a.void");});
        reject([]{vd::lsp::uri_path("file://host/a.void");});
        reject([]{vd::lsp::uri_path("file:///a%00.void");});
        reject([]{vd::lsp::uri_path("file:///a%GG.void");});
        reject([]{vd::lsp::uri_path("file:///a%FF.void");});
        reject([]{vd::lsp::uri_path("file:///a.void#fragment");});
        reject([]{vd::lsp::uri_path("file:///a/../b.void");});
        reject([]{vd::lsp::uri_path("file:///a%5Cb.void");});
        std::cout<<"UTF16 positions, owned snapshots and URI mapping passed\n";
        return 0;
    } catch(const std::exception& failure) { std::cerr<<failure.what()<<'\n'; return 1; }
}
