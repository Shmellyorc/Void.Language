#pragma once

#include "lexer.hpp"
#include <map>
#include <tuple>

namespace vd::frontend {
struct EditorProbe;

class Diagnostics {
    std::vector<Diagnostic> items;
    std::map<std::string, std::pair<std::string, std::shared_ptr<const SourceFile>>> snapshots;
    std::set<std::tuple<std::string, std::size_t, std::size_t, int, std::string>> seen;
    bool suppressed = false;
    unsigned expression_depth = 0, specialization_depth = 0;
    std::vector<RelatedLocation> context;
public:
    EditorProbe* editor = nullptr;
    EditorRecovery* recovery = nullptr;
    struct Context {
        Diagnostics& owner;std::size_t previous;unsigned previous_depth;
        Context(Diagnostics& d,const std::vector<RelatedLocation>& locations,std::optional<unsigned> depth={}):owner(d),previous(d.context.size()),previous_depth(d.specialization_depth){d.context.insert(d.context.end(),locations.begin(),locations.end());if(depth)d.specialization_depth=*depth;}
        ~Context(){owner.context.resize(previous);owner.specialization_depth=previous_depth;}
    };
    void bind(const SourceFile& source, const std::string& identity);
    void own(SourceSpan& location) const;
    CompilationError owned_failure(const CompilationError& failure) const;
    void add(const CompilationError& failure);
    void warn(SourceSpan location,Code code,const std::string& message,std::vector<RelatedLocation> related={});
    void warn(const SourceFile& source,const Token& token,Code code,const std::string& message) {warn(span(source,token),code,message);}
    std::vector<Diagnostic> warnings() const;
    bool failed() const;
    unsigned& semantic_depth() { return expression_depth; }
    unsigned current_specialization_depth() const {return specialization_depth;}
    bool full() { if (seen.size() < 100) return false; suppressed = true; return true; }
    bool within(const SourceFile& source, std::size_t begin, std::size_t end) const;
    std::size_t size() const { return seen.size(); }
    bool same_errors(const Diagnostics& other) const {
        return seen == other.seen && suppressed == other.suppressed;
    }
    bool presentation_errors() const;
    void finish_parse();
    void finish();

};
}
