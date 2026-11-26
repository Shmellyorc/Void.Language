#include "void/backend.hpp"
#include "lowering.hpp"
#include "native_abi.hpp"
#include "native_cache.hpp"
#include "target.hpp"
#include "void/async.hpp"
#include "void/compiler_observation.hpp"
#include "void/frontend.hpp"
#include "void/filesystem_abi.hpp"
#include "void/host_abi.hpp"
#include "void/http_abi.hpp"
#include "void/os_abi.hpp"
#include "void/temporal.hpp"
#include "void/url_abi.hpp"
#include "void/native_runtime.hpp"
#include "void/reference_capabilities.hpp"
#include "void/runtime_abi.hpp"
#include "void/tooling.hpp"
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ObjectTransformLayer.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/MemoryBuffer.h>
#include <atomic>
#include <cmath>
#include <stdexcept>

namespace vd {
using llvm_backend::application_build_provenance;
using llvm_backend::finish;
using llvm_backend::global_contract;
using llvm_backend::lower;
using llvm_backend::native_symbol;

namespace {
void check(llvm::Error error) {
    if (error) throw std::runtime_error(llvm::toString(std::move(error)));
}
template<class T> T unwrap(llvm::Expected<T> value) {
    if (!value) throw std::runtime_error(llvm::toString(value.takeError()));
    return std::move(*value);
}

std::shared_ptr<const DynamicBinding> prepare_dynamic(const CPrimitiveAdmission::Owner& admission,
                                                            const std::shared_ptr<Runtime>& runtime) {
    if(!admission || !admission->dynamic())return {};
    if(!runtime)throw std::invalid_argument("C dynamic closure requires a coordinated Runtime");
    auto dynamic=runtime->dynamic_binding();
    if(!dynamic)return load_dynamic(admission->dynamic(),admission->dynamic_root());
    if(!matches_dynamic(*dynamic,*admission->dynamic(),admission->dynamic_root()))
        throw std::invalid_argument("C dynamic closure is incompatible with the Runtime binding");
    return dynamic;
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
    detail::initialize_native_target();
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
    llvm_backend::check_c_primitive_target(input.c_primitives, impl_->jit->getTargetTriple());
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
    auto module = lower(input, *context, suffix, impl_->jit->getDataLayout(),selected->name,
                        nullptr,nullptr,nullptr,false,{},optimize);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple());
    ir_timing.finish();
    finish(*module, optimize);
    auto dynamic=prepare_dynamic(input.c_primitives,runtime);
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
    const auto& native=analysis.c_primitive_admission();
    // Parsed closures identify every image; managed conventional plans identify
    // both provider and adapter in their verified payload. Unmanaged conventional
    // plans name files without hashing them, just as linked thunks lack a stable
    // implementation identity. Neither is eligible for persistent reuse.
    if (native && (!native->dynamic() ||
        (native->dynamic()->conventional() && native->dynamic()->payload().empty()))) {
        if (!impl_->cache.project_root.empty()) impl_->cache_status = "unavailable";
        return cold();
    }
    if(impl_->custom_bindings || impl_->cache.project_root.empty())return cold();
    llvm_backend::check_c_primitive_target(native,impl_->jit->getTargetTriple());
    observation::Scope lookup_timing(observation::Phase::cache_lookup);
    std::string identity;
    try {
    identity="VOID-native-object-v2";
    auto field=[&](std::string_view value){llvm_backend::cache_field(identity,value);};
    field(impl_->cache.build_identity);field("integrated-log-v1;rooted-local-cleanup;join-utf8;compact-sync-write");field("checked-root-location-v2");field(std::to_string(sizeof(Runtime::Frame)));field(formatting::abi_version);field(temporal::abi_version);field(impl_->target_identity);field(analysis.entry_identity());field(async_root?"async-root-v1":"sync-root");
    field(optimize?"ir-O2":"no-ir-O2");
    field(impl_->emission==ir::Emission::entry?"entry-emission-v1":"all-exports");
    // Dynamic source lowering is always coordinated, so supplying its live
    // Runtime for replacement does not change generated code. Share this new
    // eligible family with ordinary runs; preserve every existing key recipe.
    field(analysis.compatibility_contract());field(analysis.constant_contract());field(runtime && !native ? "forced-runtime" : "automatic-runtime");
    field(analysis.application_metadata()?std::string_view(analysis.application_metadata()->bytes()):std::string_view{});
    field(std::to_string(analysis.modules().size()));
    for(const auto& m:analysis.modules()) {field("module");field(m.identity);field(m.source.path());field(m.source.text());field(std::to_string(m.imports.size()));for(const auto& i:m.imports)field(i);}
    if(native) {
        field("admitted-dynamic-object-v1");field(native->descriptor());field(native->module());
        field(native->target());field(native->dynamic()->bytes());
        // The per-invocation staging root is authority held by the fresh
        // admission/Runtime, never an object identity or serialized address.
    }
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
            if ((globals || native) && !artifact->coordinated) throw std::runtime_error("cache coordination mismatch");
            if (artifact->coordinated && !owner) {
                if (analysis.application_metadata() || globals || native) owner = std::make_shared<Runtime>(256,Runtime::AllocationPolicy::live);
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
            contract.native_thunk_identity=native && native->revision()==3 ? reinterpret_cast<std::uintptr_t>(native->thunk()) : 0;
            contract.native_dynamic_contract=native ? native->dynamic_contract() : std::string{};
            if (contract.application_metadata)
                contract.build_provenance=application_build_provenance(&analysis,*contract.application_metadata,impl_->target_identity,optimize,impl_->cache.build_identity);
            ir::Function entry;entry.name=expected;entry.module_identity=analysis.entry_identity();
            auto dynamic=prepare_dynamic(native,owner);
            auto result=impl_->load({}, {},native_symbol(entry,"__cache"),contract,impl_,true,owner,"__cache",true,std::move(object),native,dynamic);
            if(dynamic)owner->bind_dynamic(std::move(dynamic));
            if (globals) owner->admit_globals(global_contract(analysis), impl_->replacing_globals);
            store.validated();
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
}
