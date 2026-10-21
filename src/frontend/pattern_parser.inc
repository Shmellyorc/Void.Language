    Pattern pattern() {
        Depth guard(depth,source,peek());
        Pattern result;result.token=peek();
        if(accept("var")){result.kind=Pattern::Kind::capture;result.token=name();return result;}
        if(accept("_")){result.kind=Pattern::Kind::wildcard;return result;}
        if(peek().kind==TokenKind::name && reserved.contains(peek().text) && ((peek().text.size()>1 && peek().text[1]>='0' && peek().text[1]<='9') || peek().text=="bool" || peek().text=="string")){
            result.kind=Pattern::Kind::constructor;result.spelling=peek().text;result.narrowed=type();result.annotated_type=true;
            if(accept("(")){if(!at(")"))do{result.fields.push_back({TokenKind::name,"",peek().offset});result.children.push_back(pattern());}while(accept(","));need(")");}return result;
        }
        if(accept("[")) {
            result.kind=Pattern::Kind::list;
            if(!at("]"))do {
                if(accept("..")){result.rest=true;result.children.push_back(pattern());break;}
                result.children.push_back(pattern());
            }while(accept(","));
            need("]");return result;
        }
        if(accept("{")) {
            result.kind=Pattern::Kind::dictionary;
            if(!at("}"))do {result.values.push_back(expression());need(":");result.children.push_back(pattern());}while(accept(",") && !at("}"));
            need("}");return result;
        }
        if(peek().kind==TokenKind::name && peek().text!="true" && peek().text!="false" && peek().text!="null") {
            auto end=p+1;while(end+1<tokens.size() && tokens[end].text=="." && tokens[end+1].kind==TokenKind::name)end+=2;
            bool generic=false;
            if(end<tokens.size() && tokens[end].text=="<"){unsigned count=0;auto close=end;for(;close<tokens.size();++close){if(tokens[close].text=="<")++count;else if(tokens[close].text==">" && !--count)break;else if(tokens[close].kind==TokenKind::newline || tokens[close].kind==TokenKind::end)break;}if(close<tokens.size() && tokens[close].text==">"){generic=true;end=close+1;}}
            bool value_tail=false;
            if(end<tokens.size() && tokens[end].text=="("){unsigned count=0;auto close=end;for(;close<tokens.size();++close){if(tokens[close].text=="(")++count;else if(tokens[close].text==")" && !--count)break;}if(close+1<tokens.size())value_tail=precedence(tokens[close+1].text)>=4 || tokens[close+1].text=="as" || tokens[close+1].text==".." || tokens[close+1].text=="..<";}
            if(end<tokens.size() && (tokens[end].text=="(" || generic) && !value_tail) {
                result.kind=Pattern::Kind::constructor;result.spelling=take().text;
                while(accept("."))result.spelling+="."+name().text;
                if(at("<"))result.type_arguments=type_arguments();
                if(!accept("("))return result;
                if(!at(")"))do {
                    Token field{TokenKind::name,"",peek().offset};
                    if(peek(1).text=="="){field=name();need("=");}
                    result.fields.push_back(field);result.children.push_back(pattern());
                }while(accept(","));
                need(")");return result;
            }
        }
        result.kind=Pattern::Kind::value;result.values.push_back(expression(4));
        if(at("..") || at("..<")){result.kind=Pattern::Kind::range;result.exclusive=take().text=="..<";result.values.push_back(expression(4));}
        return result;
    }
    std::unique_ptr<Expr> match_expression(Token token) {
        auto result=std::make_unique<Expr>(Expr::Kind::match,std::move(token));result->parts.push_back(expression());
        need(":");newline();need("<indent>");
        while(!at("<dedent>") && !at("<end of file>")) {
            result->patterns.push_back(pattern());need(":");result->parts.push_back(expression());newline();
        }
        need("<dedent>");multiline_expression=true;return result;
    }

    std::unique_ptr<Expr> choice_expression(Token token) {
        Depth guard(depth,source,token,64);
        auto result=std::make_unique<Expr>(Expr::Kind::choice,token);
        result->parts.push_back(expression());need(":");newline();need("<indent>");
        result->parts.push_back(expression());newline();need("<dedent>");need("else");
        if(accept("if"))result->parts.push_back(choice_expression(tokens[p-1]));
        else {need(":");newline();need("<indent>");result->parts.push_back(expression());newline();need("<dedent>");}
        multiline_expression=true;return result;
    }
