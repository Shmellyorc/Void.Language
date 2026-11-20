#include "void/conditional.hpp"
#include "void/diagnostic.hpp"
#include <algorithm>
#include <stdexcept>
#include <llvm/TargetParser/Triple.h>
#include <llvm/TargetParser/Host.h>
namespace vd {
bool valid_build_condition_name(std::string_view s) {
    if(s.empty() || s.size()>64 || (s.front()!='_' && (s.front()<'a' || s.front()>'z')))return false;
    return s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")==s.npos;
}
std::string BuildConditions::key() const {
    std::string out="void-conditions-v1:";
    auto field=[&](std::string_view s){out+=std::to_string(s.size())+":"+std::string(s);};
    field(target_os);field(target_arch);
    for(const auto& [name,value]:features){field(name);field(value?"true":"false");}
    return out;
}
BuildConditions target_build_conditions(std::string_view triple) {
    llvm::Triple t(llvm::Triple::normalize(llvm::StringRef(triple.data(),triple.size())));
    BuildConditions out;
    out.target_os=t.isOSWindows()?"windows":t.isMacOSX()?"macos":t.isOSLinux()?"linux":"unsupported";
    switch(t.getArch()) {
    case llvm::Triple::x86_64:out.target_arch="x86_64";break;
    case llvm::Triple::aarch64:out.target_arch="aarch64";break;
    default:out.target_arch="unsupported";break;
    }
    return out;
}
const BuildConditions& native_build_conditions() {
    static const auto conditions=[] {
        auto selected=target_build_conditions(llvm::sys::getDefaultTargetTriple());
        const auto process=target_build_conditions(llvm::sys::getProcessTriple());
        // Native AOT uses the default triple; ORC uses the process triple. A
        // mismatched SDK must not select source for one and emit for the other.
        if(selected.target_os!=process.target_os || selected.target_arch!=process.target_arch)
            throw std::invalid_argument("Native conditional compilation requires matching LLVM default and process target OS/architecture.");
        return selected;
    }();
    return conditions;
}
namespace {
class Predicate {
    const SourceFile& source;const BuildConditions& config;std::size_t p,end;unsigned depth=0,nodes=0;
    [[noreturn]] void fail(std::string message,std::size_t at) const {
        Diagnostic d(DiagnosticCode::conditional);d.description=std::move(message);
        d.primary={std::make_shared<const SourceFile>(source),{},at,std::min(at+1,source.text().size())};
        d.hint="Use target_os/target_arch comparisons, feature.name, true/false, not, and, or and parentheses.";
        throw CompilationError({std::move(d)});
    }
    void space(){while(p<end && (source.text()[p]==' ' || source.text()[p]=='\t'))++p;}
    bool take(std::string_view s,bool word=false) {
        space();if(std::string_view(source.text()).substr(p,end-p).starts_with(s) &&
            (!word || p+s.size()==end || !((source.text()[p+s.size()]>='a' && source.text()[p+s.size()]<='z') || (source.text()[p+s.size()]>='0' && source.text()[p+s.size()]<='9') || source.text()[p+s.size()]=='_'))) {p+=s.size();return true;}return false;
    }
    std::string name() {
        space();auto start=p;while(p<end && ((source.text()[p]>='a' && source.text()[p]<='z') || (source.text()[p]>='0' && source.text()[p]<='9') || source.text()[p]=='_'))++p;
        return source.text().substr(start,p-start);
    }
    bool atom() {
        space();auto start=p;
        if(++nodes>512 || depth>=64)fail("Conditional expression resource limit exceeded.",start);
        if(take("not",true)){++depth;auto value=atom();--depth;return !value;}
        if(take("(")){++depth;auto value=either();--depth;if(!take(")"))fail("Expected ')' in compile condition.",p);return value;}
        if(take("true",true))return true;
        if(take("false",true))return false;
        const auto key=name();
        if(key=="feature") {
            if(!take("."))fail("Expected feature.name in compile condition.",p);
            space();auto at=p;const auto feature=name();
            if(!valid_build_condition_name(feature))fail("Expected a build condition name after feature.",at);
            auto found=config.features.find(feature);
            if(found==config.features.end())fail("Unknown build condition '"+feature+"'. Declare it in [build.conditions] of the owning project.",at);
            return found->second;
        }
        if(key!="target_os" && key!="target_arch")fail("Unknown compile condition '"+key+"'.",start);
        bool equal=true;
        if(take("==")){}else if(take("!="))equal=false;else fail("Expected == or != after "+key+".",p);
        space();auto at=p;if(!take("\""))fail("Expected a quoted target value.",p);
        auto first=p;while(p<end && source.text()[p]!='"')++p;
        if(p==end)fail("Unterminated target value in compile condition.",at);
        auto value=source.text().substr(first,p-first);++p;
        const bool known=key=="target_os"?(value=="windows" || value=="linux" || value=="macos"):(value=="x86_64" || value=="aarch64");
        if(!known)fail("Unknown "+key+" value '"+value+"'.",at);
        return ((key=="target_os"?config.target_os:config.target_arch)==value)==equal;
    }
    bool both(){auto value=atom();while(take("and",true)){const auto rhs=atom();value=value && rhs;}return value;}
    bool either(){auto value=both();while(take("or",true)){const auto rhs=both();value=value || rhs;}return value;}
public:
    Predicate(const SourceFile& s,std::size_t b,std::size_t e,const BuildConditions& c):source(s),config(c),p(b),end(e){}
    bool evaluate(){auto value=either();space();if(p!=end)fail("Unexpected text in compile condition.",p);return value;}
};
}
bool evaluate_build_condition(const SourceFile& s,std::size_t begin,std::size_t end,const BuildConditions& c) {
    return Predicate(s,begin,end,c).evaluate();
}
}
