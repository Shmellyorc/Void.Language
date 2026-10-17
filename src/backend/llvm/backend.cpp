#include "void/path.hpp"
#include "void/backend.hpp"
#include "void/llvm_storage_audit.hpp"
#include "void/frontend.hpp"
#include "void/numeric.hpp"
#include "void/runtime_abi.hpp"
#include "void/reference_capabilities.hpp"
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
                                  const std::string& suffix, const llvm::DataLayout& target_layout,
                                  std::vector<NominalDescriptor>* host_catalog=nullptr,std::vector<InterfaceWitness>* host_witnesses=nullptr,
                                  llvm_backend::LoweringInventory* inventory=nullptr) {
    ir::InstructionTypes checked_types;
    std::map<std::string,ir::ReferenceEffects> verified_effects;
    ir::validate(input, &checked_types,&verified_effects);
    auto module = std::make_unique<llvm::Module>(input.identity, context);
    module->setDataLayout(target_layout);
    std::uint64_t home_serial=0;
    auto home = [&](llvm::AllocaInst* a, StorageRole role, llvm_backend::HomeKind kind) {
        if(inventory) {
            inventory->function(*a->getFunction(),false);
            inventory->home(*a,role,kind,a->getFunction()->getName().str()+"/home/"+std::to_string(++home_serial));
        }
        return a;
    };
    // StructLayout caches use raw context-owned Type pointers as keys. Query
    // the module-owned copy, never the LLJIT layout that outlives these types.
    const auto& layout = module->getDataLayout();
    llvm::IRBuilder<> b(context);
    std::map<std::string,llvm::StructType*> aggregates;
    auto* interface_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy());
    auto* prepared_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),b.getInt64Ty());
    std::function<llvm::Type*(ir::Type)> type = [&](ir::Type t) -> llvm::Type* {
        if(ir::array_type(t)) throw std::invalid_argument("P23-1 arrays support validated analysis/IR only; native lowering requires P23-2 storage planning and review");
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
        return b.getIntNTy(numeric::width(t));
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
        auto reference_offsets = [&](ir::Type t, std::uint64_t base, auto& result) {
            // Translate the shared logical shape into this module's target
            // offsets. Qualifiers never change native layout or root identity.
            for (const auto& path:ir::reference_paths(t,true)) {
                auto current=t; auto offset=base;
                for (auto field:path) {
                    offset+=field_offset(current,field);
                    current=ir::field_type(current,field);
                }
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
    std::map<std::string,ir::Type> nominal_types;
    for(auto t:input.nominal_types) if(t.kind==ir::Type::managed_ref) nominal_types.emplace(t.definition->identity,t);
    auto* record_type=llvm::StructType::get(b.getPtrTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type);
    const auto* record_layout=layout.getStructLayout(record_type);
    if(record_layout->getSizeInBytes()!=sizeof(RuntimeTypeRecord) || record_layout->getElementOffset(3)!=offsetof(RuntimeTypeRecord,size) || record_layout->getElementOffset(5)!=offsetof(RuntimeTypeRecord,count))
        throw std::invalid_argument("runtime nominal descriptor ABI mismatch");
    auto string=[&](const std::string& text) {return b.CreateGlobalStringPtr(text,"",0,module.get());};
    std::vector<llvm::Constant*> records,keys;
    for(const auto& [key,t]:nominal_types) {
        std::vector<std::uint64_t> offsets; reference_offsets(ir::Type(t.definition),0,offsets);
        auto bytes=std::max<std::uint64_t>(1,layout.getTypeAllocSize(type(ir::Type(t.definition))));
        auto base=t.definition->base.lock();
        std::string shape=std::to_string(bytes)+":"+std::to_string(layout.getABITypeAlign(type(ir::Type(t.definition))).value());
        for(unsigned n=0;n<t.definition->fields.size();++n) {
            const auto& field=t.definition->fields[n];
            for(const auto& part:{field.name,field.owner,ir::type_name(field.type),std::to_string(field_offset(t,n))}) shape+="|"+std::to_string(part.size())+":"+part;
        }
        if(host_catalog) { NominalDescriptor d{key,base ? base->identity : "",shape,{bytes,{}}};d.layout.references.assign(offsets.begin(),offsets.end());host_catalog->push_back(std::move(d)); }
        auto* identity=string(key); keys.push_back(identity);
        records.push_back(llvm::ConstantStruct::get(record_type,{identity,string(base ? base->identity : ""),string(shape),llvm::ConstantInt::get(size_type,bytes),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size())}));
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
        b.CreateStore(b.CreateGlobalStringPtr("Invalid managed virtual receiver or generation type catalog"),dispatcher->getArg(dispatcher->arg_size()-1));
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
        for(const auto& field:t.definition->fields) access_contract+=encoded(field.name)+encoded(field.owner)+encoded(ir::type_name(field.type))+(field.using_field ? "U;" : "F;");
    }
    for(const auto& f:input.functions) {access_contract+=encoded(f.name)+encoded(f.visibility)+encoded(f.virtual_slot);for(auto t:f.parameters) access_contract+=encoded(ir::type_name(t))+encoded(ir::capability_key(t));access_contract+=encoded(ir::type_name(f.result))+encoded(ir::capability_key(f.result));}
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
    llvm::DIBuilder debug(*module);
    auto module_path = vd::utf8_path(input.source_path.empty() ? input.identity : input.source_path);
    auto* file = debug.createFile(vd::path_utf8(module_path.filename()), vd::path_utf8(module_path.parent_path()));
    debug.createCompileUnit(llvm::dwarf::DW_LANG_C, file, "VOID source compiler", false, "", 0);
    std::map<std::string,llvm::DIType*> debug_aggregates;
    std::function<llvm::DIType*(ir::Type)> debug_value_type = [&](ir::Type t) -> llvm::DIType* {
        if (t.kind == ir::Type::managed_ref || t.kind==ir::Type::string) return debug.createPointerType(nullptr, layout.getPointerSizeInBits());
        if(t.kind==ir::Type::interface_ref || t.kind==ir::Type::prepared_receiver) {
            auto* native=llvm::cast<llvm::StructType>(type(t));auto* offsets=layout.getStructLayout(native);std::vector<llvm::Metadata*> members;
            for(unsigned n=0;n<native->getNumElements();++n) {auto* part=native->getElementType(n);llvm::DIType* debug_part=n< (t.kind==ir::Type::interface_ref ? 2u : 3u) ? static_cast<llvm::DIType*>(debug.createPointerType(nullptr,layout.getPointerSizeInBits())) : debug.createBasicType("binding selector",64,llvm::dwarf::DW_ATE_unsigned);
                members.push_back(debug.createMemberType(file,std::to_string(n),file,0,layout.getTypeSizeInBits(part),layout.getABITypeAlign(part).value()*8,offsets->getElementOffsetInBits(n),llvm::DINode::FlagZero,debug_part));}
            return debug.createStructType(file,ir::type_name(t),file,0,offsets->getSizeInBits(),layout.getABITypeAlign(native).value()*8,llvm::DINode::FlagZero,nullptr,debug.getOrCreateArray(members));
        }
        if (t == ir::Type::none) return nullptr;
        if (t.kind == ir::Type::aggregate) {
            if (auto found = debug_aggregates.find(t.definition->identity); found != debug_aggregates.end()) return found->second;
            auto* native = llvm::cast<llvm::StructType>(type(t)); auto* offsets = layout.getStructLayout(native);
            std::vector<llvm::Metadata*> members;
            for (unsigned n = 0; n < t.definition->fields.size(); ++n) {
                const auto& field = t.definition->fields[n]; auto* native_field = type(field.type);
                members.push_back(debug.createMemberType(file,field.name,file,0,layout.getTypeSizeInBits(native_field),
                    layout.getABITypeAlign(native_field).value()*8,field_offset(t,n)*8,llvm::DINode::FlagZero,debug_value_type(field.type)));
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
            auto* slot = home(b.CreateAlloca(type(t)),StorageRole::local,llvm_backend::HomeKind::value); slots.push_back(slot);
            b.CreateStore(f.receiver && n==0 && t.kind==ir::Type::managed_ref ? static_cast<llvm::Value*>(fn->getArg(0)) : llvm::Constant::getNullValue(type(t)), slot);
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
                auto* storage = home(b.CreateAlloca(type(f.parameters[i])),StorageRole::parameter,llvm_backend::HomeKind::argument);
                b.CreateStore(fn->getArg(i),storage); add_root(f.parameters[i],storage);
            }
            for (unsigned block = 0; block < f.blocks.size(); ++block)
                for (auto t : instruction_types[block]) {
                    std::vector<std::uint64_t> offsets; reference_offsets(t,0,offsets);
                    llvm::Value* storage = nullptr;
                    if (!offsets.empty()) {
                        storage = home(b.CreateAlloca(type(t)),StorageRole::instruction,llvm_backend::HomeKind::root);
                        b.CreateStore(llvm::Constant::getNullValue(type(t)),storage);
                        add_root(t,storage);
                    }
                    value_roots[block].push_back(storage);
                }
            auto* root_array_type = llvm::ArrayType::get(slot_type,entries.size());
            auto* root_array = home(b.CreateAlloca(root_array_type),StorageRole::roots,llvm_backend::HomeKind::slots);
            for (unsigned i = 0; i < entries.size(); ++i)
                b.CreateStore(entries[i],b.CreateInBoundsGEP(root_array_type,root_array,{b.getInt32(0),b.getInt32(i)}));
            auto* frame = home(b.CreateAlloca(b.getInt8Ty(), b.getInt64(sizeof(Runtime::Frame))),StorageRole::roots,llvm_backend::HomeKind::frame);
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
                auto guard_complete = [&](ir::Type value_type,llvm::Value* value) {
                    for (const auto& path:ir::reference_paths(value_type)) {
                        auto leaf=value_type;
                        for (auto field:path) leaf=ir::field_type(leaf,field);
                        auto* reference=path.empty() ? value : b.CreateExtractValue(value,path);
                        if(leaf.kind==ir::Type::interface_ref) {
                            auto* object=b.CreateExtractValue(reference,0);auto* witness=b.CreateExtractValue(reference,1);
                            auto* selected=binding_index(reference);auto* bad=b.CreateICmpSLT(selected,b.getInt64(0));
                            // A witness for a different nominal interface is not valid storage.
                            llvm::Value* known=b.getFalse();
                            for(unsigned n=0;n<bindings.size();++n) if(bindings[n]->interface.definition==leaf.definition) known=b.CreateOr(known,b.CreateICmpEQ(selected,b.getInt64(n)));
                            bad=b.CreateOr(bad,b.CreateNot(known));
                            if(leaf.nullable) bad=b.CreateAnd(bad,b.CreateNot(b.CreateAnd(b.CreateIsNull(object),b.CreateIsNull(witness))));
                            fail_if(bad,"Invalid interface field binding");
                        } else if(!leaf.nullable) fail_if(b.CreateIsNull(reference),"Null supplied for non-nullable field");
                    }
                };
                switch (instruction.op) {
                case ir::Op::fixed_array: case ir::Op::array_get: case ir::Op::array_set:
                case ir::Op::fixed_snapshot: case ir::Op::fixed_select: case ir::Op::fixed_commit:
                    throw std::invalid_argument("P23-1 checked array IR cannot execute before P23-2 review");
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
                    auto* payload=home(allocations.CreateAlloca(b.getInt8Ty(),b.getInt64(bytes)),StorageRole::staging,llvm_backend::HomeKind::staging);
                    payload->setAlignment(layout.getABITypeAlign(native));
                    b.CreateMemSet(payload,b.getInt8(0),bytes,layout.getABITypeAlign(native));
                    for (unsigned field=0;field<instruction.arguments.size();++field) {
                        auto* value=values[instruction.arguments[field]];
                        ir::Type field_type=instruction.type.definition->fields[field].type;
                        guard_complete(field_type,value);
                        b.CreateStore(value,field_address(object,payload,field));
                    }
                    std::vector<std::uint64_t> offsets; reference_offsets(object,0,offsets);
                    auto* storage=value_roots[n].at(values.size());
                    std::vector<llvm::Value*> arguments{storage,llvm::ConstantInt::get(size_type,bytes),offset_map(offsets),llvm::ConstantInt::get(size_type,offsets.size()),payload};
                    if(nominal_types.contains(instruction.type.definition->identity)) arguments.push_back(b.CreateGlobalStringPtr(instruction.type.definition->identity));
                    arguments.push_back(failure);
                    std::vector<llvm::Type*> parameter_types;for(auto* arg:arguments) parameter_types.push_back(arg->getType());
                    auto abi=module->getOrInsertFunction(nominal_types.contains(instruction.type.definition->identity) ? "vd_gc_construct_typed" : "vd_gc_construct",llvm::FunctionType::get(b.getVoidTy(),parameter_types,false));
                    b.CreateCall(abi,arguments);
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
                    if (instruction.op==Op::gc_write) guard_complete(field,values[instruction.b]);
                    auto* address = field_address(object,values[instruction.a],static_cast<unsigned>(instruction.immediate));
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
                case Op::string_literal: case Op::string_concat: case Op::string_equal: {
                    if(instruction.op==Op::string_equal) {
                        v=b.CreateCall(module->getOrInsertFunction("vd_string_equal",b.getInt1Ty(),b.getPtrTy(),b.getPtrTy(),b.getPtrTy()),{values[instruction.a],values[instruction.b],failure});
                    } else {
                        auto* storage=value_roots[n].at(values.size());
                        if(instruction.op==Op::string_literal) {
                            const auto& text=input.strings.at(instruction.immediate);
                            b.CreateCall(module->getOrInsertFunction("vd_string_literal",b.getVoidTy(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy()),
                                {storage,b.CreateGlobalStringPtr(llvm::StringRef(text.data(),text.size())),llvm::ConstantInt::get(size_type,text.size()),failure});
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
                case Op::negate: v = values[instruction.a]->getType()->isFloatingPointTy() ? b.CreateFNeg(values[instruction.a]) : b.CreateNeg(values[instruction.a]); break;
                case Op::upcast: case Op::stage_prefix: v=values[instruction.a]; break;
                case Op::convert: {
                    auto* a = values[instruction.a]; auto from = operand_type(instruction.a), to = instruction.type;
                    if(ir::reference_value(from)) {
                        if(!to.nullable) fail_if(b.CreateIsNull(from.kind==ir::Type::interface_ref ? b.CreateExtractValue(a,0) : a),from.kind==ir::Type::interface_ref ? "Null interface reference in checked conversion" : "Null managed reference in checked conversion");
                        if(from.kind==ir::Type::interface_ref) guard_complete(to,a);
                        v=a;break;
                    }
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
                case Op::store: v = b.CreateStore(values[instruction.b], slots[instruction.a]); break;
                case Op::interface_call: case Op::call: case Op::virtual_call: case Op::base_init: {
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
                    auto* callee = instruction.op==Op::interface_call ? interface_dispatchers.at(instruction.callee) : functions.at(instruction.callee);
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
    if(inventory) for(auto& function:*module) if(!function.isDeclaration()) {
        inventory->function(function,false);
        inventory->complete(function,llvm_backend::all_storage_roles());
    }
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
        Compatibility contract, const std::shared_ptr<Impl>& self, bool checked = false, std::shared_ptr<Runtime> runtime = {}, std::span<const NominalDescriptor> catalog = {},std::span<const InterfaceWitness> witnesses = {}) {
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
        if (checked) {
            auto result=std::make_shared<NativeCode>(address.toPtr<NativeCode::CheckedEntry>(),generation,std::move(contract),runtime);
            if(runtime) runtime->admit_metadata(catalog,witnesses);
            return result;
        }
        auto entry = address.toPtr<NativeCode::Entry>();
        return std::make_shared<NativeCode>(entry, generation, std::move(contract));
    }
};
Jit::Jit() : impl_(std::make_shared<Impl>()) {
    initialize();
    impl_->jit = unwrap(llvm::orc::LLJITBuilder().create());
    bind("vd_gc_frame_enter",vd_gc_frame_enter); bind("vd_gc_frame_leave",vd_gc_frame_leave);
    bind("vd_string_literal",vd_string_literal); bind("vd_string_concat",vd_string_concat); bind("vd_string_equal",vd_string_equal);
    bind("vd_gc_construct",vd_gc_construct);
    bind("vd_gc_construct_typed",vd_gc_construct_typed);
    bind("vd_gc_dynamic_type",vd_gc_dynamic_type);
    bind("vd_gc_witness",vd_gc_witness);
    bind("vd_gc_interface_binding",vd_gc_interface_binding);
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
        if (!impl_->source_runtime) impl_->source_runtime=std::make_shared<Runtime>(256);
        runtime=impl_->source_runtime;
    }
    std::map<std::string,ir::ReferenceEffects> checked_effects;ir::validate(input,nullptr,&checked_effects);
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
            auto base=t.definition->base.lock();
            std::string text = encoded(t.definition->identity) + "base:"+encoded(base ? base->identity : "")+"{";
            for (const auto& field : t.definition->fields) text += encoded(field.name) + ":"+encoded(field.owner)+":"+encoded(ir::type_name(field.type))+";";
            layouts.emplace(t.definition->identity,text + "}");
            for (const auto& field : t.definition->fields) record(field.type);
        };
        for(auto t:input.nominal_types) record(t);
        for (const auto& f : input.functions) {
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
    auto suffix = "__v" + std::to_string(++impl_->serial);
    auto context = std::make_unique<llvm::LLVMContext>();
    std::vector<NominalDescriptor> catalog;std::vector<InterfaceWitness> witnesses;
    auto module = lower(input, *context, suffix, impl_->jit->getDataLayout(),&catalog,&witnesses);
    module->setDataLayout(impl_->jit->getDataLayout());
    module->setTargetTriple(impl_->jit->getTargetTriple().str());
    finish(*module, optimize);
    return impl_->load(std::move(module), std::move(context), native_symbol(*selected, suffix), std::move(contract), impl_, !selected->module_identity.empty() || input.coordinated, std::move(runtime),catalog,witnesses);
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
static void emit_impl(const ir::Module& input, const std::filesystem::path& output, bool optimize, bool source_program, bool runtime_object, const std::string& generation_suffix = {},
                      bool storage_proof = false, const std::filesystem::path& proof_report = {}) {
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
    llvm_backend::LoweringInventory inventory;
    std::unique_ptr<llvm_backend::AotObjectEmitter> audited;
    if (storage_proof) audited = std::make_unique<llvm_backend::AotObjectEmitter>(triple,optimize);
    auto module = lower(input, context, source_program ? "__void" : generation_suffix, machine->createDataLayout(),
                        nullptr,nullptr,storage_proof ? &inventory : nullptr);
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
    if (storage_proof) inventory.function(*main,false);
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
        if (storage_proof) inventory.home(*end,StorageRole::known_abi,llvm_backend::HomeKind::cli_parse,"adapter/input-parse");
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
        if (storage_proof) inventory.home(*llvm::cast<llvm::AllocaInst>(failure),StorageRole::known_abi,llvm_backend::HomeKind::cli_failure,"adapter/failure");
        b.CreateStore(llvm::Constant::getNullValue(failure_type), failure);
        arguments.push_back(failure);
    }
    llvm::CallInst* result;
    if (input.coordinated) {
        auto* catalog=module->getNamedGlobal("__vd_types"+(source_program ? std::string("__void") : std::string{}));
        auto* count=module->getNamedGlobal("__vd_type_count"+(source_program ? std::string("__void") : std::string{}));
        auto* size_type=b.getIntNTy(module->getDataLayout().getPointerSizeInBits());
        auto* witnesses=module->getNamedGlobal("__vd_witnesses"+(source_program ? std::string("__void") : std::string{}));auto* witness_count=module->getNamedGlobal("__vd_witness_count"+(source_program ? std::string("__void") : std::string{}));
        result=b.CreateCall(module->getOrInsertFunction("vd_runtime_entry_metadata",b.getInt32Ty(),b.getPtrTy(),b.getInt32Ty(),b.getPtrTy(),b.getPtrTy(),size_type,b.getPtrTy(),size_type),{entry,input_value,failure,catalog,b.CreateLoad(size_type,count),witnesses,b.CreateLoad(size_type,witness_count)});
    }
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
    // Public AOT retains the original optimizer/emitter path. The proof service
    // and its CLI-home normalization are deliberately NOT activated here.
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
void emit_runtime_object(const ir::Module& input, const std::filesystem::path& output, bool optimize, const std::string& generation_suffix) {
    emit_impl(input,output,optimize,false,true,generation_suffix);
}
namespace llvm_backend {
void emit_aot_storage_proof(const ir::Module& input, const std::filesystem::path& output,
                            bool optimize, const std::filesystem::path& report, ProofAdapter adapter,
                            const std::string& generation_suffix) {
    emit_impl(input,output,optimize,adapter == ProofAdapter::source,
              adapter == ProofAdapter::runtime,generation_suffix,true,report);
}
}
}
