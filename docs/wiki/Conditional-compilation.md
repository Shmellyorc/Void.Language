# Select code for the current configuration

Use conditional compilation when a declaration or import should exist only for a
particular target or project feature. Use an ordinary `if` for a runtime decision. Predicates use
[Boolean values](https://github.com/Shmellyorc/Void.Language/wiki/Types) and the supported target/feature names.

```void
compile if target_os == "linux":
    const host_name = "linux"
compile else if target_os == "windows":
    const host_name = "windows"
compile else:
    const host_name = "other"

func main():
    print(host_name)
```

On Linux it prints `linux`. Selection happens before ordinary declarations and
imports enter the program. The target comes from the active native backend;
this is not a switch for arbitrary cross compilation.

## Add a project feature

In the selected project's `void.project.toml`, add:

```toml
[build.conditions]
tracing = true
```

Then use this complete application source:

```void
compile if feature.tracing:
    func report():
        print("tracing enabled")

func main():
    compile if feature.tracing:
        report()
    print("ready")
```

With tracing enabled it prints `tracing enabled` then `ready`. Set it to `false`
and check/run again: only `ready` remains. Use [manifest project commands](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules),
such as `voidc run --project .`, for feature predicates. A standalone explicit
file has target predicates, but no owning-project feature map.

Unknown features and invalid directives diagnose. Packages use their own feature
maps, not the consumer's. Excluded source does not remove a declared dependency's
lock or native approval requirements. Changing conditions requires recompilation
and restart; it does not hot-swap an application's initialized state.

Compile-time function declarations/execution (`compile func`) are planned, not
implemented. Conditional source selection does not imply such a facility.

Next: [Native compilation](https://github.com/Shmellyorc/Void.Language/wiki/Native-compilation).
Precise predicates and selection: [Conditional compilation](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#conditional-compilation),
[CLI configuration](https://github.com/Shmellyorc/Void.Language/blob/main/docs/CLI.md).
