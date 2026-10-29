// Production AOT emission boundary extracted from the pinned machine proof.
#include "void/llvm_storage_audit.hpp"
#include "encoded_cfg.hpp"
#include <limits>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Analysis/RuntimeLibcallInfo.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/CodeGen/MachineFrameInfo.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/CodeGen/TargetFrameLowering.h>
#include <llvm/CodeGen/TargetInstrInfo.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/IR/ValueHandle.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/InitializePasses.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/PassRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SHA256.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
using namespace llvm;
namespace vd::llvm_backend {
namespace {
struct Home { WeakVH identity; std::string name; uint64_t bytes, alignment; StorageRole role; HomeKind kind;
  WeakVH canonical; bool canonicalProved = false;
};
struct RewriteObserver final : CallbackVH {
  std::function<void(Value *,Value *)> record;
  RewriteObserver(Value *value, std::function<void(Value *,Value *)> callback)
    : CallbackVH(value), record(std::move(callback)) {}
  ~RewriteObserver() = default;
  void allUsesReplacedWith(Value *replacement) override { record(getValPtr(),replacement); }
};
struct Plan { std::vector<Home> homes; uint64_t bytes = 0; bool lifetime = false; bool policy = false; json::Array entryMoves; std::set<std::string> arrayCalls; };
struct State {
  std::map<std::string, Plan> plans;
  std::map<std::string, json::Object> reports;
  std::map<std::string, json::Array> earlyObjects;
  struct TraceHome { WeakVH value; std::string id; };
  std::vector<TraceHome> traceHomes;
  std::map<const Value *, size_t> traceIndex;
  std::map<std::string, std::string> lastTrace;
  json::Array optimizationEvents, rewriteEvents;
  std::vector<std::string> activePasses;
  std::vector<std::unique_ptr<RewriteObserver>> rewriteObservers;
  std::map<std::string, std::string> pei, late;
  std::map<std::string, std::vector<std::string>> sp;
  std::map<std::string, std::map<std::string, uint64_t>> calls;
  std::map<std::string, std::vector<std::string>> errors;
  bool enforce = false;
  std::string triple;
};
std::string jsonText(const json::Value &v) { return formatv("{0}", v).str(); }
void fail(State &s, StringRef fn, StringRef reason) { s.errors[fn.str()].push_back(reason.str()); }
bool spReg(unsigned reg, const MCRegisterInfo &ri) {
  if (!reg) return false;
  StringRef n(ri.getName(reg));
  return n == "RSP" || n == "ESP" || n == "SP" || n == "WSP";
}
std::string sig(StringRef op, const std::vector<std::string> &operands) {
  // MC may shorten an x86 immediate from imm32 to imm8. Both forms have
  // identical 64-bit SP semantics when the decoded signed immediate is equal.
  std::string s = op.str();
  if (op == "ADD64ri32" || op == "ADD64ri8") s = "ADD64ri";
  if (op == "SUB64ri32" || op == "SUB64ri8") s = "SUB64ri";
  for (const auto &p : operands) s += ":" + p;
  return s;
}
std::vector<std::string> stackWrites(MachineFunction &mf, bool rawOpcodes = false) {
  const auto &ri = *mf.getSubtarget().getRegisterInfo();
  std::vector<std::string> result;
  for (auto &bb : mf) for (auto &mi : bb) {
    if (mi.isCall() || mi.isReturn() || mi.isDebugInstr() || mi.isCFIInstruction()) continue;
    bool writes = false;
    for (const auto &o : mi.operands())
      if (o.isReg() && o.isDef() && spReg(o.getReg(), ri)) writes = true;
    if (!writes) continue;
    std::vector<std::string> operands;
    for (const auto &o : mi.explicit_operands()) {
      if (o.isReg()) operands.push_back("r" + std::to_string(o.getReg()));
      else if (o.isImm()) operands.push_back("i" + std::to_string(o.getImm()));
      else operands.push_back("unsupported");
    }
    const auto op = mf.getSubtarget().getInstrInfo()->getName(mi.getOpcode());
    result.push_back(rawOpcodes ? op.str() : sig(op, operands));
  }
  return result;
}
json::Object frame(MachineFunction &mf) {
  auto &fi = mf.getFrameInfo();
  json::Array objects;
  for (int i = fi.getObjectIndexBegin(); i < fi.getObjectIndexEnd(); ++i) {
    if (fi.isDeadObjectIndex(i)) continue;
    const auto *a = fi.getObjectAllocation(i);
    objects.push_back(json::Object{{"index", i}, {"size", fi.getObjectSize(i)},
      {"offset", fi.getObjectOffset(i)}, {"alignment", int64_t(fi.getObjectAlign(i).value())},
      {"fixed", fi.isFixedObjectIndex(i)}, {"spill", fi.isSpillSlotObjectIndex(i)},
      {"alloca", a ? a->getName().str() : ""}});
  }
  json::Array writes, rawOpcodes, calls;
  for (auto &s : stackWrites(mf)) writes.push_back(s);
  for (auto &s : stackWrites(mf, true)) rawOpcodes.push_back(s);
  for (auto &bb : mf) for (auto &mi : bb)
    if (mi.isCall()) calls.push_back(mf.getSubtarget().getInstrInfo()->getName(mi.getOpcode()));
  return json::Object{{"frame", int64_t(fi.getStackSize())}, {"outgoing", int64_t(fi.getMaxCallFrameSize())},
    {"alignment", int64_t(fi.getMaxAlign().value())}, {"has_variable_objects", fi.hasVarSizedObjects()},
    {"opaque_sp_adjustment", fi.hasOpaqueSPAdjustment()}, {"objects", std::move(objects)},
    {"sp_writes", std::move(writes)}, {"mi_sp_opcodes", std::move(rawOpcodes)}, {"call_opcodes", std::move(calls)}};
}
// Investigation evidence only. This does not certify the encoded CFG or ABI
// ownership and MUST NOT replace the nonreserved-area rejection below. The final
// object checker binds linear SP/call lists, not branch targets/path multiplicity.
json::Object outgoingObservation(MachineFunction &mf, StringRef triple) {
  json::Array errors, blocks, calls;
  const auto &fi = mf.getFrameInfo();
  const auto &ti = *mf.getSubtarget().getInstrInfo();
  const auto &ri = *mf.getSubtarget().getRegisterInfo();
  auto reject = [&](StringRef why) { errors.push_back(why.str()); };
  const Triple target(triple);
  if (!target.isOSLinux() || target.getArch() != Triple::x86_64)
    reject("no Linux x64 SP observation profile");
  if (fi.hasVarSizedObjects() || fi.hasOpaqueSPAdjustment() || fi.hasTailCall() ||
      fi.getMaxAlign() > Align(16) || mf.getFunction().getCallingConv() != CallingConv::C)
    reject("dynamic/tail/realigned/non-C shape");
  std::set<const Function *> callees;
  for (const auto &bb : mf.getFunction()) for (const auto &inst : bb)
    if (auto *call = dyn_cast<CallBase>(&inst)) {
      auto *callee = call->getCalledFunction();
      if (!callee || call->getCallingConv() != CallingConv::C || callee->isVarArg() ||
          call->hasFnAttr(Attribute::ReturnsTwice) || call->hasOperandBundles()) {
        reject("unproved source call/cleanup shape"); continue;
      }
      callees.insert(callee);
    }
  std::map<const MachineBasicBlock *, int64_t> entry;
  std::vector<const MachineBasicBlock *> work;
  if (!mf.empty()) { entry[&mf.front()] = 0; work.push_back(&mf.front()); }
  uint64_t peak = 0;
  while (!work.empty()) {
    auto *bb = work.back(); work.pop_back();
    int64_t depth = entry[bb]; bool returned = false;
    json::Array events, spEvents, successors;
    if (bb->isEHPad() || bb->hasAddressTaken()) reject("EH/address-taken CFG block");
    for (const auto &mi : *bb) {
      if (mi.isDebugInstr() || mi.isCFIInstruction()) continue;
      const auto op = ti.getName(mi.getOpcode());
      if (mi.isInlineAsm() || mi.isIndirectBranch()) reject("inline assembly/indirect CFG");
      if (mi.isCall()) {
        const Function *callee = nullptr;
        for (const auto &operand : mi.operands()) if (operand.isGlobal())
          callee = dyn_cast<Function>(operand.getGlobal());
        if (op != "CALL64pcrel32" || !callee || !callees.count(callee))
          reject("unproved machine call/cleanup identity");
        if (depth % 16 != 8) reject("call-site SP is not SysV-aligned");
        events.push_back(json::Object{{"kind", "call"}, {"callee", callee ? callee->getName() : StringRef("")}, {"depth", depth}});
        calls.push_back(json::Object{{"block", bb->getNumber()}, {"opcode", op},
          {"callee", callee ? callee->getName() : StringRef("")}, {"entry_sp_depth", depth}});
        continue; // Supported C call+return is net zero, excluding callee storage.
      }
      if (mi.isReturn()) {
        returned = true;
        events.push_back(json::Object{{"kind", "return"}, {"depth", depth}});
        if (op != "RET64" || depth != 0 || !bb->succ_empty()) reject("unbalanced/unsupported return");
        continue;
      }
      bool writes = false;
      for (const auto &operand : mi.operands())
        if (operand.isReg() && operand.isDef() && spReg(operand.getReg(),ri)) writes = true;
      if (!writes) continue;
      int64_t delta = 0; bool known = false;
      if ((op == "SUB64ri32" || op == "SUB64ri8" || op == "ADD64ri32" || op == "ADD64ri8") &&
          mi.getNumExplicitOperands() == 3 && mi.getOperand(0).isReg() && mi.getOperand(1).isReg() &&
          StringRef(ri.getName(mi.getOperand(0).getReg())) == "RSP" &&
          mi.getOperand(0).getReg() == mi.getOperand(1).getReg() && mi.getOperand(2).isImm() &&
          mi.getOperand(2).getImm() >= 0) {
        delta = mi.getOperand(2).getImm() * (op.starts_with("SUB") ? 1 : -1); known = true;
      } else if (op == "PUSH64r" || op == "PUSH64rmm" || op == "PUSH64i32" || op == "PUSH64i8") {
        delta = 8; known = true;
      } else if (op == "POP64r" && mi.getNumExplicitOperands() == 1 && mi.getOperand(0).isReg() &&
                 !spReg(mi.getOperand(0).getReg(),ri)) { delta = -8; known = true; }
      if (!known) { reject("unknown SP writer: " + op.str()); continue; }
      if ((delta > 0 && depth > std::numeric_limits<int64_t>::max() - delta) ||
          (delta < 0 && depth < -delta)) { reject("negative/overflowing SP depth"); continue; }
      depth += delta; peak = std::max(peak,uint64_t(depth));
      spEvents.push_back(json::Object{{"opcode", op}, {"delta", delta}, {"depth", depth}});
      events.push_back(json::Object{{"kind", "sp"}, {"opcode", op}, {"delta", delta}, {"depth", depth}});
    }
    if (bb->succ_empty() && !returned) reject("unproved terminal CFG path");
    for (const auto *next : bb->successors()) {
      successors.push_back(next->getNumber());
      auto [at, inserted] = entry.emplace(next,depth);
      if (inserted) work.push_back(next);
      else if (at->second != depth) reject("unequal CFG join or accumulating stack cycle");
    }
    unsigned layout = 0; for (const auto &candidate : mf) { if (&candidate == bb) break; ++layout; }
    blocks.push_back(json::Object{{"block", bb->getNumber()}, {"layout", layout}, {"entry_depth", entry[bb]},
      {"exit_depth", depth}, {"successors", std::move(successors)}, {"sp_events", std::move(spEvents)}, {"ordered_events", std::move(events)}});
  }
  if (entry.size() != mf.size()) reject("uncovered machine CFG blocks");
  const bool complete = errors.empty();
  return json::Object{{"scope", "diagnostic-machine-CFG-only;not-a-delivery-certificate"},
    {"complete_machine_observation", complete}, {"final_encoded_cfg_bound", false},
    {"incoming_credit", 0}, {"reconciliation_credit", 0},
    {"peak_entry_sp_depth", complete ? json::Value(int64_t(peak)) : json::Value(nullptr)},
    {"additional_below_F", complete ? json::Value(int64_t(peak > fi.getStackSize() ? peak-fi.getStackSize() : 0)) : json::Value(nullptr)},
    {"blocks", std::move(blocks)}, {"calls", std::move(calls)}, {"errors", std::move(errors)}};
}
struct Early : MachineFunctionPass {
  static char ID; State &s;
  Early(State &s) : MachineFunctionPass(ID), s(s) {}
  bool runOnMachineFunction(MachineFunction &mf) override {
    const auto &ti = *mf.getSubtarget().getInstrInfo();
    auto &fi = mf.getFrameInfo();
    auto &objects = s.earlyObjects[mf.getName().str()];
    for (int index = fi.getObjectIndexBegin(); index < fi.getObjectIndexEnd(); ++index) {
      if (fi.isDeadObjectIndex(index)) continue;
      const auto *allocation = fi.getObjectAllocation(index);
      objects.push_back(json::Object{{"index", index}, {"bytes", fi.getObjectSize(index)},
        {"offset", fi.getObjectOffset(index)}, {"alignment", int64_t(fi.getObjectAlign(index).value())},
        {"fixed", fi.isFixedObjectIndex(index)}, {"spill", fi.isSpillSlotObjectIndex(index)},
        {"has_alloca_identity", allocation != nullptr},
        {"alloca_static_at_isel", allocation && allocation->isStaticAlloca()},
        {"alloca_block_at_isel", allocation ? allocation->getParent()->getName().str() : std::string{}}});
    }
    for (auto &bb : mf) {
      int64_t area = -1;
      for (auto &mi : bb) {
        if (mi.getOpcode() == ti.getCallFrameSetupOpcode()) area = ti.getFrameSize(mi);
        if (mi.isCall()) for (auto &o : mi.operands()) {
          StringRef name;
          if (o.isGlobal()) name = o.getGlobal()->getName();
          else if (o.isSymbol()) name = o.getSymbolName();
          if (!name.empty() && area >= 0)
            s.calls[mf.getName().str()][name.str()] = std::max<uint64_t>(
              s.calls[mf.getName().str()][name.str()], area);
        }
        if (mi.getOpcode() == ti.getCallFrameDestroyOpcode()) area = -1;
      }
    }
    return false;
  }
  void getAnalysisUsage(AnalysisUsage &a) const override { a.setPreservesAll(); MachineFunctionPass::getAnalysisUsage(a); }
}; char Early::ID;
struct Audit : MachineFunctionPass {
  static char ID; State &s; int phase;
  Audit(State &s, int p) : MachineFunctionPass(ID), s(s), phase(p) {}
  bool runOnMachineFunction(MachineFunction &mf) override {
    const auto name = mf.getName().str();
    auto snapshot = frame(mf);
    const auto key = jsonText(json::Value(json::Object(snapshot)));
    if (phase == 0) { s.pei[name] = key; return false; }
    if (phase == 2) {
      if (!s.late.count(name) || s.late[name] != key) fail(s, name, "frame/SP certificate changed during final emission");
      return false;
    }
    auto &fi = mf.getFrameInfo();
    auto *fl = mf.getSubtarget().getFrameLowering();
    auto p = s.plans.find(name);
    if (p == s.plans.end()) { fail(s, name, "function has no immutable preoptimization inventory"); return false; }
    auto &plan = p->second;
    if (!plan.entryMoves.empty()) {
      // The narrowed CLI lowering must actually yield a static frame. Reject
      // body stack adjustments, even if LLVM's generic dynamic flags miss one.
      const auto &ri = *mf.getSubtarget().getRegisterInfo();
      for (auto &bb : mf) for (auto &mi : bb) {
        if (mi.isCall() || mi.isReturn() || mi.isCFIInstruction() || mi.isDebugInstr()) continue;
        bool writes = false;
        for (const auto &o : mi.operands()) if (o.isReg() && o.isDef() && spReg(o.getReg(),ri)) writes = true;
        if (writes && !mi.getFlag(MachineInstr::FrameSetup) && !mi.getFlag(MachineInstr::FrameDestroy))
          fail(s,name,"normalized CLI home still requires a body SP adjustment");
      }
    }
    const bool enforce = plan.policy;
    const bool reservedCallFrame = fl->hasReservedCallFrame(mf);
    // A push sequence can put the outgoing area below the static local frame.
    // Do not add an estimated area or rely on hasVarSizedObjects to catch it.
    if (enforce && !reservedCallFrame && fi.getMaxCallFrameSize())
      fail(s, name, "unproved nonreserved outgoing call-frame storage");
    uint64_t r = 0, e = -fl->getOffsetOfLocalArea(), a = fl->getStackAlign().value();
    if (fi.hasVarSizedObjects() || fi.hasOpaqueSPAdjustment())
      fail(s, name, "unsupported dynamic/opaque frame; no dynamic-storage certificate");
    if (enforce && (fl->getOffsetOfLocalArea() > 0 || !a || a != 16 ||
        fl->getStackGrowthDirection() != TargetFrameLowering::StackGrowsDown ||
        fi.hasVarSizedObjects() || fi.hasOpaqueSPAdjustment() || fi.hasTailCall() || fi.getMaxAlign() > fl->getStackAlign()))
      fail(s, name, "unsupported dynamic/tail/realigned frame");
    struct Credit { int index; int64_t start, end; uint64_t bytes; bool overlap = false; };
    bool lifetime = plan.lifetime;
    for (const auto &bb : mf.getFunction()) for (const auto &i : bb)
      if (auto *call = dyn_cast<CallBase>(&i)) if (auto *callee = call->getCalledFunction())
        lifetime |= callee->getName().starts_with("llvm.lifetime.");
    std::vector<Credit> credits;
    std::set<size_t> represented;
    json::Array incoming, zeroHomes, unmatched, canonicalHomes;
    for (int i = fi.getObjectIndexBegin(); i < fi.getObjectIndexEnd(); ++i) {
      if (fi.isDeadObjectIndex(i)) continue;
      const auto size = fi.getObjectSize(i), offset = fi.getObjectOffset(i);
      if (size < 0) { fail(s, name, "variable object"); continue; }
      if (fi.isFixedObjectIndex(i) && !fi.isSpillSlotObjectIndex(i)) {
        const bool owned = false; // No production general incoming-ABI proof yet.
        Register base;
        auto reference = fl->getFrameIndexReferencePreferSP(mf, i, base, true);
        incoming.push_back(json::Object{{"sp_reference", reference.getFixed()},
          {"base_register", mf.getSubtarget().getRegisterInfo()->getName(base)}, {"offset", offset}, {"size", size}, {"caller_owned", owned},
          {"callee_entry_sp_offset", offset + int64_t(e)}});
        if (!owned) fail(s, name, "unproved incoming ownership");
        continue;
      }
      if (enforce && (offset < -int64_t(fi.getStackSize() + e) || offset + size > -int64_t(e)))
        fail(s, name, "object outside accounted local frame (including unsupported red zone)");
      if (fi.isSpillSlotObjectIndex(i)) continue; // Included in F, never subtracted.
      const auto *allocation = fi.getObjectAllocation(i);
      bool found = false;
      for (size_t h = 0; h < plan.homes.size(); ++h) {
        const auto &home = plan.homes[h];
        const bool original = home.identity && static_cast<Value *>(home.identity) == allocation;
        const bool canonical = home.canonicalProved && home.canonical && static_cast<Value *>(home.canonical) == allocation;
        if (original || canonical) {
          found = true;
          const auto *count = allocation ? dyn_cast<ConstantInt>(allocation->getArraySize()) : nullptr;
          const bool stillZero = allocation && allocation->isStaticAlloca() && count &&
            (count->isZero() || mf.getDataLayout().getTypeAllocSize(allocation->getAllocatedType()).isZero());
          // LLVM 23.1.4 FunctionLoweringInfo::set changes a static zero-byte
          // alloca into a one-byte object with the SAME AllocaInst provenance.
          // It remains wholly in F. It never represents any byte of P for R.
          const bool zeroPhysical = home.bytes == 0 && stillZero && size == 1;
          if (!represented.insert(h).second || (!zeroPhysical && uint64_t(size) != home.bytes) ||
              fi.getObjectAlign(i).value() < home.alignment)
            fail(s, name, "inexact/duplicated original home " + home.name +
              " (planned=" + std::to_string(home.bytes) + "/align" + std::to_string(home.alignment) +
              ", machine=" + std::to_string(size) + "/align" + std::to_string(fi.getObjectAlign(i).value()) + ")");
          else if (zeroPhysical) {
            if (offset < -int64_t(fi.getStackSize() + e) || offset + size > -int64_t(e))
              fail(s, name, "zero home physical byte outside accounted frame");
            zeroHomes.push_back(json::Object{{"identity", home.name}, {"index", i},
              {"logical_bytes", 0}, {"physical_bytes", size}, {"subtraction_credit", 0}});
          } else if (canonical) {
            // The original P stays immutable and every physical byte stays in F.
            // This exact rewrite has provenance, but earns no subtraction credit.
            canonicalHomes.push_back(json::Object{{"identity", home.name}, {"index", i},
              {"bytes", size}, {"subtraction_credit", 0}});
          } else credits.push_back({i, offset, offset + size, uint64_t(size)});
          break;
        }
      }
      if (!found) {
        std::string ir;
        if (allocation) { raw_string_ostream stream(ir); allocation->print(stream); }
        unmatched.push_back(json::Object{{"index", i}, {"physical_bytes", size},
          {"alignment", int64_t(fi.getObjectAlign(i).value())}, {"current_alloca", ir},
          {"has_alloca_identity", allocation != nullptr}, {"subtraction_credit", 0}});
        fail(s, name, "unmatched allocation (including clone/merge) receives no subtraction credit");
      }
    }
    for (auto &x : credits) for (auto &y : credits)
      if (x.index != y.index && std::max(x.start, y.start) < std::min(x.end, y.end)) x.overlap = true;
    for (auto &x : credits) if (enforce && !x.overlap && !lifetime) r += x.bytes;
    // Lifetime-colored functions conservatively receive NO home subtraction.
    // Neither pointer identity nor a surviving alloca alone proves merged lineage.
    if (enforce && r > fi.getStackSize()) { fail(s, name, "home credit exceeds measured frame"); r = 0; }
    const auto charge = alignTo(plan.bytes + fi.getStackSize() - r + e, a);
    if (enforce && charge > array_function_storage_budget) fail(s, name, "65536-byte policy exceeded");
    snapshot["outgoing_sp_observation"] = outgoingObservation(mf,s.triple);
    snapshot["reserved_call_frame"] = reservedCallFrame;
    snapshot["has_tail_call"] = fi.hasTailCall();
    snapshot["early_objects"] = json::Array(s.earlyObjects[name]);
    snapshot["storage_policy_applicable"] = enforce;
    snapshot["P"] = int64_t(plan.bytes); snapshot["R"] = int64_t(r); snapshot["E"] = int64_t(e);
    snapshot["A"] = int64_t(a); snapshot["C"] = enforce ? json::Value(int64_t(charge)) : json::Value(nullptr);
    snapshot["lifetime_no_credit"] = lifetime;
    snapshot["incoming"] = std::move(incoming);
    snapshot["zero_extent_homes"] = std::move(zeroHomes);
    snapshot["canonical_byte_array_homes"] = std::move(canonicalHomes);
    snapshot["unmatched_homes"] = std::move(unmatched);
    snapshot["entry_home_moves"] = json::Array(plan.entryMoves);
    snapshot["changed_after_pei"] = s.pei[name] != key;
    auto observed = json::parse(s.pei[name]);
    if (observed) snapshot["after_pei"] = std::move(*observed);
    else { consumeError(observed.takeError()); fail(s, name, "missing PEI observation"); }
    json::Object calls;
    for (auto &[callee, area] : s.calls[name]) calls[callee] = int64_t(area);
    snapshot["machine_calls"] = std::move(calls);
    json::Array originals;
    for (auto &h : plan.homes) originals.push_back(json::Object{{"name", h.name}, {"bytes", int64_t(h.bytes)},
      {"alignment", int64_t(h.alignment)}, {"role", int64_t(h.role)}, {"kind", int64_t(h.kind)}, {"original_survives", bool(h.identity)}});
    snapshot["immutable_homes"] = std::move(originals);
    s.reports[name] = std::move(snapshot); s.late[name] = key; s.sp[name] = stackWrites(mf);
    return false;
  }
  void getAnalysisUsage(AnalysisUsage &a) const override { a.setPreservesAll(); MachineFunctionPass::getAnalysisUsage(a); }
}; char Audit::ID;
// Check encoded SP writers, not just a second reading of MachineFrameInfo.
void checkObject(State &s, StringRef bytes, TargetMachine &tm, const Target &target) {
  auto object = object::ObjectFile::createObjectFile(MemoryBufferRef(bytes, "private-object"));
  if (!object) { consumeError(object.takeError()); fail(s, "object", "object parser failed"); return; }
  MCContext ctx(Triple(s.triple), tm.getMCAsmInfo(), tm.getMCRegisterInfo(), tm.getMCSubtargetInfo());
  std::unique_ptr<MCDisassembler> dis(target.createMCDisassembler(tm.getMCSubtargetInfo(), ctx));
  if (!dis) { fail(s, "object", "no disassembler"); return; }
  const auto &ii = *tm.getMCInstrInfo(); const auto &ri = tm.getMCRegisterInfo();
  std::set<std::string> seen;
  std::set<std::tuple<uint64_t, uint64_t, uint64_t>> anchors;
  auto symbols = object::computeSymbolSizes(**object);
  for (auto &[symbol, size] : symbols) {
    auto name = symbol.getName(); auto address = symbol.getAddress(); auto section = symbol.getSection();
    if (!name || !address || !section || *section == (*object)->section_end()) continue;
    std::string fn = name->str();
    if (Triple(s.triple).isOSDarwin() && fn.compare(0, 1, "_") == 0) fn.erase(0, 1);
    if (s.sp.count(fn)) anchors.insert({(**section).getIndex(), *address, size});
  }
  for (auto &[symbol, size] : symbols) {
    auto type = symbol.getType(); auto name = symbol.getName(); auto address = symbol.getAddress(); auto section = symbol.getSection();
    if (!type || !name || !address || !section) { fail(s, "object", "unreadable symbol"); continue; }
    if (*type != object::SymbolRef::ST_Function) continue;
    std::string fn = name->str();
    if (Triple(s.triple).isOSDarwin() && fn.compare(0, 1, "_") == 0) fn.erase(0, 1);
    if (!s.sp.count(fn)) {
      // Mach-O text-section anchor aliases have function type in the generic
      // object API. Accept only aliases with the identical certified byte extent.
      if (*section != (*object)->section_end() &&
          anchors.count({(**section).getIndex(), *address, size})) continue;
      fail(s, fn, "emitted function has no machine certificate (size=" + Twine(size).str() + ")"); continue;
    }
    seen.insert(fn);
    if (*section == (*object)->section_end()) { fail(s, fn, "function has no section"); continue; }
    auto content = (**section).getContents();
    if (!content) { consumeError(content.takeError()); fail(s, fn, "no function bytes"); continue; }
    uint64_t offset = *address - (**section).getAddress();
    if (offset > content->size() || size > content->size() - offset) { fail(s, fn, "invalid function extent"); continue; }
    ArrayRef<uint8_t> code(reinterpret_cast<const uint8_t *>(content->data() + offset), size);
    std::vector<std::string> actual; json::Array rawOpcodes, calls;
    for (uint64_t pos = 0; pos < size;) {
      MCInst inst; uint64_t length = 0;
      if (dis->getInstruction(inst, length, code.slice(pos), *address + pos, nulls()) != MCDisassembler::Success || !length) {
        fail(s, fn, "final encoding could not be decoded"); break;
      }
      auto &desc = ii.get(inst.getOpcode()); bool writes = false;
      if (desc.isCall()) calls.push_back(ii.getName(inst.getOpcode()));
      for (auto reg : desc.implicit_defs()) if (spReg(reg, ri)) writes = true;
      for (unsigned i = 0; i < desc.getNumDefs() && i < inst.getNumOperands(); ++i)
        if (inst.getOperand(i).isReg() && spReg(inst.getOperand(i).getReg(), ri)) writes = true;
      if (writes && !desc.isCall() && !desc.isReturn()) {
        std::vector<std::string> operands;
        for (auto &o : inst) {
          if (o.isReg()) operands.push_back("r" + std::to_string(o.getReg()));
          else if (o.isImm()) operands.push_back("i" + std::to_string(o.getImm()));
          else operands.push_back("unsupported");
        }
        actual.push_back(sig(ii.getName(inst.getOpcode()), operands));
        rawOpcodes.push_back(ii.getName(inst.getOpcode()));
      }
      pos += length;
    }
    json::Array encoded; for (auto &v : actual) encoded.push_back(v);
    s.reports[fn]["encoded_sp_writes"] = std::move(encoded);
    s.reports[fn]["mc_sp_opcodes"] = std::move(rawOpcodes);
    if (jsonText(json::Value(json::Array(calls))) != jsonText(s.reports[fn]["call_opcodes"]))
      fail(s, fn, "final encoded calls differ from machine certificate");
    s.reports[fn]["encoded_call_opcodes"] = std::move(calls);
    if (actual != s.sp[fn]) fail(s, fn, "final encoded SP writes differ from machine certificate");
  }
  for (auto &[fn, _] : s.sp) if (!seen.count(fn)) fail(s, fn, "no final encoded function found");
}
} // namespace

std::set<StorageRole> all_storage_roles() {
  return {StorageRole::parameter, StorageRole::result, StorageRole::local,
    StorageRole::instruction, StorageRole::copy, StorageRole::roots,
    StorageRole::staging, StorageRole::known_abi};
}
void LoweringInventory::function(llvm::Function &f, bool arrays) {
  for (auto &p : functions_) if (p.function == &f) {
    if (p.array_carrying != arrays) throw std::invalid_argument("Conflicting array storage applicability");
    return;
  }
  functions_.push_back({&f, arrays, {}, {}});
}
void LoweringInventory::home(llvm::AllocaInst &a, StorageRole role, HomeKind kind, std::string id) {
  for (auto &p : functions_) if (p.function == a.getFunction()) {
    for (auto &h : p.homes) if (h.allocation == &a || h.identity == id)
      throw std::invalid_argument("Duplicate original lowering home identity");
    if (id.empty()) throw std::invalid_argument("Empty original lowering home identity");
    p.homes.push_back({&a, role, kind, std::move(id)}); return;
  }
  throw std::invalid_argument("Home without declared lowering function");
}
void LoweringInventory::complete(llvm::Function &f, std::set<StorageRole> roles) {
  for (auto &p : functions_) if (p.function == &f) { p.accounted = std::move(roles); return; }
  throw std::invalid_argument("Completion without declared lowering function");
}
struct AotObjectEmitter::Impl {
  std::unique_ptr<TargetMachine> machine;
  const Target *target;
  std::string triple, profile, report, frozenDigest, optimizedDigest, inventoryDigest, inventoryText;
  DataLayout layout;
  bool optimize, frozen = false, optimized = false, emitted = false;
  Module *module = nullptr;
  State state;
  Impl(std::string t, bool o) : triple(std::move(t)), layout(""), optimize(o) {
    static std::once_flag once;
    std::call_once(once, [] {
      if (InitializeNativeTarget() || InitializeNativeTargetAsmPrinter() || InitializeNativeTargetDisassembler())
        throw std::runtime_error("Native LLVM emission/audit target unavailable");
      auto &r = *PassRegistry::getPassRegistry(); initializeCore(r); initializeCodeGen(r); initializeTarget(r);
    });
    if (StringRef(LLVM_VERSION_STRING) != "23.1.4") throw std::runtime_error("Storage audit requires LLVM 23.1.4");
    std::string error; target = TargetRegistry::lookupTarget(Triple(triple), error);
    if (!target) throw std::runtime_error(error);
    machine.reset(target->createTargetMachine(Triple(triple), "generic", "", TargetOptions(), Reloc::PIC_,
                                             std::nullopt, CodeGenOptLevel::Default));
    if (!machine) throw std::runtime_error("Cannot create AOT target machine");
    const Triple selected(triple);
    if (selected.getArch() != Triple::x86_64 && selected.getArch() != Triple::aarch64)
      throw std::runtime_error("AOT machine audit has no proved stack-register profile for this architecture");
    layout = machine->createDataLayout(); state.triple = triple;
    // Options are factory-owned defaults, not caller-mutable TargetOptions.
    profile = jsonText(json::Object{{"llvm", LLVM_VERSION_STRING}, {"triple", triple},
      {"cpu", machine->getTargetCPU()}, {"features", machine->getTargetFeatureString()},
      {"data_layout", layout.getStringRepresentation()}, {"relocation", int64_t(machine->getRelocationModel())},
      {"code_model", int64_t(machine->getCodeModel())}, {"machine_opt", int64_t(machine->getOptLevel())},
      {"target_options", "LLVM23.1.4-default;VOID-AOT-v1"},
      {"ir_pipeline", optimize ? "default<O2>;PassBuilder(no-TM)" : "verify-only"}});
  }
  std::string module_digest() const {
    std::string text; raw_string_ostream out(text); module->print(out, nullptr);
    return toHex(SHA256::hash(arrayRefFromStringRef(text)));
  }
  void check_digest(const std::string &expected) const {
    if (module_digest() != expected) throw std::invalid_argument("AOT frozen module/profile content mismatch");
  }
  void check_module() const {
    if (!module || module->getDataLayout() != layout || module->getTargetTriple() != Triple(triple))
      throw std::invalid_argument("AOT inventory/emission target identity mismatch");
    for (const auto &f : *module) {
      for (auto name : {"target-cpu", "tune-cpu", "target-features", "target-abi"}) {
        auto attribute = f.getFnAttribute(name);
        if (!attribute.isValid()) continue;
        const auto expected = StringRef(name) == "target-features" ? machine->getTargetFeatureString() :
          StringRef(name) == "target-abi" ? StringRef("") : machine->getTargetCPU();
        if (attribute.getValueAsString() != expected)
          throw std::invalid_argument("AOT function target profile mismatch: " + f.getName().str() + "/" + name);
      }
    }
    std::string message; raw_string_ostream stream(message);
    if (verifyModule(*module, &stream)) throw std::invalid_argument("Invalid audited LLVM IR: " + message);
  }
};
AotObjectEmitter::AotObjectEmitter(std::string triple, bool optimize)
  : impl_(std::make_unique<Impl>(std::move(triple), optimize)) {}
AotObjectEmitter::~AotObjectEmitter() = default;
const DataLayout& AotObjectEmitter::data_layout() const { return impl_->layout; }
const std::string& AotObjectEmitter::triple() const { return impl_->triple; }
std::string AotObjectEmitter::identity() const { return impl_->profile; }
FixedCallLayout AotObjectEmitter::fixed_call_layout(const ir::Function& function) const {
  if(!function.fixed_ownership || function.receiver || function.initializer || !function.virtual_slot.empty())
    throw std::invalid_argument("Private fixed call requires the scalar ownership contract");
  // Independently validate canonical logical types without trusting a lowering
  // annotation or executing the original body. Full-body validation is separate.
  ir::Function shape; shape.name="private-shape"; shape.parameters=function.parameters;
  shape.locals={function.result};shape.result=ir::Type::none;shape.fixed_ownership=true;
  if(function.result==ir::Type::none) shape.locals.clear();
  shape.blocks={{{},{ir::Terminator::Kind::return_void}}};
  ir::Module logical{"private-shape",{},{shape}};logical.coordinated=true;ir::validate(logical);
  LLVMContext context;
  auto* ptr=PointerType::getUnqual(context);
  // StructLayout caches retain Type* keys. Keep these temporary-context queries
  // in a fresh layout whose cache dies before the context, never the emitter.
  const DataLayout dl(impl_->layout.getStringRepresentation());
  auto* size=dl.getIntPtrType(context);
  auto extent=[&](Type* t) {return StorageExtent{dl.getTypeAllocSize(t).getFixedValue(),dl.getABITypeAlign(t).value()};};
  auto* slot=StructType::get(context,{ptr,size,ptr,size});
  auto* failure=StructType::get(context,{ptr,Type::getInt32Ty(context),ArrayType::get(ptr,32)});
  FixedCallLayout result;result.profile=identity();result.handle=extent(ptr);result.slot=extent(slot);result.failure=extent(failure);
  result.logical_identity="fixed-owned-v1:"+function.name+":";
  auto key=[](ir::Type t) {return std::to_string(t.kind)+":"+ir::Type::key(t)+":"+
    std::to_string(t.nullable)+":"+std::to_string(t.readonly);};
  for(unsigned n=0;n<slot->getNumElements();++n) result.slot_offsets.push_back(dl.getStructLayout(slot)->getElementOffset(n));
  for(unsigned n=0;n<failure->getNumElements();++n) result.failure_offsets.push_back(dl.getStructLayout(failure)->getElementOffset(n));
  auto scalar=[&](ir::Type t)->Type* {
    switch(t.kind) {
    case ir::Type::boolean:return Type::getInt1Ty(context);
    case ir::Type::i8:case ir::Type::u8:return Type::getInt8Ty(context);
    case ir::Type::i16:case ir::Type::u16:return Type::getInt16Ty(context);
    case ir::Type::i32:case ir::Type::u32:return Type::getInt32Ty(context);
    case ir::Type::i64:case ir::Type::u64:return Type::getInt64Ty(context);
    case ir::Type::f32:return Type::getFloatTy(context);
    case ir::Type::f64:return Type::getDoubleTy(context);
    default:throw std::invalid_argument("Unsupported private fixed scalar ABI");
    }
  };
  bool carries=false;
  for(auto parameter:function.parameters) {
    const bool array=ir::scalar_fixed_array(parameter);carries|=array;
    result.operands.push_back({array ? FixedCallOperand::Mode::snapshot_home : FixedCallOperand::Mode::scalar,parameter,
                               array ? result.handle : extent(scalar(parameter))});
    result.logical_identity+=key(parameter)+";";
  }
  result.result_is_home=ir::scalar_fixed_array(function.result);carries|=result.result_is_home;
  if(!carries) throw std::invalid_argument("Array-free ABI must remain unchanged");
  if(result.result_is_home) result.operands.push_back({FixedCallOperand::Mode::result_home,function.result,result.handle});
  else if(function.result!=ir::Type::none) scalar(function.result);
  result.operands.push_back({FixedCallOperand::Mode::failure,ir::Type::none,result.handle});
  result.logical_identity+="->"+key(function.result);
  return result;
}
const std::string& AotObjectEmitter::report() const { return impl_->report; }
std::string AotObjectEmitter::research_encoded_cfg(const std::filesystem::path &path,
    const std::string &function, const std::string &certificate) const {
  auto bytes = MemoryBuffer::getFile(path.string());
  if (!bytes) throw std::runtime_error("Cannot read research object: " + bytes.getError().message());
  return inspect_research_cfg((*bytes)->getBuffer(),function,certificate,impl_->profile,*impl_->machine,*impl_->target);
}
namespace {
// Pinned InstCombine simplifyAllocaArraySize: i8,N -> [N x i8],1.
// Observe the actual RAUW edge while both objects are alive. Shape coincidence,
// matching names, arbitrary metadata and a later snapshot cannot create this edge.
void recordCanonicalRewrite(State &s, Home &home, Value *oldValue, Value *newValue,
                            const DataLayout &layout) {
  if (s.activePasses.empty() || s.activePasses.back() != "InstCombinePass") return;
  auto *old = dyn_cast<AllocaInst>(oldValue); auto *next = dyn_cast<AllocaInst>(newValue);
  if (!old || !next || !home.identity || static_cast<Value *>(home.identity) != old) return;
  auto *count = dyn_cast<ConstantInt>(old->getArraySize());
  auto *nextCount = dyn_cast<ConstantInt>(next->getArraySize());
  auto *array = dyn_cast<ArrayType>(next->getAllocatedType());
  bool proved = old->isStaticAlloca() && next->isStaticAlloca() && old->getFunction() == next->getFunction() &&
    old->getAddressSpace() == 0 && next->getAddressSpace() == 0 && old->getAllocatedType()->isIntegerTy(8) &&
    count && count->getValue().getActiveBits() <= 63 && count->getZExtValue() > 1 &&
    nextCount && nextCount->isOne() && array && array->getElementType() == old->getAllocatedType() &&
    array->getNumElements() == count->getZExtValue() && old->getAlign() == next->getAlign() &&
    !old->isUsedWithInAlloca() && !next->isUsedWithInAlloca() && !old->isSwiftError() && !next->isSwiftError();
  if (proved) proved = layout.getTypeAllocSize(array) == home.bytes && next->getAlign().value() == home.alignment;
  // Never let two original homes claim a single replacement identity.
  for (auto &[name, plan] : s.plans) for (auto &other : plan.homes) if (&other != &home)
    if ((other.identity && static_cast<Value *>(other.identity) == next) ||
        (other.canonical && static_cast<Value *>(other.canonical) == next)) {
      other.canonicalProved = false; proved = false;
    }
  home.canonical = next; home.canonicalProved = proved;
  s.rewriteEvents.push_back(json::Object{{"original_identity", home.name},
    {"pass", s.activePasses.back()}, {"rule", "LLVM23.1.4-InstCombine-i8-count-to-array"},
    {"proved", proved}, {"bytes", int64_t(home.bytes)}, {"subtraction_credit", 0}});
}
// Observations identify births/deaths across actual production pass boundaries.
// They DO NOT map a new allocation to an old home, infer a partition, follow RAUW,
// or grant credit. A deleted address reused by LLVM gets a fresh identity.
void observeStorage(State &s, Module &module, StringRef boundary) {
  std::map<std::string, std::string> current;
  json::Array created, removed, changed;
  std::map<const Value *, std::vector<std::string>> originalNames;
  for (const auto &[function, plan] : s.plans) for (const auto &home : plan.homes)
    if (home.identity) originalNames[static_cast<Value *>(home.identity)].push_back(home.name);
  for (auto &f : module) for (auto &bb : f) for (auto &inst : bb) {
    auto *a = dyn_cast<AllocaInst>(&inst); if (!a) continue;
    std::string id;
    auto existing = s.traceIndex.find(a);
    if (existing != s.traceIndex.end()) {
      const auto &home = s.traceHomes[existing->second];
      if (home.value && static_cast<Value *>(home.value) == a) id = home.id;
    }
    if (id.empty()) {
      id = "observed/" + std::to_string(s.traceHomes.size());
      s.traceIndex[a] = s.traceHomes.size();
      s.traceHomes.push_back({WeakVH(a), id});
    }
    std::string type; raw_string_ostream out(type); a->getAllocatedType()->print(out);
    std::string count; raw_string_ostream countOut(count); a->getArraySize()->printAsOperand(countOut,false);
    json::Array originals;
    for (const auto &name : originalNames[a]) originals.push_back(name);
    auto description = jsonText(json::Object{{"id", id}, {"function", f.getName().str()},
      {"type", type}, {"count", count}, {"alignment", int64_t(a->getAlign().value())},
      {"original_identities", std::move(originals)}});
    current[id] = description;
    auto old = s.lastTrace.find(id);
    if (old == s.lastTrace.end()) created.push_back(description);
    else if (old->second != description) changed.push_back(description);
  }
  for (const auto &[id, description] : s.lastTrace)
    if (!current.count(id)) removed.push_back(description);
  if (!created.empty() || !removed.empty() || !changed.empty())
    s.optimizationEvents.push_back(json::Object{{"observed_after", boundary.str()},
      {"created", std::move(created)}, {"removed", std::move(removed)}, {"changed", std::move(changed)},
      {"reconciliation_credit", 0}});
  s.lastTrace = std::move(current);
}
bool reaches(const BasicBlock *from, const BasicBlock *to, bool nonempty) {
  std::vector<const BasicBlock *> work;
  if (nonempty) for (auto *next : successors(from)) work.push_back(next);
  else work.push_back(from);
  std::set<const BasicBlock *> seen;
  while (!work.empty()) {
    auto *block = work.back(); work.pop_back();
    if (block == to) return true;
    if (!seen.insert(block).second) continue;
    for (auto *next : successors(block)) work.push_back(next);
  }
  return false;
}
void prove_cli_home(const LoweredHome &home, const DataLayout &layout) {
  auto &a = *home.allocation; auto &f = *a.getFunction();
  auto *count = dyn_cast<ConstantInt>(a.getArraySize());
  bool shape = f.getName() == "main" && f.getReturnType()->isIntegerTy(32) && f.arg_size() == 2 &&
    f.getArg(0)->getType()->isIntegerTy(32) && f.getArg(1)->getType()->isPointerTy() && !f.isVarArg() &&
    f.getCallingConv() == CallingConv::C && home.role == StorageRole::known_abi &&
    count && count->isOne() && a.getAddressSpace() == 0 && a.getAlign() == layout.getABITypeAlign(a.getAllocatedType());
  if (home.kind == HomeKind::cli_parse) shape &= a.getAllocatedType()->isPointerTy();
  else if (home.kind == HomeKind::cli_failure) {
    auto *t = dyn_cast<StructType>(a.getAllocatedType());
    shape &= t && !t->isPacked() && t->getNumElements() == 3 && t->getElementType(0)->isPointerTy() &&
      t->getElementType(1)->isIntegerTy(32) && t->getElementType(2)->isArrayTy();
    if (shape) { auto *frames = cast<ArrayType>(t->getElementType(2)); shape &= frames->getNumElements() == 32 && frames->getElementType()->isPointerTy(); }
  } else shape = false;
  if (!shape) throw std::invalid_argument("Unproved CLI entry-home shape/alignment");
  if (!reaches(&f.getEntryBlock(), a.getParent(), false) || reaches(a.getParent(), a.getParent(), true))
    throw std::invalid_argument("CLI entry home may execute repeatedly or is unreachable");
  for (auto &b : f) for (auto &instruction : b) if (auto *call = dyn_cast<CallBase>(&instruction)) {
    auto *callee = call->getCalledFunction();
    if (call->hasFnAttr(Attribute::ReturnsTwice) || (callee &&
        (callee->getIntrinsicID() == Intrinsic::stacksave || callee->getIntrinsicID() == Intrinsic::stackrestore)))
      throw std::invalid_argument("CLI entry-home lifetime crosses unsupported stack control");
  }
}
} // namespace
void AotObjectEmitter::freeze(Module &module, const LoweringInventory &inventory) {
  auto &i = *impl_;
  if (i.frozen) throw std::logic_error("AOT storage inventory is immutable/single-use");
  i.frozen = true; i.module = &module; i.check_module();
  auto &s = i.state;
  std::vector<const LoweredHome *> moves;
  for (const auto &p : inventory.functions()) {
    if (!p.function || p.function->getParent() != &module || p.function->isDeclaration())
      throw std::invalid_argument("Foreign/declaration storage inventory");
    s.enforce |= p.array_carrying;
  }
  for (const auto &f : module) if (!f.isDeclaration()) {
    const FunctionInventory *input = nullptr;
    for (const auto &p : inventory.functions()) if (p.function == &f) input = &p;
    if (!input || input->accounted != all_storage_roles())
      throw std::invalid_argument("Incomplete lowering storage categories for " + f.getName().str());
    NativeStoragePlanner planner({i.triple, i.layout.getStringRepresentation(),
      uint64_t(std::numeric_limits<int64_t>::max()), i.layout.getStackAlignment().value().value(),
      [](ir::Type) -> StorageExtent { throw std::logic_error("Use actual LLVM homes for AOT inventory"); }});
    auto &plan = s.plans[f.getName().str()]; plan.policy = input->array_carrying;
    std::set<const AllocaInst *> seen;
    for (const auto &home : input->homes) {
      auto *a = home.allocation;
      if (!a || a->getFunction() != &f || !seen.insert(a).second)
        throw std::invalid_argument("Foreign/duplicate original LLVM allocation");
      const bool cli = home.kind == HomeKind::cli_parse || home.kind == HomeKind::cli_failure;
      if (cli) { prove_cli_home(home, i.layout); if (!a->isStaticAlloca()) moves.push_back(&home); }
      auto *count = dyn_cast<ConstantInt>(a->getArraySize());
      auto size = i.layout.getTypeAllocSize(a->getAllocatedType());
      if (!count || count->getValue().getActiveBits() > 63 || size.isScalable() || (s.enforce && !a->isStaticAlloca() && !cli))
        throw std::invalid_argument("Unproved original native storage extent");
      auto n = count->getZExtValue(); auto unit = size.getFixedValue();
      if (n && unit > uint64_t(std::numeric_limits<int64_t>::max()) / n)
        throw std::length_error("Original storage extent overflow");
      auto bytes = n * unit;
      planner.storage(home.role, home.identity, {bytes, a->getAlign().value()});
      plan.homes.push_back({WeakVH(a), home.identity, bytes, a->getAlign().value(), home.role, home.kind, WeakVH(), false});
    }
    for (const auto &bb : f) for (const auto &instruction : bb) {
      if (auto *a = dyn_cast<AllocaInst>(&instruction))
        if (!seen.count(a)) throw std::invalid_argument("LLVM allocation omitted from original lowering inventory");
      if (auto *call = dyn_cast<CallBase>(&instruction)) if (auto *callee = call->getCalledFunction())
        plan.lifetime |= callee->getName().starts_with("llvm.lifetime.");
    }
    plan.bytes = planner.finish(input->accounted, false).unrounded_bytes;
  }
  // Move only the allocation instruction, never its initializer, debug users,
  // lifetime markers or calls. SSA identity and all original uses are retained.
  // The adapter producer owns these hidden ABI homes; they are not VOID values.
  for (const auto *home : moves) {
    auto *a = home->allocation; auto *f = a->getFunction();
    s.plans[f->getName().str()].entryMoves.push_back(json::Object{
      {"identity", home->identity}, {"original_block", a->getParent()->getName().str()},
      {"maximum_executions_per_invocation", 1}, {"original_identity_retained", true}});
    a->moveBefore(f->getEntryBlock().getFirstInsertionPt());
  }
  // Original call-graph closure keeps array-free callers under the proof gate
  // even when optimization removes every physical trace of the inlined home.
  bool changed;
  do {
    changed = false;
    for (auto &f : module) if (!f.isDeclaration())
      for (auto &bb : f) for (auto &inst : bb)
        if (auto *call = dyn_cast<CallBase>(&inst)) if (auto *callee = call->getCalledFunction()) {
          auto p = s.plans.find(callee->getName().str());
          if (p != s.plans.end() && p->second.policy && !s.plans[f.getName().str()].policy)
            s.plans[f.getName().str()].policy = changed = true;
        }
  } while (changed);
  for (auto &f : module) if (!f.isDeclaration())
    for (auto &bb : f) for (auto &inst : bb)
      if (auto *call = dyn_cast<CallBase>(&inst)) if (auto *callee = call->getCalledFunction()) {
        auto p = s.plans.find(callee->getName().str());
        if (p != s.plans.end() && p->second.policy) s.plans[f.getName().str()].arrayCalls.insert(callee->getName().str());
      }
  json::Object frozen;
  for (const auto &[name, plan] : s.plans) {
    json::Array homes;
    for (const auto &home : plan.homes) homes.push_back(json::Object{
      {"identity", home.name}, {"bytes", int64_t(home.bytes)}, {"alignment", int64_t(home.alignment)},
      {"role", int64_t(home.role)}, {"kind", int64_t(home.kind)}});
    frozen[name] = json::Object{{"P", int64_t(plan.bytes)}, {"policy", plan.policy}, {"homes", std::move(homes)}};
  }
  i.inventoryText = jsonText(std::move(frozen));
  i.inventoryDigest = toHex(SHA256::hash(arrayRefFromStringRef(i.inventoryText)));
  i.frozenDigest = i.module_digest();
  observeStorage(s,module,"frozen-inventory");
  for (auto &[name, plan] : s.plans) for (auto &home : plan.homes) {
    const bool owned = (home.kind == HomeKind::frame && home.role == StorageRole::roots) ||
      (home.kind == HomeKind::staging && home.role == StorageRole::staging);
    if (owned) {
      auto *original = static_cast<Value *>(home.identity); auto *identity = &home;
      s.rewriteObservers.push_back(std::make_unique<RewriteObserver>(original,
        [&s,identity,&module](Value *oldValue, Value *newValue) {
          recordCanonicalRewrite(s,*identity,oldValue,newValue,module.getDataLayout());
        }));
    }
  }
}
void AotObjectEmitter::optimize() {
  auto &i = *impl_;
  if (!i.frozen || i.optimized) throw std::logic_error("Invalid AOT optimizer lifecycle");
  i.check_module(); i.check_digest(i.frozenDigest);
  // Preserve the existing production pipeline exactly; target-aware IR
  // optimization is not silently substituted from the standalone experiment.
  if (i.optimize) {
    LoopAnalysisManager loops; FunctionAnalysisManager functions;
    CGSCCAnalysisManager cgscc; ModuleAnalysisManager modules;
    PassInstrumentationCallbacks observations;
    observations.registerBeforeNonSkippedPassCallback([&](StringRef pass, const Any &) {
      i.state.activePasses.push_back(pass.str());
    });
    auto observed = [&](StringRef pass) {
      observeStorage(i.state,*i.module,pass);
      if (i.state.activePasses.empty() || i.state.activePasses.back() != pass)
        fail(i.state,"optimizer","incomplete pass-observation nesting");
      else i.state.activePasses.pop_back();
    };
    observations.registerAfterPassCallback([&](StringRef pass, const Any &, const PreservedAnalyses &) { observed(pass); });
    observations.registerAfterPassInvalidatedCallback([&](StringRef pass, const PreservedAnalyses &) { observed(pass); });
    PassBuilder builder(nullptr,PipelineTuningOptions(),std::nullopt,&observations);
    builder.registerModuleAnalyses(modules); builder.registerCGSCCAnalyses(cgscc);
    builder.registerFunctionAnalyses(functions); builder.registerLoopAnalyses(loops);
    builder.crossRegisterProxies(loops, functions, cgscc, modules);
    auto pipeline = builder.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    pipeline.run(*i.module, modules);
  }
  i.check_module();
  for (auto &[name, plan] : i.state.plans) if (plan.policy) {
    auto *function = i.module->getFunction(name);
    if (!function || function->isDeclaration()) {
      fail(i.state, name, "array-carrying function removed without transformation-lineage proof"); continue;
    }
    std::set<std::string> retained;
    for (auto &bb : *function) for (auto &inst : bb)
      if (auto *call = dyn_cast<CallBase>(&inst)) if (auto *callee = call->getCalledFunction()) retained.insert(callee->getName().str());
    for (auto &callee : plan.arrayCalls) if (!retained.count(callee))
      fail(i.state, name, "unproved transformed array-call lineage (including fully eliminated inline homes)");
  }
  i.optimizedDigest = i.module_digest();
  i.optimized = true;
}
void AotObjectEmitter::deliver(const std::filesystem::path &destination) {
  auto &i = *impl_;
  if (!i.optimized || i.emitted) throw std::logic_error("Invalid audited AOT delivery lifecycle");
  i.emitted = true; i.check_module(); i.check_digest(i.optimizedDigest); auto &s = i.state;
  SmallVector<char, 0> bytes; raw_svector_ostream buffer(bytes);
  auto *tm = i.machine.get();
  legacy::PassManager pm; auto *mmi = new MachineModuleInfoWrapperPass(tm); auto *config = tm->createPassConfig(pm);
  config->setDisableVerify(false);
  config->insertPass(&FinalizeISelID, new Early(s));
  config->insertPass(&PrologEpilogCodeInserterID, new Audit(s, 0));
  pm.add(config); pm.add(mmi);
  // Match LLVM 23's ordinary target codegen setup. The default legacy runtime
  // library analysis uses an unknown triple and otherwise expands host memset
  // into IR loops, moving later static homes out of the entry block.
  const auto &options = tm->Options;
  pm.add(new TargetLibraryInfoWrapperPass(TargetLibraryInfoImpl(tm->getTargetTriple(), options.VecLib)));
  pm.add(new RuntimeLibraryInfoWrapper(tm->getTargetTriple(), options.ExceptionModel,
      options.FloatABIType, options.EABIVersion, options.MCOptions.ABIName, options.VecLib));
  if (config->addISelPasses()) throw std::runtime_error("Audited AOT instruction selection unavailable");
  config->addMachinePasses(); config->setInitialized(); pm.add(new Audit(s, 1));
  if (tm->addAsmPrinter(pm, buffer, nullptr, CodeGenFileType::ObjectFile, mmi->getMMI().getContext()))
    throw std::runtime_error("Audited AOT object emission unavailable");
  pm.add(new Audit(s, 2)); pm.add(createFreeMachineFunctionPass()); pm.run(*i.module);
  for (const auto &f : *i.module) if (!f.isDeclaration()) {
    const auto name = f.getName().str();
    if (!s.reports.count(name) || !s.pei.count(name) || !s.late.count(name))
      fail(s, name, "incomplete machine-function coverage");
  }
  checkObject(s, StringRef(bytes.data(), bytes.size()), *i.machine, *i.target);
  json::Object functions, errors;
  for (auto &[name, r] : s.reports) functions[name] = std::move(r);
  for (auto &[name, failures] : s.errors) { json::Array a; for (auto &v : failures) a.push_back(v); errors[name] = std::move(a); }
  auto digest = SHA256::hash(ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()));
  i.report = jsonText(json::Object{{"profile", i.profile}, {"certified_delivery", s.errors.empty()},
    {"array_storage_policy_applicable", s.enforce}, {"budget", int64_t(array_function_storage_budget)},
    {"frozen_module_sha256", i.frozenDigest}, {"optimized_module_sha256", i.optimizedDigest},
    {"optimization_storage_observations", std::move(s.optimizationEvents)},
    {"canonical_rewrite_observations", std::move(s.rewriteEvents)},
    {"inventory", i.inventoryText}, {"inventory_sha256", i.inventoryDigest}, {"object_sha256", toHex(digest)}, {"functions", std::move(functions)}, {"errors", std::move(errors)}});
  if (!s.errors.empty()) {
    std::string reason;
    for (const auto &[name, failures] : s.errors) for (const auto &v : failures) reason += name + ": " + v + "; ";
    throw std::runtime_error("AOT machine storage/emission certification failed: " + reason);
  }
  SmallString<256> temporary; int descriptor;
  auto ec = sys::fs::createUniqueFile(destination.string() + ".private-%%%%%%", descriptor, temporary);
  if (ec) throw std::runtime_error("Cannot stage certified AOT object: " + ec.message());
  raw_fd_ostream file(descriptor, true); file.write(bytes.data(), bytes.size()); file.close();
  if (file.has_error()) { sys::fs::remove(temporary); throw std::runtime_error("Certified AOT object write failed"); }
  ec = sys::fs::rename(temporary, destination.string());
  if (ec) { sys::fs::remove(temporary); throw std::runtime_error("Certified AOT object delivery failed: " + ec.message()); }
}
} // namespace vd::llvm_backend
