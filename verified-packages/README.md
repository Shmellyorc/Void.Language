# VOID source package collection

These are first-party, independently versioned VOID source libraries. They are
separate from `stdlib`. Each package has its own `void.project.toml`, identity,
version and entry. Fetching source never prepares or executes native code.

Use the existing Git dependency mechanism: HTTPS origin, full lowercase commit
SHA, package `subdir`, explicit `voidc deps fetch`, `voidc resolve`, `void.lock`
and `pkg/<alias>`. There is no registry, second cache or installer. VOIDUP manages
the toolchain; `voidc` manages project dependencies.

[raylib](raylib/README.md) is the first package. Its 600 declarations have usable
interfaces, with direct, ownership, callback and typed-equivalent counts explicit.
Native global-hook concurrency restrictions and unsuppressed device leaks remain
in its qualification report. Source inclusion does not imply public GitHub
post-push qualification or runtime exercise of every native declaration.
