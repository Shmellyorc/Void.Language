#include "void/ir.hpp"
namespace vd::ir {
Module architecture_proof(std::int32_t answer) {
    SourceLocation loc{"examples/architecture.void", 2, 5};
    Function calculate{"calculate", loc, {{
        {{Op::argument}, {Op::constant, 0, 0, answer}, {Op::add, 0, 1}},
        {Terminator::Kind::return_value, 2}}}};
    Function entry{"entry", {"examples/architecture.void", 5, 1}, {
        {{{Op::argument}, {Op::constant, 0, 0, 0}, {Op::less, 0, 1}},
         {Terminator::Kind::branch, 2, 1, 2}},
        {{{Op::constant, 0, 0, 7}}, {Terminator::Kind::return_value, 0}},
        {{{Op::argument}, {Op::call, 0, 0, 0, "calculate"}}, {Terminator::Kind::return_value, 1}}
    }};
    return {"/architecture.void", {}, {calculate, entry}};
}
}
