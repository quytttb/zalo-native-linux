# Runtime handling

**Historical, optional tooling.** The earlier architecture investigated a
user-supplied Zalo PC renderer in Qt WebEngine. As of 2026-11-02 the chosen
backend is direct Qt/C++; runtime detection/import below are retained research,
not required for the default app. See [PLAN.md](PLAN.md).

Latest measured inventory and bootstrap output: [RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md).

## Sources and provenance

Use a Zalo desktop runtime obtained through legitimate vendor channels. The
project does not download or redistribute it. Record the source, version and
hash when importing.

The Linux AppImage examined on this machine is a **community port**, not an
official Zalo Linux release. Its launcher lives in `AppDir/resources/app.asar`,
while the extracted application is `AppDir/app/` (`pc-dist/index.html`,
`pc-dist/login.html`, `main-dist/`, native dependencies and `package.json.bak`).
The launcher ASAR alone has no renderer HTML. A local tree hash proves only that
a copy is unchanged; `sourceVerified` remains `false` without a vendor signature
or trusted official hash.

## Detection order

`RuntimeLocator::locate` prefers an extracted application directory, then falls
back to ASAR files.

App-directory candidates, in order:

1. the path itself, when it already looks like an application directory;
2. `<root>/app`;
3. `<root>/AppDir/app` (extracted AppImage);
4. `<root>/runtime/app`;
5. `<root>/usr/share/zalo/app`.

ASAR candidates, in order:

1. the path itself, when it is an `.asar` file;
2. `<root>/resources/app.asar`;
3. `<root>/AppDir/resources/app.asar`;
4. `<root>/app.asar`;
5. `<root>/runtime/resources/app.asar`;
6. `<root>/usr/share/zalo/resources/app.asar`.

Default root is `$ZALO_RUNTIME_DIR`, else
`~/.local/share/zalo-native/runtime`. Pass `--runtime <path>` to override.
The native app shell reads this for diagnostics; it does not execute the core.

An application directory is recognised by renderer/build artefacts such as
`pc-dist/index.html`, `main-dist/preload-*.js`, `native/` libraries,
`pc-dist/build-info.json` and a launcher ASAR warning. This is structural
detection, not an execution or authenticity verdict.

## Manifest and static inventory

`RuntimeManifest` records:

- source kind, app directory, launcher ASAR and runtime root;
- name, app/build version, Electron version (when present), build commit/date;
- main entry, entry HTML, native modules and native library files;
- exposed preload APIs, synchronous call sites, Linux IPC channels;
- warnings and the launcher-only distinction.

`isRuntimeCore()` is true only when an entry document was found.
`isLauncherOnly()` identifies a parseable launcher ASAR with no hostable HTML.
Inspection never executes runtime code.

The preload scan is static. It can list names and call sites, but it cannot prove
that an API is essential, safely hostable, or sufficient for login. A measured
`26.10.20` app directory exposed 46 names, 21 synchronous call sites (19 with
computed channels), 12 Linux channels and 15 native library files.

## Import and verification

```bash
./build-web/src/cli/zalort inspect /path/to/AppDir/app

./build-web/src/cli/zalort import /path/to/AppDir/app \
  "$HOME/.local/share/zalo-native/runtime" \
  --expected-version 26.10.20 \
  --expected-sha256 <tree-sha256>

./build-web/src/cli/zalort verify "$HOME/.local/share/zalo-native/runtime"
```

Import behaviour:

- copies into a dedicated directory and writes `runtime.json`;
- computes per-file and deterministic tree SHA-256 values;
- optionally rejects a version/hash mismatch;
- refuses to overwrite a non-empty directory unless it contains a previous
  `runtime.json` and `--overwrite` is passed;
- rejects symlinked directories and symlinks escaping the source; internal file
  symlinks are copied as file content;
- enforces file-count and total-byte limits during the copy.

`verify` re-hashes the directory and compares it with `runtime.json`. It detects
local modification/corruption, not vendor authenticity or host compatibility.

## ASAR layout (parser support)

```text
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

`zalort check-unpacked /path/to/app.asar` compares each unpacked file's size and
SHA-256 with the metadata in the archive. After a successful archive open, it
outputs counts only and exits 2 if a comparison cannot be verified or differs;
an archive-open failure exits 1. It does not verify packed entries, macOS
signatures or vendor authenticity. Local modifications can race the check.

Hardening: 256 MiB header cap, 500k node cap, 64-level tree depth cap,
8-level link depth cap, unsafe-name rejection (`..`, slashes in names), strict
bounds checks on every payload, size cap on reads.

## Serving and bootstrap (M2, not complete)

`LocalScheme::resolve(rootDir, url)` maps `zalo-app://…` to a canonical file
below the runtime root, enforcing directory containment and regular-file-only
serving. `zalo-bootstrap-probe` registers a `zalo-app` scheme handler and loads
the entry HTML in an offscreen `QWebEnginePage`; host injection is off by default
and only enabled by the diagnostic `--stub-preload`/`--host-api` modes.

Probe result: the document loads, but the runtime's Content Security Policy
initially refused its own local stylesheets and scripts under the custom scheme.
Omitting the `LocalScheme` flag did not change this. A scoped, in-memory rewrite
that adds only the `zalo-app:` scheme to CSP fetch directives let the scripts and
workers run without a global bypass; `RuntimePagePolicy::allowSchemeInCsp`
implements it and has synthetic tests. External origins remain restricted and
the runtime file on disk is not modified.

With the scoped rewrite, the next blockers are preload members:
`$zlogger.loadShellQos` and `$zwindow`. The diagnostic `--stub-preload` mode then
shows startup enumerating the preload surface and calling `$zelectron.getPath`,
`$znode.path`/`fs`/`fsExtra` and telemetry APIs; that mode is contract discovery,
not a working bridge. `--host-api` injects the bounded `renderer/host/preload.js`
against an isolated data root; it served 22 host calls in the latest recorded probe, but a remaining
dependency-injection failure shows that the subset is not a complete preload bridge. IndexedDB opens successfully in the `zalo-app://` origin;
the runtime's `do open indexeddb ...` lines are its own wrapper logging. No remote
host was attempted during the observation window. `localStorage` and IndexedDB
APIs are available, but persistence/quota/SQLite behaviour is unverified.

The production host still needs:

- integration and security review of the CSP rewrite, plus a clear origin/frame
  policy;
- a bounded preload/IPC bridge injected before bootstrap, including a safe
  solution for synchronous Electron calls (QWebChannel alone is asynchronous);
- consistent TLS/cookie/network/storage behaviour and Linux native services;
- explicit capability checks, a separate profile and manual login.

Do not import session data from another Zalo installation or the archived QR
probe. See [PLAN.md](PLAN.md), [BRIDGE.md](BRIDGE.md) and [LEGAL.md](LEGAL.md).
