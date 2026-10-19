#include "void/ir.hpp"
#include "void/backend.hpp"
#include "void/runtime.hpp"
#include <functional>
#include <stdexcept>
void async_ir_tests(){
    using namespace vd::ir;
    auto task_definition=std::make_shared<ValueType>();task_definition->name=task_definition->identity="$task:0::";task_definition->managed=true;task_definition->visibility="private";task_definition->task_result=FieldType(Type::i32);task_definition->fields={{"$status",Type::i32,task_definition->identity},{"$result",Type::i32,task_definition->identity}};Type task(task_definition,Type::managed_ref);
    Function leaf;leaf.name="leaf";leaf.asynchronous=true;leaf.task=task;leaf.blocks={{{{Op::argument}},{Terminator::Kind::return_value,0}}};
    Function entry=leaf;entry.name="entry";Instruction call{Op::async_call};call.callee="leaf";call.arguments={0};entry.blocks={{{{Op::argument},call,{Op::await,1}},{Terminator::Kind::return_value,2}}};
    Module valid;valid.identity="/async-ir.void";valid.coordinated=true;valid.nominal_types={task};valid.functions={leaf,entry};validate(valid);
    {auto executable=valid;executable.async_root=true;for(bool optimize:{false,true}){vd::Jit jit;auto code=jit.compile(executable,"entry",optimize,{},std::make_shared<vd::Runtime>());if(code->invoke(7)!=7)throw std::runtime_error("independent async IR execution failed");}}
    auto rejects=[&](const std::function<void(Module&)>& mutate,const char* expected){auto invalid=valid;mutate(invalid);try{validate(invalid);}catch(const std::invalid_argument& failure){if(std::string(failure.what()).find(expected)!=std::string::npos)return;throw;}throw std::runtime_error("malformed async IR accepted");};
    rejects([](auto& m){m.coordinated=false;},"coordinated runtime");
    rejects([](auto& m){m.nominal_types.clear();},"async task absent");
    rejects([](auto& m){m.functions[0].task=Type::none;},"async declaration");
    rejects([](auto& m){m.functions[1].asynchronous=false;m.functions[1].task=Type::none;},"illegal await");
    rejects([](auto& m){m.functions[1].blocks[0].instructions[1].op=Op::call;},"effect mismatch");
    rejects([](auto& m){m.functions[1].blocks[0].instructions[2].a=0;},"illegal await");
    rejects([](auto& m){m.functions[1].cleanup_entry=0;},"deferred cleanup");
    rejects([](auto& m){m.functions[1].blocks[0].end={Terminator::Kind::jump,0,99};},"branch destination");
    rejects([&](auto& m){auto& f=m.functions[1];f.locals={task};f.blocks[0].instructions.push_back({Op::load,0});},"uninitialized required local");
    rejects([](auto& m){m.functions[1].blocks[0].instructions.push_back({Op::gc_read,1});},"task payload");
    rejects([&](auto& m){Instruction create{Op::gc_construct};create.type=task;create.arguments={0,0};m.functions[1].blocks[0].instructions.push_back(create);},"task construction");
    rejects([](auto& m){
        auto definition=std::make_shared<ValueType>();definition->name=definition->identity="scratch";definition->managed=true;definition->fields={{"number",Type::i32,"scratch"}};
        Type borrowed(definition,Type::managed_ref);borrowed.construction=true;
        auto& f=m.functions[1];f.locals={borrowed};Instruction stage{Op::stage_begin};stage.type=borrowed;
        f.blocks[0].instructions.push_back(stage);f.blocks[0].instructions.push_back({Op::await,1});f.blocks[0].instructions.push_back({Op::load,0});
    },"survives suspension");
    rejects([](auto& m){
        auto definition=std::make_shared<ValueType>();definition->name=definition->identity="scratch";definition->managed=true;definition->fields={{"number",Type::i32,"scratch"}};
        Type borrowed(definition,Type::managed_ref);borrowed.construction=true;
        auto& f=m.functions[1];f.locals={borrowed};Instruction stage{Op::stage_begin};stage.type=borrowed;
        f.blocks[0].instructions.push_back(stage);f.blocks[0].instructions.push_back({Op::await,1});f.blocks[0].instructions.push_back({Op::gc_read,3});
    },"transient instruction value");
    rejects([&](auto& m){auto broken=std::make_shared<ValueType>(*task_definition);broken->self.reset();broken->arena.reset();broken->fields.pop_back();m.functions[0].task=Type(broken,Type::managed_ref);},"task handle shape");
}
