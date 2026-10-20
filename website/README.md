# VOID Language website

Self-contained static website. All work lives in `/website`; the compiler, Runtime,
existing documentation, editor projects and tests are independent of this project.
No npm packages, framework, backend, analytics or external fonts are used.

## Local preview

From the repository root, with Python 3.9+:

```sh
python3 website/scripts/check.py
python3 -m http.server 4173 --bind 127.0.0.1 --directory website/public
```

Open http://127.0.0.1:4173. Stop with Ctrl+C.

## Build

```sh
python3 website/scripts/check.py
python3 website/scripts/build.py
```

The build copies only `website/public/` into `website/dist/`. The output is ignored
by the repository's existing `dist/` rule. No compiler build is involved. To preview
that exact output, change the server's directory argument to `website/dist`.
The Python scripts use only the standard library. Python is a development/build
tool; the deployed website needs only a static file host.

## Files

- `public/index.html`: home page, roadmap, semantic HTML and metadata.
- `public/{why-void,features,examples,docs,getting-started}/index.html`: separate
  content pages with shared styles, navigation and accessible theme controls.
- `public/assets/`: CSS, progressive JavaScript, original SVG mark, optimized reference-based landscape and
  a 1200×630 PNG social preview.
- `public/docs/*/index.html`: 30 public language guides with sidebar navigation,
  complete examples, explanations and expected results.
- `public/examples/`: three introductory repository examples and 27 guide programs.
- `public/404.html`, `robots.txt`, `sitemap.xml`, `_headers`: static hosting files.
- `scripts/`: small validation and copy-build scripts.
- `REVIEW.md`: content sources, validation evidence and owner review items.

The original generated landscape and its built-in image_gen prompt are retained
in `artwork/`; only the optimized WebP is deployed.

Navigation, documentation links and native details disclosures work without
JavaScript. JavaScript enables the mobile menu, theme selection (saved locally
when storage is available) and current-page navigation highlighting. A system
dark preference is respected until a visitor chooses a theme. No data is sent.

## Cloudflare Pages — owner deployment only

No project, domain, DNS, Git or publication changes have been made.
After owner review, create a Pages project connected to this repository with:

| Setting | Value |
| --- | --- |
| Framework preset | None |
| Production branch | Owner-selected existing branch |
| Root directory | `website` |
| Build command | `python3 scripts/check.py && python3 scripts/build.py` |
| Build output directory | `dist` |

Use a Pages build image providing Python 3.9+. The validation script reads the
repository's sibling `examples` directory; use a full repository
checkout. If using a prebuilt Direct Upload instead, build locally and upload
only the contents of `website/dist`, never the entire repository.

These settings follow Cloudflare's [static HTML guide](https://developers.cloudflare.com/pages/framework-guides/deploy-anything/)
and [build configuration guide](https://developers.cloudflare.com/pages/configuration/build-configuration/).

After the owner approves deployment:

1. Check the Pages preview, navigation, asset responses and 404 behavior.
2. In the Pages project's **Custom domains**, add `voidlang.com`. An apex domain
   must be a Cloudflare zone in the same account; follow the dashboard's DNS and
   certificate steps. See [custom domains](https://developers.cloudflare.com/pages/configuration/custom-domains/).
3. Add `voidlang.net` through the owner-controlled domain setup. Use Cloudflare
   Bulk Redirects to issue a permanent redirect from that host to
   `https://voidlang.com`, preserving paths and query strings. Configure `www`
   hostnames only if desired. See the [domain redirect example](https://developers.cloudflare.com/pages/how-to/www-redirect/).
4. Verify HTTPS on both domains and the actual redirect response. The canonical,
   sitemap and social URLs already use `https://voidlang.com`.
5. Decide whether the production `pages.dev` hostname should redirect to the
   primary domain; preserve preview deployment access as appropriate.

`_headers` supplies self-only resource loading, nosniff and frame restrictions on
Cloudflare Pages. The Python preview server does not interpret `_headers` or
provide production 404 behavior. Cloudflare/DNS/HTTPS behavior must be checked by
the owner after deployment. There are no Functions, routing rewrites or SPA fallback.

## Content updates

Check the actual compiler contracts before changing claims. The Language Bible
includes future design; do not treat every example as executable. Pages contain
public explanations and guides written for language users. Internal handoffs and
workflow language must stay out of `public/`.
When upstream examples change, update the corresponding downloads and displayed
code together, rerun `check.py`, and execute the examples with a current `voidc`.
No public installer, package manager, performance benchmark, license grant or
release date is promised. The SVG mark is an original approximation of the mockup
and needs owner branding approval before publication.

## Guide example verification

`python3 website/scripts/check.py` also verifies that displayed guide source
matches each downloadable program. `scripts/guide-examples.json` records the
input and expected result for each guide; execute these with a current compiler
when changing examples. The verified local compiler for this revision was
`build/ci-release/voidc`. Public documentation contains no installation claims
or links that require readers to open internal Markdown files.

To repeat the executable guide checks from the repository root:

```sh
python3 website/scripts/verify_examples.py build/ci-release/voidc
```

Choose a current compiler path for your checkout; an older architecture-only
binary cannot validate the current language features. This check is optional for
website deployment and never builds or modifies the compiler.

## Expanded content

Why VOID explains the problems, design choices, influences and current/future
boundaries. The public documentation covers foundations, data and behavior,
concurrency, native integration, editor tooling and live development.
The examples page offers a native dropdown with 15 curated programs, direct
fragment links, downloads and guide links. Without JavaScript all examples remain
readable. Public content does not send visitors to internal Markdown files.

Repository examples are working test-suite programs. The website preserves its
copies and checks its own displayed code, expected results and links. Async
programs use the existing explicit async adapter; that execution requirement is
shown rather than presenting them as synchronous CLI programs.

To also rerun the existing async AOT examples without building anything:

```sh
python3 website/scripts/verify_examples.py build/ci-release/voidc --async-artifacts build/order1/production
```

Both binary paths refer to existing local builds; choose matching paths for your
checkout. Website deployment needs neither the compiler nor these artifacts.
