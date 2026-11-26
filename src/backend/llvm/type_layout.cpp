#include "type_layout.hpp"
#include "void/numeric.hpp"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <stdexcept>

namespace vd::llvm_backend {

TypeLayout::TypeLayout(llvm::Module& module, bool coordinated)
    : context_(module.getContext()), layout_(module.getDataLayout()), coordinated_(coordinated) {
    auto* pointer = llvm::PointerType::get(context_, 0);
    interface_ = llvm::StructType::get(pointer, pointer);
    prepared_ = llvm::StructType::get(pointer, pointer, pointer, llvm::Type::getInt64Ty(context_));
}

llvm::Type* TypeLayout::operator()(ir::Type t) {
    if(ir::collection_type(t)) {
        if(!coordinated_) throw std::invalid_argument("Native fixed arrays require a coordinated Runtime");
        return llvm::PointerType::get(context_, 0);
    }
    if(t.kind==ir::Type::interface_ref) return interface_;
    if(t.kind==ir::Type::prepared_receiver) return prepared_;
    if (t.kind == ir::Type::managed_ref || t.kind==ir::Type::string) return llvm::PointerType::get(context_, 0);
    if (t.kind == ir::Type::aggregate) {
        if (auto found = aggregates_.find(t.definition->identity); found != aggregates_.end()) return found->second;
        auto* result = llvm::StructType::create(context_,t.definition->identity); aggregates_.emplace(t.definition->identity,result);
        std::vector<llvm::Type*> fields;
        auto base=t.definition->base.lock();
        if(base) fields.push_back((*this)(ir::Type(base)));
        for(unsigned n=base ? static_cast<unsigned>(base->fields.size()) : 0;n<t.definition->fields.size();++n) fields.push_back((*this)(t.definition->fields[n].type));
        result->setBody(fields,false); return result;
    }
    if (t == ir::Type::boolean) return llvm::Type::getInt1Ty(context_);
    if (t == ir::Type::none) return llvm::Type::getVoidTy(context_);
    if (t == ir::Type::f32) return llvm::Type::getFloatTy(context_);
    if (t == ir::Type::f64) return llvm::Type::getDoubleTy(context_);
    if(t.kind==ir::Type::numeric_enum)t=*t.definition->enum_backing;
    return llvm::IntegerType::get(context_, numeric::width(t));
}

std::uint64_t TypeLayout::field_offset(ir::Type owner, unsigned field) {
    std::uint64_t offset=0; auto* native=(*this)(ir::Type(owner.definition));
    for(auto index:ir::physical_field_path(owner,field)) {
        auto* aggregate=llvm::cast<llvm::StructType>(native);
        offset+=layout_.getStructLayout(aggregate)->getElementOffset(index); native=aggregate->getElementType(index);
    }
    return offset;
}

void TypeLayout::reference_offsets(ir::Type t, std::uint64_t base, std::vector<std::uint64_t>& result) {
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
        if(current.kind==ir::Type::prepared_receiver) result.push_back(offset+layout_.getPointerSize());
    }
}
}
