# Understand async execution

_Draft for Boss approval; repository documentation is canonical._

Async functions produce tasks; `await` suspends the current async activation while
waiting for a result. This complete example returns 27 for input 2:

```void
async func ticket_total(ticket_count: i32) -> i32:
    return ticket_count * 12

async func entry(ticket_count: i32) -> i32:
    var pending_total = ticket_total(ticket_count)
    var booking_fee = 3
    var tickets = await pending_total
    return tickets + booking_fee
```

Save as `async.void`. `voidc check async.void` validates it. Ordinary `voidc run`
and `voidc build` do not supply the async execution host. To reproduce execution
from a test-enabled engineering build, use the repository's existing async adapter
through the learning-example checker:

```sh
python3 tests/cli/learning.py build/llvm23-release/voidc build/llvm23-release
# PASS includes async input 2 => 27 in normal and optimized AOT modes
```

This command checks the canonical `examples/learn/async.void`, matching the source
above. It requires the matching `void_async_tests` target and native manifests;
there is no claim that it is part of an installed end-user package.
Task groups govern child lifetimes/cancellation. Restricted value-only worker
transfers are a separate facility from cooperative async execution.

Canonical contracts and limits: [async](https://github.com/Shmellyorc/Void.Language/blob/main/docs/ASYNC.md), [task retention](https://github.com/Shmellyorc/Void.Language/blob/main/docs/STAGE8_ASYNC_RETENTION.md), [host integration](https://github.com/Shmellyorc/Void.Language/blob/main/docs/NATIVE_LIBRARIES.md).
