#!/usr/bin/env python3
"""Check static references, metadata and authentic examples using Python stdlib."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit, unquote
import sys
import json
import re

ROOT = Path(__file__).resolve().parents[1]
PUBLIC = ROOT / 'public'
REPO = ROOT.parent
errors = []
manifest = json.loads((ROOT / 'scripts/guide-examples.json').read_text())
samples = {'/' + row['source'].removeprefix('public/'): row for row in manifest}
for row in manifest:
    if row.get('maintenance') not in ('teaching', 'repository-copy'):
        errors.append(f'Unknown example maintenance policy: {row["source"]}')
    if row.get('maintenance') == 'repository-copy':
        if (ROOT / row['source']).read_bytes() != (REPO / row['repository_source']).read_bytes():
            errors.append(f'Repository copy changed: {row["source"]}')
class Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids = set()
        self.links = []
        self.tags = []
        self.examples = []
        self.current_example = None
        self.results = []
        self.current_result = None
    def handle_starttag(self, tag, attrs):
        data = dict(attrs)
        self.tags.append((tag, data))
        if tag == 'div' and 'guide-result' in data.get('class', '').split():
            self.current_result = [data, '']
        if tag == 'code' and 'data-source' in data:
            self.current_example = [data['data-source'], '']
        if 'id' in data:
            if data['id'] in self.ids:
                errors.append(f'Duplicate id: {data["id"]}')
            self.ids.add(data['id'])
        for key in ('href', 'src'):
            if key in data:
                self.links.append(data[key])
    def handle_data(self, data):
        if self.current_result is not None:
            self.current_result[1] += data
        if self.current_example is not None:
            self.current_example[1] += data
    def handle_endtag(self, tag):
        if tag == 'div' and self.current_result is not None:
            self.results.append(self.current_result)
            self.current_result = None
        if tag == 'code' and self.current_example is not None:
            self.examples.append(self.current_example)
            self.current_example = None
for path in PUBLIC.rglob('*.html'):

    page = Page()
    page.feed(path.read_text())
    if not any(tag == 'html' and data.get('lang') == 'en' for tag, data in page.tags):
        errors.append(f'{path.name}: missing lang')
    for source, displayed in page.examples:
        sample = PUBLIC / source.lstrip('/')
        if not sample.is_file() or sample.read_text().rstrip('\n') != displayed.rstrip('\n'):
            errors.append(f'{path}: displayed example differs from {source}')
    for data, displayed in page.results:
        row = samples.get(data.get('data-source'))
        expected = re.search(r'Input\s+(-?\d+).*?(?:Expected result|Result)\s+(-?\d+)', displayed, re.S)
        if not row or not expected or (int(expected[1]), int(expected[2])) != (row['input'], row['expected']):
            errors.append(f'{path}: displayed input/result differs from validation manifest')
        elif (data.get('data-input'), data.get('data-expected')) != (str(row['input']), str(row['expected'])):
            errors.append(f'{path}: result metadata differs from validation manifest')
    for url in page.links:

        parts = urlsplit(url)
        if parts.scheme or parts.netloc:
            marker = '/blob/main/'
            if url.startswith('https://github.com/Shmellyorc/Void.Language' + marker):
                local = REPO / unquote(parts.path.split(marker, 1)[1])
                if not local.is_file(): errors.append(f'Missing repository guide: {url}')
            continue
        target = PUBLIC / unquote(parts.path.lstrip('/')) if parts.path.startswith('/') else path.parent / unquote(parts.path)
        if target.is_dir(): target = target / 'index.html'
        if parts.path and not target.exists(): errors.append(f'Missing asset: {url}')
        if parts.fragment:
            linked = Page()
            if parts.path and target.is_file(): linked.feed(target.read_text())
            else: linked = page
            if parts.fragment not in linked.ids: errors.append(f'Missing anchor: {url}')
    text = path.read_text()
    if re.search(r'clanker|owner review|hosted acceptance|handoff|stage [0-9]', text, re.I):
        errors.append(f'{path}: internal workflow language in public content')
    if any(urlsplit(url).path.endswith('.md') for url in page.links):
        errors.append(f'{path}: public explanation links to internal Markdown')
    if path.name == 'index.html':
        for key in ('description', 'viewport', 'twitter:card'):
            if not any(tag == 'meta' and data.get('name') == key for tag, data in page.tags):
                errors.append(f'Missing metadata: {key}')
        if sum(tag == 'h1' for tag, data in page.tags) != 1:
            errors.append(f'{path}: expected one page heading')
        if 'VOID 2.0' in path.read_text(): errors.append(f'{path}: outdated branding')
for route in ('why-void','features','examples','docs','getting-started'):
    if not (PUBLIC/route/'index.html').is_file(): errors.append(f'Missing page: {route}')
for route in ('syntax','variables','types','functions','control-flow','collections','structs','errors'):
    if not (PUBLIC/'docs'/route/'index.html').is_file(): errors.append(f'Missing guide: {route}')
getting_started = (PUBLIC/'getting-started/index.html').read_text()
if 'There are no public builds available yet.' not in getting_started or 'build/debug' in getting_started:
    errors.append('Getting started must describe development status without install steps')
for name in ('architecture',):
    if (PUBLIC/'examples'/f'{name}.void').read_bytes() != (REPO/'examples'/f'{name}.void').read_bytes():
        errors.append(f'{name}: downloaded example differs from repository')
if errors:
    print('\n'.join(errors), file=sys.stderr)
    raise SystemExit(1)
print('PASS: local links, repository guide targets, anchors, metadata, page headings, routes, branding, source parity and expected-result parity')
