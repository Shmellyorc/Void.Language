// LLVM 19.1.7 investigation only. Not a VOID emitter or production certificate.
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
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Constants.h>
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
namespace {
struct Home { WeakVH identity; std::string name; uint64_t bytes, alignment; };
struct Plan { std::vector<Home> homes; uint64_t bytes = 0; bool lifetime = false; std::set<std::string> fiveArgs; };
struct State {
  std::map<std::string, Plan> plans;
  std::map<std::string, json::Object> reports;
  std::map<std::string, std::string> pei, late;
  std::map<std::string, std::vector<std::string>> sp;
  std::map<std::string, std::map<std::string, uint64_t>> calls;
  std::map<std::string, std::vector<std::string>> errors;
  json::Value incomingProof = nullptr;
  std::string fault, triple;
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
    {"alignment", int64_t(fi.getMaxAlign().value())}, {"objects", std::move(objects)},
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
    uint64_t r = 0, e = -fl->getOffsetOfLocalArea(), a = fl->getStackAlign().value();
    if (fl->getOffsetOfLocalArea() > 0 || !a || a != 16 ||
        fl->getStackGrowthDirection() != TargetFrameLowering::StackGrowsDown ||
        fi.hasVarSizedObjects() || fi.hasOpaqueSPAdjustment() || fi.hasTailCall() || fi.getMaxAlign() > fl->getStackAlign())
      fail(s, name, "unsupported dynamic/tail/realigned frame");
    struct Credit { int index; int64_t start, end; uint64_t bytes; bool overlap = false; };
    bool lifetime = plan.lifetime;
    for (const auto &bb : mf.getFunction()) for (const auto &i : bb)
      if (auto *call = dyn_cast<CallBase>(&i)) if (auto *callee = call->getCalledFunction())
        lifetime |= callee->getName().starts_with("llvm.lifetime.");
    std::vector<Credit> credits;
    std::set<size_t> represented;
    json::Array incoming;
    for (int i = fi.getObjectIndexBegin(); i < fi.getObjectIndexEnd(); ++i) {
      if (fi.isDeadObjectIndex(i)) continue;
      const auto size = fi.getObjectSize(i), offset = fi.getObjectOffset(i);
      if (size < 0) { fail(s, name, "variable object"); continue; }
      if (fi.isFixedObjectIndex(i) && !fi.isSpillSlotObjectIndex(i)) {
        // Deliberately narrow ownership proof: this exact five-scalar/pointer
        // C signature and a previously certified actual Windows caller edge.
        bool owned = false;
        if (s.triple == "x86_64-pc-windows-msvc" && mf.getFunction().arg_size() == 5 &&
            mf.getFunction().getCallingConv() == CallingConv::C && !mf.getFunction().isVarArg() &&
            size == 8 && offset == 32 && e == 8 && mf.getFunction().getArg(0)->getType()->isPointerTy() &&
            mf.getFunction().getArg(1)->getType()->isPointerTy() &&
            mf.getFunction().getArg(2)->getType()->isIntegerTy(32) &&
            mf.getFunction().getArg(3)->getType()->isIntegerTy(32) &&
            mf.getFunction().getArg(4)->getType()->isPointerTy() &&
            mf.getFunction().getReturnType()->isVoidTy()) {
          if (auto *root = s.incomingProof.getAsObject()) {
            auto *functions = root->getObject("functions");
            if (root->getBoolean("certified").value_or(false) && root->getString("triple") == s.triple && functions)
              for (auto &kv : *functions) if (auto *caller = kv.second.getAsObject()) {
                if (auto *edges = caller->getObject("machine_calls"))
                  if (edges->getInteger(name).value_or(-1) == 40)
                    if (auto *five = caller->getArray("proved_five_arg_edges"))
                      for (auto &edge : *five) if (edge.getAsString() == name) owned = true;
              }
          }
        }
        Register base;
        auto reference = fl->getFrameIndexReferencePreferSP(mf, i, base, true);
        incoming.push_back(json::Object{{"sp_reference", reference.getFixed()},
          {"base_register", mf.getSubtarget().getRegisterInfo()->getName(base)}, {"offset", offset}, {"size", size}, {"caller_owned", owned},
          {"callee_entry_sp_offset", offset + int64_t(e)}});
        if (!owned) fail(s, name, "unproved incoming ownership");
        continue;
      }
      if (offset < -int64_t(fi.getStackSize() + e) || offset + size > -int64_t(e))
        fail(s, name, "object outside accounted local frame (including unsupported red zone)");
      if (fi.isSpillSlotObjectIndex(i)) continue; // Included in F, never subtracted.
      const auto *allocation = fi.getObjectAllocation(i);
      bool found = false;
      for (size_t h = 0; h < plan.homes.size(); ++h) {
        const auto &home = plan.homes[h];
        if (home.identity && static_cast<Value *>(home.identity) == allocation) {
          found = true;
          if (!represented.insert(h).second || uint64_t(size) != home.bytes || fi.getObjectAlign(i).value() < home.alignment)
            fail(s, name, "inexact/duplicated original home");
          else credits.push_back({i, offset, offset + size, uint64_t(size)});
          break;
        }
      }
      if (!found) fail(s, name, "unmatched allocation (including clone/merge) receives no subtraction credit");
    }
    for (auto &x : credits) for (auto &y : credits)
      if (x.index != y.index && std::max(x.start, y.start) < std::min(x.end, y.end)) x.overlap = true;
    for (auto &x : credits) if (!x.overlap && !lifetime) r += x.bytes;
    // Lifetime-colored functions conservatively receive NO home subtraction.
    // Neither pointer identity nor a surviving alloca alone proves merged lineage.
    if (r > fi.getStackSize()) { fail(s, name, "home credit exceeds measured frame"); r = 0; }
    const auto charge = alignTo(plan.bytes + fi.getStackSize() - r + e, a);
    if (charge > 65536) fail(s, name, "65536-byte policy exceeded");
    snapshot["P"] = int64_t(plan.bytes); snapshot["R"] = int64_t(r); snapshot["E"] = int64_t(e);
    snapshot["A"] = int64_t(a); snapshot["C"] = int64_t(charge);
    snapshot["lifetime_no_credit"] = lifetime;
    snapshot["incoming"] = std::move(incoming);
    snapshot["changed_after_pei"] = s.pei[name] != key;
    auto observed = json::parse(s.pei[name]);
    if (observed) snapshot["after_pei"] = std::move(*observed);
    else { consumeError(observed.takeError()); fail(s, name, "missing PEI observation"); }
    json::Object calls;
    for (auto &[callee, area] : s.calls[name]) calls[callee] = int64_t(area);
    snapshot["machine_calls"] = std::move(calls);
    json::Array five;
    // Actual late Windows store at outgoing SP+32 followed by the matching
    // direct call, plus the original exact C signature and pre-PEI area=40.
    for (auto &bb : mf) {
      bool stored = false;
      for (auto &mi : bb) {
        const auto &ti = *mf.getSubtarget().getInstrInfo();
        if (ti.getName(mi.getOpcode()) == "MOV64mr" && mi.getNumOperands() >= 6 &&
            mi.getOperand(0).isReg() && spReg(mi.getOperand(0).getReg(), *mf.getSubtarget().getRegisterInfo()) &&
            mi.getOperand(1).isImm() && mi.getOperand(1).getImm() == 1 &&
            mi.getOperand(2).isReg() && !mi.getOperand(2).getReg() &&
            mi.getOperand(3).isImm() && mi.getOperand(3).getImm() == 32) stored = true;
        if (mi.isCall()) {
          for (auto &o : mi.operands()) if (o.isGlobal()) {
            auto callee = o.getGlobal()->getName().str();
            if (stored && plan.fiveArgs.count(callee) && s.calls[name][callee] == 40) five.push_back(callee);
          }
          stored = false;
        }
      }
    }
    snapshot["proved_five_arg_edges"] = std::move(five);
    json::Array originals;
    for (auto &h : plan.homes) originals.push_back(json::Object{{"name", h.name}, {"bytes", int64_t(h.bytes)},
      {"alignment", int64_t(h.alignment)}, {"original_survives", bool(h.identity)}});
    snapshot["immutable_homes"] = std::move(originals);
    s.reports[name] = std::move(snapshot); s.late[name] = key; s.sp[name] = stackWrites(mf);
    return false;
  }
  void getAnalysisUsage(AnalysisUsage &a) const override { a.setPreservesAll(); MachineFunctionPass::getAnalysisUsage(a); }
}; char Audit::ID;
struct Fault : MachineFunctionPass {
  static char ID; State &s;
  Fault(State &s) : MachineFunctionPass(ID), s(s) {}
  bool runOnMachineFunction(MachineFunction &mf) override {
    if (s.fault == "frame") mf.getFrameInfo().setStackSize(mf.getFrameInfo().getStackSize() + 16);
    if (s.fault == "sp") for (auto &bb : mf) for (auto &mi : bb) {
      bool writes = false;
      for (auto &o : mi.operands())
        if (o.isReg() && o.isDef() && spReg(o.getReg(), *mf.getSubtarget().getRegisterInfo())) writes = true;
      if (writes && !mi.isCall() && !mi.isReturn()) for (auto &o : mi.explicit_operands())
        if (o.isImm() && o.getImm()) { o.setImm(o.getImm() + 1); return true; }
    }
    return !s.fault.empty();
  }
}; char Fault::ID;
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
int main(int argc, char **argv) {
  // INPUT MODE(normal|O2) REPORT OBJECT [FAULT(none|frame|sp|omit|encoded)] [CALLER-CERT]
  if (argc < 5 || argc > 7 || StringRef(LLVM_VERSION_STRING) != "19.1.7") return 1;
  if (sys::fs::exists(argv[4])) { errs() << "refusing an existing delivery path\n"; return 1; }
  State s; if (argc > 5 && StringRef(argv[5]) != "none") s.fault = argv[5];
  if (!s.fault.empty() && s.fault != "frame" && s.fault != "sp" && s.fault != "omit" && s.fault != "encoded") return 1;
  if (argc > 6) {
    auto f = MemoryBuffer::getFile(argv[6]); if (!f) return 1;
    auto j = json::parse((*f)->getBuffer()); if (!j) { consumeError(j.takeError()); return 1; }
    s.incomingProof = std::move(*j);
    // A proof imported by this local investigation must still name the actual
    // delivered caller object. Production should keep certificates in memory.
    SmallString<256> objectPath(argv[6]); sys::path::replace_extension(objectPath, ".o");
    auto object = MemoryBuffer::getFile(objectPath); if (!object) return 1;
    auto bytes = (*object)->getBuffer();
    auto digest = SHA256::hash(ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()));
    auto *root = s.incomingProof.getAsObject();
    if (!root || root->getString("private_object_sha256") != toHex(digest)) return 1;
  }
  InitializeAllTargetInfos(); InitializeAllTargets(); InitializeAllTargetMCs(); InitializeAllAsmPrinters(); InitializeAllDisassemblers();
  auto &registry = *PassRegistry::getPassRegistry(); initializeCore(registry); initializeCodeGen(registry); initializeTarget(registry);
  LLVMContext context; SMDiagnostic diagnostic;
  auto module = parseIRFile(argv[1], diagnostic, context);
  if (!module) { diagnostic.print(argv[0], errs()); return 1; }
  s.triple = module->getTargetTriple(); std::string error;
  if (s.triple != "x86_64-pc-linux-gnu" && s.triple != "x86_64-pc-windows-msvc" &&
      s.triple != "arm64-apple-macosx14.0.0") return 1;
  const auto *target = TargetRegistry::lookupTarget(s.triple, error); if (!target) { errs() << error; return 1; }
  std::unique_ptr<TargetMachine> machine(target->createTargetMachine(s.triple, "generic", "", TargetOptions(), Reloc::PIC_, std::nullopt, CodeGenOptLevel::Default));
  if (!machine || module->getDataLayout() != machine->createDataLayout() || verifyModule(*module, &errs())) return 1;
  for (auto &f : *module) if (!f.isDeclaration()) {
    auto &p = s.plans[f.getName().str()];
    for (auto &bb : f) for (auto &i : bb) {
      if (auto *call = dyn_cast<CallBase>(&i)) if (auto *callee = call->getCalledFunction())
        {
          if (callee->getName().starts_with("llvm.lifetime.")) p.lifetime = true;
          auto *t = callee->getFunctionType();
          if (!t->isVarArg() && t->getNumParams() == 5 && t->getReturnType()->isVoidTy() &&
              t->getParamType(0)->isPointerTy() && t->getParamType(1)->isPointerTy() &&
              t->getParamType(2)->isIntegerTy(32) && t->getParamType(3)->isIntegerTy(32) &&
              t->getParamType(4)->isPointerTy() && call->getCallingConv() == CallingConv::C)
            p.fiveArgs.insert(callee->getName().str());
        }
      if (auto *a = dyn_cast<AllocaInst>(&i)) {
        if (s.fault == "omit" && a->getName() == "snapshot") continue; // Fault fixture only.
        auto *n = dyn_cast<ConstantInt>(a->getArraySize());
        if (!n || !a->isStaticAlloca()) { fail(s, f.getName(), "nonstatic original inventory"); continue; }
        auto size = module->getDataLayout().getTypeAllocSize(a->getAllocatedType());
        if (size.isScalable() || n->getZExtValue() > 65536 || size.getFixedValue() > 65536) { fail(s, f.getName(), "unsupported original extent"); continue; }
        uint64_t bytes = size.getFixedValue() * n->getZExtValue();
        p.bytes = alignTo(p.bytes, a->getAlign()) + bytes;
        p.homes.push_back({WeakVH(a), a->getName().str(), bytes, a->getAlign().value()});
      }
    }
  }
  if (StringRef(argv[2]) == "O2") {
    LoopAnalysisManager lam; FunctionAnalysisManager fam; CGSCCAnalysisManager cam; ModuleAnalysisManager mam;
    PassBuilder pb(machine.get()); pb.registerModuleAnalyses(mam); pb.registerCGSCCAnalyses(cam); pb.registerFunctionAnalyses(fam); pb.registerLoopAnalyses(lam); pb.crossRegisterProxies(lam, fam, cam, mam);
    auto pipeline = pb.buildPerModuleDefaultPipeline(OptimizationLevel::O2); pipeline.run(*module, mam);
  } else if (StringRef(argv[2]) != "normal") return 1;
  SmallString<256> optimizedPath(argv[3]); sys::path::replace_extension(optimizedPath, ".optimized.ll");
  std::error_code irError; raw_fd_ostream optimized(optimizedPath, irError); if (irError) return 1;
  module->print(optimized, nullptr); optimized.close();
  SmallVector<char, 0> buffer; raw_svector_ostream output(buffer);
  auto *tm = static_cast<LLVMTargetMachine *>(machine.get());
  legacy::PassManager pm; auto *mmi = new MachineModuleInfoWrapperPass(tm); auto *config = tm->createPassConfig(pm);
  config->setDisableVerify(false);
  config->insertPass(&FinalizeISelID, new Early(s));
  config->insertPass(&PrologEpilogCodeInserterID, new Audit(s, 0));
  pm.add(config); pm.add(mmi);
  if (config->addISelPasses()) return 1;
  config->addMachinePasses(); config->setInitialized();
  pm.add(new Audit(s, 1));
  if (s.fault == "frame" || s.fault == "sp") pm.add(new Fault(s));
  if (tm->addAsmPrinter(pm, output, nullptr, CodeGenFileType::ObjectFile, mmi->getMMI().getContext())) return 1;
  pm.add(new Audit(s, 2)); pm.add(createFreeMachineFunctionPass());
  pm.run(*module);
  if (s.fault == "encoded") {
    // This Linux x64 fixture changes the low immediate byte of the real large
    // SUB rsp encoding after all MachineFunction checks. It is never executed.
    bool changed = false;
    for (size_t i = 0; i + 7 <= buffer.size(); ++i)
      if (uint8_t(buffer[i]) == 0x48 && uint8_t(buffer[i+1]) == 0x81 && uint8_t(buffer[i+2]) == 0xec) {
        buffer[i+3] ^= 0x10; changed = true; break;
      }
    if (!changed) fail(s, "fault", "encoded fault fixture could not be applied");
  }
  checkObject(s, StringRef(buffer.data(), buffer.size()), *machine, *target);
  json::Object functions, errors;
  for (auto &[name, report] : s.reports) functions[name] = std::move(report);
  for (auto &[name, failures] : s.errors) { json::Array a; for (auto &v : failures) a.push_back(v); errors[name] = std::move(a); }
  auto digest = SHA256::hash(ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size()));
  json::Object result{{"llvm", LLVM_VERSION_STRING}, {"triple", s.triple}, {"mode", argv[2]},
    {"cpu", "generic"}, {"explicit_features", ""}, {"relocation", "PIC"},
    {"machine_optimization", "Default/O2"}, {"data_layout", module->getDataLayoutStr()}, {"budget", 65536},
    {"certified", s.errors.empty()}, {"boundary", "after addMachinePasses, before AsmPrinter; checked after AsmPrinter and MC encoding"},
    {"private_object_sha256", toHex(digest)}, {"private_object_bytes", int64_t(buffer.size())},
    {"functions", std::move(functions)}, {"errors", std::move(errors)}};
  std::error_code ec; raw_fd_ostream report(argv[3], ec); if (ec) return 1;
  report << formatv("{0:2}", json::Value(std::move(result))) << '\n'; report.close(); if (report.has_error()) return 1;
  if (!s.errors.empty()) return 2; // No object is delivered, even if assembly succeeded.
  SmallString<256> temporary; int descriptor;
  ec = sys::fs::createUniqueFile(std::string(argv[4]) + ".private-%%%%%%", descriptor, temporary);
  if (ec) return 1;
  raw_fd_ostream delivered(descriptor, true);
  delivered.write(buffer.data(), buffer.size()); delivered.close();
  if (delivered.has_error()) { sys::fs::remove(temporary); return 1; }
  ec = sys::fs::rename(temporary, argv[4]);
  if (ec) { sys::fs::remove(temporary); return 1; }
  return 0;
}
