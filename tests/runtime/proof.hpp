#pragma once
#include "void/ir.hpp"
namespace runtime_test {
inline vd::ir::Module proof(bool changed_layout = false) {
    using namespace vd::ir;
    auto leaf = std::make_shared<ValueType>(ValueType{"runtime.Leaf","Leaf",{{"payload",Type::i32}}});
    if (changed_layout) leaf->fields.push_back({"extra",Type::i64});
    Type reference(leaf,Type::managed_ref);
    auto inner = std::make_shared<ValueType>(ValueType{"runtime.Inner","Inner",{{"reference",reference},{"flag",Type::boolean}}});
    Type nested(inner);
    auto outer = std::make_shared<ValueType>(ValueType{"runtime.Outer","Outer",{{"tag",Type::i32},{"nested",nested}}});
    Type value(outer), object(outer,Type::managed_ref);
    Module m; m.identity = "runtime_proof"; m.coordinated = true;
    Function make; make.name = "make"; make.result = reference;
    make.blocks = {{{{Op::gc_allocate,0,0,0,{}, {},reference}, {Op::argument}, {Op::gc_write,0,1}, {Op::gc_collect}}, {Terminator::Kind::return_value,0}}};
    Function pack; pack.name = "pack"; pack.parameters = {reference}; pack.result = value;
    pack.blocks = {{{{Op::argument}, {Op::constant,0,0,1,{}, {},Type::boolean},
        {Op::aggregate,0,0,0,{}, {},nested,{0,1}}, {Op::constant,0,0,7},
        {Op::aggregate,0,0,0,{}, {},value,{3,2}}, {Op::gc_collect}}, {Terminator::Kind::return_value,4}}};
    Function entry; entry.name = "entry"; entry.locals = {object,Type::i32,reference};
    entry.blocks = {
        {{{Op::argument}, {Op::call,0,0,0,"make",{},reference,{0}},
          {Op::call,0,0,0,"make",{},reference,{0}}, {Op::store,2,2},
          {Op::call,0,0,0,"pack",{},value,{1}}, {Op::gc_collect},
          {Op::gc_allocate,0,0,0,{}, {},object}, {Op::extract,4,0,1},
          {Op::gc_write,6,7,1}, {Op::store,0,6}, {Op::constant,0,0,20}, {Op::store,1,10}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,1}, {Op::constant,0,0,0}, {Op::greater,0,1}}, {Terminator::Kind::branch,2,2,3}},
        {{{Op::gc_collect}, {Op::load,1}, {Op::constant,0,0,1}, {Op::subtract,1,2}, {Op::store,1,3}}, {Terminator::Kind::jump,0,1}},
        {{{Op::load,0}, {Op::gc_read,0,0,1}, {Op::extract,1,0,0}, {Op::gc_read,2},
          {Op::load,2}, {Op::gc_read,4}, {Op::add,3,5}, {Op::constant,0,0,7}, {Op::add,6,7}}, {Terminator::Kind::return_value,8}}
    };
    Function failure; failure.name = "failure";
    failure.blocks = {{{{Op::argument}, {Op::call,0,0,0,"make",{},reference,{0}}, {Op::gc_collect},
        {Op::constant,0,0,1}, {Op::constant,0,0,0}, {Op::divide,3,4}}, {Terminator::Kind::return_value,5}}};
    m.functions = {make,pack,entry,failure};
    Function mutate; mutate.name = "mutate"; mutate.parameters = {value,Type::i32}; mutate.result = Type::none;
    mutate.receiver = true; mutate.receiver_mutates = true; mutate.locals = {value};
    mutate.blocks = {{{{Op::argument}, {Op::argument,1}, {Op::gc_allocate,0,0,0,{}, {},reference}, {Op::gc_write,2,1},
        {Op::constant,0,0,1,{}, {},Type::boolean}, {Op::aggregate,0,0,0,{}, {},nested,{2,4}},
        {Op::insert,0,5,1}, {Op::store,0,6}, {Op::gc_collect}}, {Terminator::Kind::return_void}}};
    auto container_definition = std::make_shared<ValueType>(ValueType{"runtime.Container","Container",{{"value",value},{"padding",Type::i32}}});
    Type container(container_definition);
    Function receiver; receiver.name = "entry_receiver"; receiver.locals = {container,value};
    receiver.blocks = {{{{Op::argument}, {Op::call,0,0,0,"make",{},reference,{0}}, {Op::constant,0,0,1}, {Op::add,0,2},
        {Op::call,0,0,0,"make",{},reference,{3}}, {Op::call,0,0,0,"pack",{},value,{1}},
        {Op::constant,0,0,0}, {Op::aggregate,0,0,0,{}, {},container,{5,6}}, {Op::store,0,7}, {Op::store,1,5},
        {Op::call,0,0,0,"mutate",{},Type::none,{3},Place{0,{0}}}, {Op::gc_collect},
        {Op::load,0}, {Op::extract,12,0,0}, {Op::extract,13,0,1}, {Op::extract,14,0,0}, {Op::gc_alive,15}}, {Terminator::Kind::branch,16,1,2}},
        {{{Op::load,0}, {Op::extract,0,0,0}, {Op::extract,1,0,1}, {Op::extract,2,0,0}, {Op::gc_read,3},
          {Op::load,1}, {Op::extract,5,0,1}, {Op::extract,6,0,0}, {Op::gc_read,7}, {Op::add,4,8}}, {Terminator::Kind::return_value,9}},
        {{{Op::constant,0,0,-999}}, {Terminator::Kind::return_value,0}}};
    m.functions.push_back(mutate); m.functions.push_back(receiver);
    return m;
}
}
