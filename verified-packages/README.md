# VOID source packages

Each library has its own `void.project.toml`, package ID, version and
public entry. Use `voidc add` for application dependencies and
`voidc package install` to register a library for reuse across projects.
Git dependencies record an exact commit and package directory in the
consumer's manifest and lock.

[raylib](raylib/README.md) documents how to add a bundled library,
inspect its source and use the same package layout for a new library.
Its preparation tool is a separate compiler resource. Binding
generators and verification records live in `tools/raylib/` in the
VOID source checkout.
