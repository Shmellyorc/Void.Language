#pragma once

#include "model.hpp"

namespace vd::frontend {

std::pair<Unit*, Structure*> structure_lookup(Unit&, Unit& root,
    const std::map<std::string, std::unique_ptr<Unit>>&, const std::string& spelling, const Token&);
Type resolve_type(Type, Unit&, Unit& root, const std::map<std::string, std::unique_ptr<Unit>>&, const Token&);
std::string declaring_field_name(Type owner, unsigned index);
void check_dictionary_key(const SourceFile&, const Token&, Type, const std::map<std::string, std::unique_ptr<Unit>>&);
}
