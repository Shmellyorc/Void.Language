# Text and encoding contract — Sprint 3

Baseline: e24d3a95db03fd282c87a5d9b885471057e38798. Implementation authorized by Boss;
Golden Wrench review follows local verification. No Git mutations or hosted CI.

All inputs are immutable/readonly observations. Results own their storage. Embedded
NUL survives. Resource exhaustion uses the existing checked Runtime failure channel;
ordinary invalid arguments return module errors extending error.error.

## Text

Case-sensitive literal searches return Unicode-scalar indexes, -1 when absent.
Empty needles match at zero (index_of) and scalar length (last_index_of); every
string contains/starts/ends with empty text. Overlapping matches count for last_index_of.
Substring accepts a scalar start in [-length,length], with negatives relative to
length. Count -1 means the remaining scalars; other negative counts and out-of-range
spans fail. No normalization or grapheme interpretation is performed.

Whitespace is exactly Unicode 16.0 White_Space: U+0009–000D, 0020, 0085, 00A0,
1680, 2000–200A, 2028–2029, 202F, 205F, 3000; excludes BOM and zero-width space.
Empty strings are empty and blank. Trimming removes complete whitespace scalars.

Split uses a nonempty literal delimiter, nonoverlapping left-to-right matches and
preserves empty fields (including adjacent/trailing delimiters); empty input yields
one empty field. Split_lines recognizes CRLF as one delimiter and lone CR/LF, with
the same empty-field behavior. Both return limit_exceeded if the number of fields
would exceed max_parts (default 1,048,576), never silently truncate. Join of an empty
array is empty. Replacement rejects empty searches; replace_first substitutes once,
replace substitutes nonoverlapping matches. Repeat rejects negative counts. Padding
requires nonnegative scalar width and exactly one scalar of padding, even if no
padding is needed. Widths at or below the input length preserve the input.

Fallible construction operations bound each produced string and total split payload
to 64 MiB (67,108,864 UTF-8 bytes); arithmetic is checked before growth. Lists grow
geometrically using existing source arrays; literal matching uses KMP over UTF-8
bytes with scalar boundary results. No repeated scalar indexing or string concatenation
is used for accumulation.

Casing is locale-independent Unicode 16.0 full default lower/uppercase, including
expansions and contextual Final_Sigma. Locale tailorings (Turkish/Azeri/Lithuanian),
case folding, normalization and grapheme segmentation are excluded. A private
canonical-text helper is needed because source strings expose neither scalar
codepoints nor Unicode property/mapping tables. It reuses the shared UTF-8 validator
and existing Runtime string construction; no representation or public syntax changes.

## Hex and Base64

Boss confirmed plain `encode(...) -> string` plus recoverable `try_encode(...)`
for both codecs. `try_encode` and `decode` return result with structured error_kind,
inherited diagnostic fields and an offset (UTF-8 byte offset, -1 when no individual
byte applies). Encoded output and decoded bytes are bounded to 64 MiB. Plain encode
turns an exceeded output budget into an ordinary checked Runtime failure; try_encode
returns limit_exceeded instead. Allocation exhaustion uses checked Runtime failure
for every variant and is not mislabeled as malformed input. Hex accepts exactly pairs of ASCII digits;
no prefixes, whitespace or separators. Encode defaults to lowercase; uppercase is
explicit. Empty succeeds.

Base64 selects alphabet.standard (+/) or alphabet.url_safe (-_) and padding.required
or padding.omitted, defaults standard/required. Required padding is exactly RFC 4648
padding (none when divisible by four without a partial quartet); omitted rejects '='.
No alphabet autodetection or whitespace skipping. Impossible lengths, misplaced/excess
padding, nonzero unused trailing bits, NUL, non-ASCII and mixed alphabets reject.

## ASCII

Encode accepts only U+0000–007F and decode only bytes 00–7F. Both are fallible and
bounded to 64 MiB. Individual-byte predicates accept u8; high bytes return false.
Alphabetic A–Z/a–z, digit 0–9, alphanumeric union, uppercase A–Z, lowercase a–z,
whitespace 09–0D/20, printable 20–7E, control 00–1F/7F. Byte lower/upper changes
only ASCII letters, leaving all other u8 values unchanged.

## Crash preservation

Every new sprint process harness preserves crashing source bytes before temporary
cleanup into evidence/crashes/<unique incident>/name_crash.void, with matching SHA-256,
command/status/cwd/stdout/stderr, executable hash, generator and relevant stdlib inputs.
Ordinary expected diagnostics are not crashes. Crash artifacts are never overwritten.
Historical filesystem SIGSEGV remains unresolved and its existing evidence is protected.

## Application examples

```void
use text
use convert
use encoding/hex
use encoding/base64

func main():
    var fields = convert.unwrap(text.split("雪||😀|", "|"))
    var bytes: [u8] = [0, 128, 255]
    var encoded = hex.encode(bytes, uppercase = true)
    var url = base64.encode(bytes, base64.alphabet.url_safe, base64.padding.omitted)
    var checked = hex.try_encode(bytes)
```

`fields` contains four fields, including the two empty fields. `encoded` is
0080FF; `url` is AID_. `checked` is Ok("0080ff"). For untrusted decoding, inspect
Err's kind and byte offset or propagate with `?` from a compatible result-returning
function. No decoding API silently drops malformed characters.
