#pragma once
#include <stop_token>
namespace vd {
// Borrowed per invocation and thread: a language-server worker can stop bounded
// constant work without changing immutable analysis or Runtime cancellation.
inline thread_local std::stop_token compile_evaluation_stop;
class CompileEvaluationCancellation {
    std::stop_token previous;
public:
    explicit CompileEvaluationCancellation(std::stop_token token):previous(compile_evaluation_stop){compile_evaluation_stop=token;}
    ~CompileEvaluationCancellation(){compile_evaluation_stop=previous;}
    CompileEvaluationCancellation(const CompileEvaluationCancellation&)=delete;
    CompileEvaluationCancellation& operator=(const CompileEvaluationCancellation&)=delete;
};
}
