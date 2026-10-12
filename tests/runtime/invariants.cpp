#include "void/runtime.hpp"
#include <cstdlib>
#include <exception>
#include <string>
int main(int argc, char** argv) {
    std::set_terminate([] { std::_Exit(79); });
    if (argc != 2) return 2;
    vd::Runtime runtime;
    vd::Runtime::Attachment a(runtime); vd::Runtime::Managed m(runtime);
    int foreign = 0; void* bad = &foreign; constexpr std::size_t zero = 0;
    vd::Runtime::Slot slot{&bad,sizeof(bad),&zero,1};
    if (std::string(argv[1]) == "corrupt-root") {
        vd::Runtime::Frame frame(runtime,&slot,1);
        runtime.collect();
    } else if (std::string(argv[1]) == "frame-order") {
        auto* first = new vd::Runtime::Frame(runtime,&slot,1);
        vd::Runtime::Frame second(runtime,&slot,1);
        delete first;
    } else return 2;
    return 3; // Invariant failures must never resume Managed code.
}
