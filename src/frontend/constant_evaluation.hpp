#pragma once

#include "model.hpp"

namespace vd::frontend {

struct CompileEvaluation {
    static constexpr std::uint64_t step_limit=1048576, graph_step_limit=8388608;
    static constexpr std::size_t byte_limit=64*1024*1024, graph_byte_limit=256*1024*1024;
    unsigned depth=0, calls=0, nesting=0;
    // Calls retain their enclosing expression frame. Count statement sequences
    // and loop handlers alongside it, so separate limits cannot multiply native
    // stack use. Keep room to report the limit on the required 1 MiB stack.
    struct Nesting {
        CompileEvaluation& state;
        Nesting(CompileEvaluation& state,const SourceFile& source,const Token& site):state(state) {
            if(state.nesting>=256)error(source,site,Code::limit,"Compile-time combined expression/statement nesting limit of 256 exceeded.");
            ++state.nesting;
        }
        ~Nesting(){--state.nesting;}
    };
    std::uint64_t steps=0, graph_steps=0;
    std::size_t bytes=0, graph_bytes=0;
    std::vector<std::optional<ir::ConstantValue>>* locals=nullptr;
    std::function<void(Function&,const SourceFile&,const Token&,bool)> prepare;
    std::function<void(Type)> numeric_definition;
    void step(const SourceFile& source,const Token& site,std::uint64_t work=1);
    void charge_bytes(const SourceFile&, const Token&, std::size_t);
    void charge(const SourceFile& source,const Token& site,const ir::ConstantValue& value);
    bool matches(const SourceFile&, const Pattern&, const ir::ConstantValue&);
    ir::ConstantValue match_expression(const SourceFile&, const Expr&);
    ir::ConstantValue local(const SourceFile& source,const Expr& e);
    struct Outcome {unsigned flow=next_flow;ir::ConstantValue value;};
    Outcome sequence(const SourceFile&,const std::vector<Statement>&);
    Outcome loop(const SourceFile&,const Statement&);
    Outcome match_statement(const SourceFile&, const Statement&);
    void assign(const SourceFile&,const Statement&);
    ir::ConstantValue call(const SourceFile&,const Expr&);
    struct Selection {
        std::optional<unsigned> slot;
        ir::ConstantValue captured;
        struct Part {std::size_t index;bool array;};
        std::vector<Part> path;
    };
    const ir::ConstantValue& read(const SourceFile&,const Token&,const Selection&);
    Selection select(const SourceFile&,const Expr&);
};

bool compile_value_type(Type);
bool constant_core_operation(std::string_view target);
bool constant_pattern_equal(const SourceFile&, const Token&, Type, const ir::ConstantValue&, const ir::ConstantValue&, CompileEvaluation&);
const ir::ConstantValue* constant_dictionary_find(const SourceFile&, const Token&, Type, const ir::ConstantValue&,
                                                 const ir::ConstantValue&, CompileEvaluation*);
ir::ConstantValue constant_value(const SourceFile&, const Expr&, CompileEvaluation* = nullptr);
void validate_compile_function(const SourceFile&, Function&);
void prepare_compile_call(CompileEvaluation*,Function&,const SourceFile&,const Token&,bool);
}
