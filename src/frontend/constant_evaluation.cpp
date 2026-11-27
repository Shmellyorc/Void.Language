#include "constant_evaluation.hpp"
#include "stack_guard.hpp"
#include "void/compile_evaluation.hpp"
#include "void/format.hpp"
#include "void/math_ops.hpp"
#include "void/utf8.hpp"
#include <algorithm>
#include <bit>
#include <limits>

namespace vd::frontend {

// Only typed, pure ASTs reach this evaluator. The shared software kernel
// preserves declared precision and wrapping independently of host arithmetic.
// G93 extends the existing typed constant-expression kernel. Calls carry the
// declaration selected by ordinary binding; local frames contain owned values,
// never Runtime objects, borrowed places, native addresses or executable code.
bool compile_value_type(Type root) {
    std::vector<Type> pending{root};std::set<const ir::ValueType*> seen;
    while(!pending.empty()) {
        auto t=pending.back();pending.pop_back();
        if(t==Type::none || t==Type::string || t==Type::boolean || numeric::number(t) || t.kind==Type::numeric_enum)continue;
        if(t.kind!=Type::aggregate && !ir::collection_type(t))return false;
        if(!t.definition || t.definition->callable || t.definition->managed || t.definition->native_resource)return false;
        if(!seen.insert(t.definition.get()).second)continue;
        if(ir::collection_type(t)) {
            pending.push_back(t.kind==Type::dictionary?ir::dictionary_value(t):ir::array_element(t));
            if(t.kind==Type::dictionary)pending.push_back(ir::dictionary_key(t));
        } else for(const auto& f:t.definition->fields)pending.push_back(f.type);
    }
    return true;
}
void collection_constant_type(const SourceFile&,const Token&,Type);
bool constant_core_operation(std::string_view target) {
    return target == "$text.index" || target == "$text.length" || target == "$text.byte_length" ||
        target == "$dict.count" || target == "$dict.contains" || target == "$safe.dict.get" ||
        target == "$safe.array.get" || target == "$array.count";
}
bool compile_intrinsic(const Expr& e) {
    if(!e.display_adapter.empty())return false;
    return e.target.starts_with("$numeric::") || e.target=="$interpolation" || e.target=="$hole" ||
        constant_core_operation(e.target) || e.target=="$display.string" || e.target=="$display.format" || e.op=="$enum_equal" ||
        e.op.starts_with("$enum.") || e.op=="$collection_equal";
}
void validate_compile_function(const SourceFile& source,Function& f) {
    auto reject=[&](const Token& site,const std::string& reason){error(source,site,Code::constant_call,"compile func: "+reason);};
    if(f.asynchronous || !f.native_symbol.empty() || f.c_primitive || f.initializer || f.accessor ||
       f.virtual_method || f.overriding || f.interface_requirement || f.anonymous || f.token.text=="deinit")
        reject(f.token,"requires an ordinary synchronous source function without lifecycle, virtual or native effects.");
    if(f.receiver && f.receiver->type.kind!=Type::aggregate)reject(f.token,"requires a pure value-struct receiver.");
    if(!compile_value_type(f.result))reject(f.token,"result type requires Runtime identity or unsupported storage.");
    for(const auto& t:f.locals)if(!compile_value_type(t))reject(f.token,"local/parameter type requires Runtime identity or unsupported storage.");
    std::vector<const Statement*> statements;for(const auto& s:f.body)statements.push_back(&s);
    std::vector<const Expr*> expressions;
    std::vector<const Pattern*> patterns;
    while(!statements.empty()) {
        const auto& s=*statements.back();statements.pop_back();
        using K=Statement::Kind;
        if(s.kind==K::scoped || s.kind==K::deferred || s.asynchronous_iteration)
            reject(s.token,"resource scopes, deferred effects and async iteration are not supported constant operations.");
        if(s.pattern)patterns.push_back(&*s.pattern);
        if(s.kind==K::assignment) {
            if(s.selection || s.event || !s.compound_target.empty() || s.compound_conversion)
                reject(s.token,"only direct local value assignments are supported; external state and property effects are forbidden.");
            if(f.receiver && s.slot==f.receiver->slot)reject(s.token,"a compile-time receiver must remain readonly.");
            Type selected=f.locals.at(s.slot);
            for(auto index:s.field_indexes) {
                if(selected.kind!=Type::aggregate)reject(s.token,"assignment cannot mutate referenced storage.");
                selected=ir::field_type(selected,index);
            }
            if(s.index && selected.kind!=Type::fixed_array)reject(s.token,"only owned fixed-array elements may be mutated.");
        }
        if(s.kind==K::loop && !s.binder.text.empty() && !s.range_end && !ir::array_type(s.expression->type))
            reject(s.token,"constant iteration supports ranges, conditions and arrays.");
        for(const auto* e:{s.expression.get(),s.selection.get(),s.index.get(),s.range_end.get()})if(e)expressions.push_back(e);
        for(const auto& child:s.body)statements.push_back(&child);
        for(const auto& child:s.otherwise)statements.push_back(&child);
    }
    while(!expressions.empty() || !patterns.empty()) {
        if(!patterns.empty()) {
            const auto& pattern = *patterns.back(); patterns.pop_back();
            if(!compile_value_type(pattern.type) || ir::reference_value(pattern.narrowed))
                reject(pattern.token,"pattern requires Runtime identity or unsupported storage.");
            for(const auto& value : pattern.values)expressions.push_back(value.get());
            for(const auto& child : pattern.children)patterns.push_back(&child);
            continue;
        }
        const auto& e=*expressions.back();expressions.pop_back();using K=Expr::Kind;
        if(!compile_value_type(e.type))reject(e.token,"expression requires Runtime identity or unsupported storage.");
        switch(e.kind) {
        case K::name:case K::literal:case K::unary:case K::binary:case K::conversion:case K::choice:
        case K::sum:case K::array_literal:case K::dictionary_literal:case K::array_default:case K::return_pack:
        case K::array_index:case K::test:case K::match:break;
        case K::field:
            if(e.property || (e.target.starts_with("$") && !constant_core_operation(e.target)))reject(e.token,"this property requires runtime evaluation.");
            break;
        case K::construction:
            if(e.type.kind!=Type::aggregate || !e.initializer.empty())reject(e.token,"managed allocation and custom initialization are forbidden.");
            break;
        case K::call:case K::method_call:
            if(e.compile_target) {
                if(!e.compile_target->compile_time || e.asynchronous_call || !e.native_symbol.empty() || !e.virtual_slot.empty() || e.interface_call)
                    reject(e.token,"calls require an explicitly marked, statically selected compile func.");
            } else if(!compile_intrinsic(e))reject(e.token,"this call has runtime-only or unsupported effects.");
            if(e.op=="$enum_equal" || e.op=="$collection_equal")collection_constant_type(source,e.token,e.parts.at(0)->type);
            break;
        default:reject(e.token,"operation has runtime-only or unsupported effects.");
        }
        for(const auto& child:e.parts)expressions.push_back(child.get());
        for(const auto& child:e.omitted)expressions.push_back(child.get());
        for(const auto& pattern:e.patterns)patterns.push_back(&pattern);
    }
}
void CompileEvaluation::step(const SourceFile& source,const Token& site,std::uint64_t work) {
    if(compile_evaluation_stop.stop_requested())error(source,site,Code::limit,"Compile-time evaluation cancelled.");
    if(work>step_limit-steps || work>graph_step_limit-graph_steps)
        error(source,site,Code::limit,"Compile-time evaluation work budget exceeded (1048576 steps per expression; 8388608 per analysis).");
    steps+=work;graph_steps+=work;
}

void CompileEvaluation::charge_bytes(const SourceFile& source, const Token& site, std::size_t amount) {
    if(amount>byte_limit-bytes || amount>graph_byte_limit-graph_bytes)
        error(source,site,Code::limit,"Compile-time temporary value budget exceeded (64 MiB per expression; 256 MiB per analysis).");
    bytes+=amount;graph_bytes+=amount;
}
void CompileEvaluation::charge(const SourceFile& source,const Token& site,const ir::ConstantValue& value) {
    std::vector<const ir::ConstantValue*> pending{&value};
    while(!pending.empty()) {
        const auto& v=*pending.back();pending.pop_back();
        charge_bytes(source,site,sizeof(v)+v.text.size());
        for(const auto& field:v.fields)pending.push_back(&field);
    }
}

ir::ConstantValue CompileEvaluation::local(const SourceFile& source,const Expr& e) {
    if(!locals || e.slot>=locals->size() || !locals->at(e.slot))
        error(source,e.token,Code::constant_call,"Compile-time local value is not initialized.");
    return *locals->at(e.slot);
}

void prepare_compile_call(CompileEvaluation* evaluation,Function& f,const SourceFile& source,const Token& site,bool body) {
    if(!evaluation || !evaluation->prepare)error(source,site,Code::constant_call,"Compile-time function is unavailable during this declaration dependency.");
    evaluation->prepare(f,source,site,body);
}
ir::ConstantValue constant_expression(const SourceFile&,const Expr&,CompileEvaluation*);
ir::ConstantValue constant_value(const SourceFile& source,const Expr& e,CompileEvaluation* evaluation) {
    const bool compile_call=e.compile_target && (e.kind==Expr::Kind::call || e.kind==Expr::Kind::method_call);
    if(!evaluation) {
        if(compile_call)throw std::logic_error("Missing constant evaluation context");
        return constant_expression(source,e,nullptr);
    }
    CompileEvaluation::Nesting nesting(*evaluation,source,e.token);
    if(!evaluation->depth){evaluation->steps=0;evaluation->bytes=0;}
    if(evaluation->depth>=128)error(source,e.token,Code::limit,"Compile-time evaluation nesting limit of 128 exceeded.");
    ++evaluation->depth;
    struct Leave {CompileEvaluation& state;~Leave(){--state.depth;}} leave{*evaluation};
    evaluation->step(source,e.token);
    // Keep recursive calls out of the general expression dispatcher's frame.
    ir::ConstantValue value;
    if(compile_call)value=evaluation->call(source,e);
    else if(e.kind==Expr::Kind::name)value=evaluation->local(source,e);
    else value=constant_expression(source,e,evaluation);
    evaluation->charge(source,e.token,value);
    return value;
}
VOID_SEMANTIC_HANDLER ir::ConstantValue CompileEvaluation::call(const SourceFile& source,const Expr& e) {
    auto& f=*e.compile_target;
    prepare_compile_call(this,f,source,e.token,true);
    if(calls>=64)error(source,e.token,Code::limit,"Compile-time recursive call depth limit of 64 exceeded.");
    if(f.locals.size()>(byte_limit-bytes)/sizeof(std::optional<ir::ConstantValue>))error(source,e.token,Code::limit,"Compile-time local storage budget exceeded.");
    const auto frame_bytes=f.locals.size()*sizeof(std::optional<ir::ConstantValue>);
    if(frame_bytes>graph_byte_limit-graph_bytes)error(source,e.token,Code::limit,"Compile-time local storage budget exceeded.");
    bytes+=frame_bytes;graph_bytes+=frame_bytes;
    std::vector<std::optional<ir::ConstantValue>> frame(f.locals.size());
    // Snapshot supplied arguments in source order, then map to formal slots.
    const auto first=f.receiver?1u:0u;
    if(first)frame.at(f.receiver->slot)=constant_value(source,*e.parts.at(0),this);
    for(std::size_t n=first;n<e.parts.size();++n)
        frame.at(f.parameters.at(e.argument_indexes.at(n-first)).slot)=constant_value(source,*e.parts[n],this);
    auto* previous=locals;locals=&frame;++calls;
    struct Leave {CompileEvaluation& state;decltype(locals) previous;~Leave(){state.locals=previous;--state.calls;}} leave{*this,previous};
    try {return sequence(f.compile_owner->source,f.body).value;}
    catch(const CompilationError& failure) {
        auto diagnostics=failure.diagnostics();
        for(auto& d:diagnostics)if(d.related.size()<12)d.related.push_back({span(source,e.token),"While evaluating compile func '"+f.token.text+"'."});
        throw CompilationError(std::move(diagnostics));
    }
}
VOID_SEMANTIC_HANDLER void CompileEvaluation::assign(const SourceFile& source,const Statement& s) {
    // As in lowering, evaluate place/index and RHS before writing owned storage.
    auto index=s.index?std::optional(constant_value(source,*s.index,this)):std::nullopt;
    auto right=constant_value(source,*s.expression,this);
    auto& storage=locals->at(s.slot);if(!storage)error(source,s.token,Code::constant_call,"Assignment reads an uninitialized compile-time local.");
    auto* place=&*storage;auto type=s.assigned_type;
    for(auto field:s.field_indexes)place=&place->fields.at(field);
    if(index) {
        std::uint64_t selected;
        try {selected=ir::checked_array_index(index->bits,s.index->type,place->repeat.value_or(place->fields.size()));}
        catch(const std::out_of_range&){error(source,s.index->token,Code::invalid_value,"Constant array index out of bounds.");}
        if(place->repeat) {
            const auto count=*place->repeat;
            std::size_t prototype_bytes=0;std::vector<const ir::ConstantValue*> pending{&place->fields.at(0)};
            while(!pending.empty()) {
                auto* v=pending.back();pending.pop_back();const auto amount=sizeof(*v)+v->text.size();
                if(amount>byte_limit-prototype_bytes)error(source,s.token,Code::limit,"Compile-time fixed-array prototype exceeds temporary budget.");
                prototype_bytes+=amount;for(const auto& field:v->fields)pending.push_back(&field);
            }
            if(count>(byte_limit-bytes)/prototype_bytes)error(source,s.token,Code::limit,"Compile-time fixed-array materialization exceeds temporary budget.");
            auto item=place->fields.at(0);place->fields.assign(count,item);place->repeat.reset();charge(source,s.token,*place);
        }
        place=&place->fields.at(selected);
    }
    try {
        if(s.op=="=")*place=std::move(right);
        else if(type==Type::string) {
            if(right.text.size()>ir::string_constant_limit-place->text.size())error(source,s.token,Code::limit,"Folded string exceeds the 16 MiB constant limit.");
            place->text+=right.text;
        } else {
            auto primitive=type.kind==Type::numeric_enum?Type(*type.definition->enum_backing):type;
            place->bits=numeric::binary(operation(s.op.substr(0,s.op.size()-1)),place->bits,right.bits,primitive);
        }
        charge(source,s.token,*place);
    } catch(const std::invalid_argument& failure){error(source,s.compound_token,Code::zero_divisor,failure.what());}
}
const ir::ConstantValue& CompileEvaluation::read(const SourceFile& source,const Token& site,const Selection& selected) {
    const auto* value=&selected.captured;
    if(selected.slot) {
        if(!locals || !locals->at(*selected.slot))error(source,site,Code::constant_call,"Compile-time local value is not initialized.");
        value=&*locals->at(*selected.slot);
    }
    for(auto part:selected.path)value=&value->fields.at(part.array && value->repeat?0:part.index);
    return *value;
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Selection CompileEvaluation::select(const SourceFile& source,const Expr& e) {
    // Match lowering's selected storage: freeze index expressions, keep local
    // value ancestry live, and capture computed roots / growable handles once.
    std::vector<const Expr*> path;const auto* root=&e;
    while(root->kind==Expr::Kind::field || root->kind==Expr::Kind::array_index) {
        step(source,root->token);path.push_back(root);root=root->parts.at(0).get();
    }
    Selection selected;
    if(root->kind==Expr::Kind::name)selected.slot=root->slot;
    else selected.captured=constant_value(source,*root,this);
    for(auto it=path.rbegin();it!=path.rend();++it) {
        const auto& part=**it;
        if(part.kind==Expr::Kind::field)selected.path.push_back({part.slot,false});
        else {
            if(part.parts[0]->type.kind==Type::growable_array) {
                auto captured=read(source,part.token,selected);charge(source,part.token,captured);
                selected.captured=std::move(captured);selected.slot.reset();selected.path.clear();
            }
            auto index=constant_value(source,*part.parts[1],this);
            const auto& array=read(source,part.token,selected);
            try {selected.path.push_back({static_cast<std::size_t>(ir::checked_array_index(index.bits,part.parts[1]->type,array.repeat.value_or(array.fields.size()))),true});}
            catch(const std::out_of_range&){error(source,part.token,Code::invalid_value,"Constant array index out of bounds.");}
        }
    }
    return selected;
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Outcome CompileEvaluation::loop(const SourceFile& source,const Statement& s) {
    Nesting nesting(*this,source,s.token);
    if(s.binder.text.empty()) {
        while(!s.expression || constant_value(source,*s.expression,this).bits) {
            step(source,s.token);auto result=sequence(source,s.body);
            if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
        }
    } else if(s.range_end) {
        auto first=constant_value(source,*s.expression,this).bits,last=constant_value(source,*s.range_end,this).bits;
        const auto type=s.expression->type;bool up=numeric::binary(ir::Op::less_equal,first,last,type);
        auto cursor=s.reverse?last:first,terminal=s.reverse?first:last;
        if(s.reverse){up=!up;if(s.exclusive){if(first==last)return {};cursor=numeric::binary(up?ir::Op::add:ir::Op::subtract,cursor,1,type);}}
        while(!(s.exclusive && !s.reverse && cursor==terminal)) {
            step(source,s.token);locals->at(s.slot)=ir::ConstantValue{cursor,{}};
            auto result=sequence(source,s.body);if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
            if((!s.exclusive || s.reverse) && cursor==terminal)break;
            cursor=numeric::binary(up?ir::Op::add:ir::Op::subtract,cursor,1,type);
        }
    } else {
        Selection selected;
        if(s.expression->type.kind==Type::growable_array)
            selected.captured=constant_value(source,*s.expression,this);
        else selected=select(source,*s.expression);
        const auto& initial=read(source,s.token,selected);auto count=initial.repeat.value_or(initial.fields.size());
        for(std::uint64_t n=0;n<count;++n) {
            step(source,s.token);const auto index=s.reverse?count-1-n:n;
            const auto& array=read(source,s.token,selected);
            locals->at(s.slot)=array.fields.at(array.repeat?0:index);charge(source,s.token,*locals->at(s.slot));
            if(!s.index_binder.text.empty())locals->at(s.index_slot)=ir::ConstantValue{index,{}};
            auto result=sequence(source,s.body);if(result.flow==return_flow)return result;if(result.flow==break_flow)break;
        }
    }
    return {};
}
VOID_SEMANTIC_HANDLER CompileEvaluation::Outcome CompileEvaluation::sequence(const SourceFile& source,const std::vector<Statement>& body) {
    if(body.empty())return {};
    Nesting nesting(*this,source,body.front().token);
    for(const auto& s:body) {
        step(source,s.token);using K=Statement::Kind;
        switch(s.kind) {
        case K::variable:locals->at(s.slot)=s.expression?constant_value(source,*s.expression,this):ir::ConstantValue{};break;
        case K::assignment:assign(source,s);break;
        case K::expression:(void)constant_value(source,*s.expression,this);break;
        case K::returned:return {return_flow,s.expression?constant_value(source,*s.expression,this):ir::ConstantValue{}};
        case K::conditional:{auto result=sequence(source,constant_value(source,*s.expression,this).bits?s.body:s.otherwise);if(result.flow!=next_flow)return result;break;}
        case K::loop:{auto result=loop(source,s);if(result.flow!=next_flow)return result;break;}
        case K::match:{auto result=match_statement(source,s);if(result.flow!=next_flow)return result;break;}
        case K::broken:return {break_flow,{}};case K::continued:return {continue_flow,{}};
        case K::yielded:throw std::logic_error("yield reached constant execution");
        case K::no_op:break;
        case K::destructure:{auto value=constant_value(source,*s.expression,this);for(unsigned n=0;n<s.bindings.size();++n)locals->at(s.bindings[n].slot)=value.fields.at(n);break;}
        default:error(source,s.token,Code::constant_call,"Unsupported compile-time statement.");
        }
    }
    return {};
}

#include "collection_constants.inc"
#include "enum_constants.inc"
namespace {
std::size_t constant_array_count(const ir::ConstantValue& value) {
    return value.repeat.value_or(value.fields.size());
}

const ir::ConstantValue* constant_array_find(const Expr& index_expr,const ir::ConstantValue& index,
                                             const ir::ConstantValue& array) {
    try {
        const auto offset=ir::checked_array_index(index.bits,index_expr.type,constant_array_count(array));
        return &array.fields.at(array.repeat?0:static_cast<std::size_t>(offset));
    } catch(const std::out_of_range&) {
        return nullptr;
    }
}

ir::ConstantValue inspect_constant(const SourceFile& source,const Expr& e,CompileEvaluation* evaluation) {
    const auto owner=constant_value(source,*e.parts.at(0),evaluation);
    const auto type=e.parts[0]->type;
    if(e.target=="$array.count")return {constant_array_count(owner),{}};
    if(e.target=="$dict.count")return {constant_dictionary_entries(source,e.token,type,owner,evaluation).size(),{}};
    if(e.target=="$text.byte_length")return {owner.text.size(),{}};
    if(type==Type::string) {
        if(evaluation)evaluation->step(source,e.token,(owner.text.size()+63)/64);
        const auto count=utf8_scalar_count(owner.text);
        if(e.target=="$text.length")return {count,{}};
        const auto& operand=*e.parts.at(1);
        const auto index=constant_value(source,operand,evaluation);
        try {
            const auto selected=ir::checked_array_index(index.bits,operand.type,count);
            std::size_t offset=0;
            for(std::uint64_t n=0;n<selected;++n)offset+=utf8_scalar_bytes(owner.text,offset);
            return {0,{},owner.text.substr(offset,utf8_scalar_bytes(owner.text,offset))};
        } catch(const std::out_of_range&) {
            error(source,e.token,Code::invalid_value,"String index outside extent.");
        }
    }
    const auto& operand=*e.parts.at(1);
    const auto index=constant_value(source,operand,evaluation);
    const bool dictionary=type.kind==Type::dictionary;
    const auto* selected=dictionary?constant_dictionary_find(source,e.token,type,owner,index,evaluation):
        constant_array_find(operand,index,owner);
    if(e.target=="$dict.contains")return {selected!=nullptr,{}};
    if(e.target=="$safe.dict.get" || e.target=="$safe.array.get") {
        ir::ConstantValue result;
        result.fields.resize(3);
        result.fields[0].bits=selected?0:1;
        if(selected)result.fields[1]=*selected;
        else result.fields[2].text=dictionary?"key not found":"index out of range";
        return result;
    }
    if(selected)return *selected;
    error(source,e.token,Code::invalid_value,dictionary?"Dictionary key not found.":"Constant array index out of bounds.");
}
}
ir::ConstantValue constant_expression(const SourceFile& source, const Expr& e, CompileEvaluation* evaluation) {
    // Presentation recovery keeps the nominal type of a partial enum member,
    // but the hole has no value or operands. Propagate the existing poison
    // boundary instead of evaluating it as an ordinary constant expression.
    if(e.kind==Expr::Kind::editor_hole)throw Poison{};
    if(e.kind==Expr::Kind::test || e.kind==Expr::Kind::match) {
        if(!evaluation)error(source,e.token,Code::constant_call,"Pattern requires a compile-time evaluation context.");
        return evaluation->match_expression(source,e);
    }
    if(e.kind==Expr::Kind::array_index || constant_core_operation(e.target))
        return frontend_case([&]{return inspect_constant(source,e,evaluation);});
    if(e.kind==Expr::Kind::test || e.kind==Expr::Kind::match)
        return evaluation->match_expression(source,e);
    if(e.kind==Expr::Kind::call && e.op=="$collection_equal") return frontend_case([&]() -> ir::ConstantValue {
        const auto type=e.parts.at(0)->type;collection_constant_type(source,e.token,type);
        const auto left = constant_value(source,*e.parts[0],evaluation);
        const auto right = constant_value(source,*e.parts[1],evaluation);
        return {collection_constant_equal(source,e.token,type,left,right,0,evaluation),{}};
    });
    if(auto result=enum_constant(source,e,evaluation))return *result;
    if(e.target=="$interpolation") return frontend_case([&]() -> ir::ConstantValue {
        formatting::Builder builder;
        for(const auto& part:e.parts){auto text=constant_value(source,*part,evaluation).text;if(text.size()>ir::string_constant_limit-builder.size())error(source,part->token,Code::limit,"Folded string exceeds the 16 MiB constant limit.");builder.append(text);}
        return {0,{},std::move(builder).finish()};
    });
    if(e.target=="$hole") return frontend_case([&]() -> ir::ConstantValue {
        auto value=constant_value(source,*e.parts[0],evaluation);const auto& spec=e.parts[1]->aggregate_value.text;
        auto plan=formatting::parse_numeric(spec);
        if(plan.alignment.width>ir::string_constant_limit || plan.minimum_integer>ir::string_constant_limit || plan.maximum_fraction>ir::string_constant_limit)error(source,e.token,Code::limit,"Folded display exceeds the 16 MiB constant limit.");
        auto type=e.parts[0]->type;
        auto text=numeric::number(type)?formatting::number(numeric::display_number(value.bits,type),plan):formatting::text(type==Type::boolean?(value.bits?"true":"false"):std::string_view(value.text),spec);
        if(text.size()>ir::string_constant_limit)error(source,e.token,Code::limit,"Folded display exceeds the 16 MiB constant limit.");return {0,{},std::move(text)};
    });
    if(e.target.starts_with("$display.") && e.kind==Expr::Kind::call) return frontend_case([&]() -> ir::ConstantValue {
        std::vector<ir::ConstantValue> values(e.parts.size());std::vector<Type> types(e.parts.size());
        for(unsigned n=0;n<e.parts.size();++n){auto index=e.argument_indexes.at(n);values[index]=constant_value(source,*e.parts[n],evaluation);types[index]=e.parts[n]->type;}
        const bool formatted=e.target=="$display.format";std::string text;bool valid=true;
        try {
            auto spec=formatted?std::string_view(values[1].text):std::string_view{};
            // Preserve the existing compiler constant budget before allocating
            // requested padding; Runtime formatting has no arbitrary width cap.
            auto plan=formatting::parse_numeric(spec);
            if(plan.alignment.width>ir::string_constant_limit || plan.minimum_integer>ir::string_constant_limit || plan.maximum_fraction>ir::string_constant_limit)error(source,e.token,Code::limit,"Folded display exceeds the 16 MiB constant limit.");
            text=numeric::number(types[0])?formatting::number(numeric::display_number(values[0].bits,types[0]),plan):formatting::text(types[0]==Type::boolean?(values[0].bits?"true":"false"):std::string_view(values[0].text),spec);
        }catch(const formatting::Error& failure){if(!formatted)error(source,e.token,Code::invalid_value,failure.what());valid=false;text=failure.what();}
        if(text.size()>ir::string_constant_limit)error(source,e.token,Code::limit,"Folded display exceeds the 16 MiB constant limit.");
        if(!formatted)return {0,{},std::move(text)};
        ir::ConstantValue result;result.fields.resize(3);result.fields[0].bits=valid?0:1;result.fields[valid?1:2].text=std::move(text);return result;
    });
    if(e.target.starts_with("$numeric::") && e.kind==Expr::Kind::call) return frontend_case([&]() -> ir::ConstantValue {
        std::vector<std::uint64_t> values(e.parts.size());
        for(unsigned n=0;n<e.parts.size();++n)values.at(e.argument_indexes.at(n))=constant_value(source,*e.parts[n],evaluation).bits;
        try {return {e.token.text=="abs"?numeric::absolute(values.at(0),e.type):numeric::clamp(values.at(0),values.at(1),values.at(2),e.type),{}};}
        catch(const std::invalid_argument& failure){error(source,e.token,Code::numeric_conversion,failure.what());}
    });
    if(e.kind==Expr::Kind::conversion && e.op=="$nullable_try") return frontend_case([&]() -> ir::ConstantValue {
        // Keep the owning input alive while copying into independent output
        // storage. Assigning an input from its own child destroys that child.
        const auto input=constant_value(source,*e.parts[0],evaluation);
        const bool wrapped=ir::nullable_value(e.parts[0]->type);
        const bool present=wrapped?input.fields.at(0).bits:input.bits || !input.text.empty();
        ir::ConstantValue value;value.fields.resize(2);
        if(present){value.fields[0].bits=1;value.fields[1]=wrapped?input.fields.at(1):input;}
        return value;
    });
    if(e.kind==Expr::Kind::conversion && e.op=="$nullable_lift") {ir::ConstantValue value;value.fields.resize(2);value.fields[0].bits=1;value.fields[1]=constant_value(source,*e.parts[0],evaluation);return value;}
    if(e.kind==Expr::Kind::conversion && e.op=="$nullable_unwrap") {auto value=constant_value(source,*e.parts[0],evaluation);if(!value.fields.at(0).bits)error(source,e.token,Code::numeric_conversion,"Null value in checked conversion");return value.fields.at(1);}
    if(e.kind==Expr::Kind::conversion && e.op=="identity" && e.type==e.parts[0]->type)return constant_value(source,*e.parts[0],evaluation);
    if(e.kind==Expr::Kind::binary && e.op.starts_with("$nullable_null_")){bool equal=constant_value(source,*e.parts[0],evaluation).fields.at(0).bits==constant_value(source,*e.parts[1],evaluation).fields.at(0).bits;return {e.op=="$nullable_null_equal"?equal:!equal,{}};}
    if(e.kind==Expr::Kind::choice)return constant_value(source,*e.parts[constant_value(source,*e.parts[0],evaluation).bits?1:2],evaluation);
    if(e.kind==Expr::Kind::sum) return frontend_case([&]() -> ir::ConstantValue {
        ir::ConstantValue result;result.fields.resize(e.type.definition->fields.size());result.fields[0].bits=e.slot;
        const auto& variant=e.type.definition->cases.at(e.slot);
        for(unsigned n=0;n<e.parts.size();++n)result.fields[variant.begin+n]=constant_value(source,*e.parts[n],evaluation);return result;
    });
    if(e.kind==Expr::Kind::function_reference)return {0,{},e.target};
    if(e.kind==Expr::Kind::function_null)return {};
    if(e.kind==Expr::Kind::dictionary_literal){ir::ConstantValue result;for(const auto& part:e.parts)result.fields.push_back(constant_value(source,*part,evaluation));return result;}
    if (e.kind == Expr::Kind::literal) return (e.type.kind == Type::aggregate || ir::collection_type(e.type) || e.type==Type::string) ? e.aggregate_value : ir::ConstantValue{e.value,{}};
    if(e.kind==Expr::Kind::array_default){ir::ConstantValue value;value.repeat=*e.type.definition->extent;if(*value.repeat)value.fields.push_back(constant_value(source,*e.parts.at(0),evaluation));return value;}
    if(e.kind==Expr::Kind::array_literal || e.kind==Expr::Kind::return_pack) return frontend_case([&]() -> ir::ConstantValue {
        ir::ConstantValue result;for(const auto& part:e.parts) result.fields.push_back(constant_value(source,*part,evaluation));return result;
    });
    if (e.kind == Expr::Kind::construction) return frontend_case([&]() -> ir::ConstantValue {
        auto result = e.aggregate_value;
        for (unsigned n = 0; n < e.parts.size(); ++n) result.fields[e.field_indexes[n]] = constant_value(source,*e.parts[n],evaluation);
        for (const auto& child : e.omitted) result.fields[child->slot] = constant_value(source,*child,evaluation);
        return result;
    });
    if (e.kind == Expr::Kind::field) {auto value=constant_value(source,*e.parts[0],evaluation);return value.fields.at(e.slot);}
    if (e.kind==Expr::Kind::binary && e.parts[0]->type==Type::string) return frontend_case([&]() -> ir::ConstantValue {
        auto a=constant_value(source,*e.parts[0],evaluation).text;
        auto b=constant_value(source,*e.parts[1],evaluation).text;
        if(e.op=="+") {
            if(b.size()>ir::string_constant_limit-a.size()) error(source,e.token,Code::limit,"Folded string exceeds the 16 MiB constant limit.");
            a+=b; return {0,{},std::move(a)};
        }
        return {static_cast<std::uint64_t>(e.op=="==" ? a==b : a!=b),{}};
    });
    return frontend_case([&]() -> ir::ConstantValue {
    if(e.kind!=Expr::Kind::conversion && e.kind!=Expr::Kind::unary && e.kind!=Expr::Kind::binary)
        error(source,e.token,Code::constant_call,"Operation is not supported in compile-time evaluation.");
    auto a = constant_value(source,*e.parts.at(0),evaluation).bits;
    try {
        if (e.kind == Expr::Kind::conversion) {
            if(e.token.text=="as?") {
                auto destination=ir::field_type(e.type,1);
                auto backing=destination.kind==Type::numeric_enum?Type(*destination.definition->enum_backing):destination;
                auto from=e.parts[0]->type.kind==Type::numeric_enum?Type(*e.parts[0]->type.definition->enum_backing):e.parts[0]->type;
                auto value=numeric::try_integer_convert(a,from,backing);
                ir::ConstantValue result;result.fields.resize(2);
                if(destination.kind==Type::numeric_enum && evaluation){evaluation->numeric_definition(destination);evaluation->step(source,e.token,destination.definition->numeric_members.size());}
                if(value && (destination.kind!=Type::numeric_enum || ir::numeric_enum_member(destination,*value))) {
                    result.fields[0].bits=1;result.fields[1].bits=*value;
                }
                return result;
            }
            if (ir::reference_value(e.type)) { if (!a && !e.type.nullable) throw std::invalid_argument("Cannot unwrap null in a compile-time expression"); return {a,{}}; }
            auto from=e.parts[0]->type.kind==Type::numeric_enum?Type(*e.parts[0]->type.definition->enum_backing):e.parts[0]->type;
            auto to=e.type.kind==Type::numeric_enum?Type(*e.type.definition->enum_backing):e.type;
            auto value=numeric::convert(a,from,to);
            if(e.type.kind==Type::numeric_enum && evaluation){evaluation->numeric_definition(e.type);evaluation->step(source,e.token,e.type.definition->numeric_members.size());}
            if(e.type.kind==Type::numeric_enum && !ir::numeric_enum_member(e.type,value))throw std::invalid_argument("Checked enum conversion requires a declared discriminant.");
            return {value,{}};
        }
        if (e.kind == Expr::Kind::unary) {
            auto primitive=e.type.kind==Type::numeric_enum?Type(*e.type.definition->enum_backing):e.type;
            // APInt operands must already fit their declared width. In LLVM 23
            // construction does not implicitly truncate an oversized uint64_t.
            return {e.op=="~"?numeric::binary(ir::Op::bit_xor,a,~std::uint64_t{}>>(64-numeric::width(primitive)),primitive):e.op == "not" ? !a : e.op == "-" ? numeric::negate(a,primitive) : a,{}};
        }
        if (e.op == "and" && !a) return {};
        if (e.op == "or" && a) return {1,{}};
        auto b = constant_value(source,*e.parts[1],evaluation).bits;
        if (e.op == "and" || e.op == "or") return {b,{}};
        if (ir::reference_value(e.parts[0]->type)) return {static_cast<std::uint64_t>(e.op=="==" ? a==b : a!=b),{}};
        auto primitive=e.parts[0]->type.kind==Type::numeric_enum?Type(*e.parts[0]->type.definition->enum_backing):e.parts[0]->type;
        return {numeric::binary(operation(e.op),a,b,primitive),{}};
    } catch (const CompilationError&) { throw; }
      catch (const std::invalid_argument& failure) {
        error(source,e.token,e.kind == Expr::Kind::conversion ? Code::numeric_conversion : Code::zero_divisor,failure.what());
    }
    });
}
}
