# Use the standard library in everyday programs

Import the module you need and handle the result its API returns. Start with a
small operation; follow its source documentation when you need exact limits,
resource ownership or error fields.

## Parse input

```void
use convert as convert

func main():
    if convert.parse<i32>("42") is Ok(var amount):
        print(amount + 1)
    else:
        print("enter a whole number")
```

It prints `43`. The explicit type argument selects the numeric parser.
[Generics](https://github.com/Shmellyorc/Void.Language/wiki/Generics-and-interfaces) lets one API express that operation for
multiple numeric types. [Result handling](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values) keeps bad
input out of the successful path.

## Read a file

```void
use io as io

func main():
    if io.read_text("notes.txt") is Ok(var contents):
        print(contents)
    else:
        print("could not read notes.txt")
```

Create a UTF-8 `notes.txt` in the application's working directory and run the
program. It prints the contents. The convenience operation manages its own file
lifetime. For explicit streams, use `io.open` with a typed `io.open_mode` and
follow the [close contract](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory#close-native-resources). Structured
I/O errors retain category, progress and native information.

## Read JSON data

```void
use json as json

func main():
    if json.parse("42") is Ok(var node):
        if node.as_number<i32>() is Ok(var answer):
            print(answer)
```

This prints `42`. Parsing and typed conversion are separately checked operations.
For configuration records, defaults and bounded inputs, use the
[JSON guide](https://github.com/Shmellyorc/Void.Language/blob/main/docs/json/API.md) and its maintained examples.

## Find a module by task

| Task | Modules and authoritative guidance |
| --- | --- |
| Terminal and diagnostics | [console](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/console.void), [log](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/log.void), [error](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/error.void) |
| Text and numbers | [text](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/text.void), [convert](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/convert.void), [math](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/math.void), [random](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/random.void) |
| Byte encodings | [ASCII](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/encoding/ascii.void), [hex](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/encoding/hex.void), [Base64](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/encoding/base64.void), [binary](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/encoding/binary.void) |
| Collections and messages | [collections](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/collections.void), [channel](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/channel.void) |
| Files, memory and host | [io](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/io.void), [io/memory](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/io/memory.void), [path](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/path.void), [os](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/os.void), [time](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/time.void) |
| Web and sockets | [url](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/url.void), [HTTP](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/net/http.void), [address](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/net/address.void), [network errors](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/net/error.void), [TCP](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/net/tcp.void), [UDP](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/net/udp.void) |
| Structured data and host operations | [json](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/json.void), [crypto](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/crypto.void), [archive](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/archive.void), [process](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/process.void), [system_error](https://github.com/Shmellyorc/Void.Language/blob/main/stdlib/system_error.void) |

These are 29 source modules. Lists, dictionaries, strings, `print`, results and
Task syntax are language builtins, rather than extra modules. Follow the
[complete module index](https://github.com/Shmellyorc/Void.Language/blob/main/docs/README.md#standard-library-modules) for task guides.

Modern numeric enums use `.name` for text, `.value` at integer boundaries and
`as?` to check closed membership. For example, `io.open_mode.read` is a typed
opening policy, not an arbitrary integer. Native status values, lengths and byte
data remain numbers. See [Enums and flags](https://github.com/Shmellyorc/Void.Language/wiki/Enums-and-flags).

Next: [Packages and raylib](https://github.com/Shmellyorc/Void.Language/wiki/Packages-and-raylib).
Resource and network availability remains platform-specific; consult the linked
module guide before relying on a particular backend or device.
