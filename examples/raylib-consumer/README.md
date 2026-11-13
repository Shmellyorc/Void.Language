# Pinned raylib package consumer

Copy this directory outside the compiler checkout. After Boss pushes the package,
run `python3 configure.py --rev FULL_LOWERCASE_COMMIT_SHA`, then follow the
[package setup guide](../../verified-packages/raylib/README.md).
`--origin HTTPS_URL` supports the disposable real Git qualification. The manifest
imports `pkg/raylib` from `verified-packages/raylib`; native catalog approval stays
with the application and operator.

The default run checks real record ABI, image/PNG allocation, UTF-8/codepoints,
text arrays, compression/hashes, file results, headless WAV decoding, typed
format/log and all five retained global callbacks. Run from
a writable working directory: the file check creates and removes
`void-raylib-consumer-owned-test.txt` and refuses an existing file of that name.

Pass `--window` after `voidc run ... --` for public window, texture, material,
model/mesh, font/glyph, shader and framebuffer operations, then an interactive
gallery. Pass `--audio` for a real audio device, aliases, memory-backed music and
stream playback/cleanup, retained PCM generation, stream/mixed processors,
replacement and detach. Neither device mode is inferred from headless success.
Optional approved archive assets add file-backed fonts, atlas allocation,
skeletal animation and automation-list checks. D5 qualification packages those
inputs as ordinary application resources and loads them after relocation.

All 600 declarations have usable APIs: 236 direct C ABI mappings, 352 ownership
adapters, ten callback lifecycle adapters and two typed equivalents. Arbitrary C
varargs and concurrent third-party use of global hook setters remain unsupported.
Native renderer limitations and the
unsuppressed strict graphics/audio leak failures are documented in the
[ownership contract](../../verified-packages/raylib/OWNERSHIP.md) and
[qualification report](../../docs/projects/G23_G24_IMPLEMENTATION.md).
