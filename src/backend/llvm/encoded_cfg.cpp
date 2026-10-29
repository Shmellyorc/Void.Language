// Linux x64 encoded-CFG experiment. No delivery, origin or ABI ownership credit.
#include "encoded_cfg.hpp"
#include <llvm/BinaryFormat/ELF.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInst.h>
#include <llvm/MC/MCInstrAnalysis.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/SymbolSize.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SHA256.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Target/TargetMachine.h>
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
using namespace llvm;
namespace vd::llvm_backend {
namespace {
void require(bool v, const std::string &why) { if (!v) throw std::runtime_error(why); }
template<class T> T take(Expected<T> value, const char *why) {
  if (!value) throw std::runtime_error(std::string(why)+": "+toString(value.takeError()));
  return std::move(*value);
}
std::string text(const json::Value &v) { return formatv("{0}",v).str(); }
struct Node {
  MCInst inst; uint64_t offset, size; std::vector<size_t> next;
  int64_t delta = 0; std::string kind, callee, label; int64_t depth = -1;
};
struct Graph { std::vector<std::string> labels; std::vector<std::set<size_t>> next; std::set<size_t> roots; };
// Epsilon closure stops at the next observable event. An event-free cycle is
// unsupported, not silently collapsed into a terminating path.
std::set<size_t> closure(size_t at, const std::vector<std::vector<size_t>> &next,
                         const std::map<size_t,size_t> &events) {
  std::set<size_t> result;std::map<size_t,unsigned> color;
  std::vector<std::pair<size_t,size_t>> work{{at,0}};
  while(!work.empty()) {
    auto &[node,child]=work.back();
    if(auto event=events.find(node);event!=events.end()){result.insert(event->second);work.pop_back();continue;}
    color[node]=1;
    if(child==next.at(node).size()){color[node]=2;work.pop_back();continue;}
    auto successor=next[node][child++];
    require(color[successor]!=1,"unsupported event-free CFG cycle");
    if(color[successor]!=2)work.emplace_back(successor,0);
  }
  return result;
}
std::string label(StringRef kind, int64_t depth, int64_t delta=0, StringRef callee={}) {
  return kind.str()+":"+std::to_string(depth)+(kind=="sp"?":"+std::to_string(delta):kind=="call"?":"+callee.str():"");
}
Graph eventGraph(const std::vector<std::vector<size_t>> &edges,
                 const std::vector<std::string> &labels) {
  Graph g;std::map<size_t,size_t> events;
  for(size_t n=0;n<labels.size();++n) if(!labels[n].empty()) {events[n]=g.labels.size();g.labels.push_back(labels[n]);}
  g.next.resize(g.labels.size());
  g.roots=closure(0,edges,events);
  for(auto [n,event]:events)for(auto successor:edges[n]) {
    auto found=closure(successor,edges,events);g.next[event].insert(found.begin(),found.end());
  }
  return g;
}
json::Array indices(const std::set<size_t> &v) {json::Array a;for(auto n:v)a.push_back(int64_t(n));return a;}
json::Object graphJSON(const Graph &g) {
  json::Array events;
  for(size_t n=0;n<g.labels.size();++n)events.push_back(json::Object{{"label",g.labels[n]},{"next",indices(g.next[n])}});
  return json::Object{{"entry",indices(g.roots)},{"events",std::move(events)}};
}
Graph machineGraph(const json::Object &observation) {
  require(observation.getBoolean("complete_machine_observation").value_or(false),"incomplete machine observation");
  auto *blocks=observation.getArray("blocks");require(blocks && !blocks->empty() && blocks->size()<=4096,"missing/oversized machine blocks");
  std::map<int64_t,const json::Object*> ordered; std::map<int64_t,size_t> first;
  for(const auto &v:*blocks) {auto *b=v.getAsObject();require(b && b->getInteger("layout") && b->getInteger("block"),"missing machine layout identity");
    require(ordered.emplace(*b->getInteger("layout"),b).second,"duplicate machine layout");}
  std::vector<std::vector<size_t>> edges;std::vector<std::string> labels;
  for(auto [layout,b]:ordered) {
    require(layout==int64_t(first.size()),"incomplete machine layout");
    auto *events=b->getArray("ordered_events");require(events,"missing ordered machine events");
    require(first.emplace(*b->getInteger("block"),labels.size()).second,"duplicate machine block");
    if(events->empty()) {labels.push_back("");edges.emplace_back();}
    else for(const auto &v:*events) {
      auto *e=v.getAsObject();require(e && e->getString("kind") && e->getInteger("depth"),"missing machine event");
      auto kind=*e->getString("kind");require(kind=="sp" || kind=="call" || kind=="return","unknown machine event");
      if(kind=="sp")require(e->getInteger("delta").has_value(),"missing machine SP delta");
      if(kind=="call")require(e->getString("callee").has_value() && !e->getString("callee")->empty(),"missing machine callee");
      labels.push_back(label(kind,*e->getInteger("depth"),e->getInteger("delta").value_or(0),e->getString("callee").value_or("")));
      edges.emplace_back();
      require(labels.size()<=4096,"machine event resource bound exceeded");
    }
  }
  for(auto [layout,b]:ordered) {
    auto begin=first.at(*b->getInteger("block"));auto *events=b->getArray("ordered_events");
    auto end=begin+std::max<size_t>(1,events->size());
    for(size_t n=begin;n+1<end;++n)edges[n].push_back(n+1);
    auto *successors=b->getArray("successors");require(successors,"missing machine successors");
    for(const auto &s:*successors){auto id=s.getAsInteger();require(id && first.count(*id),"missing machine successor");edges[end-1].push_back(first.at(*id));}
  }
  return eventGraph(edges,labels);
}
}
std::string inspect_research_cfg(StringRef bytes, StringRef function, StringRef certificate,
                                StringRef profile, TargetMachine &tm, const Target &target) {
  json::Object result{{"scope","research-only-Linux-x64-encoded-CFG"},{"supported",false},
    {"delivery_credit",false},{"abi_ownership_proved",false},{"physical_memory_bound_proved",false},
    {"call_continuation_contract","ordinary Linux x64 C call returns to call-site SP; ABI ledger deferred"},
    {"profile",profile.str()},{"function",function.str()},
    {"object_sha256",toHex(SHA256::hash(arrayRefFromStringRef(bytes)))}};
  json::Array instructions;
  try {
    require(StringRef(LLVM_VERSION_STRING)=="23.1.4","LLVM version mismatch");
    Triple triple(tm.getTargetTriple());require(triple.isOSLinux() && triple.getArch()==Triple::x86_64,"unsupported target profile");
    auto parsed=object::ObjectFile::createObjectFile(MemoryBufferRef(bytes,"research-object"));
    if(!parsed)throw std::runtime_error(toString(parsed.takeError()));
    auto &object=**parsed;auto *elf=dyn_cast<object::ELFObjectFileBase>(&object);
    require(elf && object.getBytesInAddress()==8 && object.isLittleEndian() && elf->getEMachine()==ELF::EM_X86_64 &&
      (elf->getEType()==ELF::ET_REL || elf->getEType()==ELF::ET_EXEC || elf->getEType()==ELF::ET_DYN),"unsupported ELF object/profile");
    require(bytes.size()>ELF::EI_NIDENT &&
      (uint8_t(bytes[ELF::EI_OSABI])==ELF::ELFOSABI_NONE || uint8_t(bytes[ELF::EI_OSABI])==ELF::ELFOSABI_LINUX) &&
      uint8_t(bytes[ELF::EI_ABIVERSION])==0 && elf->getPlatformFlags()==0,"unsupported ELF ABI/flags");
    std::map<uint64_t,std::string> functions;
    struct FunctionRange {uint64_t section,begin,size;std::string name;};std::vector<FunctionRange> ranges;
    std::optional<object::SymbolRef> selected;uint64_t extent=0;
    for(auto &[symbol,size]:object::computeSymbolSizes(object)) {
      auto name=take(symbol.getName(),"symbol name");auto type=take(symbol.getType(),"symbol type");auto address=take(symbol.getAddress(),"symbol address");auto section=take(symbol.getSection(),"symbol section");
      if(type!=object::SymbolRef::ST_Function || section==object.section_end())continue;
      if(elf->getEType()!=ELF::ET_REL)require(functions.emplace(address,name.str()).second,"ambiguous function address/alias");
      ranges.push_back({section->getIndex(),address,size,name.str()});
      if(name==function){require(!selected,"ambiguous selected function");selected=symbol;extent=size;}
    }
    require(bool(selected) && extent && extent<=1048576,"missing/oversized function extent");
    auto section=take(selected->getSection(),"selected section");auto address=take(selected->getAddress(),"selected address");
    auto &sec=*section;
    require(address<=std::numeric_limits<uint64_t>::max()-extent,"overflowing function address");
    for(const auto &r:ranges)if(r.section==sec.getIndex() && r.name!=function && r.size) {
      require(r.begin<=std::numeric_limits<uint64_t>::max()-r.size,"overflowing symbol extent");
      require(std::max(r.begin,address)>=std::min(r.begin+r.size,address+extent),"overlapping function extents");
    }
    require(sec.isText(),"function outside executable section");
    auto content=take(sec.getContents(),"function bytes");
    require(address>=sec.getAddress(),"invalid function start");uint64_t begin=address-sec.getAddress();
    require(begin<=content.size() && extent<=content.size()-begin,"truncated function extent");
    ArrayRef<uint8_t> code(reinterpret_cast<const uint8_t*>(content.data()+begin),extent);
    struct Reloc {uint64_t type;std::string symbol;int64_t addend;};std::map<uint64_t,Reloc> relocations;
    for(auto relocSection:object.sections()) {
      auto relocated=take(relocSection.getRelocatedSection(),"relocation section");
      for(auto relocation:relocSection.relocations()) {
        bool applies=elf->getEType()==ELF::ET_REL ? (relocated!=object.section_end() && (*relocated).getIndex()==sec.getIndex()) : true;
        auto offset=relocation.getOffset();if(!applies)continue;
        uint64_t origin=elf->getEType()==ELF::ET_REL?begin:address;
        if(elf->getEType()!=ELF::ET_REL && (offset<sec.getAddress() || offset-sec.getAddress()>=content.size()))continue;
        require(relocation.getType()==ELF::R_X86_64_PLT32 || relocation.getType()==ELF::R_X86_64_PC32,"unsupported relocation type in selected code section");
        if(offset<origin){require(origin-offset>=4,"relocation overlaps function start");continue;}
        if(offset-origin>=extent)continue;
        require(extent-(offset-origin)>=4,"relocation overlaps function end");
        auto symbol=relocation.getSymbol();require(symbol!=object.symbol_end(),"relocation without symbol");
        auto name=take(symbol->getName(),"relocation symbol");auto addend=take(object::ELFRelocationRef(relocation).getAddend(),"relocation addend");
        require(!name.empty(),"empty relocation symbol");
        require(relocations.emplace(offset-origin,Reloc{relocation.getType(),name.str(),addend}).second,"overlapping relocations");
      }
    }
    json::Array relocReport;
    for(const auto &[offset,r]:relocations)relocReport.push_back(json::Object{{"offset",int64_t(offset)},{"type",int64_t(r.type)},{"symbol",r.symbol},{"addend",r.addend}});
    result["relocations"]=std::move(relocReport);result["function_extent"]=int64_t(extent);
    MCContext context(triple,tm.getMCAsmInfo(),tm.getMCRegisterInfo(),tm.getMCSubtargetInfo());
    std::unique_ptr<MCDisassembler> dis(target.createMCDisassembler(tm.getMCSubtargetInfo(),context));
    std::unique_ptr<MCInstrAnalysis> analysis(target.createMCInstrAnalysis(tm.getMCInstrInfo()));
    require(bool(dis)&&bool(analysis),"MC decoder/analysis unavailable");
    const auto &ii=*tm.getMCInstrInfo();const auto &ri=tm.getMCRegisterInfo();
    std::vector<Node> nodes;std::map<uint64_t,size_t> boundaries;std::set<uint64_t> usedRelocations;
    const std::set<std::string> ordinary={"MOV32ri","MOV64rr","MOV32rr","LEA64r","TEST32rr","XOR32rr",
      "ADD64rr","CMP32ri8","CMP32ri","CMP32rr","ADD32ri8","ADD32ri","INC32r","DEC32r","MOV64mr","MOV64rm",
      "MOV32mr","MOV32rm","MOV64mi32","NOOP","NOOPL","NOOPW"};
    for(uint64_t offset=0;offset<extent;) {
      require(nodes.size()<4096,"instruction resource bound exceeded");Node n;n.offset=offset;
      require(dis->getInstruction(n.inst,n.size,code.slice(offset),address+offset,nulls())==MCDisassembler::Success && n.size && n.size<=extent-offset,"undecodable/truncated instruction");
      auto op=ii.getName(n.inst.getOpcode());auto &desc=ii.get(n.inst.getOpcode());
      bool writes=false;for(auto reg:desc.implicit_defs())if(StringRef(ri.getName(reg))=="RSP" || StringRef(ri.getName(reg))=="ESP")writes=true;
      for(unsigned k=0;k<desc.getNumDefs()&&k<n.inst.getNumOperands();++k)if(n.inst.getOperand(k).isReg()) {
        auto name=StringRef(ri.getName(n.inst.getOperand(k).getReg()));writes|=name=="RSP" || name=="ESP";
      }
      if(op=="CALL64pcrel32") {
        n.kind="call";auto r=relocations.find(offset+1);
        if(elf->getEType()==ELF::ET_REL) {
          require(n.size==5 && r!=relocations.end() && (r->second.type==ELF::R_X86_64_PLT32 || r->second.type==ELF::R_X86_64_PC32) && r->second.addend==-4,"unsupported/unresolved direct-call relocation");
          n.callee=r->second.symbol;usedRelocations.insert(r->first);
        } else {
          uint64_t callee;require(analysis->evaluateBranch(n.inst,address+offset,n.size,callee) && functions.count(callee),"unknown linked call/thunk target");n.callee=functions.at(callee);
        }
      } else if(op=="RET64")n.kind="return";
      else if(op=="JCC_1" || op=="JCC_4" || op=="JMP_1" || op=="JMP_4") {
        uint64_t targetAddress;require(analysis->evaluateBranch(n.inst,address+offset,n.size,targetAddress) && targetAddress>=address && targetAddress-address<extent,"unknown/out-of-function direct branch");
        n.next.push_back(size_t(targetAddress-address)); // Convert after all boundaries are known.
      } else if(writes) {
        n.kind="sp";
        if(op=="SUB64ri8" || op=="SUB64ri32" || op=="ADD64ri8" || op=="ADD64ri32") {
          require(n.inst.getNumOperands()==3 && n.inst.getOperand(0).isReg() && n.inst.getOperand(1).isReg() &&
            StringRef(ri.getName(n.inst.getOperand(0).getReg()))=="RSP" && n.inst.getOperand(0).getReg()==n.inst.getOperand(1).getReg() && n.inst.getOperand(2).isImm() && n.inst.getOperand(2).getImm()>=0,"unknown SP arithmetic");
          n.delta=n.inst.getOperand(2).getImm()*(op.starts_with("SUB")?1:-1);
        } else if(op=="PUSH64r" || op=="PUSH64rmm" || op=="PUSH64i8" || op=="PUSH64i32")n.delta=8;
        else if(op=="POP64r" && n.inst.getNumOperands()==1 && n.inst.getOperand(0).isReg() && StringRef(ri.getName(n.inst.getOperand(0).getReg()))!="RSP")n.delta=-8;
        else throw std::runtime_error("unknown SP writer: "+op.str());
      } else require(ordinary.count(op.str()) && !desc.isBranch() && !desc.isCall() && !desc.isReturn(),"unsupported instruction: "+op.str());
      // A relocation may not hide in an immediate, branch, or a partial opcode.
      for(auto r=relocations.lower_bound(offset);r!=relocations.end()&&r->first<offset+n.size;++r)
        require(usedRelocations.count(r->first),"unsupported relocation affecting instruction");
      boundaries[offset]=nodes.size();nodes.push_back(std::move(n));offset+=nodes.back().size;
    }
    require(usedRelocations.size()==relocations.size(),"uncovered relocations");
    for(size_t k=0;k<nodes.size();++k) {
      auto &n=nodes[k];auto op=ii.getName(n.inst.getOpcode());
      for(auto &to:n.next){require(boundaries.count(to),"branch into middle of instruction");to=boundaries.at(to);}
      if(n.kind!="return" && op!="JMP_1" && op!="JMP_4") {
        require(k+1<nodes.size(),"missing fallthrough/continuation");n.next.push_back(k+1);
      }
    }
    std::vector<size_t> work{0};nodes[0].depth=0;int64_t peak=0;
    while(!work.empty()) {
      auto k=work.back();work.pop_back();auto &n=nodes[k];int64_t after=n.depth;
      require(!(n.delta>0 && after>std::numeric_limits<int64_t>::max()-n.delta) && !(n.delta<0 && after< -n.delta),"negative/overflowing stack depth");
      after+=n.delta;peak=std::max(peak,after);
      if(n.kind=="call")require(after%16==8,"misaligned call-site SP");
      if(n.kind=="return")require(after==0,"unbalanced return");
      if(!n.kind.empty())n.label=label(n.kind,after,n.delta,n.callee);
      for(auto to:n.next) {
        if(nodes[to].depth<0){nodes[to].depth=after;work.push_back(to);}
        else require(nodes[to].depth==after,"inconsistent join/growing stack cycle");
      }
    }
    std::vector<std::vector<size_t>> edges;std::vector<std::string> labels;
    for(const auto &n:nodes) {
      require(n.depth>=0,"incomplete reachable code coverage");edges.push_back(n.next);labels.push_back(n.label);
      json::Array next;for(auto to:n.next)next.push_back(int64_t(nodes[to].offset));
      instructions.push_back(json::Object{{"offset",int64_t(n.offset)},{"size",int64_t(n.size)},
        {"opcode",ii.getName(n.inst.getOpcode()).str()},{"entry_depth",n.depth},{"event",n.label},{"callee",n.callee},
        {"bytes",toHex(code.slice(n.offset,n.size))},
        {"fallthrough",n.kind!="return" && ii.getName(n.inst.getOpcode())!="JMP_1" && ii.getName(n.inst.getOpcode())!="JMP_4"?json::Value(int64_t(n.offset+n.size)):json::Value(nullptr)},
        {"branch_target",ii.get(n.inst.getOpcode()).isBranch()?json::Value(int64_t(nodes[n.next.front()].offset)):json::Value(nullptr)},
        {"successors",std::move(next)}});
    }
    auto graph=eventGraph(edges,labels);result["event_graph"]=graphJSON(graph);result["peak_entry_sp_depth"]=peak;
    result["machine_comparison"]="not supplied";
    if(!certificate.empty()) {
      auto cert=take(json::parse(certificate),"invalid machine certificate");require(cert.getAsObject(),"invalid machine certificate");auto &c=*cert.getAsObject();
      require(c.getString("profile")==profile,"machine target/optimization profile mismatch");
      auto *functions=c.getObject("functions");auto *f=functions?functions->getObject(function):nullptr;
      auto *observation=f?f->getObject("outgoing_sp_observation"):nullptr;require(observation,"missing machine observation");
      auto machine=machineGraph(*observation);
      require(graph.labels==machine.labels && graph.next==machine.next && graph.roots==machine.roots,"encoded/machine ordered event CFG mismatch");
      require(observation->getInteger("peak_entry_sp_depth")==peak,"encoded/machine peak mismatch");
      result["machine_comparison"]="ordered event CFG and peak agree; origin/ABI unproved";
    }
    result["supported"]=true;
  } catch(const std::exception &e) {result["error"]=std::string(e.what());}
  result["instructions"]=std::move(instructions);return text(std::move(result));
}
}
