// One compiler-owned catalog for builtin member binding, hints and tooling.
// Parameter profiles are specialized from the exact receiver, including its
// projected reference authority. Lowering retains the existing kernel targets.
struct CoreMember {
    const char* name;
    unsigned owners;
    bool property, mutates, positional;
    const char* documentation;
};
static constexpr unsigned core_text=1, core_text_type=2, core_fixed=4, core_list=8, core_dictionary=16;
static constexpr CoreMember core_members[] = {
    {"length",core_text,true,false,false,"Unicode scalar count (i64); no grapheme segmentation."},
    {"byte_length",core_text,true,false,false,"UTF-8 byte count (i64), including embedded NUL."},
    {"to_utf8",core_text,false,false,false,"Copy immutable text into a fresh independent UTF-8 byte list."},
    {"from_utf8",core_text_type,false,false,false,"Copy and validate owned UTF-8 bytes; malformed encoding returns Err(\"invalid UTF-8\")."},
    {"count",core_fixed|core_list|core_dictionary,true,false,false,"Number of stored elements (i64). This property is readonly."},
    {"get",core_fixed|core_list|core_dictionary,false,false,false,"Recoverable lookup preserving value copies and deep readonly authority."},
    {"pop",core_list,false,true,false,"Remove the last list value; empty lists return Err(\"empty list\") without mutation."},
    {"push",core_list,false,true,true,"Append one value to the list."},
    {"remove_at",core_list,false,true,true,"Remove and return the value at an integer index; negative indexes count from the end."},
    {"insert",core_list,false,true,true,"Insert a value at an integer index; negative indexes count from the end."},
    {"clear",core_list|core_dictionary,false,true,true,"Remove all entries from this collection."},
    {"contains",core_dictionary,false,false,true,"Test whether the dictionary contains the supplied key."},
    {"remove",core_dictionary,false,true,true,"Remove the supplied dictionary key and report whether it was present."},
};
const CoreMember* core_member(Type receiver,std::string_view name,bool type_level=false) const {
    if(ir::nullable_type(receiver))return nullptr;
    const auto owner=receiver.kind==Type::string?(type_level?core_text_type:core_text):
        type_level?0u:receiver.kind==Type::fixed_array?core_fixed:
        receiver.kind==Type::growable_array?core_list:receiver.kind==Type::dictionary?core_dictionary:0u;
    for(const auto& api:core_members)if((api.owners&owner) && name==api.name)return &api;
    return nullptr;
}
Function core_signature(const CoreMember& api,Type receiver,const Token& site) {
    Function target;target.token=site;target.token.text=api.name;target.positional_only=api.positional;
    const std::string name=api.name;
    target.result=api.property?Type(Type::i64):name=="to_utf8"?byte_list():name=="from_utf8"?unit.make_result(Type::string,Type::string):Type(Type::none);
    auto parameter=[&](const char* name,Type type) {target.parameters.push_back({Token{TokenKind::name,name,site.offset},type});};
    if(name=="from_utf8")parameter("bytes",ir::readonly_view(byte_list()));
    if(ir::collection_type(receiver)) {
        const auto value=receiver.kind==Type::dictionary?ir::dictionary_value(receiver):ir::array_element(receiver);
        if(name=="get" || name=="pop")target.result=unit.make_result(value,Type::string);
        if(name=="remove_at")target.result=value;
        if(name=="contains" || name=="remove")target.result=Type::boolean;
        if(name=="get" || name=="contains" || name=="remove" || name=="remove_at" || name=="insert")
            parameter(receiver.kind==Type::dictionary?"key":"index",receiver.kind==Type::dictionary?ir::readonly_view(ir::dictionary_key(receiver)):Type(Type::i32));
        if(name=="push" || name=="insert")parameter("value",value);
    }
    const std::string kernel=receiver.kind==Type::string?"$text.":
        name=="get" || name=="pop"?(receiver.kind==Type::dictionary?"$safe.dict.":"$safe.array."):
        receiver.kind==Type::dictionary?"$dict.":"$array.";
    target.symbol=kernel+name+"::"+ir::capability_key(receiver);
    // Keep the established recoverable-kernel identities used by tooling.
    if(name=="get" || name=="from_utf8")target.symbol+="::"+ir::capability_key(target.parameters.front().type);
    return target;
}
void core_record(const CoreMember& api,const Function& target,Type receiver,bool type_level=false) {
    builtin_record(target,api.documentation);
    auto& record=unit.builtins.at(target.symbol);
    record.kind=api.property?AnalyzedDeclaration::Kind::value_field:AnalyzedDeclaration::Kind::receiver_method;
    record.editor_symbol_kind=api.property?7:6;record.static_member=type_level;
    record.readonly=!api.mutates;record.receiver=receiver;record.owner=Type::key(receiver);
    record.signature=api.property?target.token.text+": "+ir::type_name(target.result):editor_signature(target);
    for(const auto& parameter:target.parameters)record.documentation.parameters.push_back({parameter.token.text,
        parameter.token.text=="index"?"Integer index; negative indexes count from the end.":
        parameter.token.text=="key"?"Dictionary key to find or remove.":
        parameter.token.text=="bytes"?"The UTF-8 bytes to validate and copy.":"Value to store in the collection.",{}});
}
