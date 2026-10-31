# Native memory accounting and remaining proof boundary

Admission is a 512 MiB reservation ledger per Runtime, not a total-process RSS
ceiling. Source application retention, LLVM/JIT, allocator arenas, sanitizer
quarantine, OS socket buffers and native trust services are measured separately.
The contract has not been weakened to equate three bounded rings with bounded
application or provider memory.

| Storage | Implemented bound / charge |
| --- | --- |
| Runtime driver | One driver thread, 8 client slots, 16 transfer slots, 16 connection reservations; fixed 4,096-descriptor aggregate poll workspace |
| Each transfer | Three heap-owned 64 KiB rings; one pending 64 KiB write; 16 MiB base provider/transfer reservation, twice the owned request snapshot |
| Buffered result | Twice the positive configured response limit before admission; after detachment, charge reduces to 2 MiB plus twice actual retained capacity until publication releases it |
| Each client | 12 MiB per allowed connection plus 8 MiB metadata; idle/connection/stream caps checked before opening another transfer |
| Explicit TLS inputs | Bounded regular-file snapshots, combined at most 1 MiB per client; input bytes plus a provisional `64 × material_bytes × max_connections` parser charge |
| Headers/trailers | 64 KiB/128 fields per header block, 256 KiB/512 fields overall, 16 interim blocks; separate 16 KiB/32-field trailer budget; curl HTTP/1 additionally limits each trailer field to less than 4 KiB |
| Cookies | Provider-owned opt-in session, 1,024 cookies/1 MiB serialized census; check after each mutation before another admission/hop; clear on overflow; enumeration OOM is checked |
| Request graph | At most 64 MiB, 128 MIME parts; bounded configuration/no-proxy strings; checked preflight before encoded forms or snapshots grow |
| DNS | Configured TTL plus clearing the multi cache after eight configured origin/proxy hops; live lookups retain only their own references; curl's c-ares adapter copies at most 256 addresses per family |

Pinned curl uses 10 MiB HTTP/2 connection/maximum stream windows, 16 KiB chunks
and a finite spare pool. Its original duplicate window update violated the
admitted envelope. The exact approved correction is in authoritative source.
Three paused streams plus four small resume cycles now plateau at 52,270,438
live requested bytes under strict instrumentation, within 71,303,366 reserved
bytes. Original failure: 102,577,468 live bytes. Peer window-credit traces are
retained in [the exact dependency review](HTTP2_WINDOW_REVIEW.md).

The unmodified AWS-LC source defaults to a 100 KiB handshake-message allowance
when accepting peer certificate chains and TLS 1.3 client session tickets. Its
path that does not accept peer chains uses a 16 KiB default. Its ASN.1 template decoder
limits constructed depth to 30 and nested string decoding to 5. The selected
PKCS8 PBES2 implementation supports PBKDF2, with a finite 100,000,000 iteration
limit; it is not a memory-hard KDF. Curl's session cache has 25 peers and at most
two sessions per peer. nghttp2 bounds an individual header name/value at 65,536
bytes, defaults its dynamic table to 4,096 bytes and limits continuation frames
to eight. Source locations and hashes are recorded in
`evidence/http-memory-source-audit.json`. These are component facts, not a proof
that every parser allocation fits the provisional coefficient above.

The actual source async loop passes 2 GiB in normal/O2 JIT and independent
normal/O2 async AOT. The final post-correction run records 258 forced-GC samples and retains
212,924–219,068 bytes of managed payload, with Task/group/Handle maxima 11/2/11. Retained tasks remain
repeatably awaitable. Native bulk independently passes a 2 GiB download and
4 GiB+1 upload, independently counted and hashed by the peer; Release steady RSS
is 10,580–11,200 KiB in the final post-correction receipt. These proofs cover different ownership paths.

The near-1 MiB corpus contains distinct signed roots, not repeated PEM text
that the provider can simply deduplicate. Four independent clients perform
256 fresh verified handshakes, with negative expired/future/wrong-host,
oversized-flight, untrusted and pre-TLS-1.2 peers. Debug observed a 45,420,544-byte
peak and 45,412,352–45,420,544 steady RSS against 356,340,600 reserved bytes.
The original strict run reached identical 4,569,189-byte live samples at rounds
16 and 32, then hit its 10-second connect deadline under instrumentation. Its
receipt is preserved. The retention workload now explicitly requests 30 seconds
per connect; public defaults and sanitizer settings are unchanged. The completed
[strict receipt](evidence/http-tls-corpus-explicit-deadline-strict/commands.json)
passes all 256 handshakes and negative peers in 853.85 seconds. Live requested
allocations plateau at 4,568,514–4,577,208 bytes, sampled peak 30,430,885 bytes,
against the same 356,340,600-byte client reservation. That run's checkpoint
reads every 16 rounds are 4,569,214 bytes. The final integrated
strict repeat passes in 757.63 seconds with 4,568,497–4,569,197 live bytes and
a 30,430,868-byte sampled peak ([receipt](evidence/http-tls-corpus-final-strict/commands.json)).
The final Debug repeat passes in 347.41 seconds, with 42,635,264–45,293,568
steady RSS and a 45,293,568-byte sampled peak
([receipt](evidence/http-tls-corpus-final-debug/commands.json)).
Strict ASan/UBSan/leak checks remain enabled.

**Remaining safety review:** the explicit-material parsing coefficient is
conservative and measured, but lacks a complete malformed-ASN.1 allocation proof.
Distribution root-file/directory growth and Windows/macOS native trust/store
allocation are host-dependent and are not covered by an enforced total heap
counter. The default/system trust path cannot currently be certified as a fixed
512 MiB worst-case process allocation. Full provider-memory acceptance remains
open. No process-wide allocator replacement, provider fork, trust disablement or
contract relaxation is introduced to turn this into a passing claim.
