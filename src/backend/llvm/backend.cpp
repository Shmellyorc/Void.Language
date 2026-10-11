#include "void/backend.hpp"
#include <llvm/ADT/SmallVector.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/DIBuilder.h>
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
std::unique_ptr<llvm::Module> lower(const ir::Module& input, llvm::LLVMContext& context,
                                  const std::string& suffix) {
    ir::validate(input);
    auto module = std::make_unique<llvm::Module>(input.identity, context);
    llvm::IRBuilder<> b(context);
    auto type = [&](ir::Type t) -> llvm::Type* {
        if (t == ir::Type::boolean) return b.getInt1Ty();
        if (t == ir::Type::none) return b.getVoidTy();
        return b.getInt32Ty();
    };
    auto* signature = llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty()}, false);
    std::map<std::string, llvm::Function*> functions;
    for (const auto& name : input.imports)
        functions[name] = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, *module);
    for (const auto& f : input.functions) {
        std::vector<llvm::Type*> parameters;
        for (auto t : f.parameters) parameters.push_back(type(t));
        if (!f.module_identity.empty()) parameters.push_back(b.getPtrTy());
        auto* function_type = llvm::FunctionType::get(type(f.result), parameters, false);
        functions[f.name] = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, native_symbol(f, suffix), *module);
    }
    llvm::DIBuilder debug(*module);
    auto module_path = std::filesystem::path(input.source_path.empty() ? input.identity : input.source_path);
    auto* file = debug.createFile(module_path.filename().generic_string(), module_path.parent_path().generic_string());
    debug.createCompileUnit(llvm::dwarf::DW_LANG_C, file, "VOID source compiler", false, "", 0);
    auto* int_type = debug.createBasicType("i32", 32, llvm::dwarf::DW_ATE_signed);
    auto* bool_type = debug.createBasicType("bool", 1, llvm::dwarf::DW_ATE_boolean);
    auto debug_value_type = [&](ir::Type t) -> llvm::Metadata* {
        if (t == ir::Type::none) return nullptr;
        return t == ir::Type::boolean ? bool_type : int_type;
    };
    module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    for (const auto& f : input.functions) {
        auto* fn = functions.at(f.name);
        auto path = std::filesystem::path(f.source.path);
        auto* source_file = debug.createFile(path.filename().generic_string(), path.parent_path().generic_string());
        std::vector<llvm::Metadata*> debug_types{debug_value_type(f.result)};
        for (auto t : f.parameters) debug_types.push_back(debug_value_type(t));
        auto* debug_type = debug.createSubroutineType(debug.getOrCreateTypeArray(debug_types));
        auto* subprogram = debug.createFunction(source_file, f.source_name.empty() ? f.name : f.source_name, fn->getName(), source_file,
            f.source.line, debug_type, f.source.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        fn->setSubprogram(subprogram);
        std::vector<llvm::BasicBlock*> blocks;
        for (std::size_t i = 0; i < f.blocks.size(); ++i)
            blocks.push_back(llvm::BasicBlock::Create(context, "block" + std::to_string(i), fn));
        auto* failure_type = llvm::StructType::get(b.getPtrTy(), b.getInt32Ty(), llvm::ArrayType::get(b.getPtrTy(), 32));
        llvm::Value* failure = f.module_identity.empty() ? nullptr : fn->getArg(static_cast<unsigned>(f.parameters.size()));
        auto return_failure = [&] {
            if (f.result == ir::Type::none) b.CreateRetVoid();
            else b.CreateRet(llvm::Constant::getNullValue(type(f.result)));
        };
        std::vector<llvm::AllocaInst*> slots;
        b.SetInsertPoint(blocks.front());
        b.SetCurrentDebugLocation(llvm::DebugLoc());
        for (auto t : f.locals) {
            auto* slot = b.CreateAlloca(type(t)); slots.push_back(slot);
            b.CreateStore(llvm::Constant::getNullValue(type(t)), slot);
        }
        for (std::size_t n = 0; n < f.blocks.size(); ++n) {
            b.SetInsertPoint(blocks[n]);
            std::vector<llvm::Value*> values;
            for (const auto& instruction : f.blocks[n].instructions) {
                const auto& loc = instruction.source.path.empty() ? f.source : instruction.source;
                b.SetCurrentDebugLocation(llvm::DILocation::get(context, loc.line, loc.column, subprogram));
                llvm::Value* v = nullptr;
                using ir::Op;
                switch (instruction.op) {
                case Op::argument: v = fn->getArg(instruction.a); break;
                case Op::constant: v = llvm::ConstantInt::get(type(instruction.type), instruction.immediate, true); break;
                case Op::add: v = b.CreateAdd(values[instruction.a], values[instruction.b]); break;
                case Op::multiply: v = b.CreateMul(values[instruction.a], values[instruction.b]); break;
                case Op::less: v = b.CreateICmpSLT(values[instruction.a], values[instruction.b]); break;
                case Op::subtract: v = b.CreateSub(values[instruction.a], values[instruction.b]); break;
                case Op::divide: case Op::remainder: {
                    auto* valid = llvm::BasicBlock::Create(context, "division", fn);
                    auto* invalid = llvm::BasicBlock::Create(context, "zero_divisor", fn);
                    b.CreateCondBr(b.CreateICmpEQ(values[instruction.b], b.getInt32(0)), invalid, valid);
                    b.SetInsertPoint(invalid);
                    if (failure) {
                        auto message = source_message(loc, instruction.source_excerpt,
                            instruction.op == Op::divide ? "runtime error: Division by zero" : "runtime error: Remainder by zero");
                        message += "In function " + f.module_identity + "::" + f.source_name + "\n";
                        b.CreateStore(b.CreateGlobalStringPtr(message), b.CreateStructGEP(failure_type, failure, 0));
                        return_failure();
                    } else {
                        b.CreateCall(llvm::Intrinsic::getDeclaration(module.get(), llvm::Intrinsic::trap)); b.CreateUnreachable();
                    }
                    b.SetInsertPoint(valid);
                    // Widen before signed division so INT32_MIN / -1 is defined;
                    // truncation restores the provisional wrapping i32 contract.
                    auto* a = b.CreateSExt(values[instruction.a], b.getInt64Ty());
                    auto* divisor = b.CreateSExt(values[instruction.b], b.getInt64Ty());
                    v = b.CreateTrunc(instruction.op == Op::divide ? b.CreateSDiv(a, divisor) : b.CreateSRem(a, divisor), b.getInt32Ty()); break;
                }
                case Op::equal: v = b.CreateICmpEQ(values[instruction.a], values[instruction.b]); break;
                case Op::not_equal: v = b.CreateICmpNE(values[instruction.a], values[instruction.b]); break;
                case Op::less_equal: v = b.CreateICmpSLE(values[instruction.a], values[instruction.b]); break;
                case Op::greater: v = b.CreateICmpSGT(values[instruction.a], values[instruction.b]); break;
                case Op::greater_equal: v = b.CreateICmpSGE(values[instruction.a], values[instruction.b]); break;
                case Op::logical_not: v = b.CreateNot(values[instruction.a]); break;
                case Op::load: v = b.CreateLoad(type(f.locals[instruction.a]), slots[instruction.a]); break;
                case Op::store: v = b.CreateStore(values[instruction.b], slots[instruction.a]); break;
                case Op::call: {
                    std::vector<llvm::Value*> arguments;
                    for (auto id : instruction.arguments) arguments.push_back(values[id]);
                    if (arguments.empty() && !failure && functions.at(instruction.callee)->arg_size() == 1) arguments.push_back(values[instruction.a]);
                    auto* callee = functions.at(instruction.callee);
                    bool checked = callee->arg_size() == arguments.size() + 1;
                    if (checked && !failure) throw std::invalid_argument("Checked source calls require a source diagnostic context");
                    if (checked) arguments.push_back(failure);
                    v = b.CreateCall(callee, arguments);
                    if (checked) {
                        auto* failed = llvm::BasicBlock::Create(context, "propagate_error", fn);
                        auto* resume = llvm::BasicBlock::Create(context, "call_success", fn);
                        auto* message = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(failure_type, failure, 0));
                        b.CreateCondBr(b.CreateIsNotNull(message), failed, resume);
                        b.SetInsertPoint(failed);
                        auto* count_address = b.CreateStructGEP(failure_type, failure, 1);
                        auto* count = b.CreateLoad(b.getInt32Ty(), count_address);
                        auto* append = llvm::BasicBlock::Create(context, "trace_caller", fn);
                        auto* done = llvm::BasicBlock::Create(context, "error_return", fn);
                        auto* limit = llvm::BasicBlock::Create(context, "trace_limit", fn);
                        b.CreateCondBr(b.CreateICmpULT(count, b.getInt32(32)), append, limit);
                        b.SetInsertPoint(limit); b.CreateStore(b.getInt32(33), count_address); b.CreateBr(done);
                        b.SetInsertPoint(append);
                        auto trace = "Called from " + f.module_identity + "::" + f.source_name + "\n" +
                            source_message(loc, instruction.source_excerpt, "source call");
                        auto* frames = b.CreateStructGEP(failure_type, failure, 2);
                        auto* address = b.CreateGEP(llvm::ArrayType::get(b.getPtrTy(), 32), frames, {b.getInt32(0), count});
                        b.CreateStore(b.CreateGlobalStringPtr(trace), address);
                        b.CreateStore(b.CreateAdd(count, b.getInt32(1)), count_address);
                        b.CreateBr(done); b.SetInsertPoint(done); return_failure();
                        b.SetInsertPoint(resume);
                    }
                    break;
                }

                }
                values.push_back(v);
            }
            const auto& end = f.blocks[n].end;
            switch (end.kind) {
            case ir::Terminator::Kind::return_value: b.CreateRet(values[end.value]); break;
            case ir::Terminator::Kind::return_void: b.CreateRetVoid(); break;
            case ir::Terminator::Kind::jump: b.CreateBr(blocks[end.yes]); break;
            case ir::Terminator::Kind::branch: b.CreateCondBr(values[end.value], blocks[end.yes], blocks[end.no]); break;
            case ir::Terminator::Kind::unreachable: b.CreateUnreachable(); break;
            }
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
        Compatibility contract, const std::shared_ptr<Impl>& self, bool checked = false) {
#ifdef VOID_FUNCTION_SANITIZER_HASH
        // The host ABI is checked before reaching this boundary. Match the
        // compiler-generated plain/checked entry metadata without disabling UBSan.
        auto* function = module->getFunction(symbol);
        llvm::IRBuilder<> builder(*context);
        function->setMetadata("func_sanitize", llvm::MDNode::get(*context, {
            llvm::ConstantAsMetadata::get(builder.getInt32(VOID_FUNCTION_SANITIZER_MAGIC)),
            llvm::ConstantAsMetadata::get(builder.getInt32(checked ? VOID_CHECKED_SANITIZER_HASH : VOID_FUNCTION_SANITIZER_HASH))}));
#endif
        auto tracker = jit->getMainJITDylib().createResourceTracker();
        auto generation = std::make_shared<Generation>();
        generation->owner = self;
        generation->tracker = tracker;
        ++live;
        check(jit->addIRModule(tracker, llvm::orc::ThreadSafeModule(std::move(module), std::move(context))));
        auto address = unwrap(jit->lookup(symbol));
        if (checked) return std::make_shared<NativeCode>(address.toPtr<NativeCode::CheckedEntry>(), generation, std::move(contract));
        auto entry = address.toPtr<NativeCode::Entry>();
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
    const ir::Function* selected = nullptr;
    for (const auto& f : input.functions) if (f.name == entry) selected = &f;
    // Retain the source API's convenient entry-file lookup, never searching
    // imported functions by simple name. Proof IR continues to use exact names.
    if (!selected) for (const auto& f : input.functions)
        if (f.name == input.identity + "::" + entry && !f.module_identity.empty()) selected = &f;
    if (!selected) throw std::invalid_argument("entry function does not exist");
    if (selected->parameters != std::vector<ir::Type>{ir::Type::i32} || selected->result != ir::Type::i32)
        throw std::invalid_argument("host invocation requires i32(i32), not the selected function signature");
    auto expected_id = selected->module_identity.empty() ? input.identity + "::" + entry : selected->name;
    if (contract.function_id.empty()) contract.function_id = expected_id;
    if (contract.function_id != expected_id || contract.signature != "c:i32(i32)")
        throw std::invalid_argument("code identity or signature does not match IR");
    if (!selected->module_identity.empty()) contract.constant_contract = input.constant_contract;
    auto suffix = "__v" + std::to_string(++impl_->serial);
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = lower(input, *context, suffix);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple().str());
    finish(*module, optimize);
    return impl_->load(std::move(module), std::move(context), native_symbol(*selected, suffix), std::move(contract), impl_, !selected->module_identity.empty());
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
void emit_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program) {
    initialize();
    llvm::LLVMContext context;
    bool entry_valid = false;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol)
        entry_valid = f.parameters == std::vector<ir::Type>{ir::Type::i32} && f.result == ir::Type::i32;
    if (!entry_valid) throw std::invalid_argument("AOT CLI adapter requires entry: i32(i32)");
    auto module = lower(input, context, source_program ? "__void" : "");
    auto triple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!machine) throw std::runtime_error("cannot create native target machine");
    module->setTargetTriple(triple);
    module->setDataLayout(machine->createDataLayout());
    const ir::Function* entry_function = nullptr;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol) entry_function = &f;
    auto* entry = module->getFunction(native_symbol(*entry_function, source_program ? "__void" : ""));
    if (!entry || module->getFunction("main")) throw std::invalid_argument("AOT proof requires entry and no main");
    llvm::IRBuilder<> b(context);
    auto* main = llvm::Function::Create(llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty(), b.getPtrTy()}, false),
                                       llvm::Function::ExternalLinkage, "main", *module);
    b.SetInsertPoint(llvm::BasicBlock::Create(context, "entry", main));
    // Runtime input prevents the native proof from folding into a constant exit.
    llvm::Value* input_value = b.CreateSub(main->getArg(0), b.getInt32(2));
    if (source_program) {
        // CLI adaptation only: parsed VOID functions use the shared lowering.
        auto* parse = llvm::BasicBlock::Create(context, "parse_input", main);
        auto* ready = llvm::BasicBlock::Create(context, "run", main);
        auto* invalid = llvm::BasicBlock::Create(context, "invalid_input", main);
        auto* start = b.GetInsertBlock();
        b.CreateCondBr(b.CreateICmpEQ(main->getArg(0), b.getInt32(1)), ready, parse);
        b.SetInsertPoint(parse);
        auto* end = b.CreateAlloca(b.getPtrTy());
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
        b.CreateCall(puts, {b.CreateGlobalStringPtr("VOID: expected one optional decimal i32 input")}); b.CreateRet(b.getInt32(2));
        b.SetInsertPoint(ready);
        auto* selected = b.CreatePHI(b.getInt32Ty(), 2);
        selected->addIncoming(b.getInt32(0), start);
        auto* truncated = llvm::CastInst::CreateTruncOrBitCast(parsed, b.getInt32Ty(), "input", parse->getTerminator());
        selected->addIncoming(truncated, parse); input_value = selected;
    }
    std::vector<llvm::Value*> arguments{input_value};
    llvm::Value* failure = nullptr;
    auto* failure_type = llvm::StructType::get(b.getPtrTy(), b.getInt32Ty(), llvm::ArrayType::get(b.getPtrTy(), 32));
    if (!entry_function->module_identity.empty()) {
        failure = b.CreateAlloca(failure_type);
        b.CreateStore(llvm::Constant::getNullValue(failure_type), failure);
        arguments.push_back(failure);
    }
    auto* result = b.CreateCall(entry, arguments);
    if (failure) {
        auto* failed = llvm::BasicBlock::Create(context, "runtime_error", main);
        auto* success = llvm::BasicBlock::Create(context, "success", main);
        auto* message = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(failure_type, failure, 0));
        b.CreateCondBr(b.CreateIsNotNull(message), failed, success);
        b.SetInsertPoint(failed);
        auto puts = module->getOrInsertFunction("puts", llvm::FunctionType::get(b.getInt32Ty(), {b.getPtrTy()}, false));
        b.CreateCall(puts, {message});
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
        b.CreateCall(puts, {b.CreateLoad(b.getPtrTy(), address)});
        auto* next = b.CreateAdd(index, b.getInt32(1)); index->addIncoming(next, body);
        b.CreateBr(test); b.SetInsertPoint(done);
        auto* truncated = llvm::BasicBlock::Create(context, "trace_limit", main);
        auto* exit = llvm::BasicBlock::Create(context, "exit", main);
        b.CreateCondBr(b.CreateICmpUGT(count, b.getInt32(32)), truncated, exit);
        b.SetInsertPoint(truncated);
        b.CreateCall(puts, {b.CreateGlobalStringPtr("Source call trace limited to 32 callers.")}); b.CreateBr(exit);
        b.SetInsertPoint(exit); b.CreateRet(b.getInt32(1)); b.SetInsertPoint(success);
    }
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
