    std::uint64_t capabilities(Stream stream) override {
        native_only();
#ifdef _WIN32
        struct Save{DWORD code=GetLastError();~Save(){SetLastError(code);}} save;
        auto endpoint=classify(stream,borrowed_[index(stream)]);
#else
        SavedError save;auto endpoint=classify(stream,borrowed_[index(stream)]);
#endif
        std::uint64_t result=endpoint.available()?(input(stream)?text_input:text_output):0;
#ifndef _WIN32
        auto access=fcntl(borrowed_[index(stream)],F_GETFL);
        if(access<0 || (input(stream)?(access&O_ACCMODE)==O_WRONLY:(access&O_ACCMODE)==O_RDONLY))return 0;
#ifdef O_PATH
        if(access&O_PATH)return 0;
#endif
#endif
        if(endpoint.kind!=Kind::terminal)return result;
        if(input(stream)){InputMode saved;if(!save_mode(saved))result|=key_input|input_modes;return result;}
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info{};if(GetConsoleScreenBufferInfo(borrowed_[index(stream)].handle,&info))result|=dimensions|color|clearing;
        CONSOLE_CURSOR_INFO cursor{};if(GetConsoleCursorInfo(borrowed_[index(stream)].handle,&cursor))result|=cursor_visibility;
        if((result&dimensions) && classify(Stream::input,borrowed_[0]).kind==Kind::terminal)result|=cursor_position;
        result|=title;DWORD mode=0;if(GetConsoleMode(borrowed_[index(stream)].handle,&mode) && (mode&ENABLE_PROCESSED_OUTPUT))result|=bell;
#else
        auto fd=borrowed_[index(stream)];winsize size{};
        if(ioctl(fd,TIOCGWINSZ,&size)==0 && size.ws_col && size.ws_row)result|=dimensions;
        TermDescription term(fd);if(!term.owned)return result;
        if(term.has("civis") && term.has("cnorm"))result|=cursor_visibility;
        if(term.api.number("colors")>=16 && term.has("setaf") && term.has("setab") && term.has("op"))result|=color;
        if(term.has("clear") && term.has("cup"))result|=clearing;
        if(term.title_supported())result|=title;
        if(term.has("bel"))result|=bell;
        termios mode{};struct stat in{},out{};
        bool response=(result&dimensions) && term.has("cup") && term.get("u7")=="\033[6n" && term.get("u6")=="\033[%i%d;%dR" &&
            tcgetattr(borrowed_[0],&mode)==0 && !(mode.c_lflag&(ICANON|ECHO)) &&
            fstat(borrowed_[0],&in)==0 && fstat(fd,&out)==0 && in.st_dev==out.st_dev && in.st_rdev==out.st_rdev &&
            (fcntl(borrowed_[0],F_GETFL)&O_ACCMODE)!=O_WRONLY;
        if(response)result|=cursor_position;
#endif
        return result;
    }
    TerminalResult present(Stream stream,Presentation operation,std::int64_t a,std::int64_t b,std::string_view text,const Request& request) override {
        TerminalResult result;auto fail=[&](Error error){result.transfer.error=error;return result;};
        if(auto error=stopped(request,*this);error)return fail(error);
        if(input(stream))return fail({ErrorKind::unsupported});
        auto caps=capabilities(stream);
#ifdef _WIN32
        struct Save{DWORD code=GetLastError();~Save(){SetLastError(code);}} save;
        auto handle=borrowed_[index(stream)].handle;CONSOLE_SCREEN_BUFFER_INFO info{};
        if(!(caps&text_output))return fail({ErrorKind::closed});
        if(classify(stream,borrowed_[index(stream)]).kind!=Kind::terminal)return fail({ErrorKind::unsupported});
        if(!GetConsoleScreenBufferInfo(handle,&info))return fail(windows_error(GetLastError()));
        auto columns=info.srWindow.Right-info.srWindow.Left+1,rows=info.srWindow.Bottom-info.srWindow.Top+1;
        switch(operation){
        case Presentation::size:if(columns<=0 || rows<=0)return fail({ErrorKind::io_failure});result.first=columns;result.second=rows;return result;
        case Presentation::cursor_position:if(!(caps&cursor_position))return fail({ErrorKind::unsupported});result.first=info.dwCursorPosition.X-info.srWindow.Left;result.second=info.dwCursorPosition.Y-info.srWindow.Top;if(result.first<0 || result.second<0 || result.first>=columns || result.second>=rows)return fail({ErrorKind::io_failure});return result;
        case Presentation::set_cursor:if(a<0 || b<0 || a>=columns || b>=rows)return fail({ErrorKind::invalid_argument});if(!SetConsoleCursorPosition(handle,{static_cast<SHORT>(a+info.srWindow.Left),static_cast<SHORT>(b+info.srWindow.Top)}))return fail(windows_error(GetLastError()));break;
        case Presentation::cursor_visible:{CONSOLE_CURSOR_INFO cursor{};if(!GetConsoleCursorInfo(handle,&cursor))return fail(windows_error(GetLastError()));cursor.bVisible=a!=0;if(!SetConsoleCursorInfo(handle,&cursor))return fail(windows_error(GetLastError()));break;}
        case Presentation::color:{auto map=[](std::int64_t n){return static_cast<WORD>((n&1?FOREGROUND_RED:0)|(n&2?FOREGROUND_GREEN:0)|(n&4?FOREGROUND_BLUE:0)|(n&8?FOREGROUND_INTENSITY:0));};if(!defaults_[index(stream)])defaults_[index(stream)]=info.wAttributes;if(!SetConsoleTextAttribute(handle,static_cast<WORD>((info.wAttributes&0xff00)|map(a)|(map(b)<<4))))return fail(windows_error(GetLastError()));break;}
        case Presentation::reset_color:if(!SetConsoleTextAttribute(handle,defaults_[index(stream)].value_or(info.wAttributes)))return fail(windows_error(GetLastError()));break;
        case Presentation::clear:for(int row=info.srWindow.Top;row<=info.srWindow.Bottom;++row){if(auto error=stopped(request,*this);error)return fail(error);DWORD done=0;COORD at{info.srWindow.Left,static_cast<SHORT>(row)};auto success=FillConsoleOutputCharacterW(handle,L' ',columns,at,&done);if(!success || done!=static_cast<DWORD>(columns))return fail(windows_error(success?ERROR_WRITE_FAULT:GetLastError()));success=FillConsoleOutputAttribute(handle,info.wAttributes,columns,at,&done);if(!success || done!=static_cast<DWORD>(columns))return fail(windows_error(success?ERROR_WRITE_FAULT:GetLastError()));}if(!SetConsoleCursorPosition(handle,{info.srWindow.Left,info.srWindow.Top}))return fail(windows_error(GetLastError()));break;
        case Presentation::title:{if(text.size()>static_cast<std::size_t>(INT_MAX))throw std::length_error("title extent");int count=0;if(!text.empty()){count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);if(!count)return fail(windows_error(GetLastError()));}std::wstring wide(count,L'\0');if(count && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),wide.data(),count))return fail(windows_error(GetLastError()));if(!SetConsoleTitleW(wide.c_str()))return fail(windows_error(GetLastError()));break;}
        case Presentation::bell:if(!(caps&bell))return fail({ErrorKind::unsupported});result.sequence={'\a'};break;
        }
#else
        SavedError save;auto fd=borrowed_[index(stream)];auto endpoint=classify(stream,fd);if(!endpoint.available())return fail(native_error(static_cast<int>(endpoint.os_code)));if(endpoint.kind!=Kind::terminal)return fail({ErrorKind::unsupported});
        if(operation==Presentation::size || operation==Presentation::set_cursor || operation==Presentation::cursor_position){winsize size{};if(ioctl(fd,TIOCGWINSZ,&size)<0)return fail(native_error(errno));if(!size.ws_col || !size.ws_row)return fail({ErrorKind::unsupported});result.first=size.ws_col;result.second=size.ws_row;if(operation==Presentation::size)return result;if(operation==Presentation::set_cursor && (a<0 || b<0 || a>=size.ws_col || b>=size.ws_row))return fail({ErrorKind::invalid_argument});}
        TermDescription term(fd);if(!term.owned)return fail({ErrorKind::unsupported});std::string sequence;
        switch(operation){
        case Presentation::size:break;
        case Presentation::cursor_position:if(!(caps&cursor_position))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("u7"));result.response=true;break;
        case Presentation::set_cursor:if(!term.has("cup"))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("cup"),2,static_cast<long>(b),static_cast<long>(a));break;
        case Presentation::cursor_visible:if(!(caps&cursor_visibility))return fail({ErrorKind::unsupported});sequence=term.expand(term.get(a?"cnorm":"civis"));break;
        case Presentation::color:if(!(caps&color))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("setaf"),1,static_cast<long>(a))+term.expand(term.get("setab"),1,static_cast<long>(b));break;
        case Presentation::reset_color:if(!(caps&color))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("op"));break;
        case Presentation::clear:if(!(caps&clearing))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("clear"))+term.expand(term.get("cup"),2,0,0);break;
        case Presentation::title:if(!(caps&title))return fail({ErrorKind::unsupported});sequence="\033]2;";sequence.append(text);sequence+=term.api.flag("XT")==1?std::string("\a"):term.expand(term.get("fsl"));break;
        case Presentation::bell:if(!(caps&bell))return fail({ErrorKind::unsupported});sequence=term.expand(term.get("bel"));break;
        }
        result.sequence.assign(sequence.begin(),sequence.end());
#endif
        return result;
    }
