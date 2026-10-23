#include "void/backend.hpp"
#include "void/frontend.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks=0;
const std::string entry="func entry(input: i32) -> i32:\n    return input\n";
void check(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);++checks;}
auto accept(const std::string& source){auto a=vd::analyze_source({"/operators.void",source+entry});if(!a.succeeded())throw vd::CompilationError(a.diagnostics);++checks;try{auto m=a.program->lower();vd::ir::validate(m);}catch(const std::exception& e){throw std::runtime_error(std::string(e.what())+"\n"+source);}++checks;return a.program;}
void reject(const std::string& source,const std::string& reason=""){auto a=vd::analyze_source({"/operators.void",source+entry});check(!a.succeeded(),"invalid declaration accepted: "+source);if(!reason.empty()){bool found=false;for(const auto& d:a.diagnostics)found|=d.description.find(reason)!=std::string::npos;check(found,"wrong diagnostic for "+reason+": "+source);}}
const std::string owner="value struct point:\n    private x: i32\n";
std::string declaration(const std::string& glyph,const std::string& operands="const left: point, const right: point",const std::string& result="i32",const std::string& body="        return left.x\n",const std::string& modifier="") {return "    "+modifier+"operator "+glyph+"("+operands+") -> "+result+":\n"+body;}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::invalid_argument("expected repository root");
    std::string all=owner;
    for(const auto& glyph:{"+","-","*","/","%"})all+=declaration(glyph,"const left: point, const right: point","point","        return point(x = left.x "+std::string(glyph)+" right.x)\n");
    for(const auto& glyph:{"+","-"})all+=declaration(glyph,"const left: point","point","        return point(x = "+std::string(glyph)+"left.x)\n");
    for(const auto& glyph:{"==","<",">","<=",">="})all+=declaration(glyph,"const left: point, const right: point","bool","        return left.x "+std::string(glyph)+" right.x\n");
    all+="func (self point) read() -> i32:\n    return self.x\n";
    auto a=accept(all);auto m=a->lower();unsigned operators=0;
    for(const auto& f:m.functions)if(!f.operator_glyph.empty()){
        ++operators;check(f.operator_owner.definition->identity=="/operators.void::point","wrong declaring owner");check(f.lexical_owner==vd::ir::Type::key(f.operator_owner) && !f.receiver,"wrong static lexical owner");check(f.name==vd::ir::operator_identity(f.operator_owner,f.operator_glyph,f.parameters),"unstable operator identity");
        for(auto t:f.parameters)check(t.readonly,"operand lost const");
    }
    check(operators==12,"missing approved family");
    unsigned records=0;for(const auto& d:a->declarations())if(d.kind==vd::AnalyzedDeclaration::Kind::operator_function){++records;check(!d.rename_safe && d.owner=="/operators.void::point","operator declaration metadata");check(!d.written_parameters.empty() && d.source.end>d.source.begin,"missing source operands/span");}
    check(records==12,"missing operator metadata");
    for(bool optimized:{false,true}){vd::Jit jit;check(jit.compile(*a,optimized)->invoke(7)==7,"operator declaration module failed JIT");}
    // Exercise declaration bodies through existing ordinary IR calls. Source
    // glyph selection remains the next increment; this does not emulate it.
    for(const auto& target:m.functions)if(!target.operator_glyph.empty()) {
        auto called=m;called.analysis.reset();auto wrapper=std::find_if(called.functions.begin(),called.functions.end(),[&](const auto& f){return f.name==called.entry_symbol;});
        vd::ir::Instruction input{vd::ir::Op::argument,0},two{vd::ir::Op::constant};two.immediate=2;
        vd::ir::Instruction left{vd::ir::Op::aggregate};left.type=target.operator_owner;left.arguments={0};auto right=left;right.arguments={1};
        vd::ir::Instruction call{vd::ir::Op::call};call.callee=target.name;call.operator_glyph=target.operator_glyph;call.arguments={2};if(target.parameters.size()==2)call.arguments.push_back(3);
        wrapper->locals.clear();wrapper->blocks={{{input,two,left,right,call},{}}};
        int expected=0;const auto& glyph=target.operator_glyph;
        if(target.result==vd::ir::Type::boolean) {
            wrapper->blocks[0].end={vd::ir::Terminator::Kind::branch,4,1,2};
            vd::ir::Instruction yes{vd::ir::Op::constant};yes.immediate=1;vd::ir::Instruction no{vd::ir::Op::constant};
            wrapper->blocks.push_back({{yes},{vd::ir::Terminator::Kind::return_value,0}});wrapper->blocks.push_back({{no},{vd::ir::Terminator::Kind::return_value,0}});
            expected=glyph==">" || glyph==">=";
        }else {
            wrapper->locals={target.operator_owner};wrapper->blocks[0].instructions.push_back({vd::ir::Op::store,0,4});
            vd::ir::Instruction read{vd::ir::Op::call};read.callee="/operators.void::point::method::read";read.receiver=vd::ir::Place{0,{}};wrapper->blocks[0].instructions.push_back(read);wrapper->blocks[0].end={vd::ir::Terminator::Kind::return_value,6};
            expected=target.parameters.size()==1?(glyph=="+"?7:-7):glyph=="+"?9:glyph=="-"?5:glyph=="*"?14:glyph=="/"?3:1;
        }
        vd::ir::validate(called);for(bool optimized:{false,true}){vd::Jit jit;check(jit.compile(called,called.entry_symbol,optimized)->invoke(7)==expected,"ordinary-call operator body result "+glyph);}
    }
    accept(owner+declaration("+","const left: point, const amount: i32")+declaration("+","const amount: i32, const left: point")+declaration("+","const left: point, const amount: i64"));
    accept(owner+declaration("%","const left: point, const right: f64","f64","        return right\n"));
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>?) -> i32:\n        return 0\n","nullable");
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>) -> box<t>?:\n        return left\n","nullable");
    for(const auto& visibility:{"public ","internal ","private "}){
        auto visible=accept(owner+declaration("+","const left: point","i32","        return left.x\n",visibility));
        for(const auto& f:visible->lower().functions)if(!f.operator_glyph.empty())check(f.visibility==std::string(visibility).substr(0,std::string(visibility).size()-1),"visibility lost");
    }
    auto fresh=accept("struct node:\n    private value: i32\n    operator +(const left: node, const right: node) -> node:\n        return node(value = left.value + right.value)\n    operator ==(const left: node?, const right: node?) -> bool:\n        return left is null and right is null\n");
    for(const auto& f:fresh->lower().functions)if(f.operator_glyph=="+")check(!f.result.readonly,"fresh result lost mutable authority");
    accept("struct node:\n    private value: i32\n    operator +(const left: node) -> i32:\n        var read: func() -> i32 = func() -> i32:\n            return left.value\n        return read()\n");
    accept("enum flag:\n    empty\n    full(value: i32)\n    operator ==(const left: flag, const right: flag) -> bool:\n        return true\n");
    accept(owner+declaration("+","const left: point","i32","        var read: func() -> i32 = func() -> i32:\n            return left.x\n        return read()\n"));
    // Unused generic bodies remain deferred; declaration structure/types are checked.
    accept("value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: t) -> box<t>:\n        return unknown_body_name\n");
    auto generic=accept("value struct box<t>:\n    private value: t\n    operator +(const left: box<t>, const right: t) -> box<t>:\n        return box<t>(value = right)\nfunc use_box() -> i32:\n    var object = box<i32>()\n    return 0\n");
    auto generic_ir=generic->lower();check(std::any_of(generic_ir.functions.begin(),generic_ir.functions.end(),[](const auto& f){return !f.operator_glyph.empty();}),"generic operator not specialized");
    reject(owner+declaration("+","left: point, const right: point"),"explicit const");
    reject(owner+declaration("+","const left: i32, const right: i32","i32","        return left\n"),"exact declaring owner");
    reject("struct base:\n    x: i32\nstruct child extends base:\n    operator +(const left: base) -> i32:\n        return left.x\n","exact declaring owner");
    reject(owner+"    operator +(const left: point):\n        pass\n","explicit storable result");
    for(const auto& glyph:{"==","<",">","<=",">="})reject(owner+declaration(glyph),"exactly bool");
    for(const auto& glyph:{"!=","+=","-=","*=","/=","%=","=","and","or","not","is","as","has","++","--"})reject(owner+declaration(glyph),std::string(glyph)=="!="?"derived":"Unsupported operator");
    for(const auto& glyph:{"*","/","%","==","<",">","<=",">="})reject(owner+declaration(glyph,"const left: point"),"arity");
    reject(owner+declaration("+",""),"arity");reject(owner+declaration("+","const left: point, const right: point, const third: point"),"arity");
    for(const auto& modifier:{"static ","async ","virtual ","override ","const ","func "})reject(owner+declaration("+","const left: point","i32","        return left.x\n",modifier));
    reject(owner+declaration("+","const left: point = point()"));
    reject(owner+"    operator +<t>(const left: point) -> i32:\n        return 0\n");
    reject(owner+declaration("+","const left: point, const left: point"),"Duplicate");
    reject(owner+declaration("+","const left: point")+declaration("+","const renamed: point","bool","        return true\n","private "),"Duplicate operator");
    reject(owner+declaration("+","const left: point","i32","        left = point()\n        return 0\n"),"readonly");
    reject(owner+declaration("+","const left: point","i32","        left.x = 2\n        return 0\n"),"readonly");
    auto alias=accept("struct node:\n    value: i32\n    operator +(const left: node) -> node:\n        return left\n");
    for(const auto& f:alias->lower().functions)if(!f.operator_glyph.empty())check(f.result.readonly,"readonly operand return gained mutable authority");
    reject("struct node:\n    values: [i32]\n    operator +(const left: node) -> i32:\n        left.values.push(2)\n        return 0\n","readonly");
    reject("value struct box<t>:\n    value: t\n    operator +(const left: t) -> t:\n        return left\n","matching instantiated owner");
    reject("value struct box<t, u>:\n    value: t\n    operator +(const left: box<u, t>) -> i32:\n        return 0\n","matching instantiated owner");
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: unknown) -> i32:\n        return 0\n","Unsupported type");
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>) -> i32:\n        return 0\n    private operator +(const right: box< t >) -> bool:\n        return true\n","Duplicate generic operator");
    reject("interface callable:\n    operator +(const left: callable) -> i32:\n        return 0\n");
    reject("operator +(const left: i32) -> i32:\n    return left\n");
    accept(owner+declaration("+","const left: point","point","        var copy = left\n        copy.x = 9\n        return copy\n"));
    reject(owner+declaration("+","const left: point")+"func helper(object: point) -> i32:\n    return object.x\n","not accessible");
    reject("struct base:\n    private x: i32\nstruct child extends base:\n    operator +(const left: child) -> i32:\n        return left.x\n","not accessible");
    accept("value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: [t]) -> i32:\n        return 0\n    operator +(const left: box<t>, const right: [t, 2]) -> i32:\n        return 0\n");
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: [t, 2]) -> i32:\n        return 0\n    operator +(const left: box<t>, const renamed: [t, 1 + 1]) -> i32:\n        return 0\n","Duplicate generic operator");
    // Concrete generic tuples that coincide are duplicates even when their
    // distinct symbolic signatures were valid before specialization.
    reject("value struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: t) -> i32:\n        return 0\n    operator +(const left: box<t>, const right: i32) -> i32:\n        return 0\nfunc force() -> i32:\n    var object = box<i32>()\n    return 0\n","Duplicate operator");
    auto formatted=vd::format_source({"/operators.void",all+entry});check(bool(formatted),"operator source formatter rejected valid declarations");auto formatted_analysis=vd::analyze_source({"/operators.void",*formatted});check(formatted_analysis.succeeded(),"formatted operators changed source validity");
    {
        auto path=std::filesystem::path(argv[1])/"tests"/"stage10"/"operator_alias_test.void";
        vd::SourceOverrides overrides;
        overrides.emplace("/foreign.void",vd::SourceFile("/foreign.void","value struct other:\n    x: i32\n"));
        std::string aliases="use \"foreign.void\" as first\nuse \"foreign.void\" as second\nvalue struct box<t>:\n    value: t\n    operator +(const left: box<t>, const right: first.other) -> i32:\n        return 0\n    operator +(const left: box<t>, const right: second.other) -> i32:\n        return 0\n";
        overrides.emplace("/operator_alias_test.void",vd::SourceFile(path.string(),aliases+entry));
        auto duplicate=vd::analyze_project(path,overrides);check(!duplicate.succeeded(),"duplicate aliased concrete operand in unused generic accepted");
        check(std::any_of(duplicate.diagnostics.begin(),duplicate.diagnostics.end(),[](const auto& d){return d.description.find("Duplicate generic operator")!=std::string::npos;}),"wrong aliased tuple diagnostic");
        std::string qualified="use \"operator_alias_test.void\" as own\nvalue struct box<t>:\n    value: t\n    operator +(const left: own.box<t>, const right: t) -> box<t>:\n        return box<t>(value = right)\nfunc force() -> i32:\n    var object = box<i32>()\n    return 0\n";
        overrides.insert_or_assign("/operator_alias_test.void",vd::SourceFile(path.string(),qualified+entry));
        auto owned=vd::analyze_project(path,overrides);if(!owned.succeeded())throw vd::CompilationError(owned.diagnostics);++checks;vd::ir::validate(owned.program->lower());++checks;
    }
    // Declaration metadata must not grant forged lexical field access in hand-built IR.
    for(unsigned variant=0;variant<11;++variant){auto forged=m;auto it=std::find_if(forged.functions.begin(),forged.functions.end(),[](const auto& f){return !f.operator_glyph.empty();});auto& f=*it;
        switch(variant){case 0:f.operator_glyph="!=";break;case 1:f.parameters[0].readonly=false;break;case 2:f.operator_owner=vd::ir::Type::i32;break;case 3:f.visibility="protected";break;case 4:f.receiver=true;break;case 5:f.lexical_owner="foreign";break;case 6:f.operator_glyph.clear();break;case 7:f.parameters.resize(3,vd::ir::Type::i32);break;case 8:f.result=vd::ir::Type::none;break;case 9:forged.nominal_types.clear();break;case 10:f.blocks[0].instructions.push_back({vd::ir::Op::store,0,0});break;}
        bool failed=false;try{vd::ir::validate(forged);}catch(const std::invalid_argument&){failed=true;}check(failed,"forged operator IR accepted variant "+std::to_string(variant));
    }
    {
        auto mutating=accept(owner+declaration("+","const left: point")+"func (self point) mutate():\n    self.x++\n")->lower();
        auto& f=*std::find_if(mutating.functions.begin(),mutating.functions.end(),[](const auto& f){return !f.operator_glyph.empty();});
        vd::ir::Instruction call{vd::ir::Op::call};call.callee="/operators.void::point::method::mutate";call.receiver=vd::ir::Place{0,{}};f.blocks[0].instructions.push_back(call);
        bool failed=false;try{vd::ir::validate(mutating);}catch(const std::invalid_argument& e){failed=std::string(e.what()).find("const operator operand")!=std::string::npos;}check(failed,"mutating receiver bypassed const operator binding");
        auto array=accept(owner+declaration("+","const left: point, const values: [i32, 2]"))->lower();
        for(bool capture:{false,true}){auto forged=array;auto& op=*std::find_if(forged.functions.begin(),forged.functions.end(),[](const auto& f){return !f.operator_glyph.empty();});auto& block=op.blocks[0];
            if(capture){vd::ir::Instruction overwrite{vd::ir::Op::array_capture};overwrite.a=2;overwrite.immediate=1;block.instructions.push_back(overwrite);}
            else {op.fixed_ownership=true;auto index=block.instructions.size();block.instructions.push_back({vd::ir::Op::constant});vd::ir::Instruction select{vd::ir::Op::fixed_select,static_cast<unsigned>(index)};select.receiver=vd::ir::Place{1,{}};block.instructions.push_back(select);}
            bool rejected=false;try{vd::ir::validate(forged);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"array place bypassed const operator binding");
        }
    }
    std::cout<<checks<<" operator declaration/validation checks passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
