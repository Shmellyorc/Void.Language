// System terminfo facility, dynamically bound so ordinary linked executables do
// not require a new link archive. No source path/terminal database parser here.
struct Terminfo {
    void* library=nullptr;void** current=nullptr;
    int (*setup)(const char*,int,int*)=nullptr;void* (*select)(void*)=nullptr;
    int (*destroy)(void*)=nullptr;char* (*string)(const char*)=nullptr;
    int (*number)(const char*)=nullptr;int (*flag)(const char*)=nullptr;
    char* (*parameter)(const char*,...)=nullptr;
    char* (*safe_parameter)(int,int,const char*,...)=nullptr;
    int (*puts)(const char*,int,int(*)(int))=nullptr;
    std::mutex mutex;
    Terminfo(){
#ifdef __APPLE__
        const char* names[]={"/usr/lib/libncurses.5.4.dylib","/usr/lib/libncurses.dylib"};
#else
        const char* names[]={"libtinfo.so.6","libncursesw.so.6","libncurses.so.6"};
#endif
        for(auto name:names){library=dlopen(name,RTLD_LAZY|RTLD_LOCAL);if(library)break;}
        if(!library)return;
#define VD_TERM_LOAD(field,name) field=reinterpret_cast<decltype(field)>(dlsym(library,name))
        VD_TERM_LOAD(current,"cur_term");VD_TERM_LOAD(setup,"setupterm");VD_TERM_LOAD(select,"set_curterm");VD_TERM_LOAD(destroy,"del_curterm");VD_TERM_LOAD(string,"tigetstr");VD_TERM_LOAD(number,"tigetnum");VD_TERM_LOAD(flag,"tigetflag");VD_TERM_LOAD(parameter,"tparm");VD_TERM_LOAD(safe_parameter,"tiparm_s");VD_TERM_LOAD(puts,"tputs");
#undef VD_TERM_LOAD
    }
    // Match ordinary dynamic linkage: the system facility owns process-global
    // setupterm/tparm caches. Unloading it or calling global curses cleanup would
    // orphan those tables or destroy another host client's state. Only each
    // per-call TERMINAL is ours to release (below); the loader retains the DSO.
    ~Terminfo()=default;
    bool ready() const{return current && setup && select && destroy && string && number && flag && parameter && puts;}
};
Terminfo& terminfo(){static Terminfo facility;return facility;}
thread_local std::string* terminal_bytes=nullptr;
thread_local bool terminal_allocation_failed=false;
int collect_terminal(int byte) noexcept {try{terminal_bytes->push_back(static_cast<char>(byte));return byte;}catch(...){terminal_allocation_failed=true;return -1;}}
struct TermDescription {
    Terminfo& api=terminfo();std::unique_lock<std::mutex> lock{api.mutex};void* previous=nullptr;void* owned=nullptr;
    explicit TermDescription(int fd){
        const char* name=std::getenv("TERM");if(!api.ready() || !name || !*name || std::string_view(name)=="dumb" || std::string_view(name)=="unknown")return;
        previous=*api.current;api.select(nullptr);int error=0;errno=0;auto status=api.setup(name,fd,&error);auto code=errno;owned=*api.current;
        if(status<0 || error!=1){api.select(previous);if(owned && owned!=previous)api.destroy(owned);owned=nullptr;if(code==ENOMEM)throw std::bad_alloc();}
    }
    ~TermDescription(){if(owned){api.select(previous);if(owned!=previous)api.destroy(owned);}}
    std::string get(const char* name){if(!owned)return {};auto p=api.string(name);return !p || p==reinterpret_cast<char*>(-1)?std::string{}:std::string(p);}
    bool has(const char* name){return !get(name).empty();}
    std::string expand(std::string spec,int count=0,long a=0,long b=0){
        if(spec.empty())return {};
        if(count){auto p=api.safe_parameter?api.safe_parameter(count,0,spec.c_str(),static_cast<int>(a),static_cast<int>(b),0,0,0,0,0,0,0):api.parameter(spec.c_str(),a,b,0L,0L,0L,0L,0L,0L,0L);if(!p)throw std::invalid_argument("invalid terminal capability parameters");spec=p;}
        std::string bytes;terminal_bytes=&bytes;terminal_allocation_failed=false;
        auto status=api.puts(spec.c_str(),1,collect_terminal);terminal_bytes=nullptr;
        if(terminal_allocation_failed)throw std::bad_alloc();
        if(status<0)throw std::invalid_argument("invalid terminal capability");return bytes;
    }
    bool title_supported(){return owned && (api.flag("XT")==1 || (!get("tsl").empty() && (get("tsl")=="\033]2;") && !get("fsl").empty()));}
};
