#pragma once
// Research G33-X A fixtures and exact 1,000-iteration oracles. The mixed case
// retains array allocation (covered by a separate whole-call fault) while
// eliminating the box. Probe::constructions counts the managed-object ABI.
#define VD_ELISION_CASES(X) \
    X(local, 46995, 0) \
    X(direct, 46995, 0) \
    X(alias, 47995, 0) \
    X(effects, 47995, 0) \
    X(numeric, 174495, 0) \
    X(init, 50995, 0) \
    X(escape, 46995, 1000) \
    X(reference, 46995, 1000) \
    X(across_edge, 47495, 1000) \
    X(survives_allocation, 46995, 0)
