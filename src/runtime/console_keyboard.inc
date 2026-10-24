#ifndef _WIN32
    static bool same_mode(const termios& a,const termios& b) {
        auto local_difference=a.c_lflag^b.c_lflag;
#ifdef __APPLE__
        // Darwin's tty ioctl adds PENDIN when returning to ICANON, so queued
        // raw input is reprocessed. It is kernel state, not a failed tcsetattr.
        // Do not clear it or flush input to manufacture a byte-identical mode.
        local_difference&=~static_cast<tcflag_t>(PENDIN);
#endif
        return a.c_iflag==b.c_iflag && a.c_oflag==b.c_oflag && a.c_cflag==b.c_cflag && !local_difference &&
            std::equal(std::begin(a.c_cc),std::end(a.c_cc),std::begin(b.c_cc)) && cfgetispeed(&a)==cfgetispeed(&b) && cfgetospeed(&a)==cfgetospeed(&b);
    }
#endif
    Error save_mode(InputMode& saved) override {
        auto endpoint=classify(Stream::input,borrowed_[0]);
#ifdef _WIN32
        if(!endpoint.available())return windows_error(static_cast<DWORD>(endpoint.os_code));
        if(endpoint.kind!=Kind::terminal)return {ErrorKind::unsupported};
        DWORD mode=0,events=0;if(!GetNumberOfConsoleInputEvents(borrowed_[0].handle,&events))return windows_error(GetLastError());if(!GetConsoleMode(borrowed_[0].handle,&mode))return windows_error(GetLastError());
        std::memcpy(saved.native.data(),&mode,sizeof mode);
#else
        if(!endpoint.available())return native_error(static_cast<int>(endpoint.os_code));
        if(endpoint.kind!=Kind::terminal)return {ErrorKind::unsupported};
        auto access=fcntl(borrowed_[0],F_GETFL);if(access<0)return native_error(errno);if((access&O_ACCMODE)==O_WRONLY)return {ErrorKind::unsupported};
        termios mode{};if(tcgetattr(borrowed_[0],&mode)<0)return errno==ENOTTY?Error{ErrorKind::unsupported,errno}:native_error(errno);
        static_assert(sizeof mode<=sizeof saved.native);std::memcpy(saved.native.data(),&mode,sizeof mode);
#endif
        return {};
    }
    Error apply_mode(const InputMode& saved,ModeOptions options) override {
#ifdef _WIN32
        DWORD mode=0;std::memcpy(&mode,saved.native.data(),sizeof mode);
        auto flag=[&](DWORD bit,bool on){if(on)mode|=bit;else mode&=~bit;};
        flag(ENABLE_ECHO_INPUT,options.echo);flag(ENABLE_LINE_INPUT,options.line_input);if(!options.preserve_interrupt)flag(ENABLE_PROCESSED_INPUT,!options.control_c_as_input);
        if(!SetConsoleMode(borrowed_[0].handle,mode))return windows_error(GetLastError());
        DWORD actual=0;if(!GetConsoleMode(borrowed_[0].handle,&actual))return windows_error(GetLastError());if(actual!=mode)return {ErrorKind::io_failure,ERROR_INVALID_DATA};
#else
        termios mode{};std::memcpy(&mode,saved.native.data(),sizeof mode);
        auto flag=[&](tcflag_t bit,bool on){if(on)mode.c_lflag|=bit;else mode.c_lflag&=~bit;};
        flag(ECHO,options.echo);if(!options.echo)mode.c_lflag&=~ECHONL;flag(ICANON,options.line_input);if(!options.preserve_interrupt)flag(ISIG,!options.control_c_as_input);
        if(!options.line_input){mode.c_cc[VMIN]=1;mode.c_cc[VTIME]=0;}
        if(tcsetattr(borrowed_[0],TCSANOW,&mode)<0)return native_error(errno);
        termios actual{};if(tcgetattr(borrowed_[0],&actual)<0)return native_error(errno);if(!same_mode(mode,actual))return {ErrorKind::io_failure,EIO};
#endif
        return {};
    }
    Error restore_mode(const InputMode& saved) override {
#ifdef _WIN32
        DWORD mode=0;std::memcpy(&mode,saved.native.data(),sizeof mode);if(!SetConsoleMode(borrowed_[0].handle,mode))return windows_error(GetLastError());
        DWORD actual=0;if(!GetConsoleMode(borrowed_[0].handle,&actual))return windows_error(GetLastError());if(actual!=mode)return {ErrorKind::io_failure,ERROR_INVALID_DATA};
#else
        termios mode{};std::memcpy(&mode,saved.native.data(),sizeof mode);if(tcsetattr(borrowed_[0],TCSANOW,&mode)<0)return native_error(errno);
        termios actual{};if(tcgetattr(borrowed_[0],&actual)<0)return native_error(errno);if(!same_mode(mode,actual))return {ErrorKind::io_failure,EIO};
#endif
        return {};
    }
    std::vector<std::pair<std::string,Key>> key_sequences() override {
        std::vector<std::pair<std::string,Key>> result;
#ifndef _WIN32
        TermDescription term(borrowed_[0]);if(!term.owned)return result;
        for(auto [name,key]:{std::pair{"kcuu1",Key::up},{"kcud1",Key::down},{"kcub1",Key::left},{"kcuf1",Key::right},{"khome",Key::home},{"kend",Key::end},{"kich1",Key::insert},{"kdch1",Key::delete_key},{"kpp",Key::page_up},{"knp",Key::page_down}}){auto bytes=term.get(name);if(!bytes.empty())result.emplace_back(std::move(bytes),key);}
        for(unsigned n=1;n<=12;++n){auto name="kf"+std::to_string(n);auto bytes=term.get(name.c_str());if(!bytes.empty())result.emplace_back(std::move(bytes),static_cast<Key>(static_cast<unsigned>(Key::f1)+n-1));}
        // A terminfo entry explicitly advertising the ECMA cursor report grammar
        // also identifies the normal (non-keypad-application) navigation protocol.
        // Never infer this protocol from TERM's name or from isatty alone.
        if(term.get("u6")=="\033[%i%d;%dR" && term.get("u7")=="\033[6n"){
            for(auto [seq,key]:{std::pair{"\033[A",Key::up},{"\033[B",Key::down},{"\033[C",Key::right},{"\033[D",Key::left},{"\033[H",Key::home},{"\033[F",Key::end},{"\033[2~",Key::insert},{"\033[3~",Key::delete_key},{"\033[5~",Key::page_up},{"\033[6~",Key::page_down},{"\033OP",Key::f1},{"\033OQ",Key::f2},{"\033OR",Key::f3},{"\033OS",Key::f4}})result.emplace_back(seq,key);
            const unsigned codes[]={15,17,18,19,20,21,23,24};for(unsigned n=0;n<8;++n)result.emplace_back("\033["+std::to_string(codes[n])+"~",static_cast<Key>(static_cast<unsigned>(Key::f5)+n));
        }
#endif
        return result;
    }
#ifdef _WIN32
    std::optional<TerminalResult> key_ready() override {
        TerminalResult result;DWORD count=0;if(!GetNumberOfConsoleInputEvents(borrowed_[0].handle,&count)){result.transfer.error=windows_error(GetLastError());return result;}
        if(!count)return result;
        std::vector<INPUT_RECORD> records(count);DWORD read=0;if(!PeekConsoleInputW(borrowed_[0].handle,records.data(),count,&read)){result.transfer.error=windows_error(GetLastError());return result;}
        for(DWORD n=0;n<read;++n)if(records[n].EventType==KEY_EVENT){result.first=1;break;}return result;
    }
    Transfer read_key(std::span<std::uint8_t> bytes,const Request& request) override {
        // Use the existing strict transcoder, including its partial byte/high
        // surrogate/replay carry. Physical metadata is attached to the same
        // InputBlock; switching APIs cannot create another native byte queue.
        auto carried=wide_.pending();std::optional<PhysicalKey> metadata;
        auto result=wide_.read(bytes,request,[&](std::span<char16_t> unit,const Request& next)->Transfer {
            auto handle=borrowed_[0].handle;
            for(;;){
                if(auto error=stopped(next,*this);error)return {0,false,error};
                DWORD wait=0;if(!next.polling){wait=50;if(next.deadline)wait=static_cast<DWORD>(std::min<std::int64_t>(50,std::chrono::ceil<std::chrono::milliseconds>(remaining(next,*this)).count()));}
                auto ready=WaitForSingleObject(handle,wait);if(ready==WAIT_TIMEOUT){if(next.polling)return {0,false,{ErrorKind::would_block}};continue;}if(ready!=WAIT_OBJECT_0)return {0,false,windows_error(GetLastError())};
                INPUT_RECORD record{};DWORD count=0;if(!ReadConsoleInputW(handle,&record,1,&count))return {0,false,windows_error(GetLastError())};if(!count)continue;
                if(record.EventType!=KEY_EVENT)continue; // OS mouse/window records have no character ownership.
                auto& event=record.Event.KeyEvent;PhysicalKey key;key.down=event.bKeyDown!=FALSE;key.known=true;key.down_known=true;key.repeat=std::max<unsigned>(1,event.wRepeatCount);
                auto flags=event.dwControlKeyState;key.modifiers=(flags&SHIFT_PRESSED?1:0)|(flags&(LEFT_ALT_PRESSED|RIGHT_ALT_PRESSED)?2:0)|(flags&(LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED)?4:0);
                auto scalar=event.uChar.UnicodeChar;
                if(scalar){unit[0]=static_cast<char16_t>(scalar);key.key=scalar==13 || scalar==10?Key::enter:scalar==9?Key::tab:scalar==8 || scalar==127?Key::backspace:scalar==27?Key::escape:Key::character;metadata=key;return {1};}
                switch(event.wVirtualKeyCode){case VK_RETURN:key.key=Key::enter;break;case VK_TAB:key.key=Key::tab;break;case VK_BACK:key.key=Key::backspace;break;case VK_ESCAPE:key.key=Key::escape;break;case VK_UP:key.key=Key::up;break;case VK_DOWN:key.key=Key::down;break;case VK_LEFT:key.key=Key::left;break;case VK_RIGHT:key.key=Key::right;break;case VK_HOME:key.key=Key::home;break;case VK_END:key.key=Key::end;break;case VK_INSERT:key.key=Key::insert;break;case VK_DELETE:key.key=Key::delete_key;break;case VK_PRIOR:key.key=Key::page_up;break;case VK_NEXT:key.key=Key::page_down;break;default:if(event.wVirtualKeyCode>=VK_F1 && event.wVirtualKeyCode<=VK_F12)key.key=static_cast<Key>(static_cast<unsigned>(Key::f1)+event.wVirtualKeyCode-VK_F1);break;}
                return {0,false,{},key};
            }
        });
        if(result.completed){auto key=metadata.value_or(PhysicalKey{Key::character});if(carried){key.known=false;key.down_known=false;key.repeat=1;}
            if(!metadata){auto first=bytes[0];key.key=first==13 || first==10?Key::enter:first==9?Key::tab:first==8 || first==127?Key::backspace:first==27?Key::escape:Key::character;}result.key=key;}
        return result;
    }
#else
    std::optional<TerminalResult> key_ready() override {
        // A readiness snapshot must not move a canonical editor's unfinished
        // line into VOID's queue or change any terminal flag.
        SavedError saved;TerminalResult result;pollfd descriptor{borrowed_[0],POLLIN,0};
        auto ready=poll(&descriptor,1,0);if(ready<0){result.transfer.error=native_error(errno);return result;}
        if(descriptor.revents&POLLNVAL){result.transfer.error=native_error(EBADF);return result;}
        if(!(descriptor.revents&POLLIN) && (descriptor.revents&(POLLERR|POLLHUP))){result.transfer.error={ErrorKind::closed};return result;}
        result.first=(descriptor.revents&POLLIN)?1:0;return result;
    }
#endif
