#include "void/backend.hpp"
#include <llvm/ADT/SmallVector.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>
namespace vd {
namespace {
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
void finish(llvm::Module& module, bool optimize) {
    std::string message;
    llvm::raw_string_ostream stream(message);
    if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid LLVM lowering: " + message);
    if (optimize) {
        llvm::LoopAnalysisManager loops;
        llvm::FunctionAnalysisManager functions;
        llvm::CGSCCAnalysisManager cgscc;
        llvm::ModuleAnalysisManager modules;
        llvm::PassBuilder builder;
        builder.registerModuleAnalyses(modules);
        builder.registerCGSCCAnalyses(cgscc);
        builder.registerFunctionAnalyses(functions);
        builder.registerLoopAnalyses(loops);
        builder.crossRegisterProxies(loops, functions, cgscc, modules);
        auto pipeline = builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
        pipeline.run(module, modules);
        if (llvm::verifyModule(module, &stream)) throw std::runtime_error("invalid optimized LLVM IR: " + message);
    }
}
std::unique_ptr<llvm::Module> lower(const ir::Module& input, llvm::LLVMContext& context,
                                  const std::string& suffix) {
    ir::validate(input);
    auto module = std::make_unique<llvm::Module>(input.identity, context);
    llvm::IRBuilder<> b(context);
    auto* signature = llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty()}, false);
    std::map<std::string, llvm::Function*> functions;
    for (const auto& name : input.imports)
        functions[name] = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, *module);
    for (const auto& f : input.functions)
        functions[f.name] = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, f.name + suffix, *module);
    llvm::DIBuilder debug(*module);
    auto* file = debug.createFile("architecture.void", "examples");
    debug.createCompileUnit(llvm::dwarf::DW_LANG_C, file, "VOID architecture proof", false, "", 0);
    auto* int_type = debug.createBasicType("i32", 32, llvm::dwarf::DW_ATE_signed);
    auto types = debug.getOrCreateTypeArray({int_type, int_type});
    auto* debug_type = debug.createSubroutineType(types);
    module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    for (const auto& f : input.functions) {
        auto* fn = functions.at(f.name);
        auto path = std::filesystem::path(f.source.path);
        auto* source_file = debug.createFile(path.filename().generic_string(), path.parent_path().generic_string());
        auto* subprogram = debug.createFunction(source_file, f.name, fn->getName(), source_file,
            f.source.line, debug_type, f.source.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        fn->setSubprogram(subprogram);
        std::vector<llvm::BasicBlock*> blocks;
        for (std::size_t i = 0; i < f.blocks.size(); ++i)
            blocks.push_back(llvm::BasicBlock::Create(context, "block" + std::to_string(i), fn));
        for (std::size_t n = 0; n < f.blocks.size(); ++n) {
            b.SetInsertPoint(blocks[n]);
            std::vector<llvm::Value*> values;
            for (const auto& instruction : f.blocks[n].instructions) {
                const auto& loc = instruction.source.path.empty() ? f.source : instruction.source;
                b.SetCurrentDebugLocation(llvm::DILocation::get(context, loc.line, loc.column, subprogram));
                llvm::Value* v = nullptr;
                using ir::Op;
                switch (instruction.op) {
                case Op::argument: v = fn->getArg(0); break;
                case Op::constant: v = b.getInt32(instruction.immediate); break;
                case Op::add: v = b.CreateAdd(values[instruction.a], values[instruction.b]); break;
                case Op::multiply: v = b.CreateMul(values[instruction.a], values[instruction.b]); break;
                case Op::less: v = b.CreateICmpSLT(values[instruction.a], values[instruction.b]); break;
                case Op::call: v = b.CreateCall(functions.at(instruction.callee), {values[instruction.a]}); break;
                }
                values.push_back(v);
            }
            const auto& end = f.blocks[n].end;
            if (end.kind == ir::Terminator::Kind::return_value) b.CreateRet(values[end.value]);
            else b.CreateCondBr(values[end.value], blocks[end.yes], blocks[end.no]);
        }
    }
    debug.finalize();
    return module;
}
}
struct Jit::Impl {
    std::unique_ptr<llvm::orc::LLJIT> jit;
    std::uint64_t serial = 0;
    std::atomic<std::size_t> live{0};
    struct Generation {
        std::shared_ptr<Impl> owner;
        llvm::orc::ResourceTrackerSP tracker;
        ~Generation() {
            if (auto error = tracker->remove())
                llvm::errs() << "VOID JIT retirement failed: " << llvm::toString(std::move(error)) << '\n';
            --owner->live;
        }
    };
    std::shared_ptr<const NativeCode> load(std::unique_ptr<llvm::Module> module,
        std::unique_ptr<llvm::LLVMContext> context, const std::string& symbol,
        Compatibility contract, const std::shared_ptr<Impl>& self) {
        auto tracker = jit->getMainJITDylib().createResourceTracker();
        auto generation = std::make_shared<Generation>();
        generation->owner = self;
        generation->tracker = tracker;
        ++live;
        check(jit->addIRModule(tracker, llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
        auto entry = unwrap(jit->lookup(symbol)).toPtr<NativeCode::Entry>();
        return std::make_shared<NativeCode>(entry, generation, std::move(contract));
    }
};
Jit::Jit() : impl_(std::make_shared<Impl>()) {
    initialize();
    impl_->jit = unwrap(llvm::orc::LLJITBuilder().create());
}
Jit::~Jit() = default;
void Jit::bind_address(std::string name, std::uintptr_t entry) {
    if (!entry) throw std::invalid_argument("null native import");
    llvm::orc::SymbolMap symbols;
    symbols[impl_->jit->mangleAndIntern(name)] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr(entry), llvm::JITSymbolFlags::Exported);
    check(impl_->jit->getMainJITDylib().define(llvm::orc::absoluteSymbols(symbols)));
}
std::shared_ptr<const NativeCode> Jit::compile(const ir::Module& input, const std::string& entry,
                                              bool optimize, Compatibility contract) {
    ir::validate(input);
    bool found = false;
    for (const auto& f : input.functions) if (f.name == entry) found = true;
    if (!found) throw std::invalid_argument("entry function does not exist");
    auto expected_id = input.identity + "::" + entry;
    if (contract.function_id.empty()) contract.function_id = expected_id;
    if (contract.function_id != expected_id || contract.signature != "c:i32(i32)")
        throw std::invalid_argument("code identity or signature does not match IR");
    auto suffix = "__v" + std::to_string(++impl_->serial);
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = lower(input, *context, suffix);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple().str());
    finish(*module, optimize);
    return impl_->load(std::move(module), std::move(context), entry + suffix, std::move(contract), impl_);
}
std::size_t Jit::live_generations() const { return impl_->live.load(); }
std::shared_ptr<const NativeCode> Jit::compile_root_probe(bool optimize) {
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("root proof", *context);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple().str());
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
void emit_object(const ir::Module& input, const std::filesystem::path& output, bool optimize) {
    initialize();
    llvm::LLVMContext context;
    auto module = lower(input, context, "");
    auto triple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!machine) throw std::runtime_error("cannot create native target machine");
    module->setTargetTriple(triple);
    module->setDataLayout(machine->createDataLayout());
    auto* entry = module->getFunction("entry");
    if (!entry || module->getFunction("main")) throw std::invalid_argument("AOT proof requires entry and no main");
    llvm::IRBuilder<> b(context);
    auto* main = llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty(), b.getPtrTy()}, false),
                                       llvm::Function::ExternalLinkage, "main", *module);
    b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main));
    // Runtime input prevents the native proof from folding into a constant exit.
    auto* result = b.CreateCall(entry, {b.CreateSub(main->getArg(0), b.getInt32(2))});
    auto print = module->getOrInsertFunction("printf",
        llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, true));
    b.CreateCall(print, {b.CreateGlobalStringPtr("%d\n"), result});
    b.CreateRet(b.getInt32(0));
    finish(*module, optimize);
    std::error_code ec;
    llvm::raw_fd_ostream file(output.string(), ec, llvm::sys::fs::OF_None);
    if (ec) throw std::runtime_error("cannot write object: " + ec.message());
    llvm::legacy::PassManager passes;
    if (machine->addPassesToEmitFile(passes, file, nullptr, llvm::CodeGenFileType::ObjectFile))
        throw std::runtime_error("object emission unavailable");
    passes.run(*module);
    file.flush();
    if (file.has_error()) throw std::runtime_error("object write failed");
}
}
