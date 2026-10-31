# HTTP configuration record defaults: exact source decision

The approved HTTP design calls the four configuration records managed records,
while also requiring defaults such as `open_client(config:client_options =
client_options())` and `send(req, options:request_options = request_options())`.
VOID's current pure-default contract rejects managed constructor expressions in
field defaults and ordinary parameter defaults. This is an existing language
restriction, not a network or allocator failure. The local fixture and exact
VOID2011 diagnostic are retained in `evidence/source-defaults/`.

Concrete current candidate: `tls_options`, `proxy_options`, `client_options` and
`request_options` are **value structs** in `stdlib/net/http.void`. Their scalar,
string, enum and list fields keep the approved names/defaults. Pure constructors
are valid defaults. Provider acquisition/submission still makes an owned native
snapshot of every referenced input. Public clients/transfers, requests, response
metadata and file authority retain their managed identity and lifecycle.

The observable difference is ordinary existing value copying: assigning one
configuration record to another copies inline configuration fields. Changing
one copy's scalar/enum/string field does not modify another copy. Referenced
lists still follow existing shared list semantics until the native snapshot.
Changing a configuration never mutates an acquired client's immutable settings.

Recommended decision: approve these four value configuration records. This keeps
all approved call forms and defaults, without extending language default
execution or changing Runtime admission, GC, provider limits or security policy.
The candidate has passed the public synchronous HTTP facade through source
analysis, JIT and linked native execution in both optimization modes; this is not
full HTTP acceptance or a claim that the type adjustment is already approved.

Alternative preserving managed configuration identity: accept nullable optional
configuration parameters with `null` defaults, allocating the effective managed
configuration inside each wrapper. This changes the public parameter types and
requires guarding nested optional configuration consistently. A new language
feature allowing allocation during default evaluation is not proposed.

Boss approved the four value configuration records in chat on 30 October 2026.
The candidate is adopted. No new permission is required for this settled choice.
No provider/security contract is weakened by this source default adjustment.
