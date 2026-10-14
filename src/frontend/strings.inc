// Included into the existing frontend after Token and diagnostic helpers.
Token string_token(const SourceFile& source,std::size_t& p) {
    const auto& text=source.text(); auto start=p++;
    std::string decoded;
    auto reject=[&](std::size_t at,const std::string& message) {
        p=at; // Recovery consumes this physical line, never the following line.
        error(source,{TokenKind::symbol,"",at},Code::syntax,message);
    };
    auto append=[&](char c) {
        if (decoded.size()==ir::string_constant_limit)
            error(source,{TokenKind::symbol,"",start},Code::limit,"Decoded string exceeds the 16 MiB constant limit.");
        decoded+=c;
    };
    if (text.compare(start,3,"\"\"\"")==0) reject(start,"Multiline strings are deferred.");
    while (p<text.size() && text[p]!='"') {
        auto at=p; unsigned char c=text[p++];
        if (c<32 || c==127) reject(at,"Raw control characters and line breaks are not allowed in strings; use an escape.");
        if (c=='{' || c=='}') {
            if (p>=text.size() || text[p]!=c) reject(at,"Automatic string interpolation is deferred; double literal braces.");
            ++p; append(static_cast<char>(c));
        } else if (c=='\\') {
            if (p>=text.size()) reject(at,"Unterminated string escape.");
            char escape=text[p++];
            switch(escape) {
            case '"': append('"'); break;
            case '\\': append('\\'); break;
            case 'n': append('\n'); break;
            case 'r': append('\r'); break;
            case 't': append('\t'); break;
            case '0': append('\0'); break;
            case 'u': {
                if (p>=text.size() || text[p++]!='{') reject(at,"Unicode escape requires \\u{hex_scalar}.");
                std::uint32_t scalar=0; unsigned digits=0;
                while (p<text.size() && text[p]!='}') {
                    char h=text[p]; int value=h>='0' && h<='9' ? h-'0' : h>='a' && h<='f' ? h-'a'+10 : h>='A' && h<='F' ? h-'A'+10 : -1;
                    if (value<0 || ++digits>6) reject(at,"Unicode escape requires one through six hexadecimal digits.");
                    scalar=(scalar<<4)|static_cast<unsigned>(value); ++p;
                }
                if (!digits || p>=text.size() || scalar>0x10ffff || (scalar>=0xd800 && scalar<=0xdfff)) reject(at,"Invalid Unicode scalar escape.");
                ++p;
                if(scalar<0x80) append(static_cast<char>(scalar));
                else {
                    unsigned count=scalar<0x800 ? 2 : scalar<0x10000 ? 3 : 4;
                    auto lead=(count==2 ? 0xc0 : count==3 ? 0xe0 : 0xf0)|(scalar>>(6*(count-1)));
                    append(static_cast<char>(lead));
                    for(unsigned n=count-1;n>0;--n) append(static_cast<char>(0x80|((scalar>>(6*(n-1)))&63)));
                }
                break;
            }
            default: reject(at,"Unknown string escape.");
            }
        } else append(static_cast<char>(c));
    }
    if (p>=text.size()) reject(start,"Unterminated string literal.");
    ++p;
    return {TokenKind::string,text.substr(start,p-start),start,std::move(decoded)};
}
