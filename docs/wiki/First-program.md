# Print a message

_Draft for Boss approval; repository documentation is canonical._

Create an empty project directory and save `main.void`:

```void
use console

func main() -> i32:
    print("Hello, world!")
    return 0
```

```sh
voidc check .
# Source checked successfully
voidc run .
# Hello, world!
voidc build . -o hello
./hello
# Hello, world!
```

Windows: `-o hello.exe`, then `.\hello.exe`. `print` writes the message; `return 0`
sets the process status. Directory selection finds exact-case `main.void`.
Application entry selection is automatic. Do not pass the removed entry flag.
The compiler can also execute top-level application statements; do not mix
competing entry styles.

Walkthrough and native-flow diagram: [your first program](https://github.com/Shmellyorc/Void.Language/blob/main/docs/FIRST_PROGRAM.md).
