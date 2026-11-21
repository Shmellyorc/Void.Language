#include "void/compile_evaluation.hpp"
#include "void/frontend.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
const char* source="compile func spin() -> i32:\n    for:\n        pass\n    return 0\nconst answer = spin()\n";
}
int main() {
    try {
        std::stop_source stop;stop.request_stop();
        {
            vd::CompileEvaluationCancellation cancellation(stop.get_token());
            try {(void)vd::compile_source({"/cancelled.void",source});throw std::runtime_error("Cancelled evaluation accepted");}
            catch(const vd::CompilationError& failure) {
                require(std::string(failure.what()).find("evaluation cancelled")!=std::string::npos,"Missing cancellation diagnostic");
            }
            // Cancellation belongs to this invocation's thread, not the process.
            auto independent=std::async(std::launch::async,[]{return vd::compile_source({"/other.void","compile func square(value: i32) -> i32:\n    return value * value\nconst answer = square(8)\n"});});
            (void)independent.get();
        }
        require(!vd::compile_evaluation_stop.stop_requested(),"Cancellation binding leaked");
        try {(void)vd::compile_source({"/bounded.void",source});throw std::runtime_error("Unbounded evaluation accepted");}
        catch(const vd::CompilationError& failure) {
            require(std::string(failure.what()).find("work budget exceeded")!=std::string::npos,"Cancellation was not restored to ordinary work budget");
        }
        std::cout<<"G93 cancellation scope and concurrent compiler isolation verified\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
