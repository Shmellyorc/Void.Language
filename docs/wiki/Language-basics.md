# Write ordinary code

A VOID program uses indentation to show which statements belong together.
A colon starts a body; indent the statements inside it consistently.

```void
func main():
    var total = 0
    for number in [1, 3, 5, 7, 9]:
        if number > 5:
            break
        total += number
    print(total)
```

Save as `basics.void` and run `voidc run basics.void`. It prints `9`: the loop adds
1, 3 and 5, then stops at 7. `for` is VOID's loop keyword; it also handles a
Boolean condition or an unbounded loop. See [Iteration](https://github.com/Shmellyorc/Void.Language/wiki/Iteration) for custom
sources and [Collections](https://github.com/Shmellyorc/Void.Language/wiki/Collections) for lists and dictionaries.

## Bind a name

Use `var` for storage you will change and `const` for readonly access:

```void
func main():
    const unit_price = 12
    var quantity = 2
    quantity += 1
    print(unit_price * quantity)
```

This prints `36`. Types can be inferred from the initializer; write an annotation
such as `var quantity: i32 = 2` when it improves clarity. Readonly access through
referenced data is explained in [Managed memory](https://github.com/Shmellyorc/Void.Language/wiki/Managed-memory).

## Read the syntax

- `#` starts a comment; `##` introduces source documentation.
- `if`, `else if` and `else` use Boolean conditions.
- `and`, `or` and `not` combine Boolean expressions; `and` and `or` short-circuit.
- `pass` supplies an intentionally empty body.
- Names use lowercase ASCII letters, digits and `_`; they cannot begin with a digit.
- A declaration cannot shadow another visible binding. Choose a new name or assign
  to existing writable storage.

Write an indented `if` body as shown above. Compact `if condition: return` syntax
is deferred. Learn [Functions](https://github.com/Shmellyorc/Void.Language/wiki/Functions) to split a program into named operations
and [Projects and modules](https://github.com/Shmellyorc/Void.Language/wiki/Projects-and-modules) to split it across files.

Next: [Choose types](https://github.com/Shmellyorc/Void.Language/wiki/Types).
Exact grammar and control flow: [Language specification](https://github.com/Shmellyorc/Void.Language/blob/main/docs/LANGUAGE_BIBLE.md#source-syntax-and-names).
