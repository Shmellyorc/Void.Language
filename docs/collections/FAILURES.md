# Original failures and corrections

Every directory below is a separate original attempt. Receipts retain exact
commands, status, stdout/stderr, source/import bytes and hashes, available build
configuration and tool/generator provenance. The recorder's historical directory
name `crashes` does **not** classify every entry as a compiler crash. These 17
attempts exited normally with status 1: source diagnostics, independent IR
rejections or native test assertions. No native compiler crash, sanitizer finding
or timeout occurred in these attempts. Expected invalidation/negative diagnostics
are passing negative tests and are not included as incidents.

| Original evidence | Finding and concrete correction |
|---|---|
| [vu6mics2](evidence/crashes/incident-vu6mics2/receipt.json) | Enum equality was unsupported and an unaliased import exposed names that test locals shadowed. Use `is` for the enum and an explicit module alias. No language relaxation. |
| [4_be_qbt](evidence/crashes/incident-4_be_qbt/receipt.json) | A valid fresh-object map callback falsely required mutable input authority. The shared callable-result provenance helper now respects readonly formal leaves when deriving mutable results. Fresh and independently mutable outputs execute; input-alias laundering still rejects. |
| [jpbx0ari](evidence/crashes/incident-jpbx0ari/receipt.json) | A test tried unsupported collection identity equality. The corrected test observes mutation/independence instead. Source specialization now diagnoses unsupported collection equality before independent IR, with list/dictionary/fixed negatives. |
| [x_8b433c](evidence/crashes/incident-x_8b433c/receipt.json), [y85t0wuv](evidence/crashes/incident-y85t0wuv/receipt.json) | Empty and nonempty dictionary return branches had different inferred authority profiles. Fresh empty return literals receive their inferred return profile; existing storage and nonempty values keep invariant checking. The second attempt added expected/actual profile diagnostics to identify the mismatch. |
| [2_fxzre7](evidence/crashes/incident-2_fxzre7/receipt.json) | A fixture used unsupported `\1` string syntax. Replaced with supported text while retaining the embedded-NUL comparison. |
| [mf7fg8wc](evidence/crashes/incident-mf7fg8wc/receipt.json) | Stage 10 intentionally required the generic definition to import the operator owner. Boss separately approved the narrow [generic operator amendment](GENERIC_OPERATORS.md). This is an authorized contract change, not a retrospective defect claim. |
| [sme98iu7](evidence/crashes/incident-sme98iu7/receipt.json) | A large-workload test compared an i32 query with an i64 bound. The query now has the required exact type. |
| [nev_06jt](evidence/crashes/incident-nev_06jt/receipt.json) | The cache test omitted its project directory, so the expected cache miss could not occur. Create the disposable project directory; retain miss/hit/state assertions. |
| [woklfvtc](evidence/crashes/incident-woklfvtc/receipt.json), [0dmqik69](evidence/crashes/incident-0dmqik69/receipt.json), [gnaun5p7](evidence/crashes/incident-gnaun5p7/receipt.json) | Changing the callback factory body also changed an eager global initializer's dependency contract. Saved original/changed contracts establish that restart rejection was correct. The compatible-reload fixture now initializes its stored closure explicitly at runtime; incompatible initializer, layout, capture and instance-set tests still require restart. |
| [kbiww20w](evidence/crashes/incident-kbiww20w/receipt.json) | The first generated benchmark imported console without an alias and shadowed its exported `output`. Corrected the import, then recorded a separate successful benchmark. |
| [0fytljf2](evidence/crashes/incident-0fytljf2/receipt.json), [i32ngwr3](evidence/crashes/incident-i32ngwr3/receipt.json), [peado_o2](evidence/crashes/incident-peado_o2/receipt.json), [2be_rjlz](evidence/crashes/incident-2be_rjlz/receipt.json) | Debug/Release/sanitizer adjacent generic suites rejected existing generic methods on generic receivers; the diagnostic repeat identified the canonical-name mismatch. Independent IR now derives a method's identity from its declared method and verified concrete receiver, and derives construction suffixes from formal parameters. The existing generic suite passes after repair; new missing/mismatched/foreign receiver-proof cases reject. |

A passing retry alone establishes none of these explanations. The source changes,
specific diagnostics/contracts and focused regressions provide the causal evidence;
original failures remain intact. Historical H1–H7 incidents and other compiler
health evidence remain separate, unresolved and outside this repair claim.

The shared recorder control deliberately induces crashes, timeout, sanitizer text
and assertion failures in disposable temporary evidence. It verifies thirteen
preserved incidents, including immutable launch-time bytes, imported companions,
unsaved editor sources and a passing retry that does not erase its predecessor.
An interim control assertion expected the former companion-collision count;
the control now checks the exact expanded count. This was a Python test assertion,
not an actual VOID compiler crash. Build-command rejections and the deliberately
stopped duplicate build remain in local build logs; neither is an execution pass.
