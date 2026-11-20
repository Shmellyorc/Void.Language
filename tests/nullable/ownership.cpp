#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/runtime.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
const char* source=R"(
value struct tree:
    children: [tree?]
value struct payload:
    owner: node
    label: string?
struct node:
    name: string?
    entries: [string]?
    held: payload?
    next: node?
value struct box<t>:
    value: t?
func entry(rounds: i32) -> i32:
    var tree_root = tree()
    tree_root.children.push(tree_root)
    var root = node(name = "root", entries = ["one", "two"])
    root.held = payload(owner = root, label = "retained")
    var values: [payload?] = [null, root.held]
    var dictionary: {string: [payload?]?} = {"held": values, "null": null}
    var fixed: [payload?, 2] = [null, root.held]
    var wrapped = box<[payload?]>(value = values)
    var current = root
    for i in 0..rounds:
        var fresh = node(name = "temporary", entries = ["temporary"])
        fresh.held = payload(owner = current, label = "live")
        fresh.next = current
        current = fresh
    if (root.name as string) != "root" or (root.entries as [string])[1] != "two":
        return 1
    if values[1] is Ok(var value):
        if value.owner != root or value.label as string != "retained":
            return 2
    else:
        return 3
    if ((dictionary["held"] as [payload?])[1] as payload).owner != root:
        return 4
    if (fixed[1] as payload).label as string != "retained":
        return 5
    if ((wrapped.value as [payload?])[1] as payload).owner != root or dictionary["null"] != null:
        return 6
    if (tree_root.children[0] as tree).children.count != 1:
        return 8
    root.held = null
    root.entries = null
    current = root
    if (values[1] as payload).owner.name as string != "root":
        return 7
    return 0
)";
}
int main() {
    try {
        auto analysis=vd::analyze_source({"nullable-ownership.void",source});
        if(!analysis.succeeded())throw vd::CompilationError(std::move(analysis.diagnostics));
        auto module=analysis.program->lower();
        vd::ir::validate(module);
        bool exercised=false;
        for(auto type:module.nominal_types)if(vd::ir::nullable_value(type)) {
            auto descriptor=std::const_pointer_cast<vd::ir::ValueType>(type.definition);
            descriptor->nullable_value=false;
            bool rejected=false;try{vd::ir::validate(module);}catch(const std::invalid_argument&){rejected=true;}
            descriptor->nullable_value=true;require(rejected,"forged nullable descriptor accepted");
            exercised=true;break;
        }
        require(exercised,"nullable types missing from nominal catalog");
        for(bool optimized:{false,true}) {
            auto runtime=std::make_shared<vd::Runtime>(1);
            vd::Jit jit;auto code=jit.compile(module,module.entry_symbol,optimized,{},runtime);
            for(int iteration=0;iteration<3;++iteration) {
                require(code->invoke(200)==0,"nullable graph lost payload, alias or value snapshot");
                vd::Runtime::Attachment attachment(*runtime);vd::Runtime::Managed managed(*runtime);
                require(runtime->collect()==vd::Runtime::Collection::completed && runtime->snapshot().objects==0,"nullable roots leaked after return");
            }
            require(runtime->snapshot().completed_epoch>200,"nullable stress did not exercise collection");
        }
        std::cout<<"Nullable storage: descriptor rejection, precise GC, cycles, strings, list/dictionary payloads, fixed copies; O0/O2 passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
