#include "void/lsp.hpp"
#include "void/tooling.hpp"
#include "void/project_creation.hpp"
#include "void/numeric.hpp"
#include "void/path.hpp"
#include <bit>
#include <array>
#include <llvm/Support/JSON.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <chrono>
#include <functional>
#include <cerrno>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define NOMINMAX
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif
namespace {
namespace j=llvm::json;
namespace fs=std::filesystem;
bool read_only_source(std::string_view id) {
    return id.starts_with("/@stdlib/") || vd::package_source(id);
}
using vd::lsp::Positions;
constexpr std::array semantic_types{"namespace", "type", "struct", "interface", "enum", "typeParameter",
    "parameter", "variable", "property", "enumMember", "event", "function", "method", "operator"};
constexpr std::array semantic_modifiers{"declaration", "definition", "readonly", "static", "defaultLibrary"};
static_assert(semantic_types.size()==static_cast<std::size_t>(vd::SemanticToken::Kind::count));
constexpr char semantic_refresh_id[]="void/semanticTokens/refresh";
constexpr std::size_t max_body=8*1024*1024, max_header=8192;
struct FramingError : std::runtime_error { using std::runtime_error::runtime_error; };
// One bounded transport buffer and one analysis thread. Drain ordered mutations
// already on the pipe before analyzing; requests are barriers. No worker/cache.
class Input {
    char bytes[65536];std::size_t begin=0,end=0;
public:
    std::function<bool()> alive=[] {return true;};
    bool ready(unsigned milliseconds) {
        if(begin<end)return true;
#ifdef _WIN32
        auto handle=GetStdHandle(STD_INPUT_HANDLE);DWORD available=0;
        auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
        do {
            if(!PeekNamedPipe(handle,nullptr,0,nullptr,&available,nullptr))return true;
            if(available)return true;
            if(!milliseconds)return false;
            Sleep(1);
        }while(std::chrono::steady_clock::now()<until);
        return false;
#else
        pollfd descriptor{STDIN_FILENO,POLLIN,0};
        int status;do{status=::poll(&descriptor,1,static_cast<int>(milliseconds));}while(status<0 && errno==EINTR);
        if(status<0)throw FramingError("Cannot poll protocol input");
        return status>0;
#endif
    }
    int get() {
        if(begin==end) {
            while(!ready(250))if(!alive())throw FramingError("Client process exited");
#ifdef _WIN32
            auto count=_read(_fileno(stdin),bytes,sizeof(bytes));
#else
            auto count=::read(STDIN_FILENO,bytes,sizeof(bytes));
#endif
            if(count<0)throw FramingError("Cannot read protocol input");
            if(!count)return EOF;begin=0;end=static_cast<std::size_t>(count);
        }
        return static_cast<unsigned char>(bytes[begin++]);
    }
};
Input input;

std::string lower(std::string s) { for(auto& c:s) if(c>='A' && c<='Z') c+='a'-'A'; return s; }
std::string trim(std::string s) {
    auto first=s.find_first_not_of(" \t"), last=s.find_last_not_of(" \t");
    return first==std::string::npos ? "" : s.substr(first,last-first+1);
}
std::optional<std::string> frame() {
    std::size_t consumed=0; std::optional<std::size_t> length;
    std::set<std::string> fields;
    for(;;) {
        std::string line;
        for(;;) {
            auto c=input.get();
            if(c==EOF) {
                if(!consumed) return {};
                throw FramingError("Truncated protocol header");
            }
            if(++consumed>max_header) throw FramingError("Protocol header exceeds 8192 bytes");
            if(c=='\r') {
                if(input.get()!='\n') throw FramingError("Protocol headers require CRLF");
                if(++consumed>max_header) throw FramingError("Protocol header too large");
                break;
            }
            if((c<32 && c!='\t') || c>126) throw FramingError("Non-ASCII protocol header");
            line+=static_cast<char>(c);
        }
        if(line.empty()) break;
        auto colon=line.find(':');
        if(colon==std::string::npos || colon==0) throw FramingError("Malformed protocol header");
        auto name=lower(line.substr(0,colon)), value=trim(line.substr(colon+1));
        for(char c:name) if(!((c>='a' && c<='z') || (c>='0' && c<='9') || std::string_view("!#$%&'*+-.^_`|~").find(c)!=std::string_view::npos)) throw FramingError("Invalid header field name");
        if(!fields.insert(name).second) throw FramingError("Duplicate protocol header");
        if(name=="content-length") {
            if(value.empty()) throw FramingError("Missing Content-Length value");
            std::size_t n=0;
            for(char c:value) {
                if(c<'0' || c>'9' || n>(max_body-std::size_t(c-'0'))/10) throw FramingError("Invalid or excessive Content-Length");
                n=n*10+std::size_t(c-'0');
            }
            length=n;
        } else if(name=="content-type") {
            auto type=lower(value); auto semi=type.find(';');
            if(trim(type.substr(0,semi))!="application/vscode-jsonrpc") throw FramingError("Unsupported protocol Content-Type");
            if(semi!=std::string::npos) {
                auto charset=trim(type.substr(semi+1));
                if(charset!="charset=utf-8" && charset!="charset=utf8") throw FramingError("Only UTF-8 protocol content is supported");
            }
        }
    }
    if(!length) throw FramingError("Content-Length is required");
    std::string body(*length,'\0');
    for(auto& c:body){auto byte=input.get();if(byte==EOF)throw FramingError("Truncated protocol body");c=static_cast<char>(byte);}
    return body;
}
bool bounded_json(const std::string& text) {
    unsigned depth=0; bool quoted=false, escaped=false;
    for(char c:text) {
        if(quoted) { if(escaped) escaped=false; else if(c=='\\') escaped=true; else if(c=='"') quoted=false; }
        else if(c=='"') quoted=true;
        else if(c=='{' || c=='[') { if(++depth>128) return false; }
        else if((c=='}' || c==']') && depth) --depth;
    }
    return true; // Resource guard only. LLVM JSON performs all syntax validation.
}
void send(j::Value value) {
    std::string body; llvm::raw_string_ostream out(body); out<<value;
    std::cout<<"Content-Length: "<<body.size()<<"\r\n\r\n"<<body<<std::flush;
    if(!std::cout) throw FramingError("Protocol output disconnected");
}
void error(const j::Value& id,int code,const std::string& message) {
    send(j::Object{{"jsonrpc","2.0"},{"id",id},{"error",j::Object{{"code",code},{"message",message}}}});
}
const j::Object& object(const j::Value* value) {
    if(!value || !value->getAsObject()) throw std::invalid_argument("Expected an object");
    return *value->getAsObject();
}
std::string string(const j::Object& o,llvm::StringRef key) {
    auto text=o.getString(key); if(!text) throw std::invalid_argument("Expected string '"+key.str()+"'"); return text->str();
}
std::int64_t integer(const j::Value* v,std::int64_t lo=-2147483648LL) {
    // Bound before getAsInteger: never cast a rounded/out-of-range double to int.
    auto number=v ? v->getAsNumber() : std::nullopt;
    if(!number || !std::isfinite(*number) || *number<lo || *number>2147483647 || std::trunc(*number)!=*number)
        throw std::invalid_argument("Expected an LSP integer in range");
    return *v->getAsInteger();
}
j::Object position(vd::lsp::Position p) { return j::Object{{"line",p.line},{"character",p.character}}; }
j::Object range(const vd::SourceSpan& span) {
    Positions positions(*span.source);
    auto begin=positions.position(span.begin), end=positions.position(span.end);
    if(!begin || !end) throw std::logic_error("Compiler span is not a protocol boundary");
    return j::Object{{"start",position(*begin)},{"end",position(*end)}};
}
bool known_source_path(fs::path path,const fs::path& source) {
#ifdef _WIN32
    auto requested=path.generic_u8string(),expected=source.generic_u8string();
    auto fold=[](char8_t c){return c>=u8'A' && c<=u8'Z'?char8_t(c+u8'a'-u8'A'):c;};
    if(requested.size()>1 && expected.size()>1 && requested[1]==u8':' && expected[1]==u8':' && fold(requested[0])==fold(expected[0])) {
        requested[0]=expected[0];path=fs::path(requested);
    }
#endif
    return path==source;
}
j::Object project_summary(const fs::path& root,const vd::ProjectAnalysisResult& result,std::uint64_t revision,bool reselect) {
    j::Array errors;
    for(const auto& d:result.diagnostics) {
        if(errors.size()==8)break;
        auto count=std::min<std::size_t>(d.description.size(),256);
        while(count<d.description.size() && count && (static_cast<unsigned char>(d.description[count])&0xc0)==0x80)--count;
        errors.push_back(j::Object{{"code",vd::diagnostic_code(d.code)},{"message",d.description.substr(0,count)}});
    }
    auto state=result.state;
    return j::Object{{"schema",2},{"mode","project"},{"root",vd::path_utf8(root)},{"manifest",vd::path_utf8(root/"void.project.toml")},
        {"kind",result.manifest?j::Value(result.manifest->kind()==vd::project::Kind::application?"application":"library"):j::Value(nullptr)},
        {"entry",result.project && result.project->entry()?j::Value(vd::path_utf8(*result.project->entry())):j::Value(nullptr)},
        {"standalone",false},{"selectionRevision",std::to_string(revision)},{"snapshotKey",state?state->snapshot_key:std::string{}},
        {"status",!result.succeeded()?"error":reselect?"reselection-required":"ready"},{"requiresReselection",reselect},
        {"discoveryCount",static_cast<std::int64_t>(state?state->discovery.size():0)},{"executionCount",static_cast<std::int64_t>(state?state->execution.size():0)},
        {"diagnosticCount",static_cast<std::int64_t>(result.diagnostics.size())},{"diagnostics",std::move(errors)}};
}
class Server {
    enum class State { initial, waiting, ready, shutdown } state=State::initial;
    struct Document { std::int64_t version; std::string uri; std::optional<vd::SourceFile> source; };
    fs::path root, selected_root, entry_path, requested_root;
    vd::ToolingSession session;
    bool manifest_mode;
    std::map<std::string,Document> open;
    std::map<std::string,std::string> published;
    bool hierarchical_symbols=false;
    bool dirty=false;
    std::int64_t process_id=0;
    std::set<std::string> cancellations;
    bool related_information=false, diagnostic_versions=false, rename_edits=false, signature_plaintext=false;
    bool semantic_full=false, semantic_refresh=false, semantic_requested=false;
    bool semantic_refresh_pending=false, semantic_refresh_needed=false;
    void refresh_semantics() {
        if(!semantic_refresh_needed || semantic_refresh_pending || state!=State::ready)return;
        semantic_refresh_needed=false;
        semantic_refresh_pending=true;
        send(j::Object{{"jsonrpc","2.0"},{"id",semantic_refresh_id},{"method","workspace/semanticTokens/refresh"}});
    }
    std::string identity(const std::string& uri) const {
        auto path=vd::lsp::uri_path(uri);
        if(auto current=session.current_analysis();current) {
            if(current->semantic_analysis())
                for(const auto& m:current->semantic_analysis()->modules())if(read_only_source(m.identity) && known_source_path(path,vd::utf8_path(m.source.path())))return m.identity;
            if(current->project() && current->project()->state)
                for(const auto& [id,source]:current->project()->state->execution)
                    if(vd::package_source(id) && known_source_path(path,vd::utf8_path(source.path())))return id;
            for(const auto& d:current->result().diagnostics)
                if(vd::package_source(d.primary.module_identity) && d.primary.source &&
                   known_source_path(path,vd::utf8_path(d.primary.source->path())))return d.primary.module_identity;
        }
#ifdef _WIN32
        // Drive-letter spelling is URI/OS notation, not a VFS component.
        auto raw=path.generic_u8string();
        if(raw.size()>1 && raw[1]==u8':') {
            auto drive=root.generic_u8string();
            auto folded=[](char8_t c) { return c>=u8'A' && c<=u8'Z' ? char8_t(c+u8'a'-u8'A') : c; };
            if(folded(raw[0])!=folded(drive[0])) throw std::invalid_argument("File URI is on another drive");
            raw[0]=drive[0]; path=fs::path(raw);
        }
#endif
        auto relative=path.lexically_relative(root);
        if(relative.empty() || relative.native().starts_with(fs::path("..").native())) relative=path.lexically_relative(selected_root);
        auto utf8=relative.generic_u8string(); std::string key="/"+std::string(utf8.begin(),utf8.end());
        if(manifest_mode && key.ends_with("/void.project.toml"))vd::require_manifest_identity(key);
        else vd::require_module_identity(key); // Includes escape/canonical/case spelling policies.
        return key;
    }
    std::string uri(const std::string& id) const {
        // Filesystem canonicalization owns compiler identity, not client URI spelling.
        // Versioned edits must address the exact document opened by the client.
        if(read_only_source(id)) {
            if(auto current=session.current_analysis();current && current->semantic_analysis())for(const auto& m:current->semantic_analysis()->modules())if(m.identity==id)return vd::lsp::file_uri(vd::utf8_path(m.source.path()));
            if(vd::package_source(id)) {
                if(auto previous=published.find(id);previous!=published.end())return previous->second;
                throw std::invalid_argument("Package source has no admitted URI");
            }
            return vd::lsp::file_uri(vd::standard_library_root()/vd::utf8_path(id.substr(9)));
        }
        if(auto document=open.find(id);document!=open.end())return document->second.uri;
        return vd::lsp::file_uri(selected_root/vd::utf8_path(id).relative_path());
    }
    j::Object location(const vd::SourceSpan& span) const {
        auto target=vd::package_source(span.module_identity) && span.source
            ? vd::lsp::file_uri(vd::utf8_path(span.source->path())) : uri(span.module_identity);
        return j::Object{{"uri",std::move(target)},{"range",range(span)}};
    }
    vd::DocumentRevision next(const std::string& id) const {
        auto snapshot=session.select(); auto found=snapshot.documents().find(id);
        auto revision=found==snapshot.documents().end() ? 0 : found->second;
        if(revision==UINT64_MAX) throw std::overflow_error("Document revision exhausted");
        return revision+1;
    }
    void analyze() {
        dirty=false;
        auto result=vd::ToolingSession::analyze(session.select());
        if(!session.accept(result) || !session.is_current(*result)) return;
        std::map<std::string,j::Array> groups;
        std::map<std::string,std::string> package_uris;
        for(const auto& [id,doc]:open) { (void)doc; groups[id]; }
        for(const auto& [id,previous_uri]:published) { (void)previous_uri; groups[id]; } // Clear removed/repaired graph nodes.
        if(result->project() && result->project()->project && result->project()->project->entry())entry_path=*result->project()->project->entry();
        else if(manifest_mode)entry_path.clear();
        if(result->semantic_analysis()) for(const auto& module:result->semantic_analysis()->modules()) groups[module.identity];
        for(const auto& d:result->result().diagnostics) {
            auto id=d.primary.module_identity;
            if(id.empty()) throw std::logic_error("Project diagnostic has no VFS owner");
            if(vd::package_source(id) && d.primary.source)
                package_uris[id]=vd::lsp::file_uri(vd::utf8_path(d.primary.source->path()));
            auto message=d.description;
            if(!d.hint.empty()) message+="\nHint: "+d.hint;
            j::Object diagnostic{{"range",range(d.primary)},{"severity",d.severity==vd::Severity::error ? 1 : d.severity==vd::Severity::warning ? 2 : 3},
                {"code",vd::diagnostic_code(d.code)},{"source","VOID"},{"message",message}};
            if(related_information && !d.related.empty()) {
                j::Array related;
                for(const auto& r:d.related) related.push_back(j::Object{{"location",location(r.span)},{"message",r.description}});
                diagnostic["relatedInformation"]=std::move(related);
            }
            groups[id].push_back(std::move(diagnostic));
        }
        // Closing/reopening under another URI spelling must clear old diagnostics.
        auto target_uri=[&](const std::string& id) {
            if(auto found=package_uris.find(id);found!=package_uris.end())return found->second;
            return uri(id);
        };
        for(const auto& [id,previous_uri]:published)if(previous_uri!=target_uri(id))
            send(j::Object{{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},
                {"params",j::Object{{"uri",previous_uri},{"diagnostics",j::Array{}}}}});
        std::map<std::string,std::string> next_published;
        for(auto& [id,diagnostics]:groups) {
            const auto target=target_uri(id);
            j::Object params{{"uri",target},{"diagnostics",std::move(diagnostics)}};
            if(manifest_mode)params["voidProjectRevision"]=std::to_string(result->snapshot().revision());
            if(auto doc=open.find(id); doc!=open.end() && diagnostic_versions) params["version"]=doc->second.version;
            // Retain only nonempty diagnostics: old empty/unreachable nodes need
            // no unbounded clearing history. Open and reachable nodes seed each run.
            if(!params.getArray("diagnostics")->empty()) next_published.emplace(id,target);
            send(j::Object{{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},{"params",std::move(params)}});
        }
        published=std::move(next_published);
        if(manifest_mode)send(j::Object{{"jsonrpc","2.0"},{"method","void/projectState"},{"params",summary()}});
        if(semantic_refresh && semantic_requested) {
            semantic_refresh_needed=true;
            refresh_semantics();
        }
    }
    j::Object summary() const {
        auto current=session.current_analysis();
        if(!manifest_mode || !current || !current->project())throw std::invalid_argument("No current manifest project summary");
        return project_summary(root,*current->project(),current->snapshot().revision(),session.requires_reselection());
    }
    vd::AnalysisResult check_overlays(const vd::SourceOverrides& sources) const {
        auto snapshot=session.select();
        if(!manifest_mode)return vd::analyze_project(entry_path,sources);
        auto checked=vd::analyze_manifest_project(requested_root,snapshot.entry_override(),sources,snapshot.manifests());
        return {checked.project?checked.project->semantic_analysis():nullptr,std::move(checked.diagnostics)};
    }
    j::Value query(const std::string& method,const j::Object& params) const {
        auto id=identity(string(object(params.get("textDocument")),"uri"));
        auto& point=object(params.get("position"));
        vd::lsp::Position p{static_cast<std::uint32_t>(integer(point.get("line"),0)),static_cast<std::uint32_t>(integer(point.get("character"),0))};
        bool include=false;
        if(method=="textDocument/references") {
            auto value=object(params.get("context")).getBoolean("includeDeclaration");
            if(!value) throw std::invalid_argument("Expected includeDeclaration boolean"); include=*value;
        }
        auto empty=[&]() -> j::Value { return method=="textDocument/definition" ? j::Value(nullptr) : j::Value(j::Array{}); };
        auto current=session.current_analysis(); auto index=session.current_index();
        if(!current || !index) return empty();
        const vd::SourceFile* source=nullptr;
        for(const auto& m:current->semantic_analysis()->modules()) if(m.identity==id) source=&m.source;
        if(!source) return empty();
        auto offset=Positions(*source).offset(p);
        if(!offset) throw std::invalid_argument("Position is outside the document or inside a surrogate pair");
        auto definitions=index->definitions_at(id,*offset);
        if(definitions.empty())for(const auto& declaration:index->declarations_in(id))
            if(declaration.source.begin<=*offset && *offset<declaration.source.end){definitions.push_back(declaration);break;}
        if(definitions.empty())return empty();
        j::Array locations;std::set<std::tuple<std::string,std::size_t,std::size_t>> seen;
        auto append=[&](const vd::SourceSpan& s){if(seen.emplace(s.module_identity,s.begin,s.end).second)locations.push_back(location(s));};
        for(const auto& definition:definitions) {
            if((include || method=="textDocument/definition") && !definition.builtin)append(definition.source);
            if(method=="textDocument/references")for(const auto& ref:index->references_to(definition.identity))append(ref.source);
        }
        if(method=="textDocument/definition"){if(locations.empty())return nullptr;if(locations.size()==1)return std::move(locations.front());}
        return locations;
    }
    static std::string name_of(const vd::AnalyzedDeclaration& d) {
        return d.source.source->text().substr(d.source.begin,d.source.end-d.source.begin);
    }
    static int symbol_kind(const vd::AnalyzedDeclaration& d) {
        if(d.editor_symbol_kind)return d.editor_symbol_kind;
        if(d.identity.find("::variant::")!=std::string::npos)return 22;
        using K=vd::AnalyzedDeclaration::Kind;
        switch(d.kind){case K::import_alias:return 2;case K::function:return 12;case K::operator_function:return 25;case K::receiver_method:case K::initializer:case K::interface_requirement:return 6;
        case K::module_constant:case K::local_constant:return 14;case K::value_structure:case K::managed_structure:return 23;
        case K::interface_type:return 11;case K::value_field:case K::managed_field:return 8;case K::type_parameter:return 26;default:return 13;}
    }
    static std::string describe(const vd::AnalyzedDeclaration& d) {
        if(!d.signature.empty())return d.signature;
        if(d.identity.ends_with("::$module"))return "module "+d.module;
        auto label=name_of(d);using K=vd::AnalyzedDeclaration::Kind;
        if(d.kind==K::function || d.kind==K::receiver_method || d.kind==K::initializer || d.kind==K::interface_requirement){
            label+="(";for(std::size_t n=0;n<d.parameters.size();++n){if(n)label+=", ";
                if(n<d.written_parameters.size())label+=d.written_parameters[n].name+": ";label+=vd::ir::type_name(d.parameters[n]);if(n<d.written_parameters.size() && d.written_parameters[n].default_source)label+=" = …";}
            label+=") -> ";
        }else label+=": ";
        label+=vd::ir::type_name(d.type);
        if(d.receiver_mutates)label+=" (mutating receiver)";
        if(d.kind==K::module_constant || d.kind==K::local_constant || d.type.readonly || d.readonly)label="const "+label;
        if(d.kind==K::value_field || d.kind==K::managed_field)label+=" (declared by "+d.owner+")";
        if(d.visibility=="private" || d.visibility=="internal")label=d.visibility+" "+label;
        if(d.value && vd::numeric::integer(d.type)) {
            auto bits=*d.value;auto width=vd::numeric::width(d.type);
            if(vd::numeric::signed_integer(d.type)) {
                if(width<64 && (bits & (std::uint64_t{1}<<(width-1))))bits|=~((std::uint64_t{1}<<width)-1);
                label+=" = "+std::to_string(std::bit_cast<std::int64_t>(bits));
            }else label+=" = "+std::to_string(bits);
        }
        return label;
    }
    const vd::SourceFile* source_in(const vd::ValidatedAnalysis& program,const std::string& id) const {
        for(const auto& m:program.modules())if(m.identity==id)return &m.source;return nullptr;
    }
    j::Object semantic_tokens(const j::Object& params) {
        auto id=identity(string(object(params.get("textDocument")),"uri"));
        auto document=open.find(id);
        j::Object result{{"data",j::Array{}}};
        if(!semantic_full || document==open.end() || !document->second.source)return result;
        semantic_requested=true;
        auto current=session.current_analysis();
        if(!current || !current->index() || session.requires_reselection())return result;
        const auto* source=source_in(*current->semantic_analysis(),id);
        if(!source || source->text()!=document->second.source->text())return result;
        Positions positions(*source);
        vd::lsp::Position previous;
        auto& data=*result.getArray("data");
        for(const auto& token:current->index()->semantic_tokens(id)) {
            auto begin=positions.position(token.begin),end=positions.position(token.end);
            if(!begin || !end || begin->line!=end->line || begin->character>=end->character)continue;
            auto line=begin->line-previous.line;
            data.push_back(line);
            data.push_back(line ? begin->character : begin->character-previous.character);
            data.push_back(end->character-begin->character);
            data.push_back(static_cast<unsigned>(token.kind));
            data.push_back(token.modifiers);
            previous=*begin;
        }
        return result;
    }
    bool disk_current(const vd::ValidatedAnalysis& program) const {
        if(manifest_mode) {
            auto current=session.current_analysis();auto snapshot=session.select();
            auto checked=vd::analyze_manifest_project(requested_root,snapshot.entry_override(),snapshot.overrides(),snapshot.manifests());
            if(!checked.project || !current || !current->project() || !current->project()->project || checked.project->selection_key()!=current->project()->project->selection_key())return false;
        }
        for(const auto& m:program.modules())if(!open.contains(m.identity)) {
            try {if(vd::read_source(vd::utf8_path(m.source.path())).text()!=m.source.text())return false;}
            catch(const std::exception&){return false;}
        }
        return true;
    }
    j::Value editor_query(const std::string& method,const j::Object& params) {
        auto current=session.current_analysis();auto index=session.current_index();
        if(method=="workspace/symbol" || method=="textDocument/documentSymbol") {
            j::Array result;if(!index)return result;
            std::set<std::tuple<std::string,std::size_t,std::size_t>> seen;
            std::string selected,filter;
            if(method=="workspace/symbol")filter=string(params,"query");
            else selected=identity(string(object(params.get("textDocument")),"uri"));
            for(const auto& d:index->declarations()) {
                if(!selected.empty() && selected!=d.module)continue;
                if(d.module.starts_with("/@stdlib/") && d.visibility!="public")continue;
                if(d.builtin || d.visibility=="local" || (d.operator_glyph.empty() && !vd::valid_editor_identifier(name_of(d))))continue;
                if(name_of(d).find(filter)==std::string::npos)continue;
                if(!seen.emplace(d.module,d.source.begin,d.source.end).second)continue;
                if(method=="textDocument/documentSymbol" && hierarchical_symbols){
                    result.push_back(j::Object{{"name",name_of(d)},{"detail",describe(d)},{"kind",symbol_kind(d)},
                        {"range",range(d.extent.value_or(d.source))},{"selectionRange",range(d.source)}});
                    if(result.size()==256)break;continue;
                }
                // Flat SymbolInformation is supported by all clients, including
                // those that do not negotiate hierarchical document symbols.
                result.push_back(j::Object{{"name",name_of(d)},{"kind",symbol_kind(d)},{"location",location(d.source)},{"containerName",d.owner}});
                if(result.size()==256)break;
            }
            return result;
        }
        auto module=identity(string(object(params.get("textDocument")),"uri"));
        if(manifest_mode && module.ends_with("/void.project.toml"))return method=="textDocument/completion" || method=="textDocument/formatting"?j::Value(j::Array{}):j::Value(nullptr);
        const vd::SourceFile* source=nullptr;
        auto snapshot=session.select();
        if(auto found=snapshot.overrides().find(module);found!=snapshot.overrides().end() && found->second)source=&*found->second;
        if(!source && current && current->semantic_analysis())source=source_in(*current->semantic_analysis(),module);
        if(method=="textDocument/formatting") {
            if(read_only_source(module))return j::Array{};
            const auto& options=object(params.get("options"));
            const auto insert_spaces=options.getBoolean("insertSpaces");
            if(!insert_spaces || integer(options.get("tabSize"),1)!=4)throw std::invalid_argument("VOID formatting requires a tab size of four and an indentation style");
            j::Array result;if(!source || !index || !source_in(*current->semantic_analysis(),module) || !disk_current(*current->semantic_analysis()))return result;
            auto formatted=vd::format_source(*source,*insert_spaces);if(!formatted || *formatted==source->text())return result;
            // The same compiler checks both lexical structure and the entire
            // proposed graph before returning a conservative whole-file edit.
            auto overlays=snapshot.overrides();for(const auto& m:current->semantic_analysis()->modules())if(!read_only_source(m.identity))overlays.insert_or_assign(m.identity,m.source);
            overlays.insert_or_assign(module,vd::SourceFile(source->path(),*formatted));
            auto checked=check_overlays(overlays);
            if(!checked.program || checked.program->compatibility_contract()!=current->semantic_analysis()->compatibility_contract())return result;
            auto again=vd::format_source(vd::SourceFile(source->path(),*formatted),*insert_spaces);
            if(!again || *again!=*formatted || !session.is_current(*current) || !disk_current(*current->semantic_analysis()))return result;
            result.push_back(j::Object{{"range",range({std::make_shared<const vd::SourceFile>(*source),module,0,source->text().size()})},{"newText",*formatted}});
            return result;
        }
        const auto& point=object(params.get("position"));
        vd::lsp::Position p{static_cast<std::uint32_t>(integer(point.get("line"),0)),static_cast<std::uint32_t>(integer(point.get("character"),0))};
        if(!source)return method=="textDocument/completion"?j::Value(j::Array{}):j::Value(nullptr);
        auto offset=Positions(*source).offset(p);if(!offset)throw std::invalid_argument("Invalid UTF-16 position");
        if(method=="textDocument/completion" || method=="textDocument/signatureHelp") {
            auto overlays=snapshot.overrides();
            if(module.starts_with("/@stdlib/"))return method=="textDocument/completion"?j::Value(j::Array{}):j::Value(nullptr);
            if(current && current->semantic_analysis())for(const auto& m:current->semantic_analysis()->modules())if(!read_only_source(m.identity))overlays.insert_or_assign(m.identity,m.source);
            if(vd::package_source(module)) {
                if(!current || !current->semantic_analysis() || !disk_current(*current->semantic_analysis()))
                    return method=="textDocument/completion"?j::Value(j::Array{}):j::Value(nullptr);
            } else overlays.insert_or_assign(module,*source); // Position conversion and probe share exact bytes.
            auto assistance=manifest_mode?vd::project_editor_assistance(requested_root,snapshot.entry_override(),overlays,snapshot.manifests(),module,*offset):vd::editor_assistance(entry_path,overlays,module,*offset);
            if(method=="textDocument/signatureHelp") {
                if(assistance.signature.empty() || assistance.active_parameter>=assistance.parameter_documentation.size() || assistance.parameter_documentation[assistance.active_parameter].empty())return nullptr;
                j::Array parameters;for(std::size_t n=0;n<assistance.parameters.size();++n) {
                    j::Object parameter{{"label",assistance.parameters[n]}};
                    if(n==assistance.active_parameter) {
                        if(signature_plaintext)parameter["documentation"]=j::Object{{"kind","plaintext"},{"value",assistance.parameter_documentation[n]}};
                        else parameter["documentation"]=assistance.parameter_documentation[n];
                    }
                    parameters.push_back(std::move(parameter));
                }
                return j::Object{{"signatures",j::Array{j::Object{{"label",assistance.signature},{"parameters",std::move(parameters)}}}},{"activeSignature",0},{"activeParameter",assistance.active_parameter}};
            }
            j::Array items;for(const auto& c:assistance.candidates)items.push_back(j::Object{{"label",c.name},{"kind",c.kind},{"detail",c.detail},{"documentation",c.documentation},
                {"textEdit",j::Object{{"range",range({std::make_shared<const vd::SourceFile>(*source),module,assistance.begin,assistance.end})},{"newText",c.insert_text.empty()?c.name:c.insert_text}}}});
            if(assistance.import_context)return j::Object{{"isIncomplete",assistance.incomplete},{"items",std::move(items)}};
            return items;
        }
        if(!index)return nullptr;
        auto definitions=index->definitions_at(module,*offset);
        if(definitions.empty())for(const auto& d:index->declarations_in(module))if(d.source.begin<=*offset && *offset<d.source.end){definitions.push_back(d);break;}
        if(definitions.empty())return nullptr;
        auto* definition=&definitions.front();
        auto selected_span=definition->source;
        for(const auto& ref:index->references())if(!ref.implicit && ref.to==definition->identity && ref.source.module_identity==module && ref.source.begin<=*offset && *offset<ref.source.end){selected_span=ref.source;break;}
        if(method=="textDocument/hover") {
            std::string text;
            for(const auto& d:definitions){if(!text.empty())text+="\n\n";auto docs=d.documentation.text(d.type!=vd::ir::Type::none && (d.kind==vd::AnalyzedDeclaration::Kind::function || d.kind==vd::AnalyzedDeclaration::Kind::receiver_method || d.kind==vd::AnalyzedDeclaration::Kind::interface_requirement || d.kind==vd::AnalyzedDeclaration::Kind::operator_function));text+=describe(d)+(docs.empty()?"":"\n\n"+docs);}
            return j::Object{{"contents",j::Object{{"kind","plaintext"},{"value",std::move(text)}}},{"range",range(selected_span)}};
        }
        if(definitions.size()!=1)throw std::invalid_argument("Rename cannot choose between concrete specialization targets");
        if(read_only_source(module) || read_only_source(definition->module))throw std::invalid_argument("Installed standard and locked package sources are read-only");
        if(!rename_edits || !definition->rename_safe)throw std::invalid_argument("Rename requires version-safe documentChanges and a supported ordinary function, global, constant or uncaptured local binding");
        if(!disk_current(*current->semantic_analysis()))throw std::invalid_argument("Disk sources changed; refresh before rename");
        if(method=="textDocument/prepareRename")return j::Object{{"range",range(selected_span)},{"placeholder",name_of(*definition)}};
        auto replacement=string(params,"newName");if(!vd::valid_editor_identifier(replacement) || replacement=="entry" || replacement=="init" || replacement=="deinit" || replacement=="call")throw std::invalid_argument("Expected a non-reserved VOID identifier");
        std::map<std::string,std::vector<vd::SourceSpan>> edits;
        edits[definition->module].push_back(definition->source);
        for(const auto& r:index->references_to(definition->identity))edits[r.source.module_identity].push_back(r.source);
        for(auto& [id,spans]:edits){
            if(read_only_source(id))throw std::invalid_argument("Rename would edit a read-only source");std::sort(spans.begin(),spans.end(),[](const auto& a,const auto& b){return a.begin<b.begin;});
            spans.erase(std::unique(spans.begin(),spans.end(),[](const auto& a,const auto& b){return a.begin==b.begin && a.end==b.end;}),spans.end());
            std::size_t end=0;for(const auto& span:spans){if(span.begin<end || span.source->text().substr(span.begin,span.end-span.begin)!=name_of(*definition))throw std::invalid_argument("Uncertain or overlapping rename binding");end=span.end;}
        }
        auto translate=[&](const std::string& module,std::size_t offset) {
            std::ptrdiff_t delta=0;
            if(auto found=edits.find(module);found!=edits.end())for(const auto& span:found->second)if(span.end<=offset)delta+=static_cast<std::ptrdiff_t>(replacement.size())-static_cast<std::ptrdiff_t>(span.end-span.begin);
            return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(offset)+delta);
        };
        auto overlays=session.select().overrides();for(const auto& m:current->semantic_analysis()->modules()) {
            if(read_only_source(m.identity))continue;
            auto text=m.source.text();if(auto found=edits.find(m.identity);found!=edits.end())for(auto it=found->second.rbegin();it!=found->second.rend();++it)text.replace(it->begin,it->end-it->begin,replacement);
            overlays.insert_or_assign(m.identity,vd::SourceFile(m.source.path(),std::move(text)));
        }
        auto checked=check_overlays(overlays);if(!checked.program)throw std::invalid_argument("Rename fails compiler validation");
        vd::ToolingIndex fresh(checked.program);
        // Compare every declaration and written binding at its transformed source
        // position: valid compilation alone cannot detect legal rebinding.
        std::map<std::string,std::string> identities;
        for(const auto& d:index->declarations()) {
            if(d.builtin) {
                auto next=fresh.declaration(d.identity);
                if(!next || !next->builtin || next->type!=d.type || next->parameters!=d.parameters)throw std::invalid_argument("Rename changes a compiler-owned numeric declaration");
                identities.emplace(d.identity,d.identity);continue;
            }
            for(const auto& next:fresh.declarations())if(!next.builtin && d.module==next.module && translate(d.module,d.source.begin)==next.source.begin && d.kind==next.kind)identities.emplace(d.identity,next.identity);
        }
        if(identities.size()!=index->declarations().size() || fresh.declarations().size()!=index->declarations().size())throw std::invalid_argument("Rename changes declaration structure");
        using Binding=std::tuple<std::string,std::size_t,std::string>;
        std::set<Binding> expected,actual;
        for(const auto& r:index->references())if(!r.implicit){if(!identities.contains(r.to))throw std::invalid_argument("Uncertain reference target");expected.emplace(r.source.module_identity,translate(r.source.module_identity,r.source.begin),identities.at(r.to));}
        for(const auto& r:fresh.references())if(!r.implicit)actual.emplace(r.source.module_identity,r.source.begin,r.to);
        if(expected!=actual)throw std::invalid_argument("Rename would rebind a written reference");
        std::error_code root_error;
        auto current_root=fs::canonical(selected_root,root_error);
        if(root_error || current_root!=root)throw std::invalid_argument("Rename project root changed; restart the server");
        if(!session.is_current(*current) || !disk_current(*current->semantic_analysis()))throw std::invalid_argument("Rename snapshot changed");
        j::Array changes;for(const auto& [id,spans]:edits){j::Array replacements;for(const auto& span:spans)replacements.push_back(j::Object{{"range",range(span)},{"newText",replacement}});
            j::Value version=nullptr;if(auto doc=open.find(id);doc!=open.end())version=doc->second.version;
            changes.push_back(j::Object{{"textDocument",j::Object{{"uri",uri(id)},{"version",std::move(version)}}},{"edits",std::move(replacements)}});
        }
        return j::Object{{"documentChanges",std::move(changes)}};
    }

    void synchronize(const std::string& method,const j::Object& params) {
        const auto& document=object(params.get("textDocument")); auto id=identity(string(document,"uri"));
        if(read_only_source(id))return; // Navigation never grants overlay authority.
        auto found=open.find(id);
        if(method=="textDocument/didClose") {
            if(found==open.end()) throw std::invalid_argument("Document is not open");
            session.close(id,next(id)); open.erase(found);
        } else {
            auto version=integer(document.get("version"));
            std::string text;
            if(method=="textDocument/didOpen") {
                if(found!=open.end()) throw std::invalid_argument("Document is already open");
                if(!(manifest_mode && id.ends_with("/void.project.toml")) && string(document,"languageId")!="void") throw std::invalid_argument("Expected VOID languageId 'void'");
                if(open.size()>=4096) throw std::invalid_argument("Too many open documents");
                text=string(document,"text");
                auto selected=session.select();const auto& overrides=manifest_mode && id.ends_with("/void.project.toml")?selected.manifests():selected.overrides();
                if(manifest_mode && overrides.contains(id))session.replace(id,next(id),std::move(text));else session.open(id,next(id),std::move(text));
                open.emplace(id,Document{version,string(document,"uri"), (manifest_mode && id.ends_with("/void.project.toml")?session.select().manifests():session.select().overrides()).at(id)});
            } else {
                if(found==open.end()) throw std::invalid_argument("Document is not open");
                if(version<=found->second.version) throw std::invalid_argument("Stale or duplicate document version");
                auto changes=params.getArray("contentChanges");
                if(!changes) throw std::invalid_argument("Expected contentChanges array");
                auto selected=session.select();const auto& overrides=manifest_mode && id.ends_with("/void.project.toml")?selected.manifests():selected.overrides();
                text=overrides.at(id)?overrides.at(id)->text():std::string{};
                for(const auto& event:*changes) {
                    const auto& change=object(&event);
                    if(change.get("range") || change.get("rangeLength")) throw std::invalid_argument("Incremental document changes are unsupported");
                    text=string(change,"text");
                }
                session.replace(id,next(id),std::move(text)); found->second.version=version;
                found->second.source=(manifest_mode && id.ends_with("/void.project.toml")?session.select().manifests():session.select().overrides()).at(id);
            }
        }
        dirty=true;
    }
public:
    void cancel_queued(const j::Value& id) {
        if(!id.getAsString())try{(void)integer(&id);}catch(const std::invalid_argument&){return;}
        std::string key;llvm::raw_string_ostream out(key);out<<id;cancellations.insert(key);
    }
    void end_batch(){cancellations.clear();}
    bool pending() const {return dirty;}
    void flush() {if(dirty)analyze();}
    bool parent_alive() const {
        if(!process_id)return true;
#ifdef _WIN32
        auto process=OpenProcess(SYNCHRONIZE,FALSE,static_cast<DWORD>(process_id));
        if(!process)return GetLastError()==ERROR_ACCESS_DENIED;
        auto status=WaitForSingleObject(process,0);CloseHandle(process);return status!=WAIT_OBJECT_0;
#else
        return ::kill(static_cast<pid_t>(process_id),0)==0 || errno!=ESRCH;
#endif
    }
    explicit Server(const fs::path& entry,bool project=false,const std::optional<std::string>& override={}) : root(fs::canonical(project?entry:fs::absolute(entry).parent_path())),
        selected_root(project?root:fs::absolute(entry).parent_path().lexically_normal()),entry_path(project?fs::path{}:root/entry.filename()),requested_root(project?fs::absolute(entry):fs::absolute(entry).parent_path()),
        session(project?vd::ToolingSession(vd::ProjectSession{entry,override}):vd::ToolingSession(entry_path)),manifest_mode(project) {}
    std::optional<int> dispatch(j::Value message) {
        auto o=message.getAsObject();
        if(!o) { error(nullptr,-32600,"Invalid JSON-RPC request"); return {}; }
        auto id=o->get("id"); bool valid_id=false;
        if(id) {
            try { if(!id->getAsString()) (void)integer(id); valid_id=true; }
            catch(const std::invalid_argument&) {}
        }
        if(o->getString("jsonrpc")!="2.0") { error(valid_id ? *id : j::Value(nullptr),-32600,"Invalid JSON-RPC version"); return {}; }
        if(!o->get("method") && (o->get("result") || o->get("error"))) {
            if(semantic_refresh_pending && o->getString("id")==semantic_refresh_id) {
                semantic_refresh_pending=false;
                refresh_semantics();
                return {};
            }
            std::cerr<<"Ignoring unsolicited client response\n"; return {};
        }
        if(id && !valid_id) { error(nullptr,-32600,"Invalid LSP request ID"); return {}; }
        auto method=o->getString("method");
        if(!method) {
            error(id ? *id : j::Value(nullptr),-32600,"Request requires a method string"); return {};
        }
        auto name=method->str();
        auto fail=[&](int code,const std::string& text) { if(id) error(*id,code,text); else std::cerr<<name<<": "<<text<<'\n'; };
        if(name=="exit") {
            if(!id) return state==State::shutdown ? 0 : 1;
            fail(-32600,"Exit must be a notification"); return {};
        }
        if(state==State::shutdown) { fail(-32600,"Server has shut down"); return {}; }
        if(state==State::initial && name!="initialize") { fail(-32002,"Server is not initialized"); return {}; }
        try {
            if(id && state==State::ready && name!="shutdown") {
                std::string key;llvm::raw_string_ostream out(key);out<<*id;
                if(cancellations.erase(key)){fail(-32800,"Request cancelled");return {};}
                flush();
            }
            if(name=="initialize") {
                if(!id || state!=State::initial) { fail(-32600,"Initialize must be the first request and occur once"); return {}; }
                const auto& params=object(o->get("params"));
                const auto& caps=object(params.get("capabilities"));
                if(auto pid=params.get("processId");pid && !pid->getAsNull())process_id=integer(pid,1);
                if(auto td=caps.getObject("textDocument")) {
                    if(auto help=td->getObject("signatureHelp"))if(auto info=help->getObject("signatureInformation"))if(auto formats=info->getArray("documentationFormat"))
                        for(const auto& format:*formats)if(format.getAsString()=="plaintext")signature_plaintext=true;
                    if(auto symbols=td->getObject("documentSymbol"))hierarchical_symbols=symbols->getBoolean("hierarchicalDocumentSymbolSupport").value_or(false);
                    if(auto tokens=td->getObject("semanticTokens")) {
                        auto requests=tokens->getObject("requests");
                        auto formats=tokens->getArray("formats");
                        semantic_full=requests && formats &&
                            (requests->getBoolean("full").value_or(false) || requests->getObject("full")) &&
                            std::any_of(formats->begin(),formats->end(),[](const auto& f){return f.getAsString()=="relative";});
                    }
                }
                if(auto td=caps.getObject("textDocument")) if(auto pd=td->getObject("publishDiagnostics")) {
                    related_information=pd->getBoolean("relatedInformation").value_or(false);
                    diagnostic_versions=pd->getBoolean("versionSupport").value_or(false);
                }
                if(auto workspace=caps.getObject("workspace"))if(auto edit=workspace->getObject("workspaceEdit"))
                    rename_edits=edit->getBoolean("documentChanges").value_or(false) && (edit->getString("failureHandling")=="textOnlyTransactional" || (params.getObject("initializationOptions") && params.getObject("initializationOptions")->getBoolean("voidVersionedEdits").value_or(false)));
                if(auto workspace=caps.getObject("workspace"))if(auto tokens=workspace->getObject("semanticTokens"))
                    semantic_refresh=semantic_full && tokens->getBoolean("refreshSupport").value_or(false);
                state=State::waiting;
                j::Object capabilities{{"positionEncoding","utf-16"},{"textDocumentSync",j::Object{{"openClose",true},{"change",1},{"save",j::Object{{"includeText",false}}}}},
                        {"definitionProvider",true},{"referencesProvider",true},{"hoverProvider",true},{"completionProvider",j::Object{{"triggerCharacters",j::Array{"."," ","\"","/",":"}}}},
                        {"signatureHelpProvider",j::Object{{"triggerCharacters",j::Array{"(",","}}}},{"documentSymbolProvider",true},{"workspaceSymbolProvider",true},
                        {"documentFormattingProvider",true},{"renameProvider",rename_edits ? j::Value(j::Object{{"prepareProvider",true}}):j::Value(false)},
                        {"workspace",manifest_mode?j::Value(j::Object{{"fileOperations",j::Object{{"didCreate",j::Object{{"filters",j::Array{j::Object{{"scheme","file"},{"pattern",j::Object{{"glob","**/*"}}}}}}}},{"didRename",j::Object{{"filters",j::Array{j::Object{{"scheme","file"},{"pattern",j::Object{{"glob","**/*"}}}}}}}},{"didDelete",j::Object{{"filters",j::Array{j::Object{{"scheme","file"},{"pattern",j::Object{{"glob","**/*"}}}}}}}}}}}):j::Value(j::Object{})},{"experimental",j::Object{{"voidToolingProtocol",1},{"voidProjectProtocol",1},{"refreshProject",true}}}};
                if(semantic_full) {
                    j::Array types,modifiers;
                    for(auto type:semantic_types)types.push_back(type);
                    for(auto modifier:semantic_modifiers)modifiers.push_back(modifier);
                    capabilities["semanticTokensProvider"]=j::Object{{"legend",j::Object{{"tokenTypes",std::move(types)},
                        {"tokenModifiers",std::move(modifiers)}}},{"full",true}};
                }
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",j::Object{
                    {"capabilities",std::move(capabilities)},{"serverInfo",j::Object{{"name","VOID"},{"version","0.2.0"}}}}}});
                return {};
            }
            if(name=="initialized") {
                if(id || state!=State::waiting) { fail(-32600,"Unexpected initialized message"); return {}; }
                (void)object(o->get("params")); state=State::ready; dirty=true; return {};
            }
            if(state!=State::ready) { fail(-32002,"Awaiting initialized notification"); return {}; }
            if(name=="shutdown") {
                if(!id) { fail(-32600,"Shutdown requires a request ID"); return {}; }
                if(auto p=o->get("params"); p && !p->getAsNull()) throw std::invalid_argument("Shutdown has no parameters");
                state=State::shutdown;dirty=false; send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",nullptr}}); return {};
            }
            if(name=="textDocument/didOpen" || name=="textDocument/didChange" || name=="textDocument/didClose") {
                if(id) { fail(-32600,"Document synchronization uses notifications"); return {}; }
                synchronize(name,object(o->get("params"))); return {};
            }
            if(name=="textDocument/semanticTokens/full") {
                if(!id){fail(-32600,"Semantic tokens require a request");return {};}
                auto result=semantic_tokens(object(o->get("params")));
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",std::move(result)}});return {};
            }
            if(name=="textDocument/definition" || name=="textDocument/references") {
                if(!id) { fail(-32600,"Navigation requires a request"); return {}; }
                auto result=query(name,object(o->get("params")));
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",std::move(result)}}); return {};
            }
            if(name=="textDocument/hover" || name=="textDocument/completion" || name=="textDocument/signatureHelp" || name=="textDocument/documentSymbol" || name=="workspace/symbol" || name=="textDocument/formatting" || name=="textDocument/prepareRename" || name=="textDocument/rename") {
                if(!id){fail(-32600,"Editor query requires a request");return {};}
                auto result=editor_query(name,object(o->get("params")));
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",std::move(result)}});return {};
            }
            if(name=="void/projectState" || name=="void/reselectProject") {
                if(!manifest_mode){fail(-32601,"Manifest project mode is not selected");return {};}
                if(!id){fail(-32600,"Project control requires a request");return {};}
                if(name=="void/reselectProject") {
                    const auto& params=object(o->get("params"));std::optional<std::string> entry;
                    if(auto value=params.get("entry");value && !value->getAsNull())entry=string(params,"entry");
                    session.reselect(entry);analyze();
                }
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",summary()}});return {};
            }
            if(name=="void/refreshProject") {
                session.refresh();analyze();if(id)send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",nullptr}});return {};
            }
            if(name=="textDocument/didSave") {
                if(id){fail(-32600,"Save requires a notification");return {};}
                auto module=identity(string(object(object(o->get("params")).get("textDocument")),"uri"));
                if(read_only_source(module))return {};
                if(!open.contains(module))throw std::invalid_argument("Saved document is not open");
                session.refresh();dirty=true;return {};
            }
            if(manifest_mode && (name=="workspace/didCreateFiles" || name=="workspace/didDeleteFiles" || name=="workspace/didRenameFiles")) {
                if(id){fail(-32600,"File operations require notifications");return {};}
                const auto files=object(o->get("params")).getArray("files");if(!files || files->size()>4096)throw std::invalid_argument("Expected bounded file operations");
                for(const auto& file:*files) {
                    const auto& event=object(&file);std::string old;
                    try{old=identity(string(event,name=="workspace/didRenameFiles"?"oldUri":"uri"));}catch(const std::invalid_argument&){continue;}
                    if(read_only_source(old))continue;
                    if(name=="workspace/didCreateFiles") {
                        auto selected=session.select();const auto& overrides=old.ends_with("/void.project.toml")?selected.manifests():selected.overrides();
                        if(auto found=overrides.find(old);found!=overrides.end() && !found->second && !open.contains(old))session.close(old,next(old));
                    } else {
                        auto snapshot=session.select();const auto& overrides=old.ends_with("/void.project.toml")?snapshot.manifests():snapshot.overrides();
                        std::optional<vd::SourceFile> source;
                        if(auto found=overrides.find(old);found!=overrides.end())source=found->second;
                        if(!source)if(auto document=open.find(old);document!=open.end())source=document->second.source;
                        std::string next_id;
                        if(name=="workspace/didRenameFiles")try{next_id=identity(string(event,"newUri"));}catch(const std::invalid_argument&){}
                        if(!next_id.empty() && !read_only_source(next_id) && next_id!=old && source) {
                            auto selected=session.select();const auto& next_overrides=next_id.ends_with("/void.project.toml")?selected.manifests():selected.overrides();
                            if(next_overrides.contains(next_id) || open.contains(next_id))throw std::invalid_argument("Rename would replace an editor override");
                            auto revision=next(next_id); // Prove both counters before mutation.
                            auto removed=next(old);session.open(next_id,revision,source->text());session.unavailable(old,removed);
                        } else if(next_id!=old)session.unavailable(old,next(old));
                    }
                }
                session.refresh();dirty=true;return {};
            }
            if(name=="workspace/didChangeWatchedFiles") {
                if(id){fail(-32600,"Watch changes require a notification");return {};}
                auto changes=object(o->get("params")).getArray("changes");if(!changes)throw std::invalid_argument("Expected watched changes");
                bool refresh=false;for(const auto& change:*changes)try{const auto& event=object(&change);auto event_uri=string(event,"uri");(void)vd::lsp::uri_path(event_uri);auto type=integer(event.get("type"),1);if(type>3)throw std::invalid_argument("Invalid watch type");
                    if(manifest_mode) {
                        refresh=true; // Unknown/wrong-case boundary names still invalidate selection.
                        auto module=identity(event_uri);
                        if(read_only_source(module))continue;
                        if(type==3)session.unavailable(module,next(module));
                        if(type==1) {auto selected=session.select();const auto& overrides=module.ends_with("/void.project.toml")?selected.manifests():selected.overrides();if(auto found=overrides.find(module);found!=overrides.end() && !found->second && !open.contains(module))session.close(module,next(module));}
                    }
                    else (void)identity(event_uri);
                    refresh=true;}catch(const std::invalid_argument&){}
                if(refresh){session.refresh();dirty=true;}return {};
            }
            if(name=="$/cancelRequest") {
                if(id){fail(-32600,"Cancellation requires a notification");return {};}
                auto cancelled=object(o->get("params")).get("id");if(!cancelled)throw std::invalid_argument("Missing cancelled request ID");
                if(!cancelled->getAsString())(void)integer(cancelled);
                return {}; // Unknown/completed requests are never remembered for future ID reuse.
            }
            if(name=="$/setTrace")return {};
            fail(-32601,"Method not found");
        } catch(const std::invalid_argument& failure) { fail(-32602,failure.what()); }
          catch(const std::exception& failure) {
            std::cerr<<"Internal language server failure: "<<failure.what()<<'\n';
            fail(-32603,"Internal server error"); return 2;
        }
        return {};
    }
    int eof() const { return state==State::shutdown ? 0 : 1; }
};
int preflight(const fs::path& workspace,const fs::path& active,const fs::path& explicit_entry) {
    auto selected=vd::discover_project(workspace,active,explicit_entry);
    j::Value result=j::Object{{"entry",vd::path_utf8(selected.entry)},{"root",vd::path_utf8(selected.root)},{"standalone",selected.standalone}};
    llvm::outs()<<result<<"\n";return 0;
}
int project_preflight(const fs::path& workspace,const fs::path& active,const fs::path& root,const fs::path& entry) {
    auto selected=vd::discover_project_context(workspace,active,root,entry);
    j::Object summary=selected.manifest_mode?project_summary(selected.root,selected.analysis,0,false):j::Object{{"schema",2},{"mode","legacy"},{"root",vd::path_utf8(selected.root)},{"entry",vd::path_utf8(*selected.entry)},{"standalone",selected.standalone},{"status","ready"}};
    if(selected.manifest_mode)summary["requestedRoot"]=vd::path_utf8(root.empty()?selected.root:fs::absolute(root));
    std::string text;llvm::raw_string_ostream stream(text);stream<<j::Value(std::move(summary));stream.flush();
    if(text.size()+1>65536)throw std::invalid_argument("Project discovery summary exceeds the 64 KiB protocol limit");
    llvm::outs()<<text<<"\n";return 0;
}
int creation_preflight(const fs::path& workspace,const fs::path& active) {
    auto selected=vd::project::discover_creation(workspace,active);
    j::Array sources;
    for(const auto& id:selected.sources)sources.push_back(id);
    llvm::outs()<<j::Value(j::Object{{"schema",1},{"creation",j::Object{
        {"root",vd::path_utf8(selected.root.path)},{"rootIdentity",selected.root.identity},
        {"manifestPresent",selected.manifest_present},{"complete",selected.complete},
        {"eligibleCount",static_cast<std::int64_t>(selected.eligible_count)},{"sources",std::move(sources)}}}})<<"\n";
    return 0;
}
vd::project::CreationOptions creation_options(std::string_view kind,const std::string& name,const std::string& package_name) {
    vd::project::CreationOptions options;
    if(kind!="application" && kind!="library")throw std::invalid_argument("Invalid creation kind.");
    options.kind=kind=="library"?vd::project::Kind::library:vd::project::Kind::application;
    if(!name.empty())options.name=name;
    if(!package_name.empty())options.package_name=package_name;
    return options;
}
int creation_plan(const fs::path& root,const std::string& kind,const std::string& name,const std::string& package_name) {
    std::optional<vd::project::CreationPlan> preview;
    try {
        preview=vd::project::plan_creation(root,creation_options(kind,name,package_name));
    } catch(const vd::project::CreationCollision&) {
        auto selected=vd::project::inspect_creation_root(root);
        llvm::outs()<<j::Value(j::Object{{"schema",1},{"root",vd::path_utf8(selected.path)},
            {"rootIdentity",selected.identity},{"status","collision"}})<<"\n";
        return 0;
    }
    const auto& plan=*preview;
    const auto& metadata=plan.manifest->project();
    const auto& role=kind=="library"?*plan.manifest->package():*plan.manifest->application();
    llvm::outs()<<j::Value(j::Object{{"schema",1},{"root",vd::path_utf8(plan.root)},{"rootIdentity",plan.parent.identity},
        {"manifest",vd::path_utf8(plan.root/"void.project.toml")},{"name",metadata.name},{"kind",kind},
        {"version",metadata.version.text()},{"projectId",metadata.id},{"roleId",role.id},
        {"packageName",kind=="library"?role.name:""}})<<"\n";
    return 0;
}
int creation_write(const fs::path& root,const std::string& identity,const std::string& kind,const std::string& name,
                   const std::string& version,const std::string& project_id,const std::string& role_id,const std::string& package_name) {
    auto admit=[&] {
        auto current=vd::project::inspect_creation_root(root);
        if(current.path!=root || current.identity!=identity)throw std::invalid_argument("Project root changed after creation review.");
    };
    admit();
    auto options=creation_options(kind,name,package_name);
    options.name=name;options.version=version;options.project_id=project_id;
    if(kind=="application")options.application_id=role_id;else options.package_id=role_id;
    std::string status="created";
    try {
        auto plan=vd::project::plan_creation(root,options);
        if(plan.parent.identity!=identity)throw std::invalid_argument("Project root changed after creation review.");
        vd::project::publish_creation(plan);
    } catch(const vd::project::CreationCollision&) {admit();status="collision";}
    llvm::outs()<<j::Value(j::Object{{"schema",1},{"root",vd::path_utf8(root)},{"status",status}})<<"\n";
    return 0;
}
int run(const fs::path& entry,bool project=false,const std::optional<std::string>& override={}) {
#ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1) throw std::runtime_error("Cannot set binary protocol streams");
#endif
#ifdef SIGPIPE
    std::signal(SIGPIPE,SIG_IGN);
#endif
    Server server(entry,project,override);
    input.alive=[&]{return server.parent_alive();};
    unsigned batch=0;auto started=std::chrono::steady_clock::now();
    for(;;) {
        if(!server.parent_alive())return server.eof();
        if(server.pending() && (++batch>=64 || std::chrono::steady_clock::now()-started>std::chrono::milliseconds(50) || !input.ready(10))) {
            server.flush();batch=0;started=std::chrono::steady_clock::now();
        }
        if(!server.pending()){batch=0;started=std::chrono::steady_clock::now();}
        std::vector<std::optional<j::Value>> messages;
        std::size_t bytes=0;bool eof=false,framing_failure=false;
        do {
            std::optional<std::string> body;
            try {body=frame();}
            catch(const FramingError& failure){std::cerr<<failure.what()<<'\n';framing_failure=true;break;}
            if(!body){eof=true;break;}bytes+=body->size();
            if(!bounded_json(*body)){messages.push_back({});continue;}
            auto value=j::parse(*body);
            if(!value){std::cerr<<llvm::toString(value.takeError())<<'\n';messages.push_back({});}
            else messages.push_back(std::move(*value));
        }while(messages.size()<64 && bytes<max_body && input.ready(0));
        // Cancellation can overtake work only while it is queued. It never
        // mutates document order or suppresses a response to an accepted request.
        std::map<std::string,const j::Value*> queued;
        for(const auto& value:messages)if(value)if(auto o=value->getAsObject()) {
            if(auto id=o->get("id")){std::string key;llvm::raw_string_ostream out(key);out<<*id;queued[key]=id;}
            if(o->getString("method")=="$/cancelRequest")if(auto params=o->getObject("params"))if(auto id=params->get("id")) {
                std::string key;llvm::raw_string_ostream out(key);out<<*id;
                if(queued.contains(key))server.cancel_queued(*id);
            }
        }
        for(auto& value:messages) {
            if(!value){error(nullptr,-32700,"Parse error or JSON nesting limit exceeded");continue;}
            if(auto status=server.dispatch(std::move(*value)))return *status;
        }
        server.end_batch();
        if(framing_failure){if(!server.parent_alive())return server.eof();error(nullptr,-32700,"Invalid protocol framing");return 2;}
        if(eof){server.flush();return server.eof();}
    }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {
        auto executable=vd::path_utf8(fs::path(argv[0]));vd::configure_standard_library_root(vd::utf8_path(llvm::sys::fs::getMainExecutable(executable.c_str(),reinterpret_cast<void*>(&wmain))));
        auto text=[&](int index){auto utf8=fs::path(argv[index]).u8string();return std::string(utf8.begin(),utf8.end());};
        if(argc==4 && std::wstring_view(argv[1])==L"--discover-creation")return creation_preflight(fs::path(argv[2]),fs::path(argv[3]));
        if(argc==6 && std::wstring_view(argv[1])==L"--plan-project")return creation_plan(fs::path(argv[2]),text(3),text(4),text(5));
        if(argc==10 && std::wstring_view(argv[1])==L"--create-project")return creation_write(fs::path(argv[2]),text(3),text(4),text(5),text(6),text(7),text(8),text(9));
        if(argc==6 && std::wstring_view(argv[1])==L"--discover-project")return project_preflight(fs::path(argv[2]),fs::path(argv[3]),fs::path(argv[4]),fs::path(argv[5]));
        if((argc==3 || (argc==5 && std::wstring_view(argv[3])==L"--entry")) && std::wstring_view(argv[1])==L"--project")return run(fs::path(argv[2]),true,argc==5?std::optional(vd::path_utf8(fs::path(argv[4]))):std::nullopt);
        if(argc==5 && std::wstring_view(argv[1])==L"--discover")return preflight(fs::path(argv[2]),fs::path(argv[3]),fs::path(argv[4]));
        if(argc!=3 || std::wstring_view(argv[1])!=L"--entry") throw std::invalid_argument("Usage: voidls --entry <project entry.void>");
        return run(fs::path(argv[2]));
#else
int main(int argc,char** argv) {
    try {
        vd::configure_standard_library_root(vd::utf8_path(llvm::sys::fs::getMainExecutable(argv[0],reinterpret_cast<void*>(&vd::configure_standard_library_root))));
        if(argc==4 && std::string_view(argv[1])=="--discover-creation")return creation_preflight(vd::utf8_path(argv[2]),vd::utf8_path(argv[3]));
        if(argc==6 && std::string_view(argv[1])=="--plan-project")return creation_plan(vd::utf8_path(argv[2]),argv[3],argv[4],argv[5]);
        if(argc==10 && std::string_view(argv[1])=="--create-project")return creation_write(vd::utf8_path(argv[2]),argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9]);
        if(argc==6 && std::string_view(argv[1])=="--discover-project")return project_preflight(vd::utf8_path(argv[2]),vd::utf8_path(argv[3]),vd::utf8_path(argv[4]),vd::utf8_path(argv[5]));
        if((argc==3 || (argc==5 && std::string_view(argv[3])=="--entry")) && std::string_view(argv[1])=="--project")return run(vd::utf8_path(argv[2]),true,argc==5?std::optional(std::string(argv[4])):std::nullopt);
        if(argc==5 && std::string_view(argv[1])=="--discover")return preflight(vd::utf8_path(argv[2]),vd::utf8_path(argv[3]),vd::utf8_path(argv[4]));
        if(argc!=3 || std::string_view(argv[1])!="--entry") throw std::invalid_argument("Usage: voidls --entry <project entry.void>");
        return run(vd::utf8_path(argv[2]));
#endif
    } catch(const std::exception& failure) { std::cerr<<"voidls: "<<failure.what()<<'\n'; return 2; }
}
