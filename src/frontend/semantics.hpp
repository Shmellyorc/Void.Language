#pragma once

#include "model.hpp"

namespace vd::frontend {

// Binding owns declaration dependencies. These operations type-check the
// existing AST while each semantic traversal keeps its working state private.
struct SemanticContext {
    const std::map<std::string, std::unique_ptr<Unit>>& units;
    Unit& root;
    const std::function<void(Unit&, Constant&)>& evaluate;
    const std::function<void(Unit&, Structure&)>& defaults;
    Diagnostics& diagnostics;

    void prepare_compile_body(Unit&, Function&, const SourceFile&, const Token&, bool body) const;
    Type pure_expression(Unit&, Expr&, std::optional<Type> expected = {}) const;
    Type infer_default(Unit&, Expr&, const Function&) const;
    void analyze_constant(Unit&, Constant&) const;
    void field_defaults(Unit&, Structure&) const;
    void analyze_global(Unit&, Global&) const;
    void parameter_defaults(Unit&, Function&) const;
    void initializer_fields(Unit&, Function&) const;
    void prepare_specialized_method(Unit&, Function&) const;
    void validate_header(Unit&, Function&) const;
    void validate_generic_receiver(Unit&, Type receiver, const Token&) const;
    void analyze_unit(Unit&) const;
    void finish_editor_receiver(Unit&, Function&) const;
};
}
