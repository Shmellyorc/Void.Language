#include "lowering.hpp"
#include "stack_guard.hpp"
#include "void/console_ops.hpp"
#include "void/filesystem_ops.hpp"
#include "void/http_ops.hpp"
#include "void/math_ops.hpp"
#include "void/path.hpp"
#include <algorithm>
#include <limits>

namespace vd::frontend {
namespace {
struct StringPool {
    std::vector<std::string> values;
    std::map<std::string,unsigned,std::less<>> indexes;
    std::size_t bytes=0;
    unsigned intern(std::string_view text,const SourceFile& source,const Token& token) {
        auto found=indexes.find(text); if(found!=indexes.end()) return found->second;
        if(text.size()>ir::string_constant_limit || text.size()>ir::string_pool_limit-bytes)
            error(source,token,Code::limit,"String constants exceed the 16 MiB value or 64 MiB module-pool limit.");
        auto id=static_cast<unsigned>(values.size());
        indexes.emplace(text,id); values.emplace_back(text); bytes+=text.size(); return id;
    }
};
// All owned templates, including unused declaration metadata, belong to the
// compiled graph's pool. Checking and lowering enforce the same finite budget.
StringPool string_pool(const std::map<std::string,std::unique_ptr<Unit>>& units) {
    StringPool pool;
    for(const auto& [id,u]:units) {
        std::vector<const Expr*> expressions;
        auto value=[&](const ir::ConstantValue& root,const Token& token) {
            std::vector<const ir::ConstantValue*> pending{&root};
            while(!pending.empty()) {
                auto* current=pending.back();pending.pop_back();
                if(!current->text.empty()) pool.intern(current->text,u->source,token);
                for(const auto& child:current->fields) pending.push_back(&child);
            }
        };
        auto patterns=[&](const Pattern& root){std::vector<const Pattern*> pending{&root};while(!pending.empty()){auto* p=pending.back();pending.pop_back();for(const auto& value:p->values)expressions.push_back(value.get());for(const auto& child:p->children)pending.push_back(&child);}};
        auto statements=[&](const std::vector<Statement>& body) {
            std::vector<const Statement*> pending; for(const auto& s:body) pending.push_back(&s);
            while(!pending.empty()) {
                auto* s=pending.back();pending.pop_back();if(s->pattern)patterns(*s->pattern); if(s->expression) expressions.push_back(s->expression.get());
                for(const auto& child:s->body) pending.push_back(&child);
                for(const auto& child:s->otherwise) pending.push_back(&child);
            }
        };
        for(const auto& c:u->constants) {value(c.aggregate_value,c.declaration.token);if(c.declaration.expression) expressions.push_back(c.declaration.expression.get());}
        for(const auto& structure:u->structures) {value(structure.defaults,structure.token);statements(structure.fields);}
        for(const auto& f:u->functions) {
            statements(f.body); value(f.initialization_value,f.token);
            if(f.base_prelude) expressions.push_back(f.base_prelude.get());
            for(const auto& e:f.initialization_defaults) expressions.push_back(e.get());
            for(const auto& p:f.parameters) {if(p.default_value) value(*p.default_value,p.token);if(p.default_expression) expressions.push_back(p.default_expression.get());}
        }
        while(!expressions.empty()) {
            auto* e=expressions.back();expressions.pop_back();value(e->aggregate_value,e->token);for(const auto& p:e->patterns)patterns(p);
            for(const auto& child:e->parts) expressions.push_back(child.get());
            for(const auto& child:e->omitted) expressions.push_back(child.get());
        }
    }
    return pool;
}
class Lowering {
    const SourceFile& source;
    ir::Module& module;
    StringPool& strings;
    ir::Function output;
    std::optional<unsigned> current;
    const ConstructionContext* context=nullptr;
    std::map<unsigned,unsigned> cells;
    std::optional<unsigned> cleanup_head,return_slot;
    struct LocalCleanup {unsigned binding,active;std::string callee;};
    std::map<unsigned,std::string> local_cleanup_targets;
    std::vector<LocalCleanup> local_cleanups;
    std::set<std::string> elided_cleanup_handlers;
    bool select_local_cleanups(const Function& f) {
        local_cleanup_targets.clear();local_cleanups.clear();elided_cleanup_handlers.clear();
        if(f.asynchronous || f.deferred)return false;
        std::vector<const Statement*> pending;for(const auto& s:f.body)pending.push_back(&s);
        std::vector<const Statement*> scopes;std::vector<const Statement*> assignments;
        while(!pending.empty()) {
            auto* s=pending.back();pending.pop_back();
            if(s->kind==Statement::Kind::deferred || s->task_group)return false;
            if(s->kind==Statement::Kind::scoped)scopes.push_back(s);
            if(s->kind==Statement::Kind::assignment)assignments.push_back(s);
            for(const auto& child:s->body)pending.push_back(&child);
            for(const auto& child:s->otherwise)pending.push_back(&child);
        }
        if(scopes.empty() || f.lambdas.size()!=scopes.size())return false;
        std::set<std::string> handlers;std::map<unsigned,std::string> targets;
        for(auto* scope:scopes) {
            if(scope->cleanup_target.empty() || !scope->selection || scope->selection->kind!=Expr::Kind::function_reference || scope->selection->parts.size()!=1)return false;
            const auto& capture=*scope->selection->parts[0];
            if(capture.kind!=Expr::Kind::capture_reference || capture.slot!=scope->slot || capture.implicit)return false;
            for(auto* assignment:assignments)if(assignment->slot==scope->slot)return false;
            targets.emplace(scope->slot,scope->cleanup_target);handlers.insert(scope->selection->target);
        }
        local_cleanup_targets=std::move(targets);elided_cleanup_handlers=std::move(handlers);return true;
    }
    void dispose_local(const LocalCleanup& cleanup,const Token& token) {
        ir::Instruction call{ir::Op::local_cleanup,cleanup.binding,cleanup.active};call.callee=cleanup.callee;call.type=Type::none;call.receiver=ir::Place{cleanup.binding,{}};emit(call,token);
    }
    struct WithScope {unsigned slot;bool group=false;std::optional<unsigned> owned_group={};bool local=false;};
    std::vector<WithScope> with_scopes;
    std::vector<std::size_t> loop_with_depth;
    unsigned register_cleanup(unsigned handler,bool active,const Token& token){
        ir::Instruction node{ir::Op::gc_construct};auto type=output.locals[*cleanup_head];type.nullable=false;node.type=type;node.arguments={emit({ir::Op::load,*cleanup_head},token),handler,constant(Type::boolean,{static_cast<unsigned>(active),{}},token)};auto value=emit(node,token);emit({ir::Op::store,*cleanup_head,value},token);return capture(value,type,token);
    }
    void join_group(unsigned slot,bool cancel,const Token& token){ir::Instruction join{ir::Op::async_group_join,emit({ir::Op::load,slot},token)};join.immediate=cancel;emit(join,token);}
    void leave_with(WithScope scope,const Token& token,bool cancel=true){
        if(scope.group){join_group(scope.slot,cancel,token);return;}
        if(scope.owned_group)join_group(*scope.owned_group,cancel,token);
        if(scope.local) {
            dispose_local(local_cleanups[scope.slot],token);return;
        }
        auto slot=scope.slot;
        auto owner=emit({ir::Op::load,slot},token);ir::Instruction deactivate{ir::Op::gc_write,owner,constant(Type::boolean,{0,{}},token)};deactivate.immediate=2;emit(deactivate,token);
        auto handler=extract_managed(owner,1,token);emit({ir::Op::function_call,handler},token);
    }
    const Statement* normal_return=nullptr;
    std::vector<unsigned> cleanup_jumps;
    void return_through_cleanup(std::optional<unsigned> value,const Token& token,bool early=false){
        if(output.initializer && ir::fallible_initializer_result(output.result) && value) {
            ir::Instruction checked{ir::Op::initializer_return,*value};value=emit(checked,token);
        }
        if(output.asynchronous && early)emit({ir::Op::async_cancel_children},token);
        if(output.asynchronous)for(auto scope=with_scopes.rbegin();scope!=with_scopes.rend();++scope){if(scope->group)join_group(scope->slot,true,token);else if(scope->owned_group)join_group(*scope->owned_group,true,token);}
        if(!cleanup_head && !output.asynchronous){terminate(value?ir::Terminator{ir::Terminator::Kind::return_value,*value}:ir::Terminator{ir::Terminator::Kind::return_void});return;}
        if(value)emit({ir::Op::store,*return_slot,*value},token);
        cleanup_jumps.push_back(*current);terminate({ir::Terminator::Kind::jump});
    }
    std::vector<std::pair<unsigned, unsigned>> loops; // continue, break destinations
    unsigned block() {
        auto id = static_cast<unsigned>(output.blocks.size());
        output.blocks.push_back({{}, {ir::Terminator::Kind::unreachable}}); return id;
    }
    void terminate(ir::Terminator end) { output.blocks[*current].end = end; current.reset(); }
    void jump(unsigned to) { terminate({ir::Terminator::Kind::jump, 0, to}); }
    unsigned emit(ir::Instruction i, const Token& token) {
        if(i.op==ir::Op::load && cells.contains(i.a)) {
            auto owner=emit({ir::Op::load,cells.at(i.a)},token);
            auto value=emit({ir::Op::gc_read,owner},token);auto stored=ir::field_type(output.locals.at(cells.at(i.a)),0);
            if(stored.nullable && !output.locals.at(i.a).nullable){ir::Instruction unwrap{ir::Op::convert,value};unwrap.type=output.locals.at(i.a);value=emit(unwrap,token);}return value;
        }
        if(i.op==ir::Op::store && cells.contains(i.a)) {
            auto home=cells.at(i.a);
            if(i.initialization) {
                ir::Instruction construct{ir::Op::gc_construct};construct.type=output.locals.at(home);construct.arguments={i.b};
                auto owner=emit(construct,token);return emit({ir::Op::store,home,owner},token);
            }
            return emit({ir::Op::gc_write,emit({ir::Op::load,home},token),i.b},token);
        }
        if(i.op==ir::Op::store)i.initialization=false;
        if(i.receiver && cells.contains(i.receiver->slot)) {
            i.receiver->slot=cells.at(i.receiver->slot);i.receiver->fields.insert(i.receiver->fields.begin(),0);
        }
        const bool coordinated_operation =
            i.op == ir::Op::collection_enter ||
            i.op == ir::Op::collection_leave ||
            i.op == ir::Op::collection_append ||
            i.op == ir::Op::collection_finish ||
            i.op == ir::Op::os_query || i.op == ir::Op::host_native ||
            i.op == ir::Op::url_native ||
            i.op == ir::Op::http_native || i.op == ir::Op::http_async || i.op == ir::Op::http_control ||
            i.op == ir::Op::filesystem_native ||
            i.op == ir::Op::filesystem_async ||
            i.op == ir::Op::filesystem_control ||
            i.op == ir::Op::console_native ||
            i.op == ir::Op::console_print ||
            i.op == ir::Op::dict_new ||
            i.op == ir::Op::string_join || i.op == ir::Op::string_join_encode || i.op == ir::Op::local_cleanup || i.op == ir::Op::filesystem_write ||
            i.op == ir::Op::format_unwrap ||
            i.op == ir::Op::format_layout ||
            i.op == ir::Op::text_case || i.op == ir::Op::text_translate ||
            i.op == ir::Op::memory_write ||
            i.op == ir::Op::temporal_result ||
            i.op == ir::Op::scalar_parse ||
            i.op == ir::Op::result_unwrap ||
            i.op == ir::Op::temporal_native ||
            i.op == ir::Op::async_sleep ||
            i.op == ir::Op::random_seed ||
            i.op == ir::Op::scalar_display ||
            i.op == ir::Op::scalar_format;
        if (coordinated_operation) {
            module.coordinated = true;
        }
        i.source = source.location(token.offset);
        if (coordinated_operation ||
            i.op == ir::Op::global_cell ||
            i.op == ir::Op::function_call ||
            i.op == ir::Op::panic ||
            i.op == ir::Op::pattern_equal ||
            i.op == ir::Op::type_cast ||
            i.op == ir::Op::sum_get ||
            i.op == ir::Op::dict_get ||
            i.op == ir::Op::dict_set ||
            i.op == ir::Op::dict_contains ||
            i.op == ir::Op::dict_remove ||
            i.op == ir::Op::dict_clear ||
            i.op == ir::Op::dict_entry ||
            i.op == ir::Op::dict_check ||
            i.op == ir::Op::array_slice ||
            i.op == ir::Op::list_insert ||
            i.op == ir::Op::list_clear ||
            i.op == ir::Op::list_push ||
            i.op == ir::Op::list_remove ||
            i.op == ir::Op::list_index ||
            i.op == ir::Op::list_commit ||
            i.op == ir::Op::array_get ||
            i.op == ir::Op::array_set ||
            i.op == ir::Op::string_count ||
            i.op == ir::Op::string_index ||
            i.op == ir::Op::string_encode ||
            i.op == ir::Op::string_decode ||
            i.op == ir::Op::string_literal ||
            i.op == ir::Op::string_concat ||
            i.op == ir::Op::string_equal ||
            i.op == ir::Op::call ||
            i.op == ir::Op::interface_pack ||
            i.op == ir::Op::interface_prepare ||
            i.op == ir::Op::interface_call ||
            i.op == ir::Op::divide ||
            i.op == ir::Op::remainder ||
            i.op == ir::Op::convert ||
            i.op == ir::Op::gc_construct ||
            i.op == ir::Op::gc_read ||
            i.op == ir::Op::gc_write) {
            i.source_excerpt = source.excerpt(token.offset);
        }
        auto& instructions = output.blocks[*current].instructions;
        auto id = static_cast<unsigned>(instructions.size()); instructions.push_back(std::move(i)); return id;
    }
    unsigned extract_managed(unsigned owner,unsigned field,const Token& token){ir::Instruction read{ir::Op::gc_read,owner};read.immediate=field;return emit(read,token);}
    unsigned repeated_array(Type t,std::uint64_t count,const Token& token,const std::function<unsigned()>& element) {
        ir::Instruction allocation{ir::Op::array_allocate};allocation.type=t;
        auto owner=capture(emit(allocation,token),t,token);
        auto index=snapshot(constant(Type::u64,{0,{}},token),Type::u64,token);
        auto test=block(),body=block(),done=block();jump(test);current=test;
        terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,emit({ir::Op::load,index},token),constant(Type::u64,{count,{}},token)},token),body,done});current=body;
        auto value=element();ir::Instruction initialize{ir::Op::array_initialize,emit({ir::Op::load,owner},token),emit({ir::Op::load,index},token)};initialize.arguments={value};emit(initialize,token);
        emit({ir::Op::store,index,emit({ir::Op::add,emit({ir::Op::load,index},token),constant(Type::u64,{1,{}},token)},token)},token);jump(test);current=done;
        auto value_array=emit({ir::Op::load,owner},token);
        return ir::contains_numeric_enum(t) || (module.c_primitives && module.c_primitives->inline_array(t)) ?
            emit({ir::Op::array_finish,value_array},token) : value_array;
    }
    unsigned constant(Type t, const ir::ConstantValue& value, const Token& token) {
        if(console::endpoint_type(t)){ir::Instruction endpoint{ir::Op::console_endpoint};endpoint.type=t;endpoint.immediate=value.fields.at(0).bits;return emit(endpoint,token);}
        if(t.definition && t.definition->callable){ir::Instruction instruction{value.text.empty()?ir::Op::function_null:ir::Op::function_reference};instruction.type=t;instruction.callee=value.text;return emit(instruction,token);}
        if(t.definition && !t.definition->cases.empty()) {
            ir::Instruction sum{ir::Op::sum};sum.type=t;sum.immediate=value.fields.at(0).bits;
            const auto& variant=t.definition->cases.at(sum.immediate);
            for(unsigned n=0;n<variant.count;++n)sum.arguments.push_back(constant(ir::field_type(t,variant.begin+n),value.fields.at(variant.begin+n),token));
            return emit(sum,token);
        }
        ir::Instruction i{t.kind==Type::dictionary ? ir::Op::dict_new : ir::array_type(t) ? ir::Op::fixed_array : t.kind == Type::aggregate ? ir::Op::aggregate : t==Type::string ? ir::Op::string_literal : ir::Op::constant}; i.type = t;
        if (t==Type::string) {
            i.immediate=strings.intern(value.text,source,token);
            module.coordinated=true; return emit(i,token);
        }
        if(t.kind==Type::dictionary){for(unsigned n=0;n<value.fields.size();n+=2){auto key=constant(ir::dictionary_key(t),value.fields[n],token);auto child=constant(ir::dictionary_value(t),value.fields[n+1],token);i.arguments.push_back(dictionary_pair(key,child,t,token));}return emit(i,token);}
        if(ir::array_type(t)) {
            i.array_repeat=value.repeat.has_value();
            if(value.repeat && *value.repeat!=*t.definition->extent) throw std::logic_error("Mismatched repeated default extent");
            for(const auto& element:value.fields) i.arguments.push_back(constant(ir::array_element(t),element,token));
        }
        else if (t.kind == Type::aggregate) for (unsigned n = 0; n < t.definition->fields.size(); ++n)
            i.arguments.push_back(constant(ir::field_type(t,n),value.fields.at(n),token));
        else i.immediate = std::bit_cast<std::int64_t>(value.bits);
        return emit(i,token);
    }
    unsigned extract(unsigned value, unsigned field, const Token& token) {
        ir::Instruction i{ir::Op::extract,value}; i.immediate = field; return emit(i,token);
    }
    unsigned insert(unsigned aggregate, unsigned value, unsigned field, const Token& token) {
        ir::Instruction i{ir::Op::insert,aggregate,value}; i.immediate = field; return emit(i,token);
    }
    unsigned replace_path(unsigned aggregate, unsigned value, const std::vector<unsigned>& path, unsigned depth, const Token& token) {
        if (depth == path.size()) return value;
        auto child = extract(aggregate,path[depth],token);
        return insert(aggregate,replace_path(child,value,path,depth+1,token),path[depth],token);
    }
    Type expression_type(const Expr& e) const {
        if (e.kind==Expr::Kind::name) return output.locals.at(e.slot);
        auto t=e.type;
        if (e.kind==Expr::Kind::conversion && e.op=="upcast" && expression_type(*e.parts[0]).construction) t.construction=true;
        return t;
    }
    unsigned view_origin(unsigned value) const {
        const auto& instruction=output.blocks[*current].instructions.at(value);
        if (instruction.op==ir::Op::stage_prefix) return view_origin(instruction.a);
        if (instruction.op!=ir::Op::load) throw std::logic_error("construction snapshot requires a selected view");
        return output.construction_origins.at(instruction.a);
    }
    unsigned snapshot(unsigned value, Type t, const Token& token) {
        auto slot=static_cast<unsigned>(output.locals.size()); output.locals.push_back(t);
        if (t.construction) {
            output.construction_origins[slot]=view_origin(value);
        }
        emit({ir::Op::store,slot,value},token); return slot;
    }
    unsigned read_place(const ir::Place& place, const Token& token) {
        auto t=output.locals.at(place.slot); auto value=emit({ir::Op::load,place.slot},token);
        for (auto field:place.fields) {
            ir::Instruction i{t.kind==Type::managed_ref ? ir::Op::gc_read : ir::Op::extract,value}; i.immediate=field;
            value=emit(i,token); t=ir::field_type(t,field);
        }
        return value;
    }
    ir::Place select_place(ir::Place place, const std::vector<unsigned>& path, const Token& token) {
        auto t=output.locals.at(place.slot);
        for (auto field:path) {
            if (t.kind==Type::managed_ref && !(output.initializer && t.construction && place.slot==0 && place.fields.empty())) {
                auto slot=snapshot(read_place(place,token),t,token); place={slot,{}};
            }
            place.fields.push_back(field); t=ir::field_type(t,field);
        }
        return place;
    }
    unsigned global_cell(const Expr& e) {
        ir::Instruction read{ir::Op::global_cell}; read.callee = e.target;
        return emit(read, e.token);
    }
    ir::Place place(const Expr& e) {
        if (e.kind == Expr::Kind::global) {
            auto type = std::find_if(module.globals.begin(), module.globals.end(), [&](const auto& g) { return g.identity == e.target; })->cell;
            return {snapshot(global_cell(e), type, e.token), {0}};
        }
        if (e.kind==Expr::Kind::name) return {e.slot,{}};
        if (e.kind==Expr::Kind::field) {
            if (e.parts[0]->type.kind==Type::managed_ref) {
                auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token);
                return {slot,{e.slot}};
            }
            auto parent=place(*e.parts[0]); parent.fields.push_back(e.slot); return parent;
        }
        auto slot=snapshot(expression(e),expression_type(e),e.token); return {slot,{}};
    }
    void write_place(const ir::Place& place, unsigned value, const Token& token) {
        auto t=output.locals.at(place.slot);
        if (place.fields.empty()) { emit({ir::Op::store,place.slot,value},token); return; }
        auto root=emit({ir::Op::load,place.slot},token);
        if (t.kind==Type::managed_ref) {
            auto field=place.fields.front();
            if (place.fields.size()>1) {
                ir::Instruction read{ir::Op::gc_read,root}; read.immediate=field;
                auto aggregate=emit(read,token);
                value=replace_path(aggregate,value,place.fields,1,token);
            }
            ir::Instruction write{ir::Op::gc_write,root,value}; write.immediate=field; emit(write,token);
        } else {
            value=replace_path(root,value,place.fields,0,token); emit({ir::Op::store,place.slot,value},token);
        }
    }
    struct Selection {Type type;std::function<unsigned()> read;std::function<void(unsigned)> write;bool dependent=false;bool read_before_write=true;};
    unsigned capture(unsigned value,Type type,const Token& token) {
        // A growable handle is an ordinary local alias. The raw array capture
        // operation requires mutable storage authority and is needed only for
        // fixed arrays, where an ordinary store would copy the selected value.
        if(type.kind!=Type::fixed_array)return snapshot(value,type,token);
        auto slot=static_cast<unsigned>(output.locals.size());output.locals.push_back(type);ir::Instruction i{ir::Op::array_capture,value};i.immediate=slot;emit(i,token);return slot;
    }
    unsigned dictionary_pair(unsigned key,unsigned value,Type type,const Token& token) {ir::Instruction pair{ir::Op::aggregate};pair.type=ir::array_element(type);pair.arguments={key,value,constant(Type::u64,{0,{}},token)};return emit(pair,token);}
    void dictionary_set(unsigned owner,unsigned key,unsigned value,unsigned version,Type type,const Token& token) {
        ir::Instruction write{ir::Op::dict_set,owner,dictionary_pair(key,value,type,token)};write.arguments={version};emit(write,token);
    }
    Selection select_expression(const Expr& e) {
        auto token=e.token;
        if (e.kind == Expr::Kind::global) {
            auto selected = place(e);
            return {e.type, [=, this] { return read_place(selected, token); }, [=, this](unsigned value) { write_place(selected, value, token); }};
        }
        if(e.kind==Expr::Kind::name) {
            auto slot=e.slot;return {e.type,[=,this]{return emit({ir::Op::load,slot},token);},[=,this](unsigned value){emit({ir::Op::store,slot,value},token);}};
        }
        if(e.property) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));
            const bool managed=parent->type.kind==Type::managed_ref;
            // A computed value receiver is evaluated before the RHS. A normal
            // local value receiver keeps its selected live storage instead.
            if(e.selected_receiver && !managed)parent->read();
            std::optional<unsigned> owner;if(managed)owner=snapshot(parent->read(),parent->type,token);
            auto value_slot=std::make_shared<std::optional<unsigned>>();
            const bool setter_mutates=e.setter_mutates;
            std::optional<ir::Place> global_receiver;
            if (e.receiver_global && !e.selected_receiver && parent->type.kind == Type::aggregate) global_receiver = place(*e.parts[0]);
            auto invoke=[=,this](const std::string& target,std::optional<unsigned> value) {
                auto slot=owner.value_or(~0u);
                if(!owner && !global_receiver)slot=snapshot(parent->read(),parent->type,token);
                ir::Instruction call{ir::Op::call};call.callee=target;call.receiver=global_receiver.value_or(ir::Place{slot,{}});if(value)call.arguments={*value};
                auto result=emit(call,token);
                if(value && !managed && setter_mutates && !global_receiver)parent->write(emit({ir::Op::load,slot},token));
                return result;
            };
            auto type=e.type;auto getter=e.target,setter=e.setter;
            return {type,[=,this]{if(!*value_slot)*value_slot=snapshot(invoke(getter,{}),type,token);return emit({ir::Op::load,**value_slot},token);},[=](unsigned value){invoke(setter,value);},parent->dependent,false};
        }
        if(e.kind==Expr::Kind::field) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));auto field=e.slot;
            if(parent->type.kind==Type::managed_ref) {
                auto owner=snapshot(parent->read(),parent->type,token);
                return {e.type,[=,this]{ir::Instruction i{ir::Op::gc_read,emit({ir::Op::load,owner},token)};i.immediate=field;return emit(i,token);},[=,this](unsigned value){ir::Instruction i{ir::Op::gc_write,emit({ir::Op::load,owner},token),value};i.immediate=field;emit(i,token);}};
            }
            return {e.type,[=,this]{ir::Instruction i{ir::Op::extract,parent->read()};i.immediate=field;return emit(i,token);},[=,this](unsigned value){ir::Instruction i{ir::Op::insert,parent->read(),value};i.immediate=field;parent->write(emit(i,token));},parent->dependent};
        }
        if(e.kind==Expr::Kind::array_index) {
            auto parent=std::make_shared<Selection>(select_expression(*e.parts[0]));
            if(parent->type.kind==Type::dictionary) {
                auto owner=snapshot(parent->read(),parent->type,token);auto key=snapshot(expression(*e.parts[1]),e.parts[1]->type,token);auto version=snapshot(emit({ir::Op::dict_version,emit({ir::Op::load,owner},token)},token),Type::u64,token);
                return {e.type,[=,this]{auto object=emit({ir::Op::load,owner},token);emit({ir::Op::dict_check,object,emit({ir::Op::load,version},token)},token);return emit({ir::Op::dict_get,object,emit({ir::Op::load,key},token)},token);},[=,this](unsigned value){dictionary_set(emit({ir::Op::load,owner},token),emit({ir::Op::load,key},token),value,emit({ir::Op::load,version},token),parent->type,token);},true,false};
            }
            // Reading a nested list reference severs its former inline ancestry.
            std::optional<unsigned> selected_owner;
            if(parent->type.kind==Type::growable_array)selected_owner=capture(parent->read(),parent->type,token);
            auto index=snapshot(expression(*e.parts[1]),e.parts[1]->type,token);
            if(parent->type.kind==Type::growable_array) {
                auto owner=selected_owner.value_or(~0u);if(!selected_owner)owner=capture(parent->read(),parent->type,token);
                auto normalized=snapshot(emit({ir::Op::list_index,emit({ir::Op::load,owner},token),emit({ir::Op::load,index},token)},token),Type::u64,token);
                auto version=snapshot(emit({ir::Op::list_version,emit({ir::Op::load,owner},token)},token),Type::u64,token);
                return {e.type,[=,this]{auto object=emit({ir::Op::load,owner},token);emit({ir::Op::list_check,object,emit({ir::Op::load,version},token)},token);return emit({ir::Op::array_get,object,emit({ir::Op::load,normalized},token)},token);},[=,this](unsigned value){ir::Instruction i{ir::Op::list_commit,emit({ir::Op::load,owner},token),emit({ir::Op::load,normalized},token)};i.arguments={value,emit({ir::Op::load,version},token)};emit(i,token);},true};
            }
            // Bounds/index effects precede the RHS, while later reads reacquire
            // the current fixed value from the original selected owning place.
            emit({ir::Op::array_get,parent->read(),emit({ir::Op::load,index},token)},token);
            return {e.type,[=,this]{return emit({ir::Op::array_get,parent->read(),emit({ir::Op::load,index},token)},token);},[=,this](unsigned value){ir::Instruction i{ir::Op::array_set,parent->read(),emit({ir::Op::load,index},token)};i.arguments={value};parent->write(emit(i,token));},parent->dependent};
        }
        auto slot=capture(expression(e),e.type,token);
        return {e.type,[=,this]{return emit({ir::Op::load,slot},token);},[=,this](unsigned value){emit({ir::Op::store,slot,value},token);}};
    }
#include "pattern_lowering.inc"
    VOID_SEMANTIC_HANDLER unsigned spawned(const Expr& e) {
            auto& call=*e.parts[0];ir::Function wrapper;
            const auto owner=output.name.substr(0,output.name.find("::"));
            const auto target_owner=call.target.substr(0,call.target.find("::"));
            const auto suffix="::$spawn."+std::to_string(module.functions.size());
            const bool package_call=package_source(owner) || package_source(target_owner);
            wrapper.module_identity=package_call ? owner : target_owner;
            wrapper.name=(package_call ? output.name : call.target)+suffix;
            if(!output.lexical_owner.empty() && owner==target_owner && call.target.starts_with(output.lexical_owner+"::static::")) {
                wrapper.name=call.target+suffix;wrapper.static_member=true;wrapper.lexical_owner=output.lexical_owner;
            }
            wrapper.source_name=wrapper.name.substr(wrapper.module_identity.size()+2);wrapper.source=source.location(e.token.offset);wrapper.visibility="private";wrapper.asynchronous=true;wrapper.worker=true;wrapper.task=e.type;wrapper.result=call.type;wrapper.parameters.clear();
            ir::Block body;ir::Instruction invoke{ir::Op::call};invoke.callee=call.target;invoke.source=wrapper.source;
            wrapper.parameters.resize(call.parts.size());for(unsigned n=0;n<call.parts.size();++n){wrapper.parameters[call.argument_indexes.empty()?n:call.argument_indexes[n]]=call.parts[n]->type;invoke.arguments.push_back(n);body.instructions.push_back({ir::Op::argument,n});}
            unsigned value=body.instructions.size();body.instructions.push_back(invoke);
            if(call.type!=Type::none){wrapper.locals.push_back(call.type);body.instructions.push_back({ir::Op::store,0,value});}
            body.end={ir::Terminator::Kind::jump,0,1};wrapper.blocks.push_back(std::move(body));wrapper.cleanup_entry=1;
            ir::Block cleanup;cleanup.instructions.push_back({ir::Op::async_drain});if(call.type==Type::none)cleanup.end={ir::Terminator::Kind::return_void};else{cleanup.instructions.push_back({ir::Op::load,0});cleanup.end={ir::Terminator::Kind::return_value,1};}wrapper.blocks.push_back(std::move(cleanup));
            auto target=wrapper.name;module.functions.push_back(std::move(wrapper));return expression(call,target);

    }
    VOID_SEMANTIC_HANDLER unsigned safe_collection(const Expr& e) {
        const auto token=e.token;const auto type=e.parts[0]->type;
        const bool dictionary=type.kind==Type::dictionary,pop=e.target=="$safe.array.pop";
        auto owner=snapshot(expression(*e.parts[0]),type,token);
        std::optional<unsigned> argument;if(!pop)argument=snapshot(expression(*e.parts[1]),e.parts[1]->type,token);
        auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};
        auto number=[&](Type t,std::uint64_t bits){return constant(t,{bits,{}},token);};
        auto result=static_cast<unsigned>(output.locals.size());output.locals.push_back(e.type);
        auto success=block(),missing=block(),done=block();
        if(dictionary)terminate({ir::Terminator::Kind::branch,emit({ir::Op::dict_contains,load(owner),load(*argument)},token),success,missing});
        else {
            auto count=snapshot(emit({ir::Op::array_count,load(owner)},token),Type::i64,token);
            if(pop)terminate({ir::Terminator::Kind::branch,emit({ir::Op::greater,load(count),number(Type::i64,0)},token),success,missing});
            else {
                auto index_type=e.parts[1]->type;const bool sign=numeric::signed_integer(index_type);
                ir::Instruction widen{ir::Op::convert,load(*argument)};widen.type=sign?Type::i64:Type::u64;
                auto index=snapshot(emit(widen,token),widen.type,token);
                if(sign){auto negative=block(),check=block(),upper=block();terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,load(index),number(Type::i64,0)},token),negative,check});
                    current=negative;emit({ir::Op::store,index,emit({ir::Op::add,load(index),load(count)},token)},token);jump(check);
                    current=check;terminate({ir::Terminator::Kind::branch,emit({ir::Op::greater_equal,load(index),number(Type::i64,0)},token),upper,missing});
                    current=upper;terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,load(index),load(count)},token),success,missing});
                }else{ir::Instruction unsigned_count{ir::Op::convert,load(count)};unsigned_count.type=Type::u64;terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,load(index),emit(unsigned_count,token)},token),success,missing});}
            }
        }
        current=success;auto index=pop?number(Type::i64,~std::uint64_t{}):load(*argument);
        auto value=emit({dictionary?ir::Op::dict_get:pop?ir::Op::list_remove:ir::Op::array_get,load(owner),index},token);
        ir::Instruction ok{ir::Op::sum};ok.type=e.type;ok.arguments={value};emit({ir::Op::store,result,emit(ok,token)},token);jump(done);
        current=missing;ir::ConstantValue error;error.text=dictionary?"key not found":pop?"empty list":"index out of range";
        ir::Instruction failure{ir::Op::sum};failure.type=e.type;failure.immediate=1;failure.arguments={constant(Type::string,error,token)};emit({ir::Op::store,result,emit(failure,token)},token);jump(done);
        current=done;return load(result);
    }
    unsigned expression(const Expr& e,std::string_view parallel_target={}) {
        using K = Expr::Kind;
        switch (e.kind) {
        case K::editor_hole:throw std::logic_error("Editor recovery cannot lower an enum hole");
        case K::lambda:throw std::logic_error("Unanalyzed anonymous function");
        case K::capture_reference:return frontend_case([&]() -> unsigned {
            if(!e.implicit)return emit({ir::Op::load,cells.at(e.slot)},e.token);
            ir::Instruction cell{ir::Op::gc_construct};cell.type=e.type;cell.arguments={e.parts.empty()?emit({ir::Op::load,e.slot},e.token):expression(*e.parts[0])};return emit(cell,e.token);
        });
        case K::function_reference:return frontend_case([&]() -> unsigned {ir::Instruction reference{ir::Op::function_reference};reference.type=e.type;reference.callee=e.target;
            if(e.op=="bound")reference.arguments={expression(*e.parts[0])};
            else if(!e.parts.empty() || e.implicit){ir::Instruction environment{ir::Op::gc_construct};
                for(const auto& function:module.functions)if(function.name==e.target)environment.type=function.parameters.at(0);
                // The concrete environment is retained on the source expression.
                environment.type=e.environment;
                for(const auto& part:e.parts)environment.arguments.push_back(expression(*part));reference.arguments={emit(environment,e.token)};
            }
            return emit(reference,e.token);});
        case K::function_null:return frontend_case([&]() -> unsigned {ir::Instruction value{ir::Op::function_null};value.type=e.type;return emit(value,e.token);});
        case K::function_call:return frontend_case([&]() -> unsigned {auto function=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);std::vector<unsigned> arguments;for(unsigned n=1;n<e.parts.size();++n)arguments.push_back(snapshot(expression(*e.parts[n]),e.parts[n]->type,e.token));ir::Instruction call{ir::Op::function_call,emit({ir::Op::load,function},e.token)};for(auto slot:arguments)call.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(call,e.token);});
        case K::test: return lower_test(e);
        case K::match: return lower_match(e);
        case K::choice: return frontend_case([&]() -> unsigned {
            auto result=static_cast<unsigned>(output.locals.size());output.locals.push_back(e.type);
            auto yes=block(),no=block(),done=block();lower_condition(*e.parts[0],yes,no);
            current=yes;emit({ir::Op::store,result,expression(*e.parts[1])},e.token);jump(done);
            current=no;emit({ir::Op::store,result,expression(*e.parts[2])},e.token);jump(done);
            current=done;return emit({ir::Op::load,result},e.token);
        });
        case K::has: throw std::logic_error("Unchecked has expression");
        case K::sum: return frontend_case([&]() -> unsigned {
            ir::Instruction construct{ir::Op::sum};construct.type=e.type;construct.immediate=e.slot;
            std::vector<unsigned> slots;for(const auto& child:e.parts)slots.push_back(snapshot(expression(*child),child->type,child->token));
            for(auto slot:slots)construct.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(construct,e.token);
        });
        case K::return_pack: return frontend_case([&]() -> unsigned {
            ir::Instruction pack{ir::Op::aggregate};pack.type=e.type;
            std::vector<unsigned> slots;
            for(const auto& part:e.parts)slots.push_back(snapshot(expression(*part),part->type,part->token));
            for(auto slot:slots)pack.arguments.push_back(emit({ir::Op::load,slot},e.token));
            return emit(pack,e.token);
        });
        case K::propagate: return frontend_case([&]() -> unsigned {
            const auto type=e.parts[0]->type;
            auto source=capture(expression(*e.parts[0]),type,e.token);
            auto success=block(),failure=block();
            auto tag=extract(emit({ir::Op::load,source},e.token),0,e.token);
            terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,tag,constant(Type::i32,{0,{}},e.token)},e.token),success,failure});
            current=failure;ir::Instruction payload{ir::Op::sum_get,emit({ir::Op::load,source},e.token)};payload.immediate=2;
            auto value=emit(payload,e.token);ir::Instruction propagate{ir::Op::sum};propagate.type=output.result;propagate.immediate=1;propagate.arguments={value};
            return_through_cleanup(emit(propagate,e.token),e.token,true);
            current=success;ir::Instruction payload_ok{ir::Op::sum_get,emit({ir::Op::load,source},e.token)};payload_ok.immediate=1;return emit(payload_ok,e.token);
        });
        case K::array_default: return repeated_array(e.type,*e.type.definition->extent,e.token,[&]{return expression(*e.parts[0]);});
        case K::dictionary_literal: return frontend_case([&]() -> unsigned {
            std::vector<unsigned> pairs;for(unsigned n=0;n<e.parts.size();n+=2){auto key=snapshot(expression(*e.parts[n]),e.parts[n]->type,e.token);auto value=snapshot(expression(*e.parts[n+1]),e.parts[n+1]->type,e.token);pairs.push_back(snapshot(dictionary_pair(emit({ir::Op::load,key},e.token),emit({ir::Op::load,value},e.token),e.type,e.token),ir::array_element(e.type),e.token));}
            ir::Instruction create{ir::Op::dict_new};create.type=e.type;for(auto pair:pairs)create.arguments.push_back(emit({ir::Op::load,pair},e.token));return emit(create,e.token);
        });
        case K::array_literal: return frontend_case([&]() -> unsigned {
            ir::Instruction i{ir::Op::fixed_array};i.type=e.type;
            std::vector<unsigned> slots;
            for(const auto& part:e.parts) {auto value=expression(*part);slots.push_back(snapshot(value,part->type,part->token));}
            for(auto slot:slots) i.arguments.push_back(emit({ir::Op::load,slot},e.token));
            return emit(i,e.token);
        });
        case K::array_index: return frontend_case([&]() -> unsigned {
            auto owner_type=e.parts[0]->type;
            if(owner_type.kind==Type::growable_array && ir::scalar_array(owner_type)) {
                // A value read has no selected-write lifetime: after evaluating
                // the index, no source code runs between normalization and read.
                // Capture the list before index effects (including rebinding or
                // resizing it), then use the existing checked array_get path.
                // Assignment and selected receivers still use select_expression.
                auto owner=snapshot(expression(*e.parts[0]),owner_type,e.token);
                auto index=expression(*e.parts[1]);
                return emit({ir::Op::array_get,emit({ir::Op::load,owner},e.token),index},e.token);
            }
            if(e.target!="$text.index")return select_expression(e).read();
            auto owner=snapshot(expression(*e.parts[0]),Type::string,e.token);auto index=expression(*e.parts[1]);
            return emit({ir::Op::string_index,emit({ir::Op::load,owner},e.token),index},e.token);
        });
        case K::array_slice: return frontend_case([&]() -> unsigned {
            auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);
            auto first=snapshot(expression(*e.parts[1]),e.parts[1]->type,e.token);
            auto last=snapshot(expression(*e.parts[2]),e.parts[2]->type,e.token);
            ir::Instruction slice{ir::Op::array_slice,emit({ir::Op::load,owner},e.token),emit({ir::Op::load,first},e.token)};slice.type=e.type;slice.immediate=e.op=="..<";slice.arguments={emit({ir::Op::load,last},e.token)};return emit(slice,e.token);
        });
        case K::base_initialization: throw std::logic_error("base initialization is a dedicated prelude");
        case K::literal: return frontend_case([&]() -> unsigned {
            return constant(e.type,(e.type.kind == Type::aggregate || ir::collection_type(e.type) || e.type==Type::string) ? e.aggregate_value : ir::ConstantValue{e.value,{}},e.token);
        });
        case K::field: return frontend_case([&]() -> unsigned { auto value=expression(*e.parts[0]); if(e.target=="$text.length" || e.target=="$text.byte_length"){ir::Instruction i{ir::Op::string_count,value};i.immediate=e.target=="$text.byte_length";return emit(i,e.token);} if(e.target=="$dict.count")return emit({ir::Op::dict_count,value},e.token);if(e.target=="$array.count")return emit({ir::Op::array_count,value},e.token); ir::Instruction i{e.parts[0]->type.kind==Type::managed_ref ? ir::Op::gc_read : ir::Op::extract,value}; i.immediate=e.slot; return emit(i,e.token); });
        case K::construction: return frontend_case([&]() -> unsigned {
            const bool fallible=e.constructed_type!=Type::none;
            const auto owner_type=fallible?e.constructed_type:e.type;
            std::optional<unsigned> outcome,answer,done;
            if(fallible){answer=static_cast<unsigned>(output.locals.size());output.locals.push_back(e.type);done=block();}
            std::vector<unsigned> arguments;
            for (const auto& part : e.parts) {
                auto argument = static_cast<unsigned>(output.locals.size()); output.locals.push_back(part->type);
                auto value = expression(*part); emit({ir::Op::store,argument,value},part->token); arguments.push_back(argument);
            }
            if (e.initializer.empty()) {
                std::vector<unsigned> fields;
                for (unsigned field=0;field<owner_type.definition->fields.size();++field) {
                    Type t=ir::field_type(owner_type,field);
                    std::optional<unsigned> selected;
                    for (unsigned n=0;n<arguments.size();++n) if (e.field_indexes[n]==field) selected=emit({ir::Op::load,arguments[n]},e.token);
                    for (const auto& child:e.omitted) if (child->slot==field) selected=expression(*child);
                    auto value=selected ? *selected : constant(t,e.aggregate_value.fields.at(field),e.token);
                    fields.push_back(snapshot(value,t,e.token));
                }
                ir::Instruction i{owner_type.kind==Type::managed_ref ? ir::Op::gc_construct : ir::Op::aggregate}; i.type=owner_type;
                for (auto field:fields) i.arguments.push_back(emit({ir::Op::load,field},e.token));
                return emit(i,e.token);
            }
            bool staged=owner_type.kind==Type::managed_ref;
            auto storage_type=owner_type; storage_type.construction=staged;
            auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(storage_type);
            unsigned initial;
            if (staged) {
                output.construction_origins[slot]=slot;
                ir::Instruction begin{ir::Op::stage_begin,slot}; begin.type=storage_type; initial=emit(begin,e.token);
            } else initial=constant(owner_type,e.aggregate_value,e.token);
            emit({ir::Op::store,slot,initial},e.token);
            for (unsigned field=0;!staged && field<owner_type.definition->fields.size();++field) {
                std::optional<unsigned> value;
                if (staged) {
                    Type field_type=owner_type.definition->fields[field].type;
                    if (ir::requires_initialization(field_type)) continue;
                    value=constant(field_type,e.aggregate_value.fields.at(field),e.token);
                }
                if (e.initializer.empty()) for (unsigned n=0;n<arguments.size();++n)
                    if (e.field_indexes[n]==field) value = emit({ir::Op::load,arguments[n]},e.token);
                for (const auto& child : e.omitted) if (child->slot==field) value = expression(*child);
                if (value) {
                    auto aggregate = emit({ir::Op::load,slot},e.token);
                    if (staged) { ir::Instruction write{ir::Op::gc_write,aggregate,*value}; write.immediate=field; emit(write,e.token); }
                    else { auto updated = insert(aggregate,*value,field,e.token); emit({ir::Op::store,slot,updated},e.token); }
                }
            }
            if (!e.initializer.empty()) {
                ir::Instruction call{ir::Op::call}; call.callee=e.initializer; call.receiver=ir::Place{slot,{}}; call.initialization=true;
                call.arguments.resize(arguments.size());
                for (unsigned n=0;n<arguments.size();++n) call.arguments[e.field_indexes[n]]=emit({ir::Op::load,arguments[n]},e.token);
                const auto invoked=emit(call,e.token);
                if(fallible) {
                    outcome=capture(invoked,e.initializer_result,e.token);
                    auto yes=block(),no=block();
                    unsigned check;
                    if(staged){ir::Instruction checked{ir::Op::stage_check,invoked};checked.receiver=ir::Place{slot,{}};check=emit(checked,e.token);}
                    else check=emit({ir::Op::equal,extract(emit({ir::Op::load,*outcome},e.token),0,e.token),constant(Type::i32,{0,{}},e.token)},e.token);
                    terminate({ir::Terminator::Kind::branch,check,yes,no});
                    current=no;
                    ir::Instruction payload{ir::Op::sum_get,emit({ir::Op::load,*outcome},e.token)};payload.immediate=2;
                    auto error=emit(payload,e.token);
                    if(staged){ir::Instruction abort{ir::Op::stage_abort};abort.receiver=ir::Place{slot,{}};emit(abort,e.token);}
                    else emit({ir::Op::store,slot,constant(owner_type,e.aggregate_value,e.token)},e.token);
                    ir::Instruction failed{ir::Op::sum};failed.type=e.type;failed.immediate=1;failed.arguments={error};
                    emit({ir::Op::store,*answer,emit(failed,e.token)},e.token);jump(*done);current=yes;
                }
            }
            if (staged) {
                ir::Instruction commit{ir::Op::gc_construct}; commit.type=owner_type; commit.receiver=ir::Place{slot,{}};
                for (unsigned field=0;field<owner_type.definition->fields.size();++field) {
                    auto view=emit({ir::Op::load,slot},e.token);
                    ir::Instruction read{ir::Op::gc_read,view}; read.immediate=field;
                    commit.arguments.push_back(emit(read,e.token));
                }
                auto value=emit(commit,e.token);
                if(!fallible)return value;
                ir::Instruction ok{ir::Op::sum};ok.type=e.type;ok.arguments={value};
                emit({ir::Op::store,*answer,emit(ok,e.token)},e.token);jump(*done);current=*done;
                return emit({ir::Op::load,*answer},e.token);
            }
            auto value=emit({ir::Op::load,slot},e.token);
            if(!fallible)return value;
            ir::Instruction ok{ir::Op::sum};ok.type=e.type;ok.arguments={value};
            emit({ir::Op::store,*answer,emit(ok,e.token)},e.token);jump(*done);current=*done;
            return emit({ir::Op::load,*answer},e.token);
        });
        case K::spawned:return spawned(e);
        case K::task_cancel: return emit({ir::Op::async_cancel,expression(*e.parts[0])},e.token);
        case K::awaited: return emit({ir::Op::await,expression(*e.parts[0])},e.token);
        case K::global: return extract_managed(global_cell(e), 0, e.token);
        case K::name: return emit({ir::Op::load, e.slot}, e.token);
        case K::method_call: case K::call: return frontend_case([&]() -> unsigned {
            // Finish builtin dispatch before recursively lowering an ordinary
            // receiver, so unrelated builtin temporaries do not stack per call.
            auto builtin=frontend_case([&]() -> std::optional<unsigned> {
            if(e.target=="$console.print")return frontend_case([&]() -> unsigned {
                std::vector<unsigned> captured(e.parts.size());
                for(unsigned n=0;n<e.parts.size();++n){
                    // Bare null has no runtime allocation/identity to capture.
                    // Its shared display literal is materialized after ALL args.
                    if(e.argument_indexes[n]<e.slot && e.parts[n]->op=="$print.null")continue;
                    const auto& raw=e.argument_indexes[n]<e.slot?*e.parts[n]->parts[0]:*e.parts[n];captured[n]=snapshot(expression(raw),expression_type(raw),raw.token);
                }
                std::vector<unsigned> rendered;std::optional<unsigned> sep,end,file,flush;
                for(unsigned n=0;n<e.parts.size();++n){auto index=e.argument_indexes[n];if(index>=e.slot){auto slot=captured[n];if(index==e.slot)sep=slot;else if(index==e.slot+1)end=slot;else if(index==e.slot+2)file=slot;else flush=slot;continue;}
                    const auto& wrapper=*e.parts[n];
                    if(wrapper.op=="$print.null"){rendered.push_back(snapshot(constant(Type::string,wrapper.parts[0]->aggregate_value,wrapper.token),Type::string,wrapper.token));continue;}
                    auto value=emit({ir::Op::load,captured[n]},wrapper.token);ir::Instruction display{ir::Op::scalar_display,value};display.type=Type::string;
                    if(wrapper.target!="$display.string"){display.op=ir::Op::call;display.a=0;display.callee=wrapper.target;display.arguments={value};}
                    rendered.push_back(snapshot(emit(display,wrapper.token),Type::string,wrapper.token));
                }
                if(!sep)sep=snapshot(constant(Type::string,{0,{}," "},e.token),Type::string,e.token);
                if(!end)end=snapshot(constant(Type::string,{0,{},"\n"},e.token),Type::string,e.token);
                ir::Instruction join{ir::Op::string_join};join.type=Type::string;
                for(unsigned n=0;n<rendered.size();++n){if(n)join.arguments.push_back(emit({ir::Op::load,*sep},e.token));join.arguments.push_back(emit({ir::Op::load,rendered[n]},e.token));}join.arguments.push_back(emit({ir::Op::load,*end},e.token));
                ir::Instruction print{ir::Op::console_print,emit(join,e.token),flush?emit({ir::Op::load,*flush},e.token):constant(Type::boolean,{0,{}},e.token)};print.type=Type::none;if(file)print.arguments.push_back(emit({ir::Op::load,*file},e.token));return emit(print,e.token);
            });
            if (e.target.starts_with("$host.")) {
                std::vector<unsigned> slots(e.parts.size());
                for(unsigned n=0;n<e.parts.size();++n)slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),expression_type(*e.parts[n]),e.parts[n]->token);
                ir::Instruction operation{ir::Op::host_native};operation.type=e.type;operation.immediate=std::stoul(e.target.substr(6));
                for(auto slot:slots)operation.arguments.push_back(emit({ir::Op::load,slot},e.token));
                return emit(operation,e.token);
            }
            if (e.target.starts_with("$os.")) {
                ir::Instruction operation{ir::Op::os_query};
                operation.type = e.type;
                operation.immediate = std::stoul(e.target.substr(4));
                for (auto& part : e.parts) operation.arguments.push_back(expression(*part));
                return emit(operation, e.token);
            }
            if (e.target.starts_with("$http.")) {
                const bool asynchronous = e.target.starts_with("$http.async.");
                std::vector<unsigned> slots(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n)
                    slots[e.argument_indexes.at(n)] = snapshot(expression(*e.parts[n]), expression_type(*e.parts[n]), e.parts[n]->token);
                ir::Instruction operation{asynchronous ? ir::Op::http_async : ir::Op::http_native};
                operation.type = e.type; operation.immediate = std::stoul(e.target.substr(12 - !asynchronous));
                if (operation.immediate >= static_cast<unsigned>(http::SourceOperation::cleanup_client)) operation.op = ir::Op::http_control;
                for (auto slot : slots) operation.arguments.push_back(emit({ir::Op::load, slot}, e.token));
                return emit(operation, e.token);
            }
            if (e.target.starts_with("$url.")) {
                std::vector<unsigned> slots(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n)
                    slots[e.argument_indexes.at(n)] = snapshot(expression(*e.parts[n]), expression_type(*e.parts[n]), e.parts[n]->token);
                ir::Instruction operation{ir::Op::url_native};
                operation.type = e.type; operation.immediate = std::stoul(e.target.substr(5));
                for (auto slot : slots) operation.arguments.push_back(emit({ir::Op::load, slot}, e.token));
                return emit(operation, e.token);
            }
            if(e.target=="$io.write.checked") {
                std::vector<unsigned> slots(e.parts.size());
                for(unsigned n=0;n<e.parts.size();++n)slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),expression_type(*e.parts[n]),e.parts[n]->token);
                ir::Instruction call{ir::Op::filesystem_write};call.type=e.type;call.immediate=static_cast<unsigned>(filesystem::Operation::write_bytes);
                for(auto slot:slots)call.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(call,e.token);
            }
            if (e.target.starts_with("$io.")) {
                bool asynchronous = e.target.starts_with("$io.async.");
                std::vector<unsigned> slots(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n) {
                    slots[e.argument_indexes.at(n)] = snapshot(expression(*e.parts[n]),
                                                              expression_type(*e.parts[n]), e.parts[n]->token);
                }
                ir::Instruction operation{asynchronous ? ir::Op::filesystem_async : ir::Op::filesystem_native};
                operation.type = e.type;
                operation.immediate = std::stoul(e.target.substr(asynchronous ? 10 : 9));
                if (operation.immediate >= filesystem::claim_operation) operation.op = ir::Op::filesystem_control;
                for (auto slot : slots) operation.arguments.push_back(emit({ir::Op::load, slot}, e.token));
                return emit(operation, e.token);
            }
            if(e.target.starts_with("$console.")) {
                std::vector<unsigned> slots(e.parts.size());for(unsigned n=0;n<e.parts.size();++n)slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),expression_type(*e.parts[n]),e.parts[n]->token);
                ir::Instruction operation{ir::Op::console_native};operation.type=e.type;operation.immediate=std::stoul(e.target.substr(9));for(auto slot:slots)operation.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(operation,e.token);
            }
            if (e.target == "$random.domain") { ir::Instruction i{ir::Op::constant}; i.type=Type::u64; i.immediate=e.value; return emit(i,e.token); }
            if (e.target == "$random.xor") { auto a=expression(*e.parts[0]); auto b=expression(*e.parts[1]); ir::Instruction i{ir::Op::random_xor,a,b}; i.type=Type::u64; return emit(i,e.token); }
            if (e.target == "$random.seed") { ir::Instruction i{ir::Op::random_seed}; i.type=e.type; return emit(i,e.token); }
            if (e.target.starts_with("$math.")) return frontend_case([&]() -> unsigned {
                std::vector<unsigned> slots(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n)
                    slots.at(e.argument_indexes.at(n)) = snapshot(expression(*e.parts[n]), expression_type(*e.parts[n]), e.parts[n]->token);
                ir::Instruction operation{ir::Op::math};
                operation.immediate = std::stoll(e.target.substr(6));
                operation.type = e.type;
                operation.a = emit({ir::Op::load, slots.at(0)}, e.token);
                if (slots.size() == 2) operation.b = emit({ir::Op::load, slots.at(1)}, e.token);
                return emit(operation, e.token);
            });
            if (e.target.starts_with("$binary.")) {
                if (e.target == "$binary.width") {
                    return constant(Type::i64, {e.value, {}}, e.token);
                }

                const bool encoding = e.target == "$binary.bits";
                const auto numeric_type = encoding ? e.parts[0]->type : e.type;
                Type storage_type;

                switch (numeric::width(numeric_type)) {
                case 8:
                    storage_type = Type::u8;
                    break;
                case 16:
                    storage_type = Type::u16;
                    break;
                case 32:
                    storage_type = Type::u32;
                    break;
                default:
                    storage_type = Type::u64;
                    break;
                }

                auto value = expression(*e.parts[0]);

                if (encoding) {
                    ir::Instruction bits{ir::Op::bitcast, value};
                    bits.type = storage_type;
                    value = emit(bits, e.token);

                    ir::Instruction widen{ir::Op::convert, value};
                    widen.type = Type::u64;
                    return emit(widen, e.token);
                }

                ir::Instruction narrow{ir::Op::convert, value};
                narrow.type = storage_type;
                value = emit(narrow, e.token);

                ir::Instruction number{ir::Op::bitcast, value};
                number.type = numeric_type;
                return emit(number, e.token);
            }

            if (e.target == "$convert.parse" || e.target == "$convert.unwrap") {
                ir::Instruction operation{e.target == "$convert.parse" ?
                    ir::Op::scalar_parse : ir::Op::result_unwrap, expression(*e.parts[0])};
                operation.type = e.type;
                return emit(operation, e.token);
            }

            if (e.target == "$text.case") {
                ir::Instruction operation{ir::Op::text_case,
                    expression(*e.parts[0]), expression(*e.parts[1])};
                operation.type = Type::string;
                return emit(operation, e.token);
            }

            if (e.target == "$text.translate") {
                ir::Instruction operation{ir::Op::text_translate};
                operation.type = e.type;
                operation.arguments.resize(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n)
                    operation.arguments.at(e.argument_indexes.at(n)) = expression(*e.parts[n]);
                return emit(operation, e.token);
            }

            if (e.target == "$memory.write") {
                ir::Instruction operation{ir::Op::memory_write};
                operation.type = Type::none;
                for (auto& part : e.parts) {
                    operation.arguments.push_back(expression(*part));
                }
                return emit(operation, e.token);
            }

            if (e.target == "$temporal.result") {
                ir::Instruction operation{ir::Op::temporal_result, expression(*e.parts[0])};
                operation.type = e.type;
                return emit(operation, e.token);
            }

            if(e.target=="$async.sleep") {
                std::array<unsigned,2> slots{};
                for(unsigned n=0;n<e.parts.size();++n)
                    slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),e.parts[n]->type,e.parts[n]->token);
                auto seconds=emit({ir::Op::load,slots[0]},e.token);
                auto nanos=emit({ir::Op::load,slots[1]},e.token);
                ir::Instruction operation{ir::Op::async_sleep,seconds,nanos};
                operation.type=e.type;
                return emit(operation,e.token);
            }
            if(e.target.starts_with("$time.")) {
                std::vector<unsigned> slots(e.parts.size());for(unsigned n=0;n<e.parts.size();++n)slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),e.parts[n]->type,e.parts[n]->token);
                ir::Instruction operation{ir::Op::temporal_native};operation.type=e.type;operation.immediate=std::stoul(e.target.substr(6));for(auto slot:slots)operation.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(operation,e.token);
            }
            if(e.target=="$layout.unwrap"){ir::Instruction unwrap{ir::Op::format_unwrap,expression(*e.parts[0])};unwrap.type=Type::string;return emit(unwrap,e.token);}
            if (e.target == "$collections.version" || e.target == "$collections.check") {
                // Capture arguments once in source order, as for ordinary calls.
                std::vector<unsigned> slots(e.parts.size());
                for (unsigned n = 0; n < e.parts.size(); ++n)
                    slots[e.argument_indexes.at(n)] = snapshot(expression(*e.parts[n]), expression_type(*e.parts[n]), e.parts[n]->token);
                ir::Instruction operation{e.target == "$collections.check" ? ir::Op::list_check : ir::Op::list_version,
                                          emit({ir::Op::load, slots[0]}, e.token)};
                if (slots.size() == 2) operation.b = emit({ir::Op::load, slots[1]}, e.token);
                return emit(operation, e.token);
            }
            if(e.target.starts_with("$collection.")) {
                const auto operation=e.target.substr(12);unsigned owner=0;if(!e.parts.empty())owner=expression(*e.parts[0]);
                if(operation=="invalid"){ir::Instruction fail{ir::Op::panic};fail.callee="Invalid enum/result variant tag during display";return emit(fail,e.token);}
                if(operation=="tag")return emit({ir::Op::extract,owner},e.token);
                if(operation=="payload"){ir::Instruction get{ir::Op::sum_get,owner};get.immediate=e.parts.at(1)->value;return emit(get,e.token);}
                if(operation=="version" && e.parts[0]->type.kind==Type::fixed_array)return constant(Type::u64,{0,{}},e.token);
                if(operation=="check" && e.parts[0]->type.kind==Type::fixed_array)return owner;
                if(operation=="entry")return emit({ir::Op::dict_entry,owner,expression(*e.parts[1])},e.token);
                if(operation=="key" || operation=="value"){ir::Instruction field{ir::Op::extract,owner};field.immediate=operation=="value";return emit(field,e.token);}
                auto op=operation=="enter"?ir::Op::collection_enter:operation=="leave"?ir::Op::collection_leave:operation=="append"?ir::Op::collection_append:operation=="finish"?ir::Op::collection_finish:operation=="version"?(e.parts[0]->type.kind==Type::dictionary?ir::Op::dict_version:ir::Op::list_version):(e.parts[0]->type.kind==Type::dictionary?ir::Op::dict_check:ir::Op::list_check);
                ir::Instruction i{op,owner};i.type=e.type;if(operation=="check")i.b=expression(*e.parts[1]);return emit(i,e.token);
            }
            if(e.target.starts_with("$layout.")){ir::Instruction layout{ir::Op::format_layout,expression(*e.parts[0])};layout.type=e.type;layout.immediate=e.target=="$layout.alignment";return emit(layout,e.token);}
            if(e.target=="$interpolation")return frontend_case([&]() -> unsigned {
                std::vector<unsigned> slots;for(const auto& part:e.parts)slots.push_back(snapshot(expression(*part),Type::string,part->token));
                ir::Instruction join{ir::Op::string_join};join.type=Type::string;for(auto slot:slots)join.arguments.push_back(emit({ir::Op::load,slot},e.token));return emit(join,e.token);
            });
            if(e.target=="$hole")return frontend_case([&]() -> unsigned {
                auto value=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.parts[0]->token);
                auto spec=expression(*e.parts[1]);ir::Instruction format{ir::Op::scalar_format,emit({ir::Op::load,value},e.token),spec};format.type=e.type_arguments.at(0);
                if(!e.display_adapter.empty()){format.op=ir::Op::call;format.callee=e.display_adapter;format.arguments={format.a,format.b};format.a=format.b=0;}
                ir::Instruction text{ir::Op::format_unwrap,emit(format,e.parts[0]->token)};text.type=Type::string;return emit(text,e.parts[0]->token);
            });
            if(e.target.starts_with("$display."))return frontend_case([&]() -> unsigned {
                std::vector<unsigned> slots(e.parts.size());
                for(unsigned n=0;n<e.parts.size();++n)slots[e.argument_indexes.at(n)]=snapshot(expression(*e.parts[n]),expression_type(*e.parts[n]),e.parts[n]->token);
                auto value=emit({ir::Op::load,slots.at(0)},e.token);
                ir::Instruction i{e.target=="$display.string"?ir::Op::scalar_display:ir::Op::scalar_format,value};i.type=e.type;
                if(e.target=="$display.format")i.b=emit({ir::Op::load,slots.at(1)},e.token);
                return emit(i,e.token);
            });
            if(e.target.starts_with("$safe."))return safe_collection(e);
            // Keep ordinary interpolation followed by encoding. The experimental
            // rooted segment operation has no generally profitable source class;
            // automatic fusion must not guess from logger identity or byte length.
            if(e.target=="$text.to_utf8" || e.target=="$text.from_utf8"){ir::Instruction i{e.target=="$text.to_utf8"?ir::Op::string_encode:ir::Op::string_decode,expression(*e.parts[0])};i.type=e.type;return emit(i,e.token);}
            if(e.target=="$event.emit")return event_emit(e);
            if(e.target.starts_with("$dict.")) return frontend_case([&]() -> unsigned {
                auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);auto key=e.parts.size()>1?expression(*e.parts[1]):0;
                return emit({e.token.text=="clear"?ir::Op::dict_clear:e.token.text=="contains"?ir::Op::dict_contains:ir::Op::dict_remove,emit({ir::Op::load,owner},e.token),key},e.token);
            });
            if(e.target.starts_with("$array.")) return frontend_case([&]() -> unsigned {
                auto owner=snapshot(expression(*e.parts[0]),e.parts[0]->type,e.token);
                if(e.token.text=="clear")return emit({ir::Op::list_clear,emit({ir::Op::load,owner},e.token)},e.token);
                auto argument=expression(*e.parts[1]);
                if(e.token.text=="insert") {
                    ir::Instruction boundary{ir::Op::list_index,emit({ir::Op::load,owner},e.token),argument};boundary.immediate=1;
                    auto index=snapshot(emit(boundary,e.token),Type::u64,e.token);
                    auto version=snapshot(emit({ir::Op::list_version,emit({ir::Op::load,owner},e.token)},e.token),Type::u64,e.token);
                    auto value=snapshot(expression(*e.parts[2]),e.parts[2]->type,e.token);
                    ir::Instruction insert{ir::Op::list_insert,emit({ir::Op::load,owner},e.token),emit({ir::Op::load,index},e.token)};insert.arguments={emit({ir::Op::load,value},e.token),emit({ir::Op::load,version},e.token)};return emit(insert,e.token);
                }
                return emit({e.token.text=="push" ? ir::Op::list_push : ir::Op::list_remove,emit({ir::Op::load,owner},e.token),argument},e.token);
            });
            return {};
            });
            if(builtin)return *builtin;
            std::optional<ir::Place> receiver;
            std::optional<Selection> selected_receiver;
            std::size_t first = 0;
            if (e.kind == K::method_call) {
                first = 1;
                if(e.interface_call) receiver=frontend_case([&]() -> ir::Place {
                    ir::Instruction prepare{ir::Op::interface_prepare,expression(*e.parts[0])};prepare.callee=e.target;
                    auto prepared=expression_type(*e.parts[0]);prepared.kind=Type::prepared_receiver;prepared.nullable=false;prepared.preparation=e.target;
                    prepare.type=prepared;auto slot=snapshot(emit(prepare,e.token),prepared,e.token);return ir::Place{slot,{}};
                }); else
                if (e.parts[0]->type.kind==Type::managed_ref) {
                    auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}};
                } else if(e.selected_receiver) {
                    selected_receiver=select_expression(*e.parts[0]);
                    auto slot=snapshot(selected_receiver->read(),expression_type(*e.parts[0]),e.token);receiver=ir::Place{slot,{}};
                } else if (e.receiver_addressable) receiver=place(*e.parts[0]);
                else { auto slot=snapshot(expression(*e.parts[0]),expression_type(*e.parts[0]),e.token); receiver=ir::Place{slot,{}}; }

            }
            // Materialize each argument in a slot: later short-circuit arguments
            // may create new blocks, while IR instruction values stay block-local.
            std::vector<unsigned> slots;std::optional<unsigned> native_callback;
            for (std::size_t n = first; n < e.parts.size(); ++n) frontend_case([&] {
                const auto& arg = e.parts[n];
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(expression_type(*arg));
                auto value = expression(*arg);if((!e.native_symbol.empty() && native_binding(e.native_symbol).name=="register") && (e.argument_indexes.empty()?n-first:e.argument_indexes[n-first])==1)native_callback=value;
                if (e.c_callback_argument == (e.argument_indexes.empty() ? n-first : e.argument_indexes[n-first]))
                    native_callback = value;
                if (expression_type(*arg).construction) {
                    output.construction_origins[slot]=view_origin(value);
                }
                emit({ir::Op::store, slot, value}, arg->token); slots.push_back(slot);
            });
            return frontend_case([&]() -> unsigned {
            if(selected_receiver && !e.parts[0]->property)emit({ir::Op::store,receiver->slot,selected_receiver->read()},e.token);
            ir::Instruction i{e.asynchronous_call || !parallel_target.empty() ? ir::Op::async_call : e.interface_call ? ir::Op::interface_call : !e.virtual_slot.empty() && !e.super_call ? ir::Op::virtual_call : ir::Op::call}; i.callee = parallel_target.empty()?e.target+(e.super_call && e.token.text=="deinit"?"::$body$"+Type::key(e.parts[0]->type):""):std::string(parallel_target); i.receiver = receiver;
            if(e.op=="$operator")i.operator_glyph=e.token.text=="!="?"==":e.token.text;
            i.os_location_adapter = e.op == "$os.location";
            if (context) if (auto found=context->calls.find(&e);found!=context->calls.end())
                i.callee=ir::construction_name(e.target,{found->second.begin(),found->second.end()});
            i.arguments.resize(slots.size());
            for(unsigned n=0;n<slots.size();++n) i.arguments[e.argument_indexes.empty()?n:e.argument_indexes[n]]=emit({ir::Op::load,slots[n]},e.token);
            if(!e.native_symbol.empty()){i.op=ir::Op::native_call;i.callee=e.native_symbol;i.type=e.type;if(native_callback)i.arguments[1]=*native_callback;}
            if (e.c_callback_argument) i.arguments[*e.c_callback_argument] = *native_callback;
            auto result=emit(i,e.token);
            if(selected_receiver) {
                // Forward declarations need not have been lowered yet. The
                // checked call already records whether the receiver can mutate.
                if(e.receiver_writes)selected_receiver->write(emit({ir::Op::load,receiver->slot},e.token));
            }
            return result;
            });
        });
        case K::conversion: return frontend_case([&]() -> unsigned {
            auto value = expression(*e.parts[0]);
            if(e.op=="$nullable_try") {
                auto from=e.parts[0]->type;auto saved=snapshot(value,from,e.token);
                auto present=ir::nullable_value(from)?emit({ir::Op::equal,extract(value,0,e.token),constant(Type::i32,{1,{}},e.token)},e.token):emit({ir::Op::not_equal,value,constant(from,{},e.token)},e.token);
                auto result=static_cast<unsigned>(output.locals.size());output.locals.push_back(e.type);
                auto yes=block(),no=block(),done=block();terminate({ir::Terminator::Kind::branch,present,yes,no});
                current=no;ir::Instruction absent{ir::Op::sum};absent.type=e.type;emit({ir::Op::store,result,emit(absent,e.token)},e.token);jump(done);
                current=yes;ir::Instruction get{ir::nullable_value(from)?ir::Op::sum_get:ir::Op::convert,emit({ir::Op::load,saved},e.token)};get.immediate=ir::nullable_value(from)?1:0;get.type=ir::nullable_payload(from);
                ir::Instruction some{ir::Op::sum};some.type=e.type;some.immediate=1;some.arguments={emit(get,e.token)};emit({ir::Op::store,result,emit(some,e.token)},e.token);jump(done);
                current=done;return emit({ir::Op::load,result},e.token);
            }
            if(e.op=="$nullable_lift"){ir::Instruction packed{ir::Op::sum};packed.type=e.type;packed.immediate=1;packed.arguments={value};return emit(packed,e.token);}
            if(e.op=="$nullable_unwrap") {
                auto saved=snapshot(value,e.parts[0]->type,e.token);auto yes=block(),no=block();
                auto tag=extract(emit({ir::Op::load,saved},e.token),0,e.token);
                terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,tag,constant(Type::i32,{1,{}},e.token)},e.token),yes,no});
                current=no;ir::Instruction fail{ir::Op::panic};fail.callee="Null value in checked conversion";emit(fail,e.token);terminate({ir::Terminator::Kind::unreachable});
                current=yes;ir::Instruction get{ir::Op::sum_get,emit({ir::Op::load,saved},e.token)};get.immediate=1;return emit(get,e.token);
            }
            if(e.token.text=="as?") {ir::Instruction i{ir::Op::try_convert,value};i.type=expression_type(e);return emit(i,e.token);}
            if(e.op=="identity" && expression_type(e)==expression_type(*e.parts[0])) return value; ir::Instruction i{e.op=="interface" ? ir::Op::interface_pack : e.op=="upcast" ? (expression_type(e).construction ? ir::Op::stage_prefix : ir::Op::upcast) : ir::Op::convert,value}; i.type = expression_type(e);i.callee=e.target; return emit(i,e.token);
        });
        case K::unary: return frontend_case([&]() -> unsigned {
            auto value = expression(*e.parts[0]);
            if (e.op == "not") return emit({ir::Op::logical_not, value}, e.token);
            if (e.op == "~") return emit({ir::Op::bit_not,value},e.token);
            if (e.op == "+") return value;
            return emit({ir::Op::negate,value},e.token);
        });
        case K::binary: return frontend_case([&]() -> unsigned {
            auto left = expression(*e.parts[0]);
            auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(e.parts[0]->type);
            emit({ir::Op::store, slot, left}, e.token);
            if (e.op == "and" || e.op == "or") {
                auto rhs = block(), merge = block();
                terminate({ir::Terminator::Kind::branch, left, e.op == "and" ? rhs : merge, e.op == "and" ? merge : rhs});
                current = rhs; auto right = expression(*e.parts[1]); emit({ir::Op::store, slot, right}, e.token); jump(merge);
                current = merge; return emit({ir::Op::load, slot}, e.token);
            }
            auto right = expression(*e.parts[1]); left = emit({ir::Op::load, slot}, e.token);
            if(e.op.starts_with("$nullable_null_")) {
                auto equal=emit({ir::Op::equal,extract(left,0,e.token),extract(right,0,e.token)},e.token);
                return e.op=="$nullable_null_unequal"?emit({ir::Op::logical_not,equal},e.token):equal;
            }
            if(e.parts[0]->type==Type::string) {
                auto equal=emit({e.op=="+" ? ir::Op::string_concat : ir::Op::string_equal,left,right},e.token);
                return e.op=="!=" ? emit({ir::Op::logical_not,equal},e.token) : equal;
            }
            return emit({operation(e.op), left, right}, e.token);
        });
        }
        throw std::logic_error("unknown typed expression");
    }
    void initialize_base(const Expr& e) {
        std::vector<unsigned> arguments;
        for (const auto& part:e.parts) arguments.push_back(snapshot(expression(*part),expression_type(*part),part->token));
        ir::Instruction call{ir::Op::base_init}; call.type=e.type; call.callee=e.initializer;
        call.receiver=ir::Place{0,{}};
        if (!e.initializer.empty()) {
            call.arguments.resize(arguments.size());
            for (unsigned n=0;n<arguments.size();++n) call.arguments[e.field_indexes[n]]=emit({ir::Op::load,arguments[n]},e.token);
        } else {
            std::vector<unsigned> fields;
            for (unsigned field=0;field<e.type.definition->fields.size();++field) {
                std::optional<unsigned> selected;
                for(unsigned n=0;n<arguments.size();++n) if(e.field_indexes[n]==field) selected=emit({ir::Op::load,arguments[n]},e.token);
                for(const auto& child:e.omitted) if(child->slot==field) selected=expression(*child);
                Type t=e.type.definition->fields[field].type;
                fields.push_back(snapshot(selected ? *selected : constant(t,e.aggregate_value.fields.at(field),e.token),t,e.token));
            }
            for(auto field:fields) call.arguments.push_back(emit({ir::Op::load,field},e.token));
        }
        auto invoked=emit(call,e.token);
        if(e.constructed_type!=Type::none) {
            auto outcome=capture(invoked,e.initializer_result,e.token);auto yes=block(),no=block();
            ir::Instruction check{ir::Op::stage_check,invoked};check.receiver=ir::Place{0,{}};
            terminate({ir::Terminator::Kind::branch,emit(check,e.token),yes,no});current=no;
            ir::Instruction payload{ir::Op::sum_get,emit({ir::Op::load,outcome},e.token)};payload.immediate=2;
            auto error=emit(payload,e.token);ir::Instruction failed{ir::Op::sum};failed.type=output.result;failed.immediate=1;failed.arguments={error};
            return_through_cleanup(emit(failed,e.token),e.token,true);current=yes;
        }
    }
    unsigned event_emit(const Expr& expression_node){
        const auto& token=expression_node.token;auto array=expression_node.parts[0]->type;auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};auto owner=snapshot(expression(*expression_node.parts[0]),array,token);std::vector<unsigned> arguments;for(unsigned n=1;n<expression_node.parts.size();++n)arguments.push_back(snapshot(expression(*expression_node.parts[n]),expression_node.parts[n]->type,token));
        auto count=snapshot(emit({ir::Op::array_count,load(owner)},token),Type::i64,token);ir::Instruction slice{ir::Op::array_slice,load(owner),constant(Type::i64,{0,{}},token)};slice.type=array;slice.immediate=1;slice.arguments={load(count)};auto subscribers=capture(emit(slice,token),array,token);auto cursor=snapshot(constant(Type::i64,{0,{}},token),Type::i64,token);auto test=block(),body=block(),done=block();jump(test);current=test;terminate({ir::Terminator::Kind::branch,emit({ir::Op::less,load(cursor),load(count)},token),body,done});current=body;
        ir::Instruction call{ir::Op::function_call,emit({ir::Op::array_get,load(subscribers),load(cursor)},token)};for(auto argument:arguments)call.arguments.push_back(load(argument));emit(call,token);emit({ir::Op::store,cursor,emit({ir::Op::add,load(cursor),constant(Type::i64,{1,{}},token)},token)},token);jump(test);current=done;return 0;
    }
    Type assignment_type(const Statement& s) {
        if(s.selection)return expression_type(*s.selection);
        auto type=output.locals.at(s.slot);for(auto field:s.field_indexes)type=ir::field_type(type,field);
        return s.index?(type.kind==Type::dictionary?ir::dictionary_value(type):ir::array_element(type)):type;
    }
    unsigned compound_value(const Statement& s,unsigned old_slot,unsigned right) {
        auto rhs=snapshot(right,expression_type(*s.expression),s.expression->token);
        auto left=emit({ir::Op::load,old_slot},s.compound_token);right=emit({ir::Op::load,rhs},s.compound_token);
        if(s.compound_target.empty())return emit({s.assigned_type==Type::string?ir::Op::string_concat:operation(s.op.substr(0,s.op.size()-1)),left,right},s.token);
        ir::Instruction call{ir::Op::call};call.callee=s.compound_target;call.operator_glyph=s.op.substr(0,1);call.arguments={left,right};auto value=emit(call,s.compound_token);
        if(s.compound_conversion){emit({ir::Op::store,s.compound_slot,value},s.compound_token);return expression(*s.compound_conversion);}return value;
    }
    void event_subscription(const Statement& statement){
        const auto& token=statement.token;auto load=[&](unsigned slot){return emit({ir::Op::load,slot},token);};auto array=statement.assigned_type;auto selected=statement.selection?select_expression(*statement.selection):Selection{array,[&]{return read_place(ir::Place{statement.slot,statement.field_indexes},token);},{}};auto owner=snapshot(selected.read(),array,token);auto callback=snapshot(expression(*statement.expression),statement.expression->type,token);
        if(statement.op=="+="){emit({ir::Op::list_push,load(owner),load(callback)},token);return;}
        auto cursor=snapshot(emit({ir::Op::array_count,load(owner)},token),Type::i64,token);auto test=block(),body=block(),remove=block(),done=block();jump(test);current=test;terminate({ir::Terminator::Kind::branch,emit({ir::Op::greater,load(cursor),constant(Type::i64,{0,{}},token)},token),body,done});current=body;emit({ir::Op::store,cursor,emit({ir::Op::subtract,load(cursor),constant(Type::i64,{1,{}},token)},token)},token);auto selected_callback=emit({ir::Op::array_get,load(owner),load(cursor)},token);terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,selected_callback,load(callback)},token),remove,test});current=remove;emit({ir::Op::list_remove,load(owner),load(cursor)},token);jump(done);current=done;
    }
    void sequence(const std::vector<Statement>& body) { for (const auto& s : body) if (current) statement(s); }
    void statement(const Statement& s) {
        // Match the semantic handler boundaries during recursive lowering.
        if(s.kind==Statement::Kind::loop){if(s.binder.text.empty())loop_statement(s);else iterable_statement(s);return;}
        if(s.kind==Statement::Kind::scoped){scoped_statement(s);return;}
        if(s.kind==Statement::Kind::conditional){conditional_statement(s);return;}
        ordinary_statement(s);
    }
    VOID_SEMANTIC_HANDLER void ordinary_statement(const Statement& s) {
        using K = Statement::Kind;
        switch (s.kind) {
        case K::match: {
            auto subject=capture(expression(*s.expression),s.expression->type,s.token);auto done=block();
            for(const auto& arm:s.body){auto yes=block(),no=block();lower_pattern(*arm.pattern,subject,yes,no);current=yes;sequence(arm.body);if(current)jump(done);current=no;}
            if(s.exhaustive)terminate({ir::Terminator::Kind::unreachable});else jump(done);
            if(s.flow&next_flow)current=done;break;
        }
        case K::loop: case K::scoped: case K::conditional: return;
        case K::deferred:register_cleanup(expression(*s.expression),true,s.token);break;
        case K::no_op: break;
        case K::variable: {
            ir::Instruction zero{ir::Op::constant}; zero.type = output.locals[s.slot];
            auto value = s.expression ? expression(*s.expression) : emit(zero, s.token);
            ir::Instruction store{ir::Op::store,s.slot,value};store.initialization=true;emit(store,s.token); break;
        }
        case K::destructure: {
            const auto value=expression(*s.expression);
            for(unsigned n=0;n<s.bindings.size();++n) {
                const auto& binding=s.bindings[n];
                ir::Instruction store{ir::Op::store,binding.slot,extract(value,n,binding.token)};
                store.initialization=true;
                emit(store,binding.token);
            }
            break;
        }
        case K::assignment: {
            if(s.event){event_subscription(s);break;}
            if(s.selection) {
                auto selected=select_expression(*s.selection);auto old=(selected.read_before_write || s.op!="=")?selected.read():0;std::optional<unsigned> old_slot;
                if(s.op!="=")old_slot=snapshot(old,assignment_type(s),s.token);
                auto value=expression(*s.expression);
                if(old_slot)value=compound_value(s,*old_slot,value);
                selected.write(value);break;
            }
            if(s.index) {
                auto selected=select_place({s.slot,{}},s.field_indexes,s.token);
                auto array_type=output.locals[selected.slot];for(auto field:selected.fields)array_type=ir::field_type(array_type,field);
                if(array_type.kind==Type::dictionary) {
                    auto owner=snapshot(read_place(selected,s.token),array_type,s.token);auto index_slot=snapshot(expression(*s.index),s.index->type,s.index->token);auto object=emit({ir::Op::load,owner},s.token);auto version=snapshot(emit({ir::Op::dict_version,object},s.token),Type::u64,s.token);std::optional<unsigned> old;
                    if(s.op!="=")old=snapshot(emit({ir::Op::dict_get,object,emit({ir::Op::load,index_slot},s.token)},s.token),assignment_type(s),s.token);
                    auto value=expression(*s.expression);if(old)value=compound_value(s,*old,value);
                    dictionary_set(emit({ir::Op::load,owner},s.token),emit({ir::Op::load,index_slot},s.token),value,emit({ir::Op::load,version},s.token),array_type,s.token);break;
                }
                std::optional<unsigned> captured_array;if(array_type.kind==Type::growable_array)captured_array=snapshot(read_place(selected,s.token),array_type,s.token);
                auto index=expression(*s.index);auto index_slot=snapshot(index,s.index->type,s.index->token);
                auto array=captured_array?emit({ir::Op::load,*captured_array},s.token):read_place(selected,s.token);
                if(array_type.kind==Type::growable_array) {
                    auto owner_slot=snapshot(array,array_type,s.token);
                    auto normalized=emit({ir::Op::list_index,array,index},s.index->token);
                    auto normalized_slot=snapshot(normalized,Type::u64,s.token);
                    std::optional<unsigned> old_slot;
                    if(s.op!="=")old_slot=snapshot(emit({ir::Op::array_get,array,normalized},s.index->token),assignment_type(s),s.token);
                    auto version_slot=snapshot(emit({ir::Op::list_version,array},s.token),Type::u64,s.token);
                    auto value=expression(*s.expression);
                    if(s.op!="=")value=compound_value(s,*old_slot,value);
                    ir::Instruction commit{ir::Op::list_commit,emit({ir::Op::load,owner_slot},s.token),emit({ir::Op::load,normalized_slot},s.token)};
                    commit.arguments={value,emit({ir::Op::load,version_slot},s.token)};emit(commit,s.index->token);break;
                }
                auto old=emit({ir::Op::array_get,array,index},s.index->token); // Bounds check before RHS, even for '='.
                std::optional<unsigned> old_slot;
                if(s.op!="=") old_slot=snapshot(old,assignment_type(s),s.token);
                auto value=expression(*s.expression);
                if(old_slot) value=compound_value(s,*old_slot,value);
                index=emit({ir::Op::load,index_slot},s.index->token);
                array=read_place(selected,s.token); // Reacquire current inline bytes after RHS effects.
                ir::Instruction update{ir::Op::array_set,array,index};update.arguments={value};
                auto updated=emit(update,s.index->token);write_place(selected,updated,s.token);break;
            }
            auto selected=select_place({s.slot,{}},s.field_indexes,s.token);
            // Load compound-assignment LHS before evaluating its RHS.
            std::optional<unsigned> old_slot;
            if (s.op != "=") {
                auto slot = static_cast<unsigned>(output.locals.size()); output.locals.push_back(assignment_type(s)); old_slot = slot;
                auto old = read_place(selected,s.token);
                emit({ir::Op::store, slot, old}, s.token);
            }
            auto value = expression(*s.expression);
            if (old_slot) {
                value = compound_value(s,*old_slot,value);
            }
            write_place(selected,value,s.token); break;
        }
        case K::expression: expression(*s.expression); break;
        case K::returned:
            return_through_cleanup(s.expression?std::optional<unsigned>{expression(*s.expression)}:std::nullopt,s.token,&s!=normal_return);
            break;
        case K::broken:for(auto n=with_scopes.size();n>loop_with_depth.back();--n)leave_with(with_scopes[n-1],s.token);jump(loops.back().second); break;
        case K::continued:for(auto n=with_scopes.size();n>loop_with_depth.back();--n)leave_with(with_scopes[n-1],s.token);jump(loops.back().first); break;
        }
    }
    VOID_SEMANTIC_HANDLER void scoped_statement(const Statement& s) {
        if(s.task_group){emit({ir::Op::store,s.slot,emit({ir::Op::async_group_enter},s.token)},s.token);with_scopes.push_back({s.slot,true});sequence(s.body);if(current)join_group(s.slot,false,s.token);with_scopes.pop_back();return;}
        if(auto selected=local_cleanup_targets.find(s.slot);selected!=local_cleanup_targets.end()) {
            auto active=static_cast<unsigned>(output.locals.size());output.locals.push_back(Type::boolean);
            local_cleanups.push_back({s.slot,active,selected->second});auto index=static_cast<unsigned>(local_cleanups.size()-1);
            emit({ir::Op::store,active,constant(Type::boolean,{0,{}},s.token)},s.token);
            emit({ir::Op::store,s.slot,expression(*s.expression)},s.token);
            emit({ir::Op::store,active,constant(Type::boolean,{1,{}},s.token)},s.token);
            with_scopes.push_back({index,false,{},true});sequence(s.body);
            if(current)leave_with(with_scopes.back(),s.token,false);with_scopes.pop_back();return;
        }
        std::optional<unsigned> group;
        auto home=cells.at(s.slot);auto cell=output.locals.at(home);auto field=ir::field_type(cell,0);ir::Instruction construct{ir::Op::gc_construct};construct.type=cell;construct.arguments={constant(field,{0,{}},s.token)};emit({ir::Op::store,home,emit(construct,s.token)},s.token);
        auto node=register_cleanup(expression(*s.selection),false,s.token);
        auto value=expression(*s.expression);emit({ir::Op::store,s.slot,value},s.token);
        ir::Instruction activate{ir::Op::gc_write,emit({ir::Op::load,node},s.token),constant(Type::boolean,{1,{}},s.token)};activate.immediate=2;emit(activate,s.token);
        // Acquisition can propagate Err before the resource scope exists.
        // Register and activate disposal first, then create the child group;
        // a group-allocation failure still reaches the acquired cleanup.
        if (output.asynchronous) {
            group = snapshot(emit({ir::Op::async_group_enter}, s.token), Type::u64, s.token);
        }
        with_scopes.push_back({node,false,group});sequence(s.body);if(current)leave_with(with_scopes.back(),s.token,false);with_scopes.pop_back();return;
    }

    VOID_SEMANTIC_HANDLER void conditional_statement(const Statement& s) {
        auto yes=block(),no=block(),merge=block();
        lower_condition(*s.expression,yes,no);
        current = yes; sequence(s.body); bool yes_next = current.has_value(); if (current) jump(merge);
        current = no; sequence(s.otherwise); bool no_next = current.has_value(); if (current) jump(merge);
        if (yes_next || no_next) current = merge;
        return;
    }

    VOID_SEMANTIC_HANDLER void iterable_statement(const Statement& s) {
        const auto item=output.locals[s.slot];
        auto scalar=[&](Type t,std::uint64_t bits){return constant(t,{bits,{}},s.token);};
        auto load=[&](unsigned slot){return emit({ir::Op::load,slot},s.token);};
        auto store=[&](unsigned slot,unsigned value){ir::Instruction store{ir::Op::store,slot,value};store.initialization=slot==s.slot || (!s.index_binder.text.empty() && slot==s.index_slot);emit(store,s.token);};
        unsigned cursor,terminal,direction;std::optional<unsigned> owner,version;std::optional<Selection> fixed_view;
        auto test=block(),body=block(),step=block(),done=block();
        if(s.range_end) {
            auto first=snapshot(expression(*s.expression),item,s.token);
            auto last=snapshot(expression(*s.range_end),item,s.token);
            direction=snapshot(emit({ir::Op::less_equal,load(first),load(last)},s.token),Type::boolean,s.token);
            cursor=snapshot(load(s.reverse?last:first),item,s.token);
            terminal=snapshot(load(s.reverse?first:last),item,s.token);
            if(s.reverse) {
                store(direction,emit({ir::Op::logical_not,load(direction)},s.token));
                if(s.exclusive) {
                    auto prepare=block();terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,load(first),load(last)},s.token),prepare,done});current=prepare;
                    auto up=block(),down=block(),ready=block();terminate({ir::Terminator::Kind::branch,load(direction),up,down});
                    current=up;store(cursor,emit({ir::Op::add,load(cursor),scalar(item,1)},s.token));jump(ready);
                    current=down;store(cursor,emit({ir::Op::subtract,load(cursor),scalar(item,1)},s.token));jump(ready);current=ready;
                }
            }
        } else {
            auto array=s.expression->type;
            // Root the selected handle without invoking fixed value-copy storage.
            owner=static_cast<unsigned>(output.locals.size());output.locals.push_back(array);
            if(array.kind==Type::fixed_array)fixed_view=select_expression(*s.expression);
            if(array.kind!=Type::fixed_array)store(*owner,expression(*s.expression));else {ir::Instruction capture{ir::Op::array_capture,fixed_view ? fixed_view->read() : expression(*s.expression)};capture.immediate=*owner;emit(capture,s.token);}
            auto count=snapshot(emit({array.kind==Type::dictionary?ir::Op::dict_count:ir::Op::array_count,load(*owner)},s.token),Type::i64,s.token);
            if(array.kind==Type::growable_array || array.kind==Type::dictionary)version=snapshot(emit({array.kind==Type::dictionary?ir::Op::dict_version:ir::Op::list_version,load(*owner)},s.token),Type::u64,s.token);
            cursor=snapshot(s.reverse ? emit({ir::Op::subtract,load(count),scalar(Type::i64,1)},s.token) : scalar(Type::i64,0),Type::i64,s.token);
            terminal=snapshot(s.reverse ? scalar(Type::i64,~std::uint64_t{0}) : load(count),Type::i64,s.token);
            direction=snapshot(scalar(Type::boolean,!s.reverse),Type::boolean,s.token);
        }
        jump(test);current=test;
        if(fixed_view){ir::Instruction refresh{ir::Op::array_capture,fixed_view->read()};refresh.immediate=*owner;emit(refresh,s.token);}
        if(version)emit({s.expression->type.kind==Type::dictionary?ir::Op::dict_check:ir::Op::list_check,load(*owner),load(*version)},s.token);
        if(!s.range_end || (s.exclusive && !s.reverse))terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,load(cursor),load(terminal)},s.token),body,done});else jump(body);
        current=body;
        if(!s.range_end && s.expression->type.kind==Type::dictionary){auto entry=emit({ir::Op::dict_entry,load(*owner),load(cursor)},s.token);ir::Instruction value{ir::Op::extract,entry};value.immediate=1;store(s.slot,emit(value,s.token));if(!s.index_binder.text.empty()){ir::Instruction key{ir::Op::extract,entry};key.immediate=0;store(s.index_slot,emit(key,s.token));}}
        else {store(s.slot,s.range_end ? load(cursor) : emit({ir::Op::array_get,load(*owner),load(cursor)},s.token));
        if(!s.index_binder.text.empty())store(s.index_slot,load(cursor));}
        loops.emplace_back(step,done);loop_with_depth.push_back(with_scopes.size());sequence(s.body);if(current)jump(step);loops.pop_back();loop_with_depth.pop_back();
        current=step;
        if(s.range_end && (!s.exclusive || s.reverse)) {
            auto advance=block();terminate({ir::Terminator::Kind::branch,emit({ir::Op::equal,load(cursor),load(terminal)},s.token),done,advance});current=advance;
        }
        auto up=block(),down=block();terminate({ir::Terminator::Kind::branch,load(direction),up,down});
        current=up;store(cursor,emit({ir::Op::add,load(cursor),scalar(s.range_end?item:Type(Type::i64),1)},s.token));jump(test);
        current=down;store(cursor,emit({ir::Op::subtract,load(cursor),scalar(s.range_end?item:Type(Type::i64),1)},s.token));jump(test);
        current=done;return;
    }

    VOID_SEMANTIC_HANDLER void loop_statement(const Statement& s) {
        auto test = block(), body = block(), done = block(); jump(test);
        current = test;
        if (always(s.expression.get())) jump(body);
        else lower_condition(*s.expression,body,done);
        loops.emplace_back(test, done);loop_with_depth.push_back(with_scopes.size());current = body; sequence(s.body); if (current) jump(test); loops.pop_back();loop_with_depth.pop_back();
        if (s.flow & next_flow) current = done;
        return;
    }

public:
    explicit Lowering(const SourceFile& s, ir::Module& m, StringPool& p) : source(s), module(m), strings(p) {}
    const std::set<std::string>& elided_handlers() const {return elided_cleanup_handlers;}
    ir::Function lower(const Function& f, const ConstructionContext* construction=nullptr) {
        ConstructionContext projected;
        if(construction) {
            projected.parameters=managed_construction_parameters(f,construction->parameters);
            for(auto [slot,origin]:construction->aliases)if(f.locals.at(slot).kind==Type::managed_ref)projected.aliases.emplace(slot,origin);
            for(const auto& [expression,mask]:construction->calls) {
                auto& checked=projected.calls[expression];const unsigned first=expression->kind==Expr::Kind::method_call?1u:0u;
                for(unsigned n=0;n<expression->parts.size();++n) {
                    const auto formal=n<first || expression->argument_indexes.empty()?n:first+expression->argument_indexes[n-first];
                    if(mask.contains(formal) && expression->parts[n]->type.kind==Type::managed_ref)checked.insert(formal);
                }
            }
        }
        context=construction?&projected:nullptr;normal_return=!f.body.empty() && f.body.back().kind==Statement::Kind::returned?&f.body.back():nullptr;
        output = {}; output.name = f.symbol; output.source_name = f.concrete_name.empty() ? f.token.text : f.concrete_name; output.display_name=f.display_name; output.source = source.location(f.token.offset);
        if(f.static_owner)output.source_name=f.symbol.substr(f.static_owner->type.definition->module_identity.size()+2);
        output.static_member=bool(f.static_owner) || (f.global_initializer && !f.lexical_owner.empty());
        output.asynchronous=f.asynchronous;output.task=f.task;output.lexical_owner=f.receiver?Type::key(f.receiver->type):f.lexical_owner;output.lexical_parent=f.lexical_parent;output.operator_owner=f.operator_owner;output.operator_glyph=f.operator_glyph;
        output.bound_receiver=f.bound_receiver;output.closure=f.anonymous;cells.clear();cleanup_head.reset();return_slot.reset();cleanup_jumps.clear();with_scopes.clear();loop_with_depth.clear();
        output.parameters.clear(); output.result = f.result; output.locals = f.locals; current = block();
        const bool rooted_cleanup=select_local_cleanups(f);
        if(f.cleanup_type!=Type::none){cleanup_head=output.locals.size();auto head=f.cleanup_type;head.nullable=true;output.locals.push_back(head);}
        if((cleanup_head || f.asynchronous) && f.result!=Type::none){return_slot=output.locals.size();output.locals.push_back(f.result);}
        for(const auto& [slot,type]:f.captured_locals){if(rooted_cleanup && local_cleanup_targets.contains(slot))continue;cells.emplace(slot,static_cast<unsigned>(output.locals.size()));output.locals.push_back(type);}
        for(const auto& [slot,field]:f.capture_inputs){
            auto environment=emit({ir::Op::argument,0},f.token);ir::Instruction read{ir::Op::gc_read,environment};read.immediate=field;
            emit({ir::Op::store,cells.at(slot),emit(read,f.token)},f.token);
        }
        if(f.receiver && cells.contains(f.receiver->slot)){
            auto value=emit({ir::Op::argument,0},f.receiver->token);ir::Instruction store{ir::Op::store,f.receiver->slot,value};store.initialization=true;emit(store,f.receiver->token);
        }
        output.virtual_slot=f.virtual_slot; output.overriding=f.overriding; output.visibility=f.visibility;
        if(f.receiver) output.method_name=f.concrete_method_name.empty()?f.token.text:f.concrete_method_name;
        output.accessor=f.accessor; output.receiver = f.receiver.has_value(); output.receiver_mutates = f.receiver_mutates; output.initializer = f.initializer;
        if (f.receiver) {
            output.source_name = f.receiver->type.definition->name + "::method::" + output.method_name;
            output.parameters.push_back(f.receiver->type);
        }
        for (std::size_t n = 0; n < f.parameters.size(); ++n) {
            const auto& p = f.parameters[n]; output.parameters.push_back(p.type);
            auto value = emit({ir::Op::argument, static_cast<unsigned>(n + (f.receiver ? 1 : 0))}, p.token); ir::Instruction store{ir::Op::store,p.slot,value};store.initialization=true;emit(store,p.token);
        }
        if (context) {
            output.construction_origins=context->aliases;
            for (const auto& [slot,origin]:context->aliases) output.locals.at(slot).construction=true;
            for (auto index:context->parameters) output.parameters.at(index).construction=true;
            if (!f.initializer) output.name=ir::construction_name(f.symbol,{context->parameters.begin(),context->parameters.end()});
        }
        if (f.base_prelude) initialize_base(*f.base_prelude);
        if (f.initializer && f.receiver->type.kind==Type::managed_ref) {
            auto base=f.receiver->type.definition->base.lock();
            for (unsigned field=base ? static_cast<unsigned>(base->fields.size()) : 0;field<f.receiver->type.definition->fields.size();++field) {
                Type t=f.receiver->type.definition->fields[field].type;
                std::optional<unsigned> selected;
                for(const auto& child:f.initialization_defaults) if(child->slot==field) selected=expression(*child);
                if (ir::requires_initialization(t) && !(checked_initializer_default(t) && selected)) continue;
                auto value=selected ? *selected : constant(t,f.initialization_value.fields.at(field),f.token);
                auto view=emit({ir::Op::load,0},f.token);
                ir::Instruction write{ir::Op::gc_write,view,value}; write.immediate=field; emit(write,f.token);
            }
        }
        sequence(f.body);
        if(current)return_through_cleanup({},f.token);
        if(cleanup_head || f.asynchronous){
            auto entry=block();output.cleanup_entry=entry;for(auto from:cleanup_jumps)output.blocks[from].end.yes=entry;current=entry;
            if(f.asynchronous)emit({ir::Op::async_drain},f.token);
            for(auto i=local_cleanups.rbegin();i!=local_cleanups.rend();++i)dispose_local(*i,f.token);
            if(cleanup_head && !rooted_cleanup){
            // A separate loop entry prevents executing the structured drain again.
            auto loop=block();jump(loop);current=loop;
            auto head=output.locals[*cleanup_head];auto node=head;node.nullable=false;
            auto body=block(),done=block();auto selected=emit({ir::Op::load,*cleanup_head},f.token);terminate({ir::Terminator::Kind::branch,emit({ir::Op::not_equal,selected,constant(head,{0,{}},f.token)},f.token),body,done});
            current=body;ir::Instruction unwrap{ir::Op::convert,emit({ir::Op::load,*cleanup_head},f.token)};unwrap.type=node;auto owner=emit(unwrap,f.token);auto handler=capture(extract_managed(owner,1,f.token),ir::field_type(node,1),f.token);auto previous=extract_managed(owner,0,f.token);emit({ir::Op::store,*cleanup_head,previous},f.token);auto active=extract_managed(owner,2,f.token);auto invoke=block();terminate({ir::Terminator::Kind::branch,active,invoke,loop});current=invoke;emit({ir::Op::cleanup_call,emit({ir::Op::load,handler},f.token)},f.token);jump(loop);
            current=done;}if(return_slot)terminate({ir::Terminator::Kind::return_value,emit({ir::Op::load,*return_slot},f.token)});else terminate({ir::Terminator::Kind::return_void});
        }
        return std::move(output);
    }
};

void generic_provenance(ir::Module& module,const std::map<std::string,std::unique_ptr<Unit>>& units) {
    for(const auto& [id,unit]:units) {
        for(const auto& generic:unit->generic_functions)if(!generic.generics.empty() && !generic.symbol.empty())
            module.generic_declarations.emplace(generic.symbol,ir::Module::GenericDeclaration{id,static_cast<unsigned>(generic.generics.size()),false});
        for(const auto& generic:unit->generic_structures)
            module.generic_declarations.emplace(generic.definition->identity,ir::Module::GenericDeclaration{id,static_cast<unsigned>(generic.generics.size()),true});
        for(const auto& function:unit->functions)if(!function.generic_arguments.empty()) {
            module.generic_instances.emplace(function.symbol,ir::Module::GenericInstance{function.generic_origin,function.generic_arguments});
            for(const auto& [mask,context]:function.construction_contexts) {
                const auto managed=managed_construction_parameters(function,mask);
                if(!managed.empty())module.generic_instances.emplace(ir::construction_name(function.symbol,{managed.begin(),managed.end()}),
                    ir::Module::GenericInstance{function.generic_origin,function.generic_arguments});
            }
        }
        for(const auto& structure:unit->structures)if(structure.generic_origin)
            module.generic_instances.emplace(structure.definition->identity,ir::Module::GenericInstance{structure.generic_origin->definition->identity,structure.type_arguments});
    }
}
}

void check_string_pool(const std::map<std::string, std::unique_ptr<Unit>>& units) {
    (void)string_pool(units);
}

ir::Module lower_graph(const std::map<std::string, std::unique_ptr<Unit>>& units, const std::string& entry,
                      std::shared_ptr<const project::LockedDependencies> dependencies) {
    ir::Module module;
    module.locked_dependencies = std::move(dependencies);
    for (const auto& [id, unit] : units) if (unit->c_primitives) module.c_primitives = unit->c_primitives;
    if (module.c_primitives) module.coordinated = true;
    auto strings=string_pool(units);
    std::map<std::string, std::set<std::string>> reach;
    for (const auto& [id, unit] : units) {
        std::vector<std::string> pending{id};
        while (!pending.empty()) { auto key = pending.back(); pending.pop_back(); if (!reach[id].insert(key).second) continue; for (const auto& import : units.at(key)->imports) if (!import.target.empty()) pending.push_back(import.target); }
    }
    std::set<std::string> emitted;
    while (emitted.size() < units.size()) {
        for (const auto& [id, unit] : units) if (!emitted.contains(id)) {
            bool ready = true;
            for (const auto& dependency : reach[id]) if (!emitted.contains(dependency) && !reach[dependency].contains(id)) ready = false;
            if (!ready) continue;
            for (const auto& [member, owner] : units) if (!emitted.contains(member) && reach[id].contains(member) && reach[member].contains(id)) {
                for (const auto& value : owner->globals) module.globals.push_back({value.identity, member, value.visibility, value.cell, value.initializer->symbol, value.syntax, owner->source.location(value.token.offset),value.static_owner==Type::none?std::string{}:Type::key(value.static_owner)});
                emitted.insert(member);
            }
            break;
        }
    }
    for(const auto& [id,unit]:units)for(auto type:unit->synthetic_types){module.nominal_types.push_back(type);if(type.kind==Type::managed_ref)module.coordinated=true;}
    for (const auto& [id,unit] : units) for (const auto& structure : unit->structures) {
        module.nominal_types.push_back(Type::nominal(structure.definition));
        if (structure.definition->managed || structure.definition->interface_type) module.coordinated=true;
    }
    std::set<std::string> results;
    for(const auto& [id,unit]:units)for(const auto& syntax:unit->results)if(syntax.resolved!=Type::none && results.insert(Type::key(syntax.resolved)).second)module.nominal_types.push_back(syntax.resolved);
    module.identity = entry; module.entry_symbol = entry + "::entry";module.root_identity=entry;
    for(const auto& [id,u]:units) {auto& imports=module.source_imports[id];for(const auto& i:u->imports) if(!i.target.empty()) imports.insert(i.target);}
    module.source_path = units.at(entry)->source.path();
    generic_provenance(module,units);
    std::set<std::string> elided_handlers;
    for (auto& [id, unit] : units) {
        Lowering lowering(unit->source,module,strings);
        for (const auto& f : unit->functions) {
            if(elided_handlers.contains(f.symbol))continue;
            if (f.c_primitive) {
                const auto& binding = module.c_primitives->bindings().at(*f.c_primitive);
                ir::Function wrapper;
                wrapper.name = f.symbol;
                wrapper.source_name = f.token.text;
                wrapper.module_identity = id;
                wrapper.source = unit->source.location(f.token.offset);
                wrapper.parameters.clear();
                for (const auto& parameter : f.parameters) wrapper.parameters.push_back(parameter.type);
                wrapper.result = f.result;
                ir::Block body;
                ir::Instruction call{ir::Op::c_primitive_call};
                call.callee = module.c_primitives->call_identity(binding);
                call.immediate = *f.c_primitive;
                call.type = wrapper.result;
                call.source = wrapper.source;
                for (unsigned argument = 0; argument < wrapper.parameters.size(); ++argument) {
                    body.instructions.push_back({ir::Op::argument, argument});
                    call.arguments.push_back(argument);
                }
                body.instructions.push_back(std::move(call));
                body.end = {wrapper.result == Type::none ? ir::Terminator::Kind::return_void :
                    ir::Terminator::Kind::return_value, static_cast<unsigned>(wrapper.parameters.size())};
                wrapper.blocks.push_back(std::move(body));
                module.functions.push_back(std::move(wrapper));
                continue;
            }
            if(!f.native_symbol.empty()){module.native_library=std::string(native_provider(f.native_symbol));module.coordinated=true;continue;}
            if(f.interface_requirement) {
                ir::Function requirement;requirement.name=f.symbol;requirement.parameters={f.receiver->type};
                for(const auto& p:f.parameters) requirement.parameters.push_back(p.type);
                requirement.result=f.result;requirement.receiver=true;requirement.method_name=f.token.text;requirement.visibility=f.visibility;requirement.module_identity=id;
                module.interface_requirements.push_back(std::move(requirement));continue;
            }
            if (!f.initializer || f.receiver->type.kind!=Type::managed_ref) {
                auto lowered = lowering.lower(f); lowered.module_identity = id;elided_handlers.insert(lowering.elided_handlers().begin(),lowering.elided_handlers().end());
                if (f.global_initializer) {
                    auto value = std::find_if(unit->globals.begin(), unit->globals.end(), [&](const auto& g) { return g.initializer == &f; });
                    lowered.result = value->cell;
                    for (auto& block : lowered.blocks) if (block.end.kind == ir::Terminator::Kind::return_value) {
                        ir::Instruction construct{ir::Op::gc_construct}; construct.type = value->cell; construct.arguments = {block.end.value}; construct.source = value->initializer->body.front().expression ? unit->source.location(value->token.offset) : lowered.source;
                        block.end.value = block.instructions.size(); block.instructions.push_back(std::move(construct));
                    }
                }
                if(f.receiver && f.token.text=="deinit"){
                    auto wrapper=lowered;wrapper.locals={lowered.parameters[0]};wrapper.blocks.clear();wrapper.cleanup_entry.reset();wrapper.closure=wrapper.bound_receiver=false;
                    const auto body_suffix="::$body$"+Type::key(lowered.parameters[0]);lowered.name+=body_suffix;lowered.display_name=lowered.source_name;lowered.source_name+=body_suffix;lowered.method_name+=body_suffix;lowered.visibility="private";lowered.virtual_slot.clear();lowered.overriding=false;
                    ir::Instruction load{ir::Op::load,0};load.source=wrapper.source;ir::Instruction release{ir::Op::gc_release,0};release.source=wrapper.source;ir::Instruction call{ir::Op::call};call.callee=lowered.name;call.receiver=ir::Place{0,{}};call.source=wrapper.source;
                    wrapper.blocks={{{load,release},{ir::Terminator::Kind::branch,1,1,2}},{{call},{ir::Terminator::Kind::return_void}},{{},{ir::Terminator::Kind::return_void}}};module.functions.push_back(std::move(wrapper));
                }
                module.functions.push_back(std::move(lowered));
            }
            std::set<std::set<unsigned>> emitted_contexts;
            for (const auto& [mask,context]:f.construction_contexts) {
                const auto managed=managed_construction_parameters(f,mask);
                if(managed.empty() || !emitted_contexts.insert(managed).second)continue;
                auto lowered=lowering.lower(f,&context); lowered.module_identity=id;elided_handlers.insert(lowering.elided_handlers().begin(),lowering.elided_handlers().end());
                module.functions.push_back(std::move(lowered));
            }
        }
    }
    const auto& selected=*units.at(entry);
    const auto* application=application_entry(selected);
    if(application || (!selected.globals.empty() && !selected.declarations.contains("entry") && !selected.declarations.contains("main"))) {
        module.application=true;module.entry_symbol=entry+"::$app_adapter";
        ir::Function adapter;adapter.name=module.entry_symbol;adapter.source_name="$app_adapter";adapter.module_identity=entry;adapter.source=selected.source.location(application ? application->token.offset : 0);
        adapter.parameters={Type::i32};adapter.result=Type::i32;adapter.locals={Type::i32};
        ir::Instruction call{ir::Op::call};call.callee=application ? application->symbol : "";call.type=application ? application->result : Type(Type::none);
        ir::Block body; if (application) body.instructions.push_back(call);
        if(!application || application->result==Type::none){body.instructions.push_back({ir::Op::constant,0});body.end={ir::Terminator::Kind::return_value,application ? 1u : 0u};}
        else body.end={ir::Terminator::Kind::return_value,0};
        adapter.blocks.push_back(std::move(body));module.functions.push_back(std::move(adapter));
    }
    module.interface_bindings=units.at(entry)->member_catalog->interface_bindings;
    module.strings=std::move(strings.values);
    if(!module.strings.empty()) module.coordinated=true;
    std::function<void(Type)> trace_string=[&](Type t) {
        if(ir::collection_type(t) || ir::reference_paths(t,true)!=ir::reference_paths(t)) module.coordinated=true;
        if(t.kind==Type::aggregate) for(const auto& field:t.definition->fields) trace_string(field.type);
    };
    for(const auto& f:module.functions) { trace_string(f.result); for(auto t:f.parameters) trace_string(t); for(auto t:f.locals) trace_string(t); }
    for(auto t:module.nominal_types) if(t.definition) trace_string(Type(t.definition));
    numeric::append_prelude(module);
    // A conventional application can import a standard module that also offers
    // async APIs. Only its entry closure is executable, and IR validation still
    // rejects async or worker code reachable from that closure.
    if (module.c_primitives && module.c_primitives->dynamic() && module.c_primitives->dynamic()->conventional())
        ir::retain_entry(module);
    ir::validate(module); return module;
}
}
