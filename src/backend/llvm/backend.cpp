#include "void/path.hpp"
#include "void/backend.hpp"
#include "void/frontend.hpp"
#include "void/numeric.hpp"
#include "void/runtime_abi.hpp"
#include <cmath>
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
                                  const std::string& suffix, const llvm::DataLayout& target_layout) {
    ir::InstructionTypes checked_types;
    ir::validate(input, &checked_types);
    auto module = std::make_unique<llvm::Module>(input.identity, context);
    module->setDataLayout(target_layout);
    // StructLayout caches use raw context-owned Type pointers as keys. Query
    // the module-owned copy, never the LLJIT layout that outlives these types.
    const auto& layout = module->getDataLayout();
    llvm::IRBuilder<> b(context);
    std::map<std::string,llvm::StructType*> aggregates;
    std::function<llvm::Type*(ir::Type)> type = [&](ir::Type t) -> llvm::Type* {
        if (t.kind == ir::Type::managed_ref) return b.getPtrTy();
        if (t.kind == ir::Type::aggregate) {
            if (auto found = aggregates.find(t.definition->identity); found != aggregates.end()) return found->second;
            auto* result = llvm::StructType::create(context,t.definition->identity); aggregates.emplace(t.definition->identity,result);
            std::vector<llvm::Type*> fields; for (const auto& field : t.definition->fields) fields.push_back(type(field.type));
            result->setBody(fields,false); return result;
        }
        if (t == ir::Type::boolean) return b.getInt1Ty();
        if (t == ir::Type::none) return b.getVoidTy();
        if (t == ir::Type::f32) return b.getFloatTy();
        if (t == ir::Type::f64) return b.getDoubleTy();
        return b.getIntNTy(numeric::width(t));
    };
    auto* signature = llvm::FunctionType::get(b.getInt32Ty(), {b.getInt32Ty()}, false);
    std::map<std::string, llvm::Function*> functions;
    for (const auto& name : input.imports)
        functions[name] = llvm::Function::Create(signature, llvm::Function::ExternalLinkage, name, *module);
    for (const auto& f : input.functions) {
        std::vector<llvm::Type*> parameters;
        for (unsigned n = 0; n < f.parameters.size(); ++n) parameters.push_back(f.receiver && n == 0 ? b.getPtrTy() : type(f.parameters[n]));
        if (!f.module_identity.empty() || input.coordinated) parameters.push_back(b.getPtrTy());
        auto* function_type = llvm::FunctionType::get(type(f.result), parameters, false);
        functions[f.name] = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, native_symbol(f, suffix), *module);
    }
    llvm::DIBuilder debug(*module);
    auto module_path = vd::utf8_path(input.source_path.empty() ? input.identity : input.source_path);
    auto* file = debug.createFile(vd::path_utf8(module_path.filename()), vd::path_utf8(module_path.parent_path()));
    debug.createCompileUnit(llvm::dwarf::DW_LANG_C, file, "VOID source compiler", false, "", 0);
    std::map<std::string,llvm::DIType*> debug_aggregates;
    std::function<llvm::DIType*(ir::Type)> debug_value_type = [&](ir::Type t) -> llvm::DIType* {
        if (t.kind == ir::Type::managed_ref) return debug.createPointerType(nullptr, layout.getPointerSizeInBits());
        if (t == ir::Type::none) return nullptr;
        if (t.kind == ir::Type::aggregate) {
            if (auto found = debug_aggregates.find(t.definition->identity); found != debug_aggregates.end()) return found->second;
            auto* native = llvm::cast<llvm::StructType>(type(t)); auto* offsets = layout.getStructLayout(native);
            std::vector<llvm::Metadata*> members;
            for (unsigned n = 0; n < t.definition->fields.size(); ++n) {
                const auto& field = t.definition->fields[n]; auto* native_field = type(field.type);
                members.push_back(debug.createMemberType(file,field.name,file,0,layout.getTypeSizeInBits(native_field),
                    layout.getABITypeAlign(native_field).value()*8,offsets->getElementOffsetInBits(n),llvm::DINode::FlagZero,debug_value_type(field.type)));
            }
            auto* result = debug.createStructType(file,ir::type_name(t),file,0,offsets->getSizeInBits(),
                layout.getABITypeAlign(native).value()*8,llvm::DINode::FlagZero,nullptr,debug.getOrCreateArray(members));
            debug_aggregates.emplace(t.definition->identity,result); return result;
        }
        return debug.createBasicType(ir::type_name(t),numeric::width(t), t == ir::Type::boolean ? llvm::dwarf::DW_ATE_boolean : numeric::floating(t) ? llvm::dwarf::DW_ATE_float : numeric::signed_integer(t) ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_unsigned);
    };
    module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    std::size_t function_index = 0;
    for (const auto& f : input.functions) {
        const auto& instruction_types = checked_types.at(function_index++);
        auto* fn = functions.at(f.name);
        auto path = vd::utf8_path(f.source.path);
        auto* source_file = debug.createFile(vd::path_utf8(path.filename()), vd::path_utf8(path.parent_path()));
        std::vector<llvm::Metadata*> debug_types{debug_value_type(f.result)};
        for (unsigned n = 0; n < f.parameters.size(); ++n) {
            auto* logical = debug_value_type(f.parameters[n]);
            if (f.receiver && n == 0) logical = debug.createReferenceType(llvm::dwarf::DW_TAG_reference_type,logical,layout.getPointerSizeInBits(),layout.getPointerABIAlignment(0).value()*8);
            debug_types.push_back(logical);
        }
        auto* debug_type = debug.createSubroutineType(debug.getOrCreateTypeArray(debug_types));
        auto* subprogram = debug.createFunction(source_file, f.source_name.empty() ? f.name : f.source_name, fn->getName(), source_file,
            f.source.line, debug_type, f.source.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        fn->setSubprogram(subprogram);
        std::vector<llvm::BasicBlock*> blocks;
        for (std::size_t i = 0; i < f.blocks.size(); ++i)
            blocks.push_back(llvm::BasicBlock::Create(context, "block" + std::to_string(i), fn));
        auto* failure_type = llvm::StructType::get(b.getPtrTy(), b.getInt32Ty(), llvm::ArrayType::get(b.getPtrTy(), 32));
        llvm::Value* failure = f.module_identity.empty() && !input.coordinated ? nullptr : fn->getArg(static_cast<unsigned>(f.parameters.size()));
        llvm::Value* root_frame = nullptr;
        auto leave_frame = [&] {
            if (input.coordinated) b.CreateCall(module->getOrInsertFunction("vd_gc_frame_leave", b.getVoidTy(), b.getPtrTy()), {root_frame});
        };
        auto return_failure = [&] {
            leave_frame();
            if (f.result == ir::Type::none) b.CreateRetVoid();
            else b.CreateRet(llvm::Constant::getNullValue(type(f.result)));
        };
        std::vector<llvm::Value*> slots;
        b.SetInsertPoint(blocks.front());
        b.SetCurrentDebugLocation(llvm::DebugLoc());
        for (unsigned n = 0; n < f.locals.size(); ++n) {
            auto t = f.locals[n];
            if (f.receiver && n == 0 && t.kind==ir::Type::aggregate) { slots.push_back(fn->getArg(0)); continue; }
            auto* slot = b.CreateAlloca(type(t)); slots.push_back(slot);
            b.CreateStore(f.receiver && n==0 && t.kind==ir::Type::managed_ref ? static_cast<llvm::Value*>(fn->getArg(0)) : llvm::Constant::getNullValue(type(t)), slot);
        }
        std::map<unsigned,llvm::Value*> stage_storage;
        for (const auto& block:f.blocks) for (const auto& instruction:block.instructions) if (instruction.op==ir::Op::stage_begin) {
            auto* native=type(ir::Type(instruction.type.definition));
            auto bytes=std::max<std::uint64_t>(1,layout.getTypeAllocSize(native));
            auto* storage=b.CreateAlloca(b.getInt8Ty(),b.getInt64(bytes));
            storage->setAlignment(layout.getABITypeAlign(native));
            b.CreateMemSet(storage,b.getInt8(0),bytes,layout.getABITypeAlign(native));
            stage_storage.emplace(instruction.a,storage);
        }
        std::function<void(ir::Type,std::uint64_t,std::vector<std::uint64_t>&)> reference_offsets = [&](ir::Type t, std::uint64_t offset, auto& result) {
            if (t.construction) return; // A view points to stack storage, never a heap root.
            if (t.kind == ir::Type::managed_ref) result.push_back(offset);
            else if (t.kind == ir::Type::aggregate) {
                auto* native = llvm::cast<llvm::StructType>(type(t));
                auto* offsets = layout.getStructLayout(native);
                for (unsigned i = 0; i < t.definition->fields.size(); ++i)
                    reference_offsets(t.definition->fields[i].type, offset + offsets->getElementOffset(i), result);
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
        std::vector<std::vector<llvm::Value*>> value_roots(f.blocks.size());
        if (input.coordinated) {
            auto* slot_type = llvm::StructType::get(b.getPtrTy(), size_type, b.getPtrTy(), size_type);
            const auto* slot_layout = layout.getStructLayout(slot_type);
            if (slot_layout->getSizeInBytes() != sizeof(Runtime::Slot) ||
                slot_layout->getElementOffset(1) != offsetof(Runtime::Slot,size) ||
                slot_layout->getElementOffset(2) != offsetof(Runtime::Slot,offsets) ||
                slot_layout->getElementOffset(3) != offsetof(Runtime::Slot,count))
                throw std::invalid_argument("runtime root ABI does not match target");
            std::vector<llvm::Value*> entries;
            auto add_root = [&](ir::Type t, llvm::Value* address) {
                std::vector<std::uint64_t> offsets;
                reference_offsets(t, 0, offsets);
                if (offsets.empty()) return;
                llvm::Value* entry = llvm::UndefValue::get(slot_type);
                entry = b.CreateInsertValue(entry,address,{0});
                entry = b.CreateInsertValue(entry,llvm::ConstantInt::get(size_type,layout.getTypeAllocSize(type(t))),{1});
                entry = b.CreateInsertValue(entry,offset_map(offsets),{2});
                entry = b.CreateInsertValue(entry,llvm::ConstantInt::get(size_type,offsets.size()),{3});
                entries.push_back(entry);
            };
            for (unsigned i = 0; i < slots.size(); ++i) add_root(f.locals[i],slots[i]);
            for (auto [slot,storage]:stage_storage) add_root(ir::Type(f.locals[slot].definition),storage);
            for (unsigned i = 0; i < f.parameters.size(); ++i) {
                if (f.parameters[i].construction) { add_root(ir::Type(f.parameters[i].definition),fn->getArg(i)); continue; }
                if (f.receiver && i == 0) continue;
                std::vector<std::uint64_t> offsets; reference_offsets(f.parameters[i],0,offsets);
                if (offsets.empty()) continue;
                auto* storage = b.CreateAlloca(type(f.parameters[i]));
                b.CreateStore(fn->getArg(i),storage); add_root(f.parameters[i],storage);
            }
            for (unsigned block = 0; block < f.blocks.size(); ++block)
                for (auto t : instruction_types[block]) {
                    std::vector<std::uint64_t> offsets; reference_offsets(t,0,offsets);
                    llvm::Value* storage = nullptr;
                    if (!offsets.empty()) {
                        storage = b.CreateAlloca(type(t));
                        b.CreateStore(llvm::Constant::getNullValue(type(t)),storage);
                        add_root(t,storage);
                    }
                    value_roots[block].push_back(storage);
                }
            auto* root_array_type = llvm::ArrayType::get(slot_type,entries.size());
            auto* root_array = b.CreateAlloca(root_array_type);
            for (unsigned i = 0; i < entries.size(); ++i)
                b.CreateStore(entries[i],b.CreateInBoundsGEP(root_array_type,root_array,{b.getInt32(0),b.getInt32(i)}));
            auto* frame = b.CreateAlloca(b.getInt8Ty(), b.getInt64(sizeof(Runtime::Frame)));
            frame->setAlignment(llvm::Align(alignof(Runtime::Frame))); root_frame = frame;
            b.CreateCall(module->getOrInsertFunction("vd_gc_frame_enter",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type),
                {root_frame,root_array,llvm::ConstantInt::get(size_type,entries.size())});
        }
        for (std::size_t n = 0; n < f.blocks.size(); ++n) {
            b.SetInsertPoint(blocks[n]);
            if (input.coordinated) b.CreateCall(module->getOrInsertFunction("vd_gc_poll",b.getVoidTy()));
            std::vector<llvm::Value*> values;
            std::vector<ir::Type> value_types;
            for (const auto& instruction : f.blocks[n].instructions) {
                const auto& loc = instruction.source.path.empty() ? f.source : instruction.source;
                b.SetCurrentDebugLocation(llvm::DILocation::get(context, loc.line, loc.column, subprogram));
                llvm::Value* v = nullptr;
                using ir::Op;
                auto fail_if = [&](llvm::Value* invalid_value, const std::string& description) {
                    auto* invalid = llvm::BasicBlock::Create(context,"numeric_error",fn);
                    auto* valid = llvm::BasicBlock::Create(context,"numeric_valid",fn);
                    b.CreateCondBr(invalid_value,invalid,valid); b.SetInsertPoint(invalid);
                    if (failure) {
                        auto message = source_message(loc,instruction.source_excerpt,"runtime error: " + description);
                        message += "In function " + f.module_identity + "::" + f.source_name + "\n";
                        b.CreateStore(b.CreateGlobalStringPtr(message),b.CreateStructGEP(failure_type,failure,0)); return_failure();
                    } else { b.CreateCall(llvm::Intrinsic::getDeclaration(module.get(),llvm::Intrinsic::trap)); b.CreateUnreachable(); }
                    b.SetInsertPoint(valid);
                };
                // IR operand types preserve signedness, even when native widths coincide.
                auto operand_type = [&](unsigned id) { return value_types.at(id); };
                switch (instruction.op) {
                case Op::gc_poll: case Op::gc_collect: {
                    if (instruction.op == Op::gc_poll) v = b.CreateCall(module->getOrInsertFunction("vd_gc_poll",b.getVoidTy()));
                    else {
                        v = b.CreateCall(module->getOrInsertFunction("vd_gc_collect",b.getVoidTy(),b.getPtrTy()),{failure});
                        fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Collection failed");
                    }
                    break;
                }
                case Op::gc_alive:
                    v = b.CreateCall(module->getOrInsertFunction("vd_gc_alive",b.getInt1Ty(),b.getPtrTy()),{values[instruction.a]}); break;
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
                    auto* payload=allocations.CreateAlloca(b.getInt8Ty(),b.getInt64(bytes));
                    payload->setAlignment(layout.getABITypeAlign(native));
                    b.CreateMemSet(payload,b.getInt8(0),bytes,layout.getABITypeAlign(native));
                    for (unsigned field=0;field<instruction.arguments.size();++field) {
                        auto* value=values[instruction.arguments[field]];
                        ir::Type field_type=instruction.type.definition->fields[field].type;
                        if (field_type.kind==ir::Type::managed_ref && !field_type.nullable) fail_if(b.CreateIsNull(value),"Null supplied for non-nullable field");
                        b.CreateStore(value,b.CreateStructGEP(native,payload,field));
                    }
                    std::vector<std::uint64_t> offsets; reference_offsets(object,0,offsets);
                    auto* storage=value_roots[n].at(values.size());
                    b.CreateCall(module->getOrInsertFunction("vd_gc_construct",b.getVoidTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type,b.getPtrTy(),b.getPtrTy()),
                        {storage,llvm::ConstantInt::get(size_type,bytes),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size()),payload,failure});
                    fail_if(b.CreateIsNotNull(b.CreateLoad(b.getPtrTy(),b.CreateStructGEP(failure_type,failure,0))),"Managed construction allocation failed");
                    v=b.CreateLoad(type(instruction.type),storage); break;
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
                    if (object.construction && instruction.op==Op::gc_write && field.kind==ir::Type::managed_ref && !field.nullable)
                        fail_if(b.CreateIsNull(values[instruction.b]),"Null supplied for required construction field");
                    auto* address = b.CreateStructGEP(type(ir::Type(object.definition)),values[instruction.a],static_cast<unsigned>(instruction.immediate));
                    if (instruction.op == Op::gc_read) v = b.CreateLoad(type(object.definition->fields.at(static_cast<std::size_t>(instruction.immediate)).type),address);
                    else v = b.CreateStore(values[instruction.b],address);
                    break;
                }
                case Op::aggregate:
                    v = llvm::Constant::getNullValue(type(instruction.type));
                    for (unsigned n = 0; n < instruction.arguments.size(); ++n) v = b.CreateInsertValue(v,values[instruction.arguments[n]],{n});
                    break;
                case Op::extract: v = b.CreateExtractValue(values[instruction.a],{static_cast<unsigned>(instruction.immediate)}); break;
                case Op::insert: v = b.CreateInsertValue(values[instruction.a],values[instruction.b],{static_cast<unsigned>(instruction.immediate)}); break;
                case Op::argument:
                    if (f.receiver && f.parameters[0].kind==ir::Type::aggregate && instruction.a == 0) v = b.CreateLoad(type(f.parameters[0]),fn->getArg(0));
                    else v = fn->getArg(instruction.a);
                    break;
                case Op::constant:
                    if (instruction.type.kind == ir::Type::managed_ref) v = llvm::ConstantPointerNull::get(b.getPtrTy());
                    else if (numeric::floating(instruction.type)) v = llvm::ConstantFP::get(context,llvm::APFloat(instruction.type == ir::Type::f32 ? llvm::APFloat::IEEEsingle() : llvm::APFloat::IEEEdouble(),llvm::APInt(numeric::width(instruction.type),static_cast<std::uint64_t>(instruction.immediate))));
                    else v = llvm::ConstantInt::get(type(instruction.type),instruction.immediate,true);
                    break;
                case Op::negate: v = values[instruction.a]->getType()->isFloatingPointTy() ? b.CreateFNeg(values[instruction.a]) : b.CreateNeg(values[instruction.a]); break;
                case Op::convert: {
                    auto* a = values[instruction.a]; auto from = operand_type(instruction.a), to = instruction.type;
                    if (from.kind==ir::Type::managed_ref) { fail_if(b.CreateIsNull(a),"Null managed reference in checked conversion"); v=a; break; }
                    if (from == to) { v = a; break; }
                    if (numeric::integer(from) && numeric::integer(to)) {
                        auto* wide = numeric::signed_integer(from) ? b.CreateSExt(a,b.getIntNTy(128)) : b.CreateZExt(a,b.getIntNTy(128));
                        auto low = numeric::signed_integer(to) ? llvm::APInt::getSignedMinValue(numeric::width(to)).sext(128) : llvm::APInt(128,0);
                        auto high = numeric::signed_integer(to) ? llvm::APInt::getSignedMaxValue(numeric::width(to)).sext(128) : llvm::APInt::getMaxValue(numeric::width(to)).zext(128);
                        fail_if(b.CreateOr(b.CreateICmpSLT(wide,llvm::ConstantInt::get(context,low)),b.CreateICmpSGT(wide,llvm::ConstantInt::get(context,high))),"Checked numeric conversion is out of range");
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
                case Op::store: v = b.CreateStore(values[instruction.b], slots[instruction.a]); break;
                case Op::call: {
                    std::vector<llvm::Value*> arguments;
                    if (instruction.receiver) {
                        auto* address = slots.at(instruction.receiver->slot);
                        auto t = f.locals.at(instruction.receiver->slot);
                        for (auto field : instruction.receiver->fields) {
                            if (t.kind==ir::Type::managed_ref) { address=b.CreateLoad(type(t),address); t=ir::Type(t.definition); }
                            address = b.CreateStructGEP(type(t),address,field);
                            t = t.definition->fields.at(field).type;
                        }
                        arguments.push_back(t.kind==ir::Type::managed_ref ? b.CreateLoad(type(t),address) : address);
                    }
                    for (auto id : instruction.arguments) arguments.push_back(values[id]);
                    bool imported = std::find(input.imports.begin(),input.imports.end(),instruction.callee) != input.imports.end();
                    bool legacy_single = imported;
                    for (const auto& target : input.functions) if (target.name == instruction.callee) legacy_single = !target.receiver && target.parameters.size() == 1;
                    if (arguments.empty() && legacy_single) arguments.push_back(values[instruction.a]);
                    auto* callee = functions.at(instruction.callee);
                    bool checked = callee->arg_size() == arguments.size() + 1;
                    if (checked && !failure) throw std::invalid_argument("Checked source calls require a source diagnostic context");
                    if (checked) arguments.push_back(failure);
                    llvm::AllocaInst* native_guard = nullptr;
                    if (input.coordinated && imported) {
                        // A fixed entry-block slot, including imports inside loops.
                        llvm::IRBuilder<> entry_builder(&*fn->getEntryBlock().begin());
                        native_guard = entry_builder.CreateAlloca(b.getInt8Ty(),b.getInt64(sizeof(Runtime::Native)));
                        native_guard->setAlignment(llvm::Align(alignof(Runtime::Native)));
                        b.CreateCall(module->getOrInsertFunction("vd_gc_native_enter",b.getVoidTy(),b.getPtrTy()),{native_guard});
                    }
                    v = b.CreateCall(callee, arguments);
                    if (native_guard) b.CreateCall(module->getOrInsertFunction("vd_gc_native_leave",b.getVoidTy(),b.getPtrTy()),{native_guard});
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
                auto result_type = instruction_types[n].at(values.size());
                if (input.coordinated && value_roots[n].at(values.size())) b.CreateStore(v,value_roots[n][values.size()]);
                value_types.push_back(result_type);
                values.push_back(v);
            }
            const auto& end = f.blocks[n].end;
            switch (end.kind) {
            case ir::Terminator::Kind::return_value: leave_frame(); b.CreateRet(values[end.value]); break;
            case ir::Terminator::Kind::return_void: leave_frame(); b.CreateRetVoid(); break;
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
    std::shared_ptr<Runtime> source_runtime;
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
        Compatibility contract, const std::shared_ptr<Impl>& self, bool checked = false, std::shared_ptr<Runtime> runtime = {}) {
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
        if (checked) return std::make_shared<NativeCode>(address.toPtr<NativeCode::CheckedEntry>(), generation, std::move(contract), std::move(runtime));
        auto entry = address.toPtr<NativeCode::Entry>();
        return std::make_shared<NativeCode>(entry, generation, std::move(contract));
    }
};
Jit::Jit() : impl_(std::make_shared<Impl>()) {
    initialize();
    impl_->jit = unwrap(llvm::orc::LLJITBuilder().create());
    bind("vd_gc_frame_enter",vd_gc_frame_enter); bind("vd_gc_frame_leave",vd_gc_frame_leave);
    bind("vd_gc_construct",vd_gc_construct);
    bind("vd_gc_poll",vd_gc_poll); bind("vd_gc_collect",vd_gc_collect); bind("vd_gc_allocate",vd_gc_allocate);
    bind("vd_gc_alive",vd_gc_alive); bind("vd_gc_native_enter",vd_gc_native_enter); bind("vd_gc_native_leave",vd_gc_native_leave);
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
                                              bool optimize, Compatibility contract, std::shared_ptr<Runtime> runtime) {
    if (runtime && !input.coordinated) {
        auto coordinated = input;
        coordinated.coordinated = true;
        return compile(coordinated,entry,optimize,std::move(contract),std::move(runtime));
    }
    if (input.coordinated && input.analysis && !runtime) {
        if (!impl_->source_runtime) impl_->source_runtime=std::make_shared<Runtime>(256);
        runtime=impl_->source_runtime;
    }
    ir::validate(input);
    if (input.coordinated != static_cast<bool>(runtime)) throw std::invalid_argument("coordinated IR requires an explicit runtime owner");
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
    // Compiler records always override host-supplied source safety metadata.
    contract.analysis = input.analysis;
    contract.source_contract = input.analysis ? input.analysis->compatibility_contract() : std::string{};
    contract.runtime_identity = reinterpret_cast<std::uintptr_t>(runtime.get());
    if (input.coordinated && !input.analysis) {
        std::map<std::string,std::string> layouts;
        auto encoded = [](const std::string& text) { return std::to_string(text.size()) + ":" + text; };
        std::function<void(ir::Type)> record = [&](ir::Type t) {
            if (!t.definition || layouts.contains(t.definition->identity)) return;
            std::string text = encoded(t.definition->identity) + "{";
            for (const auto& field : t.definition->fields) text += encoded(field.name) + ":" + std::to_string(field.type.kind) + ":" + encoded(ir::Type::key(field.type)) + ";";
            layouts.emplace(t.definition->identity,text + "}");
            for (const auto& field : t.definition->fields) record(field.type);
        };
        for (const auto& f : input.functions) {
            record(f.result); for (auto t : f.parameters) record(t); for (auto t : f.locals) record(t);
            for (const auto& block : f.blocks) for (const auto& i : block.instructions) record(i.type);
        }
        contract.source_contract += "\nphase15-runtime-layouts:";
        for (const auto& [name,text] : layouts) { (void)name; contract.source_contract += std::to_string(text.size()) + ":" + text; }
    }
    if (!selected->module_identity.empty()) contract.constant_contract = input.analysis ? input.analysis->constant_contract() : input.constant_contract;
    auto suffix = "__v" + std::to_string(++impl_->serial);
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = lower(input, *context, suffix, impl_->jit->getDataLayout());
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple().str());
    finish(*module, optimize);
    return impl_->load(std::move(module), std::move(context), native_symbol(*selected, suffix), std::move(contract), impl_, !selected->module_identity.empty() || input.coordinated, std::move(runtime));
}
std::shared_ptr<const NativeCode> Jit::compile(const ValidatedAnalysis& analysis, bool optimize, Compatibility contract) {
    auto module = analysis.lower();
    return compile(module,module.entry_symbol,optimize,std::move(contract));
}
bool Jit::replace(ReloadSlot& slot, const ValidatedAnalysis& analysis, bool optimize, Compatibility contract) {
    auto current = slot.acquire();
    if (contract.function_id.empty()) contract.function_id = analysis.entry_identity() + "::entry";
    contract.constant_contract = analysis.constant_contract();
    contract.source_contract = analysis.compatibility_contract();
    contract.analysis = analysis.shared_from_this();
    contract.runtime_identity=current->compatibility().runtime_identity;
    if (!(current->compatibility() == contract)) return false;
    auto module=analysis.lower();
    return slot.replace(compile(module,module.entry_symbol,optimize,std::move(contract),current->runtime_owner()));
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
static void emit_impl(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, bool runtime_object) {
    if ((runtime_object && !input.coordinated) || (input.coordinated && !runtime_object && !source_program)) throw std::invalid_argument("coordinated AOT requires the runtime proof bootstrap");
    initialize();
    llvm::LLVMContext context;
    bool entry_valid = false;
    for (const auto& f : input.functions) if (f.name == input.entry_symbol)
        entry_valid = f.parameters == std::vector<ir::Type>{ir::Type::i32} && f.result == ir::Type::i32;
    if (!entry_valid) throw std::invalid_argument("AOT CLI adapter requires entry: i32(i32)");
    auto triple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    std::unique_ptr<llvm::TargetMachine> machine(target->createTargetMachine(triple, "generic", "",
        llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!machine) throw std::runtime_error("cannot create native target machine");
    auto module = lower(input, context, source_program ? "__void" : "", machine->createDataLayout());
    module->setTargetTriple(triple);
    module->setDataLayout(machine->createDataLayout());
    if (!runtime_object) {
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
    llvm::CallInst* result;
    if (input.coordinated) result=b.CreateCall(module->getOrInsertFunction("vd_runtime_entry",b.getInt32Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy()),{entry,input_value,failure});
    else result=b.CreateCall(entry, arguments);
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
    }
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
void emit_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program) {
    emit_impl(input,output,optimize,source_program,false);
}
void emit_runtime_object(const ir::Module& input, const std::filesystem::path& output, bool optimize) {
    emit_impl(input,output,optimize,false,true);
}
}
