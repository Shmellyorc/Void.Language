# First real source compiler

Implemented locally on 10 October 2026, using LLVM 19.1.7. LANGUAGE_BIBLE.md
remains the language-design source of truth; this document describes the working
subset and explicitly provisional choices. Multi-file compilation is now implemented;
see [MODULE_SYSTEM.md](MODULE_SYSTEM.md) for current import and ownership rules.
Earlier foundation-only status in
historical documents predates this phase.

## Working pipeline

`SourceFile` validates UTF-8 and retains original byte offsets. The indentation
lexer emits tokens, newlines and block boundaries. The recursive-descent parser
builds an AST per file. The module graph resolves all function signatures first, then
semantic analysis checks
lexical bindings and exact types, annotates expressions and determines possible
control-flow exits. Only validated ASTs lower into the shared VOID IR. The IR
validator checks its independent invariants before LLVM lowering.

The LLVM backend compiles that same IR for both ORC JIT and TargetMachine object
emission. There is no architecture-example recognizer or second source code
generator. Changing a source literal or function name changes both native outputs.
Compiler responsibilities stop at code generation; the existing experimental
heap, reload leases and C bridge remain separate runtime/host responsibilities.

## Syntax and supported semantics

```swift
func sum(left: i32, right: i32) -> i32:
    return left + right

func entry(value: i32) -> i32:
    var total: i32
    var index = 0
    for index < value:
        index++
        if index % 2 == 0:
            continue
        total += sum(index, 1)
    for:
        if total >= 20:
            break
        total += 3
    return total
```

- Functions use `func name(parameter: type, ...) -> type:`. Module-level
  `private`, `public` and `internal` modifiers are supported; see MODULE_SYSTEM.md. Parameters require
  types. Omit the return annotation for no result. Empty `return` or fallthrough
  is valid only for a no-result function. Recursive and forward calls work.
- Types are `i32` and `bool`. Literals are decimal integers, `true`, and `false`.
  Literal values must fit i32, including `-2147483648`. No implicit bool/integer
  conversion occurs. Calls check positional argument counts and exact types.
- `var x = expression`, `var x: i32 = expression`, and `var x: i32` work.
  Annotated variables without an initializer default to zero/false. Parameters
  and locals are mutable. Bindings are visible after declaration and only within
  their lexical scope. Shadowing visible locals, parameters or function names is
  rejected; sibling scopes can reuse names.
- Assignment `=`, arithmetic compounds `+= -= *= /= %=`, and standalone postfix
  `++`/`--` work. Increment/decrement cannot be used as expression values.
- Arithmetic uses unary `+`/`-`, `+ - * / %`; comparisons use `< <= > >= == !=`.
  Equality accepts matching i32 or bool operands; ordering requires i32.
- Boolean operators are `not`, `and`, `or`. `and`/`or` short-circuit at runtime.
  Arguments and binary operands evaluate left to right. From weakest to strongest:
  `or`, `and`, `not`, comparisons, addition/subtraction, multiplication/division/
  remainder, unary arithmetic. Parentheses override precedence. Comparisons do
  not implement chained-comparison semantics; write `a < b and b < c`.
- `if condition:`, `else if condition:`, and `else:` nest normally. Conditions
  require bool. Every reachable end of a value-returning function needs a return.
  Analysis is conservative for conditional branches and conditional loops;
  unbounded `for:` and literal `for true:` need no return after the loop unless a
  reachable break can exit it. Unreachable statements still receive semantic
  checks, and are omitted from lowering.
- All loops use `for`: conditional `for condition:` or unbounded `for:`.
  `continue` reevaluates the nearest loop condition; `break` exits the nearest
  loop. Collection/range iteration is rejected until its types exist.
- Function calls can be statements, including no-result calls. Other standalone
  value expressions are rejected.

## Source and diagnostics

Files must be valid UTF-8; a leading UTF-8 BOM is accepted. LF, CRLF and CR line
endings work, as does a missing final newline. Comments start with `#` (including
`##` documentation-style lines), and may contain Unicode. Indentation uses
spaces with consistent enclosing levels; four spaces are recommended, rather
than imposed. Tabs are diagnosed. Blank/comment lines do not open blocks.
Expressions and declarations are single-line in this subset.

Names currently accept lowercase ASCII letters, digits after the first character,
and underscores. This does not settle future Unicode identifiers or the full
language naming policy. Unsupported characters and types are diagnosed.
Locations are one-based file/line/Unicode-scalar column positions, with the actual
source line and a caret. Scalar columns are not terminal display widths for wide
or combining characters. Diagnostics stop after the first error; ranges, error
codes and recovery are future work.

For example, `while value > 0:` produces a VOID diagnostic at that keyword:

```text
example.void:2:5: error: VOID uses 'for' for all loops. Hint: Replace 'while condition:' with 'for condition:'.
    while value > 0:
    ^
```

Invalid indentation/tokens, unresolved names, duplicate declarations, argument
counts, type mismatches, invalid returns and missing return paths are rejected
before LLVM. Malformed UTF-8 is rejected even in comments.

## Provisional numeric behavior

This slice makes inherited i32 proof arithmetic explicit rather than leaving LLVM
undefined behavior exposed: addition, subtraction, multiplication and negation
wrap modulo 2^32. Signed division truncates toward zero, with remainder having
the dividend's sign. Division is widened to i64 before truncating back to i32;
`-2147483648 / -1` therefore wraps to `-2147483648` and its remainder is zero.
A directly literal zero divisor is a source error. Dynamic zero divisors trap
through `llvm.trap`; a structured VOID runtime diagnostic is not implemented.
This is a provisional implementation contract, not a final decision for every
numeric type, checked arithmetic or release/debug overflow policy. Resolve that
broader policy before extending numeric types. No Language Bible rule was changed
to accommodate backend convenience.

## CLI and native executable boundary

```sh
voidc check program.void
voidc run program.void [decimal-i32-input] [--no-opt]
voidc emit program.void -o program.o [--no-opt]
```

`check` needs no entry function. `run` and `emit` currently require an
`entry(i32) -> i32` adapter, with any parameter name. This is the CLI host ABI,
not a restriction on language function signatures. Input defaults to zero.
O2 is the default; `--no-opt` selects unoptimized LLVM lowering. `emit` produces a
host-native object; a matching Clang driver links it (see README). Its native
`main` accepts the same optional input, prints the result and exits with status
zero; malformed/out-of-range input or extra arguments exit with status 2.

Source symbols encode their declaring module and function identity before the C
adapter is added, allowing same-named functions in different modules and helpers
named `main`, `printf` or `strtoll` without native adapter collisions. Stable
module identity is implemented; public library/export ABI remains deferred. Generated line
tables refer to the real input file. Variable debug records are not implemented.

## Validation and next phase

The existing six architectural suites remain, with `frontend` and `source_cli`
added to the same test executable/build. Frontend checks run three real files
and thirteen independent programs through normal/O2 ORC, plus over fifty negative
and boundary cases. Integration tests check, emit, link, verify DWARF and execute
three actual files with normal/O2 JIT and AOT, using paths with spaces. A modified
architecture source changes `42` to `13` and renames its helper to `main`; both
engines then return 23 for input 10. Tests retain the native `printf` proof.
See WORK_LOG.md for actual configuration commands and results.

The user confirmed this original eight-suite frontend phase passed Linux,
Windows and macOS Actions. The next module phase adds two suites and is documented
in MODULE_SYSTEM.md and WORK_LOG.md. Its hosted matrix remains pending.

Next: strengthen const/project-root constants, numeric policy and source-first
runtime errors around the multi-file compiler, defining constant dependency
cycles before initialization. Managed types must wait for the existing
GC/root/thread integration questions; structs, collections/ranges, generics and
async remain outside these phases.
