#include "void/compiler_observation.hpp"
#include "void/path.hpp"
#include "native_cache.hpp"
#include "target.hpp"
#include <llvm/ExecutionEngine/Orc/ObjectTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include "void/backend.hpp"
#include "void/llvm_storage_audit.hpp"
#ifdef VOID_LLVM_RESEARCH
#include "void/private_fixed_call_research.hpp"
#endif
#include "void/frontend.hpp"
#include "void/tooling.hpp"
#include "void/numeric.hpp"
#include "void/math_ops.hpp"
#include "void/temporal_ops.hpp"
#include "void/console_ops.hpp"
#include "void/filesystem_ops.hpp"
#include "void/os.hpp"
#include "void/host_ops.hpp"
#include "void/filesystem_abi.hpp"
#include "void/url_ops.hpp"
#include "void/http_ops.hpp"
#include "void/url_abi.hpp"
#include "void/http_abi.hpp"
#include "void/os_abi.hpp"
#include "void/host_abi.hpp"
#include "void/format.hpp"
#include "void/temporal.hpp"
#include "void/runtime_abi.hpp"
#include "void/async.hpp"
#include "void/native_binding.hpp"
#include "void/c_primitive.hpp"
#include "void/native_runtime.hpp"
#include "void/reference_capabilities.hpp"
#include <cmath>
#include <array>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Scalar/LoopPassManager.h>
#include <llvm/Transforms/Scalar/SimpleLoopUnswitch.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
namespace vd {
namespace {
std::string global_contract(const ValidatedAnalysis& analysis) {
    auto field = [](std::string_view text) { return std::to_string(text.size()) + ":" + std::string(text); };
    return "void.globals.v1" + field(analysis.entry_identity()) + field(analysis.compatibility_contract());
}
std::string global_contract(const ir::Module& input) {
    if (input.globals.empty()) return {};
    if (input.analysis) return global_contract(*input.analysis);
    std::string out = "void.globals.v1";
    auto field = [&](std::string_view text) { out += std::to_string(text.size()) + ":" + std::string(text); };
    field(input.identity);
    for (const auto& value : input.globals) { field(value.identity); field(value.visibility); field(value.syntax); field(ir::type_name(value.cell)); field(ir::capability_key(value.cell)); }
    field(input.analysis ? input.analysis->compatibility_contract() : input.constant_contract);
    return out;
}
void initialize() {
    static std::once_flag once;
    std::call_once(once, [] {
        if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter())
            throw std::runtime_error("native LLVM target unavailable");
    });
}
void check(llvm::Error error) {
    if (error) throw std::runtime_error(llvm::toString(std::move(error)));
}
template<class T> T unwrap(llvm::Expected<T> value) {
    if (!value) throw std::runtime_error(llvm::toString(value.takeError()));
    return std::move(*value);
}
void remove_dominated_scalar_defaults(llvm::Function& function, llvm::FunctionAnalysisManager& analyses) {
    // Focus on large CFGs, where an extra definition can force expensive
    // dominance-frontier searches during scalar promotion.
    if (function.size() < 128) return;
    auto& entry = function.getEntryBlock();
    llvm::DominatorTree* dominators = nullptr;
    for (auto& instruction : entry) {
        auto* slot = llvm::dyn_cast<llvm::AllocaInst>(&instruction);
        if (!slot || !slot->isStaticAlloca() || slot->isArrayAllocation() || slot->isUsedByMetadata()) continue;
        auto* type = slot->getAllocatedType();
        if (!type->isIntegerTy() && !type->isFloatingPointTy()) continue;

        llvm::StoreInst* initial = nullptr;
        llvm::StoreInst* replacement = nullptr;
        bool private_scalar = true;
        for (auto* user : slot->users()) {
            if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user)) {
                if (load->isSimple() && load->getType() == type) continue;
            }
            auto* store = llvm::dyn_cast<llvm::StoreInst>(user);
            if (!store || !store->isSimple() || store->getPointerOperand() != slot ||
                store->getValueOperand()->getType() != type) {
                private_scalar = false;
                break;
            }
            auto* value = llvm::dyn_cast<llvm::Constant>(store->getValueOperand());
            const bool entry_zero = store->getParent() == &entry && value && value->isNullValue();
            auto*& definition = entry_zero ? initial : replacement;
            if (definition) {
                private_scalar = false;
                break;
            }
            definition = store;
        }
        if (!private_scalar || !initial || !replacement ||
            initial->getMetadata(llvm::LLVMContext::MD_DIAssignID)) continue;
        if (replacement->getParent() == &entry && !initial->comesBefore(replacement)) continue;
        if (!dominators) dominators = &analyses.getResult<llvm::DominatorTreeAnalysis>(function);
        if (llvm::all_of(slot->users(), [&](const llvm::User* user) {
                auto* load = llvm::dyn_cast<llvm::LoadInst>(user);
                return !load || dominators->dominates(replacement, load);
            }))
            initial->eraseFromParent();
    }
}
void finish(llvm::Module& module, bool optimize, llvm::TargetMachine* analysis_machine = nullptr,
            bool version_loops = false) {
    std::string message;
    llvm::raw_string_ostream stream(message);
    { observation::Scope timing(observation::Phase::llvm_verification);
      if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid LLVM lowering: " + message); }
    if (!optimize) observation::skipped(observation::Phase::llvm_optimization);
    if (optimize) {
        observation::Scope optimization_timing(observation::Phase::llvm_optimization);
        llvm::LoopAnalysisManager loops;
        llvm::FunctionAnalysisManager functions;
        llvm::CGSCCAnalysisManager cgscc;
        llvm::ModuleAnalysisManager modules;
        llvm::PipelineTuningOptions tuning;
        tuning.SLPVectorization = false;
        llvm::PassBuilder builder(analysis_machine, tuning);
        if (analysis_machine && version_loops) {
            // Runtime-free AOT: expose invariant control before vectorization.
            // Coordinated modules keep their accepted checkpoint/call pipeline.
            // LLVM owns legality/profitability; strict FP and failures remain.
            builder.registerVectorizerStartEPCallback([](llvm::FunctionPassManager& pipeline,
                                                        llvm::OptimizationLevel) {
                pipeline.addPass(llvm::createFunctionToLoopPassAdaptor(
                    llvm::SimpleLoopUnswitchPass(true), true));
            });
        }
        builder.registerModuleAnalyses(modules);
        builder.registerCGSCCAnalyses(cgscc);
        builder.registerFunctionAnalyses(functions);
        builder.registerLoopAnalyses(loops);
        builder.crossRegisterProxies(loops, functions, cgscc, modules);
        // The proof requests only the dominator tree. Removing stores leaves
        // it valid for the existing pipeline.
        for (auto& function : module) remove_dominated_scalar_defaults(function, functions);
        auto pipeline = builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        pipeline.run(module, modules);
        optimization_timing.finish();
        { observation::Scope timing(observation::Phase::llvm_verification);
          if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid optimized LLVM IR: " + message); }
    }
}
std::unique_ptr<llvm::TargetMachine> native_machine() {
    const auto triple = detail::native_deployment_triple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!machine) throw std::runtime_error("cannot create native target machine");
    return machine;
}
static void check_c_primitive_target(const ir::Module& input, const llvm::Triple& target) {
    if (!input.c_primitives) return;
    const bool windows = input.c_primitives->target() == "x86_64-pc-windows-msvc";
    const bool matching = windows ? target.isOSWindows() && target.getEnvironment() == llvm::Triple::MSVC :
        target.isOSLinux() && target.getEnvironment() == llvm::Triple::GNU;
    if (target.getArch() != llvm::Triple::x86_64 || !matching)
        throw std::invalid_argument("C primitive descriptor does not match the native LLVM target");
}
static llvm::CallInst* call_c_primitive(llvm::IRBuilder<>& b, llvm::Module& module, const CPrimitiveAdmission& admission,
                                      std::uint32_t ordinal, llvm::Value* words, std::uint32_t count, llvm::Value* result,
                                      llvm::Value* spans = nullptr, unsigned span_count = 0) {
    if (const auto& plan = admission.dynamic()) {
        auto bridge = module.getOrInsertFunction(c_dynamic_call, b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(),
            b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
        return b.CreateCall(bridge, {b.CreateGlobalString(plan->bytes()), b.getInt32(plan->bytes().size()),
            b.getInt32(ordinal), words, b.getInt32(count), result,
            spans ? spans : llvm::ConstantPointerNull::get(b.getPtrTy()), b.getInt32(span_count)});
    }
    auto transport = module.getOrInsertFunction(admission.transport(), b.getInt32Ty(), b.getPtrTy(),
        b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy());
    const auto& descriptor = admission.descriptor();
    auto* bytes = b.CreateGlobalString(descriptor);
    if (spans) {
        auto bridge = module.getOrInsertFunction(c_span_bridge, b.getInt32Ty(), b.getPtrTy(),
            b.getPtrTy(), b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(),
            b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
        return b.CreateCall(bridge, {transport.getCallee(), bytes, b.getInt32(descriptor.size()),
            b.getInt32(ordinal), words, b.getInt32(count), result, spans, b.getInt32(span_count)});
    }
    return b.CreateCall(transport, {bytes, b.getInt32(descriptor.size()),
        b.getInt32(ordinal), words, b.getInt32(count), result});
}
void write_object(llvm::Module& module, llvm::TargetMachine& machine, const std::filesystem::path& output) {
    observation::Scope timing(observation::Phase::native_object_emission);
    std::error_code ec;
    llvm::raw_fd_ostream file(path_utf8(output), ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("cannot write object: " + ec.message());
    llvm::legacy::PassManager passes;
    if (machine.addPassesToEmitFile(passes, file, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("object emission unavailable");
    passes.run(module);
    file.flush();
    if (file.has_error()) throw std::runtime_error("object write failed");
}
bool contains_arrays(ir::Type t) {
    if(ir::array_type(t)) return t.kind==ir::Type::fixed_array;
    if(t.kind==ir::Type::aggregate) for(const auto& field:t.definition->fields) if(contains_arrays(field.type)) return true;
    return false;
}


unsigned scalar_encoding(ir::Type t) {
    switch(t.kind) {
#define VD_SCALAR(name) case ir::Type::name: return static_cast<unsigned>(ScalarEncoding::name)
    VD_SCALAR(i8);VD_SCALAR(i16);VD_SCALAR(i32);VD_SCALAR(i64);
    VD_SCALAR(u8);VD_SCALAR(u16);VD_SCALAR(u32);VD_SCALAR(u64);
    VD_SCALAR(f32);VD_SCALAR(f64);VD_SCALAR(boolean);
#undef VD_SCALAR
    default:throw std::invalid_argument("Native fixed arrays require numeric or bool elements");
    }
}
std::string native_symbol(const ir::Function& function, const std::string& suffix) {
    if (function.module_identity.empty()) return function.name + suffix;
    // Injective ASCII encoding, independent of host paths, aliases and addresses.
    // Keep arbitrary VFS punctuation out of platform assembler/linker grammars.
    const char* hex = "0123456789abcdef";
    std::string symbol = "__vd_";
    for (unsigned char c : function.name) { symbol += hex[c >> 4]; symbol += hex[c & 15]; }
    return symbol + suffix;
}
std::string source_message(const SourceLocation& location, const std::string& excerpt,
                           const std::string& description) {
    return location.path + ":" + std::to_string(location.line) + ":" + std::to_string(location.column) +
        ": " + description + "\n" + excerpt;
}
// Reuse the validated construction-view ABI for a bounded local helper, rather
// than passing a stack address through an ordinary managed-reference root.
// Original symbols/signatures remain intact for all other calls and reloads.
struct ElisionHelper { unsigned parameter; std::string module; std::set<unsigned> aliases; };
// Root-copy retirement needs an explicit root-transfer contract in addition to
// freshness. A factory can return fresh storage while also invoking a retaining
// native/callback target; that is not permission to shorten the caller's homes.
bool allocation_root_transfer(ir::Op op) {
    using O=ir::Op;
    switch(op) {
    case O::argument: case O::constant: case O::load: case O::store:
    case O::gc_construct: case O::gc_allocate: case O::gc_read: case O::gc_write:
    case O::gc_poll: case O::gc_collect:
    case O::add: case O::subtract: case O::multiply: case O::divide: case O::remainder:
    case O::less: case O::less_equal: case O::greater: case O::greater_equal:
    case O::equal: case O::not_equal: case O::logical_not: case O::negate: case O::convert:
        return true;
    default:return false;
    }
}
struct ElisionFactory { std::string module; bool root_transfer; };
// A fresh result is current-heap storage on every successful return. This
// bounded straight-line check reuses typed IR; parameter/field/unknown-call
// results are deliberately not an ownership promise.
std::map<std::string,ElisionFactory> allocation_heap_factories(const ir::Module& input,
        const ir::InstructionTypes& types,const std::map<std::string,ir::ReferenceEffects>& effects) {
    std::map<std::string,ElisionFactory> result;
    for(unsigned fn=0;fn<input.functions.size();++fn) {
        const auto& f=input.functions[fn];
        if(f.visibility!="private" || f.module_identity.empty() || f.asynchronous || f.worker || f.receiver ||
           f.initializer || f.closure || f.cleanup_entry || f.blocks.size()!=1 ||
           f.blocks[0].instructions.size()>64 || f.result.kind!=ir::Type::managed_ref || f.result.construction)
            continue;
        const auto summary=effects.find(f.name);
        // Outlined defaults are already typed but have no formal-effect
        // summary here. They are not a fresh-result contract for callers.
        if(summary==effects.end())continue;
        bool formal=false;
        for(const auto& [path,fact]:summary->second.returned)formal|=!fact.origins.empty();
        if(formal || f.blocks[0].end.kind!=ir::Terminator::Kind::return_value)continue;
        std::vector<bool> locals(f.locals.size()),values;
        bool root_transfer=true;
        for(const auto& i:f.blocks[0].instructions) {
            root_transfer &= allocation_root_transfer(i.op) && !i.receiver;
            if(i.op==ir::Op::gc_write && !(numeric::number(types[fn][0][i.b]) || types[fn][0][i.b]==ir::Type::boolean))
                root_transfer=false;
            bool heap=i.op==ir::Op::gc_construct || i.op==ir::Op::gc_allocate || i.op==ir::Op::global_cell ||
                (i.op==ir::Op::constant && types[fn][0][values.size()].kind==ir::Type::managed_ref && i.immediate==0);
            if(i.op==ir::Op::load)heap=locals[i.a];
            if(i.op==ir::Op::store)locals[i.a]=values[i.b];
            values.push_back(heap);
        }
        if(values[f.blocks[0].end.value])result.emplace(f.name,ElisionFactory{f.module_identity,root_transfer});
    }
    return result;
}
std::map<std::string,ElisionHelper> allocation_elision_helpers(const ir::Module& input,
        const ir::InstructionTypes& types,const std::map<std::string,ir::ReferenceEffects>& effects) {
    std::map<std::string,ElisionHelper> result;
    std::set<std::string> names;
    for(const auto& f:input.functions)names.insert(f.name);
    for(unsigned fn=0;fn<input.functions.size();++fn) {
        const auto& f=input.functions[fn];
        if(f.visibility!="private" || f.module_identity.empty() || f.asynchronous || f.worker ||
           f.receiver || f.initializer || f.closure || f.cleanup_entry || !f.virtual_slot.empty() ||
           !f.construction_origins.empty() || f.blocks.empty() || f.blocks.size()>8)
            continue;
        if(!(numeric::number(f.result) || f.result==ir::Type::boolean || f.result==ir::Type::none))continue;
        std::optional<unsigned> parameter;
        bool safe=true;
        for(unsigned n=0;n<f.parameters.size();++n) {
            auto t=f.parameters[n];
            if(t.kind==ir::Type::managed_ref && !t.nullable && !t.construction && t.definition &&
               !t.definition->native_resource && !t.definition->task_result && !t.definition->callable &&
               !t.definition->interface_type && t.definition->base.expired()) {
                if(parameter)safe=false;else parameter=n;
            } else if(!(numeric::number(t) || t==ir::Type::boolean))safe=false;
        }
        if(!safe || !parameter || *parameter>=f.locals.size() || f.locals[*parameter]!=f.parameters[*parameter])continue;
        // A pre-existing construction context need not have the same body in
        // hand-built validated IR. Do not redirect to an unproven namesake.
        const auto specialized=ir::construction_name(f.name,{*parameter});
        if(names.contains(specialized))continue;
        for(auto t:f.locals)
            if(!(numeric::number(t) || t==ir::Type::boolean || (t.kind==ir::Type::managed_ref && !t.construction)))safe=false;
        unsigned instructions=0;
        for(const auto& block:f.blocks)instructions+=block.instructions.size();
        if(instructions>64)continue;
        const auto& summary=effects.at(f.name);
        for(auto origin:summary.retained)if(origin.parameter==*parameter)safe=false;
        for(const auto& [path,fact]:summary.returned)for(auto origin:fact.origins)
            if(origin.parameter==*parameter)safe=false;
        std::set<unsigned> aliases{*parameter},unborrowed_stores,unborrowed_loads;
        for(unsigned bn=0;bn<f.blocks.size();++bn) {
            const auto& block=f.blocks[bn];
            std::vector<bool> borrowed_values(block.instructions.size());
            if(block.end.kind!=ir::Terminator::Kind::return_value && block.end.kind!=ir::Terminator::Kind::return_void &&
               block.end.kind!=ir::Terminator::Kind::jump && block.end.kind!=ir::Terminator::Kind::branch){safe=false;break;}
            if(block.end.kind==ir::Terminator::Kind::branch &&
               types[fn][bn][block.end.value].kind==ir::Type::managed_ref){safe=false;break;}
            for(unsigned id=0;id<block.instructions.size();++id) {
                const auto& i=block.instructions[id];
                if(i.receiver || !i.arguments.empty()) {safe=false;break;}
                using O=ir::Op;
                if(i.op==O::argument && i.a==*parameter)borrowed_values[id]=true;
                if(i.op==O::load && aliases.contains(i.a))borrowed_values[id]=true;
                if(i.op==O::load && !borrowed_values[id])unborrowed_loads.insert(i.a);
                if(i.op==O::store) {
                    if(!borrowed_values[i.b])unborrowed_stores.insert(i.a);
                    if(borrowed_values[i.b]) {
                        if(f.locals[i.a]!=f.parameters[*parameter])safe=false;
                        aliases.insert(i.a);
                    } else if(aliases.contains(i.a))safe=false;
                }
                switch(i.op) {
                case O::argument: case O::constant: case O::load: case O::store:
                case O::gc_read: case O::gc_poll: case O::gc_collect:
                case O::add: case O::subtract: case O::multiply: case O::divide: case O::remainder:
                case O::less: case O::less_equal: case O::greater: case O::greater_equal:
                case O::equal: case O::not_equal: case O::logical_not: case O::negate: case O::convert:
                    break;
                case O::gc_write:
                    if(!(numeric::number(types[fn][bn][i.b]) || types[fn][bn][i.b]==ir::Type::boolean))safe=false;
                    break;
                default:safe=false;break;
                }
                // Identity/null/type inspection of the borrowed pointer is outside
                // this initial helper proof, even when a summary reports no escape.
                auto pointer=[&](unsigned value){return value<id && types[fn][bn][value].kind==ir::Type::managed_ref;};
                if(i.op!=O::gc_read && i.op!=O::gc_write && i.op!=O::store && i.op!=O::load &&
                   i.op!=O::argument && i.op!=O::constant && i.op!=O::gc_poll && i.op!=O::gc_collect &&
                   (pointer(i.a) || pointer(i.b)))safe=false;
            }
            for(unsigned id=0;id<block.instructions.size();++id) {
                const auto& i=block.instructions[id];
                if(i.op==ir::Op::store && aliases.contains(i.a) && !borrowed_values[i.b])safe=false;
                if(i.op==ir::Op::load && aliases.contains(i.a) && !borrowed_values[id])safe=false;
            }
        }
        // An alias discovered in a later block cannot retroactively bless an
        // earlier nonformal store/load. Construction-origin validation below
        // independently checks the selected clone across the original CFG.
        for(auto slot:aliases)if(unborrowed_stores.contains(slot) || unborrowed_loads.contains(slot))safe=false;
        if(safe)result.emplace(f.name,ElisionHelper{*parameter,f.module_identity,std::move(aliases)});
    }
    return result;
}
// The proof owns both heap-elision eligibility and omission of pointer homes.
// A stack address may flow only through private, single-origin local aliases and
// field receivers. Every load must belong to the current construction instance:
// reaching a construction starts a new epoch, even on a loop backedge. Children
// remain ordinary heap references, rooted separately in the stack payload.
struct BlockAllocationElision {
    std::vector<std::set<unsigned>> values;
    std::set<unsigned> locals;
    std::set<std::pair<unsigned,unsigned>> calls;
    // Instruction after which a proven payload has no remaining owner uses.
    // Child SSA/local homes remain independently authoritative.
    struct Retirement {
        std::vector<std::pair<unsigned,unsigned>> payloads;
        std::vector<unsigned> values,locals;
    };
    std::map<std::pair<unsigned,unsigned>,Retirement> retirements;
    std::vector<std::vector<unsigned>> predecessors;
    bool precise_copies=false;
};
BlockAllocationElision block_allocation_elision(const ir::Function &f,
                                                const std::vector<std::vector<ir::Type>> &instruction_types,
                                                const llvm::DataLayout &layout,
                                                const std::function<llvm::Type *(ir::Type)> &type,
                                                const std::map<std::string,ElisionHelper>& helpers,
                                                const std::map<std::string,ElisionFactory>& factories) {
    using O = ir::Op;
    BlockAllocationElision result{std::vector<std::set<unsigned>>(f.blocks.size()), {}, {}, {}, {}, false};
    if (f.asynchronous || f.receiver || f.initializer || f.cleanup_entry)
        return result;
    struct Use { unsigned block, index; enum Kind { field, store, call, alias, escape } kind; };
    using Position = std::pair<unsigned,unsigned>;
    std::vector<std::vector<std::vector<Use>>> uses(f.blocks.size());
    std::vector<std::vector<Position>> local_uses(f.locals.size());
    std::vector<std::vector<unsigned>> predecessors(f.blocks.size());
    bool retire_copies=!f.worker && !f.closure && f.virtual_slot.empty();
    auto nullable_alias=[](ir::Type a,ir::Type b) {
        if(a.kind!=ir::Type::managed_ref || b.kind!=ir::Type::managed_ref ||
           a.construction || b.construction || a.readonly!=b.readonly ||
           a.readonly_references!=b.readonly_references)return false;
        a.nullable=b.nullable=false;return a==b;
    };
    // Index once. Candidate walks visit their alias uses, rather than rescanning
    // every instruction in the function for each locally constructed object.
    for (unsigned bn=0;bn<f.blocks.size();++bn) {
        const auto& block=f.blocks[bn]; uses[bn].resize(block.instructions.size());
        auto use=[&](unsigned value,unsigned index,Use::Kind kind) {
            if(value<uses[bn].size() && instruction_types[bn][value].kind==ir::Type::managed_ref)
                uses[bn][value].push_back({bn,index,kind});
        };
        for(unsigned id=0;id<block.instructions.size();++id) {
            const auto& i=block.instructions[id];
            if(i.op==O::local_cleanup || i.op==O::cleanup_call || i.op==O::gc_release)
                return result;
            if(i.receiver)local_uses[i.receiver->slot].push_back({bn,id});
            const auto helper=helpers.find(i.callee);
            const bool borrowed=i.op==O::call && !i.receiver && helper!=helpers.end() && helper->second.module==f.module_identity;
            // Earlier copy retirement is bounded to synchronous typed operations
            // with known root transfer. Unknown/public/native/callback/async and
            // lifecycle operations retain the original conservative homes.
            const auto factory=factories.find(i.callee);
            const bool factory_transfer=i.op==O::call && !i.receiver && factory!=factories.end() &&
                factory->second.module==f.module_identity && factory->second.root_transfer;
            if(!(allocation_root_transfer(i.op) || borrowed || factory_transfer) || i.receiver)
                retire_copies=false;
            // Unlike construction, a field write has no new publication-time
            // child-heap check. Keep conservative homes for reference writes.
            if(i.op==O::gc_write && !(numeric::number(instruction_types[bn][i.b]) || instruction_types[bn][i.b]==ir::Type::boolean))
                retire_copies=false;
            for(unsigned arg=0;arg<i.arguments.size();++arg)
                use(i.arguments[arg],id,borrowed && arg==helper->second.parameter?Use::call:Use::escape);
            switch(i.op) {
            case O::load: local_uses[i.a].push_back({bn,id}); break;
            case O::store: local_uses[i.a].push_back({bn,id}); use(i.b,id,Use::store); break;
            case O::call:
                if(i.arguments.empty())use(i.a,id,borrowed && helper->second.parameter==0?Use::call:Use::escape);
                break;
            case O::gc_read: use(i.a,id,Use::field); break;
            case O::gc_write: use(i.a,id,Use::field); use(i.b,id,Use::escape); break;
            case O::convert:
                use(i.a,id,nullable_alias(instruction_types[bn][i.a],instruction_types[bn][id])?Use::alias:Use::escape);break;
            case O::negate: case O::logical_not: case O::bitcast:
                use(i.a,id,Use::escape); break;
            case O::constant: case O::argument: case O::stage_begin:
            case O::gc_construct: case O::gc_allocate: case O::global_cell:
            case O::gc_poll: case O::gc_collect: case O::string_literal:
            case O::function_null: case O::dict_new: break;
            default:
                // Unknown operand conventions can only reduce eligibility.
                use(i.a,id,Use::escape); use(i.b,id,Use::escape); break;
            }
        }
        const auto& end=block.end;
        if(end.kind==ir::Terminator::Kind::return_value || end.kind==ir::Terminator::Kind::branch)
            use(end.value,~0u,Use::escape);
        if(end.kind==ir::Terminator::Kind::jump || end.kind==ir::Terminator::Kind::branch)
            predecessors[end.yes].push_back(bn);
        if(end.kind==ir::Terminator::Kind::branch)predecessors[end.no].push_back(bn);
    }
    // Propagate unproven heap affinity through the SAME indexed alias uses.
    // No incoming pointer becomes trusted merely because its type is managed.
    // Heap construction validates child ownership; elision may omit that check
    // only when every initial/replacement child is already proved current-heap
    // (or null). Any construction used as a child remains heap-backed because
    // field publication is an escape in the alias proof below.
    // Heap affinity alone is insufficient to trust fields (allocation and
    // initialization may be separate). Limit immutable-field provenance to a
    // completed gc_construct, directly or through locals whose every stored
    // origin is one of those constructions (including proved local aliases).
    // Build this fact once from the existing local index, never per candidate.
    std::vector<bool> constructed_locals(f.locals.size()),completed_candidate(f.locals.size());
    std::vector<unsigned> dependencies(f.locals.size()),completed;
    std::vector<std::vector<unsigned>> completed_users(f.locals.size());
    for(unsigned slot=0;slot<f.locals.size();++slot) {
        bool stored=false,possible=slot>=f.parameters.size();
        for(auto [bn,id]:local_uses[slot]) {
            const auto& i=f.blocks[bn].instructions[id];
            if(i.receiver)possible=false;
            if(i.op!=O::store)continue;
            stored=true;const auto& value=f.blocks[bn].instructions[i.b];
            if(value.op==O::gc_construct)continue;
            if(value.op==O::load) {
                ++dependencies[slot];completed_users[value.a].push_back(slot);
            } else possible=false;
        }
        completed_candidate[slot]=stored && possible;
        if(completed_candidate[slot] && !dependencies[slot]) {
            constructed_locals[slot]=true;completed.push_back(slot);
        }
    }
    // Least fixed point rooted only in completed constructions. An incoming
    // parameter, factory, uncertain merge or self-supporting alias cycle can
    // never certify its own fields. Visit each indexed store dependency once.
    for(unsigned cursor=0;cursor<completed.size();++cursor)
        for(auto slot:completed_users[completed[cursor]])
            if(!--dependencies[slot] && completed_candidate[slot]) {
                constructed_locals[slot]=true;completed.push_back(slot);
            }
    std::vector<std::vector<bool>> unproven(f.blocks.size());
    std::vector<bool> unproven_locals(f.locals.size());
    std::vector<Position> uncertain;
    for(unsigned bn=0;bn<f.blocks.size();++bn) {
        unproven[bn].resize(f.blocks[bn].instructions.size());
        for(unsigned id=0;id<f.blocks[bn].instructions.size();++id) {
            const auto& i=f.blocks[bn].instructions[id];
            if(instruction_types[bn][id].kind!=ir::Type::managed_ref)continue;
            const auto factory=factories.find(i.callee);
            // A successful construction validates immutable child bindings.
            // The owner's current-heap proof is propagated below; managed type
            // alone (including an incoming readonly owner) grants no affinity.
            bool immutable_child=false;
            if(i.op==O::gc_read && instruction_types[bn][i.a].kind==ir::Type::managed_ref &&
               instruction_types[bn][i.a].definition->fields.at(i.immediate).readonly) {
                const auto& owner=f.blocks[bn].instructions[i.a];
                immutable_child=owner.op==O::gc_construct ||
                    (owner.op==O::load && constructed_locals[owner.a]);
            }
            const bool heap=immutable_child || i.op==O::load || i.op==O::gc_construct || i.op==O::gc_allocate || i.op==O::global_cell ||
                (i.op==O::constant && i.immediate==0) ||
                (i.op==O::call && !i.receiver && factory!=factories.end() && factory->second.module==f.module_identity);
            if(!heap){unproven[bn][id]=true;uncertain.push_back({bn,id});}
        }
    }
    for(std::size_t next=0;next<uncertain.size();++next) {
        auto [block,id]=uncertain[next];
        for(const auto& use:uses[block][id]) {
            // Only field/store uses propagate heap affinity through an instruction.
            // Return/branch escapes carry ~0u, not an instruction index.
            if(use.kind!=Use::field && use.kind!=Use::store)continue;
            const auto& access=f.blocks[use.block].instructions[use.index];
            if(use.kind==Use::field && access.op==O::gc_read &&
               instruction_types[use.block][use.index].kind==ir::Type::managed_ref &&
               !unproven[use.block][use.index]) {
                unproven[use.block][use.index]=true;uncertain.push_back({use.block,use.index});
            }
            if(use.kind!=Use::store)continue;
            const auto slot=f.blocks[use.block].instructions[use.index].a;
            if(unproven_locals[slot])continue;
            unproven_locals[slot]=true;
            for(auto [lb,li]:local_uses[slot])if(f.blocks[lb].instructions[li].op==O::load && !unproven[lb][li]) {
                unproven[lb][li]=true;uncertain.push_back({lb,li});
            }
        }
    }
    // Per-function scratch for the SAME must-alias proof. Small alias sets use
    // one word per block; no candidate allocates a tree node for every state.
    std::vector<std::uint64_t> must_in,must_out;
    std::vector<unsigned> must_work,alias_bits;
    std::vector<bool> must_queued;
    unsigned elided_bytes=0;
    for(unsigned bn=0;bn<f.blocks.size();++bn)for(unsigned origin=0;origin<f.blocks[bn].instructions.size();++origin) {
        const auto& construct=f.blocks[bn].instructions[origin];
        auto t=instruction_types[bn][origin];
        if(construct.op!=O::gc_construct || t.kind!=ir::Type::managed_ref || !t.definition ||
           t.construction || t.definition->native_resource || t.definition->task_result ||
           !t.definition->base.expired() || t.definition->callable || t.definition->interface_type)
            continue;
        bool supported=true,has_references=false;
        for(unsigned field=0;field<t.definition->fields.size();++field) {
            ir::Type ft=t.definition->fields[field].type;
            const bool child=ft.kind==ir::Type::managed_ref && !ft.construction && ft.definition &&
                !ft.definition->native_resource && !ft.definition->task_result && !ft.definition->callable &&
                !ft.definition->interface_type;
            if(!(numeric::integer(ft) || numeric::floating(ft) || ft==ir::Type::boolean || child))supported=false;
            has_references|=child;
            if(child && unproven[bn][construct.arguments[field]])supported=false;
        }
        const auto bytes=layout.getTypeAllocSize(type(ir::Type(t.definition))).getFixedValue();
        if(!supported || !bytes || bytes>256 || elided_bytes+bytes>16384)continue;
        std::set<Position> values{{bn,origin}};
        std::set<unsigned> locals;
        std::set<Position> calls;
        std::vector<Position> pending{{bn,origin}};
        bool safe=true,block_lifetime=true,nullable_flow=false;
        std::vector<Position> lifetime_uses;
        unsigned final_use=origin;
        auto lifetime_use=[&](unsigned block,unsigned id) {
            lifetime_uses.push_back({block,id});
            if(block!=bn || id<origin)block_lifetime=false;
            else final_use=std::max(final_use,id);
        };
        for(std::size_t next=0;next<pending.size() && safe;++next) {
            auto [block,id]=pending[next];
            lifetime_use(block,id);
            for(const auto& use:uses[block][id]) {
                lifetime_use(use.block,use.index);
                if(use.kind==Use::escape){safe=false;break;}
                if(use.kind==Use::alias) {
                    nullable_flow=true;
                    if(values.insert({use.block,use.index}).second)pending.push_back({use.block,use.index});
                    continue;
                }
                if(use.kind==Use::field) {
                    const auto& access=f.blocks[use.block].instructions[use.index];
                    if(access.op==O::gc_write && instruction_types[use.block][access.b].kind==ir::Type::managed_ref &&
                       unproven[use.block][access.b]){safe=false;break;}
                }
                if(use.kind==Use::call){calls.insert({use.block,use.index});continue;}
                if(use.kind!=Use::store)continue;
                const auto slot=f.blocks[use.block].instructions[use.index].a;
                if(f.locals[slot]!=t) {
                    if(!nullable_alias(f.locals[slot],t)){safe=false;break;}
                    nullable_flow=true;
                }
                if(!locals.insert(slot).second)continue;
                for(auto [lb,li]:local_uses[slot]) {
                    const auto& i=f.blocks[lb].instructions[li];
                    if(i.receiver){safe=false;break;}
                    if(i.op==O::load && values.insert({lb,li}).second)pending.push_back({lb,li});
                }
            }
        }
        if(!safe)continue;
        // One origin per local. A rebound local could still expose an older
        // loop instance or another object's identity; keep that case on heap.
        std::vector<std::vector<Position>> events(f.blocks.size());
        for(auto slot:locals)for(auto p:local_uses[slot]) {
            const auto& i=f.blocks[p.first].instructions[p.second];
            lifetime_use(p.first,p.second);
            if(i.op==O::store && !values.contains({p.first,i.b}))safe=false;
            events[p.first].push_back(p);
        }
        if(!safe)continue;
        std::optional<Position> joined_retirement;
        if(has_references && !block_lifetime && !f.worker && !f.closure && f.virtual_slot.empty()) {
            const auto& fork=f.blocks[bn].end;
            if(fork.kind==ir::Terminator::Kind::branch && fork.yes!=fork.no && fork.yes!=bn && fork.no!=bn) {
                std::set<unsigned> region{bn,fork.yes,fork.no};
                // A direct arm, or ONE nested diamond with a direct exit.
                // Vertices are distinct and internal edges acyclic. Existing
                // must-alias validation still rejects unsafe incoming paths.
                auto arm_exit=[&](unsigned arm)->std::optional<unsigned> {
                    const auto& end=f.blocks[arm].end;
                    if(end.kind==ir::Terminator::Kind::jump)return end.yes;
                    if(end.kind!=ir::Terminator::Kind::branch || end.yes==end.no ||
                       region.contains(end.yes) || region.contains(end.no))return {};
                    const auto& left=f.blocks[end.yes].end;const auto& right=f.blocks[end.no].end;
                    if(left.kind!=ir::Terminator::Kind::jump || right.kind!=ir::Terminator::Kind::jump ||
                       left.yes!=right.yes || left.yes==end.yes || left.yes==end.no || region.contains(left.yes))return {};
                    const auto& tail=f.blocks[left.yes].end;
                    if(tail.kind!=ir::Terminator::Kind::jump)return {};
                    region.insert(end.yes);region.insert(end.no);region.insert(left.yes);
                    return tail.yes;
                };
                const auto left=arm_exit(fork.yes),right=arm_exit(fork.no);
                if(left && right && *left==*right && !region.contains(*left) && region.size()<=6) {
                    auto join=*left;region.insert(join);bool bounded=true;
                    // One empty common join may forward directly to an existing
                    // nonempty retirement block. Keep BOTH entry checkpoints;
                    // clear after its first instruction, never across an edge.
                    if(f.blocks[join].instructions.empty()) {
                        const auto& end=f.blocks[join].end;
                        if(end.kind!=ir::Terminator::Kind::jump || region.contains(end.yes) ||
                           f.blocks[end.yes].instructions.empty())bounded=false;
                        else {join=end.yes;region.insert(join);}
                    }
                    unsigned last=0;
                    for(auto [block,id]:lifetime_uses) {
                        if(block==bn){if(id<origin)bounded=false;}
                        else if(!region.contains(block))bounded=false;
                        if(block==join)last=std::max(last,id);
                    }
                    if(bounded)joined_retirement=Position{join,last};
                }
            }
        }
        // Nullable/cast flow stays within a bounded reconverging region (at most eight blocks).
        // Existing epoch/must-alias validation below still rejects old instances,
        // rebinding, pre-birth loads and every escaping owner use.
        if(nullable_flow && (!has_references || !joined_retirement))continue;
        if(has_references) {
            const auto& instructions=f.blocks[bn].instructions;
            const bool owning_local=origin+1<instructions.size() && instructions[origin+1].op==O::store &&
                instructions[origin+1].b==origin;
            // Existing CFG owners retain their accepted lifetime. New direct
            // temporaries must have no local publication and an exact bounded
            // lifetime in their construction block; SSA cannot cross IR edges.
            if(!owning_local && (!locals.empty() || !block_lifetime) && !joined_retirement)continue;
        }
        events[bn].push_back({bn,origin});
        for(auto& block:events) {std::sort(block.begin(),block.end());block.erase(std::unique(block.begin(),block.end()),block.end());}
        // Must-dataflow over the indexed alias events. Reset at every birth;
        // loads before a rebinding store fail, including loop-carried aliases.
        // With no local aliases the lattice is empty: birth is its only event,
        // so the accepted proof is vacuous. Larger sets keep the original solver.
        if(!locals.empty() && locals.size()<=64) {
            alias_bits.resize(f.locals.size());unsigned bit=0;
            for(auto slot:locals)alias_bits[slot]=bit++;
            const auto all=locals.size()==64?~std::uint64_t{0}:(std::uint64_t{1}<<locals.size())-1;
            must_in.resize(f.blocks.size());must_out.assign(f.blocks.size(),all);
            must_queued.assign(f.blocks.size(),true);must_work.clear();must_work.reserve(f.blocks.size());
            // Reverse insertion visits source order initially. Thereafter only
            // a changed predecessor output schedules a successor, once at a time.
            for(unsigned block=f.blocks.size();block-->0;)must_work.push_back(block);
            auto enqueue=[&](unsigned block) {
                if(!must_queued[block]){must_queued[block]=true;must_work.push_back(block);}
            };
            while(!must_work.empty()) {
                const auto block=must_work.back();must_work.pop_back();must_queued[block]=false;
                auto state=(block==0 || predecessors[block].empty())?std::uint64_t{0}:all;
                for(auto pred:predecessors[block])state&=must_out[pred];
                must_in[block]=state;
                for(auto [eb,id]:events[block]) {
                    if(eb==bn && id==origin)state=0;
                    else if(f.blocks[eb].instructions[id].op==O::store)
                        state|=std::uint64_t{1}<<alias_bits[f.blocks[eb].instructions[id].a];
                }
                if(must_out[block]!=state) {
                    must_out[block]=state;const auto& end=f.blocks[block].end;
                    if(end.kind==ir::Terminator::Kind::jump || end.kind==ir::Terminator::Kind::branch)enqueue(end.yes);
                    if(end.kind==ir::Terminator::Kind::branch)enqueue(end.no);
                }
            }
            for(unsigned block=0;block<f.blocks.size() && safe;++block) {
                auto state=must_in[block];
                for(auto [eb,id]:events[block]) {
                    const auto& i=f.blocks[eb].instructions[id];
                    if(eb==bn && id==origin)state=0;
                    else if(i.op==O::store)state|=std::uint64_t{1}<<alias_bits[i.a];
                    else if(i.op==O::load && !(state&(std::uint64_t{1}<<alias_bits[i.a]))){safe=false;break;}
                }
            }
        } else if(!locals.empty()) {
            std::vector<std::set<unsigned>> incoming(f.blocks.size(),locals),outgoing(f.blocks.size(),locals);
            bool changed;
            do {
                changed=false;
                for(unsigned block=0;block<f.blocks.size();++block) {
                    auto state=(block==0 || predecessors[block].empty())?std::set<unsigned>{}:locals;
                    for(auto pred:predecessors[block])std::erase_if(state,[&](unsigned slot){return !outgoing[pred].contains(slot);});
                    if(incoming[block]!=state){incoming[block]=state;changed=true;}
                    for(auto [eb,id]:events[block]) {
                        if(eb==bn && id==origin)state.clear();
                        else if(f.blocks[eb].instructions[id].op==O::store)state.insert(f.blocks[eb].instructions[id].a);
                    }
                    if(outgoing[block]!=state){outgoing[block]=std::move(state);changed=true;}
                }
            }while(changed);
            for(unsigned block=0;block<f.blocks.size() && safe;++block) {
                auto state=incoming[block];
                for(auto [eb,id]:events[block]) {
                    const auto& i=f.blocks[eb].instructions[id];
                    if(eb==bn && id==origin)state.clear();
                    else if(i.op==O::store)state.insert(i.a);
                    else if(i.op==O::load && !state.contains(i.a)){safe=false;break;}
                }
            }
        }
        if(safe) {
            for(auto [block,id]:values)result.values[block].insert(id);
            result.locals.insert(locals.begin(),locals.end());
            result.calls.insert(calls.begin(),calls.end());
            if(has_references && block_lifetime)
                result.retirements[{bn,final_use}].payloads.push_back({bn,origin});
            else if(has_references && joined_retirement)
                result.retirements[*joined_retirement].payloads.push_back({bn,origin});
            elided_bytes+=bytes;
        }
    }
    if(retire_copies) {
        auto ordinary=[](ir::Type t) {
            return t.kind==ir::Type::managed_ref && !t.construction && t.definition &&
                !t.definition->native_resource && !t.definition->task_result && !t.definition->callable &&
                !t.definition->interface_type && t.definition->base.expired();
        };
        // SSA values are block-local. At final use, every surviving copy has
        // already reached its own existing root home (including a child read).
        // Returned values retain their home through frame exit.
        for(unsigned bn=0;bn<f.blocks.size();++bn)for(unsigned id=0;id<uses[bn].size();++id) {
            if(!ordinary(instruction_types[bn][id]) || result.values[bn].contains(id))continue;
            unsigned last=id;
            for(const auto& use:uses[bn][id])last=std::max(last,use.index);
            if(last!=~0u)result.retirements[{bn,last}].values.push_back(id);
        }
        // Only block-local homes with every store before every load can retire.
        // Thus each loop instance overwrites the home before any new load; a
        // loop-carried read-before-store, receiver place or CFG alias falls back.
        for(unsigned slot=f.parameters.size();slot<f.locals.size();++slot) {
            if(!ordinary(f.locals[slot]) || result.locals.contains(slot) || local_uses[slot].empty())continue;
            const auto bn=local_uses[slot].front().first;
            bool safe=true,stored=false;unsigned last_store=0,first_load=~0u,last=0;
            for(auto [block,id]:local_uses[slot]) {
                const auto& i=f.blocks[block].instructions[id];
                if(block!=bn || i.receiver){safe=false;break;}
                if(i.op==O::store){stored=true;last_store=std::max(last_store,id);}
                else if(i.op==O::load)first_load=std::min(first_load,id);
                else {safe=false;break;}
                last=std::max(last,id);
            }
            if(safe && stored && last_store<first_load)
                result.retirements[{bn,last}].locals.push_back(slot);
        }
    }
    // Retain the G39 payload/copy lifetime safety floor. Optional copies
    // elsewhere need a reachable collection driver before function exit.
    // Polls service external requests; they do not initiate collection.
    const bool payload_lifetime=std::any_of(result.retirements.begin(),result.retirements.end(),
        [](const auto& event){return !event.second.payloads.empty();});
    auto collection_driver=[&](unsigned bn,unsigned id) {
        const auto& i=f.blocks[bn].instructions[id];
        const auto factory=factories.find(i.callee);
        return i.op==O::gc_collect || i.op==O::gc_allocate ||
            (i.op==O::gc_construct && !result.values[bn].contains(id)) ||
            (i.op==O::call && factory!=factories.end() &&
             factory->second.module==f.module_identity && factory->second.root_transfer);
    };
    // One reverse reachability walk over the existing CFG. In particular,
    // a later loop instance's allocation can observe this instance's roots.
    // Lack of a same-block driver alone does not make its clears redundant.
    std::vector<bool> reaches_driver(f.blocks.size()),successor_driver(f.blocks.size());
    std::vector<unsigned> pending;
    for(unsigned bn=0;bn<f.blocks.size();++bn)
        for(unsigned id=0;id<f.blocks[bn].instructions.size();++id)
            if(collection_driver(bn,id)) {reaches_driver[bn]=true;pending.push_back(bn);break;}
    for(unsigned cursor=0;cursor<pending.size();++cursor)
        for(auto pred:predecessors[pending[cursor]])if(!reaches_driver[pred]) {
            reaches_driver[pred]=true;pending.push_back(pred);
        }
    for(unsigned bn=0;bn<f.blocks.size();++bn)if(reaches_driver[bn])
        for(auto pred:predecessors[bn])successor_driver[pred]=true;
    for(unsigned bn=0;bn<f.blocks.size();++bn) {
        const auto& instructions=f.blocks[bn].instructions;
        bool driver=successor_driver[bn];
        for(unsigned id=instructions.size();id-->0;) {
            if(auto event=result.retirements.find({bn,id});event!=result.retirements.end() &&
               !payload_lifetime && !driver) {
                event->second.values.clear();event->second.locals.clear();
            }
            driver |= collection_driver(bn,id);
        }
        // Batch only across intervals with no observable safepoint. Calls and
        // polls remain barriers, including polls which can service another
        // mutator's collection. Never move across an implicit block-entry poll,
        // a root-home overwrite on a CFG edge, or a mandatory payload clear.
        std::vector<unsigned> values,locals;
        auto flush=[&](unsigned id) {
            if(values.empty() && locals.empty())return;
            auto& target=result.retirements[{bn,id}];
            target.values.insert(target.values.end(),values.begin(),values.end());
            target.locals.insert(target.locals.end(),locals.begin(),locals.end());
            values.clear();locals.clear();
        };
        for(unsigned id=0;id<instructions.size();++id) {
            const auto op=instructions[id].op;
            const bool barrier=op==O::call || op==O::gc_poll || op==O::gc_collect ||
                op==O::gc_allocate || (op==O::gc_construct && !result.values[bn].contains(id));
            if(barrier && id)flush(id-1);
            if(auto event=result.retirements.find({bn,id});event!=result.retirements.end()) {
                values.insert(values.end(),event->second.values.begin(),event->second.values.end());
                locals.insert(locals.end(),event->second.locals.begin(),event->second.locals.end());
                event->second.values.clear();event->second.locals.clear();
                if(!event->second.payloads.empty())flush(id);
            }
        }
        if(!instructions.empty())flush(instructions.size()-1);
    }
    result.precise_copies=retire_copies;
    result.predecessors=std::move(predecessors);
    std::erase_if(result.retirements,[](const auto& event) {
        const auto& r=event.second;return r.payloads.empty() && r.values.empty() && r.locals.empty();
    });
    return result;
}
std::unique_ptr<llvm::Module> lower(const ir::Module& source, llvm::LLVMContext& context,
                                  const std::string& suffix, const llvm::DataLayout& target_layout, const std::string& exported_entry,
                                  std::vector<NominalDescriptor>* host_catalog=nullptr,std::vector<InterfaceWitness>* host_witnesses=nullptr,
                                  llvm_backend::LoweringInventory* inventory=nullptr,
                                  bool fixed_call_research=false,std::optional<project::DebugInfo> source_debug={},bool optimized=false) {
    // Pure default DAGs are outlined into ordinary private typed-IR functions.
    // The normal shared lowerer supplies roots, failure cleanup and native ABI;
    // defaults execute once per element without expanding IR by array extent.
    ir::Module input=source;
    // Snapshot value-array parameters at enqueue, through ordinary checked IR.
    std::vector<ir::Function> snapshots;
    for(const auto& f:source.functions)if(f.asynchronous)for(unsigned n=0;n<f.parameters.size();++n)if(contains_arrays(f.parameters[n])){
        ir::Function copy;copy.name=f.name+"::$asynccopy."+std::to_string(n);copy.source=f.source;copy.module_identity=f.module_identity;copy.source_name=copy.name.substr(f.module_identity.size()+2);copy.visibility="private";copy.parameters={f.parameters[n]};copy.result=f.parameters[n];
        copy.blocks={{{{ir::Op::argument}},{ir::Terminator::Kind::return_value,0}}};snapshots.push_back(std::move(copy));
    }
    input.functions.insert(input.functions.end(),snapshots.begin(),snapshots.end());
    ir::InstructionTypes checked_types;
    std::map<std::string,ir::ReferenceEffects> verified_effects;
    ir::validate(input,&checked_types,&verified_effects);
    using Origin=std::tuple<std::string,unsigned,unsigned>;
    std::map<Origin,Origin> origins;
    std::map<Origin,std::string> defaults,outlined;
    std::set<std::string> default_functions,names;std::size_t default_serial=0;
    for(const auto& f:input.functions){names.insert(f.name);for(unsigned b=0;b<f.blocks.size();++b)for(unsigned i=0;i<f.blocks[b].instructions.size();++i)origins[{f.name,b,i}]={f.name,b,i};}
    for(unsigned index=0;index<input.functions.size();++index) {
        const auto owner=input.functions[index];
        for(unsigned block=0;block<owner.blocks.size();++block)for(unsigned position=0;position<owner.blocks[block].instructions.size();++position) {
            const auto& instruction=owner.blocks[block].instructions[position];
            if(instruction.op!=ir::Op::fixed_array || !instruction.array_repeat || ir::scalar_fixed_array(instruction.type) || !*instruction.type.definition->extent)continue;
            const auto prototype=instruction.arguments.front();const auto origin=origins.at({owner.name,block,prototype});
            auto existing=defaults.find(origin);
            if(existing!=defaults.end()){outlined[{owner.name,block,position}]=existing->second;continue;}
            std::string name,short_name;do{short_name="$array_default_"+std::to_string(default_serial++);name=owner.module_identity.empty()?short_name:owner.module_identity+"::"+short_name;}while(names.contains(name));names.insert(name);default_functions.insert(name);defaults.emplace(origin,name);outlined[{owner.name,block,position}]=name;
            std::set<unsigned> needed;std::function<void(unsigned)> collect=[&](unsigned id){if(!needed.insert(id).second)return;for(auto child:owner.blocks[block].instructions[id].arguments)collect(child);};collect(prototype);
            ir::Function helper;helper.name=name;helper.source=instruction.source;helper.source_name=short_name;helper.module_identity=owner.module_identity;helper.visibility="private";helper.parameters.clear();helper.result=ir::array_element(instruction.type);helper.blocks.emplace_back();
            std::map<unsigned,unsigned> remap;std::vector<ir::Type> helper_types;
            for(auto id:needed){auto copied=owner.blocks[block].instructions[id];for(auto& child:copied.arguments)child=remap.at(child);auto next=static_cast<unsigned>(helper.blocks[0].instructions.size());remap[id]=next;origins[{name,0,next}]=origins.at({owner.name,block,id});helper.blocks[0].instructions.push_back(std::move(copied));helper_types.push_back(checked_types[index][block][id]);}
            helper.blocks[0].end={ir::Terminator::Kind::return_value,remap.at(prototype)};input.functions.push_back(std::move(helper));checked_types.push_back({std::move(helper_types)});
        }
    }
    auto module = std::make_unique<llvm::Module>(input.identity, context);
    module->setDataLayout(target_layout);
#ifdef VOID_LLVM_RESEARCH
    std::uint64_t home_serial=0;
#else
    (void)inventory; (void)fixed_call_research;
#endif
    auto home = [&](llvm::AllocaInst* a, StorageRole role, llvm_backend::HomeKind kind) {
#ifdef VOID_LLVM_RESEARCH
        if(inventory) {
            inventory->function(*a->getFunction(),false);
            inventory->home(*a,role,kind,a->getFunction()->getName().str()+"/home/"+std::to_string(++home_serial));
        }
#else
        (void)role; (void)kind;
#endif
        return a;
    };
    // StructLayout caches use raw context-owned Type pointers as keys. Query
    // the module-owned copy, never the LLJIT layout that outlives these types.
    const auto& layout = module->getDataLayout();
    llvm::IRBuilder<> b(context);
#ifdef VOID_LLVM_RESEARCH
    if(fixed_call_research) {
#include "private_fixed_call_fixture.inc"
    }
#endif
    std::map<std::string,llvm::StructType*> aggregates;
    auto* interface_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy());
    auto* prepared_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty());
    std::function<llvm::Type*(ir::Type)> type = [&](ir::Type t) -> llvm::Type* {
        if(ir::collection_type(t)) {
            if(!input.coordinated) throw std::invalid_argument("Native fixed arrays require a coordinated Runtime");
            return b.getPtrTy();
        }
        if(t.kind==ir::Type::interface_ref) return interface_type;
        if(t.kind==ir::Type::prepared_receiver) return prepared_type;
        if (t.kind == ir::Type::managed_ref || t.kind==ir::Type::string) return b.getPtrTy();
        if (t.kind == ir::Type::aggregate) {
            if (auto found = aggregates.find(t.definition->identity); found != aggregates.end()) return found->second;
            auto* result = llvm::StructType::create(context,t.definition->identity); aggregates.emplace(t.definition->identity,result);
            std::vector<llvm::Type*> fields;
            auto base=t.definition->base.lock();
            if(base) fields.push_back(type(ir::Type(base)));
            for(unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<t.definition->fields.size();++n) fields.push_back(type(t.definition->fields[n].type));
            result->setBody(fields,false); return result;
        }
        if (t == ir::Type::boolean) return b.getInt1Ty();
        if (t == ir::Type::none) return b.getVoidTy();
        if (t == ir::Type::f32) return b.getFloatTy();
        if (t == ir::Type::f64) return b.getDoubleTy();
        if(t.kind==ir::Type::numeric_enum)t=*t.definition->enum_backing;
        return b.getIntNTy(numeric::width(t));
    };
    auto checked_integer=[&](llvm::Value* value,ir::Type from,ir::Type to) {
        auto* wide_type=b.getIntNTy(128);
        auto extend=[&](std::uint64_t bits) {
            auto limit=llvm::APInt(numeric::width(to),bits);
            return llvm::ConstantInt::get(context,numeric::signed_integer(to)?limit.sext(128):limit.zext(128));
        };
        auto* wide=numeric::signed_integer(from)?b.CreateSExt(value,wide_type):b.CreateZExt(value,wide_type);
        auto* low=extend(numeric::limit(to,false));
        auto* high=extend(numeric::limit(to,true));
        auto* valid=b.CreateAnd(b.CreateICmpSGE(wide,low),b.CreateICmpSLE(wide,high));
        return std::pair{wide,valid};
    };
    auto enum_membership=[&](ir::Type enumeration,llvm::Value* value,bool wide) {
        if(enumeration.definition->enum_flags)return static_cast<llvm::Value*>(b.getTrue());
        auto backing=ir::Type(*enumeration.definition->enum_backing);
        llvm::Value* valid=b.getFalse();
        for(const auto& member:enumeration.definition->numeric_members) {
            auto bits=llvm::APInt(numeric::width(backing),member.bits);
            if(wide)bits=numeric::signed_integer(backing)?bits.sext(128):bits.zext(128);
            valid=b.CreateOr(valid,b.CreateICmpEQ(value,llvm::ConstantInt::get(context,bits)));
        }
        return valid;
    };
    auto field_offset=[&](ir::Type owner,unsigned field) {
        std::uint64_t offset=0; auto* native=type(ir::Type(owner.definition));
        for(auto index:ir::physical_field_path(owner,field)) {
            auto* aggregate=llvm::cast<llvm::StructType>(native);
            offset+=layout.getStructLayout(aggregate)->getElementOffset(index); native=aggregate->getElementType(index);
        }
        return offset;
    };
    auto field_address=[&](ir::Type owner,llvm::Value* address,unsigned field) {
        std::vector<llvm::Value*> path{b.getInt32(0)};
        for(auto index:ir::physical_field_path(owner,field)) path.push_back(b.getInt32(index));
        return b.CreateGEP(type(ir::Type(owner.definition)),address,path);
    };
    std::function<void(ir::Type,std::uint64_t,std::vector<std::uint64_t>&)> reference_offsets;
    reference_offsets = [&](ir::Type t,std::uint64_t base,std::vector<std::uint64_t>& result) {
        // Scalar array payloads live in Runtime buffers; only their handles are
        // embedded in native values. Trace handles even though numeric element
        // shapes have no logical managed-reference leaves.
        if(ir::collection_type(t) || (t.kind==ir::Type::managed_ref && t.definition && t.definition->task_result)) {result.push_back(base);return;}
        if(t.kind==ir::Type::aggregate) {
            for(unsigned n=0;n<t.definition->fields.size();++n) reference_offsets(ir::field_type(t,n),base+field_offset(t,n),result);
            return;
        }
        for(const auto& path:ir::reference_paths(t,true)) {
            auto current=t;auto offset=base;
            for(auto field:path) {offset+=field_offset(current,field);current=ir::field_type(current,field);}
            result.push_back(offset);
            if(current.kind==ir::Type::prepared_receiver) result.push_back(offset+layout.getPointerSize());
        }
    };
        auto* size_type = b.getIntNTy(layout.getPointerSizeInBits());
        auto offset_map = [&](const std::vector<std::uint64_t>& offsets) -> llvm::Constant* {
            std::vector<llvm::Constant*> constants;
            for (auto offset : offsets) constants.push_back(llvm::ConstantInt::get(size_type, offset));
            auto* array_type = llvm::ArrayType::get(size_type, constants.size());
            return new llvm::GlobalVariable(*module, array_type, true, llvm::GlobalValue::PrivateLinkage,
                llvm::ConstantArray::get(array_type, constants), "gc.offsets");
        };
    auto element_key=[&](ir::Type t) {
        if(numeric::number(t) || t==ir::Type::boolean)return std::string("void.fixed.scalar.v1/")+std::to_string(scalar_encoding(t));
        auto key=std::string("void.array.element.v1/")+std::to_string(t.kind)+":"+ir::Type::key(t)+(t.nullable?"?":"");
        if(t.kind==ir::Type::numeric_enum)key+="|"+ir::numeric_enum_layout(t);
        return key;
    };
    auto list_key=[&](ir::Type element) {auto key=element_key(element);constexpr std::string_view scalar="void.fixed.scalar.v1/";return key.starts_with(scalar) ? "void.list.v1/"+key.substr(scalar.size()) : "void.list.v2/"+key;};
    auto* element_field_type=llvm::StructType::get(b.getPtrTy(),size_type,b.getInt64Ty(),b.getInt32Ty(),b.getInt32Ty());
    auto* element_record_type=llvm::StructType::get(b.getPtrTy(),size_type,size_type,size_type,b.getPtrTy());
    if(layout.getTypeAllocSize(element_field_type)!=sizeof(NativeArrayField) || layout.getStructLayout(element_field_type)->getElementOffset(3)!=offsetof(NativeArrayField,kind) || layout.getTypeAllocSize(element_record_type)!=sizeof(NativeArrayElement))throw std::invalid_argument("array element descriptor ABI mismatch");
    std::map<ir::Type,llvm::Constant*> element_records;
    auto element_record=[&](ir::Type root)->llvm::Constant* {
        if(auto found=element_records.find(root);found!=element_records.end())return found->second;
        std::vector<llvm::Constant*> fields;
        std::function<void(ir::Type,std::uint64_t,bool)> visit=[&](ir::Type t,std::uint64_t offset,bool inactive) {
            if(t.kind==ir::Type::aggregate) {for(unsigned n=0;n<t.definition->fields.size();++n)visit(ir::field_type(t,n),offset+field_offset(t,n),inactive || (!t.definition->cases.empty() && n!=0));return;}
            unsigned kind;std::string target;std::uint64_t extent=0;
            if(t.kind==ir::Type::numeric_enum)kind=scalar_encoding(*t.definition->enum_backing);
            else if(numeric::number(t) || t==ir::Type::boolean)kind=scalar_encoding(t);
            else if(t.kind==ir::Type::managed_ref){kind=11;target=ir::Type::key(t);}
            else if(t.kind==ir::Type::string)kind=12;
            else if(t.kind==ir::Type::interface_ref){kind=13;target=ir::Type::key(t);}
            else if(t.kind==ir::Type::fixed_array){kind=14;target=element_key(ir::array_element(t));extent=*t.definition->extent;}
            else if(t.kind==ir::Type::growable_array){kind=15;target=list_key(ir::array_element(t));}
            else if(t.kind==ir::Type::dictionary){kind=16;target="void.dict.v1/"+element_key(ir::array_element(t));}
            else throw std::invalid_argument("unsupported native array element");
            fields.push_back(llvm::ConstantStruct::get(element_field_type,{b.CreateGlobalString(target),llvm::ConstantInt::get(size_type,offset),b.getInt64(extent),b.getInt32(kind),b.getInt32(t.nullable || (inactive && (kind==11 || kind==13 || kind>=14)))}));
        };visit(root,0,false);
        auto* fields_type=llvm::ArrayType::get(element_field_type,fields.size());auto* data=new llvm::GlobalVariable(*module,fields_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantArray::get(fields_type,fields),"array.element.fields");
        auto* record=new llvm::GlobalVariable(*module,element_record_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantStruct::get(element_record_type,{b.CreateGlobalString(element_key(root)),llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(type(root))),llvm::ConstantInt::get(size_type,layout.getABITypeAlign(type(root)).value()),llvm::ConstantInt::get(size_type,fields.size()),data}),"array.element");
        element_records.emplace(root,record);return record;
    };
    auto* dictionary_record_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),size_type,size_type);
    if(layout.getTypeAllocSize(dictionary_record_type)!=sizeof(NativeDictionary) || layout.getStructLayout(dictionary_record_type)->getElementOffset(2)!=offsetof(NativeDictionary,value_offset))throw std::invalid_argument("dictionary descriptor ABI mismatch");
    std::map<ir::Type,llvm::Constant*> dictionary_records;
    auto dictionary_record=[&](ir::Type t)->llvm::Constant* {
        if(auto found=dictionary_records.find(t);found!=dictionary_records.end())return found->second;auto entry=ir::array_element(t);
        auto* record=new llvm::GlobalVariable(*module,dictionary_record_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantStruct::get(dictionary_record_type,{element_record(entry),element_record(ir::dictionary_key(t)),llvm::ConstantInt::get(size_type,field_offset(entry,1)),llvm::ConstantInt::get(size_type,field_offset(entry,2))}),"dictionary.layout");dictionary_records.emplace(t,record);return record;
    };
    const auto elision_helpers=allocation_elision_helpers(input,checked_types,verified_effects);
    const auto heap_factories=allocation_heap_factories(input,checked_types,verified_effects);
    std::vector<BlockAllocationElision> elisions;
    std::set<std::string> borrowed_targets;
    for(unsigned n=0;n<input.functions.size();++n) {
        const auto& f=input.functions[n];
        elisions.push_back(block_allocation_elision(f,checked_types[n],layout,type,elision_helpers,heap_factories));
        for(auto [block,id]:elisions.back().calls)borrowed_targets.insert(f.blocks[block].instructions[id].callee);
    }
    std::map<std::string,std::string> borrowed_names;
    const auto original_functions=input.functions.size();
    for(unsigned n=0;n<original_functions;++n) {
        const auto& original=input.functions[n];
        if(!borrowed_targets.contains(original.name))continue;
        const auto parameter=elision_helpers.at(original.name).parameter;
        auto borrowed=original;
        borrowed.name=ir::construction_name(original.name,{parameter});
        borrowed_names.emplace(original.name,borrowed.name);
        if(names.contains(borrowed.name))throw std::logic_error("allocation elision helper identity collision");
        borrowed.parameters[parameter].construction=true;
        for(auto slot:elision_helpers.at(original.name).aliases) {
            borrowed.locals[slot].construction=true;
            borrowed.construction_origins.emplace(slot,parameter);
        }
        if(auto instance=input.generic_instances.find(original.name);instance!=input.generic_instances.end())
            input.generic_instances.emplace(borrowed.name,instance->second);
        names.insert(borrowed.name);input.functions.push_back(std::move(borrowed));
    }
    if(input.functions.size()!=original_functions) {
        ir::validate(input,&checked_types,&verified_effects);
        for(unsigned n=original_functions;n<input.functions.size();++n)
            elisions.push_back({std::vector<std::set<unsigned>>(input.functions[n].blocks.size()),{}, {}, {}, {}, false});
    }
    auto* signature = llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty()}, false);
    std::set<std::string> source_entries{input.entry_symbol};
    for (const auto& entry : input.functions) if (entry.name == input.entry_symbol && entry.source_name == "$app_adapter")
        for (const auto& block : entry.blocks) for (const auto& instruction : block.instructions) if (instruction.op == ir::Op::call) source_entries.insert(instruction.callee);
    std::map<std::string, llvm::Function*> functions;
    for (const auto& name : input.imports)
        functions[name] = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, *module);
    for (const auto& f : input.functions) {
        std::vector<llvm::Type*> parameters;
        for (unsigned n = 0; n < f.parameters.size(); ++n) parameters.push_back(f.receiver && n == 0 ? b.getPtrTy() : type(f.parameters[n]));
        if (!f.module_identity.empty() || input.coordinated) parameters.push_back(b.getPtrTy());
        auto* function_type = llvm::FunctionType::get(f.asynchronous?b.getPtrTy():type(f.result), parameters, false);
        // Source-private bodies are called within this LLVM module, including
        // bodies from the flattened import graph. Keep the selected host entry
        // lookup-visible, and preserve public/project and legacy native exports.
        const auto linkage = default_functions.contains(f.name) || (!f.module_identity.empty() && f.visibility == "private" && f.name != exported_entry)
            ? llvm::Function::InternalLinkage : llvm::Function::ExternalLinkage;
        functions[f.name] = llvm::Function::Create(function_type, linkage, native_symbol(f, suffix)+(f.asynchronous?"$async":""), *module);
    }
    auto* async_generation=new llvm::GlobalVariable(*module,b.getInt8Ty(),true,llvm::GlobalValue::PrivateLinkage,b.getInt8(0),"async.generation");
    struct AsyncPlan {llvm::StructType* frame;llvm::Function* resume;std::vector<unsigned> parameters,locals;std::vector<std::vector<unsigned>> values;std::vector<std::uint64_t> offsets;};
    std::map<std::string,AsyncPlan> async_plans;
    for(unsigned index=0;index<input.functions.size();++index){const auto& f=input.functions[index];if(!f.asynchronous)continue;
        AsyncPlan plan;std::vector<llvm::Type*> fields{b.getPtrTy(),b.getInt32Ty()};std::vector<ir::Type> logical{f.task,ir::Type::i32};
        auto add=[&](ir::Type t){auto i=static_cast<unsigned>(fields.size());fields.push_back(t==ir::Type::none?b.getInt8Ty():type(t));logical.push_back(t);return i;};
        for(auto t:f.parameters)plan.parameters.push_back(add(t));for(auto t:f.locals)plan.locals.push_back(add(t));
        for(const auto& block:checked_types[index]){std::vector<unsigned> values;for(auto t:block)values.push_back(add(t));plan.values.push_back(std::move(values));}
        plan.frame=llvm::StructType::create(context,fields,"async.frame"+suffix);auto* native=layout.getStructLayout(plan.frame);
        for(unsigned n=0;n<logical.size();++n)if(logical[n]!=ir::Type::none)reference_offsets(logical[n],native->getElementOffset(n),plan.offsets);
        plan.resume=llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),{b.getPtrTy(),b.getPtrTy()},false),llvm::GlobalValue::InternalLinkage,"async.resume"+suffix,*module);
#ifdef VOID_ASYNC_START_SANITIZER_HASH
        auto metadata=[&](llvm::Function* function,std::int32_t magic,std::int32_t hash){function->setMetadata("func_sanitize",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(magic)),llvm::ConstantAsMetadata::get(b.getInt32(hash))}));};
        if(input.async_root && f.name==exported_entry)metadata(functions.at(f.name),VOID_ASYNC_START_SANITIZER_MAGIC,VOID_ASYNC_START_SANITIZER_HASH);
        metadata(plan.resume,VOID_ASYNC_RESUME_SANITIZER_MAGIC,VOID_ASYNC_RESUME_SANITIZER_HASH);
#endif
        async_plans.emplace(f.name,std::move(plan));
    }
    std::map<std::string,std::uint64_t> function_ids;
    for(const auto& [name,function]:functions)function_ids.emplace(name,function_ids.size()+1);
    std::map<std::string,llvm::Function*> callable_dispatchers;
    std::map<std::string,ir::Type> callable_types;
    std::vector<ir::Type> pending_callables;
    for(auto t:input.nominal_types)pending_callables.push_back(t);
    for(const auto& f:input.functions){pending_callables.push_back(f.result);pending_callables.insert(pending_callables.end(),f.parameters.begin(),f.parameters.end());pending_callables.insert(pending_callables.end(),f.locals.begin(),f.locals.end());}
    std::set<const ir::ValueType*> seen_callable_types;
    while(!pending_callables.empty()){
        auto t=pending_callables.back();pending_callables.pop_back();if(!t.definition || !seen_callable_types.insert(t.definition.get()).second)continue;
        if(t.definition->callable){callable_types.emplace(ir::Type::key(t),t);for(const auto& p:t.definition->call_parameters)pending_callables.push_back(p);pending_callables.push_back(*t.definition->call_result);}
        if(t.definition->element)pending_callables.push_back(*t.definition->element);for(const auto& field:t.definition->fields)pending_callables.push_back(field.type);
    }
    for(const auto& [key,t]:callable_types){
        std::vector<llvm::Type*> parameters{type(t)};for(const auto& p:t.definition->call_parameters)parameters.push_back(type(p));parameters.push_back(b.getPtrTy());auto result=ir::Type(*t.definition->call_result);
        auto* dispatcher=llvm::Function::Create(llvm::FunctionType::get(type(result),parameters,false),llvm::GlobalValue::InternalLinkage,"callable_dispatch",*module);callable_dispatchers.emplace(key,dispatcher);
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"select_callable",dispatcher));auto* invalid=llvm::BasicBlock::Create(context,"invalid_callable",dispatcher);auto* selected=b.CreateSwitch(b.CreateExtractValue(dispatcher->getArg(0),0),invalid);
        for(const auto& target:input.functions){
            if(target.asynchronous || target.receiver || target.initializer || target.parameters.size()!=t.definition->call_parameters.size()+(target.closure?1:0) || target.result!=result || !ir::capability_assignable(target.result,result))continue;
            bool same=true;for(unsigned n=0;n<t.definition->call_parameters.size();++n)same&=target.parameters[n+(target.closure?1:0)]==ir::Type(t.definition->call_parameters[n]) && ir::capability_key(target.parameters[n+(target.closure?1:0)])==ir::capability_key(t.definition->call_parameters[n]);if(!same)continue;
            auto* invoke=llvm::BasicBlock::Create(context,"invoke_callable",dispatcher);selected->addCase(b.getInt64(function_ids.at(target.name)),invoke);b.SetInsertPoint(invoke);
            std::vector<llvm::Value*> arguments;if(target.closure)arguments.push_back(b.CreateExtractValue(dispatcher->getArg(0),1));for(unsigned n=1;n<dispatcher->arg_size();++n)arguments.push_back(dispatcher->getArg(n));auto* value=b.CreateCall(functions.at(target.name),arguments);if(result==ir::Type::none)b.CreateRetVoid();else b.CreateRet(value);
        }
        b.SetInsertPoint(invalid);b.CreateStore(b.CreateGlobalString("Null or incompatible function value"),dispatcher->getArg(dispatcher->arg_size()-1));if(result==ir::Type::none)b.CreateRetVoid();else b.CreateRet(llvm::Constant::getNullValue(type(result)));
    }
    std::map<std::string,llvm::Function*> callable_comparisons;
    for(const auto& [key,t]:callable_types){
        auto* compare=llvm::Function::Create(llvm::FunctionType::get(b.getInt1Ty(),{type(t),type(t)},false),llvm::GlobalValue::InternalLinkage,"callable_identity",*module);callable_comparisons.emplace(key,compare);
        auto* entry=llvm::BasicBlock::Create(context,"compare",compare);auto* choose=llvm::BasicBlock::Create(context,"same_function",compare);auto* different=llvm::BasicBlock::Create(context,"different",compare);auto* ordinary=llvm::BasicBlock::Create(context,"closure_identity",compare);b.SetInsertPoint(entry);auto* left=compare->getArg(0);auto* right=compare->getArg(1);b.CreateCondBr(b.CreateICmpEQ(b.CreateExtractValue(left,0),b.CreateExtractValue(right,0)),choose,different);b.SetInsertPoint(different);b.CreateRet(b.getFalse());b.SetInsertPoint(choose);auto* selected=b.CreateSwitch(b.CreateExtractValue(left,0),ordinary);
        for(const auto& target:input.functions)if(target.bound_receiver){
            if(target.parameters.size()!=t.definition->call_parameters.size()+1 || target.result!=ir::Type(*t.definition->call_result))continue;bool same=true;for(unsigned n=0;n<t.definition->call_parameters.size();++n)same&=target.parameters[n+1]==ir::Type(t.definition->call_parameters[n]);if(!same)continue;
            auto* bound=llvm::BasicBlock::Create(context,"bound_receiver",compare);selected->addCase(b.getInt64(function_ids.at(target.name)),bound);b.SetInsertPoint(bound);auto environment=target.parameters[0];auto receiver=ir::field_type(environment,0);auto* a=b.CreateLoad(type(receiver),field_address(environment,b.CreateExtractValue(left,1),0));auto* c=b.CreateLoad(type(receiver),field_address(environment,b.CreateExtractValue(right,1),0));llvm::Value* equal;
            if(receiver.kind==ir::Type::interface_ref)equal=b.CreateAnd(b.CreateICmpEQ(b.CreateExtractValue(a,0),b.CreateExtractValue(c,0)),b.CreateICmpEQ(b.CreateExtractValue(a,1),b.CreateExtractValue(c,1)));else equal=b.CreateICmpEQ(a,c);b.CreateRet(equal);
        }
        b.SetInsertPoint(ordinary);b.CreateRet(b.CreateICmpEQ(b.CreateExtractValue(left,1),b.CreateExtractValue(right,1)));
    }
    std::map<std::string,ir::Type> nominal_types;
    for(auto t:input.nominal_types) if(t.kind==ir::Type::managed_ref || t.kind==ir::Type::interface_ref || t.kind==ir::Type::aggregate) nominal_types.emplace(t.definition->identity,t);
    auto* record_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getPtrTy(),b.getInt32Ty());
    const auto* record_layout=layout.getStructLayout(record_type);
    if(record_layout->getSizeInBytes()!=sizeof(RuntimeTypeRecord) || record_layout->getElementOffset(3)!=offsetof(RuntimeTypeRecord,size) || record_layout->getElementOffset(5)!=offsetof(RuntimeTypeRecord,count) || record_layout->getElementOffset(6)!=offsetof(RuntimeTypeRecord,dictionary_key_schema) || record_layout->getElementOffset(7)!=offsetof(RuntimeTypeRecord,dictionary_key_forbidden))
        throw std::invalid_argument("runtime nominal descriptor ABI mismatch");
    auto string=[&](const std::string& text) {return b.CreateGlobalString(text,"",0,module.get());};
    std::vector<llvm::Constant*> records,keys;
    for(const auto& [key,t]:nominal_types) {
        auto payload=t.kind==ir::Type::interface_ref ? t : ir::Type(t.definition);
        std::vector<std::uint64_t> offsets;reference_offsets(payload,0,offsets);
        auto bytes=std::max<std::uint64_t>(1,layout.getTypeAllocSize(type(payload)));
        auto base=t.definition->base.lock();
        std::string shape=(t.kind==ir::Type::interface_ref ? "interface:" : "")+std::to_string(bytes)+":"+std::to_string(layout.getABITypeAlign(type(payload)).value());
        for(unsigned n=0;n<t.definition->fields.size();++n) {
            const auto& field=t.definition->fields[n];
            for(const auto& part:{field.name,field.owner,ir::type_name(field.type),std::to_string(field_offset(t,n))}) shape+="|"+std::to_string(part.size())+":"+part;
        }
        const auto key_schema=element_key(t);const bool key_forbidden=ir::dictionary_equality_owner(t).has_value();
        if(host_catalog) { NominalDescriptor d{key,base ? base->identity : "",shape,{bytes,{}}};d.dictionary_key_schema=key_schema;d.dictionary_key_forbidden=key_forbidden;d.layout.references.assign(offsets.begin(),offsets.end());host_catalog->push_back(std::move(d)); }
        auto* identity=string(key); keys.push_back(identity);
        records.push_back(llvm::ConstantStruct::get(record_type,{identity,string(base ? base->identity : ""),string(shape),llvm::ConstantInt::get(size_type,bytes),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size()),string(key_schema),b.getInt32(key_forbidden)}));
    }
    auto* catalog_type=llvm::ArrayType::get(record_type,records.size());
    new llvm::GlobalVariable(*module,catalog_type,true,llvm::GlobalValue::ExternalLinkage,llvm::ConstantArray::get(catalog_type,records),"__vd_types"+suffix);
    new llvm::GlobalVariable(*module,size_type,true,llvm::GlobalValue::ExternalLinkage,llvm::ConstantInt::get(size_type,records.size()),"__vd_type_count"+suffix);
    auto* keys_type=llvm::ArrayType::get(b.getPtrTy(),keys.size());
    auto* dynamic_keys=new llvm::GlobalVariable(*module,keys_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantArray::get(keys_type,keys),"dynamic_type_keys");
    std::map<std::string,llvm::Function*> dispatchers;
    for(const auto& root:input.functions) if(!root.virtual_slot.empty() && root.virtual_slot==root.name) {
        auto* dispatcher=llvm::Function::Create(functions.at(root.name)->getFunctionType(),llvm::GlobalValue::InternalLinkage,"virtual_"+native_symbol(root,suffix),*module);
        dispatchers.emplace(root.virtual_slot,dispatcher);
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"select_type",dispatcher));
        auto* selected=b.CreateCall(module->getOrInsertFunction("vd_gc_dynamic_type",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),size_type),{dispatcher->getArg(0),dynamic_keys,llvm::ConstantInt::get(size_type,keys.size())});
        auto* invalid=llvm::BasicBlock::Create(context,"invalid_receiver",dispatcher);
        auto* cases=b.CreateSwitch(selected,invalid,static_cast<unsigned>(keys.size()));
        std::map<std::string,std::string> targets;
        for(const auto& target:ir::virtual_targets(input,root)) targets.emplace(ir::Type::key(target.dynamic_type),target.implementation);
        unsigned index=0;
        for(const auto& [key,t]:nominal_types) {
            auto case_index=index++;
            auto target=targets.find(key);if(target==targets.end()) continue;
            auto* invoke=llvm::BasicBlock::Create(context,"invoke_override",dispatcher);cases->addCase(b.getInt64(case_index),invoke);b.SetInsertPoint(invoke);
            std::vector<llvm::Value*> args;for(auto& arg:dispatcher->args()) args.push_back(&arg);
            auto* result=b.CreateCall(functions.at(target->second),args);
            if(root.result==ir::Type::none) b.CreateRetVoid();else b.CreateRet(result);
        }
        b.SetInsertPoint(invalid);
        b.CreateStore(b.CreateGlobalString("Invalid managed virtual receiver or generation type catalog"),dispatcher->getArg(dispatcher->arg_size()-1));
        if(root.result==ir::Type::none) b.CreateRetVoid();else b.CreateRet(llvm::Constant::getNullValue(type(root.result)));
    }
    // Canonical data-only contracts use independently inferred effects, including
    // the complete eligible family. Body-only edits retain old witness addresses.
    auto encoded=[](std::string_view value){return std::to_string(value.size())+":"+std::string(value);};
    auto effect_contract=[&](const ir::ReferenceEffects& effects) {
        std::string text;auto origin=[&](const ir::ReferenceOrigin& value) {std::string out=std::to_string(value.parameter)+":";for(auto n:value.path) out+=std::to_string(n)+",";return out+";";};
        for(auto [set,tag]:{std::pair{&effects.mutations,'M'},std::pair{&effects.retained,'H'},std::pair{&effects.required_mutable,'Q'}}) for(const auto& o:*set) text+=tag+origin(o);
        for(const auto& [path,fact]:effects.returned) {text+="R[";for(auto n:path) text+=std::to_string(n)+",";text+=fact.readonly ? "]C" : "]V";for(const auto& o:fact.origins) text+=origin(o);}
        return text;
    };
    std::map<std::string,std::string> interface_contracts;
    const auto source_contract=input.analysis ? input.analysis->compatibility_contract() : input.constant_contract;
    std::string access_contract=encoded(source_contract)+encoded(input.root_identity);
    for(const auto& [scope,imports]:input.source_imports) {access_contract+=encoded(scope);for(const auto& imported:imports) access_contract+=encoded(imported);}
    for(auto t:input.nominal_types) {
        access_contract+=encoded(ir::type_name(t))+encoded(std::to_string(t.kind))+encoded(t.definition->module_identity)+encoded(t.definition->visibility);auto base=t.definition->base.lock();access_contract+=encoded(base ? base->identity : "");
        if(t.kind==ir::Type::numeric_enum)access_contract+=encoded(ir::numeric_enum_layout(t));
        for(const auto& variant:t.definition->cases)access_contract+=encoded(variant.name)+encoded(std::to_string(variant.begin))+encoded(std::to_string(variant.count));
        for(const auto& field:t.definition->fields) access_contract+=encoded(field.name)+encoded(field.owner)+encoded(ir::type_name(field.type))+(field.using_field ? "U;" : "F;");
    }
    for(const auto& f:input.functions) if(!default_functions.contains(f.name)){access_contract+=encoded(f.name)+encoded(f.visibility)+encoded(f.virtual_slot);for(auto t:f.parameters) access_contract+=encoded(ir::type_name(t))+encoded(ir::capability_key(t));access_contract+=encoded(ir::type_name(f.result))+encoded(ir::capability_key(f.result));}
    for(const auto& r:input.interface_requirements) {
        auto& text=interface_contracts[ir::Type::key(r.parameters[0])];text+=encoded(r.name)+encoded(ir::type_name(r.result))+encoded(ir::capability_key(r.result));
        for(auto t:r.parameters) text+=encoded(ir::type_name(t))+encoded(ir::capability_key(t));
        text+=encoded(effect_contract(verified_effects.at(r.name)));
    }
    std::vector<const ir::Module::Binding*> bindings;
    for(const auto& binding:input.interface_bindings) bindings.push_back(&binding);
    std::sort(bindings.begin(),bindings.end(),[](auto* a,auto* c){return a->key<c->key;});
    for(auto* binding:bindings) interface_contracts[ir::Type::key(binding->interface)]+=encoded(binding->key);
    auto* witness_record_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy());
    if(layout.getTypeAllocSize(witness_record_type)!=sizeof(RuntimeWitnessRecord)) throw std::invalid_argument("runtime witness record ABI mismatch");
    std::vector<llvm::Constant*> witness_records,witness_keys;
    for(auto* binding:bindings) {
        InterfaceWitness data{binding->key,ir::Type::key(binding->interface),ir::Type::key(binding->view),interface_contracts.at(ir::Type::key(binding->interface))+access_contract};
        if(host_witnesses) host_witnesses->push_back(data);
        auto* key=string(data.key);witness_keys.push_back(key);
        witness_records.push_back(llvm::ConstantStruct::get(witness_record_type,{key,string(data.interface_identity),string(data.view_identity),string(data.contract)}));
    }
    auto* witness_catalog_type=llvm::ArrayType::get(witness_record_type,witness_records.size());
    new llvm::GlobalVariable(*module,witness_catalog_type,true,llvm::GlobalValue::ExternalLinkage,llvm::ConstantArray::get(witness_catalog_type,witness_records),"__vd_witnesses"+suffix);
    new llvm::GlobalVariable(*module,size_type,true,llvm::GlobalValue::ExternalLinkage,llvm::ConstantInt::get(size_type,witness_records.size()),"__vd_witness_count"+suffix);
    auto* witness_keys_type=llvm::ArrayType::get(b.getPtrTy(),witness_keys.size());
    auto* binding_keys=new llvm::GlobalVariable(*module,witness_keys_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantArray::get(witness_keys_type,witness_keys),"interface_binding_keys");
    auto binding_index=[&](llvm::Value* view) {return b.CreateCall(module->getOrInsertFunction("vd_gc_interface_binding",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),size_type),{b.CreateExtractValue(view,0),b.CreateExtractValue(view,1),binding_keys,llvm::ConstantInt::get(size_type,bindings.size())});};
    std::map<std::string,llvm::Function*> preparations,interface_dispatchers;
    for(const auto& requirement:input.interface_requirements) {
        auto* prepare=llvm::Function::Create(llvm::FunctionType::get(prepared_type,{interface_type,b.getPtrTy()},false),llvm::GlobalValue::InternalLinkage,"prepare_"+native_symbol(requirement,suffix),*module);
        preparations.emplace(requirement.name,prepare);
        std::vector<llvm::Type*> parameters{prepared_type};for(unsigned n=1;n<requirement.parameters.size();++n) parameters.push_back(type(requirement.parameters[n]));parameters.push_back(b.getPtrTy());
        auto* invoke=llvm::Function::Create(llvm::FunctionType::get(type(requirement.result),parameters,false),llvm::GlobalValue::InternalLinkage,"interface_"+native_symbol(requirement,suffix),*module);
        interface_dispatchers.emplace(requirement.name,invoke);
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"binding",prepare));
        auto* invalid=llvm::BasicBlock::Create(context,"invalid_binding",prepare);auto* selected=b.CreateSwitch(binding_index(prepare->getArg(0)),invalid,static_cast<unsigned>(bindings.size()));
        b.SetInsertPoint(llvm::BasicBlock::Create(context,"prepared",invoke));
        auto* bad_call=llvm::BasicBlock::Create(context,"invalid_prepared_receiver",invoke);auto* called=b.CreateSwitch(b.CreateExtractValue(invoke->getArg(0),3),bad_call,static_cast<unsigned>(bindings.size()));
        for(unsigned n=0;n<bindings.size();++n) {
            auto method=bindings[n]->methods.find(requirement.name);if(method==bindings[n]->methods.end()) continue;
            const auto& [path,name]=method->second;
            auto target=std::find_if(input.functions.begin(),input.functions.end(),[&](const auto& f){return f.name==name;});
            auto* project=llvm::BasicBlock::Create(context,"select_receiver",prepare);selected->addCase(b.getInt64(n),project);b.SetInsertPoint(project);
            auto* owner=b.CreateExtractValue(prepare->getArg(0),0);llvm::Value* address=owner;auto current=bindings[n]->view;
            for(auto field:path) {
                address=field_address(current,address,field);current=ir::field_type(current,field);
                if(current.kind==ir::Type::managed_ref) address=b.CreateLoad(b.getPtrTy(),address);
            }
            auto* zero=llvm::ConstantPointerNull::get(b.getPtrTy());llvm::Value* token=llvm::Constant::getNullValue(prepared_type);
            token=b.CreateInsertValue(token,owner,0);token=b.CreateInsertValue(token,current.kind==ir::Type::managed_ref ? address : zero,1);token=b.CreateInsertValue(token,current.kind==ir::Type::managed_ref ? zero : address,2);token=b.CreateInsertValue(token,b.getInt64(n),3);b.CreateRet(token);
            auto* call=llvm::BasicBlock::Create(context,"invoke_binding",invoke);called->addCase(b.getInt64(n),call);b.SetInsertPoint(call);
            std::vector<llvm::Value*> arguments{b.CreateExtractValue(invoke->getArg(0),target->parameters[0].kind==ir::Type::managed_ref ? 1 : 2)};
            for(unsigned k=1;k<invoke->arg_size();++k) arguments.push_back(invoke->getArg(k));
            auto* callee=target->virtual_slot.empty() ? functions.at(name) : dispatchers.at(target->virtual_slot);
            auto* result=b.CreateCall(callee,arguments);if(requirement.result==ir::Type::none) b.CreateRetVoid();else b.CreateRet(result);
        }
        b.SetInsertPoint(invalid);b.CreateStore(string("Invalid interface binding, domain or generation catalog"),prepare->getArg(1));b.CreateRet(llvm::Constant::getNullValue(prepared_type));
        b.SetInsertPoint(bad_call);b.CreateStore(string("Invalid prepared interface receiver"),invoke->getArg(invoke->arg_size()-1));if(requirement.result==ir::Type::none) b.CreateRetVoid();else b.CreateRet(llvm::Constant::getNullValue(type(requirement.result)));
    }
    // An absent profile keeps the historical JIT, standalone and proof metadata.
    std::unique_ptr<llvm::DIBuilder> debug;
    llvm::DIFile* file=nullptr;
    const bool debug_types_enabled=source_debug!=project::DebugInfo::line;
    auto debug_file=[&](const std::string& name) {
        const auto path=vd::utf8_path(name);
        return debug->createFile(vd::path_utf8(path.filename()),vd::path_utf8(path.parent_path()));
    };
    if(source_debug!=project::DebugInfo::none) {
        debug=std::make_unique<llvm::DIBuilder>(*module);
        file=debug_file(input.source_path.empty()?input.identity:input.source_path);
        debug->createCompileUnit(llvm::dwarf::DW_LANG_C,file,"VOID source compiler",source_debug && optimized,"",0,"",
            debug_types_enabled?llvm::DICompileUnit::FullDebug:llvm::DICompileUnit::LineTablesOnly);
        module->addModuleFlag(llvm::Module::Warning,"Debug Info Version",llvm::DEBUG_METADATA_VERSION);
        if(source_debug)module->addModuleFlag(llvm::Module::Warning,"Dwarf Version",4);
    }
    std::map<std::string,llvm::DIType*> debug_aggregates;
    std::function<llvm::DIType*(ir::Type)> debug_value_type = [&](ir::Type t) -> llvm::DIType* {
        if (t.kind == ir::Type::managed_ref || t.kind==ir::Type::string) return debug->createPointerType(nullptr, layout.getPointerSizeInBits());
        if(t.kind==ir::Type::interface_ref || t.kind==ir::Type::prepared_receiver) {
            auto* native=llvm::cast<llvm::StructType>(type(t));auto* offsets=layout.getStructLayout(native);std::vector<llvm::Metadata*> members;
            for(unsigned n=0;n<native->getNumElements();++n) {auto* part=native->getElementType(n);llvm::DIType* debug_part=n< (t.kind==ir::Type::interface_ref ? 2u : 3u) ? static_cast<llvm::DIType*>(debug->createPointerType(nullptr,layout.getPointerSizeInBits())) : debug->createBasicType("binding selector",64,llvm::dwarf::DW_ATE_unsigned);
                members.push_back(debug->createMemberType(file,std::to_string(n),file,0,layout.getTypeSizeInBits(part),layout.getABITypeAlign(part).value()*8,offsets->getElementOffsetInBits(n),llvm::DINode::FlagZero,debug_part));}
            return debug->createStructType(file,ir::type_name(t),file,0,offsets->getSizeInBits(),layout.getABITypeAlign(native).value()*8,llvm::DINode::FlagZero,nullptr,debug->getOrCreateArray(members));
        }
        if (ir::collection_type(t)) return debug->createPointerType(nullptr,layout.getPointerSizeInBits(),0,{},ir::type_name(t));
        if (t == ir::Type::none) return nullptr;
        if (t.kind == ir::Type::aggregate) {
            if (auto found = debug_aggregates.find(t.definition->identity); found != debug_aggregates.end()) return found->second;
            auto* native = llvm::cast<llvm::StructType>(type(t)); auto* offsets = layout.getStructLayout(native);
            std::vector<llvm::Metadata*> members;
            for (unsigned n = 0; n < t.definition->fields.size(); ++n) {
                const auto& field = t.definition->fields[n]; auto* native_field = type(field.type);
                members.push_back(debug->createMemberType(file,field.name,file,0,layout.getTypeSizeInBits(native_field),
                    layout.getABITypeAlign(native_field).value()*8,field_offset(t,n)*8,llvm::DINode::FlagZero,debug_value_type(field.type)));
            }
            auto* result = debug->createStructType(file,ir::type_name(t),file,0,offsets->getSizeInBits(),
                layout.getABITypeAlign(native).value()*8,llvm::DINode::FlagZero,nullptr,debug->getOrCreateArray(members));
            debug_aggregates.emplace(t.definition->identity,result); return result;
        }
        if(t.kind==ir::Type::numeric_enum) {
            auto backing=ir::Type(*t.definition->enum_backing);
            return debug->createBasicType(ir::type_name(t),numeric::width(backing),
                numeric::signed_integer(backing)?llvm::dwarf::DW_ATE_signed:llvm::dwarf::DW_ATE_unsigned);
        }
        return debug->createBasicType(ir::type_name(t),numeric::width(t), t == ir::Type::boolean ? llvm::dwarf::DW_ATE_boolean : numeric::floating(t) ? llvm::dwarf::DW_ATE_float : numeric::signed_integer(t) ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_unsigned);
    };
    std::size_t function_index = 0;
    for (const auto& f : input.functions) {
        auto& elision=elisions.at(function_index);
        const auto& instruction_types = checked_types.at(function_index++);
        const auto& elided_values=elision.values;
        const auto& elided_locals=elision.locals;
        const auto* async=f.asynchronous?&async_plans.at(f.name):nullptr;
        auto* fn = async?async->resume:functions.at(f.name);
        llvm::BasicBlock* setup=async?llvm::BasicBlock::Create(context,"async.enter",fn):nullptr;
        auto frame_home=[&](unsigned field){return b.CreateStructGEP(async->frame,fn->getArg(0),field);};
        llvm::DIFile* source_file=nullptr;
        llvm::DISubprogram* subprogram=nullptr;
        std::map<unsigned,const DebugBinding*> debug_bindings;
        std::map<std::string,llvm::DILocalScope*> file_scopes;
        if(debug) {
            source_file=debug_file(f.source.path);
            std::vector<llvm::Metadata*> debug_types;
            if(debug_types_enabled) {
                debug_types.push_back(debug_value_type(f.result));
                for(unsigned n=0;n<f.parameters.size();++n) {
                    auto* logical=debug_value_type(f.parameters[n]);
                    if(f.receiver && n==0)logical=debug->createReferenceType(llvm::dwarf::DW_TAG_reference_type,logical,layout.getPointerSizeInBits(),layout.getPointerABIAlignment(0).value()*8);
                    debug_types.push_back(logical);
                }
            }
            auto flags=llvm::DISubprogram::SPFlagDefinition;
            if(fn->hasLocalLinkage())flags|=llvm::DISubprogram::SPFlagLocalToUnit;
            if(source_debug && optimized)flags|=llvm::DISubprogram::SPFlagOptimized;
            auto* debug_type=debug->createSubroutineType(debug->getOrCreateTypeArray(debug_types));
            subprogram=debug->createFunction(source_file,!f.display_name.empty()?f.display_name:f.source_name.empty()?f.name:f.source_name,fn->getName(),
                source_file,f.source.line,debug_type,f.source.line,llvm::DINode::FlagPrototyped,flags);
            fn->setSubprogram(subprogram);
            if(source_debug==project::DebugInfo::full && !optimized && input.analysis) {
                const auto& names=input.analysis->debug_bindings();
                if(auto found=names.find(f.name);found!=names.end())
                    for(const auto& binding:found->second)debug_bindings.emplace(binding.slot,&binding);
            }
        }
        std::vector<llvm::BasicBlock*> blocks;
        for (std::size_t i = 0; i < f.blocks.size(); ++i)
            blocks.push_back(llvm::BasicBlock::Create(context, "block" + std::to_string(i), fn));
        auto* failure_type = llvm::StructType::get(b.getPtrTy(), b.getInt32Ty(), llvm::ArrayType::get(b.getPtrTy(), 32));
        llvm::Value* failure = async?fn->getArg(1):f.module_identity.empty() && !input.coordinated ? nullptr : fn->getArg(static_cast<unsigned>(f.parameters.size()));
        llvm::Value* root_frame = nullptr;
        const bool deinit_body=f.receiver && (f.method_name=="deinit" || f.method_name.starts_with("deinit::$body$"));
        const bool collection_body=std::any_of(f.blocks.begin(),f.blocks.end(),[](const auto& block){return std::any_of(block.instructions.begin(),block.instructions.end(),[](const auto& i){return i.op==ir::Op::collection_finish || i.op==ir::Op::collection_append || i.op==ir::Op::collection_enter || i.op==ir::Op::collection_leave;});});
        const bool display_body=f.receiver && (f.method_name=="string" || f.method_name=="format");
        auto leave_frame = [&] {
            if(collection_body)b.CreateCall(module->getOrInsertFunction("vd_collection_scope_leave",b.getVoidTy()));
            if(display_body)b.CreateCall(module->getOrInsertFunction("vd_display_leave",b.getVoidTy()));
            if(deinit_body)b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_leave",b.getVoidTy()));
            if (root_frame) b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave", b.getVoidTy(), b.getPtrTy()), {root_frame});
        };
        unsigned active_block=0;
        auto return_failure = [&] {
            if(f.cleanup_entry && active_block<*f.cleanup_entry){b.CreateBr(blocks[*f.cleanup_entry]);return;}
            leave_frame();
            if (async || f.result == ir::Type::none) b.CreateRetVoid();
            else b.CreateRet(llvm::Constant::getNullValue(type(f.result)));
        };
        std::vector<llvm::Value*> slots;
        b.SetInsertPoint(async?setup:blocks.front());
        b.SetCurrentDebugLocation(llvm::DebugLoc());
        // Pointer-shaped locals share one lifetime. Keep compound values in
        // their own precise descriptors, preserving nested-layout root proofs,
        // and scalar locals separate for ordinary promotion.
        std::vector<unsigned> local_root_indexes(f.locals.size(),~0u);
        std::vector<llvm::Type*> local_root_types;
        std::vector<std::vector<std::uint64_t>> local_root_offsets;
        if(!async)for(unsigned n=0;n<f.locals.size();++n) {
            if(f.receiver && n==0 && f.locals[n].kind==ir::Type::aggregate)continue;
            std::vector<std::uint64_t> offsets;
            if(!elided_locals.contains(n))reference_offsets(f.locals[n],0,offsets);
            if(!offsets.empty() && type(f.locals[n])->isPointerTy()) {
                local_root_indexes[n]=local_root_types.size();local_root_types.push_back(type(f.locals[n]));
                local_root_offsets.push_back(std::move(offsets));
            }
        }
        // Payload child words use the existing local Slot. Proven block-local
        // lifetimes clear these words after final use; other owners retain the
        // accepted function-duration lifetime.
        // Pack their exact reference fields into the existing local Slot, rather
        // than adding a Slot to every remaining allocation's authority lookup.
        std::map<std::pair<unsigned,unsigned>,unsigned> payload_root_indexes;
        for(unsigned bn=0;bn<f.blocks.size();++bn)for(auto id:elided_values[bn]) {
            if(f.blocks[bn].instructions[id].op!=ir::Op::gc_construct)continue;
            const auto object=ir::Type(instruction_types[bn][id].definition);
            std::vector<std::uint64_t> offsets;reference_offsets(object,0,offsets);
            if(offsets.empty())continue;
            payload_root_indexes.emplace(std::pair{bn,id},local_root_types.size());
            local_root_types.push_back(type(object));local_root_offsets.push_back(std::move(offsets));
        }
        auto* local_roots_type=llvm::StructType::get(context,local_root_types);
        llvm::Value* local_roots=nullptr;
        if(!local_root_types.empty()) {
            local_roots=home(b.CreateAlloca(local_roots_type),StorageRole::local,llvm_backend::HomeKind::value);
            b.CreateMemSet(local_roots,b.getInt8(0),layout.getTypeAllocSize(local_roots_type),layout.getABITypeAlign(local_roots_type));
        }
        for (unsigned n = 0; n < f.locals.size(); ++n) {
            auto t = f.locals[n];
            if (f.receiver && n == 0 && t.kind==ir::Type::aggregate) { slots.push_back(fn->getArg(0)); continue; }
            const bool packed=local_root_indexes[n]!=~0u;
            llvm::Value* slot = async?frame_home(async->locals[n]):packed?b.CreateStructGEP(local_roots_type,local_roots,local_root_indexes[n]):home(b.CreateAlloca(type(t)),StorageRole::local,llvm_backend::HomeKind::value); slots.push_back(slot);
            if(!async && (!packed || (f.receiver && n==0)))b.CreateStore(f.receiver && n==0 && t.kind==ir::Type::managed_ref ? static_cast<llvm::Value*>(fn->getArg(0)) : llvm::Constant::getNullValue(type(t)), slot);
        }
        std::map<unsigned,llvm::Value*> stage_storage;
        for (const auto& block:f.blocks) for (const auto& instruction:block.instructions) if (instruction.op==ir::Op::stage_begin) {
            auto* native=type(ir::Type(instruction.type.definition));
            auto bytes=std::max<std::uint64_t>(1,layout.getTypeAllocSize(native));
            auto* storage=home(b.CreateAlloca(b.getInt8Ty(),b.getInt64(bytes)),StorageRole::staging,llvm_backend::HomeKind::staging);
            storage->setAlignment(layout.getABITypeAlign(native));
            b.CreateMemSet(storage,b.getInt8(0),bytes,layout.getABITypeAlign(native));
            stage_storage.emplace(instruction.a,storage);
        }
        std::map<std::pair<unsigned,unsigned>,llvm::Value*> elided_payloads;
        for(const auto& [position,index]:payload_root_indexes)
            elided_payloads.emplace(position,b.CreateStructGEP(local_roots_type,local_roots,index));
        std::map<ir::Type,llvm::Value*> copy_staging;
        auto plan_copy=[&](ir::Type t) {
            if(!contains_arrays(t) || copy_staging.contains(t)) return;
            auto* storage=home(b.CreateAlloca(type(t)),StorageRole::copy,llvm_backend::HomeKind::copy);
            b.CreateStore(llvm::Constant::getNullValue(type(t)),storage);copy_staging.emplace(t,storage);
        };
        for(auto t:f.locals) plan_copy(t);
        plan_copy(f.result);
        for(const auto& block:instruction_types) for(auto t:block) plan_copy(t);
        std::map<ir::Type,llvm::Value*> element_staging,copy_cursors,array_sources;
        auto plan_element=[&](ir::Type child){if(!element_staging.contains(child)){auto* storage=home(b.CreateAlloca(type(child)),StorageRole::copy,llvm_backend::HomeKind::copy);b.CreateStore(llvm::Constant::getNullValue(type(child)),storage);element_staging.emplace(child,storage);}};
        std::set<ir::Type> planned;
        std::function<void(ir::Type)> plan_elements=[&](ir::Type t) {
            if(ir::scalar_array(t))return;
            if(!planned.insert(t).second)return;
            if(t.kind==ir::Type::dictionary){auto entry=ir::array_element(t);plan_element(entry);plan_element(ir::dictionary_key(t));plan_element(ir::dictionary_value(t));plan_elements(entry);}
            else if(ir::array_type(t)) {
                auto child=ir::array_element(t);
                plan_element(child);
                auto* source=home(b.CreateAlloca(type(t)),StorageRole::copy,llvm_backend::HomeKind::copy);b.CreateStore(llvm::Constant::getNullValue(type(t)),source);array_sources.emplace(t,source);
                auto* cursor=home(b.CreateAlloca(b.getInt64Ty()),StorageRole::copy,llvm_backend::HomeKind::copy);copy_cursors.emplace(t,cursor);
                if(!copy_staging.contains(t)){auto* copy=home(b.CreateAlloca(type(t)),StorageRole::copy,llvm_backend::HomeKind::copy);b.CreateStore(llvm::Constant::getNullValue(type(t)),copy);copy_staging.emplace(t,copy);}
                plan_elements(child);
            } else if(t.kind==ir::Type::aggregate)for(const auto& field:t.definition->fields)plan_elements(field.type);
        };
        for(auto t:f.locals)plan_elements(t);plan_elements(f.result);for(const auto& block:instruction_types)for(auto t:block)plan_elements(t);
        for(const auto& block:f.blocks)for(const auto& i:block.instructions)if(i.op==ir::Op::array_allocate)plan_element(ir::array_element(i.type));
        for(unsigned bi=0;bi<f.blocks.size();++bi)for(const auto& i:f.blocks[bi].instructions)if(i.op==ir::Op::list_insert)plan_element(ir::array_element(instruction_types[bi][i.a]));
        std::function<void(ir::Type,llvm::Value*,llvm::Value*,const std::function<void()>&)> copy_value;
        std::function<void(ir::Type,llvm::Value*,llvm::Value*,const std::function<void()>&)> copy_children;
        copy_value=[&](ir::Type t,llvm::Value* destination,llvm::Value* source,const std::function<void()>& check_failure) {
            if(ir::scalar_fixed_array(t)) {
                b.CreateCall(module->getOrInsertFunction("vd_array_copy",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{destination,source,b.getInt32(scalar_encoding(ir::array_element(t))),b.getInt64(*t.definition->extent),failure});check_failure();return;
            }
            if(t.kind==ir::Type::fixed_array) {
                b.CreateStore(b.CreateLoad(type(t),source),array_sources.at(t));source=array_sources.at(t);
                auto child=ir::array_element(t);auto* record=element_record(child);auto* count=b.getInt64(*t.definition->extent);
                b.CreateCall(module->getOrInsertFunction("vd_value_array_copy",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{destination,source,record,count,failure});check_failure();
                copy_children(t,destination,count,check_failure);return;
            }
            b.CreateStore(b.CreateLoad(type(t),source),destination);
            if(t.kind==ir::Type::aggregate)for(unsigned field=0;field<t.definition->fields.size();++field) {
                auto child=ir::field_type(t,field);
                if(contains_arrays(child)) {
                    llvm::BasicBlock* done=nullptr;
                    if(!t.definition->cases.empty()) {
                        unsigned variant=0;for(;variant<t.definition->cases.size();++variant){const auto& c=t.definition->cases[variant];if(field>=c.begin && field<c.begin+c.count)break;}
                        auto* active=llvm::BasicBlock::Create(context,"copy_active_payload",fn);done=llvm::BasicBlock::Create(context,"copy_payload_done",fn);
                        b.CreateCondBr(b.CreateICmpEQ(b.CreateLoad(b.getInt32Ty(),field_address(t,source,0)),b.getInt32(variant)),active,done);b.SetInsertPoint(active);
                    }
                    copy_value(child,field_address(t,destination,field),field_address(t,source,field),check_failure);
                    if(done){b.CreateBr(done);b.SetInsertPoint(done);}
                }
            }
        };
        copy_children=[&](ir::Type t,llvm::Value* destination,llvm::Value* count,const std::function<void()>& check_failure) {
                auto child=ir::array_element(t);auto* record=element_record(child);
                if(!contains_arrays(child))return;
                auto* source=destination;
                auto* cursor=copy_cursors.at(t);b.CreateStore(b.getInt64(0),cursor);
                auto* test=llvm::BasicBlock::Create(context,"array.copy.test",fn);auto* body=llvm::BasicBlock::Create(context,"array.copy.body",fn);auto* done=llvm::BasicBlock::Create(context,"array.copy.done",fn);b.CreateBr(test);b.SetInsertPoint(test);auto* index=b.CreateLoad(b.getInt64Ty(),cursor);b.CreateCondBr(b.CreateICmpULT(index,count),body,done);b.SetInsertPoint(body);
                auto* temporary=element_staging.at(child);
                b.CreateCall(module->getOrInsertFunction("vd_value_array_read",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{temporary,source,record,count,b.getInt32(t.kind==ir::Type::growable_array),index,failure});check_failure();
                // The selected native value remains a precise whole-value root while
                // recursive child copies allocate. Publication follows their success.
                if(contains_arrays(child)) {
                    auto* copied=copy_staging.contains(child)?copy_staging.at(child):temporary;copy_value(child,copied,temporary,check_failure);temporary=copied;
                }
                b.CreateCall(module->getOrInsertFunction("vd_value_array_write",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt32Ty(),b.getPtrTy()),{destination,record,count,b.getInt32(t.kind==ir::Type::growable_array),index,temporary,b.getInt64(0),b.getInt32(0),b.getInt32(0),failure});check_failure();b.CreateStore(b.CreateAdd(index,b.getInt64(1)),cursor);b.CreateBr(test);b.SetInsertPoint(done);return;
        };
        std::vector<std::vector<llvm::Value*>> value_roots(f.blocks.size());
        if (input.coordinated) {
            auto* slot_type = llvm::StructType::get(b.getPtrTy(), size_type, b.getPtrTy(), size_type);
            const auto* slot_layout = layout.getStructLayout(slot_type);
            if (slot_layout->getSizeInBytes() != sizeof(Runtime::Slot) ||
                slot_layout->getElementOffset(1) != offsetof(Runtime::Slot,size) ||
                slot_layout->getElementOffset(2) != offsetof(Runtime::Slot,offsets) ||
                slot_layout->getElementOffset(3) != offsetof(Runtime::Slot,count))
                throw std::invalid_argument("runtime root ABI does not match target");
            struct RootEntry {llvm::Value* address;std::uint64_t bytes;llvm::Constant* offsets;std::uint64_t count;};
            std::vector<RootEntry> entries;
            auto add_storage_root = [&](std::uint64_t bytes, llvm::Value* address, const std::vector<std::uint64_t>& offsets) {
                if(!offsets.empty())entries.push_back({address,bytes,offset_map(offsets),offsets.size()});
            };
            auto add_root = [&](ir::Type t, llvm::Value* address) {
                std::vector<std::uint64_t> offsets;reference_offsets(t,0,offsets);
                add_storage_root(layout.getTypeAllocSize(type(t)),address,offsets);
            };
            std::vector<std::uint64_t> local_offsets;
            for (unsigned i = 0; i < slots.size(); ++i) {
                if(elided_locals.contains(i))continue;
                if(local_root_indexes[i]==~0u)add_root(f.locals[i],slots[i]);
            }
            for(unsigned index=0;index<local_root_offsets.size();++index)
                for(auto offset:local_root_offsets[index])
                    local_offsets.push_back(layout.getStructLayout(local_roots_type)->getElementOffset(index)+offset);
            if(local_roots)add_storage_root(layout.getTypeAllocSize(local_roots_type),local_roots,local_offsets);
            for(auto [t,storage]:copy_staging) add_root(t,storage);
            for(auto [t,storage]:element_staging) add_root(t,storage);
            for(auto [t,storage]:array_sources) add_root(t,storage);
            for (auto [slot,storage]:stage_storage) add_root(ir::Type(f.locals[slot].definition),storage);
            for (unsigned i = 0; i < f.parameters.size(); ++i) {
                if(async){add_root(f.parameters[i],frame_home(async->parameters[i]));continue;}
                if (f.parameters[i].construction) { add_root(ir::Type(f.parameters[i].definition),fn->getArg(i)); continue; }
                if (f.receiver && i == 0) continue;
                std::vector<std::uint64_t> offsets; reference_offsets(f.parameters[i],0,offsets);
                if (offsets.empty()) continue;
                auto* storage = home(b.CreateAlloca(type(f.parameters[i])),StorageRole::parameter,llvm_backend::HomeKind::argument);
                b.CreateStore(fn->getArg(i),storage); add_root(f.parameters[i],storage);
            }
            // Pointer-shaped instruction values have one function-duration
            // lifetime. Pack their homes into a target-layout aggregate and register
            // its exact reference offsets as one ordinary Slot. A single zero
            // fill replaces thousands of allocas, null stores and descriptors;
            // every reference word remains visible at every original safepoint.
            // Compound values retain their independent layout descriptors.
            std::vector<llvm::Type*> root_types;
            std::vector<std::vector<std::uint64_t>> root_offsets;
            // C2: validated instruction values are block-local. Locals carry
            // every CFG edge, so synchronous blocks may share pointer homes.
            // Keep ordinary function-duration root visibility until overwrite:
            // stale words still name live rooted objects, as in the baseline.
            // This reduces descriptor words without per-edge clear stores.
            if(!async)for(unsigned bn=0;bn<instruction_types.size();++bn) {
                unsigned block_roots=0;
                for(unsigned id=0;id<instruction_types[bn].size();++id) {
                    if(elided_values[bn].contains(id))continue;
                    auto t=instruction_types[bn][id];
                    std::vector<std::uint64_t> offsets;reference_offsets(t,0,offsets);
                    if(!offsets.empty() && type(t)->isPointerTy()) {
                        if(block_roots==root_types.size()) {
                            root_types.push_back(type(t));root_offsets.push_back(std::move(offsets));
                        }
                        ++block_roots;
                    }
                }
            }
            auto* packed_type=llvm::StructType::get(context,root_types);
            llvm::Value* packed=nullptr;
            if(!root_types.empty()) {
                packed=home(b.CreateAlloca(packed_type),StorageRole::instruction,llvm_backend::HomeKind::root);
                const auto* geometry=layout.getStructLayout(packed_type);
                b.CreateMemSet(packed,b.getInt8(0),geometry->getSizeInBytes(),layout.getABITypeAlign(packed_type));
                std::vector<std::uint64_t> offsets;
                for(unsigned n=0;n<root_offsets.size();++n)for(auto offset:root_offsets[n])offsets.push_back(geometry->getElementOffset(n)+offset);
                add_storage_root(geometry->getSizeInBytes(),packed,offsets);
            }
            for (unsigned block = 0; block < f.blocks.size(); ++block) {
                unsigned root_index=0;
                for (unsigned position=0;position<instruction_types[block].size();++position) {
                    auto t=instruction_types[block][position];
                    std::vector<std::uint64_t> offsets; reference_offsets(t,0,offsets);
                    llvm::Value* storage = nullptr;
                    if (!offsets.empty() && !elided_values[block].contains(position)) {
                        if(async || !type(t)->isPointerTy()) {
                            storage=async?frame_home(async->values[block][position]):home(b.CreateAlloca(type(t)),StorageRole::instruction,llvm_backend::HomeKind::root);
                            if(!async)b.CreateStore(llvm::Constant::getNullValue(type(t)),storage);
                            add_root(t,storage);
                        }
                        else storage=b.CreateStructGEP(packed_type,packed,root_index++);
                    }
                    value_roots[block].push_back(storage);
                }
            }
            auto* root_array_type = llvm::ArrayType::get(slot_type,entries.size());
            auto* root_array = home(b.CreateAlloca(root_array_type),StorageRole::roots,llvm_backend::HomeKind::slots);
            // Sizes and exact reference maps are immutable. Bulk-initialize
            // descriptors, then install each stack address before frame_enter.
            // This preserves every home/descriptor and avoids thousands of
            // independent constant stores in wide generated functions.
            if(!entries.empty()) {
                std::vector<llvm::Constant*> descriptors;
                for(const auto& entry:entries)descriptors.push_back(llvm::ConstantStruct::get(slot_type,{llvm::ConstantPointerNull::get(b.getPtrTy()),llvm::ConstantInt::get(size_type,entry.bytes),entry.offsets,llvm::ConstantInt::get(size_type,entry.count)}));
                auto* initial=new llvm::GlobalVariable(*module,root_array_type,true,llvm::GlobalValue::PrivateLinkage,llvm::ConstantArray::get(root_array_type,descriptors),"gc.slot.template");
                auto alignment=layout.getABITypeAlign(root_array_type);initial->setAlignment(alignment);
                b.CreateMemCpy(root_array,alignment,initial,alignment,layout.getTypeAllocSize(root_array_type));
                for(unsigned i=0;i<entries.size();++i){auto* entry=b.CreateInBoundsGEP(root_array_type,root_array,{b.getInt32(0),b.getInt32(i)});b.CreateStore(entries[i].address,b.CreateStructGEP(slot_type,entry,0));}
            }
            if(!entries.empty() || async || display_body || collection_body || deinit_body) {
            auto* frame = home(b.CreateAlloca(b.getInt8Ty(), b.getInt64(sizeof(Runtime::Frame))),StorageRole::roots,llvm_backend::HomeKind::frame);
            frame->setAlignment(llvm::Align(alignof(Runtime::Frame))); root_frame = frame;
            b.CreateCall(module->getOrInsertFunction("vd_gc_frame_enter",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type),
                {root_frame,root_array,llvm::ConstantInt::get(size_type,entries.size())});
            }
        }
        // Refine G40's null proof after assigning the EXISTING root homes.
        // SSA homes are shared across blocks; source-nullness alone is not a
        // physical-home fact. Source assignments and payload retirement stay
        // untouched. Only synchronous, known root-transfer functions qualify.
        std::set<std::pair<unsigned,unsigned>> redundant_root_definitions;
        if(input.coordinated && elision.precise_copies) {
            using Key=std::pair<llvm::Value*,std::int64_t>;
            std::map<Key,unsigned> root_keys;
            std::vector<std::vector<unsigned>> value_keys(f.blocks.size());
            std::vector<unsigned> local_keys(f.locals.size(),~0u);
            std::set<unsigned> all,initial;
            auto key=[&](llvm::Value* address) {
                if(!address)return ~0u;
                std::int64_t offset=0;
                auto* base=llvm::GetPointerBaseWithConstantOffset(address,offset,layout);
                if(!llvm::isa<llvm::AllocaInst>(base) || offset<0)return ~0u;
                auto [where,fresh]=root_keys.emplace(Key{base,offset},root_keys.size());
                if(fresh)all.insert(where->second);
                return where->second;
            };
            for(unsigned slot=0;slot<f.locals.size();++slot)
                if(f.locals[slot].kind==ir::Type::managed_ref && !elided_locals.contains(slot)) {
                    local_keys[slot]=key(slots[slot]);
                    if(slot>=f.parameters.size() && local_keys[slot]!=~0u)initial.insert(local_keys[slot]);
                }
            for(unsigned bn=0;bn<f.blocks.size();++bn)
                for(unsigned id=0;id<instruction_types[bn].size();++id) {
                    // Other pointer-shaped values also overwrite shared homes.
                    auto k=type(instruction_types[bn][id])->isPointerTy()?key(value_roots[bn][id]):~0u;
                    value_keys[bn].push_back(k);
                    if(k!=~0u)initial.insert(k); // Actual entry zero initialization.
                }
            const bool bounded=f.blocks.size()<=256 && all.size()<=256 &&
                f.blocks.size()*all.size()<=16384;
            std::vector<std::set<unsigned>> incoming,outgoing;
            if(bounded){incoming.assign(f.blocks.size(),all);outgoing=incoming;}
            auto transfer=[&](unsigned bn,std::set<unsigned> state,bool plan) {
                const auto& instructions=f.blocks[bn].instructions;
                std::vector<bool> null_values(instructions.size());
                for(unsigned id=0;id<instructions.size();++id) {
                    const auto& i=instructions[id];auto t=instruction_types[bn][id];
                    if(t.kind==ir::Type::managed_ref && !t.construction)
                        null_values[id]=(i.op==ir::Op::constant && i.immediate==0) ||
                            (i.op==ir::Op::load && local_keys[i.a]!=~0u && state.contains(local_keys[i.a]));
                    if(i.op==ir::Op::store && local_keys[i.a]!=~0u) {
                        if(null_values[i.b])state.insert(local_keys[i.a]);else state.erase(local_keys[i.a]);
                    }
                    auto k=value_keys[bn][id];
                    if(k!=~0u) {
                        if(null_values[id]) {
                            if(plan && state.contains(k))redundant_root_definitions.insert({bn,id});
                            state.insert(k);
                        } else state.erase(k);
                    }
                    if(auto event=elision.retirements.find({bn,id});event!=elision.retirements.end()) {
                        auto& clear=event->second;
                        if(plan) {
                            std::erase_if(clear.values,[&](unsigned v){return state.contains(value_keys[bn][v]);});
                            std::erase_if(clear.locals,[&](unsigned slot){return state.contains(local_keys[slot]);});
                        }
                        for(auto v:clear.values)if(value_keys[bn][v]!=~0u)state.insert(value_keys[bn][v]);
                        for(auto slot:clear.locals)if(local_keys[slot]!=~0u)state.insert(local_keys[slot]);
                        // Never infer payload-field state from a copy-home fact.
                    }
                }
                return state;
            };
            if(bounded) {
                bool changed;
                do {
                    changed=false;
                    for(unsigned bn=0;bn<f.blocks.size();++bn) {
                        auto state=(bn==0 || elision.predecessors[bn].empty())?initial:all;
                        for(auto pred:elision.predecessors[bn])
                            std::erase_if(state,[&](unsigned k){return !outgoing[pred].contains(k);});
                        auto next=transfer(bn,state,false);
                        if(state!=incoming[bn] || next!=outgoing[bn])changed=true;
                        incoming[bn]=std::move(state);outgoing[bn]=std::move(next);
                    }
                }while(changed);
            }
            // Above the same bounded budget, only this block's writes count.
            for(unsigned bn=0;bn<f.blocks.size();++bn)
                transfer(bn,bounded?incoming[bn]:std::set<unsigned>{},true);
        }
        if(collection_body) {
            auto* entered=b.CreateCall(module->getOrInsertFunction("vd_collection_scope",b.getInt1Ty(),b.getPtrTy()),{failure});
            auto* invalid=llvm::BasicBlock::Create(context,"collection_scope_error",fn);auto* ready=llvm::BasicBlock::Create(context,"collection_scope_ready",fn);
            b.CreateCondBr(entered,ready,invalid);b.SetInsertPoint(invalid);
            b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave",b.getVoidTy(),b.getPtrTy()),{root_frame});if(f.result==ir::Type::none)b.CreateRetVoid();else b.CreateRet(llvm::Constant::getNullValue(type(f.result)));
            blocks[0]=ready;b.SetInsertPoint(ready);
        }
        if(display_body) {
            llvm::Value* object=f.parameters[0].kind==ir::Type::managed_ref?static_cast<llvm::Value*>(fn->getArg(0)):llvm::ConstantPointerNull::get(b.getPtrTy());
            b.CreateCall(module->getOrInsertFunction("vd_display_enter",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{b.CreateGlobalString(f.name),object,failure});
            auto* invalid=llvm::BasicBlock::Create(context,"display_hook_error",fn);auto* ready=llvm::BasicBlock::Create(context,"display_hook_ready",fn);
            b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),invalid,ready);b.SetInsertPoint(invalid);
            // Entry failed before any source cleanup was reached or guard pushed.
            if(collection_body)b.CreateCall(module->getOrInsertFunction("vd_collection_scope_leave",b.getVoidTy()));
            b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave",b.getVoidTy(),b.getPtrTy()),{root_frame});b.CreateRet(llvm::Constant::getNullValue(type(f.result)));
            blocks[0]=ready;b.SetInsertPoint(ready);
        }
        if(deinit_body)b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_enter",b.getVoidTy()));
        if (!async && !input.globals.empty() && source_entries.contains(f.name)) {
            b.CreateCall(module->getOrInsertFunction("vd_globals_ready", b.getVoidTy(), b.getPtrTy()), {failure});
            auto* invalid = llvm::BasicBlock::Create(context, "globals.entry_early", fn);
            auto* ready = llvm::BasicBlock::Create(context, "globals.entry_ready", fn);
            b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(), failure)), invalid, ready);
            b.SetInsertPoint(invalid); return_failure(); blocks[0] = ready; b.SetInsertPoint(ready);
        }
        llvm::SwitchInst* resume_states=nullptr;unsigned resume_serial=0;
        if(async){auto* invalid=llvm::BasicBlock::Create(context,"async.invalid_state",fn);resume_states=b.CreateSwitch(b.CreateLoad(b.getInt32Ty(),frame_home(1)),invalid);resume_states->addCase(b.getInt32(0),blocks.front());b.SetInsertPoint(invalid);b.CreateStore(b.CreateGlobalString("Invalid async resume state"),failure);return_failure();}
        for (std::size_t n = 0; n < f.blocks.size(); ++n) {
            active_block=n;b.SetInsertPoint(blocks[n]);
            if(source_debug)b.SetCurrentDebugLocation(llvm::DebugLoc());
            if (input.coordinated) b.CreateCall(module->getOrInsertFunction("vd_gc_poll",b.getVoidTy()));
            if(!async && input.coordinated && (!f.cleanup_entry || n<*f.cleanup_entry)){
                auto* cancelled=b.CreateCall(module->getOrInsertFunction("vd_worker_checkpoint",b.getInt1Ty(),b.getPtrTy()),{failure});
                auto* bad=llvm::BasicBlock::Create(context,"worker.cancelled",fn);auto* good=llvm::BasicBlock::Create(context,"worker.continue",fn);b.CreateCondBr(cancelled,bad,good);b.SetInsertPoint(bad);return_failure();b.SetInsertPoint(good);
            }
            if(async && n<*f.cleanup_entry){
                b.CreateCall(module->getOrInsertFunction("vd_async_checkpoint",b.getVoidTy(),b.getPtrTy()),{failure});
                auto* bad=llvm::BasicBlock::Create(context,"async.checkpoint_failure",fn);auto* good=llvm::BasicBlock::Create(context,"async.checkpoint_valid",fn);
                b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,good);b.SetInsertPoint(bad);return_failure();b.SetInsertPoint(good);
            }
            std::vector<llvm::Value*> values;
            std::vector<ir::Type> value_types;
            const bool defer_return_load=ir::checked_storage_cleanup_return(f,n,input.c_primitives.get());
            auto check_cleanup_return=[&] {
                if(async)b.CreateCall(module->getOrInsertFunction("vd_async_finish",b.getVoidTy(),b.getPtrTy()),{failure});
                auto* failed=llvm::BasicBlock::Create(context,"cleanup_failed",fn);auto* ready=llvm::BasicBlock::Create(context,"cleanup_complete",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),failed,ready);b.SetInsertPoint(failed);return_failure();b.SetInsertPoint(ready);
            };
            for (const auto& instruction : f.blocks[n].instructions) {
                const auto& loc = instruction.source.path.empty() ? f.source : instruction.source;
                if(subprogram) {
                    llvm::DILocalScope* scope=subprogram;
                    if(source_debug && loc.path!=f.source.path) {
                        auto [entry,fresh]=file_scopes.try_emplace(loc.path);
                        if(fresh)entry->second=debug->createLexicalBlockFile(subprogram,debug_file(loc.path));
                        scope=entry->second;
                    }
                    b.SetCurrentDebugLocation(llvm::DILocation::get(context,loc.line,loc.column,scope));
                }
                if(defer_return_load && &instruction==&f.blocks[n].instructions.back())check_cleanup_return();
                llvm::Value* v = nullptr;
                using ir::Op;
                auto trace_failure = [&] {
                        auto* count_address = b.CreateStructGEP(failure_type, failure, 1);
                        auto* count = b.CreateLoad(b.getInt32Ty(), count_address);
                        auto* append = llvm::BasicBlock::Create(context, "trace_caller", fn);
                        auto* done = llvm::BasicBlock::Create(context, "error_return", fn);
                        auto* limit = llvm::BasicBlock::Create(context, "trace_limit", fn);
                        b.CreateCondBr(b.CreateICmpULT(count, b.getInt32(32)), append, limit);
                        b.SetInsertPoint(limit); b.CreateStore(b.getInt32(33), count_address); b.CreateBr(done);
                        b.SetInsertPoint(append);
                        auto trace = "Called from " + f.module_identity + "::" + (f.display_name.empty()?f.source_name:f.display_name) + "\n" +
                            source_message(loc, instruction.source_excerpt, "source call");
                        auto* frames = b.CreateStructGEP(failure_type, failure, 2);
                        auto* address = b.CreateGEP(llvm::ArrayType::get(b.getPtrTy(), 32), frames, {b.getInt32(0), count});
                        b.CreateStore(b.CreateGlobalString(trace), address);
                        b.CreateStore(b.CreateAdd(count, b.getInt32(1)), count_address);
                        b.CreateBr(done); b.SetInsertPoint(done); return_failure();
                };
                auto fail_if = [&](llvm::Value* invalid_value, const std::string& description) {
                    auto* invalid = llvm::BasicBlock::Create(context,"numeric_error",fn);
                    auto* valid = llvm::BasicBlock::Create(context,"numeric_valid",fn);
                    b.CreateCondBr(invalid_value,invalid,valid); b.SetInsertPoint(invalid);
                    if (failure) {
                        auto message = source_message(loc,instruction.source_excerpt,"runtime error: " + description);
                        message += "In function " + f.module_identity + "::" + (f.display_name.empty()?f.source_name:f.display_name) + "\n";
                        b.CreateStore(b.CreateGlobalString(message),b.CreateStructGEP(failure_type,failure,0)); return_failure();
                    } else { b.CreateCall(llvm::Intrinsic::getOrInsertDeclaration(module.get(),llvm::Intrinsic::trap)); b.CreateUnreachable(); }
                    b.SetInsertPoint(valid);
                };
                auto selected_failure = [&] {
                    auto* bad=llvm::BasicBlock::Create(context,"selected.failure",fn);auto* ready=llvm::BasicBlock::Create(context,"selected.valid",fn);
                    b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,ready);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(ready);
                };
                // IR operand types preserve signedness, even when native widths coincide.
                auto operand_type = [&](unsigned id) { return value_types.at(id); };
                std::function<void(ir::Type,llvm::Value*)> guard_complete;
                guard_complete = [&](ir::Type value_type,llvm::Value* value) {
                    if(value_type.definition && value_type.definition->callable){if(!value_type.nullable)fail_if(b.CreateICmpEQ(b.CreateExtractValue(value,0),b.getInt64(0)),"Null supplied for non-nullable function field");return;}
                    if(value_type.kind==ir::Type::aggregate) {
                        for(unsigned field=0;field<value_type.definition->fields.size();++field) {
                            llvm::BasicBlock* done=nullptr;
                            if(!value_type.definition->cases.empty() && field) {
                                unsigned variant=0;for(;variant<value_type.definition->cases.size();++variant){const auto& c=value_type.definition->cases[variant];if(field>=c.begin && field<c.begin+c.count)break;}
                                auto* active=llvm::BasicBlock::Create(context,"validate_active_payload",fn);done=llvm::BasicBlock::Create(context,"validate_payload_done",fn);
                                b.CreateCondBr(b.CreateICmpEQ(b.CreateExtractValue(value,0),b.getInt32(variant)),active,done);b.SetInsertPoint(active);
                            }
                            guard_complete(ir::field_type(value_type,field),b.CreateExtractValue(value,field));
                            if(done){b.CreateBr(done);b.SetInsertPoint(done);}
                        }
                        return;
                    }
                    if(!ir::reference_value(value_type))return;
                    auto leaf=value_type;auto* reference=value;
                    if(leaf.kind==ir::Type::interface_ref) {
                        auto* object=b.CreateExtractValue(reference,0);auto* witness=b.CreateExtractValue(reference,1);
                        auto* selected=binding_index(reference);auto* bad=b.CreateICmpSLT(selected,b.getInt64(0));
                        llvm::Value* known=b.getFalse();
                        for(unsigned n=0;n<bindings.size();++n)if(bindings[n]->interface.definition==leaf.definition)known=b.CreateOr(known,b.CreateICmpEQ(selected,b.getInt64(n)));
                        bad=b.CreateOr(bad,b.CreateNot(known));
                        if(leaf.nullable)bad=b.CreateAnd(bad,b.CreateNot(b.CreateAnd(b.CreateIsNull(object),b.CreateIsNull(witness))));
                        fail_if(bad,"Invalid interface field binding");
                    } else if(!leaf.nullable)fail_if(b.CreateIsNull(reference),"Null supplied for non-nullable field");
                };
                auto array_failure=[&] {fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Array operation failed");};
                auto native_arg=[&](ir::Type t,unsigned id,bool own=false)->llvm::Value* {
                    auto* source=value_roots[n].at(id);
                    if(!source){source=element_staging.at(t);b.CreateStore(values[id],source);}
                    if(own && contains_arrays(t)){auto* destination=copy_staging.at(t);copy_value(t,destination,source,array_failure);return destination;}
                    return source;
                };
                auto native_write=[&](ir::Type array,llvm::Value* owner,llvm::Value* index,llvm::Value* value,bool initialize=false,llvm::Value* version=nullptr) {
                    b.CreateCall(module->getOrInsertFunction("vd_value_array_write",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt32Ty(),b.getPtrTy()),{owner,element_record(ir::array_element(array)),b.getInt64(array.definition->extent.value_or(0)),b.getInt32(array.kind==ir::Type::growable_array),index,value,version?version:b.getInt64(0),b.getInt32(version!=nullptr),b.getInt32(initialize),failure});if(version)selected_failure();else array_failure();
                };
                auto bits=[&](llvm::Value* value,ir::Type t)->llvm::Value* {
                    if(numeric::floating(t)) value=b.CreateBitCast(value,b.getIntNTy(numeric::width(t)));
                    return b.CreateZExtOrTrunc(value,b.getInt64Ty());
                };
                auto array_index=[&](unsigned id,ir::Type array,unsigned owner,bool boundary=false)->llvm::Value* {
                    auto index_type=operand_type(id);auto* index=values.at(id);
                    index=numeric::signed_integer(index_type)?b.CreateSExtOrTrunc(index,b.getInt64Ty()):b.CreateZExtOrTrunc(index,b.getInt64Ty());
                    llvm::Value* count;
                    if(array.kind==ir::Type::growable_array) {
                        count=ir::scalar_array(array) ? b.CreateCall(module->getOrInsertFunction("vd_list_count",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{value_roots[n].at(owner),b.getInt32(scalar_encoding(ir::array_element(array))),failure}) : b.CreateCall(module->getOrInsertFunction("vd_value_array_count",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{value_roots[n].at(owner),element_record(ir::array_element(array)),failure});array_failure();
                    } else count=b.getInt64(*array.definition->extent);
                    if(numeric::signed_integer(index_type)) index=b.CreateSelect(b.CreateICmpSLT(index,b.getInt64(0)),b.CreateAdd(index,count),index);
                    fail_if(boundary?b.CreateICmpUGT(index,count):b.CreateICmpUGE(index,count),array.kind==ir::Type::growable_array ? "List index outside extent" : "Fixed array index outside extent");return index;
                };
                switch (instruction.op) {
                case Op::fixed_array: {
                    auto t=instruction.type;auto* storage=value_roots[n].at(values.size());
                    if(!ir::scalar_array(t)) {
                        auto child=ir::array_element(t);auto* count=b.getInt64(t.definition->extent.value_or(instruction.arguments.size()));
                        b.CreateCall(module->getOrInsertFunction("vd_value_array_new",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{storage,element_record(child),count,b.getInt32(t.kind==ir::Type::growable_array),failure});array_failure();
                        if(instruction.array_repeat && *t.definition->extent) {
                            auto* cursor=copy_cursors.at(t);b.CreateStore(b.getInt64(0),cursor);
                            auto* test=llvm::BasicBlock::Create(context,"array.default.test",fn);auto* body=llvm::BasicBlock::Create(context,"array.default.body",fn);auto* done=llvm::BasicBlock::Create(context,"array.default.done",fn);b.CreateBr(test);b.SetInsertPoint(test);auto* index=b.CreateLoad(b.getInt64Ty(),cursor);b.CreateCondBr(b.CreateICmpULT(index,count),body,done);b.SetInsertPoint(body);
                            auto* value=b.CreateCall(functions.at(outlined.at({f.name,n,static_cast<unsigned>(values.size())})),{failure});array_failure();auto* temporary=element_staging.at(child);b.CreateStore(value,temporary);native_write(t,storage,index,temporary,true);
                            b.CreateStore(b.CreateAdd(index,b.getInt64(1)),cursor);b.CreateBr(test);b.SetInsertPoint(done);
                        } else for(unsigned i=0;i<instruction.arguments.size();++i)native_write(t,storage,b.getInt64(i),native_arg(child,instruction.arguments[i],true),true);
                        v=b.CreateLoad(type(t),storage);break;
                    }
                    if(t.kind==ir::Type::growable_array) {
                        auto* encoding=b.getInt32(scalar_encoding(ir::array_element(t)));
                        b.CreateCall(module->getOrInsertFunction("vd_list_new",b.getVoidTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{storage,encoding,b.getInt64(instruction.arguments.size()),failure});array_failure();
                        for(unsigned i=0;i<instruction.arguments.size();++i) {
                            b.CreateCall(module->getOrInsertFunction("vd_list_write",b.getVoidTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{storage,encoding,b.getInt64(i),bits(values[instruction.arguments[i]],ir::array_element(t)),b.getInt64(0),b.getInt32(0),failure});array_failure();
                        }
                        v=b.CreateLoad(type(t),storage);break;
                    }
                    auto* encoding=b.getInt32(scalar_encoding(ir::array_element(t)));auto* count=b.getInt64(*t.definition->extent);
                    auto* initial=instruction.array_repeat && !instruction.arguments.empty()?bits(values[instruction.arguments.front()],ir::array_element(t)):static_cast<llvm::Value*>(b.getInt64(0));
                    b.CreateCall(module->getOrInsertFunction("vd_array_default",b.getVoidTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getPtrTy()),{storage,encoding,count,initial,failure});array_failure();
                    if(!instruction.array_repeat) for(unsigned i=0;i<instruction.arguments.size();++i) {
                        b.CreateCall(module->getOrInsertFunction("vd_array_write",b.getVoidTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),
                            {storage,b.getInt64(i),bits(values[instruction.arguments[i]],ir::array_element(t)),encoding,count,failure});array_failure();
                    }
                    v=b.CreateLoad(type(t),storage);break;
                }
                case Op::array_get: case Op::array_set: case Op::fixed_snapshot: {
                    auto t=operand_type(instruction.a);auto* source=value_roots[n].at(instruction.a);
                    const bool scalar_list_read=instruction.op==Op::array_get && t.kind==ir::Type::growable_array && ir::scalar_array(t);
                    llvm::Value* index=nullptr;
                    if(scalar_list_read) {
                        // The Runtime normalizes and bounds-checks against its
                        // validated header in the same operation as the read.
                        auto raw=values.at(instruction.b);
                        index=numeric::signed_integer(operand_type(instruction.b))?b.CreateSExtOrTrunc(raw,b.getInt64Ty()):b.CreateZExtOrTrunc(raw,b.getInt64Ty());
                    } else if(instruction.op!=Op::fixed_snapshot)index=array_index(instruction.b,t,instruction.a);
                    if(!ir::scalar_array(t)) {
                        auto child=ir::array_element(t);auto* count=b.getInt64(t.definition->extent.value_or(0));
                        if(instruction.op==Op::array_get) {
                            auto* result=element_staging.at(child);
                            b.CreateCall(module->getOrInsertFunction("vd_value_array_read",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{result,source,element_record(child),count,b.getInt32(t.kind==ir::Type::growable_array),index,failure});array_failure();v=b.CreateLoad(type(child),result);
                        } else {auto* result=value_roots[n].at(values.size());copy_value(t,result,source,array_failure);native_write(t,result,index,native_arg(child,instruction.arguments.front(),true));v=b.CreateLoad(type(t),result);}
                        break;
                    }
                    auto* encoding=b.getInt32(scalar_encoding(ir::array_element(t)));auto* count=b.getInt64(t.definition->extent.value_or(0));
                    if(instruction.op==Op::array_get) {
                        auto* value=t.kind==ir::Type::growable_array ?
                            b.CreateCall(module->getOrInsertFunction("vd_list_read_index",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{source,encoding,index,b.getInt32(numeric::signed_integer(operand_type(instruction.b))),failure}) :
                            b.CreateCall(module->getOrInsertFunction("vd_array_read",b.getInt64Ty(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{source,index,encoding,count,failure});
                        fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Array operation failed during index access");
                        auto element=ir::array_element(t);auto* narrow=b.CreateTruncOrBitCast(value,b.getIntNTy(numeric::width(element)));
                        v=numeric::floating(element)?b.CreateBitCast(narrow,type(element)):narrow;
                    } else {
                        auto* destination=value_roots[n].at(values.size());
                        copy_value(t,destination,source,array_failure);
                        if(instruction.op==Op::array_set) {
                            b.CreateCall(module->getOrInsertFunction("vd_array_write",b.getVoidTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),
                                {destination,index,bits(values[instruction.arguments[0]],ir::array_element(t)),encoding,count,failure});array_failure();
                        }
                        v=b.CreateLoad(type(t),destination);
                    }
                    break;
                }
                case Op::array_allocate: {
                    auto t=instruction.type;auto* result=value_roots[n].at(values.size());b.CreateCall(module->getOrInsertFunction("vd_value_array_new",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{result,element_record(ir::array_element(t)),b.getInt64(*t.definition->extent),b.getInt32(0),failure});array_failure();v=b.CreateLoad(type(t),result);break;
                }
                case Op::array_initialize: {
                    auto t=operand_type(instruction.a);native_write(t,value_roots[n].at(instruction.a),values[instruction.b],native_arg(ir::array_element(t),instruction.arguments[0],true),true);break;
                }
                case Op::array_finish: {
                    auto t=operand_type(instruction.a);
                    b.CreateCall(module->getOrInsertFunction("vd_value_array_complete",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),
                        {value_roots[n].at(instruction.a),element_record(ir::array_element(t)),b.getInt64(*t.definition->extent),failure});
                    selected_failure();v=values[instruction.a];break;
                }
                case Op::array_capture: v=b.CreateStore(values[instruction.a],slots.at(instruction.immediate));break;
                case Op::list_check: {
                    auto t=operand_type(instruction.a);
                    auto* version=ir::scalar_array(t) ? b.CreateCall(module->getOrInsertFunction("vd_list_version",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{value_roots[n].at(instruction.a),b.getInt32(scalar_encoding(ir::array_element(t))),failure}) : b.CreateCall(module->getOrInsertFunction("vd_value_array_version",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{value_roots[n].at(instruction.a),element_record(ir::array_element(t)),failure});array_failure();
                    fail_if(b.CreateICmpNE(version,values[instruction.b]),"List structurally modified during iteration");break;
                }
                case Op::array_count: case Op::list_version: {
                    auto t=operand_type(instruction.a);
                    if(t.kind==ir::Type::fixed_array)v=b.getInt64(*t.definition->extent);
                    else if(!ir::scalar_array(t)){v=b.CreateCall(module->getOrInsertFunction(instruction.op==Op::list_version ? "vd_value_array_version" : "vd_value_array_count",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{value_roots[n].at(instruction.a),element_record(ir::array_element(t)),failure});array_failure();}
                    else {v=b.CreateCall(module->getOrInsertFunction(instruction.op==Op::list_version ? "vd_list_version" : "vd_list_count",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{value_roots[n].at(instruction.a),b.getInt32(scalar_encoding(ir::array_element(t))),failure});array_failure();}
                    break;
                }
                case Op::dict_new: {
                    auto* result=value_roots[n].at(values.size());b.CreateCall(module->getOrInsertFunction("vd_dict_new",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{result,dictionary_record(instruction.type),failure});array_failure();for(auto argument:instruction.arguments){auto* version=b.CreateCall(module->getOrInsertFunction("vd_dict_count",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{result,dictionary_record(instruction.type),b.getInt32(1),failure});array_failure();b.CreateCall(module->getOrInsertFunction("vd_dict_write",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{result,dictionary_record(instruction.type),native_arg(ir::array_element(instruction.type),argument,true),version,failure});array_failure();}v=b.CreateLoad(b.getPtrTy(),result);break;
                }
                case Op::dict_get: case Op::dict_contains: case Op::dict_remove: case Op::dict_set: case Op::dict_entry: case Op::dict_count: case Op::dict_version: case Op::dict_check: case Op::dict_clear: {
                    auto owner=operand_type(instruction.a);auto* root=value_roots[n].at(instruction.a);auto* record=dictionary_record(owner);auto entry=ir::array_element(owner);
                    auto check=[&]{if(instruction.op==Op::dict_set){selected_failure();return;}fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),instruction.op==Op::dict_get?"Dictionary key not found or lookup failed":instruction.op==Op::dict_set?"Dictionary selected write invalidated or failed":"Dictionary operation failed");};
                    if(instruction.op==Op::dict_count || instruction.op==Op::dict_version || instruction.op==Op::dict_check){auto* count=b.CreateCall(module->getOrInsertFunction("vd_dict_count",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{root,record,b.getInt32(instruction.op!=Op::dict_count),failure});check();if(instruction.op==Op::dict_check)fail_if(b.CreateICmpNE(count,values[instruction.b]),"Dictionary structurally modified during iteration or selected access");else v=count;}
                    else if(instruction.op==Op::dict_clear){b.CreateCall(module->getOrInsertFunction("vd_dict_clear",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{root,record,failure});check();}
                    else if(instruction.op==Op::dict_set){b.CreateCall(module->getOrInsertFunction("vd_dict_write",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{root,record,native_arg(entry,instruction.b,true),values[instruction.arguments[0]],failure});check();}
                    else if(instruction.op==Op::dict_remove){auto* found=b.CreateCall(module->getOrInsertFunction("vd_dict_remove",b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{root,record,native_arg(ir::dictionary_key(owner),instruction.b,true),failure});check();v=b.CreateICmpNE(found,b.getInt32(0));}
                    else {auto* result=element_staging.at(entry);
                        if(instruction.op==Op::dict_entry)b.CreateCall(module->getOrInsertFunction("vd_dict_entry",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{result,root,record,values[instruction.b],failure});
                        else {auto* found=b.CreateCall(module->getOrInsertFunction("vd_dict_read",b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{result,root,record,native_arg(ir::dictionary_key(owner),instruction.b,true),b.getInt32(instruction.op==Op::dict_contains),failure});if(instruction.op==Op::dict_contains)v=b.CreateICmpNE(found,b.getInt32(0));}
                        check();if(instruction.op!=Op::dict_contains){v=b.CreateLoad(type(entry),result);if(instruction.op==Op::dict_get)v=b.CreateExtractValue(v,1);}
                    }break;
                }
                case Op::array_slice: {
                    auto t=operand_type(instruction.a);auto* source=value_roots[n].at(instruction.a);
                    auto* first=array_index(instruction.b,t,instruction.a,instruction.immediate!=0);
                    auto* last=array_index(instruction.arguments[0],t,instruction.a,instruction.immediate!=0);
                    auto* descending=b.CreateICmpUGT(first,last);
                    auto* length=b.CreateSelect(descending,b.CreateSub(first,last),b.CreateSub(last,first));
                    if(!instruction.immediate)length=b.CreateAdd(length,b.getInt64(1));
                    // Equal exclusive boundaries are empty; every other start is an element.
                    if(instruction.immediate){auto* empty=llvm::BasicBlock::Create(context,"slice.empty",fn);auto* checked=llvm::BasicBlock::Create(context,"slice.start",fn);auto* join=llvm::BasicBlock::Create(context,"slice.copy",fn);b.CreateCondBr(b.CreateICmpEQ(first,last),empty,checked);b.SetInsertPoint(empty);b.CreateBr(join);b.SetInsertPoint(checked);array_index(instruction.b,t,instruction.a);b.CreateBr(join);b.SetInsertPoint(join);}
                    auto* result=value_roots[n].at(values.size());
                    b.CreateCall(module->getOrInsertFunction("vd_value_array_slice",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{result,source,element_record(ir::array_element(t)),b.getInt64(t.definition->extent.value_or(0)),b.getInt32(t.kind==ir::Type::growable_array),first,length,b.CreateZExt(descending,b.getInt32Ty()),failure});array_failure();
                    if(contains_arrays(ir::array_element(t)))copy_children(instruction.type,result,length,array_failure);
                    v=b.CreateLoad(type(instruction.type),result);break;
                }
                case Op::list_insert: case Op::list_clear: {
                    auto t=operand_type(instruction.a);auto* source=value_roots[n].at(instruction.a);auto* record=element_record(ir::array_element(t));
                    if(instruction.op==Op::list_clear)b.CreateCall(module->getOrInsertFunction("vd_value_array_clear",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{source,record,failure});
                    else b.CreateCall(module->getOrInsertFunction("vd_value_array_insert",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{source,record,values[instruction.b],native_arg(ir::array_element(t),instruction.arguments[0],true),values[instruction.arguments[1]],failure});
                    if(instruction.op==Op::list_insert)fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"List selected insertion invalidated or failed");else array_failure();break;
                }
                case Op::list_index: case Op::list_push: case Op::list_remove: case Op::list_commit: {
                    auto t=operand_type(instruction.a);auto element=ir::array_element(t);auto* source=value_roots[n].at(instruction.a);
                    if(!ir::scalar_array(t)) {
                        if(instruction.op==Op::list_index){v=array_index(instruction.b,t,instruction.a,instruction.immediate!=0);break;}
                        if(instruction.op==Op::list_commit)native_write(t,source,values[instruction.b],native_arg(element,instruction.arguments[0],true),false,values[instruction.arguments[1]]);
                        else if(instruction.op==Op::list_push){b.CreateCall(module->getOrInsertFunction("vd_value_array_push",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{source,element_record(element),native_arg(element,instruction.b,true),failure});array_failure();}
                        else {auto* index=array_index(instruction.b,t,instruction.a);auto* result=element_staging.at(element);b.CreateCall(module->getOrInsertFunction("vd_value_array_remove",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy()),{result,source,element_record(element),index,failure});array_failure();v=b.CreateLoad(type(element),result);}
                        break;
                    }
                    auto* encoding=b.getInt32(scalar_encoding(element));
                    if(instruction.op==Op::list_index) {v=array_index(instruction.b,t,instruction.a,instruction.immediate!=0);break;}
                    if(instruction.op==Op::list_push) {
                        v=b.CreateCall(module->getOrInsertFunction("vd_list_push",b.getVoidTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{source,encoding,bits(values[instruction.b],element),failure});array_failure();
                    } else if(instruction.op==Op::list_commit) {
                        v=b.CreateCall(module->getOrInsertFunction("vd_list_write",b.getVoidTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy()),{source,encoding,values[instruction.b],bits(values[instruction.arguments[0]],element),values[instruction.arguments[1]],b.getInt32(1),failure});
                        selected_failure();
                    } else {
                        auto* index=array_index(instruction.b,t,instruction.a);
                        auto* value=b.CreateCall(module->getOrInsertFunction("vd_list_remove",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{source,encoding,index,failure});array_failure();
                        auto* narrow=b.CreateTruncOrBitCast(value,b.getIntNTy(numeric::width(element)));v=numeric::floating(element)?b.CreateBitCast(narrow,type(element)):narrow;
                    }
                    break;
                }
                case Op::fixed_select: case Op::fixed_commit:
                    throw std::invalid_argument("Private owned-selection IR is not a source array operation");
                case Op::interface_pack: {
                    auto* object=values[instruction.a];auto* nonnull=llvm::BasicBlock::Create(context,"bind_interface",fn);auto* joined=llvm::BasicBlock::Create(context,"interface_view",fn);auto* start=b.GetInsertBlock();
                    b.CreateCondBr(b.CreateIsNull(object),joined,nonnull);b.SetInsertPoint(nonnull);
                    auto* witness=b.CreateCall(module->getOrInsertFunction("vd_gc_witness",b.getPtrTy(),b.getPtrTy()),{string(instruction.callee)});
                    fail_if(b.CreateIsNull(witness),"Interface binding is not admitted");
                    llvm::Value* pair=llvm::Constant::getNullValue(interface_type);pair=b.CreateInsertValue(pair,object,0);pair=b.CreateInsertValue(pair,witness,1);auto* bound=b.GetInsertBlock();b.CreateBr(joined);b.SetInsertPoint(joined);
                    auto* phi=b.CreatePHI(interface_type,2);phi->addIncoming(llvm::Constant::getNullValue(interface_type),start);phi->addIncoming(pair,bound);v=phi;break;
                }
                case Op::interface_prepare:
                    v=b.CreateCall(preparations.at(instruction.callee),{values[instruction.a],failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Interface receiver preparation failed");break;
                case Op::gc_poll: case Op::gc_collect: {
                    if (instruction.op == Op::gc_poll) v = b.CreateCall(module->getOrInsertFunction("vd_gc_poll",b.getVoidTy()));
                    else {
                        v = b.CreateCall(module->getOrInsertFunction("vd_gc_collect",b.getVoidTy(),b.getPtrTy()),{failure});
                        fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Collection failed");
                    }
                    break;
                }
                case Op::gc_release:v=b.CreateCall(module->getOrInsertFunction("vd_gc_release",b.getInt1Ty(),b.getPtrTy()),{values[instruction.a]});break;
                case Op::gc_alive:
                    v = b.CreateCall(module->getOrInsertFunction("vd_gc_alive",b.getInt1Ty(),b.getPtrTy()),{values[instruction.a]}); break;
                case Op::stage_check:
                    v=b.CreateICmpEQ(b.CreateExtractValue(values[instruction.a],0),b.getInt32(0));break;
                case Op::initializer_return:
                    v=values[instruction.a];break;
                case Op::stage_abort: {
                    auto slot=instruction.receiver->slot;
                    auto* native=type(ir::Type(f.locals.at(slot).definition));
                    v=b.CreateMemSet(stage_storage.at(slot),b.getInt8(0),std::max<std::uint64_t>(1,layout.getTypeAllocSize(native)),layout.getABITypeAlign(native));
                    break;
                }
                case Op::stage_begin: {
                    auto* native=type(ir::Type(instruction.type.definition));
                    v=stage_storage.at(instruction.a);
                    b.CreateMemSet(v,b.getInt8(0),std::max<std::uint64_t>(1,layout.getTypeAllocSize(native)),layout.getABITypeAlign(native));
                    break;
                }
                case Op::gc_construct: {
                    auto object=ir::Type(instruction.type.definition);
                    auto* native=type(object);
                    auto bytes=std::max<std::uint64_t>(1,layout.getTypeAllocSize(native));
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    const auto planned=elided_payloads.find({n,static_cast<unsigned>(values.size())});
                    llvm::Value* payload;
                    if(planned!=elided_payloads.end())payload=planned->second;
                    else {
                        auto* storage=home(allocations.CreateAlloca(b.getInt8Ty(),b.getInt64(bytes)),StorageRole::staging,llvm_backend::HomeKind::staging);
                        storage->setAlignment(layout.getABITypeAlign(native));payload=storage;
                    }
                    b.CreateMemSet(payload,b.getInt8(0),bytes,layout.getABITypeAlign(native));
                    for (unsigned field=0;field<instruction.arguments.size();++field) {
                        auto* value=values[instruction.arguments[field]];
                        ir::Type field_type=instruction.type.definition->fields[field].type;
                        guard_complete(field_type,value);
                        b.CreateStore(value,field_address(object,payload,field));
                    }
                    if(elided_values[n].contains(values.size())) { v=payload; break; }
                    std::vector<std::uint64_t> offsets; reference_offsets(object,0,offsets);
                    auto* storage=value_roots[n].at(values.size());
                    std::vector<llvm::Value*> arguments{storage,llvm::ConstantInt::get(size_type,bytes),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size()),payload};
                    if(nominal_types.contains(instruction.type.definition->identity)) arguments.push_back(b.CreateGlobalString(instruction.type.definition->identity));
                    arguments.push_back(failure);
                    std::vector<llvm::Type*> parameter_types;for(auto* arg:arguments) parameter_types.push_back(arg->getType());
                    auto abi=module->getOrInsertFunction(nominal_types.contains(instruction.type.definition->identity) ? "vd_gc_construct_typed" : "vd_gc_construct",llvm::FunctionType::get(b.getVoidTy(),parameter_types,false));
                    b.CreateCall(abi,arguments);
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Managed construction allocation failed");
                    v=b.CreateLoad(type(instruction.type),storage); break;
                }
                case Op::global_cell: {
                    auto found = std::find_if(input.globals.begin(), input.globals.end(), [&](const auto& g) { return g.identity == instruction.callee; });
                    auto* storage = value_roots[n].at(values.size());
                    b.CreateCall(module->getOrInsertFunction("vd_global_cell", b.getVoidTy(), b.getPtrTy(), size_type, b.getPtrTy(), b.getPtrTy()),
                        {storage, llvm::ConstantInt::get(size_type, found - input.globals.begin()), b.CreateGlobalString(found->identity + " declared at " + found->source.path + ":" + std::to_string(found->source.line)), failure});
                    selected_failure();
                    v = b.CreateLoad(b.getPtrTy(), storage); break;
                }
                case Op::gc_allocate: {
                    auto object = ir::Type(instruction.type.definition);
                    std::vector<std::uint64_t> offsets; reference_offsets(object,0,offsets);
                    auto* storage = value_roots[n].at(values.size());
                    b.CreateCall(module->getOrInsertFunction("vd_gc_allocate",b.getVoidTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getPtrTy()),
                        {storage,llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(type(object))),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size()),failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Managed allocation failed");
                    v = b.CreateLoad(type(instruction.type),storage); break;
                }
                case Op::gc_read: case Op::gc_write: {
                    auto object = operand_type(instruction.a);
                    if (!object.construction) fail_if(b.CreateIsNull(values[instruction.a]),"Null managed reference");
                    ir::Type field=object.definition->fields.at(static_cast<std::size_t>(instruction.immediate)).type;
                    llvm::Value* stored=nullptr;
                    if(instruction.op==Op::gc_write) {
                        stored=values[instruction.b];
                        if(contains_arrays(field)) {
                            auto* staging=copy_staging.at(field);copy_value(field,staging,value_roots[n].at(instruction.b),array_failure);
                            stored=b.CreateLoad(type(field),staging);
                        }
                        guard_complete(field,stored);
                    }
                    auto* address = field_address(object,values[instruction.a],static_cast<unsigned>(instruction.immediate));
                    if (instruction.op == Op::gc_read) v = b.CreateLoad(type(object.definition->fields.at(static_cast<std::size_t>(instruction.immediate)).type),address);
                    else v = b.CreateStore(stored,address);
                    break;
                }
                case Op::local_cleanup:{
                    auto* invoke=llvm::BasicBlock::Create(context,"local.cleanup.active",fn);
                    auto* done=llvm::BasicBlock::Create(context,"local.cleanup.done",fn);
                    b.CreateCondBr(b.CreateLoad(b.getInt1Ty(),slots[instruction.b]),invoke,done);b.SetInsertPoint(invoke);
                    b.CreateStore(b.getFalse(),slots[instruction.b]);
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* secondary=home(allocations.CreateAlloca(failure_type),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(failure_type),secondary);
                    b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_enter",b.getVoidTy()));
                    auto* target=functions.at(instruction.callee);
                    b.CreateCall(target,{b.CreateLoad(type(f.locals[instruction.a]),slots[instruction.a]),secondary});
                    b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_leave",b.getVoidTy()));
                    b.CreateStore(llvm::Constant::getNullValue(type(f.locals[instruction.a])),slots[instruction.a]);
                    b.CreateCall(module->getOrInsertFunction("vd_cleanup_failure",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{failure,secondary});
                    b.CreateBr(done);b.SetInsertPoint(done);
                    if(active_block<*f.cleanup_entry)selected_failure();
                    v=llvm::UndefValue::get(b.getInt8Ty());break;
                }
                case Op::cleanup_call:{
                    llvm::IRBuilder<> entry_builder(&*fn->getEntryBlock().begin());auto* secondary=home(entry_builder.CreateAlloca(failure_type),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(failure_type),secondary);
                    b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_enter",b.getVoidTy()));
                    b.CreateCall(callable_dispatchers.at(ir::Type::key(operand_type(instruction.a))),{values[instruction.a],secondary});
                    b.CreateCall(module->getOrInsertFunction("vd_async_cleanup_leave",b.getVoidTy()));
                    b.CreateCall(module->getOrInsertFunction("vd_cleanup_failure",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{failure,secondary});break;
                }
                case Op::function_null:v=llvm::Constant::getNullValue(type(instruction.type));break;
                case Op::function_reference:v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,b.getInt64(function_ids.at(instruction.callee)),0);if(!instruction.arguments.empty())v=b.CreateInsertValue(v,values[instruction.arguments[0]],1);break;
                case Op::panic:
                    fail_if(b.getTrue(),instruction.callee);v=llvm::UndefValue::get(b.getInt32Ty());break;
                case Op::type_test: case Op::type_cast: {
                    auto from=operand_type(instruction.a);auto* object=values[instruction.a];if(from.kind==ir::Type::interface_ref)object=b.CreateExtractValue(object,0);
                    auto* selected=b.CreateCall(module->getOrInsertFunction("vd_gc_dynamic_type",b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),size_type),{object,dynamic_keys,llvm::ConstantInt::get(size_type,keys.size())});
                    llvm::Value* matches=b.getFalse();llvm::Value* witness=llvm::ConstantPointerNull::get(b.getPtrTy());unsigned index=0;
                    for(const auto& [key,dynamic]:nominal_types){
                        if(dynamic.kind==ir::Type::managed_ref) {
                            if(instruction.type.kind==ir::Type::managed_ref && ir::subtype(dynamic,instruction.type))matches=b.CreateOr(matches,b.CreateICmpEQ(selected,b.getInt64(index)));
                            if(instruction.type.kind==ir::Type::interface_ref)for(const auto* binding:bindings)if(binding->view==dynamic && binding->interface==instruction.type){auto* found=b.CreateICmpEQ(selected,b.getInt64(index));matches=b.CreateOr(matches,found);auto* value=b.CreateCall(module->getOrInsertFunction("vd_gc_witness",b.getPtrTy(),b.getPtrTy()),{b.CreateGlobalString(binding->key)});witness=b.CreateSelect(found,value,witness);}
                        }
                        ++index;
                    }
                    if(instruction.op==Op::type_test)v=matches;else{
                        fail_if(b.CreateNot(matches),"Managed type pattern conversion failed");v=object;
                        if(instruction.type.kind==ir::Type::interface_ref){v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,object,0);v=b.CreateInsertValue(v,witness,1);}
                    }break;
                }
                case Op::pattern_equal: {
                    std::function<llvm::Value*(ir::Type,llvm::Value*,llvm::Value*)> compare;
                    compare=[&](ir::Type t,llvm::Value* left,llvm::Value* right)->llvm::Value* {
                        if(t.kind==ir::Type::aggregate) {
                            llvm::Value* equal=b.getTrue();
                            if(!t.definition->cases.empty())equal=b.CreateICmpEQ(b.CreateExtractValue(left,0),b.CreateExtractValue(right,0));
                            for(unsigned field=t.definition->cases.empty()?0u:1u;field<t.definition->fields.size();++field) {
                                llvm::BasicBlock* done=nullptr;llvm::BasicBlock* skipped=nullptr;
                                if(!t.definition->cases.empty()) {
                                    unsigned variant=0;for(;variant<t.definition->cases.size();++variant){const auto& c=t.definition->cases[variant];if(field>=c.begin && field<c.begin+c.count)break;}
                                    auto* active=llvm::BasicBlock::Create(context,"compare_active_payload",fn);done=llvm::BasicBlock::Create(context,"compare_payload_done",fn);skipped=b.GetInsertBlock();
                                    b.CreateCondBr(b.CreateAnd(equal,b.CreateICmpEQ(b.CreateExtractValue(left,0),b.getInt32(variant))),active,done);b.SetInsertPoint(active);
                                }
                                auto* next=b.CreateAnd(equal,compare(ir::field_type(t,field),b.CreateExtractValue(left,field),b.CreateExtractValue(right,field)));
                                if(done){auto* active_end=b.GetInsertBlock();b.CreateBr(done);b.SetInsertPoint(done);auto* phi=b.CreatePHI(b.getInt1Ty(),2);phi->addIncoming(equal,skipped);phi->addIncoming(next,active_end);equal=phi;}else equal=next;
                            }
                            return equal;
                        }
                        if(t.definition && t.definition->callable)return b.CreateCall(callable_comparisons.at(ir::Type::key(t)),{left,right});
                        if(t==ir::Type::string){auto* result=b.CreateCall(module->getOrInsertFunction("vd_string_equal",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{left,right,failure});array_failure();return result;}
                        if(t.kind==ir::Type::interface_ref){left=b.CreateExtractValue(left,0);right=b.CreateExtractValue(right,0);}
                        if(numeric::floating(t))return b.CreateFCmpOEQ(left,right);
                        return b.CreateICmpEQ(left,right);
                    };
                    v=compare(operand_type(instruction.a),values[instruction.a],values[instruction.b]);break;
                }
                case Op::sum: {
                    auto t=instruction.type;v=llvm::Constant::getNullValue(type(t));v=b.CreateInsertValue(v,b.getInt32(instruction.immediate),0);
                    const auto& variant=t.definition->cases.at(instruction.immediate);
                    for(unsigned field=0;field<instruction.arguments.size();++field)v=b.CreateInsertValue(v,values[instruction.arguments[field]],variant.begin+field);
                    guard_complete(t,v);break;
                }
                case Op::sum_get: {
                    auto t=operand_type(instruction.a);unsigned variant=0;
                    for(;variant<t.definition->cases.size();++variant){const auto& c=t.definition->cases[variant];if(instruction.immediate>=c.begin && instruction.immediate<c.begin+c.count)break;}
                    fail_if(b.CreateICmpNE(b.CreateExtractValue(values[instruction.a],0),b.getInt32(variant)),"Enum/result payload does not match its active variant");
                    v=b.CreateExtractValue(values[instruction.a],static_cast<unsigned>(instruction.immediate));break;
                }
                case Op::aggregate:
                    v = llvm::Constant::getNullValue(type(instruction.type));
                    for (unsigned n = 0; n < instruction.arguments.size(); ++n) v = b.CreateInsertValue(v,values[instruction.arguments[n]],{n});
                    break;
                case Op::extract: v = b.CreateExtractValue(values[instruction.a],{static_cast<unsigned>(instruction.immediate)}); break;
                case Op::insert: v = b.CreateInsertValue(values[instruction.a],values[instruction.b],{static_cast<unsigned>(instruction.immediate)}); break;
                case Op::c_primitive_call: {
                    using Shape = CPrimitiveAdmission::Shape;
                    const auto& admission = *input.c_primitives;
                    const auto ordinal = static_cast<std::uint32_t>(instruction.immediate);
                    const auto& binding = admission.bindings().at(ordinal);
                    const auto count = static_cast<std::uint32_t>(instruction.arguments.size());
                    auto* absent = llvm::ConstantPointerNull::get(b.getPtrTy());
                    const auto* resource = binding.abi_result.resource ? &binding.abi_result :
                        count && binding.abi_parameters[0].resource ? &binding.abi_parameters[0] : nullptr;
                    if (count && binding.abi_parameters.back().callback) {
                        const auto& signature = admission.callbacks().at(binding.abi_parameters.back().callback - 1);
                        const auto callback_argument = instruction.arguments.back();
                        const auto callback_type = operand_type(callback_argument);
                        auto* callback = llvm::Function::Create(llvm::FunctionType::get(b.getVoidTy(),
                            {b.getInt64Ty(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy()}, false),
                            llvm::GlobalValue::InternalLinkage, "c_callback_entry" + suffix, *module);
                        llvm::IRBuilder<> entry(llvm::BasicBlock::Create(context, "entry", callback));
                        auto* callable = entry.CreateInsertValue(llvm::UndefValue::get(type(callback_type)), callback->getArg(0), 0);
                        callable = entry.CreateInsertValue(callable, callback->getArg(1), 1);
                        std::vector<llvm::Value*> arguments{callable};
                        for (unsigned a = 0; a < signature.parameters.size(); ++a) {
                            const auto scalar = signature.parameters[a].scalar;
                            auto* word = entry.CreateLoad(entry.getInt64Ty(),
                                entry.CreateInBoundsGEP(entry.getInt64Ty(), callback->getArg(2), entry.getInt32(a)));
                            auto* raw = entry.CreateZExtOrTrunc(word,
                                entry.getIntNTy(scalar == ir::Type::boolean ? 1 : numeric::width(scalar)));
                            arguments.push_back(numeric::floating(scalar) ? entry.CreateBitCast(raw, type(scalar)) : raw);
                        }
                        arguments.push_back(callback->getArg(4));
                        llvm::Value* returned = entry.CreateCall(callable_dispatchers.at(ir::Type::key(callback_type)), arguments);
                        const auto callback_result = signature.result.scalar;
                        if (callback_result != ir::Type::none) {
                            if (numeric::floating(callback_result))
                                returned = entry.CreateBitCast(returned, entry.getIntNTy(numeric::width(callback_result)));
                            entry.CreateStore(entry.CreateZExtOrTrunc(returned, entry.getInt64Ty()), callback->getArg(3));
                        }
                        entry.CreateRetVoid();
                        auto* words = b.CreateAlloca(b.getInt64Ty(), b.getInt32(count));
                        for (unsigned a = 0; a + 1 < count; ++a) {
                            const auto arg = instruction.arguments[a];
                            b.CreateStore(bits(values[arg], operand_type(arg)),
                                b.CreateInBoundsGEP(b.getInt64Ty(), words, b.getInt32(a)));
                        }
                        b.CreateStore(b.getInt64(0), b.CreateInBoundsGEP(b.getInt64Ty(), words, b.getInt32(count - 1)));
                        const auto result = instruction.type;
                        llvm::Value* output = absent;
                        if (!resource && result != ir::Type::none) {
                            output = b.CreateAlloca(b.getInt64Ty());
                            b.CreateStore(b.getInt64(0), output);
                        }
                        auto resolve = module->getOrInsertFunction(c_dynamic_resolve, b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
                        const auto& plan = admission.dynamic()->bytes();
                        auto* thunk = b.CreateCall(resolve, {b.CreateGlobalString(plan), b.getInt32(plan.size())});
                        fail_if(b.CreateIsNull(thunk), "C dynamic Runtime binding is unavailable");
                        auto bridge = module->getOrInsertFunction(c_callback_bridge, b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getPtrTy(), b.getInt32Ty(), b.getInt32Ty(), b.getInt64Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy());
                        const auto& descriptor = admission.descriptor();
                        const auto* owner = resource ? &admission.resource(resource->resource) : nullptr;
                        auto* handle = owner ? value_roots[n].at(values.size()) : absent;
                        llvm::Value* identity = absent;
                        if (owner) identity = b.CreateGlobalString(admission.module() + "::" + owner->name);
                        auto* target = values[callback_argument];
                        auto* status = b.CreateCall(bridge, {thunk, b.CreateGlobalString(descriptor), b.getInt32(descriptor.size()),
                            b.getInt32(ordinal), words, b.getInt32(count), output, handle, identity,
                            b.getInt32(owner ? owner->release_ordinal : 0), b.getInt32(owner && owner->shutdown_owner),
                            b.CreateExtractValue(target, 0), b.CreateExtractValue(target, 1), callback,
                            b.getInt32(signature.parameters.size()), b.getInt32(callback_result != ir::Type::none), failure});
                        selected_failure();
                        fail_if(b.CreateICmpEQ(status, b.getInt32(3)), "C callback resource requires its owning thread");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(6)), "C callback provider returned a duplicate native owner");
                        fail_if(b.CreateIsNotNull(status), "C callback transport rejected " + instruction.callee);
                        if (owner) v = b.CreateLoad(b.getPtrTy(), handle);
                        else if (result == ir::Type::none) v = llvm::UndefValue::get(b.getInt8Ty());
                        else {
                            auto* raw = b.CreateLoad(b.getInt64Ty(), output);
                            const auto width = result == ir::Type::boolean ? 1 : numeric::width(result);
                            if (width < 64) fail_if(b.CreateIsNotNull(b.CreateLShr(raw, width)), "noncanonical C callback result");
                            v = b.CreateZExtOrTrunc(raw, b.getIntNTy(width));
                            if (numeric::floating(result)) v = b.CreateBitCast(v, type(result));
                        }
                        break;
                    }
                    if (resource && (admission.revision() < 4 || resource->role == CResourceRole::close)) {
                        auto* words = b.CreateAlloca(b.getInt64Ty(), b.getInt32(count ? count : 1));
                        const bool acquire = resource->role == CResourceRole::acquire;
                        for (unsigned a = 0; a < count; ++a) {
                            const auto argument = instruction.arguments[a];
                            auto* word = !acquire && a == 0 ? b.getInt64(0) : bits(values[argument], operand_type(argument));
                            b.CreateStore(word, b.CreateInBoundsGEP(b.getInt64Ty(), words, b.getInt32(a)));
                        }
                        const auto result_type = instruction.type;
                        const auto& info = admission.resource(resource->resource);
                        auto* handle = value_roots[n].at(acquire ? values.size() : instruction.arguments[0]);
                        llvm::Value* scalar = acquire || result_type == ir::Type::none ?
                            static_cast<llvm::Value*>(absent) : b.CreateAlloca(b.getInt64Ty());
                        if (scalar != absent) b.CreateStore(b.getInt64(0), scalar);
                        auto* bytes = b.CreateGlobalString(admission.descriptor());
                        llvm::Value* transport;
                        if (const auto& plan = admission.dynamic()) {
                            auto resolve = module->getOrInsertFunction(c_dynamic_resolve, b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
                            transport = b.CreateCall(resolve, {b.CreateGlobalString(plan->bytes()), b.getInt32(plan->bytes().size())});
                            fail_if(b.CreateIsNull(transport), "C dynamic Runtime binding is unavailable");
                        } else {
                            transport = module->getOrInsertFunction(admission.transport(), b.getInt32Ty(), b.getPtrTy(),
                                b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy()).getCallee();
                        }
                        auto bridge = module->getOrInsertFunction(info.shutdown_owner ? c_owner_resource_bridge : c_resource_bridge,
                            b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getPtrTy(), b.getInt32Ty(), b.getInt32Ty());
                        auto* status = b.CreateCall(bridge, {transport, bytes, b.getInt32(admission.descriptor().size()),
                            b.getInt32(ordinal), words, b.getInt32(count), scalar, handle,
                            b.CreateGlobalString(admission.module() + "::" + info.name), b.getInt32(info.release_ordinal),
                            b.getInt32(static_cast<std::uint32_t>(resource->role))});
                        fail_if(b.CreateICmpEQ(status, b.getInt32(1)), "C resource is closed, null or invalid");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(2)), "C resource belongs to another provider or contract");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(3)), "C resource requires its owning thread");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(6)), "C provider returned a duplicate native owner");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(7)), "C callback cannot reenter its own registration");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(8)), "Retained C callback failed");
                        fail_if(b.CreateIsNotNull(status), "C resource operation rejected " + instruction.callee);
                        if (acquire) v = b.CreateLoad(b.getPtrTy(), handle);
                        else if (result_type == ir::Type::none) v = llvm::UndefValue::get(b.getInt8Ty());
                        else {
                            auto* raw = b.CreateLoad(b.getInt64Ty(), scalar);
                            const unsigned width = result_type == ir::Type::boolean ? 1 : numeric::width(result_type);
                            if (width < 64)
                                fail_if(b.CreateIsNotNull(b.CreateLShr(raw, width)), "noncanonical C resource result");
                            v = b.CreateZExtOrTrunc(raw, b.getIntNTy(width));
                            if (numeric::floating(result_type)) v = b.CreateBitCast(v, type(result_type));
                        }
                        break;
                    }
                    auto scratch = [&](unsigned bytes) {
                        auto* buffer = b.CreateAlloca(b.getInt64Ty(), b.getInt32((bytes + 7) / 8));
                        b.CreateMemSet(buffer, b.getInt8(0), bytes, llvm::Align(8));
                        return buffer;
                    };
                    auto read_bits = [&](llvm::Value* raw, ir::Type scalar) -> llvm::Value* {
                        const unsigned width = scalar == ir::Type::boolean ? 1 : numeric::width(scalar);
                        if (scalar == ir::Type::boolean)
                            fail_if(b.CreateICmpUGT(raw, llvm::ConstantInt::get(raw->getType(), 1)),
                                "noncanonical C Boolean field");
                        auto* narrowed = b.CreateZExtOrTrunc(raw, b.getIntNTy(width));
                        return numeric::floating(scalar) ? b.CreateBitCast(narrowed, type(scalar)) : narrowed;
                    };
                    std::function<void(const Shape&,ir::Type,llvm::Value*,llvm::Value*,bool)> transfer;
                    transfer = [&](const Shape& shape, ir::Type source_type, llvm::Value* value,
                                   llvm::Value* native, bool into_c) {
                        if (shape.record) {
                            const auto& record = admission.record(shape.record);
                            if (shape.extent) {
                                const auto child = ir::array_element(source_type);
                                auto element = shape; element.extent = 0;
                                auto* element_info = element_record(child);
                                if (!into_c) {
                                    b.CreateCall(module->getOrInsertFunction("vd_value_array_new", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(), b.getPtrTy()),
                                        {value, element_info, b.getInt64(shape.extent), b.getInt32(0), failure});
                                    selected_failure();
                                }
                                for (unsigned i = 0; i < shape.extent; ++i) {
                                    auto* temporary = b.CreateAlloca(type(child));
                                    if (into_c) {
                                        b.CreateCall(module->getOrInsertFunction("vd_value_array_read", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(), b.getInt64Ty(), b.getPtrTy()),
                                            {temporary, value, element_info, b.getInt64(shape.extent), b.getInt32(0), b.getInt64(i), failure});
                                        selected_failure();
                                    }
                                    transfer(element, child, temporary, b.CreateInBoundsGEP(b.getInt8Ty(), native, b.getInt32(i * record.size)), into_c);
                                    if (!into_c) {
                                        b.CreateCall(module->getOrInsertFunction("vd_value_array_write", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(), b.getInt64Ty(), b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy()),
                                            {value, element_info, b.getInt64(shape.extent), b.getInt32(0), b.getInt64(i), temporary, b.getInt64(0), b.getInt32(0), b.getInt32(1), failure});
                                        selected_failure();
                                    }
                                }
                                if (!into_c) {
                                    b.CreateCall(module->getOrInsertFunction("vd_value_array_complete", b.getVoidTy(), b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getPtrTy()),
                                        {value, element_info, b.getInt64(shape.extent), failure});
                                    selected_failure();
                                }
                                return;
                            }
                            for (unsigned i = 0; i < record.fields.size(); ++i) {
                                const auto& field = record.fields[i];
                                transfer(field.shape, ir::field_type(source_type, i),
                                    b.CreateStructGEP(type(source_type), value, i),
                                    b.CreateInBoundsGEP(b.getInt8Ty(), native, b.getInt32(field.offset)), into_c);
                            }
                            return;
                        }
                        const unsigned bytes = CPrimitiveAdmission::scalar_bytes(shape.scalar);
                        auto* raw_type = b.getIntNTy(bytes * 8);
                        if (!shape.extent) {
                            if (into_c) {
                                auto* word = bits(b.CreateLoad(type(shape.scalar), value), shape.scalar);
                                b.CreateAlignedStore(b.CreateZExtOrTrunc(word, raw_type), native, llvm::Align(bytes));
                            } else {
                                auto* word = b.CreateAlignedLoad(raw_type, native, llvm::Align(bytes));
                                b.CreateStore(read_bits(word, shape.scalar), value);
                            }
                            return;
                        }
                        const auto encoding = b.getInt32(scalar_encoding(shape.scalar));
                        const auto extent = b.getInt64(shape.extent);
                        if (!into_c) {
                            b.CreateCall(module->getOrInsertFunction("vd_array_default", b.getVoidTy(),
                                b.getPtrTy(), b.getInt32Ty(), b.getInt64Ty(), b.getInt64Ty(), b.getPtrTy()),
                                {value, encoding, extent, b.getInt64(0), failure});
                            selected_failure();
                        }
                        for (unsigned i = 0; i < shape.extent; ++i) {
                            auto* position = b.CreateInBoundsGEP(b.getInt8Ty(), native, b.getInt32(i * bytes));
                            if (into_c) {
                                auto* word = b.CreateCall(module->getOrInsertFunction("vd_array_read", b.getInt64Ty(),
                                    b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(), b.getInt64Ty(), b.getPtrTy()),
                                    {value, b.getInt64(i), encoding, extent, failure});
                                selected_failure();
                                b.CreateAlignedStore(b.CreateZExtOrTrunc(word, raw_type), position, llvm::Align(bytes));
                            } else {
                                auto* raw = b.CreateAlignedLoad(raw_type, position, llvm::Align(bytes));
                                auto* word = bits(read_bits(raw, shape.scalar), shape.scalar);
                                b.CreateCall(module->getOrInsertFunction("vd_array_write", b.getVoidTy(),
                                    b.getPtrTy(), b.getInt64Ty(), b.getInt64Ty(), b.getInt32Ty(), b.getInt64Ty(), b.getPtrTy()),
                                    {value, b.getInt64(i), word, encoding, extent, failure});
                                selected_failure();
                            }
                        }
                    };
                    llvm::Value* words = count ? static_cast<llvm::Value*>(scratch(count * 8)) : absent;
                    unsigned span_count = 0;
                    unsigned resource_count = 0;
                    llvm::Value* resource_arguments = absent;
                    llvm::StructType* resource_argument_type = nullptr;
                    llvm::Value* spans = admission.revision() >= 2 ? absent : nullptr;
                    llvm::StructType* span_type = nullptr;
                    for (unsigned a = 0; a < count; ++a) {
                        const auto argument = instruction.arguments[a];
                        const auto& shape = binding.abi_parameters[a];
                        if (shape.resource) {
                            if (!resource_argument_type) {
                                resource_argument_type = llvm::StructType::get(b.getPtrTy(), b.getPtrTy(),
                                    b.getInt32Ty(), b.getInt32Ty(), b.getInt32Ty());
                                const auto* geometry = layout.getStructLayout(resource_argument_type);
                                if (geometry->getSizeInBytes() != sizeof(CResourceArgument) ||
                                    geometry->getElementOffset(1) != offsetof(CResourceArgument, identity) ||
                                    geometry->getElementOffset(2) != offsetof(CResourceArgument, argument) ||
                                    geometry->getElementOffset(3) != offsetof(CResourceArgument, release) ||
                                    geometry->getElementOffset(4) != offsetof(CResourceArgument, shutdown_owner))
                                    throw std::invalid_argument("C resource argument ABI mismatch");
                                resource_arguments = b.CreateAlloca(resource_argument_type, b.getInt32(count));
                            }
                            const auto& owner = admission.resource(shape.resource);
                            auto* row = b.CreateInBoundsGEP(resource_argument_type, resource_arguments, b.getInt32(resource_count++));
                            llvm::Value* fields[] = {value_roots[n].at(argument),
                                b.CreateGlobalString(admission.module() + "::" + owner.name), b.getInt32(a),
                                b.getInt32(owner.release_ordinal), b.getInt32(owner.shutdown_owner)};
                            for (unsigned i = 0; i < 5; ++i)
                                b.CreateStore(fields[i], b.CreateStructGEP(resource_argument_type, row, i));
                            b.CreateStore(b.getInt64(0), b.CreateInBoundsGEP(b.getInt64Ty(), words, b.getInt32(a)));
                            continue;
                        }
                        if (shape.direction) {
                            if (!span_type) {
                                span_type = llvm::StructType::get(b.getPtrTy(), b.getInt64Ty(), b.getInt32Ty(),
                                    b.getInt32Ty(), b.getInt32Ty(), b.getInt32Ty());
                                const auto* geometry = layout.getStructLayout(span_type);
                                if (geometry->getSizeInBytes() != sizeof(CSpanRequest) ||
                                    geometry->getElementOffset(1) != offsetof(CSpanRequest, limit) ||
                                    geometry->getElementOffset(2) != offsetof(CSpanRequest, argument) ||
                                    geometry->getElementOffset(3) != offsetof(CSpanRequest, direction) ||
                                    geometry->getElementOffset(4) != offsetof(CSpanRequest, offset_argument) ||
                                    geometry->getElementOffset(5) != offsetof(CSpanRequest, length_argument))
                                    throw std::invalid_argument("C span request ABI mismatch");
                                spans = b.CreateAlloca(span_type, b.getInt32(2));
                            }
                            auto* row = b.CreateInBoundsGEP(span_type, spans, b.getInt32(span_count++));
                            llvm::Value* fields[] = {value_roots[n].at(argument), b.getInt64(shape.limit),
                                b.getInt32(a), b.getInt32(shape.direction),
                                b.getInt32(shape.offset_argument), b.getInt32(shape.length_argument)};
                            for (unsigned i = 0; i < 6; ++i)
                                b.CreateStore(fields[i], b.CreateStructGEP(span_type, row, i));
                            continue;
                        }
                        llvm::Value* word;
                        if (shape.record) {
                            auto* storage = value_roots[n].at(argument);
                            if (!storage) {
                                storage = b.CreateAlloca(type(operand_type(argument)));
                                b.CreateStore(values[argument], storage);
                            }
                            auto* native = scratch(admission.record(shape.record).size);
                            transfer(shape, operand_type(argument), storage, native, true);
                            word = b.CreatePtrToInt(native, b.getInt64Ty());
                        } else word = bits(values[argument], operand_type(argument));
                        b.CreateStore(word, b.CreateInBoundsGEP(b.getInt64Ty(), words, b.getInt32(a)));
                    }
                    const auto result_type = instruction.type;
                    const auto& result_shape = binding.abi_result;
                    llvm::Value* output = absent;
                    if (result_type != ir::Type::none && !result_shape.resource)
                        output = scratch(result_shape.record ? admission.record(result_shape.record).size : 8);
                    llvm::CallInst* status;
                    if (resource_count || result_shape.resource) {
                        const auto& plan = admission.dynamic();
                        if (!plan) throw std::invalid_argument("Mixed resource ABI requires a conventional provider");
                        auto resolve = module->getOrInsertFunction(c_dynamic_resolve, b.getPtrTy(), b.getPtrTy(), b.getInt32Ty());
                        auto* thunk = b.CreateCall(resolve, {b.CreateGlobalString(plan->bytes()), b.getInt32(plan->bytes().size())});
                        fail_if(b.CreateIsNull(thunk), "C dynamic Runtime binding is unavailable");
                        const auto* owner = result_shape.resource ? &admission.resource(result_shape.resource) : nullptr;
                        auto bridge = module->getOrInsertFunction(c_resources_bridge, b.getInt32Ty(),
                            b.getPtrTy(), b.getPtrTy(), b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty(),
                            b.getPtrTy(), b.getPtrTy(), b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(),
                            b.getInt32Ty(), b.getInt32Ty(), b.getPtrTy(), b.getInt32Ty());
                        status = b.CreateCall(bridge, {thunk, b.CreateGlobalString(admission.descriptor()),
                            b.getInt32(admission.descriptor().size()), b.getInt32(ordinal), words, b.getInt32(count), output,
                            resource_arguments, b.getInt32(resource_count), owner ? value_roots[n].at(values.size()) : absent,
                            owner ? static_cast<llvm::Value*>(b.CreateGlobalString(admission.module() + "::" + owner->name)) : absent,
                            b.getInt32(owner ? owner->release_ordinal : 0), b.getInt32(owner && owner->shutdown_owner),
                            spans, b.getInt32(span_count)});
                    } else status = call_c_primitive(b, *module, admission, ordinal, words, count, output, spans, span_count);
                    if (span_count) {
                        fail_if(b.CreateICmpEQ(status, b.getInt32(3)), "C byte span outside admitted extent");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(4)), "overlapping C byte spans");
                        fail_if(b.CreateICmpEQ(status, b.getInt32(5)), "C byte span returned an oversized written prefix");
                    }
                    fail_if(b.CreateIsNotNull(status), "C primitive transport rejected " + instruction.callee);
                    if (result_type == ir::Type::none) {
                        v = llvm::UndefValue::get(b.getInt8Ty());
                    } else if (result_shape.resource) {
                        v = b.CreateLoad(b.getPtrTy(), value_roots[n].at(values.size()));
                    } else if (result_shape.record) {
                        auto* storage = value_roots[n].at(values.size());
                        if (!storage) storage = b.CreateAlloca(type(result_type));
                        transfer(result_shape, result_type, storage, output, false);
                        v = b.CreateLoad(type(result_type), storage);
                    } else {
                        auto* raw = b.CreateLoad(b.getInt64Ty(), output);
                        const unsigned width = result_type == ir::Type::boolean ? 1 : numeric::width(result_type);
                        if (width < 64)
                            fail_if(b.CreateIsNotNull(b.CreateLShr(raw, width)), "noncanonical C primitive result");
                        v = read_bits(raw, result_type);
                    }
                    break;
                }
                case Op::native_call: {
                    unsigned op=native_operation(instruction.callee);
                    llvm::Value* bits=b.getInt64(0);llvm::Value* object=llvm::ConstantPointerNull::get(b.getPtrTy());llvm::Value* text=object;llvm::Value* environment=object;llvm::Value* entry=object;llvm::Value* function=b.getInt64(0);
                    for(auto id:instruction.arguments){auto t=operand_type(id);auto* argument=values[id];
                        if(t==ir::Type::string)text=argument;
                        else if(t.kind==ir::Type::managed_ref)object=argument;
                        else if(t.definition && t.definition->callable){
                            function=b.CreateExtractValue(argument,0);environment=b.CreateExtractValue(argument,1);
                            auto* callback=llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(),{b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()},false),llvm::GlobalValue::InternalLinkage,"native.callback"+suffix,*module);
                            llvm::IRBuilder<> cb(context);cb.SetInsertPoint(llvm::BasicBlock::Create(context,"entry",callback));llvm::Value* callable=llvm::UndefValue::get(type(t));callable=cb.CreateInsertValue(callable,callback->getArg(0),0);callable=cb.CreateInsertValue(callable,callback->getArg(1),1);cb.CreateRet(cb.CreateCall(callable_dispatchers.at(ir::Type::key(t)),{callable,callback->getArg(2),callback->getArg(3)}));
#ifdef VOID_NATIVE_CALLBACK_SANITIZER_HASH
                            callback->setMetadata("func_sanitize",llvm::MDNode::get(context,{llvm::ConstantAsMetadata::get(b.getInt32(VOID_NATIVE_CALLBACK_SANITIZER_MAGIC)),llvm::ConstantAsMetadata::get(b.getInt32(VOID_NATIVE_CALLBACK_SANITIZER_HASH))}));
#endif
                            entry=callback;
                        }else {if(t==ir::Type::f32)argument=b.CreateBitCast(argument,b.getInt32Ty());else if(t==ir::Type::f64)argument=b.CreateBitCast(argument,b.getInt64Ty());bits=b.CreateZExtOrTrunc(argument,b.getInt64Ty());}
                    }
                    llvm::Value* output=llvm::ConstantPointerNull::get(b.getPtrTy());if(instruction.type==ir::Type::string || instruction.type.kind==ir::Type::managed_ref)output=value_roots[n].at(values.size());
                    auto* result=b.CreateCall(module->getOrInsertFunction(native_provider(instruction.callee)=="void.fixture.v1"?"vd_native_fixture_call":"vd_native_sink_call",b.getInt64Ty(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{b.getInt32(op),bits,object,text,function,environment,entry,output,b.CreateGlobalString(op==14?instruction.type.definition->identity:""),failure});
                    auto* bad=llvm::BasicBlock::Create(context,"native.failure",fn);auto* okay=llvm::BasicBlock::Create(context,"native.valid",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,okay);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(okay);
                    if(instruction.type==ir::Type::none)v=llvm::UndefValue::get(b.getInt8Ty());else if(instruction.type==ir::Type::string || instruction.type.kind==ir::Type::managed_ref)v=b.CreateLoad(type(instruction.type),output);else if(instruction.type==ir::Type::f32)v=b.CreateBitCast(b.CreateTrunc(result,b.getInt32Ty()),b.getFloatTy());else if(instruction.type==ir::Type::f64)v=b.CreateBitCast(result,b.getDoubleTy());else v=b.CreateZExtOrTrunc(result,type(instruction.type));break;
                }
                case Op::async_cancel_children: v=b.CreateCall(module->getOrInsertFunction("vd_async_cancel_children",b.getVoidTy()));break;
                case Op::async_cancel:
                    b.CreateCall(module->getOrInsertFunction("vd_async_cancel",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{values[instruction.a],failure});
                    {auto* bad=llvm::BasicBlock::Create(context,"async.cancel_failure",fn);auto* good=llvm::BasicBlock::Create(context,"async.cancel_valid",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,good);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(good);}v=llvm::UndefValue::get(b.getInt8Ty());break;
                case Op::async_group_enter:
                    v=b.CreateCall(module->getOrInsertFunction("vd_async_group_enter",b.getInt64Ty(),b.getPtrTy()),{failure});
                    {auto* bad=llvm::BasicBlock::Create(context,"async.group_failure",fn);auto* good=llvm::BasicBlock::Create(context,"async.group_valid",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,good);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(good);}break;
                case Op::async_group_join: case Op::async_drain: {
                    auto* check=llvm::BasicBlock::Create(context,"async.join",fn);b.CreateBr(check);resume_states->addCase(b.getInt32(++resume_serial),check);b.SetInsertPoint(check);
                    for(unsigned k=0;k<values.size();++k)if(value_types[k]!=ir::Type::none)values[k]=b.CreateLoad(type(value_types[k]),frame_home(async->values[n][k]));
                    llvm::Value* ready=instruction.op==Op::async_drain?
                        b.CreateCall(module->getOrInsertFunction("vd_async_drain",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy()),{fn->getArg(0),failure}):
                        b.CreateCall(module->getOrInsertFunction("vd_async_group_join",b.getInt1Ty(),b.getInt64Ty(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy()),{values[instruction.a],b.getInt32(instruction.immediate),fn->getArg(0),failure});
                    auto* suspend=llvm::BasicBlock::Create(context,"async.join_suspend",fn);auto* done=llvm::BasicBlock::Create(context,"async.join_complete",fn);b.CreateCondBr(ready,done,suspend);b.SetInsertPoint(suspend);b.CreateStore(b.getInt32(resume_serial),frame_home(1));leave_frame();b.CreateRetVoid();b.SetInsertPoint(done);
                    if(instruction.op!=Op::async_drain){auto* bad=llvm::BasicBlock::Create(context,"async.join_failure",fn);auto* good=llvm::BasicBlock::Create(context,"async.join_valid",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,good);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(good);}v=llvm::UndefValue::get(b.getInt8Ty());break;
                }
                case Op::await: {
                    auto* check=llvm::BasicBlock::Create(context,"async.await",fn);b.CreateBr(check);resume_states->addCase(b.getInt32(++resume_serial),check);b.SetInsertPoint(check);
                    for(unsigned k=0;k<values.size();++k)if(value_types[k]!=ir::Type::none)values[k]=b.CreateLoad(type(value_types[k]),frame_home(async->values[n][k]));
                    auto* task=values[instruction.a];auto task_type=operand_type(instruction.a);
                    auto* ready=b.CreateCall(module->getOrInsertFunction("vd_async_ready",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy()),{task,failure});
                    auto propagate=[&]{auto* bad=llvm::BasicBlock::Create(context,"async.await_failure",fn);auto* okay=llvm::BasicBlock::Create(context,"async.await_valid",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),bad,okay);b.SetInsertPoint(bad);trace_failure();b.SetInsertPoint(okay);};propagate();
                    auto* suspend=llvm::BasicBlock::Create(context,"async.suspend",fn);auto* completed=llvm::BasicBlock::Create(context,"async.completed",fn);b.CreateCondBr(ready,completed,suspend);b.SetInsertPoint(suspend);
                    b.CreateStore(b.getInt32(resume_serial),frame_home(1));b.CreateCall(module->getOrInsertFunction("vd_async_suspend",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{fn->getArg(0),task,failure});propagate();leave_frame();b.CreateRetVoid();b.SetInsertPoint(completed);
                    auto result=instruction_types[n].at(values.size());auto* address=field_address(task_type,task,1);
                    if(result==ir::Type::none)v=llvm::UndefValue::get(b.getInt8Ty());
                    else if(contains_arrays(result)){auto* source=value_roots[n].at(values.size());b.CreateStore(b.CreateLoad(type(result),address),source);auto* destination=copy_staging.at(result);copy_value(result,destination,source,propagate);v=b.CreateLoad(type(result),destination);}
                    else v=b.CreateLoad(type(result),address);
                    break;
                }
                case Op::argument:
                    if(async)v=b.CreateLoad(type(f.parameters[instruction.a]),frame_home(async->parameters[instruction.a]));
                    else if (f.receiver && f.parameters[0].kind==ir::Type::aggregate && instruction.a == 0) v = b.CreateLoad(type(f.parameters[0]),fn->getArg(0));
                    else v = fn->getArg(instruction.a);
                    break;
                case Op::collection_enter: case Op::collection_leave: case Op::collection_append: case Op::collection_finish: {
                    if(instruction.op==Op::collection_finish){auto* storage=value_roots[n].at(values.size());b.CreateCall(module->getOrInsertFunction("vd_collection_finish",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{storage,failure});fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Collection display failed");v=b.CreateLoad(b.getPtrTy(),storage);}
                    else {auto name=instruction.op==Op::collection_enter?"vd_collection_enter":instruction.op==Op::collection_leave?"vd_collection_leave":"vd_collection_append";auto* call=b.CreateCall(module->getOrInsertFunction(name,instruction.op==Op::collection_enter?b.getInt1Ty():b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{values[instruction.a],failure});fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Collection display failed");if(instruction.op==Op::collection_enter)v=call;}
                    break;
                }
                case Op::string_join_encode:
                case Op::string_join: {
                    auto* storage=value_roots[n].at(values.size());
                    auto* pointers=llvm::ArrayType::get(b.getPtrTy(),instruction.arguments.size());
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* inputs=home(allocations.CreateAlloca(pointers),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    for(unsigned k=0;k<instruction.arguments.size();++k)b.CreateStore(value_roots[n].at(instruction.arguments[k]),b.CreateInBoundsGEP(pointers,inputs,{b.getInt32(0),b.getInt32(k)}));
                    const bool fused=instruction.op==Op::string_join_encode;
                    auto* phase=b.CreateCall(module->getOrInsertFunction(fused?"vd_string_join_encode":"vd_string_join",fused?b.getInt1Ty():b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy()),{storage,inputs,llvm::ConstantInt::get(size_type,instruction.arguments.size()),failure});
                    auto* failed=b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure));
                    if(fused)fail_if(b.CreateAnd(failed,phase),"UTF-8 text operation failed");
                    fail_if(failed,"Interpolation failed");v=b.CreateLoad(b.getPtrTy(),storage);break;
                }
                case Op::format_unwrap: {
                    auto result=values[instruction.a];auto* bad=llvm::BasicBlock::Create(context,"format_rejected",fn);auto* ready=llvm::BasicBlock::Create(context,"format_accepted",fn);
                    b.CreateCondBr(b.CreateICmpEQ(b.CreateExtractValue(result,0),b.getInt32(0)),ready,bad);b.SetInsertPoint(bad);
                    b.CreateCall(module->getOrInsertFunction("vd_display_error",b.getVoidTy(),b.getPtrTy(),b.getPtrTy()),{b.CreateExtractValue(result,2),failure});trace_failure();b.SetInsertPoint(ready);v=b.CreateExtractValue(result,1);break;
                }
                case Op::console_endpoint:
                    v=b.CreateInsertValue(llvm::Constant::getNullValue(type(instruction.type)),b.getInt64(instruction.immediate),0);break;
                case Op::console_print: {
                    auto* role=instruction.arguments.empty()?static_cast<llvm::Value*>(b.getInt64(1)):b.CreateExtractValue(values[instruction.arguments[0]],0);
                    b.CreateCall(module->getOrInsertFunction("vd_console_print",b.getVoidTy(),b.getPtrTy(),b.getInt64Ty(),b.getInt1Ty(),b.getPtrTy()),{values[instruction.a],role,values[instruction.b],failure});
                    selected_failure();v=llvm::UndefValue::get(b.getInt8Ty());break;
                }
#include "filesystem.inc"
#include "url.inc"
#include "http.inc"
#include "os.inc"
#include "host.inc"
                case Op::console_native: {
                    auto* storage=value_roots[n].at(values.size());b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),storage);
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());auto* words_type=llvm::ArrayType::get(b.getInt64Ty(),8);
                    auto* words=home(allocations.CreateAlloca(words_type),StorageRole::known_abi,llvm_backend::HomeKind::value);b.CreateStore(llvm::Constant::getNullValue(words_type),words);
                    auto* text=b.CreateStructGEP(type(instruction.type),storage,6);auto* bytes=b.CreateStructGEP(type(instruction.type),storage,7);
                    llvm::Value* data=llvm::ConstantPointerNull::get(b.getPtrTy());llvm::Value* count=b.getInt64(0);llvm::Value* timeout=b.getInt64(-1);
                    auto operation=instruction.immediate;
                    if(operation<=1 || operation==16)data=value_roots[n].at(instruction.arguments[1]);
                    if(operation==2 || operation==4)timeout=values[instruction.arguments[1]];
                    if(operation==3 || operation==5){count=values[instruction.arguments[1]];timeout=values[instruction.arguments[2]];}
                    if(operation==11 || operation==13 || operation==18){count=values[instruction.arguments[1]];timeout=values[instruction.arguments[2]];}
                    if(operation==12 || operation==20)count=values[instruction.arguments[1]];
                    if(operation>=21){fail_if(b.CreateIsNull(values[instruction.arguments[0]]),"Invalid terminal session");count=b.CreateLoad(b.getInt64Ty(),field_address(operand_type(instruction.arguments[0]),values[instruction.arguments[0]],0));}
                    llvm::Value* role=operation>=21?static_cast<llvm::Value*>(b.getInt64(0)):b.CreateExtractValue(values[instruction.arguments[0]],0);
                    b.CreateCall(module->getOrInsertFunction("vd_console_v2",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy(),b.getInt64Ty(),b.getInt64Ty(),b.getPtrTy()),{text,bytes,words,b.getInt32(operation),role,data,count,timeout,failure});
                    selected_failure();
                    for(unsigned k=0;k<6;++k){llvm::Value* word=b.CreateLoad(b.getInt64Ty(),b.CreateInBoundsGEP(words_type,words,{b.getInt32(0),b.getInt32(k)}));if(k==5)word=b.CreateTrunc(word,b.getInt1Ty());b.CreateStore(word,b.CreateStructGEP(type(instruction.type),storage,k));}
                    for(unsigned k=6;k<8;++k)b.CreateStore(b.CreateLoad(b.getInt64Ty(),b.CreateInBoundsGEP(words_type,words,{b.getInt32(0),b.getInt32(k)})),b.CreateStructGEP(type(instruction.type),storage,k+2));
                    v=b.CreateLoad(type(instruction.type),storage);break;
                }
                case Op::result_unwrap: {
                    auto input = values[instruction.a];
                    fail_if(b.CreateICmpNE(b.CreateExtractValue(input, 0), b.getInt32(0)),
                            "convert.unwrap received Err; inspect the result or use unwrap_or");
                    v = b.CreateExtractValue(input, 1);
                    break;
                }
                case Op::scalar_parse: {
                    auto value_type = ir::field_type(instruction.type, 1);
                    const bool enumeration = value_type.kind == ir::Type::aggregate;
                    const auto count = enumeration ? value_type.definition->cases.size() : 0;
                    auto* cases_type = llvm::ArrayType::get(b.getPtrTy(), count);
                    std::vector<llvm::Constant*> cases;
                    if (enumeration) {
                        for (const auto& variant : value_type.definition->cases) {
                            cases.push_back(b.CreateGlobalString(value_type.definition->identity + "." + variant.name));
                        }
                    }
                    auto* names = new llvm::GlobalVariable(*module, cases_type, true,
                        llvm::GlobalValue::PrivateLinkage, llvm::ConstantArray::get(cases_type, cases),
                        "convert.enum_cases");
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* word = home(allocations.CreateAlloca(b.getInt64Ty()),
                                      StorageRole::known_abi, llvm_backend::HomeKind::value);
                    b.CreateStore(b.getInt64(0), word);
                    auto* rejected = b.CreateCall(module->getOrInsertFunction("vd_convert_parse",
                        b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), b.getInt32Ty(),
                        b.getPtrTy(), size_type, b.getPtrTy()),
                        {word, values[instruction.a], b.getInt32(enumeration ? 11 : scalar_encoding(value_type)),
                         names, llvm::ConstantInt::get(size_type, count), failure});
                    array_failure();
                    auto* raw = b.CreateLoad(b.getInt64Ty(), word);
                    llvm::Value* parsed;
                    if (enumeration) {
                        parsed = b.CreateInsertValue(llvm::Constant::getNullValue(type(value_type)),
                                                     b.CreateTrunc(raw, b.getInt32Ty()), 0);
                    } else if (numeric::floating(value_type)) {
                        parsed = b.CreateBitCast(b.CreateZExtOrTrunc(raw,
                            b.getIntNTy(numeric::width(value_type))), type(value_type));
                    } else {
                        parsed = b.CreateZExtOrTrunc(raw, type(value_type));
                    }
                    auto* valid = b.CreateICmpEQ(rejected, b.getInt32(0));
                    v = llvm::Constant::getNullValue(type(instruction.type));
                    v = b.CreateInsertValue(v, b.CreateSelect(valid, b.getInt32(0), b.getInt32(1)), 0);
                    v = b.CreateInsertValue(v, b.CreateSelect(valid, parsed,
                        llvm::Constant::getNullValue(type(value_type))), 1);
                    v = b.CreateInsertValue(v, rejected, 2);
                    break;
                }
                case Op::text_translate: {
                    auto* storage = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), storage);
                    auto* payload = b.CreateStructGEP(type(instruction.type), storage, 1);
                    const auto& args = instruction.arguments;
                    auto* status = b.CreateCall(module->getOrInsertFunction("vd_text_translate", b.getInt32Ty(),
                        b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getPtrTy()),
                        {payload, value_roots[n].at(args[0]), value_roots[n].at(args[1]),
                         value_roots[n].at(args[2]), element_record(ir::Type::string), values[args[3]], failure});
                    array_failure();
                    v = b.CreateLoad(type(instruction.type), storage);
                    v = b.CreateInsertValue(v, b.CreateZExt(b.CreateICmpNE(status, b.getInt32(0)), b.getInt32Ty()), 0);
                    v = b.CreateInsertValue(v, status, 2);
                    break;
                }
                case Op::text_case: {
                    auto* storage = value_roots[n].at(values.size());
                    b.CreateCall(module->getOrInsertFunction("vd_text_case", b.getVoidTy(),
                        b.getPtrTy(), b.getPtrTy(), b.getInt1Ty(), b.getPtrTy()),
                        {storage, values[instruction.a], values[instruction.b], failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(), failure)),
                            "Unicode casing failed");
                    v = b.CreateLoad(b.getPtrTy(), storage);
                    break;
                }
                case Op::memory_write: {
                    b.CreateCall(module->getOrInsertFunction("vd_list_write_bytes", b.getVoidTy(),
                        b.getPtrTy(), b.getPtrTy(), b.getInt64Ty(), b.getPtrTy()),
                        {value_roots[n].at(instruction.arguments[0]),
                         value_roots[n].at(instruction.arguments[1]),
                         values[instruction.arguments[2]], failure});
                    array_failure();
                    v = llvm::UndefValue::get(b.getInt8Ty());
                    break;
                }
                case Op::temporal_result: {
                    auto input = values[instruction.a];
                    auto* storage = value_roots[n].at(values.size());
                    b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)), storage);
                    auto* rejected = llvm::BasicBlock::Create(context, "temporal.error_text", fn);
                    auto* ready = llvm::BasicBlock::Create(context, "temporal.display_ready", fn);
                    auto* tag = b.CreateExtractValue(input, 0);
                    b.CreateStore(tag, b.CreateStructGEP(type(instruction.type), storage, 0));
                    b.CreateStore(b.CreateExtractValue(input, 1),
                                  b.CreateStructGEP(type(instruction.type), storage, 1));
                    b.CreateCondBr(b.CreateICmpEQ(tag, b.getInt32(0)), ready, rejected);
                    b.SetInsertPoint(rejected);
                    auto* reason = b.CreateExtractValue(input, 2);
                    fail_if(b.CreateIsNull(reason), "Invalid temporal error payload");
                    auto error_type = ir::field_type(operand_type(instruction.a), 2);
                    auto* message = b.CreateLoad(b.getPtrTy(), field_address(error_type, reason, 0));
                    b.CreateStore(message, b.CreateStructGEP(type(instruction.type), storage, 2));
                    b.CreateBr(ready);
                    b.SetInsertPoint(ready);
                    v = b.CreateLoad(type(instruction.type), storage);
                    break;
                }
                case Op::async_sleep: {
                    auto task=ir::Type(instruction.type.definition);
                    if(layout.getTypeAllocSize(type(task))!=2*sizeof(std::int32_t) ||
                       field_offset(task,1)!=sizeof(std::int32_t))
                        throw std::logic_error("Async timer task ABI mismatch");
                    auto* output=value_roots[n].at(values.size());
                    b.CreateStore(llvm::ConstantPointerNull::get(b.getPtrTy()),output);
                    auto call=module->getOrInsertFunction("vd_async_sleep",b.getVoidTy(),b.getPtrTy(),
                        b.getInt64Ty(),b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy());
                    b.CreateCall(call,{output,values[instruction.a],values[instruction.b],
                        b.CreateGlobalString(task.definition->identity),async_generation,failure});
                    selected_failure();
                    v=b.CreateLoad(b.getPtrTy(),output);
                    break;
                }
                case Op::temporal_native: {
                    const auto& contract=temporal::operations.at(instruction.immediate);
                    auto* storage=value_roots[n].at(values.size());b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),storage);
                    auto* text=b.CreateStructGEP(type(instruction.type),storage,2);
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* words_type=llvm::ArrayType::get(b.getInt64Ty(),3);auto* words=home(allocations.CreateAlloca(words_type),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    b.CreateStore(llvm::Constant::getNullValue(words_type),words);
                    auto* integers_type=llvm::ArrayType::get(b.getInt64Ty(),contract.arguments.size());auto* texts_type=llvm::ArrayType::get(b.getPtrTy(),contract.arguments.size());
                    auto* integers=home(allocations.CreateAlloca(integers_type),StorageRole::known_abi,llvm_backend::HomeKind::value);auto* texts=home(allocations.CreateAlloca(texts_type),StorageRole::known_abi,llvm_backend::HomeKind::value);
                    auto* empty=llvm::ConstantPointerNull::get(b.getPtrTy());
                    for(unsigned k=0;k<contract.arguments.size();++k){auto* integer=b.CreateInBoundsGEP(integers_type,integers,{b.getInt32(0),b.getInt32(k)});auto* pointer=b.CreateInBoundsGEP(texts_type,texts,{b.getInt32(0),b.getInt32(k)});b.CreateStore(contract.arguments[k]=='t'?static_cast<llvm::Value*>(b.getInt64(0)):values[instruction.arguments[k]],integer);b.CreateStore(contract.arguments[k]=='t'?values[instruction.arguments[k]]:empty,pointer);}
                    auto* valid=b.CreateCall(module->getOrInsertFunction("vd_temporal",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{text,words,b.getInt32(instruction.immediate),integers,texts,failure});
                    auto* failed=llvm::BasicBlock::Create(context,"temporal_failed",fn);auto* ready=llvm::BasicBlock::Create(context,"temporal_ready",fn);b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),failed,ready);b.SetInsertPoint(failed);trace_failure();b.SetInsertPoint(ready);
                    llvm::Value* payload=b.CreateLoad(b.getPtrTy(),text);auto value_type=ir::field_type(instruction.type,1);
                    if(contract.result_words){payload=llvm::Constant::getNullValue(type(value_type));for(unsigned k=0;k<contract.result_words;++k){auto* word=b.CreateLoad(b.getInt64Ty(),b.CreateInBoundsGEP(words_type,words,{b.getInt32(0),b.getInt32(k)}));payload=contract.result_words==1?static_cast<llvm::Value*>(word):b.CreateInsertValue(payload,word,k);}}
                    v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,b.CreateSelect(valid,b.getInt32(0),b.getInt32(1)),0);v=b.CreateInsertValue(v,b.CreateSelect(valid,payload,llvm::Constant::getNullValue(type(value_type))),1);v=b.CreateInsertValue(v,b.CreateSelect(valid,empty,b.CreateLoad(b.getPtrTy(),text)),2);break;
                }
                case Op::format_layout: {
                    auto* storage=value_roots[n].at(values.size());b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),storage);
                    auto* payload=b.CreateStructGEP(type(instruction.type),storage,1);
                    auto* valid=b.CreateCall(module->getOrInsertFunction("vd_format_layout",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{payload,values[instruction.a],b.getInt32(instruction.immediate),failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Format layout failed");auto* text=b.CreateLoad(b.getPtrTy(),payload);auto* empty=llvm::ConstantPointerNull::get(b.getPtrTy());
                    v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,b.CreateSelect(valid,b.getInt32(0),b.getInt32(1)),0);v=b.CreateInsertValue(v,b.CreateSelect(valid,text,empty),1);v=b.CreateInsertValue(v,b.CreateSelect(valid,empty,text),2);break;
                }
                case Op::scalar_display: case Op::scalar_format: {
                    const bool formatted=instruction.op==Op::scalar_format;
                    auto t=operand_type(instruction.a);auto* storage=value_roots[n].at(values.size());
                    auto* empty=llvm::ConstantPointerNull::get(b.getPtrTy());
                    llvm::Value* payload=storage;
                    if(formatted){b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),storage);payload=b.CreateStructGEP(type(instruction.type),storage,1);}
                    auto* valid=b.CreateCall(module->getOrInsertFunction("vd_scalar_display",b.getInt1Ty(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy(),b.getPtrTy(),b.getInt1Ty(),b.getPtrTy()),
                        {payload,b.getInt32(t==ir::Type::string?11:scalar_encoding(t)),t==ir::Type::string?static_cast<llvm::Value*>(b.getInt64(0)):bits(values[instruction.a],t),t==ir::Type::string?values[instruction.a]:empty,formatted?values[instruction.b]:empty,b.getInt1(formatted),failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),"Display operation failed");
                    auto* text=b.CreateLoad(b.getPtrTy(),payload);
                    if(formatted){v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,b.CreateSelect(valid,b.getInt32(0),b.getInt32(1)),0);v=b.CreateInsertValue(v,b.CreateSelect(valid,text,empty),1);v=b.CreateInsertValue(v,b.CreateSelect(valid,empty,text),2);}
                    else v=text;
                    break;
                }
                case Op::string_count: case Op::string_index: case Op::string_encode: case Op::string_decode: {
                    auto text_failure=[&]{fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),instruction.op==Op::string_index?"String index outside extent":"UTF-8 text operation failed");};
                    if(instruction.op==Op::string_count)v=b.CreateCall(module->getOrInsertFunction("vd_string_count",b.getInt64Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{values[instruction.a],b.getInt32(instruction.immediate),failure});
                    else {
                        auto* storage=value_roots[n].at(values.size());
                        if(instruction.op==Op::string_decode){
                            b.CreateStore(llvm::Constant::getNullValue(type(instruction.type)),storage);auto* payload=b.CreateStructGEP(type(instruction.type),storage,1);
                            auto* valid=b.CreateCall(module->getOrInsertFunction("vd_string_decode",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{payload,value_roots[n].at(instruction.a),failure});text_failure();
                            auto* text=b.CreateLoad(b.getPtrTy(),payload);auto* empty=llvm::ConstantPointerNull::get(b.getPtrTy());
                            v=llvm::Constant::getNullValue(type(instruction.type));v=b.CreateInsertValue(v,b.CreateSelect(valid,b.getInt32(0),b.getInt32(1)),0);v=b.CreateInsertValue(v,b.CreateSelect(valid,text,empty),1);v=b.CreateInsertValue(v,b.CreateSelect(valid,empty,text),2);
                        }else {
                            if(instruction.op==Op::string_encode)b.CreateCall(module->getOrInsertFunction("vd_string_encode",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{storage,values[instruction.a],failure});
                            else b.CreateCall(module->getOrInsertFunction("vd_string_index",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getInt32Ty(),b.getInt64Ty(),b.getPtrTy()),{storage,values[instruction.a],b.getInt32(scalar_encoding(operand_type(instruction.b))),bits(values[instruction.b],operand_type(instruction.b)),failure});
                            text_failure();v=b.CreateLoad(type(instruction_types[n].at(values.size())),storage);
                        }
                    }
                    text_failure();break;
                }
                case Op::string_literal: case Op::string_concat: case Op::string_equal: {
                    if(instruction.op==Op::string_equal) {
                        v=b.CreateCall(module->getOrInsertFunction("vd_string_equal",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{values[instruction.a],values[instruction.b],failure});
                    } else {
                        auto* storage=value_roots[n].at(values.size());
                        if(instruction.op==Op::string_literal) {
                            const auto& text=input.strings.at(instruction.immediate);
                            b.CreateCall(module->getOrInsertFunction("vd_string_literal",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy()),
                                {storage,b.CreateGlobalString(llvm::StringRef(text.data(),text.size())),llvm::ConstantInt::get(size_type,text.size()),failure});
                        } else b.CreateCall(module->getOrInsertFunction("vd_string_concat",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{storage,values[instruction.a],values[instruction.b],failure});
                        v=b.CreateLoad(b.getPtrTy(),storage);
                    }
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"String operation failed");
                    break;
                }
                case Op::constant:
                    if(ir::reference_value(instruction.type) || instruction.type==ir::Type::string) v=llvm::Constant::getNullValue(type(instruction.type));
                    else if (numeric::floating(instruction.type)) v = llvm::ConstantFP::get(context,llvm::APFloat(instruction.type == ir::Type::f32 ? llvm::APFloat::IEEEsingle() : llvm::APFloat::IEEEdouble(),llvm::APInt(numeric::width(instruction.type),static_cast<std::uint64_t>(instruction.immediate))));
                    else v = llvm::ConstantInt::get(type(instruction.type),instruction.immediate,true);
                    break;
                case Op::bit_and: v=b.CreateAnd(values[instruction.a],values[instruction.b]);break;
                case Op::bit_or: v=b.CreateOr(values[instruction.a],values[instruction.b]);break;
                case Op::bit_xor: v=b.CreateXor(values[instruction.a],values[instruction.b]);break;
                case Op::bit_not: v=b.CreateNot(values[instruction.a]);break;
                case Op::shift_left: case Op::shift_right: {
                    auto* left=values[instruction.a];auto* count=values[instruction.b];
                    // Counts are integer bit patterns, modulo the value width.
                    // Mask in the original count type before converting to the
                    // LLVM-required matching operand width. Never set nsw/nuw/exact.
                    const auto width=left->getType()->getIntegerBitWidth();
                    count=b.CreateAnd(count,llvm::ConstantInt::get(count->getType(),width-1));
                    count=b.CreateZExtOrTrunc(count,left->getType());
                    v=instruction.op==Op::shift_left?b.CreateShl(left,count):
                      numeric::signed_integer(value_types[instruction.a])?b.CreateAShr(left,count):b.CreateLShr(left,count);
                    break;
                }
                case Op::negate: v = values[instruction.a]->getType()->isFloatingPointTy() ? b.CreateFNeg(values[instruction.a]) : b.CreateNeg(values[instruction.a]); break;
                case Op::upcast: case Op::stage_prefix: v=values[instruction.a]; break;
                case Op::math: {
                    const auto kind=static_cast<math::Kind>(instruction.immediate);
                    auto* scalar=type(instruction.type);
                    if(kind==math::Kind::identity) { v=values[instruction.a]; break; }
                    if(kind==math::Kind::atan2) {
                        // Fixed matching-precision C ABI; no stronger memory/errno attributes.
                        auto target=module->getOrInsertFunction(instruction.type==ir::Type::f32?"atan2f":"atan2",
                            llvm::FunctionType::get(scalar,{scalar,scalar},false));
                        v=b.CreateCall(target,{values[instruction.a],values[instruction.b]});
                        break;
                    }
                    llvm::Intrinsic::ID id=llvm::Intrinsic::not_intrinsic;
                    switch(kind) {
                    case math::Kind::floor:id=llvm::Intrinsic::floor;break;
                    case math::Kind::sqrt:id=llvm::Intrinsic::sqrt;break;
                    case math::Kind::sin:id=llvm::Intrinsic::sin;break;
                    case math::Kind::cos:id=llvm::Intrinsic::cos;break;
                    case math::Kind::tan:id=llvm::Intrinsic::tan;break;
                    case math::Kind::asin:id=llvm::Intrinsic::asin;break;
                    case math::Kind::acos:id=llvm::Intrinsic::acos;break;
                    case math::Kind::atan:id=llvm::Intrinsic::atan;break;
                    case math::Kind::pow:id=llvm::Intrinsic::pow;break;
                    case math::Kind::exp:id=llvm::Intrinsic::exp;break;
                    case math::Kind::exp2:id=llvm::Intrinsic::exp2;break;
                    case math::Kind::log:id=llvm::Intrinsic::log;break;
                    case math::Kind::log2:id=llvm::Intrinsic::log2;break;
                    default:throw std::invalid_argument("invalid validated Math operation");
                    }
                    auto* target=llvm::Intrinsic::getOrInsertDeclaration(module.get(),id,{scalar});
                    if(kind==math::Kind::pow)v=b.CreateCall(target,{values[instruction.a],values[instruction.b]});
                    else v=b.CreateCall(target,{values[instruction.a]});
                    break;
                }
                case Op::random_xor: v=b.CreateXor(values[instruction.a],values[instruction.b]); break;
                case Op::random_seed: {
                    llvm::IRBuilder<> allocations(&*fn->getEntryBlock().begin());
                    auto* word=allocations.CreateAlloca(b.getInt64Ty());
                    auto* status=allocations.CreateAlloca(b.getInt32Ty());
                    b.CreateStore(b.getInt64(0),word); b.CreateStore(b.getInt32(0),status);
                    b.CreateCall(module->getOrInsertFunction("vd_random_seed",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{word,status,failure});
                    array_failure();
                    auto* code=b.CreateLoad(b.getInt32Ty(),status);
                    auto* value=b.CreateInsertValue(llvm::Constant::getNullValue(type(instruction.type)),b.CreateZExt(b.CreateICmpNE(code,b.getInt32(0)),b.getInt32Ty()),0);
                    value=b.CreateInsertValue(value,b.CreateLoad(b.getInt64Ty(),word),1);
                    v=b.CreateInsertValue(value,code,2); break;
                }
                case Op::bitcast: v=b.CreateBitCast(values[instruction.a],type(instruction.type));break;
                case Op::try_convert: {
                    const auto destination=ir::field_type(instruction.type,1);
                    const bool enumeration=destination.kind==ir::Type::numeric_enum;
                    const auto backing=enumeration?ir::Type(*destination.definition->enum_backing):destination;
                    auto [wide,in_range]=checked_integer(values[instruction.a],operand_type(instruction.a),backing);
                    auto* absent=b.GetInsertBlock();
                    auto* present=llvm::BasicBlock::Create(context,"try_convert.some",fn);
                    auto* done=llvm::BasicBlock::Create(context,"try_convert.done",fn);
                    llvm::BasicBlock* membership=nullptr;
                    if(enumeration) {
                        membership=llvm::BasicBlock::Create(context,"try_convert.membership",fn);
                        b.CreateCondBr(in_range,membership,done);b.SetInsertPoint(membership);
                        b.CreateCondBr(enum_membership(destination,wide,true),present,done);
                    } else b.CreateCondBr(in_range,present,done);
                    b.SetInsertPoint(present);
                    auto* empty=llvm::Constant::getNullValue(type(instruction.type));
                    auto* tagged=b.CreateInsertValue(empty,b.getInt32(1),0);
                    auto* some=b.CreateInsertValue(tagged,b.CreateTrunc(wide,type(backing)),1);
                    b.CreateBr(done);b.SetInsertPoint(done);
                    auto* option=b.CreatePHI(type(instruction.type),enumeration?3:2);
                    option->addIncoming(empty,absent);
                    if(membership)option->addIncoming(empty,membership);
                    option->addIncoming(some,present);
                    v=option;break;
                }
                case Op::convert: {
                    auto* a = values[instruction.a]; auto from = operand_type(instruction.a), to = instruction.type;
                    if(from.kind==ir::Type::numeric_enum || to.kind==ir::Type::numeric_enum) {
                        auto source=from.kind==ir::Type::numeric_enum?ir::Type(*from.definition->enum_backing):from;
                        auto destination=to.kind==ir::Type::numeric_enum?ir::Type(*to.definition->enum_backing):to;
                        auto [wide,valid]=checked_integer(a,source,destination);
                        if(to.kind==ir::Type::numeric_enum)valid=b.CreateAnd(valid,enum_membership(to,wide,true));
                        fail_if(b.CreateNot(valid),"Checked enum/integer conversion is out of range or undeclared");
                        v=b.CreateTrunc(wide,type(destination));break;
                    }
                    if(from.definition && from.definition->callable){if(!to.nullable)fail_if(b.CreateICmpEQ(b.CreateExtractValue(a,0),b.getInt64(0)),"Null function value in checked conversion");v=a;break;}
                    if(ir::reference_value(from)) {
                        if(!to.nullable) fail_if(b.CreateIsNull(from.kind==ir::Type::interface_ref ? b.CreateExtractValue(a,0) : a),from.kind==ir::Type::interface_ref ? "Null interface reference in checked conversion" : "Null managed reference in checked conversion");
                        if(from.kind==ir::Type::interface_ref) guard_complete(to,a);
                        v=a;break;
                    }
                    if (from == to) { v = a; break; }
                    if (numeric::integer(from) && numeric::integer(to)) {
                        auto [wide,valid]=checked_integer(a,from,to);
                        fail_if(b.CreateNot(valid),"Checked numeric conversion is out of range");
                        v = b.CreateTrunc(wide,type(to));
                    } else if (numeric::integer(from)) {
                        v = numeric::signed_integer(from) ? b.CreateSIToFP(a,type(to)) : b.CreateUIToFP(a,type(to));
                    } else if (numeric::integer(to)) {
                        // FPToSI/UI truncates toward zero. Check the ORIGINAL float
                        // against exact representable bounds before executing it.
                        // For large signed widths, MIN-1 rounds to MIN: MIN is then
                        // the first valid representable float, so use >= MIN.
                        unsigned exponent = numeric::width(to) - (numeric::signed_integer(to) ? 1 : 0);
                        unsigned precision = from == ir::Type::f32 ? 24 : 53;
                        double minimum = numeric::signed_integer(to) ? -std::ldexp(1.0,static_cast<int>(exponent)) : 0.0;
                        bool inclusive = numeric::signed_integer(to) && exponent >= precision;
                        auto* lower = llvm::ConstantFP::get(a->getType(), inclusive ? minimum : minimum - 1.0);
                        auto* upper = llvm::ConstantFP::get(a->getType(),std::ldexp(1.0,static_cast<int>(exponent)));
                        auto* above = inclusive ? b.CreateFCmpOGE(a,lower) : b.CreateFCmpOGT(a,lower);
                        auto* valid = b.CreateAnd(above,b.CreateFCmpOLT(a,upper));
                        fail_if(b.CreateNot(valid),"Checked numeric conversion is out of range or non-finite");
                        v = numeric::signed_integer(to) ? b.CreateFPToSI(a,type(to)) : b.CreateFPToUI(a,type(to));
                    } else {
                        v = numeric::width(to) > numeric::width(from) ? b.CreateFPExt(a,type(to)) : b.CreateFPTrunc(a,type(to));
                        if (numeric::width(to) < numeric::width(from)) {
                            auto* abs_a = b.CreateUnaryIntrinsic(llvm::Intrinsic::fabs,a);
                            auto* abs_v = b.CreateUnaryIntrinsic(llvm::Intrinsic::fabs,v);
                            auto* inf_a = llvm::ConstantFP::getInfinity(a->getType());
                            auto* inf_v = llvm::ConstantFP::getInfinity(v->getType());
                            fail_if(b.CreateAnd(b.CreateFCmpOLT(abs_a,inf_a),b.CreateFCmpOEQ(abs_v,inf_v)),"Checked numeric conversion overflows to infinity");
                        }
                    }
                    break;
                }
                case Op::add: case Op::subtract: case Op::multiply: case Op::divide: case Op::remainder:
                case Op::equal: case Op::not_equal: case Op::less: case Op::less_equal: case Op::greater: case Op::greater_equal: {
                    auto* a = values[instruction.a]; auto* divisor = values[instruction.b];
                    if(operand_type(instruction.a).definition && operand_type(instruction.a).definition->callable){auto* same=b.CreateCall(callable_comparisons.at(ir::Type::key(operand_type(instruction.a))),{a,divisor});v=instruction.op==Op::equal?same:b.CreateNot(same);break;}
                    if(operand_type(instruction.a).kind==ir::Type::interface_ref) {a=b.CreateExtractValue(a,0);divisor=b.CreateExtractValue(divisor,0);}
                    bool floating = a->getType()->isFloatingPointTy();
                    bool sign = numeric::signed_integer(operand_type(instruction.a));
                    switch (instruction.op) {
                    case Op::add: v = floating ? b.CreateFAdd(a,divisor) : b.CreateAdd(a,divisor); break;
                    case Op::subtract: v = floating ? b.CreateFSub(a,divisor) : b.CreateSub(a,divisor); break;
                    case Op::multiply: v = floating ? b.CreateFMul(a,divisor) : b.CreateMul(a,divisor); break;
                    case Op::divide: case Op::remainder: {
                        if (floating) { v = b.CreateFDiv(a,divisor); break; }
                        fail_if(b.CreateICmpEQ(divisor,llvm::Constant::getNullValue(divisor->getType())),instruction.op == Op::divide ? "Division by zero" : "Remainder by zero");
                        if (sign) {
                            auto* minimum = llvm::ConstantInt::get(context,llvm::APInt::getSignedMinValue(a->getType()->getIntegerBitWidth()));
                            auto* minus_one = llvm::ConstantInt::getSigned(a->getType(),-1);
                            auto* special = b.CreateAnd(b.CreateICmpEQ(a,minimum),b.CreateICmpEQ(divisor,minus_one));
                            auto* safe = b.CreateSelect(special,llvm::ConstantInt::get(a->getType(),1),divisor);
                            auto* result = instruction.op == Op::divide ? b.CreateSDiv(a,safe) : b.CreateSRem(a,safe);
                            v = b.CreateSelect(special,instruction.op == Op::divide ? minimum : llvm::Constant::getNullValue(a->getType()),result);
                        } else v = instruction.op == Op::divide ? b.CreateUDiv(a,divisor) : b.CreateURem(a,divisor);
                        break;
                    }
                    case Op::equal: v = floating ? b.CreateFCmpOEQ(a,divisor) : b.CreateICmpEQ(a,divisor); break;
                    case Op::not_equal: v = floating ? b.CreateFCmpUNE(a,divisor) : b.CreateICmpNE(a,divisor); break;
                    case Op::less: v = floating ? b.CreateFCmpOLT(a,divisor) : sign ? b.CreateICmpSLT(a,divisor) : b.CreateICmpULT(a,divisor); break;
                    case Op::less_equal: v = floating ? b.CreateFCmpOLE(a,divisor) : sign ? b.CreateICmpSLE(a,divisor) : b.CreateICmpULE(a,divisor); break;
                    case Op::greater: v = floating ? b.CreateFCmpOGT(a,divisor) : sign ? b.CreateICmpSGT(a,divisor) : b.CreateICmpUGT(a,divisor); break;
                    case Op::greater_equal: v = floating ? b.CreateFCmpOGE(a,divisor) : sign ? b.CreateICmpSGE(a,divisor) : b.CreateICmpUGE(a,divisor); break;
                    default: break;
                    }
                    break;
                }
                case Op::logical_not: v = b.CreateNot(values[instruction.a]); break;
                case Op::load: v = b.CreateLoad(type(f.locals[instruction.a]), slots[instruction.a]); break;
                case Op::store: {
                    auto t=f.locals[instruction.a];auto* value=values[instruction.b];
                    if(contains_arrays(t)) {
                        auto* staging=copy_staging.at(t);copy_value(t,staging,value_roots[n].at(instruction.b),array_failure);
                        value=b.CreateLoad(type(t),staging);
                    }
                    v=b.CreateStore(value,slots[instruction.a]);
                    if(auto found=debug_bindings.find(instruction.a);found!=debug_bindings.end() &&
                        loc.path==found->second->source.path && loc.line==found->second->source.line &&
                        loc.column==found->second->source.column) {
                        const auto& binding=*found->second;
                        auto* debug_type=debug_value_type(t);
                        auto* variable=binding.argument?
                            debug->createParameterVariable(subprogram,binding.name,binding.argument,source_file,binding.source.line,debug_type):
                            debug->createAutoVariable(subprogram,binding.name,source_file,binding.source.line,debug_type);
                        auto* location=llvm::DILocation::get(context,binding.source.line,binding.source.column,subprogram);
                        debug->insertDeclare(slots[instruction.a],variable,debug->createExpression(),location,b.GetInsertBlock());
                        debug_bindings.erase(found);
                    }
                    break;
                }
                case Op::async_call: case Op::function_call: case Op::interface_call: case Op::call: case Op::virtual_call: case Op::base_init: {
                    if(instruction.op==Op::base_init && instruction.callee.empty()) {
                        auto* address=b.CreateLoad(b.getPtrTy(),slots[0]);
                        for(unsigned field=0;field<instruction.arguments.size();++field) {
                            auto value=values[instruction.arguments[field]];
                            guard_complete(instruction.type.definition->fields[field].type,value);
                            b.CreateStore(value,field_address(instruction.type,address,field));
                        }
                        break;
                    }
                    std::vector<llvm::Value*> arguments;
                    if(instruction.op==Op::function_call)arguments.push_back(values[instruction.a]);
                    if (instruction.receiver) {
                        auto* address = slots.at(instruction.receiver->slot);
                        auto t = f.locals.at(instruction.receiver->slot);
                        for (auto field : instruction.receiver->fields) {
                            if (t.kind==ir::Type::managed_ref) { address=b.CreateLoad(type(t),address); t=ir::Type(t.definition); }
                            address = field_address(t,address,field);
                            t = t.definition->fields.at(field).type;
                        }
                        arguments.push_back(t.kind==ir::Type::managed_ref || t.kind==ir::Type::prepared_receiver ? b.CreateLoad(type(t),address) : address);
                    }
                    for (auto id : instruction.arguments) arguments.push_back(values[id]);
                    bool imported = std::find(input.imports.begin(),input.imports.end(),instruction.callee) != input.imports.end();
                    bool legacy_single = imported;
                    for (const auto& target : input.functions) if (target.name == instruction.callee) legacy_single = !target.receiver && target.parameters.size() == 1;
                    if (arguments.empty() && legacy_single) arguments.push_back(values[instruction.a]);
                    auto* callee = instruction.op==Op::function_call ? callable_dispatchers.at(ir::Type::key(operand_type(instruction.a))) : instruction.op==Op::interface_call ? interface_dispatchers.at(instruction.callee) : functions.at(instruction.callee);
                    if(instruction.op==Op::virtual_call) {
                        for(const auto& target:input.functions) if(target.name==instruction.callee) callee=dispatchers.at(target.virtual_slot);
                    }
                    bool checked = callee->arg_size() == arguments.size() + 1;
                    if (checked && !failure) throw std::invalid_argument("Checked source calls require a source diagnostic context");
                    if (checked) arguments.push_back(failure);
                    llvm::AllocaInst* native_guard = nullptr;
                    if (input.coordinated && imported) {
                        // A fixed entry-block slot, including imports inside loops.
                        llvm::IRBuilder<> entry_builder(&*fn->getEntryBlock().begin());
                        native_guard = home(entry_builder.CreateAlloca(b.getInt8Ty(),b.getInt64(sizeof(Runtime::Native))),StorageRole::known_abi,llvm_backend::HomeKind::native_guard);
                        native_guard->setAlignment(llvm::Align(alignof(Runtime::Native)));
                        b.CreateCall(module->getOrInsertFunction("vd_gc_native_enter",b.getVoidTy(),b.getPtrTy()),{native_guard});
                    }
                    if(elision.calls.contains({n,static_cast<unsigned>(values.size())}))
                        callee=functions.at(borrowed_names.at(instruction.callee));
                    v = b.CreateCall(callee, arguments);
                    if (native_guard) b.CreateCall(module->getOrInsertFunction("vd_gc_native_leave",b.getVoidTy(),b.getPtrTy()),{native_guard});
                    if (checked) {
                        auto* failed = llvm::BasicBlock::Create(context, "propagate_error", fn);
                        auto* resume = llvm::BasicBlock::Create(context, "call_success", fn);
                        auto* message = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(failure_type, failure, 0));
                        b.CreateCondBr(b.CreateIsNotNull(message), failed, resume);
                        b.SetInsertPoint(failed);
                        trace_failure();
                        b.SetInsertPoint(resume);
                    }
                    break;
                }

                }
                auto result_type = instruction_types[n].at(values.size());
                if(result_type.kind==ir::Type::numeric_enum && instruction.op!=Op::constant)
                    fail_if(b.CreateNot(enum_membership(result_type,v,false)),"Invalid numeric enum discriminant in storage");
                if(async && result_type!=ir::Type::none)b.CreateStore(v,frame_home(async->values[n][values.size()]));
                else if (input.coordinated && value_roots[n].at(values.size()) &&
                    !redundant_root_definitions.contains({n,static_cast<unsigned>(values.size())}))
                    b.CreateStore(v,value_roots[n][values.size()]);
                // Root an extracted child/result before retiring its owner's
                // fields. A borrowed helper has returned (including its error
                // check) before this point. No safepoint splits these stores.
                if(auto retired=elision.retirements.find({n,static_cast<unsigned>(values.size())});
                   retired!=elision.retirements.end()) {
                    // Adjacent existing pointer words share one zero fill.
                    // This changes stores, never Slot geometry or authority.
                    std::map<llvm::Value*,std::vector<std::pair<std::int64_t,llvm::Value*>>> words;
                    // First-seen plan order is stable; LLVM allocation addresses
                    // must not choose instruction order or stack layout.
                    std::vector<llvm::Value*> bases;
                    auto clear_word=[&](llvm::Value* address) {
                        if(!address)return;
                        std::int64_t offset=0;
                        auto* base=llvm::GetPointerBaseWithConstantOffset(address,offset,layout);
                        if(!llvm::isa<llvm::AllocaInst>(base) || offset<0) {
                            b.CreateStore(llvm::Constant::getNullValue(b.getPtrTy()),address);return;
                        }
                        auto [group,fresh]=words.try_emplace(base);
                        if(fresh)bases.push_back(base);
                        group->second.push_back({offset,address});
                    };
                    for(auto id:retired->second.values)clear_word(value_roots[n][id]);
                    for(auto slot:retired->second.locals)clear_word(slots[slot]);
                    for(auto* base:bases) {
                        auto& group=words.at(base);
                        std::sort(group.begin(),group.end(),[](auto a,auto b){return a.first<b.first;});
                        group.erase(std::unique(group.begin(),group.end(),[](auto a,auto b){return a.first==b.first;}),group.end());
                        for(unsigned begin=0;begin<group.size();) {
                            unsigned end=begin+1;
                            while(end<group.size() && group[end].first==group[end-1].first+layout.getPointerSize())++end;
                            if(end==begin+1)b.CreateStore(llvm::Constant::getNullValue(b.getPtrTy()),group[begin].second);
                            else b.CreateMemSet(group[begin].second,b.getInt8(0),
                                (end-begin)*layout.getPointerSize(),layout.getPointerABIAlignment(0));
                            begin=end;
                        }
                    }
                    for(auto position:retired->second.payloads) {
                        const auto object=ir::Type(instruction_types[position.first][position.second].definition);
                        auto* payload=elided_payloads.at(position);
                        for(unsigned field=0;field<object.definition->fields.size();++field) {
                            ir::Type child=object.definition->fields[field].type;
                            if(child.kind==ir::Type::managed_ref)
                                b.CreateStore(llvm::Constant::getNullValue(type(child)),field_address(object,payload,field));
                        }
                    }
                }
                value_types.push_back(result_type);
                values.push_back(v);
            }
            const auto& end = f.blocks[n].end;
            if(!defer_return_load && f.cleanup_entry && n>=*f.cleanup_entry &&
               (end.kind==ir::Terminator::Kind::return_value || end.kind==ir::Terminator::Kind::return_void))check_cleanup_return();
            switch (end.kind) {
            case ir::Terminator::Kind::return_value: {
                auto* value=values[end.value];
                if(contains_arrays(f.result)) {
                    auto* staging=copy_staging.at(f.result);
                    copy_value(f.result,staging,value_roots[n].at(end.value),[&]{
                        auto* failed=llvm::BasicBlock::Create(context,"array_return_failure",fn);
                        auto* success=llvm::BasicBlock::Create(context,"array_return_success",fn);
                        b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),failure)),failed,success);
                        b.SetInsertPoint(failed);return_failure();b.SetInsertPoint(success);
                    });
                    value=b.CreateLoad(type(f.result),staging);
                }
                if(async){auto* task=b.CreateLoad(b.getPtrTy(),frame_home(0));b.CreateStore(value,field_address(f.task,task,1));b.CreateCall(module->getOrInsertFunction("vd_async_complete",b.getVoidTy(),b.getPtrTy()),{task});leave_frame();b.CreateRetVoid();}
                else {leave_frame();b.CreateRet(value);}break;
            }
            case ir::Terminator::Kind::return_void: if(async)b.CreateCall(module->getOrInsertFunction("vd_async_complete",b.getVoidTy(),b.getPtrTy()),{b.CreateLoad(b.getPtrTy(),frame_home(0))});leave_frame(); b.CreateRetVoid(); break;
            case ir::Terminator::Kind::jump: b.CreateBr(blocks[end.yes]); break;
            case ir::Terminator::Kind::branch: b.CreateCondBr(values[end.value], blocks[end.yes], blocks[end.no]); break;
            case ir::Terminator::Kind::unreachable: b.CreateUnreachable(); break;
            }
        }
    }
#include "async_start.inc"
    if (!input.globals.empty()) {
        // The host adapter owns startup. Calls between source functions use the
        // ordinary bodies, and every cell access still checks readiness/owner.
        auto declaration = std::find_if(input.functions.begin(), input.functions.end(), [&](const auto& f) { return f.name == exported_entry; });
        auto* body = module->getFunction(native_symbol(*declaration, suffix));
        auto name = body->getName().str(); body->setName(name + "$global_body");
        auto* wrapper = llvm::Function::Create(body->getFunctionType(), llvm::GlobalValue::ExternalLinkage, name, *module);
        if (auto* metadata = body->getMetadata("func_sanitize")) wrapper->setMetadata("func_sanitize", metadata);
        b.SetCurrentDebugLocation(llvm::DebugLoc());
        auto* start = llvm::BasicBlock::Create(context, "globals.begin", wrapper);
        auto* initialize = llvm::BasicBlock::Create(context, "globals.initialize", wrapper);
        auto* finish = llvm::BasicBlock::Create(context, "globals.finish", wrapper);
        auto* run = llvm::BasicBlock::Create(context, "globals.run", wrapper);
        auto* failed = llvm::BasicBlock::Create(context, "globals.failed", wrapper);
        b.SetInsertPoint(start);
        auto* failure = wrapper->getArg(wrapper->arg_size() - 1);
        auto contract = global_contract(input);
        auto* state = b.CreateCall(module->getOrInsertFunction("vd_globals_begin", b.getInt32Ty(), b.getPtrTy(), size_type, size_type, b.getPtrTy()),
            {b.CreateGlobalString(contract), llvm::ConstantInt::get(size_type, contract.size()), llvm::ConstantInt::get(size_type, input.globals.size()), failure});
        auto* dispatch = llvm::BasicBlock::Create(context, "globals.dispatch", wrapper);
        b.CreateCondBr(b.CreateICmpSLT(state, b.getInt32(0)), failed, dispatch);
        b.SetInsertPoint(dispatch); b.CreateCondBr(b.CreateICmpEQ(state, b.getInt32(1)), initialize, run);
        b.SetInsertPoint(initialize);
        auto check = [&] {
            auto* next = llvm::BasicBlock::Create(context, "globals.next", wrapper);
            b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(), failure)), finish, next); b.SetInsertPoint(next);
        };
        for (std::size_t n = 0; n < input.globals.size(); ++n) {
            auto* cell = b.CreateCall(functions.at(input.globals[n].initializer), {failure}); check();
            b.CreateCall(module->getOrInsertFunction("vd_global_publish", b.getVoidTy(), size_type, b.getPtrTy(), b.getPtrTy()),
                {llvm::ConstantInt::get(size_type, n), cell, failure}); check();
        }
        b.CreateBr(finish); b.SetInsertPoint(finish);
        b.CreateCall(module->getOrInsertFunction("vd_globals_finish", b.getVoidTy(), b.getPtrTy()), {failure});
        b.CreateCondBr(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(), failure)), failed, run);
        b.SetInsertPoint(run);
        std::vector<llvm::Value*> args; for (auto& argument : wrapper->args()) args.push_back(&argument);
        b.CreateRet(b.CreateCall(body, args));
        b.SetInsertPoint(failed); b.CreateRet(llvm::Constant::getNullValue(wrapper->getReturnType()));
    }
    if(debug)debug->finalize();
#ifdef VOID_LLVM_RESEARCH
    if(inventory) for(auto& function:*module) if(!function.isDeclaration()) {
        inventory->function(function,false);
        inventory->complete(function,llvm_backend::all_storage_roles());
    }
#endif
    return module;
}
}
struct Jit::Impl {
    std::shared_ptr<llvm::orc::LLJIT> jit;
    std::uint64_t serial = 0;
    DebugCache cache;
    ir::Emission emission = ir::Emission::all;
    std::string cache_status="disabled", target_identity, captured;
    bool custom_bindings=false, capture=false, canonical=false;
    std::shared_ptr<Runtime> source_runtime;
    bool replacing_globals = false;
    std::shared_ptr<std::atomic<std::size_t>> live=std::make_shared<std::atomic<std::size_t>>(0);
    struct Generation {
        std::shared_ptr<llvm::orc::LLJIT> jit;
        std::shared_ptr<std::atomic<std::size_t>> live;
        ApplicationMetadata::Owner application_metadata;
        std::string build_provenance;
        std::shared_ptr<const DynamicBinding> dynamic;
        llvm::orc::ResourceTrackerSP tracker;
        llvm::orc::JITDylib* dylib=nullptr;
        ~Generation() {
            if (tracker) if (auto error = tracker->remove())
                llvm::errs() << "VOID JIT retirement failed: " << llvm::toString(std::move(error)) << '\n';
            if(dylib) if(auto error=jit->getExecutionSession().removeJITDylib(*dylib))
                llvm::errs()<<"VOID JIT dylib retirement failed: "<<llvm::toString(std::move(error))<<'\n';
            --*live;
        }
    };
    std::shared_ptr<const NativeCode> load(std::unique_ptr<llvm::Module> module,
        std::unique_ptr<llvm::LLVMContext> context, const std::string& symbol,
        Compatibility contract, const std::shared_ptr<Impl>&, bool checked = false, std::shared_ptr<Runtime> runtime = {}, const std::string& suffix = {}, bool isolated = false, std::unique_ptr<llvm::MemoryBuffer> object = {}, CPrimitiveAdmission::Owner primitives = {}, std::shared_ptr<const DynamicBinding> dynamic = {}) {
#ifdef VOID_FUNCTION_SANITIZER_HASH
        // The host ABI is checked before reaching this boundary. Match the
        // compiler-generated plain/checked entry metadata without disabling UBSan.
        if(module) {
        auto* function = module->getFunction(symbol);
        llvm::IRBuilder<> builder(*context);
        function->setMetadata("func_sanitize", llvm::MDNode::get(*context, {
            llvm::ConstantAsMetadata::get(builder.getInt32(VOID_FUNCTION_SANITIZER_MAGIC)),
            llvm::ConstantAsMetadata::get(builder.getInt32(checked ? VOID_CHECKED_SANITIZER_HASH : VOID_FUNCTION_SANITIZER_HASH))}));
        }
#endif
        observation::Scope timing(observation::Phase::jit_materialization);
        auto generation = std::make_shared<Generation>();
        generation->jit=jit;generation->live=live;++*live;
        generation->application_metadata=contract.application_metadata;
        generation->build_provenance=contract.build_provenance;
        auto* target=&jit->getMainJITDylib();
        // Source generations are self-contained. Trusted raw-IR clients retain
        // their existing cross-module exported-symbol linking contract.
        if(isolated) {
            auto created=jit->getExecutionSession().createJITDylib("void.g"+std::to_string(++serial));
            if(!created)throw std::runtime_error(llvm::toString(created.takeError()));
            target=&*created;generation->dylib=target;
            target->addToLinkOrder(jit->getMainJITDylib());
            // ORC link orders are not transitive. Main's platform/process
            // links do not resolve native helpers (e.g. Windows __chkstk)
            // for this generation. Keep explicit imports first, then use
            // LLJIT's supported platform/process defaults directly.
            target->addToLinkOrder(jit->defaultLinkOrder());
        }
        auto& dylib=*target;
        generation->tracker=dylib.createResourceTracker();
        generation->dynamic = std::move(dynamic);
        if (primitives) {
            llvm::orc::SymbolMap symbols;
            if (primitives->dynamic()) {
                symbols[jit->mangleAndIntern(c_dynamic_resolve)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_dynamic_resolve_v1), llvm::JITSymbolFlags::Exported);
                symbols[jit->mangleAndIntern(c_dynamic_call)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_dynamic_call_v1), llvm::JITSymbolFlags::Exported);
                if (!primitives->callbacks().empty())
                    symbols[jit->mangleAndIntern(c_callback_bridge)] = llvm::orc::ExecutorSymbolDef(
                        llvm::orc::ExecutorAddr::fromPtr(vd_c_callback_dispatch_v1), llvm::JITSymbolFlags::Exported);
            } else {
                symbols[jit->mangleAndIntern(primitives->transport())] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(primitives->thunk()), llvm::JITSymbolFlags::Exported);
            }
            if (primitives->revision() >= 2)
                symbols[jit->mangleAndIntern(c_span_bridge)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_span_dispatch_v2), llvm::JITSymbolFlags::Exported);
            if (primitives->revision() >= 3) {
                symbols[jit->mangleAndIntern(c_resource_bridge)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_resource_dispatch_v3), llvm::JITSymbolFlags::Exported);
                symbols[jit->mangleAndIntern(c_owner_resource_bridge)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_resource_dispatch_owner_v3), llvm::JITSymbolFlags::Exported);
                symbols[jit->mangleAndIntern(c_resources_bridge)] = llvm::orc::ExecutorSymbolDef(
                    llvm::orc::ExecutorAddr::fromPtr(vd_c_resources_dispatch_v3), llvm::JITSymbolFlags::Exported);
            }
            check(dylib.define(llvm::orc::absoluteSymbols(symbols), generation->tracker));
        }
        if(object)check(jit->addObjectFile(generation->tracker,std::move(object)));
        else check(jit->addIRModule(generation->tracker, llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
        auto address = unwrap(jit->lookup(dylib,symbol));
        // Prepare the lease before committing Runtime metadata, as on the
        // original cold path: allocation failure must not leave a new catalog.
        auto result=checked
            ? std::make_shared<NativeCode>(address.toPtr<NativeCode::CheckedEntry>(),generation,std::move(contract),runtime)
            : std::make_shared<NativeCode>(address.toPtr<NativeCode::Entry>(),generation,std::move(contract));
        if(runtime) {
            auto lookup=[&](const std::string& name){return unwrap(jit->lookup(dylib,name+suffix));};
            RuntimeFailure failure;
            vd_runtime_admit_metadata(runtime.get(),lookup("__vd_types").toPtr<const RuntimeTypeRecord*>(),
                *lookup("__vd_type_count").toPtr<const std::size_t*>(),lookup("__vd_witnesses").toPtr<const RuntimeWitnessRecord*>(),
                *lookup("__vd_witness_count").toPtr<const std::size_t*>(),&failure);
            if(failure.message)throw std::runtime_error(failure.message);
        }
        return result;
    }
};
Jit::Jit(DebugCache cache,ir::Emission emission) : impl_(std::make_shared<Impl>()) {
    impl_->cache=std::move(cache);
    impl_->emission=emission;
    initialize();
    auto machine=unwrap(llvm::orc::JITTargetMachineBuilder::detectHost());
    machine.getTargetTriple()=detail::native_deployment_triple(machine.getTargetTriple());
    machine.setCodeGenOptLevel(llvm::CodeGenOptLevel::Default);
    impl_->target_identity=machine.getTargetTriple().str()+"|"+machine.getCPU()+"|"+machine.getFeatures().getString()+"|default-codegen|default-relocation|default-code-model";
    impl_->jit = unwrap(llvm::orc::LLJITBuilder().setJITTargetMachineBuilder(std::move(machine)).create());
    impl_->target_identity+="|"+impl_->jit->getDataLayout().getStringRepresentation();
    impl_->jit->getObjTransformLayer().setTransform([owner=impl_.get()](std::unique_ptr<llvm::MemoryBuffer> object)->llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>> {
        if(owner->capture) {try{owner->captured=object->getBuffer().str();}catch(const std::bad_alloc&){owner->captured.clear();}}
        return std::move(object);
    });
    bind("vd_gc_frame_enter",vd_gc_frame_enter); bind("vd_gc_frame_leave",vd_gc_frame_leave);
    // Intrinsic lowering may select these standard libcalls even in isolated
    // source generations or a cache hit. Bind only the closed scalar Math set.
#define VD_BIND_MATH_UNARY(name) \
    bind(#name,static_cast<double(*)(double)>(std::name)); \
    bind(#name "f",static_cast<float(*)(float)>(std::name))
    VD_BIND_MATH_UNARY(floor);VD_BIND_MATH_UNARY(sqrt);
    VD_BIND_MATH_UNARY(sin);VD_BIND_MATH_UNARY(cos);VD_BIND_MATH_UNARY(tan);
    VD_BIND_MATH_UNARY(asin);VD_BIND_MATH_UNARY(acos);VD_BIND_MATH_UNARY(atan);
    VD_BIND_MATH_UNARY(exp);VD_BIND_MATH_UNARY(exp2);VD_BIND_MATH_UNARY(log);VD_BIND_MATH_UNARY(log2);
#undef VD_BIND_MATH_UNARY
    bind("atan2",static_cast<double(*)(double,double)>(std::atan2));
    bind("atan2f",static_cast<float(*)(float,float)>(std::atan2));
    bind("pow",static_cast<double(*)(double,double)>(std::pow));
    bind("powf",static_cast<float(*)(float,float)>(std::pow));
    bind("vd_random_seed", vd_random_seed);
    bind("vd_filesystem_call", vd_filesystem_call);bind("vd_filesystem_write_checked",vd_filesystem_write_checked);
    bind("vd_url_call", vd_url_call);
    bind("vd_http_call", vd_http_call);
    bind("vd_http_cleanup", vd_http_cleanup);
    bind("vd_os_call", vd_os_call);
    bind("vd_host_call",vd_host_call);bind("vd_host_cleanup",vd_host_cleanup);bind("vd_host_capture",vd_host_capture);
    bind("vd_net_call",vd_net_call);bind("vd_net_cleanup",vd_net_cleanup);
    bind("vd_file_control", vd_file_control);
    bind("vd_async_sleep",vd_async_sleep);
    bind("vd_console_v2",vd_console_v2);bind("vd_console_print",vd_console_print);
    bind("vd_temporal",vd_temporal);bind("vd_format_layout",vd_format_layout);bind("vd_display_enter",vd_display_enter);bind("vd_display_leave",vd_display_leave);bind("vd_display_error",vd_display_error);
    bind("vd_collection_scope",vd_collection_scope);bind("vd_collection_scope_leave",vd_collection_scope_leave);bind("vd_collection_enter",vd_collection_enter);bind("vd_collection_leave",vd_collection_leave);bind("vd_collection_append",vd_collection_append);bind("vd_collection_finish",vd_collection_finish);
    bind("vd_scalar_display",vd_scalar_display);bind("vd_string_join",vd_string_join);bind("vd_string_join_encode",vd_string_join_encode);
    bind("vd_text_case", vd_text_case);
    bind("vd_text_translate", vd_text_translate);
    bind("vd_string_count",vd_string_count);bind("vd_string_index",vd_string_index);bind("vd_string_encode",vd_string_encode);bind("vd_string_decode",vd_string_decode);
    bind("vd_string_literal",vd_string_literal); bind("vd_string_concat",vd_string_concat); bind("vd_string_equal",vd_string_equal);
    bind("vd_array_default",vd_array_default);bind("vd_array_copy",vd_array_copy);
    bind("vd_array_read",vd_array_read);bind("vd_array_write",vd_array_write);
    bind("vd_dict_new",vd_dict_new);bind("vd_dict_count",vd_dict_count);bind("vd_dict_read",vd_dict_read);bind("vd_dict_entry",vd_dict_entry);bind("vd_dict_write",vd_dict_write);bind("vd_dict_remove",vd_dict_remove);bind("vd_dict_clear",vd_dict_clear);
    bind("vd_value_array_insert",vd_value_array_insert);bind("vd_value_array_clear",vd_value_array_clear);bind("vd_value_array_slice",vd_value_array_slice);
    bind("vd_value_array_new",vd_value_array_new);bind("vd_value_array_count",vd_value_array_count);bind("vd_value_array_complete",vd_value_array_complete);bind("vd_value_array_version",vd_value_array_version);bind("vd_value_array_read",vd_value_array_read);bind("vd_value_array_write",vd_value_array_write);bind("vd_value_array_push",vd_value_array_push);bind("vd_value_array_remove",vd_value_array_remove);bind("vd_value_array_copy",vd_value_array_copy);
    bind("vd_list_new",vd_list_new);bind("vd_list_count",vd_list_count);bind("vd_list_version",vd_list_version);
    bind("vd_convert_parse", vd_convert_parse);
    bind("vd_list_write_bytes", vd_list_write_bytes);
    bind("vd_list_read",vd_list_read);bind("vd_list_read_index",vd_list_read_index);bind("vd_list_write",vd_list_write);bind("vd_list_push",vd_list_push);bind("vd_list_remove",vd_list_remove);
    bind("vd_gc_construct",vd_gc_construct);
    bind("vd_gc_construct_typed",vd_gc_construct_typed);
    bind("vd_gc_dynamic_type",vd_gc_dynamic_type);
    bind("vd_gc_witness",vd_gc_witness);
    bind("vd_gc_interface_binding",vd_gc_interface_binding);
    bind("vd_globals_ready",vd_globals_ready); bind("vd_globals_begin",vd_globals_begin); bind("vd_globals_finish",vd_globals_finish);
    bind("vd_global_publish",vd_global_publish); bind("vd_global_cell",vd_global_cell);
    bind("vd_cleanup_failure",vd_cleanup_failure);bind("vd_gc_poll",vd_gc_poll); bind("vd_gc_collect",vd_gc_collect); bind("vd_gc_allocate",vd_gc_allocate);
    bind("vd_gc_release",vd_gc_release);bind("vd_gc_alive",vd_gc_alive); bind("vd_gc_native_enter",vd_gc_native_enter); bind("vd_gc_native_leave",vd_gc_native_leave);
    bind("vd_native_fixture_call",vd_native_fixture_call);bind("vd_native_sink_call",vd_native_sink_call);bind("vd_worker_checkpoint",vd_worker_checkpoint);bind("vd_async_root",vd_async_root);bind("vd_async_new",vd_async_new);bind("vd_async_start",vd_async_start);bind("vd_async_ready",vd_async_ready);bind("vd_async_suspend",vd_async_suspend);bind("vd_async_complete",vd_async_complete);bind("vd_async_cancel_children",vd_async_cancel_children);bind("vd_async_cleanup_enter",vd_async_cleanup_enter);bind("vd_async_cleanup_leave",vd_async_cleanup_leave);bind("vd_async_finish",vd_async_finish);bind("vd_async_cancel",vd_async_cancel);bind("vd_async_checkpoint",vd_async_checkpoint);bind("vd_async_group_enter",vd_async_group_enter);bind("vd_async_group_join",vd_async_group_join);bind("vd_async_drain",vd_async_drain);
    impl_->custom_bindings=false;
}
Jit::~Jit() = default;
void Jit::bind_address(std::string name, std::uintptr_t entry) {
    if (!entry) throw std::invalid_argument("null native import");
    if (reserved_c_transport(name))
        throw std::invalid_argument("C primitive transport requires per-generation admission");
    impl_->custom_bindings=true;
    llvm::orc::SymbolMap symbols;
    symbols[impl_->jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr(entry), llvm::JITSymbolFlags::Exported);
    check(impl_->jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(symbols)));
}
static std::string application_build_provenance(const ValidatedAnalysis* analysis, const ApplicationMetadata& metadata,
                                                std::string_view target, bool optimize, std::string_view inputs) {
    std::string recipe="VOID-application-build-v1";
    auto field=[&](std::string_view value){llvm_backend::cache_field(recipe,value);};
    field(LLVM_VERSION_STRING);field(target);field(optimize?"O2":"O0");field(inputs);field(metadata.bytes());
    if(analysis) {
        field(analysis->entry_identity());field(analysis->compatibility_contract());
        for(const auto& source:analysis->modules()) {field(source.identity);field(source.source.text());}
    }
    return llvm_backend::digest(recipe);
}
std::shared_ptr<const NativeCode> Jit::compile(const ir::Module& input, const std::string& entry,
                                              bool optimize, Compatibility contract, std::shared_ptr<Runtime> runtime) {
    if(auto* attached=Runtime::attached();attached && attached->is_managed()) {
        Runtime::Native native(*attached);
        return compile(input,entry,optimize,std::move(contract),std::move(runtime));
    }
    if (runtime && !input.coordinated) {
        auto coordinated = input;
        coordinated.coordinated = true;
        return compile(coordinated,entry,optimize,std::move(contract),std::move(runtime));
    }
    if (input.coordinated && input.analysis && !runtime) {
        if (input.application_metadata || !input.globals.empty() ||
            (input.c_primitives && input.c_primitives->dynamic())) runtime = std::make_shared<Runtime>(256,Runtime::AllocationPolicy::live);
        else { if (!impl_->source_runtime) impl_->source_runtime=std::make_shared<Runtime>(256,Runtime::AllocationPolicy::live); runtime=impl_->source_runtime; }
    }
    std::map<std::string,ir::ReferenceEffects> checked_effects;ir::validate(input,nullptr,&checked_effects);
    check_c_primitive_target(input, impl_->jit->getTargetTriple());
    if (input.coordinated != static_cast<bool>(runtime)) throw std::invalid_argument("coordinated IR requires an explicit runtime owner");
    const ir::Function* selected = nullptr;
    for (const auto& f : input.functions) if (f.name == entry) selected = &f;
    // Retain the source API's convenient entry-file lookup, never searching
    // imported functions by simple name. Proof IR continues to use exact names.
    if (!selected) for (const auto& f : input.functions)
        if (f.name == input.identity + "::" + entry && !f.module_identity.empty()) selected = &f;
    if (!selected) throw std::invalid_argument("entry function does not exist");
    if (input.c_primitives && selected->module_identity == input.c_primitives->module())
        throw std::invalid_argument("C primitive binding cannot be a host entry");
    if(selected->asynchronous!=input.async_root)throw std::invalid_argument("Async entry requires the explicit scheduler root adapter");
    if (runtime) runtime->start_application(input.application_metadata,false);
    if (!input.globals.empty()) runtime->admit_globals(global_contract(input), impl_->replacing_globals, false);
    contract.async_root=input.async_root;
    if (selected->parameters != std::vector<ir::Type>{ir::Type::i32} || selected->result != ir::Type::i32)
        throw std::invalid_argument("host invocation requires i32(i32), not the selected function signature");
    auto expected_id = selected->module_identity.empty() ? input.identity + "::" + entry : selected->name;
    if (contract.function_id.empty()) contract.function_id = expected_id;
    if (contract.function_id != expected_id || contract.signature != "c:i32(i32)")
        throw std::invalid_argument("code identity or signature does not match IR");
    // Compiler records always override host-supplied source safety metadata.
    contract.analysis = input.analysis;
    contract.application_metadata = input.application_metadata;
    if (input.application_metadata)
        contract.build_provenance = application_build_provenance(input.analysis.get(),*input.application_metadata,impl_->target_identity,optimize,impl_->cache.build_identity);
    contract.source_contract = input.analysis ? input.analysis->compatibility_contract() : std::string{};
    if (input.c_primitives && !input.analysis)
        contract.source_contract = input.c_primitives->descriptor();
    contract.runtime_identity = reinterpret_cast<std::uintptr_t>(runtime.get());
    contract.native_thunk_identity = input.c_primitives && input.c_primitives->revision() == 3 ?
        reinterpret_cast<std::uintptr_t>(input.c_primitives->thunk()) : 0;
    contract.native_dynamic_contract = input.c_primitives ? input.c_primitives->dynamic_contract() : std::string{};
    if (input.coordinated && !input.analysis) {
        std::map<std::string,std::string> layouts;
        auto encoded = [](const std::string& text) { return std::to_string(text.size()) + ":" + text; };
        std::function<void(ir::Type)> record = [&](ir::Type t) {
            if (!t.definition || layouts.contains(t.definition->identity)) return;
            auto base=t.definition->base.lock();
            std::string text = encoded(t.definition->identity) + "base:"+encoded(base ? base->identity : "")+"{";
            for (const auto& field : t.definition->fields) text += encoded(field.name) + ":"+encoded(field.owner)+":"+encoded(ir::type_name(field.type))+":"+encoded(ir::capability_key(field.type))+";";
            if(t.kind==ir::Type::numeric_enum)text+=encoded(ir::numeric_enum_layout(t));
            for(const auto& variant:t.definition->cases)text+=encoded(variant.name)+":"+std::to_string(variant.begin)+":"+std::to_string(variant.count)+";";
            layouts.emplace(t.definition->identity,text + "}");
            for (const auto& field : t.definition->fields) record(field.type);
        };
        for(auto t:input.nominal_types) record(t);
        for (const auto& f : input.functions) {
            contract.source_contract+=f.asynchronous?"async;":"sync;";
            contract.source_contract+="virtual:"+encoded(f.name)+encoded(f.virtual_slot)+encoded(f.method_name)+encoded(f.visibility);
            contract.source_contract+=encoded(ir::type_name(f.result))+encoded(ir::capability_key(f.result))+std::to_string(f.parameters.size())+":";
            for(auto t:f.parameters) contract.source_contract+=encoded(ir::type_name(t))+encoded(ir::capability_key(t));
            contract.source_contract+=f.receiver_mutates ? "mutating;" : "nonmutating;";
            record(f.result); for (auto t : f.parameters) record(t); for (auto t : f.locals) record(t);
            for (const auto& block : f.blocks) for (const auto& i : block.instructions) record(i.type);
        }
        for(const auto& r:input.interface_requirements) {
            contract.source_contract+="interface:"+encoded(r.name)+encoded(r.visibility)+encoded(ir::type_name(r.result))+encoded(ir::capability_key(r.result));
            for(auto t:r.parameters) contract.source_contract+=encoded(ir::type_name(t))+encoded(ir::capability_key(t));
            const auto& effect=checked_effects.at(r.name);
            for(auto [set,tag]:{std::pair{&effect.mutations,'M'},std::pair{&effect.retained,'H'},std::pair{&effect.required_mutable,'Q'}}) for(const auto& origin:*set) {
                contract.source_contract+=tag+std::to_string(origin.parameter)+":";for(auto field:origin.path) contract.source_contract+=std::to_string(field)+",";contract.source_contract+=';';
            }
            for(const auto& [path,fact]:effect.returned) {
                contract.source_contract+="R[";for(auto field:path) contract.source_contract+=std::to_string(field)+",";contract.source_contract+=fact.readonly ? "]C" : "]V";
                for(const auto& origin:fact.origins) {contract.source_contract+=std::to_string(origin.parameter)+":";for(auto field:origin.path) contract.source_contract+=std::to_string(field)+",";contract.source_contract+=';';}
            }
        }
        for(const auto& binding:input.interface_bindings) contract.source_contract+=encoded(binding.key);
        for(const auto& [scope,imports]:input.source_imports) {contract.source_contract+=encoded(scope);for(const auto& imported:imports) contract.source_contract+=encoded(imported);}
        contract.source_contract += "\nphase15-runtime-layouts:";
        for (const auto& [name,text] : layouts) { (void)name; contract.source_contract += std::to_string(text.size()) + ":" + text; }
    }
    if (!selected->module_identity.empty()) contract.constant_contract = input.analysis ? input.analysis->constant_contract() : input.constant_contract;
    observation::Scope ir_timing(observation::Phase::llvm_ir);
    auto suffix = impl_->canonical ? "__cache" : "__v" + std::to_string(++impl_->serial);
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = lower(input, *context, suffix, impl_->jit->getDataLayout(),selected->name);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple());
    ir_timing.finish();
    finish(*module, optimize);
    std::shared_ptr<const DynamicBinding> dynamic;
    if (input.c_primitives && input.c_primitives->dynamic()) {
        const auto& plan = input.c_primitives->dynamic();
        const auto& root = input.c_primitives->dynamic_root();
        dynamic = runtime->dynamic_binding();
        if (!dynamic) dynamic = load_dynamic(plan, root);
        else if (!matches_dynamic(*dynamic, *plan, root))
            throw std::invalid_argument("C dynamic closure is incompatible with the Runtime binding");
    }
    auto result = impl_->load(std::move(module), std::move(context), native_symbol(*selected, suffix), std::move(contract), impl_, !selected->module_identity.empty() || input.coordinated, runtime,suffix,!selected->module_identity.empty() || bool(input.c_primitives),{},input.c_primitives,dynamic);
    if (dynamic) runtime->bind_dynamic(std::move(dynamic));
    if (!input.globals.empty()) runtime->admit_globals(global_contract(input), impl_->replacing_globals);
    return result;
}
const std::string& Jit::cache_status() const {return impl_->cache_status;}
std::shared_ptr<const NativeCode> Jit::compile_analysis(const ValidatedAnalysis& analysis,bool optimize,Compatibility contract,std::shared_ptr<Runtime> runtime,bool async_root) {
    contract.async_root=async_root;
    if(auto* attached=Runtime::attached();attached && attached->is_managed()) {
        Runtime::Native native(*attached);return compile_analysis(analysis,optimize,std::move(contract),std::move(runtime),async_root);
    }
    require_source_entry(analysis);
    impl_->cache_status="disabled";
    auto cold=[&]{auto module=analysis.lower(impl_->emission);module.async_root=async_root;return compile(module,module.entry_symbol,optimize,contract,runtime);};
    if(!impl_->cache.project_root.empty() && impl_->cache.build_identity.empty()){impl_->cache_status="unavailable";return cold();}
    if (analysis.c_primitive_admission()) {
        if (!impl_->cache.project_root.empty()) impl_->cache_status = "unavailable";
        return cold();
    }
    if(impl_->custom_bindings || impl_->cache.project_root.empty())return cold();
    observation::Scope lookup_timing(observation::Phase::cache_lookup);
    std::string identity;
    try {
    identity="VOID-native-object-v2";
    auto field=[&](std::string_view value){llvm_backend::cache_field(identity,value);};
    field(impl_->cache.build_identity);field("integrated-log-v1;rooted-local-cleanup;join-utf8;compact-sync-write");field("checked-root-location-v2");field(std::to_string(sizeof(Runtime::Frame)));field(formatting::abi_version);field(temporal::abi_version);field(impl_->target_identity);field(analysis.entry_identity());field(async_root?"async-root-v1":"sync-root");
    field(optimize?"ir-O2":"no-ir-O2");
    field(impl_->emission==ir::Emission::entry?"entry-emission-v1":"all-exports");
    field(analysis.compatibility_contract());field(analysis.constant_contract());field(runtime ? "forced-runtime" : "automatic-runtime");
    field(analysis.application_metadata()?std::string_view(analysis.application_metadata()->bytes()):std::string_view{});
    field(std::to_string(analysis.modules().size()));
    for(const auto& m:analysis.modules()) {field("module");field(m.identity);field(m.source.path());field(m.source.text());field(std::to_string(m.imports.size()));for(const auto& i:m.imports)field(i);}
    } catch(const std::bad_alloc&) {impl_->cache_status="unavailable";lookup_timing.finish();return cold();}
    llvm_backend::NativeCache store(impl_->cache.project_root,identity);
    if(!store.available()){impl_->cache_status="unavailable";lookup_timing.finish();return cold();}
    auto artifact=store.read();
    lookup_timing.finish();
    if(artifact) {
        try {
            auto object=llvm::MemoryBuffer::getMemBufferCopy(artifact->object);
            auto owner=runtime;
            const bool globals = std::any_of(analysis.declarations().begin(), analysis.declarations().end(), [](const auto& d) { return d.kind == AnalyzedDeclaration::Kind::global_variable; });
            if (globals && !artifact->coordinated) throw std::runtime_error("global cache coordination mismatch");
            if (artifact->coordinated && !owner) {
                if (analysis.application_metadata() || globals) owner = std::make_shared<Runtime>(256,Runtime::AllocationPolicy::live);
                else { if (!impl_->source_runtime) impl_->source_runtime = std::make_shared<Runtime>(256,Runtime::AllocationPolicy::live); owner = impl_->source_runtime; }
            }
            if (owner) owner->start_application(analysis.application_metadata(),false);
            if (globals) owner->admit_globals(global_contract(analysis), impl_->replacing_globals, false);
            if(bool(owner)!=artifact->coordinated)throw std::runtime_error("cache coordination mismatch");
            auto expected=analysis.entry_symbol();
            if(contract.function_id.empty())contract.function_id=expected;
            if(contract.function_id!=expected || contract.signature!="c:i32(i32)")throw std::invalid_argument("code identity or signature does not match IR");
            contract.source_contract=analysis.compatibility_contract();contract.constant_contract=analysis.constant_contract();
            contract.analysis=analysis.shared_from_this();contract.runtime_identity=reinterpret_cast<std::uintptr_t>(owner.get());
            contract.application_metadata=analysis.application_metadata();
            if (contract.application_metadata)
                contract.build_provenance=application_build_provenance(&analysis,*contract.application_metadata,impl_->target_identity,optimize,impl_->cache.build_identity);
            ir::Function entry;entry.name=expected;entry.module_identity=analysis.entry_identity();
            auto result=impl_->load({}, {},native_symbol(entry,"__cache"),contract,impl_,true,owner,"__cache",true,std::move(object));
            if (globals) owner->admit_globals(global_contract(analysis), impl_->replacing_globals);
            impl_->cache_status="hit";return result;
        } catch(const std::exception&) { /* Discard candidate; compile once normally. */ }
    }
    impl_->cache_status="miss";impl_->canonical=true;impl_->capture=true;impl_->captured.clear();
    try {
        auto result=cold();impl_->canonical=false;impl_->capture=false;
        if(!impl_->captured.empty()) { observation::Scope timing(observation::Phase::cache_store);
            store.write(bool(result->runtime_owner()),impl_->captured); }
        impl_->captured.clear();return result;
    } catch(...) {impl_->canonical=false;impl_->capture=false;impl_->captured.clear();throw;}
}
std::shared_ptr<const NativeCode> Jit::compile(const ValidatedAnalysis& analysis, bool optimize, Compatibility contract) {
    return compile_analysis(analysis,optimize,std::move(contract),{});
}
std::shared_ptr<const NativeCode> Jit::compile_async(const ValidatedAnalysis& analysis,bool optimize,Compatibility contract){return compile_analysis(analysis,optimize,std::move(contract),{},true);}
std::shared_ptr<const NativeCode> Jit::replacement(const NativeCode& current,const ValidatedAnalysis& analysis,bool optimize,Compatibility contract) {
    if(contract.function_id.empty())contract.function_id=analysis.entry_symbol();
    contract.constant_contract=analysis.constant_contract();contract.source_contract=analysis.compatibility_contract();
    contract.analysis=analysis.shared_from_this();contract.runtime_identity=current.compatibility().runtime_identity;contract.async_root=current.compatibility().async_root;
    contract.application_metadata=analysis.application_metadata();
    const auto& native = analysis.c_primitive_admission();
    contract.native_thunk_identity = native && native->revision() == 3 ?
        reinterpret_cast<std::uintptr_t>(native->thunk()) : 0;
    contract.native_dynamic_contract = native ? native->dynamic_contract() : std::string{};
    if(!(current.compatibility()==contract))return {};
    struct Replacement { bool& flag; explicit Replacement(bool& value) : flag(value) { flag = true; } ~Replacement() { flag = false; } } replacement(impl_->replacing_globals);
    return compile_analysis(analysis,optimize,std::move(contract),current.runtime_owner(),current.compatibility().async_root);
}
bool Jit::replace(ReloadSlot& slot,const ValidatedAnalysis& analysis,bool optimize,Compatibility contract) {
    auto next=replacement(*slot.acquire(),analysis,optimize,std::move(contract));return next && slot.replace(std::move(next));
}
bool Jit::replace(ReloadSlot& slot,const ToolingSession& session,const SessionAnalysis& revision,bool optimize,Compatibility contract) {
    if(!session.is_current(revision) || !revision.result().succeeded())return false;
    auto next=replacement(*slot.acquire(),*revision.result().program,optimize,std::move(contract));
    return next && session.is_current(revision) && slot.replace(std::move(next));
}
std::size_t Jit::live_generations() const { return impl_->live->load(); }
std::shared_ptr<const NativeCode> Jit::compile_root_probe(bool optimize) {
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("root proof", *context);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple());
    llvm::IRBuilder<> b(*context);
    auto* ptr = b.getPtrTy();
    auto alloc = module->getOrInsertFunction("vd_probe_allocate", llvm::FunctionType::get(ptr, false));
    auto enter = module->getOrInsertFunction("vd_probe_enter", llvm::FunctionType::get(b.getVoidTy(), {ptr}, false));
    auto poll = module->getOrInsertFunction("vd_probe_poll", llvm::FunctionType::get(b.getVoidTy(), false));
    auto inspect = module->getOrInsertFunction("vd_probe_inspect", llvm::FunctionType::get(b.getInt32Ty(), {ptr}, false));
    auto leave = module->getOrInsertFunction("vd_probe_leave", llvm::FunctionType::get(b.getVoidTy(), {ptr}, false));
    auto name = "root_probe__v" + std::to_string(++impl_->serial);
    auto* fn = llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty()}, false),
                                      llvm::Function::ExternalLinkage, name, *module);
    auto* start = llvm::BasicBlock::Create(*context, "entry", fn);
    auto* test = llvm::BasicBlock::Create(*context, "test", fn);
    auto* body = llvm::BasicBlock::Create(*context, "body", fn);
    auto* done = llvm::BasicBlock::Create(*context, "done", fn);
    b.SetInsertPoint(start);
    auto* slot = b.CreateAlloca(ptr);
    b.CreateStore(b.CreateCall(alloc), slot);
    b.CreateCall(enter, {slot});
    b.CreateBr(test);
    b.SetInsertPoint(test);
    auto* i = b.CreatePHI(b.getInt32Ty(), 2);
    i->addIncoming(b.getInt32(0), start);
    b.CreateCondBr(b.CreateICmpSLT(i, fn->getArg(0)), body, done);
    b.SetInsertPoint(body);
    b.CreateCall(poll);
    auto* next = b.CreateAdd(i, b.getInt32(1));
    i->addIncoming(next, body);
    b.CreateBr(test);
    b.SetInsertPoint(done);
    auto* result = b.CreateCall(inspect, {b.CreateLoad(ptr, slot)});
    b.CreateCall(leave, {slot});
    b.CreateRet(result);
    finish(*module, optimize);
    return impl_->load(std::move(module), std::move(context), name, {"/proof::root_probe"}, impl_);
}
static void emit_impl(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, bool runtime_object, const std::string& generation_suffix = {},
                      bool storage_proof = false, const std::filesystem::path& proof_report = {}, std::string_view build_inputs = {},
                      std::optional<project::DebugInfo> source_debug = {}) {
    if ((runtime_object && !input.coordinated) || (input.coordinated && !runtime_object && !source_program)) throw std::invalid_argument("coordinated AOT requires the runtime proof bootstrap");
    initialize();
    llvm::LLVMContext context;
    bool entry_valid = false;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol)
        entry_valid = f.parameters == std::vector<ir::Type>{ir::Type::i32} && f.result == ir::Type::i32;
    for(const auto& f:input.functions)if(f.name==input.entry_symbol && f.asynchronous!=input.async_root)throw std::invalid_argument("Async AOT entry requires the explicit scheduler root adapter");
    if (!entry_valid) throw std::invalid_argument("AOT CLI adapter requires entry: i32(i32)");
    auto machine = native_machine();
    check_c_primitive_target(input, machine->getTargetTriple());
    const auto triple = machine->getTargetTriple().str();
#ifdef VOID_LLVM_RESEARCH
    llvm_backend::LoweringInventory inventory;
    std::unique_ptr<llvm_backend::AotObjectEmitter> audited;
    if (storage_proof) audited = std::make_unique<llvm_backend::AotObjectEmitter>(triple,optimize);
#else
    if (storage_proof) throw std::invalid_argument("LLVM research certification is not enabled");
    (void)proof_report;
#endif
    observation::Scope ir_timing(observation::Phase::llvm_ir);
    auto module = lower(input, context, source_program ? "__void" : generation_suffix, machine->createDataLayout(),input.entry_symbol,
                        nullptr,nullptr
#ifdef VOID_LLVM_RESEARCH
                        ,storage_proof ? &inventory : nullptr
#else
                        ,nullptr
#endif
                        ,false,source_debug,optimize);
    module->setTargetTriple(machine->getTargetTriple());
    module->setDataLayout(machine->createDataLayout());
    llvm::GlobalVariable* application_record = nullptr;
    if (input.application_metadata) {
        llvm::IRBuilder<> constants(context);
        application_record = constants.CreateGlobalString(input.application_metadata->bytes(),"vd.application.metadata.v1",0,module.get(),false);
        auto provenance = application_build_provenance(input.analysis.get(),*input.application_metadata,triple,optimize,build_inputs);
        auto* identity = constants.CreateGlobalString(provenance,"vd.application.provenance.v1",0,module.get());
        llvm::appendToUsed(*module,{application_record,identity});
    }
    if (!runtime_object) {
    const ir::Function* entry_function = nullptr;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol) entry_function = &f;
    auto* entry = module->getFunction(native_symbol(*entry_function, source_program ? "__void" : ""));
    if (!entry || module->getFunction("main")) throw std::invalid_argument("AOT proof requires entry and no main");
    llvm::IRBuilder<> b(context);
    auto* main = llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty(), b.getPtrTy()}, false),
                                       llvm::Function::ExternalLinkage, "main", *module);
    b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main));
#ifdef VOID_LLVM_RESEARCH
    if (storage_proof) inventory.function(*main,false);
#endif
    // Runtime input prevents the native proof from folding into a constant exit.
    llvm::Value* input_value = b.CreateSub(main->getArg(0), b.getInt32(2));
    if (source_program && !input.application) {
        // CLI adaptation only: parsed VOID functions use the shared lowering.
        auto* parse = llvm::BasicBlock::Create(context, "parse_input", main);
        auto* ready = llvm::BasicBlock::Create(context, "run", main);
        auto* invalid = llvm::BasicBlock::Create(context, "invalid_input", main);
        auto* start = b.GetInsertBlock();
        b.CreateCondBr(b.CreateICmpEQ(main->getArg(0), b.getInt32(1)), ready, parse);
        b.SetInsertPoint(parse);
        auto* end = b.CreateAlloca(b.getPtrTy());
#ifdef VOID_LLVM_RESEARCH
        if (storage_proof) inventory.home(*end,StorageRole::known_abi,llvm_backend::HomeKind::cli_parse,"adapter/input-parse");
#endif
        auto* argument_address = b.CreateGEP(b.getPtrTy(), main->getArg(1), b.getInt64(1));
        auto* argument = b.CreateLoad(b.getPtrTy(), argument_address);
        auto convert = module->getOrInsertFunction("strtoll", llvm::FunctionType::get(b.getInt64Ty(), {b.getPtrTy(), b.getPtrTy(), b.getInt32Ty()}, false));
        auto* parsed = b.CreateCall(convert, {argument, end, b.getInt32(10)});
        auto* tail = b.CreateLoad(b.getPtrTy(), end);
        auto* valid = b.CreateAnd(b.CreateICmpEQ(main->getArg(0), b.getInt32(2)), b.CreateICmpNE(tail, argument));
        valid = b.CreateAnd(valid, b.CreateICmpEQ(b.CreateLoad(b.getInt8Ty(), tail), b.getInt8(0)));
        valid = b.CreateAnd(valid, b.CreateICmpSGE(parsed, b.getInt64(static_cast<std::uint64_t>(INT32_MIN))));
        valid = b.CreateAnd(valid, b.CreateICmpSLE(parsed, b.getInt64(INT32_MAX)));
        b.CreateCondBr(valid, ready, invalid);
        b.SetInsertPoint(invalid);
        auto puts = module->getOrInsertFunction("puts", llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, false));
        b.CreateCall(puts, {b.CreateGlobalString("VOID: expected one optional decimal i32 input")}); b.CreateRet(b.getInt32(2));
        b.SetInsertPoint(ready);
        auto* selected = b.CreatePHI(b.getInt32Ty(), 2);
        selected->addIncoming(b.getInt32(0), start);
        auto* truncated = llvm::CastInst::CreateTruncOrBitCast(parsed, b.getInt32Ty(), "input", parse->getTerminator()->getIterator());
        selected->addIncoming(truncated, parse); input_value = selected;
    }
    if(source_program && input.application) {
        input_value=b.getInt32(0);
        if (llvm::Triple(module->getTargetTriple()).isOSWindows()) main->setName("wmain");
    }
    std::vector<llvm::Value*> arguments{input_value};
    llvm::Value* failure = nullptr;
    auto* failure_type = llvm::StructType::get(b.getPtrTy(), b.getInt32Ty(), llvm::ArrayType::get(b.getPtrTy(), 32));
    if (!entry_function->module_identity.empty()) {
        failure = b.CreateAlloca(failure_type);
#ifdef VOID_LLVM_RESEARCH
        if (storage_proof) inventory.home(*llvm::cast<llvm::AllocaInst>(failure),StorageRole::known_abi,llvm_backend::HomeKind::cli_failure,"adapter/failure");
#endif
        b.CreateStore(llvm::Constant::getNullValue(failure_type), failure);
        arguments.push_back(failure);
    }
    llvm::CallInst* result;
    const auto* dynamic = input.c_primitives ? input.c_primitives->dynamic().get() : nullptr;
    if (input.c_primitives && !dynamic) {
        auto* absent = llvm::ConstantPointerNull::get(b.getPtrTy());
        auto* status = call_c_primitive(b, *module, *input.c_primitives, c_primitive_probe, absent, 0, absent);
        auto* mismatch = llvm::BasicBlock::Create(context, "c_primitive_mismatch", main);
        auto* admitted = llvm::BasicBlock::Create(context, "c_primitive_admitted", main);
        b.CreateCondBr(b.CreateIsNotNull(status), mismatch, admitted);
        b.SetInsertPoint(mismatch);
        auto puts = module->getOrInsertFunction("puts", b.getInt32Ty(), b.getPtrTy());
        b.CreateCall(puts, {b.CreateGlobalString("VOID: C primitive provider rejected the exact descriptor or fixture identity")});
        b.CreateRet(b.getInt32(1));
        b.SetInsertPoint(admitted);
    }
    if (input.coordinated) {
        auto* catalog=module->getNamedGlobal("__vd_types"+(source_program ? std::string("__void") : std::string{}));
        auto* count=module->getNamedGlobal("__vd_type_count"+(source_program ? std::string("__void") : std::string{}));
        auto* size_type=b.getIntNTy(module->getDataLayout().getPointerSizeInBits());
        auto* witnesses=module->getNamedGlobal("__vd_witnesses"+(source_program ? std::string("__void") : std::string{}));auto* witness_count=module->getNamedGlobal("__vd_witness_count"+(source_program ? std::string("__void") : std::string{}));
        if (dynamic) {
            const bool forward = source_program && input.application;
            auto* absent = llvm::ConstantPointerNull::get(b.getPtrTy());
            auto bootstrap = module->getOrInsertFunction("vd_runtime_entry_dynamic_v1", b.getInt32Ty(), b.getPtrTy(),
                b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), size_type, b.getPtrTy(), size_type,
                b.getInt32Ty(), b.getPtrTy(), b.getPtrTy(), size_type, b.getPtrTy(), size_type);
            result = b.CreateCall(bootstrap, {entry,input_value,failure,catalog,b.CreateLoad(size_type,count),
                witnesses,b.CreateLoad(size_type,witness_count),
                forward ? static_cast<llvm::Value*>(main->getArg(0)) : b.getInt32(0),
                forward ? static_cast<llvm::Value*>(main->getArg(1)) : absent,
                application_record ? static_cast<llvm::Value*>(application_record) : absent,
                llvm::ConstantInt::get(size_type, application_record ? input.application_metadata->bytes().size() : 0),
                b.CreateGlobalString(dynamic->bytes()),llvm::ConstantInt::get(size_type,dynamic->bytes().size())});
        } else if (application_record) {
            const bool forward = source_program && input.application;
            const bool wide = forward && llvm::Triple(module->getTargetTriple()).isOSWindows();
            auto bootstrap=module->getOrInsertFunction("vd_runtime_entry_context_v1",b.getInt32Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getInt32Ty(),b.getPtrTy(),b.getInt1Ty(),b.getPtrTy(),size_type);
            result=b.CreateCall(bootstrap,{entry,input_value,failure,catalog,b.CreateLoad(size_type,count),witnesses,b.CreateLoad(size_type,witness_count),
                forward?static_cast<llvm::Value*>(main->getArg(0)):b.getInt32(0),
                forward?static_cast<llvm::Value*>(main->getArg(1)):llvm::ConstantPointerNull::get(b.getPtrTy()),
                b.getInt1(wide),application_record,llvm::ConstantInt::get(size_type,input.application_metadata->bytes().size())});
        } else if (source_program && input.application)
            result=b.CreateCall(module->getOrInsertFunction("vd_runtime_entry_application",b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getInt32Ty(),b.getPtrTy(),b.getInt1Ty()),{entry,failure,catalog,b.CreateLoad(size_type,count),witnesses,b.CreateLoad(size_type,witness_count),main->getArg(0),main->getArg(1),b.getInt1(llvm::Triple(module->getTargetTriple()).isOSWindows())});
        else result=b.CreateCall(module->getOrInsertFunction("vd_runtime_entry_metadata",b.getInt32Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type),{entry,input_value,failure,catalog,b.CreateLoad(size_type,count),witnesses,b.CreateLoad(size_type,witness_count)});
    }
    else result=b.CreateCall(entry, arguments);
    if (failure) {
        auto* failed = llvm::BasicBlock::Create(context, "runtime_error", main);
        auto* success = llvm::BasicBlock::Create(context, "success", main);
        auto* message = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(failure_type, failure, 0));
        b.CreateCondBr(b.CreateIsNotNull(message), failed, success);
        b.SetInsertPoint(failed);
        // Use the platform CRT's stderr, preserving the existing captured source
        // messages and bounded trace without requiring Runtime for scalar objects.
#ifdef _WIN32
        auto* stream=b.CreateCall(module->getOrInsertFunction("__acrt_iob_func",b.getPtrTy(),b.getInt32Ty()),{b.getInt32(2)});
#else
#ifdef __APPLE__
        const char* stream_name="__stderrp";
#else
        const char* stream_name="stderr";
#endif
        auto* stream_global=module->getNamedGlobal(stream_name);
        if(!stream_global)stream_global=new llvm::GlobalVariable(*module,b.getPtrTy(),false,llvm::GlobalValue::ExternalLinkage,nullptr,stream_name);
        auto* stream=b.CreateLoad(b.getPtrTy(),stream_global);
#endif
        auto print_error=module->getOrInsertFunction("fprintf",llvm::FunctionType::get(b.getInt32Ty(),{b.getPtrTy(),b.getPtrTy()},true));
        auto* format=b.CreateGlobalString("%s");
        b.CreateCall(print_error, {stream,format,message});
        auto* count = b.CreateLoad(b.getInt32Ty(), b.CreateStructGEP(failure_type, failure, 1));
        auto* start = b.GetInsertBlock();
        auto* test = llvm::BasicBlock::Create(context, "trace", main);
        auto* body = llvm::BasicBlock::Create(context, "print_caller", main);
        auto* done = llvm::BasicBlock::Create(context, "failure_exit", main);
        b.CreateBr(test); b.SetInsertPoint(test);
        auto* index = b.CreatePHI(b.getInt32Ty(), 2); index->addIncoming(b.getInt32(0), start);
        b.CreateCondBr(b.CreateAnd(b.CreateICmpULT(index, count), b.CreateICmpULT(index, b.getInt32(32))), body, done);
        b.SetInsertPoint(body);
        auto* frames = b.CreateStructGEP(failure_type, failure, 2);
        auto* address = b.CreateGEP(llvm::ArrayType::get(b.getPtrTy(), 32), frames, {b.getInt32(0), index});
        b.CreateCall(print_error, {stream,format,b.CreateLoad(b.getPtrTy(), address)});
        auto* next = b.CreateAdd(index, b.getInt32(1)); index->addIncoming(next, body);
        b.CreateBr(test); b.SetInsertPoint(done);
        auto* truncated = llvm::BasicBlock::Create(context, "trace_limit", main);
        auto* exit = llvm::BasicBlock::Create(context, "exit", main);
        b.CreateCondBr(b.CreateICmpUGT(count, b.getInt32(32)), truncated, exit);
        b.SetInsertPoint(truncated);
        b.CreateCall(print_error, {stream,format,b.CreateGlobalString("\nSource call trace limited to 32 callers.\n")}); b.CreateBr(exit);
        b.SetInsertPoint(exit); b.CreateCall(print_error,{stream,format,b.CreateGlobalString("\n")}); b.CreateRet(b.getInt32(1)); b.SetInsertPoint(success);
    }
    auto print = module->getOrInsertFunction("printf",
        llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, true));
    if(input.application)b.CreateRet(result);
    else {b.CreateCall(print, {b.CreateGlobalString("%d\n"), result});
    b.CreateRet(b.getInt32(0));}
    }
#ifdef VOID_LLVM_RESEARCH
    if (storage_proof) {
        for (auto& function:*module) if (!function.isDeclaration()) {
            inventory.function(function,false); inventory.complete(function,llvm_backend::all_storage_roles());
        }
        auto save_ir = [&](const char* suffix) {
            std::error_code error;llvm::raw_fd_ostream file(output.string()+suffix,error);
            if(error) throw std::runtime_error("cannot write storage-proof IR");
            module->print(file,nullptr);
        };
        auto save_report = [&] {
            std::error_code error;llvm::raw_fd_ostream file(proof_report.string(),error);
            if(error) throw std::runtime_error("cannot write storage-proof certificate");
            file<<audited->report()<<"\n";
        };
        save_ir(".original.ll");audited->freeze(*module,inventory);save_ir(".entry.ll");
        audited->optimize();save_ir(".optimized.ll");
        try {audited->deliver(output);} catch (...) {save_report();throw;}
        save_report();return;
    }
#endif
    // Public AOT retains the original optimizer/emitter path. The proof service
    // and its CLI-home normalization are deliberately NOT activated here.
    // Analyze against the same generic deployment machine that emits the object.
    // JIT callers retain target-independent O2 and their existing ORC policy.
    ir_timing.finish();
    finish(*module, optimize, machine.get(), !input.coordinated);
    write_object(*module,*machine,output);
}
void emit_async_object(const ir::Module& source,const std::filesystem::path& output,bool optimize){auto module=source;module.async_root=true;emit_object(module,output,optimize,true);}
void emit_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, std::string_view build_inputs,
                 std::optional<project::DebugInfo> source_debug) {
    emit_impl(input,output,optimize,source_program,false,{},false,{},build_inputs,source_debug);
}
void emit_runtime_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, const std::string& generation_suffix) {
    emit_impl(input,output,optimize,false,true,generation_suffix);
}
#ifdef VOID_LLVM_RESEARCH
namespace llvm_backend {
void emit_private_fixed_call_research(const ir::Module& input, const std::filesystem::path& output,
                                     bool optimize) {
    initialize();
    auto machine=native_machine();
    llvm::LLVMContext context;
    auto module=lower(input,context,"",machine->createDataLayout(),input.entry_symbol,nullptr,nullptr,nullptr,true);
    module->setTargetTriple(machine->getTargetTriple());
    finish(*module,optimize,machine.get(),!input.coordinated);
    write_object(*module,*machine,output);
}
void emit_aot_storage_proof(const ir::Module& input, const std::filesystem::path& output,
                            bool optimize, const std::filesystem::path& report, ProofAdapter adapter,
                            const std::string& generation_suffix) {
    emit_impl(input,output,optimize,adapter == ProofAdapter::source,
              adapter == ProofAdapter::runtime,generation_suffix,true,report);
}
}
#endif
}
