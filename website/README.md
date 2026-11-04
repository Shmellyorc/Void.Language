# VOID Language website

Independent static HTML, CSS and JavaScript. All website sources and tooling live
here; compiler sources, repository `/docs`, repository `/examples` and language
regression tests are separate and must not be edited by website work.

## Preview and build

From the repository root, with Python 3.9+:

```sh
python3 website/scripts/check.py
python3 website/scripts/build.py
python3 -m http.server 4173 --bind 127.0.0.1 --directory website/dist
```

Open http://127.0.0.1:4173. Stop the server with Ctrl+C. The build copies only
`website/public` into the generated `website/dist` directory. No npm, framework,
backend, database, analytics or external fonts are required.

## Example maintenance

Website examples teach a focused idea and are maintained independently of compiler
regression fixtures. Keep helpful small examples; remove assertions and operations
that exist only to test compiler behavior when adapting a fixture for readers.

- `public/examples/index.html`: curated dropdown, explanations and source downloads.
- `public/examples/*.void` and `public/examples/guides/`: downloadable programs.
- `public/docs/*/index.html`: documentation with independently authored samples.
- `public/docs/snippets/`: documentation-owned programs, fragments and support modules.
- `scripts/guide-examples.json`: primary input/result, additional cases, async/application modes
  and an explicit `teaching` or `repository-copy` maintenance policy.

A `teaching` program is website-owned; do not require it to match a regression
fixture. A `repository-copy` retains exact byte parity with its recorded source.
Every displayed program must match its download. Displayed inputs/results and
result metadata must agree with the manifest. `check.py` enforces these checks,
links, anchors, metadata, branding and public-content boundaries.

Documentation rows additionally identify their `guide` and `kind` (`program` or
`fragment`). Complete programs have displayed input/results. Fragments are
explicitly labeled and have no standalone entry/result claim; their exact code is
compiled in the manifest's `context_before` / `context_after`, optionally indented
by `context_indent`. Those small verification wrappers stay private. Supporting
module files are listed under `support` and displayed/downloaded with their entry.
The checker requires complete documentation-source coverage and rejects reuse of
Examples-page downloads or token-identical programs. No native-binding program is
claimed without the required trusted host. See `DOCS_EXAMPLES_REVIEW.md` for the
per-guide review and actual verification results.

To compile and execute all programs with existing local builds:

```sh
python3 website/scripts/verify_examples.py build/llvm23-release/voidc --async-build build/llvm23-release
```

Select a current compatible build for your checkout. Async validation uses the
existing `void_async_tests --emit` adapter and matching runtime archives from the
selected directory. It freshly emits each exact website source in normal and
optimized LLVM modes, links with the Clang/LLD driver and libraries from the build’s LLVM 23.1.4 `void-native.txt` manifest, and executes
its documented cases. Synchronous documentation programs and contextual fragments
run through both normal and optimized JIT modes. All generated objects/executables live in an automatically
removed temporary directory. No compiler/runtime rebuild or modification occurs.
Existing fixture executables cannot validate independently authored website code
and are not reused. Missing async tooling fails verification instead of silently
skipping execution. Website deployment does not require compiler binaries.

Application rows use `mode: "app"`, `expected_status` and `expected_stdout`, with
no numeric adapter input or printed-result field. Their result bars distinguish
process status from standard output. The verifier compares the exact stdout and
exit status independently in optimized/unoptimized modes; main-based downloads
also execute through directory discovery. A successful return from main is not a
printing API. See `STAGE8_WEBSITE_REVIEW.md` for the historical content and validation
record.

## Homepage showcase and roadmap

The homepage uses three short source programs under `public/showcase/`, registered
with `showcase: true` in the existing example manifest. The hero editor and
terminal share a source/result contract; below-hero demos provide source downloads.
The verifier executes showcase cases in both optimized and unoptimized JIT modes.
No separate sample or SEO framework is required.

`public/roadmap/index.html` describes the implemented foundation, current work and
future direction. Main navigation links directly to `/roadmap/`; `/#roadmap`
remains a working legacy target for the short homepage teaser. There are now 50
indexable pages. Both the error page and the repository availability page are noindex. See `HOME_SHOWCASE_REVIEW.md`
for scope, source evidence, preservation comparisons and verification.

## Public content

The site contains 43 guides. The example selector includes 27 teaching programs. Navigation and guides work
without JavaScript; all examples remain readable when the selector enhancement
is unavailable. JavaScript supplies the menu, locally saved theme and dropdown. The default is
light, independent of the operating-system preference; an explicitly saved theme
is honored. Without JavaScript, the CSS default is also light.
Public pages do not expose internal workflow language or substitute Markdown
links for explanations. Keep implemented behavior distinct from future design.

The cleanup example models scope ownership in memory. See the files and paths guide for real filesystem operations. Async programs require the explicit async
host adapter; the ordinary synchronous CLI entry adapter does not run them.
Factorial demonstrates small non-negative inputs; i32 results beyond 12! wrap.

## Repository availability and current review

All public GitHub anchors use the stable internal route `/github/`. This works
with JavaScript disabled. The availability page is excluded from the sitemap and
declares `noindex`. When public access is officially announced, the maintainer can change that
one landing page to link or redirect to the approved repository destination.
Keep the current repository-link routing regression until that announcement.

Primary Docs navigation opens `/docs/syntax/`. The sidebar and footer also link
to the full guide index at `/docs/`. The verifier stages sources in a temporary
directory to keep compiler caches out of the public build. Its HTTP example uses
a loopback fixture. See [the modernization review](MODERNIZATION_REVIEW.md).

## Modernized SEO acceptance

[SEO modernization review](SEO_MODERNIZATION_REVIEW.md) records the current
50 indexable pages, historical route/anchor compatibility, metadata inventory,
crawl results and read-only live comparison. The existing checker also guards
noindex/header conflicts and crawlability of the availability page.

## SEO maintenance

Keep each page's title, description and social metadata specific to its content.
Use self-canonical HTTPS `voidlang.com` directory URLs and update the sitemap when
adding/removing a page. Do not invent modification dates or enforce arbitrary
metadata character limits. Run the same validator against source and built HTML:

```sh
python3 website/scripts/check.py
python3 website/scripts/test_check.py
python3 website/scripts/build.py
python3 website/scripts/check.py --directory website/dist
```

`test_check.py` uses isolated negative fixtures to test the existing checker.
`SEO_AUDIT.md` records the page-by-page review and owner follow-up;
`SEO_LIVE_CHECK.json` records the pre-deployment public HTTP observations.
Both stay outside the public build. Search Console and deployment remain owner actions.

## Cloudflare Pages — owner deployment only

No deployment, DNS, Git publication or Cloudflare changes have been performed.

| Setting | Value |
| --- | --- |
| Framework preset | None |
| Production branch | Owner-selected existing branch |
| Root directory | `website` |
| Build command | `python3 scripts/check.py && python3 scripts/build.py` |
| Build output directory | `dist` |

Use a Python 3.9+ build environment with the full repository checkout for retained
source-copy checks. Alternatively build locally and upload only `website/dist`.
See Cloudflare's [static HTML guide](https://developers.cloudflare.com/pages/framework-guides/deploy-anything/)
and [build configuration guide](https://developers.cloudflare.com/pages/configuration/build-configuration/).

After owner approval, add `voidlang.com` through Pages Custom domains. An apex must
be a Cloudflare zone in the same account. Configure `voidlang.net` as an
owner-controlled permanent redirect to the primary domain, preserving paths and
queries. Follow the [custom domain guide](https://developers.cloudflare.com/pages/configuration/custom-domains/)
and [domain redirect example](https://developers.cloudflare.com/pages/how-to/www-redirect/).
Verify HTTPS, actual redirect responses, assets, 404 and `_headers` behavior after
deployment. Python's preview server does not interpret Cloudflare headers or
production 404 routing. Canonical/social URLs and sitemap use `voidlang.com`.

Original background artwork and its generation prompt remain in `artwork/`; only
optimized WebP assets deploy. `review/` and `REVIEW.md` contain private verification
notes/screenshots and are excluded from output.
