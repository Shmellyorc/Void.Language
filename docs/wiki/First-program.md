# Create your first application

With [the tools installed](https://github.com/Shmellyorc/Void.Language/wiki/Installation) and the selected version on your session PATH, create a project in a fresh directory:

```sh
voidc new app --name Hello
cd Hello
```

If you have not exported PATH, use `voidup run default voidc --` in place of
`voidc` in these commands. Installation and project creation use native tools.

This creates `void.project.toml` and `src/app.void`. Replace `src/app.void` with:

```void
use console

func main() -> i32:
    print("Hello, world!")
    return 0
```

`print` writes text. `main` is the application entry, and its returned `i32` becomes
the process status. Zero conventionally means success. The `console` import is
available for richer terminal operations; `print` itself is a builtin.

## Check, run, build

Run these commands inside `Hello`:

```sh
voidc check .
voidc run .
voidc build . -o hello
./hello
```

Both executions print `Hello, world!`. `check` diagnoses without executing the
application. `run` uses the JIT; `build` creates an executable. On Windows use
`voidc build . -o hello.exe`, then `.\hello.exe` in PowerShell.

You can also save this program as `main.void` in a directory without a manifest
and run `voidc run .`. Manifestless directories select that exact filename;
manifest projects discover the application's main from their source inventory.
[Projects and modules](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules) explains that difference.

## Try a change

Change the message, check again and rerun. Then add a variable using
[Language basics](https://github.com/Shmellyorc/Void.Language/wiki/Language-basics), or open the folder with the
[VOID editor extension](https://github.com/Shmellyorc/Void.Language/wiki/Editor-setup).

Next: [Language basics](https://github.com/Shmellyorc/Void.Language/wiki/Language-basics).
Precise entry rules: [Modules and applications](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#modules-and-applications).
