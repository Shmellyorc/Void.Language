#pragma once

#include "void/ir.hpp"

namespace vd::ir {

// The instruction validator establishes the nominal types and control-flow
// structure consumed by these independent ownership and capability proofs.
void validate_construction(const Function&, const Module&, const std::vector<std::vector<Type>>&);
void validate_generator_suspensions(const Function&);
void validate_fixed_ownership(const Function&, const Module&, const std::vector<std::vector<Type>>&);
void validate_numeric_enum_slots(const Function&, const CPrimitiveAdmission*);
void validate_reference_capabilities(const Module&, InstructionTypes&,
                                     std::map<std::string, ReferenceEffects>*);

bool derived_equality_adapter(const Function&);
bool checked_array_shape(Type, const CPrimitiveAdmission*);
bool checked_array_allocation(const Instruction&, const CPrimitiveAdmission*);
std::set<unsigned> checked_array_homes(const Function&, const CPrimitiveAdmission*);

}
