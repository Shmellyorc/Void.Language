// Production AOT emission boundary extracted from the pinned machine proof.
#include "void/llvm_storage_audit.hpp"
#include <limits>
#include <mutex>
#include <stdexcept>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
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
struct Home { WeakVH identity; std::string name; uint64_t bytes, alignment; StorageRole role; HomeKind kind; };
struct Plan { std::vector<Home> homes; uint64_t bytes = 0; bool lifetime = false; bool policy = false; json::Array entryMoves; std::set<std::string> arrayCalls; };
struct State {
  std::map<std::string, Plan> plans;
  std::map<std::string, json::Object> reports;
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
struct Early : MachineFunctionPass {
  static char ID; State &s;
  Early(State &s) : MachineFunctionPass(ID), s(s) {}
  bool runOnMachineFunction(MachineFunction &mf) override {
    const auto &ti = *mf.getSubtarget().getInstrInfo();
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
    json::Array incoming, zeroHomes, unmatched;
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
        if (home.identity && static_cast<Value *>(home.identity) == allocation) {
          found = true;
          const auto *count = allocation ? dyn_cast<ConstantInt>(allocation->getArraySize()) : nullptr;
          const bool stillZero = allocation && allocation->isStaticAlloca() && count &&
            (count->isZero() || mf.getDataLayout().getTypeAllocSize(allocation->getAllocatedType()).isZero());
          // LLVM 19.1.7 FunctionLoweringInfo::set changes a static zero-byte
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
    snapshot["storage_policy_applicable"] = enforce;
    snapshot["P"] = int64_t(plan.bytes); snapshot["R"] = int64_t(r); snapshot["E"] = int64_t(e);
    snapshot["A"] = int64_t(a); snapshot["C"] = enforce ? json::Value(int64_t(charge)) : json::Value(nullptr);
    snapshot["lifetime_no_credit"] = lifetime;
    snapshot["incoming"] = std::move(incoming);
    snapshot["zero_extent_homes"] = std::move(zeroHomes);
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
  std::unique_ptr<MCDisassembler> dis(target.createMCDisassembler(*tm.getMCSubtargetInfo(), ctx));
  if (!dis) { fail(s, "object", "no disassembler"); return; }
  const auto &ii = *tm.getMCInstrInfo(); const auto &ri = *tm.getMCRegisterInfo();
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
    if (StringRef(LLVM_VERSION_STRING) != "19.1.7") throw std::runtime_error("Storage audit requires LLVM 19.1.7");
    std::string error; target = TargetRegistry::lookupTarget(triple, error);
    if (!target) throw std::runtime_error(error);
    machine.reset(target->createTargetMachine(triple, "generic", "", TargetOptions(), Reloc::PIC_,
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
      {"target_options", "LLVM19.1.7-default;VOID-AOT-v1"},
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
    if (!module || module->getDataLayout() != layout || module->getTargetTriple() != triple)
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
const std::string& AotObjectEmitter::report() const { return impl_->report; }
namespace {
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
      uint64_t(std::numeric_limits<int64_t>::max()), i.layout.getStackAlignment().value(),
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
      plan.homes.push_back({WeakVH(a), home.identity, bytes, a->getAlign().value(), home.role, home.kind});
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
    a->moveBefore(&*f->getEntryBlock().getFirstInsertionPt());
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
}
void AotObjectEmitter::optimize() {
  auto &i = *impl_;
  if (!i.frozen || i.optimized) throw std::logic_error("Invalid AOT optimizer lifecycle");
  i.check_module(); i.check_digest(i.frozenDigest);
  // Preserve the existing production pipeline exactly; target-aware IR
  // optimization is not silently substituted from the standalone experiment.
  if (i.optimize) {
    LoopAnalysisManager loops; FunctionAnalysisManager functions;
    CGSCCAnalysisManager cgscc; ModuleAnalysisManager modules; PassBuilder builder;
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
  auto *tm = static_cast<LLVMTargetMachine *>(i.machine.get());
  legacy::PassManager pm; auto *mmi = new MachineModuleInfoWrapperPass(tm); auto *config = tm->createPassConfig(pm);
  config->setDisableVerify(false);
  config->insertPass(&FinalizeISelID, new Early(s));
  config->insertPass(&PrologEpilogCodeInserterID, new Audit(s, 0));
  pm.add(config); pm.add(mmi);
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
