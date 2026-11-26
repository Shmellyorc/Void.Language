#pragma once

#include "lexer.hpp"

// These handler boundaries keep unrelated temporaries out of recursive semantic
// frames even in Release. The existing depth budget must fit a 1 MiB stack.
#if defined(_MSC_VER)
#define VOID_SEMANTIC_HANDLER __declspec(noinline)
#else
#define VOID_SEMANTIC_HANDLER __attribute__((noinline))
#endif

namespace vd::frontend {

// Keep unrelated temporary state out of a recursive compiler frame.
template<class Work> VOID_SEMANTIC_HANDLER decltype(auto) frontend_case(Work&& work) { return work(); }

class Depth {
    unsigned& depth;
public:
    Depth(unsigned& d, const SourceFile& source, const Token& token, unsigned limit = 128) : depth(d) {
        if (depth >= limit) error(source, token, Code::nesting, "Source nesting limit exceeded.", "Split deeply nested expressions or blocks into smaller functions.");
        ++depth;
    }
    ~Depth() { --depth; }
};
}
