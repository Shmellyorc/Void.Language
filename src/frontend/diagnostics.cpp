#include "diagnostics.hpp"
#include <algorithm>

namespace vd::frontend {
void Diagnostics::bind(const SourceFile& source, const std::string& identity) {
    snapshots.insert_or_assign(source.path(), std::make_pair(identity, std::make_shared<const SourceFile>(source)));
}

void Diagnostics::own(SourceSpan& location) const {
    if(!location.source)return;
    auto found = snapshots.find(location.source->path());
    if (found != snapshots.end() && (&found->second.second->text() == &location.source->text() || found->second.second->text() == location.source->text())) {
        location.module_identity = found->second.first; location.source = found->second.second;
    }
}

CompilationError Diagnostics::owned_failure(const CompilationError& failure) const {
    auto records=failure.diagnostics();
    for(auto& record:records) {
        own(record.primary);
        for(auto& related:record.related)own(related.span);
    }
    return CompilationError(std::move(records));
}

void Diagnostics::add(const CompilationError& failure) {
    for (auto d : failure.diagnostics()) {
        d.related.insert(d.related.end(),context.begin(),context.end());
        own(d.primary); for (auto& related : d.related) own(related.span);
        std::set<std::tuple<std::string, std::size_t, std::size_t, std::string>> related_seen;
        std::erase_if(d.related, [&](const auto& related) {
            return !related_seen.emplace(related.span.source->path(), related.span.begin, related.span.end, related.description).second;
        });
        auto key = std::make_tuple(d.primary.source->path(), d.primary.begin, d.primary.end, static_cast<int>(d.code), d.description);
        if (seen.contains(key)) continue;
        if (seen.size() == 100) { suppressed = true; continue; }
        seen.insert(std::move(key));
        items.push_back(std::move(d));
    }
}

void Diagnostics::warn(SourceSpan location,Code code,const std::string& message,std::vector<RelatedLocation> related) {
    if(items.size()-seen.size()>=100)return;
    auto own=[&](SourceSpan& site){if(auto found=snapshots.find(site.source->path());found!=snapshots.end()){site.module_identity=found->second.first;site.source=found->second.second;}};
    own(location);for(auto& item:related)own(item.span);
    for(const auto& item:items)if(item.severity==Severity::warning && item.primary.source->path()==location.source->path() && item.primary.begin==location.begin && item.code==code)return;
    Diagnostic d{code};d.severity=Severity::warning;d.primary=std::move(location);d.description=message;d.related=context;d.related.insert(d.related.end(),related.begin(),related.end());items.push_back(std::move(d));
}

std::vector<Diagnostic> Diagnostics::warnings() const {std::vector<Diagnostic> result;for(const auto& d:items)if(d.severity==Severity::warning)result.push_back(d);return result;}

bool Diagnostics::failed() const { return std::any_of(items.begin(), items.end(), [](const auto& d) { return d.severity == Severity::error; }); }

bool Diagnostics::within(const SourceFile& source, std::size_t begin, std::size_t end) const {
    return std::any_of(items.begin(), items.end(), [&](const auto& d) {
        return d.severity == Severity::error && d.primary.source->path() == source.path() && d.primary.begin >= begin && (d.primary.begin < end || (end == source.text().size() && d.primary.begin == end));
    });
}

bool Diagnostics::presentation_errors() const {
    return recovery && recovery->presentation && !suppressed &&
        std::all_of(items.begin(),items.end(),[&](const auto& item) {
            return item.severity!=Severity::error || item.primary.module_identity==recovery->module;
        });
}

void Diagnostics::finish_parse() {
    if(!presentation_errors() || std::any_of(items.begin(),items.end(),[](const auto& item) {
        return item.severity==Severity::error && (item.code==Code::import_path ||
            item.code==Code::import_read || item.code==Code::alias_conflict || item.code==Code::limit);
    }))finish();
}

void Diagnostics::finish() {
    if (!failed()) return;
    // VFS owner, byte offset, then code: independent of discovery and host enumeration.
    std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return std::tie(a.primary.module_identity, a.primary.begin, a.code) < std::tie(b.primary.module_identity, b.primary.begin, b.code);
    });
    if (suppressed) { auto d = items.back();d.severity=Severity::error; d.code = Code::limit; d.description = "Diagnostic limit of 100 reached; further diagnostic analysis stopped."; d.related.clear(); d.expected_type.clear(); d.received_type.clear(); d.hint = "Fix the reported errors and check again."; items.push_back(std::move(d)); }
    throw CompilationError(std::move(items));
}
}
