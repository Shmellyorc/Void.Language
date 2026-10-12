const {buildSync} = require('esbuild');
buildSync({entryPoints: ['src/extension.ts'], outfile: 'dist/extension.js', bundle: true,
  platform: 'node', format: 'cjs', target: 'node20', external: ['vscode'], legalComments: 'eof'});
// Include full runtime dependency licenses in the distributable, not only headers.
const fs = require('node:fs');
const path = require('node:path');
const lock = require('./package-lock.json');
const notices = [];
for (const [folder, metadata] of Object.entries(lock.packages)) {
  if (!folder || metadata.dev) continue;
  const license = ['LICENSE', 'License.txt', 'LICENSE.md', 'LICENSE.txt', 'license', 'license.md']
    .find(name => fs.existsSync(path.join(folder, name)));
  if (!license) throw new Error('Missing runtime dependency license: ' + folder);
  notices.push(`${folder} ${metadata.version}\n${fs.readFileSync(path.join(folder, license), 'utf8')}`);
}
fs.writeFileSync('dist/THIRD_PARTY_NOTICES.txt', notices.join('\n\n--------------------\n\n'));
