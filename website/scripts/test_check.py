#!/usr/bin/env python3
"""Regression-test the existing checker against isolated, deliberately broken pages."""
import html
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def replace(old, new):
    def change(raw):
        text = raw.decode()
        if old not in text:
            raise AssertionError(f'Mutation target not found: {old}')
        return text.replace(old, new, 1).encode()
    return change


class SEOChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='void-seo-check-tests-')
        cls.public = Path(cls.temporary.name) / 'public'
        shutil.copytree(ROOT / 'public', cls.public)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def check(self):
        return subprocess.run([sys.executable, str(ROOT / 'scripts/check.py'),
                               '--directory', str(self.public)],
                              capture_output=True, text=True, timeout=30)

    def test_accepts_current_site(self):
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_seo_and_content_regressions(self):
        guide = 'docs/generics/index.html'
        cases = [
            (guide, lambda b: re.sub(rb'<title>.*?</title>', b'', b), 'nonempty title'),
            (guide, lambda b: re.sub(rb'<title>.*?</title>', b'<title>VOID Language</title>', b), 'does not describe'),
            (guide, lambda b: re.sub(rb'<title>.*?</title>', b'<title>Syntax Basics \xe2\x80\x94 VOID Language Documentation</title>', b), 'duplicate title'),
            (guide, lambda b: re.sub(rb'<meta name="description"[^>]*>', b'', b), 'description meta tag'),
            (guide, lambda b: re.sub(rb'(<meta name="description" content=")[^"]*', rb'\1', b), 'description meta tag'),
            (guide, lambda b: re.sub(rb'(<meta name="description" content=")[^"]*', rb'\1TODO', b), 'description is a placeholder'),
            (guide, replace('</head>', '<meta name="description" content="Another description"></head>'), 'description meta tag'),
            (guide, lambda b: re.sub(rb'<link rel="canonical"[^>]*>', b'', b), 'canonical must be exactly'),
            (guide, replace('href="https://voidlang.com/docs/generics/"', 'href="https://voidlang.net/docs/generics/"'), 'canonical must be exactly'),
            (guide, replace('content="https://voidlang.com/docs/generics/"', 'content="https://voidlang.com/docs/async/"'), 'og:url does not match'),
            (guide, replace('https://voidlang.com/assets/social.png', 'https://voidlang.com/assets/missing.png'), 'social preview asset'),
            ('assets/social.png', lambda b: None, 'social preview asset'),
            ('assets/social.png', lambda b: b'broken image', 'readable PNG'),
            (guide, replace('content="1200"', 'content="400"'), 'dimensions do not match'),
            (guide, replace('<h1 class="page-title">Generics</h1>', ''), 'meaningful H1'),
            (guide, replace('</main>', '<h1>Another heading</h1></main>'), 'meaningful H1'),
            (guide, lambda b: b.replace(b'<h2>', b'<h3>', 1).replace(b'</h2>', b'</h3>', 1), 'skipped heading level'),
            (guide, replace('<h2>', '<h3>'), 'mismatched heading/title'),
            (guide, replace('<html lang="en">', '<html lang="fr">'), 'lang="en"'),
            (guide, replace('width=device-width', 'width=1200'), 'mobile viewport'),
            (guide, replace('</head>', '<meta name="robots" content="noindex"></head>'), 'unintended indexing'),
            ('404.html', replace('<meta name="robots" content="noindex">', ''), '404 must explicitly use noindex'),
            ('robots.txt', replace('Allow: /', 'Disallow: /'), 'public URL is blocked'),
            ('robots.txt', replace('https://voidlang.com/sitemap.xml', 'https://voidlang.net/sitemap.xml'), 'correct absolute Sitemap'),
            ('sitemap.xml', lambda b: re.sub(rb'<url>.*?</url>', b'', b, count=1, flags=re.S), 'coverage mismatch'),
            ('sitemap.xml', lambda b: b.replace(b'</urlset>', re.search(rb'<url>.*?</url>', b, re.S)[0]+b'</urlset>'), 'duplicate sitemap'),
            ('sitemap.xml', replace('<loc>https://voidlang.com/</loc>', '<loc>https://voidlang.com/assets/social.png</loc>'), 'nonindexable URL'),
            ('docs/index.html', replace('href="/docs/generics/"', 'href="/docs/missing/"'), 'broken local reference'),
            (guide, replace('href="#part-1"', 'href="#missing-part"'), 'broken fragment'),
            (guide, replace('href="/docs/"', 'href="/docs/index.html"'), 'link requires a redirect'),
            ('_headers', lambda b: b+b'\n  X-Robots-Tag: noindex\n', 'indexing restriction requires review'),
            ('index.html', replace('"@type":"WebSite"', '"@type":}'), 'invalid JSON-LD'),
            ('index.html', replace('"@type":"WebSite"', '"@type":"Product"'), 'WebSite JSON-LD does not match'),
        ]
        for relative, mutation, diagnostic in cases:
            with self.subTest(path=relative, diagnostic=diagnostic):
                path = self.public / relative
                before = path.read_bytes()
                try:
                    after = mutation(before)
                    self.assertNotEqual(after, before)
                    if after is None:
                        path.unlink()
                    else:
                        path.write_bytes(after)
                    result = self.check()
                    self.assertNotEqual(result.returncode, 0, 'Checker accepted corrupted fixture')
                    self.assertIn(diagnostic, result.stderr)
                finally:
                    path.write_bytes(before)

    def test_unique_descriptions_and_source_result_checks(self):
        guide = self.public / 'docs/generics/index.html'
        original = guide.read_text()
        syntax = (self.public / 'docs/syntax/index.html').read_text()
        description = re.search(r'<meta name="description" content="([^"]*)"', syntax)[1]
        try:
            guide.write_text(re.sub(r'(<meta name="description" content=")[^"]*', lambda m:m[1]+description, original))
            self.assertIn('duplicate description', self.check().stderr)
        finally:
            guide.write_text(original)
        source = self.public / 'examples/functions.void'
        before = source.read_bytes()
        try:
            source.write_bytes(before+b'\n# A changed download must fail display parity.\n')
            self.assertIn('displayed source differs', self.check().stderr)
        finally:
            source.write_bytes(before)
        properties = self.public / 'docs/properties/index.html'
        before = properties.read_text()
        try:
            properties.write_text(before.replace('Expected result <strong>27</strong>', 'Expected result <strong>28</strong>'))
            self.assertIn('displayed input/result differs', self.check().stderr)
        finally:
            properties.write_text(before)

    def test_documentation_ownership_and_fragments(self):
        cases = [
            ('docs/generics/index.html', lambda b: b.replace(b'/docs/snippets/generics.void', b'/examples/guides/generics.void'), 'documentation code must use'),
            ('docs/generics/index.html', replace(' download>', '>'), 'matching download link'),
            ('docs/strings/index.html', lambda b: b.replace(b'fragment', b'illustration').replace(b'Fragment', b'Illustration'), 'fragment must be identified'),
            ('docs/snippets/strings.void', lambda b: b+b'\n# Changed documentation download.\n', 'displayed source differs'),
            ('docs/snippets/generics.void', lambda b: (self.public / 'examples/guides/generics.void').read_bytes(), 'documentation duplicates'),
            ('docs/snippets/modules/pricing.void', lambda b: None, 'missing documentation source'),
        ]
        for relative, mutation, diagnostic in cases:
            with self.subTest(path=relative):
                path = self.public / relative
                before = path.read_bytes()
                try:
                    after = mutation(before)
                    if after is None:
                        path.unlink()
                    else:
                        path.write_bytes(after)
                    result = self.check()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)
                finally:
                    path.write_bytes(before)

    def test_homepage_and_roadmap_contracts(self):
        cases = [
            ('index.html', replace('>Explore VOID</a>', '>Explore VOID →</a>'), 'primary hero action'),
            ('index.html', replace('Read the docs <span aria-hidden="true">↗</span>', 'Read the docs'), 'secondary hero action'),
            ('index.html', replace('class="result">2</span>', 'class="result">3</span>'), 'terminal command/result differs'),
            ('index.html', replace('data-input="18"', 'data-input="19"'), 'terminal command/result differs'),
            ('index.html', replace('id="roadmap"', 'id="roadmap-teaser"'), 'roadmap teaser anchor'),
            ('index.html', replace('href="/roadmap/">Roadmap', 'href="/#roadmap">Roadmap'), 'Roadmap navigation'),
            ('roadmap/index.html', lambda b: re.sub(rb'<link rel="canonical"[^>]*>', b'', b), 'canonical must be exactly'),
            ('showcase/scores.void', lambda b: b+b'\n# Source parity regression.\n', 'displayed source differs'),
            ('sitemap.xml', lambda b: re.sub(rb'\s*<url>\s*<loc>https://voidlang.com/roadmap/</loc>\s*</url>', b'', b), 'coverage mismatch'),
        ]
        for relative, mutation, diagnostic in cases:
            with self.subTest(path=relative, diagnostic=diagnostic):
                path = self.public / relative
                before = path.read_bytes()
                try:
                    after = mutation(before)
                    self.assertNotEqual(before, after)
                    path.write_bytes(after)
                    result = self.check()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)
                finally:
                    path.write_bytes(before)

    def test_code_window_decorations(self):
        cases = [
            ('examples/index.html', replace('Download source', 'Download source ↓'), 'download label must be'),
            ('docs/generics/index.html', replace('Download source', 'Download source &#x2193;'), 'download label must be'),
            ('docs/strings/index.html', replace('Download fragment', 'Download fragment &darr;'), 'download label must be'),
            ('docs/strings/index.html', replace('Download fragment', 'Download source'), 'download label must be'),
            ('examples/index.html', replace('> · </span>', '> → </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> &rarr; </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> &#x2192; </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> &bull; </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> . </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> </span>'), 'U+00B7 middle-dot separator'),
            ('docs/generics/index.html', replace('> · </span>', '> · · </span>'), 'U+00B7 middle-dot separator'),
        ]
        for relative, mutation, diagnostic in cases:
            with self.subTest(path=relative, diagnostic=diagnostic):
                path = self.public / relative
                before = path.read_bytes()
                try:
                    after = mutation(before)
                    self.assertNotEqual(before, after)
                    path.write_bytes(after)
                    result = self.check()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stderr)
                finally:
                    path.write_bytes(before)

    def test_application_status_is_not_stdout(self):
        path = self.public / 'docs/syntax/index.html'
        original = path.read_text()
        cases = [
            ('Exit status <strong>7</strong>', 'Expected result <strong>7</strong>'),
            ('data-expected-status="7"', 'data-expected-status="0"'),
            ('data-expected-stdout=""', 'data-expected-stdout="7"'),
            ('Standard output empty', 'Standard output 7'),
            ('data-mode="app"', 'data-mode="entry"'),
            ('data-mode="app"', 'data-mode="app" data-input="0"'),
            ('Exit status <strong>7</strong><span aria-hidden="true"> · </span>',
             'Exit status <strong>7</strong><span aria-hidden="true"> → </span>'),
        ]
        for old, new in cases:
            with self.subTest(mutation=new):
                try:
                    self.assertIn(old, original)
                    path.write_text(original.replace(old, new, 1))
                    result = self.check()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('application result must separate', result.stderr)
                finally:
                    path.write_text(original)

    def test_clarity_has_no_arbitrary_character_limit(self):
        guide = self.public / 'docs/generics/index.html'
        before = guide.read_text()
        title = re.search(r'<title>(.*?)</title>', before)[1]
        description = re.search(r'<meta name="description" content="([^"]*)"', before)[1]
        longer_title = 'Generics in VOID — Reusing Typed Functions with Inferred Arguments and Concrete Type Requirements'
        longer_description = html.unescape(description) + ' The guide follows a larger-of-two-values function and explains why each concrete type must support the comparison it uses.'
        try:
            guide.write_text(before.replace(title, html.escape(longer_title, quote=True)).replace(description, html.escape(longer_description, quote=True)))
            result = self.check()
            self.assertEqual(result.returncode, 0, result.stderr)
        finally:
            guide.write_text(before)


if __name__ == '__main__':
    unittest.main()
