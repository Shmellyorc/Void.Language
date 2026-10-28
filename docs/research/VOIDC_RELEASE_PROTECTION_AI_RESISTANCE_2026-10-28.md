# VOIDC release protection and AI reverse-engineering resistance

Research date: 2026-10-28. Status: research for Golden Wrench review and Boss decisions; no implementation authorized. This is a separate long-term investigation, not a change to VOIDC's development roadmap.

## Assessment

New research offers useful ways to disrupt neural function identification, damage decompiler representations, and increase arithmetic-simplification cost. **The examined evidence does not establish a general, semantics-preserving native compiler defense that withstands modern, adaptive AI analysts using execution, tracing, multiple models and repeated reconstruction attempts.** Some results demonstrate measurable resistance to narrower attacks. Others measure misleading names, classifier errors or expression size rather than concealed behavior.

The realistic objective is to increase the time, monetary cost and specialist effort needed to extract selected application logic, while preserving application correctness and supportability. A defensible claim must name the attacker, task and budget. “AI-proof” is unsupported.

The most useful new mathematical research candidate is equality-saturation-based arithmetic transformation, for future evaluation against both synthesis and LLMs. Architectural-state arithmetic and MIR-level transformations deserve further investigation, but have greater compatibility risks. Neural feature manipulation has direct cross-model evidence for identification tasks; it has no demonstrated guarantee against equivalent program reconstruction. These are research priorities, not approved product features.

## Scope and evidence discipline

This report follows the supplied C++20/LLVM-Clang-LLD 19.1.7, native AOT, precise GC, async and native interoperability constraints. It deliberately does not re-audit or redesign existing VOIDC protection or work in progress. Compatibility assessments below are architectural inferences, not locally tested integration results.

Three independent research tracks examined defenses, AI-assisted attacks, and mathematical foundations. Primary papers, official documentation and selected public artifacts were inspected. No downloaded research program, obfuscator or model was executed; published measurements were not reproduced. No compiler tests, debugger/ptrace sessions or hosted CI were run. Unknown costs are explicitly unknown, rather than assumed negligible. Publication or artifact availability is not proof of correctness or independent replication.

Evidence categories used here:

- **Demonstrated, scoped:** an experiment supports the stated task and tested setting.
- **Author-reported:** a primary report supplies a result, but full inspection or independent reproduction is limited.
- **Architectural inference:** a plausible integration or bypass implication, requiring experiments.
- **Speculative:** a research idea without an established protective result.

## Threat model and observable goals

Assume an analyst possesses the release binary and distributed resources, can run chosen inputs, inspect memory and system interactions, debug or emulate a copy, patch it, compare multiple builds, and combine conventional tools with local or hosted models. Include multiple independent attempts and an informed second round after the protection family is disclosed. Assume ordinary desktop deployment; trusted hardware or a remote secret changes this model and must be evaluated separately.

Measure five distinct outcomes:

| Outcome | What constitutes recovery | What does not establish protection |
| --- | --- | --- |
| Source structure and names | Useful function boundaries, types, data layout, control flow and meaningful identifiers | Different spelling or unattractive pseudocode |
| Algorithm extraction | Correct critical computation, constants and state transitions | Failure to reproduce the original implementation text |
| Equivalent application behavior | An independent implementation passing withheld outputs, errors and stateful sequences | Compilation alone, embedding similarity, matching code coverage alone |
| Asset extraction | Recovering the actual distributed or decoded content | Absence of plaintext in an on-disk string scan |
| Modification and authenticity | Unauthorized behavior change or acceptance of altered distributed content | A checksum which an attacker can patch together with its verifier |

An analyst can learn behavior without recovering source structure. Conversely, a plausible explanation or readable decompilation may be incorrect. Executing one path does not expose every path, and black-box access does not make every function efficiently learnable. None of these distinctions justifies assuming a whole application remains confidential on an analyst-controlled machine.

## What AI-assisted reverse engineering can already do

### Agentic, adaptive and repeated analysis

**SRE-Bench, preprint v2, 1 October 2026.** Nineteen privately authored programs average 16.9K LOC; 262 instances contain 1,572 tasks and 44 protection primitives. Public hardening reduces GPT-5.6-Sol's mean score from 4.69 to 2.50/6; GPT-6-Astra loses 0.10 points but spends 1.5× more per point. Separate uncapped internal runs report Astra 88.0% pass@1 and 99.2% pass@4: **full binary-instance solves, all six tasks passing**, not original-source recovery. The private grader selects successful attempts, leaving practical selection unresolved. Internal settings remove safeguards and budget caps; public settings exclude certain refusals/context failures—Astra has 181/262 instances graded. Tools include decompilers, debuggers and symbolic analysis. This is consequential counterevidence, but remains a preprint with private targets and author-operated evaluation, limiting independent reproduction. [Paper §§4/B/C](https://arxiv.org/html/2608.11469v2), [evaluation access policy](https://sre-bench.lol/evaluate).

**Towards LLM-Resistant Software Protection, BAR, 27 February 2026; experiments December 2025.** On 24 public Linux x86-64 CTF challenges, Claude Code/Opus 4.5 solves 15/24 with a decompiler and 17/24 without; Codex CLI/GPT-5.1-Codex-Max solves 14/24 in both settings. ChatGPT-5.2 Pro solves 19/24; the union solves 21/24, including six of eight custom-VM challenges. Up to three attempts stop at success. These small public tasks have contamination and differing-interface limitations, without a shared time/token cap; there is no controlled evaluation of a newly built defense. Damaging a decompiler therefore cannot be assumed to stop an agent that reads assembly or constructs tools. [Paper, Tables I/IV](https://www.ndss-symposium.org/wp-content/uploads/bar2026-58.pdf).

### Recovery metrics require care

| Primary study | Experiment and measured result | Meaning and limit |
| --- | --- | --- |
| [LLM4Decompile, EMNLP, November 2024](https://aclanthology.org/2024.emnlp-main.203.pdf) | On 164 HumanEval C functions, End-6.7B averages 45.37% re-executability over O0–O3 versus GPT-4o 16.01%; on 2,621 ExeBench functions, 17.98% versus 3.78%. [Public code/models](https://github.com/albertan017/LLM4Decompile). | Tests establish executable recovery on their inputs, not universal equivalence. Small public functions and Linux x86-64 limit extrapolation. |
| [DecompileBench, ACL Findings 2025](https://arxiv.org/html/2505.11340) | 23,400 functions from 130 OSS-Fuzz projects. Average Coverage Equivalence Rate: Hex-Rays 41.8%, GPT-4o refinement 34.6%, DeepSeek-V3 32.9%, LLM4Decompile 16.4%. [Artifact](https://github.com/Jennieett/DecompileBench). | LLM readability improves while functional reliability remains problematic. CER compares coverage behavior on a corpus; it is not output/state equivalence or a proof. |
| [SK²Decompile, September 2025 preprint; ICLR 2026](https://arxiv.org/html/2509.22114v1) | From stripped-binary IDA pseudocode, structure recovery uses placeholder identifiers, then a second learned phase generates names. Training rewards compilation/placeholder recovery. [Conference record](https://proceedings.iclr.cc/paper_files/paper/2026/hash/9132583dd869acb0cafeb0ed25eb031a-Abstract-Conference.html). | An important attack direction: names and structure can be recovered separately. Compiler acceptance alone does not establish correct behavior or whole-application equivalence. |
| [BinDeObfBench, preprint, 9 April 2026](https://arxiv.org/html/2604.08083) | Tests pseudocode deobfuscation with GPT-4o, o1, DeepSeek-R1/V3, Qwen2.5-Coder and task-trained ChatDEOB. | Its semantic score uses learned embeddings/entity overlap because pseudocode is non-executable. Do not treat that score as a recovered-program success rate. |

### Program synthesis is part of the attacker, even without an LLM

**Chisel, OOPSLA, 8 October 2024:** execution traces guide compositional synthesis; it recovers almost-original programs, modulo names, on 86% of 546 source-level benchmarks combining six control-flow transformations. A binary experiment uses IDA plus manual recompilation repairs; 55/60 cases shrink, sampled from programs already solved at source level. It excludes important families including virtualization and MBA, and tests rather than proves whole-program equivalence. The implication is that control-flow complexity must be tested against trace-driven reconstruction, not just a static prompt. [Paper §§6–7](https://bmarwritescode.github.io/assets/pdf/chisel.pdf), [publication/artifact record](https://doi.org/10.1145/3689789).

**XSmir, CCS, 13–17 October 2025:** combines stochastic synthesis with constant, mask, affine and polynomial inference. With 60 seconds per selected expression, approximately 76.3% of obfuscated expressions are recovered versus Xyntia's 63%. Targets include VMProtect, Tigress, OLLVM and Loki; candidates receive SMT equivalence checks using Bitwuzla. This is expression recovery, not unrestricted program reconstruction. Results on separate synthetic benchmarks use different timeouts/size criteria and must not be pooled. More syntax can leave the same compact I/O function available to synthesis. [Paper §§5–6](https://binsec.github.io/assets/publications/papers/2025-ccs.pdf), [reproduction artifact](https://zenodo.org/records/17036259).

## Novel and Experimental AI-Resistance Research

### 1. SaMBA: search the space of equivalent arithmetic, rather than handcraft more identities

**New principle:** equality saturation stores many equivalent expressions in an e-graph; an extraction cost favoring larger expressions chooses complex mixed Boolean-arithmetic forms. Arithmetic is fixed-width modulo 2ⁿ. **Evidence:** AsiaCCS, 1–5 June 2026; seven traditional analysis tools. On 1,000 NeuReduce expressions, Syntia returns 105 results, 39 inequivalent: only 66 are correct. Other simplifiers can retain equivalence while recovering little structural simplicity. SHA-512 over 1 GB grows from 3.63 to 6.04 seconds; reported average runtime overhead is 66.27%, not a VOID application estimate. No LLM/agent evaluation. [Paper §§4.2–4.3](https://eprints.cs.univie.ac.at/8730/1/2026_samba.pdf).

**Protects:** arithmetic structure and solver cost; not proven algorithm secrecy or asset protection. **Integration inference:** a selective integer IR transformation is plausible without another runtime. The inspected artifact contains Rust/egg generation, Z3 bit-vector checks and Tigress benchmarks, not an LLVM pass. Its 32-bit rule checks are insufficient to approve all widths or LLVM poison/overflow semantics. Size, build-time and application-level cost need measurement. [Author artifact](https://github.com/CDL-AsTra/SaMBA/tree/74ccd067f9d0b0b255f1140d0f581f499ed71cb9).

### 2. A²-MBA: architectural state and resistance to learned simplification rules

**New principle:** combine arithmetic transformation with architectural state and anti-generalization hardening, challenging a simplifier's assumptions and learned rewrite reuse. **Evidence:** September 2025 preprint / August 2026 Computers & Security report; abstract reports ProMBA success falling from 97.2% to 8.7%, with up to 45.5% semantically wrong results. Full primary text was inaccessible in this investigation; treat those numbers as author-reported. ProMBA is synthesis/rule learning, not an LLM reverse-engineering agent. [Journal record](https://www.sciencedirect.com/science/article/pii/S0167404826000428), [preprint record](https://papers.ssrn.com/sol3/papers.cfm?abstract_id=5550463).

**Protects:** expression reconstruction against particular simplifiers. An inspected **independent**, non-author LLVM 21 reproduction documents deviations: CoBRA produces source-matching candidates for 16/16 changed expressions in one algebraic export, not a binary-recovery test; protected-to-source equivalence was sampled. Stateful export limits are not resistance proofs. Its synthetic native benchmark reports 18.44× runtime for one configuration, not whole-app cost. These reproduction results neither validate nor refute the original implementation. No demonstrated adaptive LLM/trace resistance; LLVM 19.1.7 and cross-ISA state handling remain unverified. [Reproduction limitations](https://github.com/xqzme69/A2MBA-LLVM/blob/dc8097917abb3dad5f3e73bff1270769b523faa0/docs/PAPER_DEVIATIONS.md), [benchmarks](https://github.com/xqzme69/A2MBA-LLVM/blob/dc8097917abb3dad5f3e73bff1270769b523faa0/docs/BENCHMARKING.md).

### 3. Polaris: attack the machine representation consumed by decompilers

**New principle:** MIR-level transformations perturb function boundaries, control flow and data flow after high-level optimization. **Evidence:** ICSE Demonstrations, 12–18 April 2026. Evaluation uses 100 Csmith-generated programs, despite broader wording in the abstract. Binary Ninja completes 75/100 versus 100/100 baseline; IDA/Ghidra complete all, with damaged pseudocode. At O2, reported runtime is 1.48× and binary size 3.39×; O0 reaches 2.15×/5.95×. Timeouts are excluded, so these are not equal-size full-corpus performance comparisons. No LLM equivalent-behavior or adaptive dynamic evaluation. [Paper, Tables 1–2](https://ttfish.cc/content/Papers/ICSE26-Polaris.pdf).

**Protects:** static segmentation and presentation. Agents can switch representations, so this is a testable cost hypothesis. **Integration/cost:** author artifact requires an LLVM 16.0.6 backend fork and source markers, not a drop-in 19.1.7 pass. Unwind, root-location, ABI and architecture correctness would need separate qualification. Build-time cost is unknown. It is a useful research direction, not a recommendation to replace VOIDC's toolchain. [Inspected source/README](https://github.com/za233/Polaris-Obfuscator).

### 4. Kelpie and asmFooler: target neural representations across models

**New principle:** Kelpie aligns CFG shape and instruction distributions with a different function while preserving the original computation. Backward liveness analysis constrains insertions. This targets the learned representation, not just human readability. **Evidence:** EuroS&P 2026; inspected preprint v2 dated 11 June. Across eight classifiers/retrievers, approximately 10,000 transformed payloads yield mean targeted mimicry of 40% classification and 42% retrieval@10. HermesSim is more robust, at 25%/18%; adversarial retraining partially reduces success. Tested systems include Asm2Vec, jTrans, CLAP, Trex and graph models. These are identification tasks, not LLM reasoning or behavioral clones. [Paper §IV](https://arxiv.org/html/2605.18231v2).

**Protects:** recognition and matching; no asset/behavior secrecy demonstrated. **Integration inference:** machine-level diversification is plausible but current assembly/liveness assumptions need platform validation. Runtime, size and build costs are not established here. The public assembly generator has incomplete usage documentation, rather than a complete reproduction package. [Author source](https://gitlab.inria.fr/gsauger/kelpieasm).

Related **asmFooler, FSE 2026**, tests 9,565 variants from 620 samples against six models. Its public artifact includes an LLVM 19 OLLVM port and ELF64 tooling, which is useful reproducibility evidence, not certification for LLVM **19.1.7**, PE/Mach-O or VOID GC. Transfer varies by model; the best targeted result is not a universal success rate. [Paper](https://arxiv.org/html/2602.12681v1), [inspected artifact package](https://zenodo.org/records/17116512).

### 5. Acoda and poisoned identifiers: AI-specific effects that do not establish native protection

**Acoda, ICSE Companion April 2026; preprint 10 June:** a genetic search combines source transformations aimed at safety alignment and token processing. Seven models, including GPT-4o, DeepSeek, Qwen, Llama and Gemma, show reported attack success up to 70%. Evaluation includes LLM response judgments; refusal is counted as an effect. Python source strings/names and token tricks do not establish a surviving native compiler defense. Model adaptation, normalization and execution can change the outcome. Runtime overhead on source examples cannot be transferred to VOIDC. [Paper](https://arxiv.org/html/2606.11755v1).

**Poisoned Identifiers, preprint, 5 April 2026:** two JavaScript archetypes, primarily Claude Opus 4.6, 192 API runs. Wrong names coexist with correct semantic commentary in 15/17 manually scored outputs. Asking for a fresh implementation reduces propagation to 0–20% on one archetype and 0% on the other. This is direct evidence that naming interference can fail an adaptive reconstruction test. No native integration or semantic secrecy result. [Case study](https://arxiv.org/html/2604.04289v1).

Do not base a product contract on inducing refusal, prompt injection or a particular tokenizer. Those are fragile analyzer behaviors and can damage trustworthy diagnostics without concealing computation.

### 6. NeuroShield: domain-specific restructuring, with an important correctness limit

**New principle:** DNN operator fusion, fake operators and computation reordering disrupt recognizable model structure. **Evidence:** CCS 2025; ACM publication 22 November 2025. Reported operator recovery falls from 98.5% under general obfuscation to 3.03% for vision and 47.18% for NLP; execution overhead is 7.8–36.1%. However, bounded-activation fake paths do not preserve exact behavior for every input: Table 3 reports ESM accuracy 89.31%→85.96%. This is protection of an embedded ML model against specialized model reconstruction, not proof against LLM analysts of arbitrary native programs. [Primary paper, Tables 2–3](https://tonitick.github.io/paper/dnnobfuse_cr_v1.pdf), [publication](https://doi.org/10.1145/3719027.3765144).

The transferable idea is to disturb a domain-specific semantic representation, rather than merely expand syntax. Applying it to exact game logic is speculative; LLVM integration, build/size costs and preservation of arbitrary application behavior are unestablished. This result cannot justify accepting numerical changes in VOID applications.

### 7. Cryptographic obfuscation and hardware-assisted boundaries

Cryptographic obfuscation requires its own security definition. General virtual-black-box obfuscation is impossible for some function families; this does **not** prove every useful specialized protection impossible. Indistinguishability obfuscation instead compares obfuscations of equivalent equal-size circuits; it does not automatically hide all behavior or make a program unlearnable through queries. Practical feasibility and assumptions must be examined separately from the theorem. [Barak et al., CRYPTO 2001 / expanded 2010 paper](https://www.wisdom.weizmann.ac.il/~oded/PS/obf4.pdf), [Jain–Lin–Sahai, conditional iO existence, 2020](https://arxiv.org/abs/2008.09317).

**OBSCURE, TCHES, 12 March 2024:** encrypted multi-instructions execute through a tamper-resistant secure element; rectangular universalization/Beneš networks hide wiring in full mode. Conditional VBB security assumes cryptographic primitives **and trusted hardware**, permitting what oracle access already reveals. Its restricted C subset requires static control flow and fixed alias regions. Estimated AES white-box execution is 0.4–0.5 seconds versus a cited 13 μs native implementation; these are estimates, not measured hardware performance. Full-mode slides estimate 2–20 seconds. Public compiler/runtime/benchmark sources exist, but this changes the execution architecture and trust boundary; it is not an ordinary LLVM pass. No AI attack experiment. Protects specified computation under assumptions, not all observable behavior or rendered assets. [Paper](https://eprint.iacr.org/2024/077), [author estimates](https://affine.group/slides/2024_CHES_Obscure.pdf), [inspected pinned artifact](https://github.com/CryptoExperts/OBSCURE/tree/b494c8b99707e3ff9287edd2b5ba3e40a3cf383a).

**Diamond iO, February 2025 preprint, revised 18 June 2026:** matrix products reduce a functional-encryption-to-iO bottleneck. General-circuit claims use a pseudorandom-oracle model and assumptions including a new all-product LWE assumption. This is substantial mathematical research, not demonstrated practical native protection. [Current paper](https://eprint.iacr.org/2025/236).

Its **28 April 2025** implementation benchmark uses a public circuit, hiding only an embedded key; simplifies encryption/PRF construction and omits the oracle used by the general-functionality security proof. Benchmark 1 uses 32 vCPUs/256 GB RAM, commit `f3b2fdc`: public-circuit multiplicative depth 0 takes 15.80/2.542 minutes for obfuscation/evaluation, 34.30 GB peak RAM and 6.303 GB output. Depth 0 excludes the final decryption multiplication. At depth 10: 129.3/29.94 minutes, 225.1 GB RAM and 19.77 GB output. These are tiny circuits, not game binaries; the early implementation does not validate the later general theorem. [Author benchmark and deviations](https://machina-io.com/posts/hello_world_first).

Artifact status also matters: the original repository was archived in June 2026; the replacement README at the inspected October 2026 commit says functional-encryption/iO implementations were removed, leaving interfaces. No LLVM 19.1.7 integration or AI evaluation is established. Keep this on a research watch list, outside ordinary deployment options. [Archived repository](https://github.com/MachinaIO/diamond-io), [pinned replacement README](https://github.com/MachinaIO/mxx/blob/11004eeb438aa13c57f3c5ec387b1eaa85bb2007/README.md).

Another useful mathematical building block is **normalized multivariate binary polynomials**, BAR 2024: equivalence and generation over integers modulo 2ʷ can support correctness-aware diversification. The same normal forms also help an attacker simplify expressions. There is no LLM-resistance or whole-game overhead result here; an LLVM integer implementation would still need semantic qualification and independent attack testing. [Primary paper](https://www.ndss-symposium.org/wp-content/uploads/bar2024-14-paper.pdf).

### 8. A possible VOIDC research contribution — unproven

An interesting future experiment would search **semantically valid compiler transformations against an ensemble of attacker representations**: assembly, lifted IR, pseudocode and trace-informed synthesis. Optimize the worst observed attack outcome subject to hard latency/size limits, then test on held-out models, programs and transformation seeds. This combines ideas above; **this investigation has not established it as a new invention or an effective defense**.

The novelty worth investigating is robustness to an informed attacker changing representations, rather than overfitting an LLM prompt or maximizing syntactic size. The fitness function should measure withheld behavioral reconstruction and attacker cost. Correctness must be checked independently. A search that wins only against known models can still fail through tool normalization, adversarial retraining or a stronger future model. No framework or implementation is proposed now.

## Existing techniques as comparison baselines

These are reference options, not requests to rebuild features VOIDC already has. Cost entries are qualitative engineering expectations unless a cited experiment above supplies numbers; there is no measured VOIDC overhead in this report.

| Technique | Useful protection and bypass | Runtime / size / build cost | Evidence and future disposition |
| --- | --- | --- | --- |
| Separate private symbols; minimize unnecessary release names/metadata | Removes lexical clues and accidental source disclosure; exports, ABI, required runtime tables and behavior remain available | No added executed instructions expected; release size can fall; extra artifact handling | Strong tooling support; ordinary release hygiene, not novel AI resistance. Preserve private symbols and crash identity. |
| Flattening, opaque predicates, substitution and classic MBA | Disrupts static control/data flow; normalization, tracing and synthesis can recover semantics | Branches, register pressure and I-cache growth; compile cost can rise; no universal acceptable factor | Useful controlled baseline. Do not infer success from pseudocode ugliness or more instructions. |
| String/constant protection | Hides literal scans; decoding and runtime observation expose needed values | Decode latency, allocations/residency, possible binary growth; actual costs unknown | Limited exposure reduction, no key secrecy. Required constants remain observable when used. |
| Build/function diversification | Makes exact byte signatures and reuse across builds less convenient; semantic matching and per-build analysis remain | Layout can improve or harm cache behavior; extra build/storage/symbol-map costs | Measure cross-build transfer. Different bytes alone do not establish harder AI reconstruction. |
| Selective virtualization | Adds another semantic layer to selected logic; emulator reconstruction and traces remain possible | Potentially large execution/size cost and costly qualification; unknown for VOID | Highest-complexity optional comparator. Whole-runtime virtualization is unjustified. |
| Encrypted asset packages | Reduces casual offline extraction while ciphertext stays encrypted; client decode/GPU upload exposes content | Storage/decode/streaming costs, buffers and key management | Packaging protection, not durable confidentiality from an authorized client owner. Independently benchmark extraction. |
| Signing, authenticated packages and integrity checks | Establishes origin/tamper evidence under a trusted verifier; local checks can be patched on a controlled copy | Hash/signature and startup/update cost; self-checks add runtime work | Prefer platform enforcement for authenticity. No encryption or algorithm secrecy follows from signing. |
| Anti-debugging, packing, self-modifying loaders | Can increase initial access cost; analysts can patch/emulate/dump or change tools | Startup, page faults, compatibility, support and AV costs; unknown | High correctness/support risk. Not a default candidate for VOID; no deceptive crash-as-protection behavior. |

LLVM 19 documents private-debug extraction and format-specific stripping; this is not permission to remove arbitrary sections or runtime metadata. Keep ELF debug companions, PE PDBs and Mach-O dSYMs private and tied to the exact distributed build. [LLVM 19 objcopy reference](https://releases.llvm.org/19.1.0/docs/CommandGuide/llvm-objcopy.html).

## Platform and LLVM 19.1.7 compatibility

### Native platform features have specific purposes

| Platform | Relevant established mechanisms | Boundary and compatibility concerns |
| --- | --- | --- |
| Linux / ELF | PIE/ASLR, non-executable memory, RELRO/binding policy; visibility and separated debug artifacts | Exploit mitigations do not conceal instructions. ELF has no universal desktop code-signature enforcement equivalent to a trusted application-store policy. Preserve relocations, unwind and GC data. [Linker semantics](https://sourceware.org/binutils/docs/ld/Options.html). |
| Windows / PE | DEP/ASLR, CFG and signed distribution; private PDBs | CFG verifies indirect targets; it is not anti-decompilation. Compiler instrumentation and linker metadata are both needed; jump/callback rewrites must remain compatible. Do not infer coverage from a linker flag alone. [Microsoft CFG specification, updated October 2025](https://learn.microsoft.com/en-us/cpp/build/reference/guard-enable-control-flow-guard?view=msvc-170). |
| macOS / Mach-O | Code signing/notarization, Hardened Runtime, library validation; private dSYMs | Protects integrity and loading policy, not readable-code confidentiality. Dynamic-code/self-modifying approaches may require entitlements and weaken protections; plug-ins/native interop need explicit qualification. Transform before final signing. [Apple signing model](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/AboutCS/AboutCS.html), [Hardened Runtime](https://developer.apple.com/documentation/security/hardened-runtime), [library validation](https://developer.apple.com/documentation/BundleResources/Entitlements/com.apple.security.cs.disable-library-validation). |

IR arithmetic techniques are conceptually portable across ELF/PE/Mach-O, but that is not evidence that a particular implementation works on all three. Architectural/MIR techniques are ISA-sensitive. Native x86-64 results do not qualify AArch64, Windows unwinding or Apple signing. None of the novel candidates here is a demonstrated, ready-to-use three-platform VOIDC/LLVM 19.1.7 integration.

### Reuse the existing compilation path, if later authorized

LLVM 19.1.7 supports pass registration/plugins for IR; its code-generation pipeline uses the legacy pass manager, so MIR integration has a distinct dependency. An external LLVM fork is not made compatible merely by its name. A Clang plugin flag does not transform an already-emitted native object. Any experiment must enter the actual LLVM pipeline that produces VOID application code, with the pinned compiler configuration. [Pinned pass documentation](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/docs/WritingAnLLVMNewPMPass.rst).

There is an existing limited diversification facility: LLD 19.1.7 ELF accepts seeded `--shuffle-sections`. It reorders matched sections, not instruction semantics; it is not a portable protection mode or a cryptographic primitive. [Pinned option definition](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/lld/ELF/Options.td).

Future qualification must preserve these invariants:

- Integer rewrites match defined VOID behavior, including width, signed operations, checks and exceptional cases. Modulo identities do not automatically preserve checked arithmetic. Do not introduce LLVM poison through invalid overflow flags, shifts or speculative operations. Float transformations require separate NaN, signed-zero, rounding and exception analysis. [Pinned LLVM language semantics](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.7/llvm/docs/LangRef.rst).
- Managed references remain visible to the actual GC root scheme. Encoding a pointer as an integer or moving a live reference to untracked storage is not harmless obfuscation. The LLVM GC documentation illustrates why root locations and relocations require invariants; it does not establish that VOID uses statepoints. [LLVM 19 GC contract](https://releases.llvm.org/19.1.0/docs/Statepoints.html).
- Async suspension/cancellation, cleanup and resource ownership keep their order and lifetimes. Do not transform scheduling/coordination code merely because it is cold. Exclude uncertain runtime/generated regions from any first experiment.
- Preserve external call signatures, alignment, callbacks, atomics, calling conventions, unwind/exception information and precise stack behavior. MIR stack/register transformations require stronger evidence than arithmetic expression substitution.
- Keep Debug execution usable. Capture private debug data for the transformed artifact, not an earlier untransformed binary. A private transformation map may be necessary; retain the seed, recipe/version, hashes and toolchain identity for reproducible crash analysis.

For any future experiment, initially select pure integer application functions without allocations, suspension, FFI or runtime hooks. That is a conservative experimental boundary, not a proposal to weaken production semantics. Expand eligibility only with evidence.

## Three possible future strategies

| Strategy | Contents, conditional on what already exists | Evidence / complexity / native cost | Recommendation |
| --- | --- | --- | --- |
| **A — Lightweight** | Ordinary release metadata minimization, private symbols and platform authenticity/hardening; optional measured layout diversification | Strong infrastructure evidence; low added complexity; no general AI-secrecy result. Little or no added application work for metadata removal; platform checks still have costs. | Best practical baseline. Do not spend this mission reimplementing existing facilities. |
| **B — Balanced** | A plus a narrowly selected arithmetic candidate, tested against synthesis and tool-assisted models; protect only useful cold/noncritical logic after measurement | Medium integration complexity for IR-only work; resistance evidence is narrow and application cost unknown | Best future research path. SaMBA supplies inspectable generation/proof artifacts; evaluate it before architectural-state or MIR integration. Reject if optimization erases it or adaptive recovery remains cheap. |
| **C — Maximum practical** | B plus selectively qualified MIR/dataflow disruption or virtualization, optional asset packaging, and platform-backed integrity policy | Highest engineering/support cost, potentially large runtime and size overhead; no whole-application or AI guarantee | Only developer opt-in after per-target qualification. Do not combine every technique or include speculative cryptographic/hardware dependencies by default. |

Ranked for present decision-making: **A first for practicality; B first for new research value; C last because costs and compatibility evidence are weakest.** No option is approved or promised to achieve a particular resistance factor. Published overheads above are measured on different workloads and cannot be added, multiplied or advertised as VOIDC estimates.

## A small future measurement, not a new framework

Boss would authorize the target, budget and candidate separately. A bounded pilot can use **two newly authored VOID programs**, three variants (ordinary optimized release, existing protection baseline, baseline plus one candidate), three model families, and two independent attempts: 36 first-round agent sessions. An equally sized informed second round gives **72 sessions total**, capped at ten minutes each: at most 12 agent-hours plus an explicit aggregate monetary cap covering both rounds. Ten minutes is a screening budget, shorter than many published analyses. This is a proposed pilot size, not a claim of statistical power. Add a medium application and more seeds only if promising. Reuse ordinary build/test scripts and a CSV/JSON manifest; no service or benchmark platform is needed.

Include a distinctive stateful gameplay computation and an asset-processing/application workload, rather than only familiar sorting/hash functions. Make originals private until evaluation is frozen, with withheld parameterizations and inputs. Public algorithms are useful controls, but identifying a known algorithm from prior knowledge must not count as reconstructing a private one.

1. **Freeze inputs and artifacts.** Exact LLVM/VOIDC versions, binary hashes, build settings, protection version/seed, OS/ISA, dependencies, private symbol identity and target exposure. Compare identical inputs and optimization across variants. Keep a build-identical replay and independent diversification seeds separate.
2. **Establish correctness first.** Run the same outputs/state/error/resource tests on each build; use exhaustive or SMT checks where small domains permit, randomized tests otherwise. Verify GC stress, async cancellation, callbacks/FFI, allocation failures, real 1 MiB stacks and crash symbolication when the selected transformation can affect them. A crash or disabled test is a failed candidate, never protection success.
3. **Measure cost.** Clean build/link time, peak build memory, stripped/debug sizes, startup, steady-state latency, frame-time tails, peak memory/stack and relevant I/O. Repeat stable runtime measurements; report raw samples and dispersion. Keep code/data overhead separate.
4. **Give analysts equal capabilities.** Freeze model snapshot, harness, prompts, context, sampling parameters and time/token/tool budgets. Provide the same decompiler/disassembler, debugger/tracer, emulator and scripting/synthesis tools. A model/tool failure, refusal or unavailable endpoint is a separate category from technical resistance. Do not rely on hidden writeups or leaked symbols.
5. **Grade outcomes separately.** Names/types/boundaries, correct algorithm, executable behavioral clone, asset extraction and a specified tamper task each get their own result. Run clones against withheld outputs and state transitions; use fixed-width proofs for extracted formulas when feasible. Semantic-similarity scores and LLM judgments are supplemental, not the pass oracle.
6. **Test adaptation.** Preserve first-pass results, then reveal the transformation family and allow a fixed additional budget. Permit assembly-only analysis, normalization, fresh implementation, traces, synthesis and multiple model attempts. Count total spend/time. Report pass@1, union success and the actual practical selector; an oracle choosing a good attempt is a distinct metric.
7. **Preserve all evidence.** Prompts, tool versions, logs, binaries, seeds, grader and unsuccessful outcomes. Follow VOID's incident evidence practice: preserve exact `_crash.void` input and invocation for abnormal compiler/runner exits, alongside binary identity, without replacing history with successful retries.

Before running, choose an acceptable runtime/size/build ceiling and required improvement in verified recovery success at a fixed cumulative attack budget. Record unrecovered cases as censored time/cost observations; do not compare only successful attempts or report a recovery-time median when the cap prevents its estimation. A candidate passes this **pilot gate** only if it improves an owner-valued task within those limits and survives the informed second round. Confidence intervals will be broad. An unobserved recovery within a budget means only “not recovered in these trials,” not secure.

## Long-term architecture recommendation and owner decisions

If later evidence justifies product work, keep protection an optional policy in the **existing AOT pipeline**. Reuse LLVM/linker facilities, ordinary artifact packaging and existing correctness/incident tests. Prefer independently selectable transformations and an explicit eligibility boundary; avoid a second frontend, GC, scheduler, bytecode platform or protection service. Establish pass ordering experimentally because ordinary optimization can erase a transformation or amplify its cost. Retain the pinned LLVM 19.1.7 until Boss authorizes anything else.

Use artifact-side policy/selection where feasible so application authors need not rewrite source. Exclude public ABI/runtime-sensitive regions until qualified. Diversification recipes must be reproducible privately; treat seeds as build identity, not security keys. Any transformation affecting signed content precedes final signatures. Removal of names must never remove semantic metadata needed for execution, ownership or GC.

Only these decisions genuinely require Golden Wrench/Boss:

1. **What deserves protection?** Algorithm extraction, casual asset copying, authenticity or cloning require different measures. Recommendation: choose one valuable algorithm and one asset task for the first study.
2. **Which future experiment, if any?** Recommendation: an inspectable arithmetic-generation candidate with no runtime redesign, followed by an informed synthesis/agent test. Do not approve a toolchain fork based on a decompiler failure count.
3. **What costs are acceptable?** Set latency/frame-tail, executable-size, clean-build and investigation-budget ceilings before experiments; this report cannot infer commercial tolerances.
4. **Does the deployment permit a different trust boundary?** Hardware/online protection requires separate product decisions about offline play, availability, privacy and portability. Recommendation: keep those out of ordinary VOIDC protection claims.

There is no production implementation, compatibility certification or protection acceptance in this report. Stop here for independent Golden Wrench review and Boss authorization.
