// One selector per full-source lexer. It never rewrites bytes or creates scope.
class ConditionalSelection {
    struct Branch {
        std::size_t indent,body=0,offset;
        bool parent,active,taken,otherwise=false,seen=false,valid=true;
        char style=0;
    };
    const SourceFile& source;Diagnostics& diagnostics;const BuildConditions& config;
    std::vector<ConditionalRegion>* regions;
    std::vector<Branch> branches;
    // Disabled text still has balanced lexical units. Cache only their source
    // extents, never tokens/AST/diagnostics. A lazy framing pass avoids per-line
    // lookahead through excluded text. Strings use the existing lexer.
    std::vector<std::pair<std::size_t,std::size_t>> continuations;
    bool framed=false;
    std::size_t inactive_until=0,inactive_owner=0;
    void frame_excluded(std::size_t begin,std::size_t end) {
        const auto& text=source.text();
        if(!framed) {
            framed=true;
            struct Open {char kind;std::size_t begin,line;bool continuation;};
            std::vector<Open> opens;std::size_t overflow=0;
            const auto& lines=source.line_starts();
            auto line=std::size_t(std::upper_bound(lines.begin(),lines.end(),begin)-lines.begin()-1);
            std::string_view previous;
            for(auto p=begin;p<text.size();) {
                while(line+1<lines.size() && lines[line+1]<=p)++line;
                const auto at=p;const auto c=text[p];
                if(c=='#'){p=text.find_first_of("\r\n",p);if(p==text.npos)p=text.size();continue;}
                if(c=='\r' || c=='\n'){previous={};++p;continue;}
                if(c==' ' || c=='\t'){++p;continue;}
                if(c=='"') {
                    std::vector<InterpolationHole> holes;std::vector<Token> chunks;
                    try {
                        (void)string_token(source,p,&holes,&chunks);
                        // A first-token compile line remains structural in
                        // disabled text, including between quote characters.
                        auto next=line+1;bool directive=false;
                        for(;next<lines.size() && lines[next]<p;++next) {
                            const auto prefix=indentation_at(text,lines[next]);
                            if(word(std::string_view(text).substr(prefix.end),"compile")) {p=lines[next];directive=true;break;}
                        }
                        if(!directive && next>line+1)continuations.emplace_back(at,p);
                    } catch(const CompilationError&) {
                        p=text.find_first_of("\r\n",at);if(p==text.npos)p=text.size();
                    }
                    previous={};continue;
                }
                if(letter(c)) {
                    while(p<text.size() && (letter(text[p]) || digit(text[p])))++p;
                    previous=std::string_view(text).substr(at,p-at);continue;
                }
                if(c=='(' || c=='[' || c=='{') {
                    if(opens.size()==256 || overflow)++overflow;
                    else opens.push_back({c,p,line,c=='(' || (c=='{' && previous!="defer")});
                } else if(c==')' || c==']' || c=='}') {
                    if(overflow)--overflow;
                    else if(!opens.empty() && ((c==')' && opens.back().kind=='(') ||
                        (c==']' && opens.back().kind=='[') || (c=='}' && opens.back().kind=='{'))) {
                        const auto open=opens.back();opens.pop_back();
                        if(open.continuation && line>open.line)continuations.emplace_back(open.begin,p+1);
                    }
                }
                previous={};++p;
            }
            std::sort(continuations.begin(),continuations.end());
        }
        auto next=std::lower_bound(continuations.begin(),continuations.end(),begin,
            [](const auto& extent,auto offset){return extent.first<offset;});
        for(;next!=continuations.end() && next->first<end;++next)if(next->second>end) {
            inactive_until=std::max(inactive_until,next->second);
            for(const auto& branch:branches)if(!branch.active){inactive_owner=branch.indent;break;}
        }
    }
    void report(std::size_t at,const std::string& message) {
        try {error(source,{TokenKind::name,"compile",at},Code::conditional,message);}
        catch(const CompilationError& failure){diagnostics.add(failure);}
    }
    void finish(const Branch& branch) {
        if(!branch.seen)report(branch.offset,"A compile branch requires an indented source region.");
    }
    static bool word(std::string_view text,std::string_view value) {
        return text.starts_with(value) && (text.size()==value.size() || (!letter(text[value.size()]) && !digit(text[value.size()])));
    }
public:
    ConditionalSelection(const SourceFile& s,Diagnostics& d,const BuildConditions& c,std::vector<ConditionalRegion>* r)
        :source(s),diagnostics(d),config(c),regions(r){}
    // Returns false for directive and excluded lines. Blank lines do not end regions.
    bool line(std::size_t start,Indentation& prefix,bool continuation) {
        const auto& text=source.text();auto end=text.find_first_of("\r\n",prefix.end);
        if(end==std::string::npos)end=text.size();
        auto content=std::string_view(text).substr(prefix.end,end-prefix.end);
        const bool compile_word=word(content,"compile");
        auto after_compile=compile_word?content.substr(7):std::string_view{};
        while(!after_compile.empty() && (after_compile.front()==' ' || after_compile.front()=='\t'))after_compile.remove_prefix(1);
        const bool directive=compile_word && !word(after_compile,"func");
        if(content.empty() || content.front()=='#')return true;
        auto body=directive?content.substr(7):std::string_view{};
        while(!body.empty() && (body.front()==' ' || body.front()=='\t'))body.remove_prefix(1);
        const bool alternative=directive && word(body,"else");
        if(directive && prefix.width<=inactive_owner)inactive_until=0;
        if(!directive && start<inactive_until)continuation=true;
        // A missing branch body cannot borrow a dedented call continuation.
        while(continuation && !branches.empty() && !branches.back().seen && prefix.width<=branches.back().indent) {
            finish(branches.back());branches.pop_back();
        }
        if((directive || !branches.empty()) && (!continuation || directive) && prefix.mixed!=std::string::npos)report(prefix.mixed,"Mixed tabs and spaces in conditional source indentation.");
        // Cosmetic indentation in ordinary continuations must not close a region.
        if(!continuation) {
            while(!branches.empty() && prefix.width<=branches.back().indent) {
                if(alternative && prefix.width==branches.back().indent)break;
                finish(branches.back());branches.pop_back();
            }
        }
        std::size_t removed=0;
        for(auto& branch:branches) {
            if(alternative && &branch==&branches.back() && prefix.width==branch.indent)break;
            if(!branch.seen){branch.body=prefix.width;branch.seen=true;}
            if(!continuation && branch.indent && prefix.style!=branch.style)report(prefix.end,"Inconsistent tabs and spaces in conditional source indentation.");
            if(!continuation && prefix.width<branch.body)report(prefix.end,"Conditional source indentation does not match its branch body.");
            removed+=branch.body-branch.indent;
        }
        const bool parent=branches.empty() || branches.back().active;
        if(directive) {
            if(regions)regions->push_back({prefix.end,end,true});
            if(continuation){report(prefix.end,"Compile directives require a source-region boundary outside expression continuations.");return false;}
            bool quoted=false;auto stop=content.size();
            for(std::size_t n=0;n<content.size();++n){if(content[n]=='"')quoted=!quoted;else if(content[n]=='#' && !quoted){stop=n;break;}}
            while(stop && (content[stop-1]==' ' || content[stop-1]=='\t'))--stop;
            bool valid=stop && content[stop-1]==':';
            const auto body_begin=std::size_t(body.data()-content.data());
            auto condition_begin=body_begin;
            bool otherwise=false;
            if(alternative) {
                if(branches.empty() || prefix.width!=branches.back().indent){report(prefix.end,"Compile else has no matching compile if at this indentation.");return false;}
                auto& branch=branches.back();finish(branch);
                if(branch.otherwise){report(prefix.end,"Compile else cannot follow compile else.");branch.valid=false;}
                condition_begin+=4;
                while(condition_begin<stop && (content[condition_begin]==' ' || content[condition_begin]=='\t'))++condition_begin;
                if(condition_begin<stop && word(content.substr(condition_begin),"if"))condition_begin+=2;
                else otherwise=true;
            }else if(word(body,"if"))condition_begin+=2;
            else valid=false;
            bool selected=false;
            if(!valid)report(prefix.end,"Expected compile if <condition>:, compile else if <condition>: or compile else:.");
            else if(otherwise) {
                auto rest=content.substr(condition_begin,stop-1-condition_begin);
                if(rest.find_first_not_of(" \t")!=rest.npos){report(prefix.end+condition_begin,"Unexpected text after compile else.");valid=false;}
                selected=true;
            }else {
                try {selected=evaluate_build_condition(source,prefix.end+condition_begin,prefix.end+stop-1,config);}
                catch(const CompilationError& failure){diagnostics.add(failure);valid=false;}
            }
            if(alternative) {
                auto& branch=branches.back();branch.valid &= valid;
                branch.active=branch.valid && branch.parent && !branch.taken && selected;
                branch.taken|=selected;branch.otherwise=otherwise;branch.seen=false;branch.body=0;branch.offset=prefix.end;
            }else {
                if(branches.size()>=64){report(prefix.end,"Conditional region nesting limit of 64 exceeded.");return false;}
                branches.push_back({prefix.width,0,prefix.end,parent,parent && valid && selected,selected,false,false,valid,prefix.style});
            }
            return false;
        }
        if(!parent) {
            frame_excluded(prefix.end,end);
            if(regions)regions->push_back({prefix.end,end,false});
            return false;
        }
        // A continued argument may be less indented than the enclosing region.
        prefix.width=prefix.width>=removed?prefix.width-removed:0;
        inactive_until=0;return true;
    }
    std::size_t width(std::size_t physical) const {
        std::size_t removed=0;for(const auto& branch:branches)if(branch.seen)removed+=branch.body-branch.indent;
        return physical>=removed?physical-removed:0;
    }
    void finish(){for(const auto& branch:branches)finish(branch);}
};
