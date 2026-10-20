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
`website/public` into the ignored `website/dist` directory. No npm, framework,
backend, database, analytics or external fonts are required.

## Example maintenance

Website examples teach a focused idea and are maintained independently of compiler
regression fixtures. Keep helpful small examples; remove assertions and operations
that exist only to test compiler behavior when adapting a fixture for readers.

- `public/examples/index.html`: curated dropdown, explanations and source downloads.
- `public/examples/*.void` and `public/examples/guides/`: downloadable programs.
- `public/docs/*/index.html`: guides using the same canonical downloadable sources.
- `scripts/guide-examples.json`: primary input/result, additional cases, async mode
  and an explicit `teaching` or `repository-copy` maintenance policy.

A `teaching` program is website-owned; do not require it to match a regression
fixture. A `repository-copy` retains exact byte parity with its recorded source.
Every displayed program must match its download. Displayed inputs/results and
result metadata must agree with the manifest. `check.py` enforces these checks,
links, anchors, metadata, branding and public-content boundaries.

The factorial and loop guides share their canonical files at
`public/examples/functions.void` and `public/examples/loops.void`. No duplicate
copies of those programs are maintained under `guides/`.

To compile and execute all programs with existing local builds:

```sh
python3 website/scripts/verify_examples.py build/ci-release/voidc --async-build build/ci-release
```

Select a current compatible build for your checkout. Async validation uses the
existing `void_async_tests --emit` adapter and matching runtime archives from the
selected directory. It freshly emits each exact website source in normal and
optimized LLVM modes, links with `clang++-19` (override using `--cxx`), and executes
its documented cases. All generated objects/executables live in an automatically
removed temporary directory. No compiler/runtime rebuild or modification occurs.
Existing fixture executables cannot validate independently authored website code
and are not reused. Missing async tooling fails verification instead of silently
skipping execution. Website deployment does not require compiler binaries.

## Public content

The site contains 30 guides and 15 curated examples. Navigation and guides work
without JavaScript; all examples remain readable when the selector enhancement
is unavailable. JavaScript supplies the menu, locally saved theme and dropdown.
Public pages do not expose internal workflow language or substitute Markdown
links for explanations. Keep implemented behavior distinct from future design.

The cleanup example is an in-memory model of scoped ownership, not a claim that a
filesystem/native-resource API exists. Async programs require the explicit async
host adapter; the ordinary synchronous CLI entry adapter does not run them.
Factorial demonstrates small non-negative inputs; i32 results beyond 12! wrap.

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
