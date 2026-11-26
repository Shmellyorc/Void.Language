#pragma once

#include "void/ir.hpp"
#include <functional>

namespace llvm { class DataLayout; class Type; }

namespace vd::llvm_backend {

struct ElisionHelper { unsigned parameter; std::string module; std::set<unsigned> aliases; };
struct ElisionFactory { std::string module; bool root_transfer; };

// A stack address may flow only through private, single-origin local aliases and
// field receivers. Every load must belong to the current construction instance:
// reaching a construction starts a new epoch, even on a loop backedge. Children
// remain ordinary heap references, rooted separately in the stack payload.
struct BlockAllocationElision {
    std::vector<std::set<unsigned>> values;
    std::set<unsigned> locals;
    std::set<std::pair<unsigned,unsigned>> calls;
    // Instruction after which a proven payload has no remaining owner uses.
    // Child SSA/local homes remain independently authoritative.
    struct Retirement {
        std::vector<std::pair<unsigned,unsigned>> payloads;
        std::vector<unsigned> values,locals;
    };
    std::map<std::pair<unsigned,unsigned>,Retirement> retirements;
    std::vector<std::vector<unsigned>> predecessors;
    bool precise_copies=false;
};
std::map<std::string, ElisionFactory> allocation_heap_factories(
    const ir::Module&, const ir::InstructionTypes&, const std::map<std::string, ir::ReferenceEffects>&);
std::map<std::string, ElisionHelper> allocation_elision_helpers(
    const ir::Module&, const ir::InstructionTypes&, const std::map<std::string, ir::ReferenceEffects>&);
BlockAllocationElision block_allocation_elision(const ir::Function&,
    const std::vector<std::vector<ir::Type>>&, const llvm::DataLayout&,
    const std::function<llvm::Type*(ir::Type)>&, const std::map<std::string, ElisionHelper>&,
    const std::map<std::string, ElisionFactory>&);

}
