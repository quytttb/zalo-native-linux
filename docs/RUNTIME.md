# Runtime handling

## Sources

The official Zalo Linux desktop build ships as an Electron ASAR archive
(`app.asar` plus an adjacent `app.asar.unpacked/` directory for native
modules). `RuntimeLocator::candidateAsars` searches, in order:

1. The path itself, when it already points at an `.asar` file.
2. `<root>/resources/app.asar`
3. `<root>/AppDir/resources/app.asar` (extracted AppImage)
4. `<root>/app.asar`
5. `<root>/runtime/resources/app.asar`
6. `<root>/usr/share/zalo/resources/app.asar`

Default root is `$ZALO_RUNTIME_DIR`, else
`~/.local/share/zalo-native/runtime`. Pass `--runtime <path>` to override.

## ASAR layout (what the parser implements)

```
u32 @0   4                  size of next field
u32 @4   H                  header pickle length
u32 @8   payload length     (JSON length + 4)
u32 @12  J                  JSON header length
bytes 16..16+J              header JSON: {"files": {...}}
data base 8+H               concatenated file payloads
```

Entries are directories (`{"files": {...}}`), links (`{"link": "..."}`), or
files (`{"size": N, "offset": "N", "unpacked": bool}`).
`unpacked` files live in `<archive>.unpacked/<path>`.

Hardening: 256 MiB header cap, 500k node cap, 64-level tree depth cap,
8-level link depth cap, unsafe-name rejection (`..`, slashes in names), strict
bounds checks on every payload, size cap on reads.

## Manifest

`RuntimeManifest::fromArchive` extracts name/version, Electron version,
main entry, first known HTML entry (`index.html`, `login.html`, `dist/…`,
…), and native `.node` module names — plus warnings for anything missing.
It never *executes* runtime code.

## Serving (M2)

`LocalScheme::resolve(rootDir, url)` maps `zalo-app://…` to a canonical file
below the runtime root, enforcing directory containment and regular-file-only
serving. The QML host also disables remote/local file access flags.
