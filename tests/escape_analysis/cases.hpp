#pragma once
// Name, exact result at n=1000, G35 heap constructions, accepted G34 count.
#define VD_ESCAPE_CASES(X) \
    X(branch_alias, 48495, 0, 1000) \
    X(branch_birth, 47662, 0, 500) \
    X(nullable, 46995, 0, 1000) \
    X(references, 47995, 1000, 2000) \
    X(nested, 46995, 2000, 3000) \
    X(field_rebind, 47995, 2000, 3000) \
    X(alias_rebind, 16000, 2000, 2000) \
    X(identity, 1000, 2000, 2000) \
    X(loop_carried, 498500, 1001, 1001) \
    X(helper_return, 46995, 1000, 1000) \
    X(helper_read, 46995, 0, 1000) \
    X(helper_write, 47995, 0, 1000) \
    X(helper_reference, 46995, 1000, 2000) \
    X(public_helper, 46995, 1000, 1000) \
    X(helper_generic, 46995, 0, 1000) \
    X(reference_effects, 15000, 2000, 3000) \
    X(readonly_helper, 46995, 0, 1000) \
    X(payload_budget, 65, 1, 65) \
    X(context_control, 14000, 1000, 1000) \
    X(null_failure, 46995, 1000, 1001) \
    X(numeric_failure, 7000, 0, 1) \
    X(incoming_reference, 46995, 2000, 2000) \
    X(incoming_rebind, 46995, 3000, 3000) \
    X(unknown_factory, 46995, 2000, 2000) \
    X(outlined_array, 2, 0, 0) \
    X(direct_reference, 46995, 1000, 2000) \
    X(direct_helper, 46995, 1000, 2000) \
    X(direct_factory, 46995, 1000, 2000) \
    X(direct_nested, 46995, 2000, 3000) \
    X(direct_identity, 0, 3000, 3000) \
    X(direct_public, 46995, 2000, 2000) \
    X(direct_incoming, 46995, 2000, 2000) \
    X(direct_nullable, 1000, 0, 1000) \
    X(root_retirement, 46995, 1000, 2000) \
    X(child_survives, 140985, 1000, 2000) \
    X(direct_effects, 15000, 2000, 3000) \
    X(helper_branch, 47495, 1000, 2000) \
    X(helper_branch_write, 48495, 0, 1000) \
    X(readonly_field, 46995, 1000, 3000) \
    X(readonly_incoming, 46995, 3000, 3000) \
    X(mutable_field, 46995, 2000, 3000) \
    X(factory_field, 46995, 3000, 3000) \
    X(helper_branch_rebind, 8000, 1500, 1500) \
    X(helper_block_limit, 46995, 1000, 1000) \
    X(direct_null_failure, 7000, 0, 0)
