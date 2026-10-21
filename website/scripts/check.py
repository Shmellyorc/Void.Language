#!/usr/bin/env python3
"""Validate static SEO, crawlable links and teaching-example parity with stdlib."""
import argparse
from collections import deque
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import struct
import sys
from urllib.parse import unquote, urljoin, urlsplit
from urllib.robotparser import RobotFileParser
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
SITE = 'https://voidlang.com'
SOCIAL_IMAGE = SITE + '/assets/social.png'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=Path, default=ROOT / 'public',
                    help='Public directory to check; use website/dist to verify built output')
args = parser.parse_args()
PUBLIC = args.directory.resolve()
errors = []


def fail(path, message):
    errors.append(f'{path}: {message}')


def normalized(text):
    return ' '.join(text.split()).casefold()


class Page(HTMLParser):
    def __init__(self, path):
        super().__init__(convert_charrefs=True)
        self.path = path
        self.tags = []
        self.ids = set()
        self.links = []
        self.meta = {}
        self.canonicals = []
        self.titles = []
        self.headings = []
        self.anchors = []
        self.examples = []
        self.results = []
        self.structured = []
        self.outputs = []
        self.window_downloads = []
        self.in_code_window = False
        self.output = None
        self.in_head = False
        self.capture = self.anchor = self.example = self.result = self.script = None

    def handle_starttag(self, tag, attrs):
        data = dict(attrs)
        self.tags.append((tag, data))
        if tag == 'figure' and 'guide-code' in data.get('class', '').split():
            self.in_code_window = True
        if tag == 'head':
            self.in_head = True
        if tag == 'meta' and self.in_head:
            key = data.get('name', data.get('property', '')).casefold()
            self.meta.setdefault(key, []).append(data.get('content', ''))
        if tag == 'link' and 'canonical' in data.get('rel', '').casefold().split():
            if not self.in_head:
                fail(self.path, 'canonical must be in the HTML head')
            self.canonicals.append(data.get('href', ''))
        if tag == 'title' or re.fullmatch(r'h[1-6]', tag):
            if self.capture is not None:
                fail(self.path, f'unclosed heading/title before {tag}')
            self.capture = [tag, '']
        if tag == 'br' and self.capture:
            self.capture[1] += ' '
        if tag == 'br' and self.output:
            self.output[1] += '\n'
        if tag == 'script':
            self.script = [data, '']
        if tag == 'a':
            self.anchor = [data, '']
        if tag == 'img':
            if 'alt' not in data:
                fail(self.path, 'image missing alt attribute')
            elif not data['alt'].strip() and data.get('role') != 'presentation' and data.get('aria-hidden') != 'true':
                fail(self.path, 'empty image alt must be explicitly decorative')
            if self.anchor:
                self.anchor[1] += data.get('alt', '')
        if 'id' in data:
            if data['id'] in self.ids:
                fail(self.path, f'duplicate id {data["id"]}')
            self.ids.add(data['id'])
        if tag == 'code' and 'data-source' in data:
            self.example = [data['data-source'], '']
        if tag == 'code' and 'data-result-for' in data:
            self.output = [data, '']
        if tag == 'div' and 'guide-result' in data.get('class', '').split():
            self.result = [data, '']
        for key in ('href', 'src'):
            if key in data:
                self.links.append(data[key])
        if tag == 'meta' and data.get('http-equiv', '').casefold() == 'refresh':
            fail(self.path, 'HTML refresh redirects are not appropriate here')

    def handle_data(self, data):
        for capture in (self.capture, self.anchor, self.example, self.result, self.script, self.output):
            if capture is not None:
                capture[1] += data

    def handle_endtag(self, tag):
        if (tag == 'title' or re.fullmatch(r'h[1-6]', tag)) and self.capture and tag != self.capture[0]:
            fail(self.path, f'mismatched heading/title closing tag {tag}')
        if self.capture and tag == self.capture[0]:
            (self.titles if tag == 'title' else self.headings).append(self.capture[1] if tag == 'title' else tuple(self.capture))
            self.capture = None
        if tag == 'head':
            self.in_head = False
        if tag == 'figure':
            self.in_code_window = False
        if tag == 'a' and self.anchor is not None:
            if self.in_code_window and 'download' in self.anchor[0]:
                self.window_downloads.append(self.anchor)
            self.anchors.append(self.anchor)
            self.anchor = None
        if tag == 'code' and self.example is not None:
            self.examples.append(self.example)
            self.example = None
        if tag == 'code' and self.output is not None:
            self.outputs.append(self.output)
            self.output = None
        if tag == 'div' and self.result is not None:
            self.results.append(self.result)
            self.result = None
        if tag == 'script' and self.script is not None:
            if self.script[0].get('type', '').casefold() == 'application/ld+json':
                self.structured.append(self.script[1])
            self.script = None


def one_meta(page, key):
    values = page.meta.get(key, [])
    if len(values) != 1 or not values[0].strip():
        fail(page.path, f'expected one nonempty {key} meta tag')
        return ''
    return values[0].strip()


def page_route(path):
    relative = path.relative_to(PUBLIC)
    if path.name == 'index.html':
        return '/' if relative.parent == Path('.') else '/' + relative.parent.as_posix() + '/'
    return '/' + relative.as_posix()


def is_site_url(value, page_url=False):
    parsed = urlsplit(value)
    return (parsed.scheme == 'https' and parsed.netloc == 'voidlang.com'
            and not parsed.query and not parsed.fragment
            and parsed.path.startswith('/') and '//' not in parsed.path
            and (not page_url or parsed.path.endswith('/')))


def local_target(path):
    target = (PUBLIC / unquote(path).lstrip('/')).resolve()
    if not target.is_relative_to(PUBLIC):
        return None
    return target / 'index.html' if target.is_dir() else target


if not PUBLIC.is_dir():
    raise SystemExit(f'Missing public directory: {PUBLIC}')
pages = {}
for path in sorted(PUBLIC.rglob('*.html')):
    page = Page(path)
    page.feed(path.read_text())
    pages[path.resolve()] = page
indexable = {path: page for path, page in pages.items() if path != PUBLIC / '404.html'}
expected_urls = {SITE + page_route(path) for path in indexable}
if not indexable:
    fail(PUBLIC, 'no indexable pages found')

manifest = json.loads((ROOT / 'scripts/guide-examples.json').read_text())
samples = {'/' + row['source'].removeprefix('public/'): row for row in manifest}
for row in manifest:
    source = PUBLIC / row['source'].removeprefix('public/')
    if not source.is_file():
        fail(source, 'missing downloadable example')
        continue
    if row.get('maintenance') not in ('teaching', 'repository-copy'):
        fail(source, 'unknown example maintenance policy')
    if row.get('mode') == 'app':
        if (type(row.get('expected_status')) is not int
                or not 0 <= row['expected_status'] <= 255
                or row.get('expected_stdout') != ''
                or any(key in row for key in ('input', 'expected', 'cases'))
                or row.get('kind') == 'fragment'):
            fail(source, 'application manifest requires separate status and empty stdout, without adapter inputs')
    elif type(row.get('input')) is not int or type(row.get('expected')) is not int:
        fail(source, 'entry adapter manifest requires integer input and result')
    if row.get('maintenance') == 'repository-copy':
        original = REPO / row['repository_source']
        if not original.is_file() or source.read_bytes() != original.read_bytes():
            fail(source, 'repository copy differs from its original')

# Documentation owns its samples; existing Examples-page rows remain independent.
doc_samples = [row for row in manifest if row.get('guide')]
doc_sources = {}
for row in doc_samples:
    source = '/' + row['source'].removeprefix('public/')
    if (not source.startswith('/docs/snippets/') or row.get('maintenance') != 'teaching'
            or row.get('kind') not in ('program', 'fragment')):
        fail(source, 'documentation sample needs an owned path and explicit program/fragment kind')
    if source in doc_sources:
        fail(source, 'duplicate documentation sample registration')
    doc_sources[source] = row
    if row.get('kind') == 'fragment':
        if not row.get('context_before') and not row.get('context_after'):
            fail(source, 'fragment requires an explicit executable validation context')
        if row.get('context_indent', 0) not in (0, 4):
            fail(source, 'unsupported fragment context indentation')
        if row.get('mode') == 'async':
            fail(source, 'async teaching examples must remain complete adapter programs')
    for support in row.get('support', []):
        url = '/' + support.removeprefix('public/')
        if not url.startswith('/docs/snippets/') or url in doc_sources:
            fail(url, 'invalid or duplicate documentation support module')
        doc_sources[url] = row
actual_doc_sources = {'/' + path.relative_to(PUBLIC).as_posix()
                      for path in (PUBLIC / 'docs/snippets').rglob('*.void')}
if set(doc_sources) != actual_doc_sources:
    fail(PUBLIC / 'docs/snippets', 'documentation source inventory differs from manifest')


def source_tokens(text):
    # Ignore formatting/comments for duplicate detection, preserve string contents.
    return tuple(token for token in re.findall(r'"(?:\\.|[^"\\])*"|\#[^\n]*|\w+|[^\s]', text)
                 if not token.startswith('#'))


example_programs = {source_tokens(path.read_text())
                    for path in (PUBLIC / 'examples').rglob('*.void')}
for source in doc_sources:
    path = local_target(source)
    if path is None or not path.is_file():
        fail(source, 'missing documentation source')
    elif source_tokens(path.read_text()) in example_programs:
        fail(source, 'documentation duplicates an Examples-page program')

# One graph built from ordinary HTML anchors; JavaScript is not involved.
graph = {path: set() for path in indexable}
titles = {}
descriptions = {}
structured_count = 0
placeholder = re.compile(r'\b(?:lorem ipsum|todo|tbd|untitled|insert title|insert description|your page title)\b', re.I)
for path, page in pages.items():
    route = page_route(path)
    canonical = SITE + route
    html_tags = [data for tag, data in page.tags if tag == 'html']
    if len(html_tags) != 1 or html_tags[0].get('lang') != 'en':
        fail(path, 'expected one html element with lang="en"')
    viewport = one_meta(page, 'viewport')
    if 'width=device-width' not in viewport.replace(' ', '') or not re.search(r'initial-scale\s*=\s*1(?:\.0)?(?:\s*,|\s*$)', viewport):
        fail(path, 'mobile viewport must use device width and initial-scale=1')
    if len(page.titles) != 1 or not page.titles[0].strip():
        fail(path, 'expected one nonempty title')
        title = ''
    else:
        title = page.titles[0].strip()
        titles.setdefault(normalized(title), []).append(path)
        if placeholder.search(title) or normalized(title) in {'void', 'void language', 'home', 'document'}:
            fail(path, 'title is a placeholder or does not describe its page')
    description = one_meta(page, 'description')
    if description:
        descriptions.setdefault(normalized(description), []).append(path)
        if placeholder.search(description) or normalized(description) in {'void', 'void language', 'description', normalized(title)}:
            fail(path, 'description is a placeholder or just repeats its title')
    if len([text for tag, text in page.headings if tag == 'h1' and text.strip()]) != 1 or sum(tag == 'h1' for tag, _ in page.headings) != 1:
        fail(path, 'expected exactly one meaningful H1')
    previous = 0
    for tag, text in page.headings:
        level = int(tag[1])
        if not text.strip() or level > previous + 1:
            fail(path, f'empty heading or skipped heading level at {tag}: {text}')
        previous = level
    directives = {token.strip().casefold() for key in ('robots', 'googlebot') for value in page.meta.get(key, []) for token in value.split(',')}
    if path in indexable:
        if path.name != 'index.html':
            fail(path, 'indexable pages must use directory/index.html for clean slash URLs')
        if page.canonicals != [canonical] or not is_site_url(canonical, page_url=True):
            fail(path, f'canonical must be exactly {canonical}')
        if {'noindex', 'none', 'nofollow'} & directives:
            fail(path, 'unintended indexing/crawling restriction')
        for key, value in [('og:url', canonical), ('og:title', title), ('og:description', description),
                           ('og:site_name', 'VOID Language'), ('og:type', 'website'),
                           ('twitter:card', 'summary_large_image'), ('twitter:title', title),
                           ('twitter:description', description)]:
            if one_meta(page, key) != value:
                fail(path, f'{key} does not match this page')
        for key in ('og:image', 'twitter:image'):
            image_url = one_meta(page, key)
            if image_url != SOCIAL_IMAGE or not is_site_url(image_url):
                fail(path, f'{key} must reference the absolute primary-host social image')
            image_path = local_target(urlsplit(image_url).path)
            if image_path is None or not image_path.is_file() or not image_path.stat().st_size:
                fail(path, f'missing or empty social preview asset: {image_url}')
            elif image_path.suffix == '.png':
                header = image_path.read_bytes()[:24]
                if len(header) != 24 or header[:8] != b'\x89PNG\r\n\x1a\n' or header[12:16] != b'IHDR':
                    fail(path, 'social preview is not a readable PNG')
                elif key == 'og:image':
                    width, height = struct.unpack('>II', header[16:24])
                    if (one_meta(page, 'og:image:width'), one_meta(page, 'og:image:height')) != (str(width), str(height)):
                        fail(path, 'social image dimensions do not match the file')
        for key in ('og:image:alt', 'twitter:image:alt'):
            if not one_meta(page, key):
                fail(path, 'social preview needs descriptive image alt metadata')
    else:
        if 'noindex' not in directives:
            fail(path, '404 must explicitly use noindex')
        if page.canonicals:
            fail(path, '404 must not advertise another page as its canonical')
    for source, displayed in page.examples:
        sample = local_target(source)
        if sample is None or not sample.is_file() or sample.read_text().rstrip('\n') != displayed.rstrip('\n'):
            fail(path, f'displayed source differs from {source}')
    if path.parent.parent == PUBLIC / 'docs' and path.name == 'index.html':
        guide = path.parent.name
        expected = {source for source, row in doc_sources.items() if row['guide'] == guide}
        displayed_sources = [source for source, _ in page.examples]
        if any(not source.startswith('/docs/snippets/') for source in displayed_sources):
            fail(path, 'documentation code must use documentation-owned sources')
        if set(displayed_sources) != expected or len(displayed_sources) != len(set(displayed_sources)):
            fail(path, 'documentation display/manifest coverage mismatch')
        downloads = {attrs.get('href') for attrs, _ in page.anchors if 'download' in attrs}
        if not expected <= downloads:
            fail(path, 'documentation source needs a matching download link')
        if guide != 'native-integration' and not expected:
            fail(path, 'guide lacks an independently verified documentation sample')
        for source in expected:
            row = doc_sources[source]
            if row.get('kind') == 'fragment' and 'fragment' not in path.read_text().casefold():
                fail(path, 'illustrative fragment must be identified for readers')
        expected_results = {source for source, row in doc_sources.items()
                            if row['guide'] == guide and row.get('kind') == 'program'
                            and source == '/' + row['source'].removeprefix('public/')}
        if {data.get('data-source') for data, _ in page.results} != expected_results:
            fail(path, 'only complete programs should advertise standalone input/results')
    for data, displayed in page.outputs:
        source = data.get('data-result-for')
        row = samples.get(source)
        if not row or not row.get('showcase'):
            fail(path, 'terminal output requires a registered showcase program')
            continue
        expected_output = f'$ voidc run {Path(source).name} {row["input"]} {row["expected"]}'
        if (normalized(displayed) != normalized(expected_output)
                or data.get('data-input') != str(row['input'])
                or data.get('data-expected') != str(row['expected'])):
            fail(path, 'showcase terminal command/result differs from validation manifest')
    for data, displayed in page.results:
        row = samples.get(data.get('data-source'))
        if row and row.get('mode') == 'app':
            result = re.fullmatch(r'\s*Exit status\s+(\d+)\s+·\s+Standard output empty\s*', displayed)
            if (not result or int(result[1]) != row['expected_status']
                    or data.get('data-mode') != 'app'
                    or data.get('data-expected-status') != str(row['expected_status'])
                    or data.get('data-expected-stdout') != row['expected_stdout']
                    or 'data-input' in data or 'data-expected' in data):
                fail(path, 'application result must separate exit status and stdout without an adapter input')
            continue
        result = re.fullmatch(r'\s*Input\s+(-?\d+)\s+·\s+(?:Expected result|Result)\s+(-?\d+)\s*', displayed)
        if not result:
            fail(path, 'code-window result bar must use one U+00B7 middle-dot separator')
            continue
        if not row or (int(result[1]), int(result[2])) != (row['input'], row['expected']):
            fail(path, 'displayed input/result differs from validation manifest')
        elif (data.get('data-input'), data.get('data-expected')) != (str(row['input']), str(row['expected'])):
            fail(path, 'result metadata differs from validation manifest')
    for attrs, text in page.window_downloads:
        source = attrs.get('href')
        row = doc_sources.get(source, samples.get(source, {}))
        expected_label = 'Download fragment' if row.get('kind') == 'fragment' else 'Download source'
        if text.strip() != expected_label:
            fail(path, f'code-window download label must be {expected_label!r}, without decorations')
    for attrs, text in page.anchors:
        if not (text.strip() or attrs.get('aria-label', '').strip() or attrs.get('title', '').strip()):
            fail(path, 'link has no descriptive text or accessible name')
    for reference in page.links:
        if not reference.strip():
            fail(path, 'empty asset/link reference')
            continue
        url = urlsplit(urljoin(canonical, reference))
        if url.netloc == 'voidlang.net':
            fail(path, 'internal link uses redirecting secondary domain')
        if url.netloc != 'voidlang.com':
            marker = '/blob/main/'
            if reference.startswith('https://github.com/Shmellyorc/Void.Language' + marker):
                original = REPO / unquote(url.path.split(marker, 1)[1])
                if not original.is_file():
                    fail(path, f'missing repository reference {reference}')
            if url.path.endswith('.md'):
                fail(path, 'public explanation links to internal Markdown')
            continue
        if url.scheme != 'https':
            fail(path, f'internal reference must use HTTPS: {reference}')
        target = local_target(url.path)
        if target is None or not target.is_file():
            fail(path, f'broken local reference {reference}')
            continue
        if target in pages:
            target_route = page_route(target)
            if target in indexable and url.path != target_route:
                fail(path, f'link requires a redirect; use {target_route}')
            if url.fragment and unquote(url.fragment) not in pages[target].ids:
                fail(path, f'broken fragment {reference}')
            if path in graph and target in indexable and reference in [a.get('href') for a, _ in page.anchors]:
                graph[path].add(target)
    content = path.read_text()
    if re.search(r'clanker|owner review|hosted acceptance|handoff|stage [0-9]', content, re.I):
        fail(path, 'internal workflow language in public content')
    if 'VOID 2.0' in content:
        fail(path, 'outdated public branding')
    for raw in page.structured:
        structured_count += 1
        try:
            data = json.loads(raw)
        except (ValueError, TypeError) as error:
            fail(path, f'invalid JSON-LD: {error}')
            continue
        allowed = {'@context', '@type', '@id', 'name', 'alternateName', 'url', 'inLanguage'}
        if not isinstance(data, dict) or set(data) - allowed:
            fail(path, 'unsupported/unreviewed structured-data fields')
        elif (path != PUBLIC / 'index.html' or data.get('@context') != 'https://schema.org'
              or data.get('@type') != 'WebSite' or data.get('@id') != SITE + '/#website'
              or data.get('url') != SITE + '/' or data.get('name') != 'VOID Language'
              or data.get('alternateName') != 'VOID' or data.get('inLanguage') != 'en'):
            fail(path, 'WebSite JSON-LD does not match the visible site identity')
    if path == PUBLIC / 'index.html' and len(page.structured) != 1:
        fail(path, 'homepage must contain one reviewed WebSite JSON-LD block')

for label, values in [('title', titles), ('description', descriptions)]:
    for value, paths in values.items():
        if len(paths) > 1:
            fail(PUBLIC, f'duplicate {label} on {", ".join(str(p.relative_to(PUBLIC)) for p in paths)}')

sitemap_path = PUBLIC / 'sitemap.xml'
try:
    tree = ET.parse(sitemap_path)
    namespace = '{http://www.sitemaps.org/schemas/sitemap/0.9}'
    if tree.getroot().tag != namespace + 'urlset':
        fail(sitemap_path, 'expected sitemap urlset namespace')
    sitemap_urls = []
    for node in tree.getroot():
        locations = node.findall(namespace + 'loc')
        if node.tag != namespace + 'url' or len(locations) != 1 or not locations[0].text:
            fail(sitemap_path, 'invalid or missing sitemap URL entry')
            continue
        url = locations[0].text.strip()
        sitemap_urls.append(url)
        if not is_site_url(url, page_url=True) or url not in expected_urls:
            fail(sitemap_path, f'invalid, noncanonical or nonindexable URL {url}')
    if len(sitemap_urls) != len(set(sitemap_urls)):
        fail(sitemap_path, 'duplicate sitemap URLs')
    if set(sitemap_urls) != expected_urls:
        fail(sitemap_path, f'coverage mismatch: missing={sorted(expected_urls-set(sitemap_urls))}; extra={sorted(set(sitemap_urls)-expected_urls)}')
except (OSError, ET.ParseError) as error:
    fail(sitemap_path, f'unreadable sitemap: {error}')

robots_path = PUBLIC / 'robots.txt'
try:
    robots = robots_path.read_text()
    declarations = re.findall(r'^\s*Sitemap\s*:\s*(\S+)\s*$', robots, re.M | re.I)
    if declarations != [SITE + '/sitemap.xml']:
        fail(robots_path, 'expected one correct absolute Sitemap reference')
    rules = RobotFileParser()
    rules.parse(robots.splitlines())
    for url in sorted(expected_urls | {SOCIAL_IMAGE, SITE + '/assets/site.js', SITE + '/assets/style.css'}):
        if not rules.can_fetch('Googlebot', url) or not rules.can_fetch('*', url):
            fail(robots_path, f'intended public URL is blocked: {url}')
except OSError as error:
    fail(robots_path, str(error))

# Reject deploy headers that would inadvertently override the HTML indexing policy.
headers_path = PUBLIC / '_headers'
if headers_path.is_file():
    rule = None
    for line in headers_path.read_text().splitlines():
        if line and not line.startswith((' ', '\t', '#')):
            rule = line.strip()
        if re.match(r'\s*X-Robots-Tag\s*:', line, re.I) and re.search(r'\b(?:noindex|none)\b', line, re.I):
            if rule not in ('/404', '/404.html'):
                fail(headers_path, f'indexing restriction requires review: {rule}')

home = PUBLIC / 'index.html'
visited = set()
queue = deque([home])
while queue:
    path = queue.popleft()
    if path not in visited:
        visited.add(path)
        queue.extend(graph.get(path, set()) - visited)
for path in set(indexable) - visited:
    fail(path, 'not reachable from homepage through ordinary HTML links')

# Homepage and roadmap use the same content/source contracts as existing guides.
showcase = {'/' + row['source'].removeprefix('public/') for row in manifest if row.get('showcase')}
showcase_files = {'/' + path.relative_to(PUBLIC).as_posix()
                  for path in (PUBLIC / 'showcase').rglob('*.void')}
if showcase != showcase_files:
    fail(PUBLIC / 'showcase', 'showcase source inventory differs from manifest')
if home in pages:
    page = pages[home]
    if {source for source, _ in page.examples} != showcase:
        fail(home, 'homepage showcase display/manifest coverage mismatch')
    if len(page.outputs) != 1 or page.outputs[0][0].get('data-result-for') not in showcase:
        fail(home, 'homepage needs one verified hero terminal output')
    downloads = {attrs.get('href') for attrs, _ in page.anchors if 'download' in attrs}
    # The hero remains an editor/terminal presentation; its source is used in the
    # displayed command. Below-hero demos retain direct source downloads.
    hero_sources = {attrs.get('data-result-for') for attrs, _ in page.outputs}
    if not (showcase - hero_sources) <= downloads:
        fail(home, 'showcase programs need their matching download links')
    primary = [(attrs, text) for attrs, text in page.anchors
               if {'button', 'primary'} <= set(attrs.get('class', '').split())]
    if not primary or primary[0][0].get('href') != '/examples/' or primary[0][1].strip() != 'Explore VOID':
        fail(home, 'primary hero action must be Explore VOID without an arrow')
    if not any(attrs.get('href') == '/docs/' and normalized(text) == 'read the docs ↗'
               for attrs, text in page.anchors):
        fail(home, 'secondary hero action must retain Read the docs and its diagonal arrow')
    if 'roadmap' not in page.ids:
        fail(home, 'keep the legacy homepage roadmap teaser anchor')
for path, page in pages.items():
    roadmap_links = [attrs for attrs, text in page.anchors if text.strip() == 'Roadmap']
    if (path in indexable or roadmap_links) and (len(roadmap_links) != 1 or roadmap_links[0].get('href') != '/roadmap/'):
        fail(path, 'Roadmap navigation must point directly to /roadmap/')

for route in ('why-void', 'features', 'examples', 'docs', 'getting-started', 'roadmap'):

    if PUBLIC / route / 'index.html' not in pages:
        fail(PUBLIC, f'missing page {route}')
for route in ('syntax', 'variables', 'types', 'functions', 'control-flow', 'collections', 'structs', 'errors'):
    if PUBLIC / 'docs' / route / 'index.html' not in pages:
        fail(PUBLIC, f'missing guide {route}')
getting_started = PUBLIC / 'getting-started/index.html'
if getting_started.is_file():
    text = getting_started.read_text()
    if 'There are no public builds available yet.' not in text or 'build/debug' in text:
        fail(getting_started, 'must describe development status without install steps')
architecture = PUBLIC / 'examples/architecture.void'
if not architecture.is_file() or architecture.read_bytes() != (REPO / 'examples/architecture.void').read_bytes():
    fail(architecture, 'download differs from preserved repository example')

if errors:
    print('\n'.join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f'PASS: {len(indexable)} indexable pages + 404; unique metadata, canonicals, social assets, '
      f'{len(expected_urls)} sitemap URLs, robots, HTML reachability, {structured_count} JSON-LD block, '
      'heading hierarchy, links, source/result parity')
