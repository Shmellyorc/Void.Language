# Write asynchronous work with clear ownership

An async function produces a task. `await` waits for its result while permitting
the owner to run other cooperative work. Start independent tasks before awaiting
them when you want work to overlap.

```void
async func ticket_total(ticket_count: i32) -> i32:
    return ticket_count * 12

async func entry(ticket_count: i32) -> i32:
    var pending_total = ticket_total(ticket_count)
    var booking_fee = 3
    var tickets = await pending_total
    return tickets + booking_fee
```

For input 2, this computes `27`. `pending_total` is a task handle; `tickets` is the
integer result after the await. This small example explains the control flow;
it does not demonstrate parallel CPU execution.

## Check and execute the example

Save as `async.void` and use `voidc check async.void`. Ordinary `voidc run` and
`voidc build` do **not** supply an async root host. An application's normal `main`
is synchronous. Async libraries require the explicit embedding/root adapter
described in the [async contract](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ASYNC.md#explicit-root-adapter).

For contributors with a matching test-enabled source build, the maintained
learning checker executes this exact example through that adapter:

```sh
python3 -B tests/cli/learning.py build/llvm23-release/voidc build/llvm23-release
```

It needs the matching `void_async_tests` target and native resources. That target
is an engineering fixture, not a command shipped in the installed user toolchain.
The checker also runs the other learning examples; use the canonical embedding
guide when building your own host.

## Own the work

Use structured groups to bound child lifetimes. Group exit joins admitted work;
there are no detached source tasks. Runtime failure and cancellation can cause
draining, while a typed `Err` is an ordinary successful task result. Handle it
using [result patterns](https://github.com/Shmellyorc/Void.Language/wiki/Errors-and-nullable-values), not by assuming it cancels siblings.

Cancellation waits for entered native work and descendants before resources are
retired. A completion signal alone does not make a running frame safe to destroy.
Source workers use restricted transfer rules and cannot access runtime globals,
even through helpers. Shared managed identity is not blanket cross-thread access.

Concrete generic async functions are supported. Async interface dispatch, async
callable values, async value receivers and async destructors remain unavailable.
For a source that advances asynchronously, see the worked
[async iterator](https://github.com/Shmellyorc/Void.Language/wiki/Iteration#await-the-next-item).

Next: [Conditional compilation](https://github.com/Shmellyorc/Void.Language/wiki/Conditional-compilation) or
[Standard library](https://github.com/Shmellyorc/Void.Language/wiki/Standard-library).
Precise scheduler, group, worker and cleanup rules: [Async/Tasks](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ASYNC.md).
