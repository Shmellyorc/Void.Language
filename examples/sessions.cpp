#include "void/tooling.hpp"
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    try {
        if (argc!=2) throw std::invalid_argument("Supply the examples/analysis directory");
        const std::filesystem::path root(argv[1]);
        vd::ToolingSession session(root/"main.void");
        auto analyze=[&] {
            auto result=vd::ToolingSession::analyze(session.select());
            if (!session.accept(result)) throw std::runtime_error("Fresh result rejected");
            return result;
        };
        auto first=analyze();
        if (!first->result().succeeded()) throw vd::CompilationError(first->result().diagnostics);
        auto symbol=session.current_index()->declaration("/worker.void::calculate");
        std::cout << "revision " << first->snapshot().revision() << ": " << symbol->identity
                  << "; references " << session.current_index()->references_to(symbol->identity).size() << '\n';
        auto worker=vd::read_source(root/"worker.void").text();
        worker.replace(worker.find("stable + 1"),10,"stable + 9");
        session.open("/worker.void",1,worker);
        if (session.current_index()) throw std::runtime_error("Stale query accepted");
        std::cout << "unsaved edit: previous analysis stale; full validation required\n";
        auto body=analyze();
        if (!body->result().succeeded() || session.invalidation().source_compatibility!=vd::SourceCompatibility::exact_match)
            throw std::runtime_error("Body compatibility");
        symbol=session.current_index()->declaration(symbol->identity);
        std::cout << "fresh query: " << symbol->identity << "; body exact match; affected dependencies:";
        for (const auto& id:session.invalidation().dependent_modules) std::cout << ' ' << id;
        std::cout << '\n';
        session.replace("/worker.void",2,"func calculate(x: i32) -> i32:\n    return missing\n");
        auto invalid=analyze();
        if (invalid->result().succeeded()) throw std::runtime_error("Invalid source accepted");
        std::cout << "revision " << invalid->snapshot().revision() << " diagnostics:\n"
                  << vd::CompilationError(invalid->result().diagnostics).what() << '\n';
        session.replace("/worker.void",3,worker);
        if (!analyze()->result().succeeded() || !session.current_index()) throw std::runtime_error("Repair failed");
        std::cout << "repair: fresh successful analysis\n";
        session.open("/values.void",1,"const base: i32 = 41\n");
        if (!analyze()->result().succeeded() || session.invalidation().source_compatibility!=vd::SourceCompatibility::restart_required)
            throw std::runtime_error("Constant compatibility");
        std::cout << "constant edit: restart required; no native publication\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
