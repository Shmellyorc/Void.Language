#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>
#include <chrono>
int main(int argc,char** argv) {
    if(argc!=3)return 1;
    llvm::LLVMContext context;llvm::SMDiagnostic diagnostic;
    auto start=std::chrono::steady_clock::now();
    auto module=llvm::parseIRFile(argv[1],diagnostic,context);
    if(!module){diagnostic.print(argv[0],llvm::errs());return 2;}
    module->setModuleIdentifier(argv[2]);
    if(llvm::verifyModule(*module,&llvm::errs()))return 3;
    llvm::LoopAnalysisManager loops;llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager cgscc;llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder builder;
    builder.registerModuleAnalyses(modules);builder.registerCGSCCAnalyses(cgscc);
    builder.registerFunctionAnalyses(functions);builder.registerLoopAnalyses(loops);
    builder.crossRegisterProxies(loops,functions,cgscc,modules);
    auto pipeline=builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    pipeline.run(*module,modules);
    if(llvm::verifyModule(*module,&llvm::errs()))return 4;
    llvm::outs()<<"Verified standard O2 replay seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<"\n";
    return 0;
}
