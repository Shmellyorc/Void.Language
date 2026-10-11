#include "void/lsp.hpp"
#include "void/tooling.hpp"
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <iostream>
#include <map>
#include <set>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
namespace j=llvm::json;
namespace fs=std::filesystem;
using vd::lsp::Positions;
constexpr std::size_t max_body=8*1024*1024, max_header=8192;
struct FramingError : std::runtime_error { using std::runtime_error::runtime_error; };
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
            auto c=std::cin.get();
            if(c==EOF) {
                if(!consumed) return {};
                throw FramingError("Truncated protocol header");
            }
            if(++consumed>max_header) throw FramingError("Protocol header exceeds 8192 bytes");
            if(c=='\r') {
                if(std::cin.get()!='\n') throw FramingError("Protocol headers require CRLF");
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
    if(*length && !std::cin.read(body.data(),static_cast<std::streamsize>(*length))) throw FramingError("Truncated protocol body");
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
class Server {
    enum class State { initial, waiting, ready, shutdown } state=State::initial;
    struct Document { std::int64_t version; };
    fs::path root, selected_root;
    vd::ToolingSession session;
    std::map<std::string,Document> open;
    std::set<std::string> published;
    bool related_information=false, diagnostic_versions=false;
    std::string identity(const std::string& uri) const {
        auto path=vd::lsp::uri_path(uri);
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
        vd::require_module_identity(key); // Includes escape/canonical/case spelling policies.
        return key;
    }
    std::string uri(const std::string& id) const { return vd::lsp::file_uri(root/vd::utf8_path(id).relative_path()); }
    j::Object location(const vd::SourceSpan& span) const {
        return j::Object{{"uri",uri(span.module_identity)},{"range",range(span)}};
    }
    vd::DocumentRevision next(const std::string& id) const {
        auto snapshot=session.select(); auto found=snapshot.documents().find(id);
        auto revision=found==snapshot.documents().end() ? 0 : found->second;
        if(revision==UINT64_MAX) throw std::overflow_error("Document revision exhausted");
        return revision+1;
    }
    void analyze() {
        auto result=vd::ToolingSession::analyze(session.select());
        if(!session.accept(result) || !session.is_current(*result)) return;
        std::map<std::string,j::Array> groups;
        for(const auto& [id,doc]:open) { (void)doc; groups[id]; }
        for(const auto& id:published) groups[id]; // Clear removed/repaired graph nodes.
        if(result->result().program) for(const auto& module:result->result().program->modules()) groups[module.identity];
        for(const auto& d:result->result().diagnostics) {
            auto id=d.primary.module_identity;
            if(id.empty()) throw std::logic_error("Project diagnostic has no VFS owner");
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
        published.clear();
        for(auto& [id,diagnostics]:groups) {
            j::Object params{{"uri",uri(id)},{"diagnostics",std::move(diagnostics)}};
            if(auto doc=open.find(id); doc!=open.end() && diagnostic_versions) params["version"]=doc->second.version;
            // Retain only nonempty diagnostics: old empty/unreachable nodes need
            // no unbounded clearing history. Open and reachable nodes seed each run.
            if(!params.getArray("diagnostics")->empty()) published.insert(id);
            send(j::Object{{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},{"params",std::move(params)}});
        }
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
        for(const auto& m:current->result().program->modules()) if(m.identity==id) source=&m.source;
        if(!source) return empty();
        auto offset=Positions(*source).offset(p);
        if(!offset) throw std::invalid_argument("Position is outside the document or inside a surrogate pair");
        auto definition=index->definition_at(id,*offset);
        if(!definition) for(const auto& declaration:index->declarations_in(id))
            if(declaration.source.begin<=*offset && *offset<declaration.source.end) { definition=declaration; break; }
        if(!definition) return empty();
        if(method=="textDocument/definition") return location(definition->source);
        j::Array locations;
        if(include) locations.push_back(location(definition->source));
        for(const auto& ref:index->references_to(definition->identity)) locations.push_back(location(ref.source));
        return locations;
    }
    void synchronize(const std::string& method,const j::Object& params) {
        const auto& document=object(params.get("textDocument")); auto id=identity(string(document,"uri"));
        auto found=open.find(id);
        if(method=="textDocument/didClose") {
            if(found==open.end()) throw std::invalid_argument("Document is not open");
            session.close(id,next(id)); open.erase(found);
        } else {
            auto version=integer(document.get("version"));
            std::string text;
            if(method=="textDocument/didOpen") {
                if(found!=open.end()) throw std::invalid_argument("Document is already open");
                if(string(document,"languageId")!="void") throw std::invalid_argument("Expected VOID languageId 'void'");
                if(open.size()>=4096) throw std::invalid_argument("Too many open documents");
                text=string(document,"text"); session.open(id,next(id),std::move(text)); open.emplace(id,Document{version});
            } else {
                if(found==open.end()) throw std::invalid_argument("Document is not open");
                if(version<=found->second.version) throw std::invalid_argument("Stale or duplicate document version");
                auto changes=params.getArray("contentChanges");
                if(!changes) throw std::invalid_argument("Expected contentChanges array");
                text=session.select().overrides().at(id)->text();
                for(const auto& event:*changes) {
                    const auto& change=object(&event);
                    if(change.get("range") || change.get("rangeLength")) throw std::invalid_argument("Incremental document changes are unsupported");
                    text=string(change,"text");
                }
                session.replace(id,next(id),std::move(text)); found->second.version=version;
            }
        }
        analyze();
    }
public:
    explicit Server(const fs::path& entry) : root(fs::canonical(fs::absolute(entry).parent_path())),
        selected_root(fs::absolute(entry).parent_path().lexically_normal()),session(root/entry.filename()) { (void)vd::lsp::file_uri(root/entry.filename()); }
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
            if(name=="initialize") {
                if(!id || state!=State::initial) { fail(-32600,"Initialize must be the first request and occur once"); return {}; }
                const auto& params=object(o->get("params"));
                const auto& caps=object(params.get("capabilities"));
                if(auto td=caps.getObject("textDocument")) if(auto pd=td->getObject("publishDiagnostics")) {
                    related_information=pd->getBoolean("relatedInformation").value_or(false);
                    diagnostic_versions=pd->getBoolean("versionSupport").value_or(false);
                }
                state=State::waiting;
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",j::Object{
                    {"capabilities",j::Object{{"positionEncoding","utf-16"},{"textDocumentSync",j::Object{{"openClose",true},{"change",1}}},
                        {"definitionProvider",true},{"referencesProvider",true}}},{"serverInfo",j::Object{{"name","VOID"},{"version","0.1.0"}}}}}});
                return {};
            }
            if(name=="initialized") {
                if(id || state!=State::waiting) { fail(-32600,"Unexpected initialized message"); return {}; }
                (void)object(o->get("params")); state=State::ready; analyze(); return {};
            }
            if(state!=State::ready) { fail(-32002,"Awaiting initialized notification"); return {}; }
            if(name=="shutdown") {
                if(!id) { fail(-32600,"Shutdown requires a request ID"); return {}; }
                if(auto p=o->get("params"); p && !p->getAsNull()) throw std::invalid_argument("Shutdown has no parameters");
                state=State::shutdown; send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",nullptr}}); return {};
            }
            if(name=="textDocument/didOpen" || name=="textDocument/didChange" || name=="textDocument/didClose") {
                if(id) { fail(-32600,"Document synchronization uses notifications"); return {}; }
                synchronize(name,object(o->get("params"))); return {};
            }
            if(name=="textDocument/definition" || name=="textDocument/references") {
                if(!id) { fail(-32600,"Navigation requires a request"); return {}; }
                auto result=query(name,object(o->get("params")));
                send(j::Object{{"jsonrpc","2.0"},{"id",*id},{"result",std::move(result)}}); return {};
            }
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
int run(const fs::path& entry) {
#ifdef _WIN32
    if(_setmode(_fileno(stdin),_O_BINARY)==-1 || _setmode(_fileno(stdout),_O_BINARY)==-1) throw std::runtime_error("Cannot set binary protocol streams");
#endif
#ifdef SIGPIPE
    std::signal(SIGPIPE,SIG_IGN);
#endif
    Server server(entry);
    for(;;) {
        std::optional<std::string> body;
        try { body=frame(); }
        catch(const FramingError& failure) { std::cerr<<failure.what()<<'\n'; error(nullptr,-32700,"Invalid protocol framing"); return 2; }
        if(!body) return server.eof();
        if(!bounded_json(*body)) { error(nullptr,-32700,"JSON nesting limit exceeded"); continue; }
        auto message=j::parse(*body);
        if(!message) { std::cerr<<llvm::toString(message.takeError())<<'\n'; error(nullptr,-32700,"Parse error"); continue; }
        if(auto status=server.dispatch(std::move(*message))) return *status;
    }
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=3 || std::wstring_view(argv[1])!=L"--entry") throw std::invalid_argument("Usage: voidls --entry <project entry.void>");
        return run(fs::path(argv[2]));
#else
int main(int argc,char** argv) {
    try {
        if(argc!=3 || std::string_view(argv[1])!="--entry") throw std::invalid_argument("Usage: voidls --entry <project entry.void>");
        return run(vd::utf8_path(argv[2]));
#endif
    } catch(const std::exception& failure) { std::cerr<<"voidls: "<<failure.what()<<'\n'; return 2; }
}
