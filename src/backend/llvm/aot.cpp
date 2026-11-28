#include "void/backend.hpp"
#include "lowering.hpp"
#include "native_abi.hpp"
#include "target.hpp"
#include "void/c_primitive.hpp"
#include "void/compiler_observation.hpp"
#include "void/frontend.hpp"
#include "void/llvm_storage_audit.hpp"
#include "void/runtime_abi.hpp"
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <llvm/IR/ModuleSummaryIndex.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/FileSystem.h>
#include "void/path.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <stdexcept>

namespace vd {
using llvm_backend::application_build_provenance;
using llvm_backend::call_c_primitive;
using llvm_backend::finish;
using llvm_backend::lower;
using llvm_backend::native_symbol;

static void emit_impl(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, bool runtime_object, const std::string& generation_suffix = {},
                      bool storage_proof = false, const std::filesystem::path& proof_report = {}, std::string_view build_inputs = {},
                      std::optional<project::DebugInfo> source_debug = {}, diagnostics::PrivateMap* private_map = nullptr, bool size = false, const PublicationResources* resources = nullptr, bool thin_lto = false) {
    if ((runtime_object && !input.coordinated) || (input.coordinated && !runtime_object && !source_program)) throw std::invalid_argument("coordinated AOT requires the runtime proof bootstrap");
    detail::initialize_native_target();
    llvm::LLVMContext context;
    bool entry_valid = false;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol)
        entry_valid = f.parameters == std::vector<ir::Type>{ir::Type::i32} && f.result == ir::Type::i32;
    for(const auto& f:input.functions)if(f.name==input.entry_symbol && f.asynchronous!=input.async_root)throw std::invalid_argument("Async AOT entry requires the explicit scheduler root adapter");
    if (!entry_valid) throw std::invalid_argument("AOT CLI adapter requires entry: i32(i32)");
    auto machine = detail::native_aot_machine(private_map != nullptr);
    // No-opt AOT must also disable machine scheduling/optimization. Otherwise
    // a later source line can execute before an earlier local's store, even
    // when IR optimization is off and the emitted debug locations are exact.
    if(!optimize)machine->setOptLevel(llvm::CodeGenOptLevel::None);
    if(size)machine->setOptLevel(llvm::CodeGenOptLevel::Default);
    llvm_backend::check_c_primitive_target(input.c_primitives, machine->getTargetTriple());
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
                        ,false,source_debug,optimize,private_map,resources);
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
    finish(*module, optimize, machine.get(), !input.coordinated && !size && !thin_lto, size, thin_lto);
    if (thin_lto) {
        observation::Scope timing(observation::Phase::native_object_emission);
        std::error_code error;
        llvm::raw_fd_ostream file(path_utf8(output), error, llvm::sys::fs::OF_None);
        if (error) throw std::runtime_error("Cannot write ThinLTO bitcode: " + error.message());
        llvm::ProfileSummaryInfo profile(*module);
        auto index = llvm::buildModuleSummaryIndex(*module, nullptr, &profile);
        llvm::WriteBitcodeToFile(*module, file, false, &index, true);
        file.close();
        if (file.has_error()) throw std::runtime_error("Cannot finish ThinLTO bitcode.");
    } else {
        detail::write_object(*module,*machine,output);
    }
}
void emit_async_object(const ir::Module& source,const std::filesystem::path& output,bool optimize){auto module=source;module.async_root=true;emit_object(module,output,optimize,true);}
void emit_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, std::string_view build_inputs,
                 std::optional<project::DebugInfo> source_debug) {
    emit_impl(input,output,optimize,source_program,false,{},false,{},build_inputs,source_debug);
}
diagnostics::PrivateMap emit_publication_object(const ir::Module& input, const std::filesystem::path& output,
                                                bool optimize, std::string_view semantic_inputs_sha256, bool size, bool debug,
                                                const PublicationResources* resources, bool thin_lto) {
    diagnostics::PrivateMap map(semantic_inputs_sha256);
    emit_impl(input,output,optimize,true,false,{},false,{}, {},debug ? project::DebugInfo::full : project::DebugInfo::none,&map,size,resources,thin_lto);
    return map;
}
void emit_runtime_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, const std::string& generation_suffix) {
    emit_impl(input,output,optimize,false,true,generation_suffix);
}
#ifdef VOID_LLVM_RESEARCH
namespace llvm_backend {
void emit_private_fixed_call_research(const ir::Module& input, const std::filesystem::path& output,
                                     bool optimize) {
    detail::initialize_native_target();
    auto machine=detail::native_aot_machine();
    llvm::LLVMContext context;
    auto module=lower(input,context,"",machine->createDataLayout(),input.entry_symbol,nullptr,nullptr,nullptr,true);
    module->setTargetTriple(machine->getTargetTriple());
    finish(*module,optimize,machine.get(),!input.coordinated);
    detail::write_object(*module,*machine,output);
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
