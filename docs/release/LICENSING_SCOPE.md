# VOID original-code licensing scope

The owner selected MIT and authorized its application to VOID 2.0. The repository
root `LICENSE`, installed as `VOID-MIT-LICENSE.txt` alongside this document, grants
MIT rights in VOID-original code. Attribution is `Copyright (c) 2026 shmellyorc`,
as established in the approved implementation plan and recorded Git identity.
Upstream copyrights and licenses remain in force for their respective material.

MIT covers the original compiler/frontend/IR/backend/driver implementation in
`src/` and `include/`, original Runtime and native fixture/sink code, original
`stdlib/*.void` sources, language server, Vim/Neovim/VS Code integration source,
and VOID-authored tools, CMake and workflow code. Original examples/tests and
their generators are covered subject to the exceptions below. The licensing
scope, inventory and redistribution guide authored for this change are also MIT.
Neither a directory name nor a generated extension proves ownership.

## Material retaining separate terms

- `third_party/` contains upstream software, data and notices. curl, date, Ryu,
  tzdata, PSL and HTTP libraries keep their upstream terms, including in locally
  modified files. Preserve their NOTICE and provenance records. Identifiable
  original VOID modifications do not relicense the surrounding upstream code.
- `src/runtime/unicode_case_tables.inc` contains generated Unicode 16.0 data.
  Its Unicode license and Python/Perl input provenance remain separate from the
  original generator's MIT license. See installed `UNICODE-LICENSE.txt` and the
  attribution in `REDISTRIBUTION.txt`.
- `third_party/date/src/void_embedded_tzdb.hpp` embeds IANA time-zone data and
  date/Ryu/tzdata notice strings. Keep those data terms and strings intact;
  `tools/embed_tzdata.py` being MIT does not relicense the inputs.
- Numerical/reference outputs, including `tests/formatting/vectors.inc`
  (Python Decimal) and `tests/math/vectors.json` (MPFR 4.2.2), retain their input
  provenance. Whether any protected third-party expression survives generation
  remains unverified; they are not installed by compiler/tooling components.
- Downloaded SDKs, libraries, archives, `node_modules`, build outputs and retained
  binary evidence are not VOID-original grants. Dependency metadata is not a
  declaration that its dependencies are MIT.
- Website/artwork, the protected public README and unclassified historical
  documentation/evidence are outside this original-code grant's stated scope.
  Their ownership/licensing is not resolved by placing MIT at the root.

## Attribution evidence and remaining questions

The inspected baseline `95ce0a76e9a95198cd4fe9d5b6bb4cbfad9bd5c5` records 174
commits under one author/committer identity, `shmellyorc`, beginning 10 October
2026. The owner's directive supplies the MIT selection and authority to apply
that attribution to the owner's original code. Git metadata is not independent
proof of rights to every imported fragment or an assignment from others.
Earlier imported work, employer/contractor interests, unrecorded contributions,
borrowed fragments and Python/Perl-derived data attribution remain precise
provenance questions for owner confirmation. No assignments or legal clearance
are invented. Preserve any independent contributor notices discovered later.

## Runtime and downstream applications

The MIT grant includes original Runtime and standard-library portions copied or
linked into generated native programs. Preserve VOID's MIT copyright and
permission notice with covered copies/substantial portions; a companion notices
file is suitable material to carry with a distribution. This does not require
the application's unrelated original code to use MIT.

Third-party portions retain their own requirements. The installed
`REDISTRIBUTION.txt` and `NOTICE_INVENTORY.json` map those materials; MIT does not
waive GNU corresponding-source/replacement obligations, MPL source availability,
or platform runtime conditions. No SDK redistribution or public release approval
follows from this scope document.
