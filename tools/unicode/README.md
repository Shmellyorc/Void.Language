# Pinned default Unicode casing

Runtime tables are Unicode **16.0.0**, independent of the host locale and installed
Unicode libraries. Regeneration is a developer action, not a build dependency:

```sh
python3 tools/unicode/generate_case_tables.py --unicore /path/to/perl/unicore --output src/runtime/unicode_case_tables.inc
```

The tool requires Python's `unicodedata.unidata_version` and the supplied Perl UCD
version to both equal 16.0.0. Full unconditional lower/uppercase mappings come from
Python's Unicode tables. Cased/Case_Ignorable inversion lists come from Perl's
Unicode 16.0 generated property data. Input hashes, table counts and the generated
output hash are in `docs/text-encoding/evidence/unicode.json`. The generated table
is committed source input; users need neither Python nor Perl to build/run VOID.

Authoritative references: [Unicode 16.0](https://www.unicode.org/versions/Unicode16.0.0/),
[default case algorithms §3.13](https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/),
[SpecialCasing](https://www.unicode.org/Public/16.0.0/ucd/SpecialCasing.txt).
Only unconditional mappings and the locale-independent Final_Sigma context apply.
Case_Ignorable takes precedence over Cased when properties overlap. Two context
passes keep long ignorable runs linear. No case folding or locale tailoring.

Unicode data copyright © 1991–2024 Unicode, Inc. Distributed under the
[Unicode data license](https://www.unicode.org/license.txt). Python's tables derive
from the same Unicode Character Database; Python is distributed under the
[PSF license](https://docs.python.org/3/license.html). Preserve these attributions
when regenerating or redistributing the generated data.
