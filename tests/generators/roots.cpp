#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
void reject(const vd::ir::Module& module) {
    try { vd::ir::validate(module); }
    catch(const std::invalid_argument&) { return; }
    throw std::runtime_error("IR validator admitted an invalid generator lifecycle");
}
}

int main(int argc,char** argv) {
    try {
        require(argc==3,"expected synchronous and asynchronous fixture paths");
        auto compiled=vd::compile_project(argv[1]);
        auto invalid=compiled.module;
        bool changed=false;
        for(auto& f:invalid.functions)for(auto& block:f.blocks)
            if(block.end.kind==vd::ir::Terminator::Kind::yield) {
                block.end.yes=0;
                changed=true;
            }
        require(changed,"missing generator suspension");
        reject(invalid);

        invalid=vd::compile_project(argv[2]).module;
        changed=false;
        for(auto& f:invalid.functions)for(auto& block:f.blocks)for(auto& operation:block.instructions)
            if(operation.op==vd::ir::Op::async_generator_bind) {
                operation={vd::ir::Op::gc_poll};
                changed=true;
            }
        require(changed,"missing async generator bind");
        reject(invalid);

        for(bool optimized:{false,true}) {
            vd::Jit jit;
            auto runtime=std::make_shared<vd::Runtime>(1);
            auto code=jit.compile(compiled.module,compiled.module.entry_symbol,optimized,{},runtime);
            require(code->invoke(0)==1,"explicit closure lost a root or retained a dead local");
            require(code->invoke(1)==43,"exhaustion lost a root or retained the frame");
            vd::Runtime::Attachment attached(*runtime);
            vd::Runtime::Managed managed(*runtime);
            require(runtime->collect()==vd::Runtime::Collection::completed,"collection after generator return failed");
            require(!runtime->snapshot().objects && !runtime->memory_snapshot().handles,
                    "generator roots or handles survived invocation");
        }
    } catch(const std::exception& failure) {
        std::cerr<<failure.what()<<'\n';
        return 1;
    }
}
