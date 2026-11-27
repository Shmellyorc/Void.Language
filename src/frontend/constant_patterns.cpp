#include "constant_evaluation.hpp"
#include "stack_guard.hpp"

namespace vd::frontend {

VOID_SEMANTIC_HANDLER bool CompileEvaluation::matches(const SourceFile& source,const Pattern& pattern,
                                                       const ir::ConstantValue& subject) {
    Nesting nesting(*this,source,pattern.token);
    step(source,pattern.token);
    using K=Pattern::Kind;
    switch(pattern.kind) {
    case K::wildcard:
        return true;
    case K::capture:
        charge(source,pattern.token,subject);
        locals->at(pattern.slot)=subject;
        return true;
    case K::value: {
        const auto& expression=*pattern.values.at(0);
        if(expression.kind==Expr::Kind::sum && expression.parts.empty())
            return subject.fields.at(0).bits==expression.slot;
        auto value=constant_value(source,expression,this);
        return constant_pattern_equal(source,pattern.token,pattern.type,subject,value,*this);
    }
    case K::range: {
        const auto first=constant_value(source,*pattern.values.at(0),this).bits;
        const auto last=constant_value(source,*pattern.values.at(1),this).bits;
        const auto compare=[&](ir::Op op,std::uint64_t lhs,std::uint64_t rhs) {
            return numeric::binary(op,lhs,rhs,pattern.type)!=0;
        };
        if(compare(ir::Op::less_equal,first,last))
            return compare(ir::Op::greater_equal,subject.bits,first) &&
                compare(pattern.exclusive?ir::Op::less:ir::Op::less_equal,subject.bits,last);
        return compare(ir::Op::less_equal,subject.bits,first) &&
            compare(pattern.exclusive?ir::Op::greater:ir::Op::greater_equal,subject.bits,last);
    }
    case K::list: {
        const auto fixed=pattern.children.size()-(pattern.rest?1:0);
        const auto count=subject.repeat.value_or(subject.fields.size());
        if(pattern.rest?count<fixed:count!=fixed)return false;
        for(std::size_t n=0;n<fixed;++n)
            if(!matches(source,pattern.children[n],subject.fields.at(subject.repeat?0:n)))return false;
        if(!pattern.rest || pattern.children.back().kind==K::wildcard)return true;
        ir::ConstantValue tail;
        for(std::size_t n=fixed;n<count;++n) {
            step(source,pattern.token);
            const auto& item=subject.fields.at(subject.repeat?0:n);
            charge(source,pattern.token,item);
            tail.fields.push_back(item);
        }
        return matches(source,pattern.children.back(),tail);
    }
    case K::dictionary:
        for(std::size_t n=0;n<pattern.children.size();++n) {
            auto key=constant_value(source,*pattern.values[n],this);
            const auto* value=constant_dictionary_find(source,pattern.token,pattern.type,subject,key,this);
            if(!value || !matches(source,pattern.children[n],*value))return false;
        }
        return true;
    case K::constructor:
        if(pattern.nullable)
            return subject.fields.at(0).bits!=0 && matches(source,pattern.children.at(0),subject.fields.at(1));
        if(pattern.type.definition && !pattern.type.definition->cases.empty() &&
           subject.fields.at(0).bits!=pattern.tag)return false;
        if(pattern.rest)return matches(source,pattern.children.at(0),subject);
        for(std::size_t n=0;n<pattern.children.size();++n)
            if(!matches(source,pattern.children[n],subject.fields.at(pattern.indexes.at(n))))return false;
        return true;
    }
    throw std::logic_error("Invalid compile-time pattern");
}

VOID_SEMANTIC_HANDLER CompileEvaluation::Outcome CompileEvaluation::match_statement(
    const SourceFile& source,const Statement& statement) {
    const auto subject=constant_value(source,*statement.expression,this);
    for(const auto& arm:statement.body)
        if(matches(source,*arm.pattern,subject))return sequence(source,arm.body);
    return {};
}

VOID_SEMANTIC_HANDLER ir::ConstantValue CompileEvaluation::match_expression(const SourceFile& source,const Expr& expression) {
    std::vector<std::optional<ir::ConstantValue>> frame;
    struct Restore {
        CompileEvaluation& state;
        decltype(locals) previous;
        ~Restore(){state.locals=previous;}
    } restore{*this,locals};
    if(expression.constant_pattern_frame) {
        charge_bytes(source,expression.token,expression.constant_pattern_frame*sizeof(frame.front()));
        frame.resize(expression.constant_pattern_frame);
        locals=&frame;
    }
    const auto subject=constant_value(source,*expression.parts.at(0),this);
    if(expression.kind==Expr::Kind::test)return {matches(source,expression.patterns.at(0),subject),{}};
    for(std::size_t n=0;n<expression.patterns.size();++n)
        if(matches(source,expression.patterns[n],subject))return constant_value(source,*expression.parts.at(n+1),this);
    error(source,expression.token,Code::invalid_value,"No match expression arm selected.");
}

}
