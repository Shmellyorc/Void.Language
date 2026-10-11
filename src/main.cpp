#include "void/backend.hpp"
#include <iostream>
int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string(argv[1]) == "--emit-proof") {
            vd::emit_object(vd::ir::architecture_proof(), argv[2], true);
            std::cout << "Wrote native architecture proof object\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--jit-proof") {
            vd::Jit jit;
            auto code = jit.compile(vd::ir::architecture_proof(), "entry");
            auto result = code->invoke(0);
            std::cout << result << '\n';
            return result == 42 ? 0 : 1;
        }
        std::cerr << "Usage: voidc --emit-proof <object> | --jit-proof\n"
                     "Architecture harness: .void source compilation is not implemented yet.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "VOID compiler development error: " << error.what() << '\n';
        return 1;
    }
}
