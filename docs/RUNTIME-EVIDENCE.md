# Runtime evidence — imported app directory and bootstrap probe

**Date:** 2026-10-30
**Status:** diagnostic evidence only. This does **not** verify an official Linux
Zalo runtime, authenticate the vendor, log in, or prove host compatibility.

## Runtime under test

The only runtime available on this machine came from a community AppImage. It is
**not** an official Zalo Linux release and its provenance is unverified. No
official Zalo PC installer was available, so the source-selection item in
[M1](TODO.md) remains open.

```text
app version        26.10.20
build commit       12ef36d
build date         2026-10-29T15:57:45.868Z
source kind        app-dir (extracted application directory)
entry HTML         pc-dist/index.html
main entry         bootstrap.js
native libraries   15 (Linux, macOS and Windows variants listed statically)
preload APIs       46 exposed names
sync call sites    21 (2 literal channels, 19 computed channels)
Linux IPC channels 12
```

Imported copy:

```text
~/.local/share/zalo-native/runtime
files             13,369
bytes             255,047,692
tree SHA-256      f46e7998416732498476f6134234edf0d20c3b75fe1d8e54ef5d7dd84a3d2a3a
sourceVerified    false
```

Commands used:

```bash
# Inspect without executing runtime code.
./build-web/src/cli/zalort inspect /path/to/AppDir/app

# Copy into an isolated directory, pinning version and tree hash.
./build-web/src/cli/zalort import /path/to/AppDir/app \
  "$HOME/.local/share/zalo-native/runtime" \
  --expected-version 26.10.20 \
  --expected-sha256 f46e7998416732498476f6134234edf0d20c3b75fe1d8e54ef5d7dd84a3d2a3a

# Re-hash the copy and compare it with runtime.json.
./build-web/src/cli/zalort verify "$HOME/.local/share/zalo-native/runtime"
```

`verify` proves local integrity against `runtime.json` only. It cannot prove
that the files came from Zalo.

## Preload inventory (static scan)

The scan finds these notable exposed names:

```text
$zapp $zlogin $zdb $zsqlite $zsqlitebw $znetwork $znoti $zcall
$znode $zelectron $zelectronNative $zFileManager $zsessionManager
$zsharedWorker $zupdater $zscreencap __SENTRY_IPC__
__zaloFlatpakTransfer __zaloOfflineStorage MetriczGlobal
```

It is an inventory, not a compatibility verdict. No host implementation exists
for these members yet.

The two literal synchronous channels are:

```text
electron-store-get-data
zalo-wayland-titlebar:enabled
```

The other 19 `sendSync` sites pass a computed channel. QWebChannel is
asynchronous from JavaScript, so it cannot serve `sendSync` directly.

The Linux-specific channels already expected by the core are:

```text
zalo-linux-resolve-transferred-files
zalo-linux-auto-paste-screenshot
zalo-linux-get-theme
zalo-linux-theme-change
zalo-wayland-titlebar:enabled
zalo-wayland-titlebar:action
zalo-wayland-titlebar:maximized
zalo-wayland-controls
linux-offline:read
linux-offline:write
linux-offline:clear
linux-offline:refresh
```

## Vendor download for the macOS baseline

On 2026-10-30 the download link on `https://www.zalo.me/pc` directed a macOS
user agent via `https://www.zalo.me/download/zalo-pc?utm=90000` to
`https://res-download-pc.zadn.vn/mac/ZaloSetup-universal-26.10.20.dmg`, then a
CDN mirror. The 337,901,830-byte DMG was downloaded outside the repository;
its local SHA-256 is
`a45d30b7f7b060f76b87722ef419b8f41d23a2913cdef084dcb0955dde8d85a0`.
Read-only archive inspection found `Zalo.app/Contents/Resources/app.asar` and
`app.asar.unpacked`; `Info.plist` declares bundle `com.vng.zalo`, version
`26.10.20`, build `26.10.20.3253`. The redirect chain documents where the file
was obtained; no vendor-published hash or macOS signature has been independently
verified. Nothing from the archive is included in this repository.

For offline diagnostics only, `pc-dist`, `main-dist`, `package.json` and
`bootstrap.js` were extracted from that archive into a separate cache directory
(13,096 files). This is a partial renderer extraction, not a full runtime import;
macOS native libraries and other archive files are not included. `zalort inspect`
identified version `26.10.20`, entry `pc-dist/index.html`, 44 exposed preload
names and 20 static synchronous call sites (19 computed). The reported counts
describe this extraction and are not a complete compatibility guarantee.

An attempt to extract all archive entries using the current strict ASAR reader
stopped on an unpacked native file whose size differed from the ASAR header. A
separate read-only inventory found all 101 unpacked entries present: 49 matched
both their declared size and embedded SHA-256, and 52 differed in both. These
are local comparisons against metadata in the same ASAR, not vendor signature
verification. The cause is unverified; no mismatch check was bypassed and the
partial full extraction is not an imported or verified runtime.
The repository command `zalort check-unpacked <app.asar>` reproduces these counts
and exits 2; it checks unpacked entries against metadata inside the same archive,
not packed entries, macOS code signatures or an independently trusted digest.
All 52 different files were Mach-O binaries (`.node`/`.dylib`) with a code
signature load command found by static inspection. This is consistent with a
possible post-ASAR signing change, but presence of the command neither verifies
the signature nor proves the cause of the mismatch.

One mismatched arm64 `db-cross-v4-native.node` was examined further, not the
whole app. Its CMS signature verified over its CodeDirectory, and all 18
SHA-256 code-page hashes matched the file. Its signer certificate identifies
`Developer ID Application: VNG ONLINE CO.,LTD (CVB6BX97VM)`. OpenSSL could
build the chain to the Apple root downloaded from Apple's PKI page only with
`-ignore_critical`; without that option it rejected an unhandled critical
extension. This limited check does **not** validate Apple's critical-extension
policy, revocation, timestamp, bundle seal or the remaining binaries, and does
not establish why the ASAR metadata differs. It is not equivalent to a full
macOS `codesign`/Gatekeeper verification and does not authorize bypassing the
ASAR checks.

With `--rewrite-csp --block-remote --stub-preload --host-api --wait-ms 8000`,
the isolated off-the-record probe loaded the entry with title `Zalo - Đăng nhập
Zalo`; IndexedDB opened, 19 host requests were recorded and no remote request
reached the interceptor. One dependency-injection rejection remained. Diagnostic
stubs were used; this is not a successful real login or end-to-end bootstrap.
The installed community runtime has not been verified to match this archive.

Earlier debugger observations found a missing store `watch` option. Bounded,
best-effort polling now exists and is tested, but the `system-time` rejection
persists. The debugger also observed `zelectronNative.getResourceUsage` throwing
while a runtime image-cache module initialized; Qt host support for equivalent
image resource accounting does not exist yet, so this remains explicitly
unsupported. These observations use diagnostic stubs and a partial extraction;
they do not establish whether the resource error causes the DI failure. Runtime
code registers `system-time` in login startup, but the reason registration was
not reached is not yet proven.

## Local Olaz DMG and installed Zalo

On 2026-10-30, read-only inspection of the user-provided `Olaz-1.0.dmg`
identified an Olaz 1.0 macOS app with host scripts, a bridge table and adapters.
Its host script describes named store options and dot-path keys, and its bridge
uses WebKit message handlers and a Proxy-based compatibility layer. These are
third-party reference observations, not vendor-runtime or security verification;
no Olaz code or assets are included here. The local DMG SHA-256 is
`ed19821ba8a484b6af37bd2e97ad48719df81e72d3578c701493c24bef516bf2`.

The installed `Zalo.AppImage` is identified by its desktop entry as an
**unofficial Linux port**; its version marker says `26.10.20`. Its local SHA-256
is `c0f56646bf9524abb424476b93944d72f730fc7780d0a2b76ab8476bb120d198`.
These observations do not authenticate its origin or prove it matches the
separately imported runtime. The existing launcher uses `--no-sandbox`; that is
not a security recommendation or a default for this project. No user session
was inspected or copied.

## Sync transport spike

`tests/test_sync_scheme.cpp` proves that a **synchronous XHR** issued by page
JavaScript to a same-origin Qt custom scheme completes and returns
`sync:200:pong`. This is transport evidence only: it does not implement
Electron IPC, `sendSync` semantics, channel authorization or reply encoding, and
it does not prove the 19 computed channels can be served.

## Bootstrap probe

`zalo-bootstrap-probe` loads the entry HTML in an offscreen Qt WebEngine page.
By default it injects **no preload or host APIs**. It has separate opt-in
diagnostic modes for recording stubs (`--stub-preload`) and the bounded host
layer (`--host-api`). It never authenticates and exits after a fixed wait.

```bash
QT_QPA_PLATFORM=offscreen \
QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" --wait-ms 8000
```

Observed result:

- The document loaded: `loadFinished(true)`, title `Zalo - Đăng nhập Zalo`,
  `readyState: complete`.
- Qt's `zalo-app://` origin refused its own local stylesheets and scripts under
  the runtime's Content Security Policy (`style-src`/`script-src 'self'`). The
  page rendered a login shell, but its renderer bundles did not run.
- No remote host was requested. Because the scripts were blocked, this run says
  nothing yet about Zalo's HTTP/WebSocket, TLS or cookie requirements.
- All 46 preload names were `undefined`, as expected without injection.
- The only uncaught error was `Cannot read properties of undefined (reading
  'loadShellQos')`, consistent with a missing preload object.
- `localStorage` read/write succeeded; `indexedDB`, `caches`, `navigator.
  serviceWorker` were present as objects; `crossOriginIsolated` was `false`.
- Two non-fatal file misses were logged: `favicon-96x96.v1.png` and
  `android-icon-192x192.png`.

Interpretation:

1. Serving the files is not enough. The custom scheme/origin must be made
   compatible with the runtime's CSP before any bootstrap conclusion is possible.
2. Preload injection is mandatory before the renderer can initialize; loading
   HTML alone is not a minimal bootstrap.
3. Storage APIs exist, but persistence, quota, SQLite/native modules and
   cross-origin isolation remain unverified.
4. Network behavior cannot be assessed until local scripts execute.

## CSP experiments

The probe can compare scheme/CSP strategies without logging in:

```bash
# Baseline: mark the scheme local (current default).
./build-web/zalo-bootstrap-probe --runtime "$RUNTIME" --scheme-mode local --block-remote

# Omit the LocalScheme flag; the origin is still host-bearing.
./build-web/zalo-bootstrap-probe --runtime "$RUNTIME" --scheme-mode standard --block-remote

# Prove CSP is the blocker. This is a blanket bypass and is NOT a candidate default.
./build-web/zalo-bootstrap-probe --runtime "$RUNTIME" --scheme-mode csp-ignored --block-remote

# Scoped alternative: add zalo-app: only to fetch directives, in memory.
./build-web/zalo-bootstrap-probe --runtime "$RUNTIME" --rewrite-csp --block-remote
```

Observed:

| Mode | Result |
| --- | --- |
| `local` | CSP refused local styles/scripts. |
| `standard` | Same refusals; the `LocalScheme` flag is not the cause. |
| `csp-ignored` | Scripts executed; confirms CSP is the blocker, but this is a blanket bypass. |
| `rewrite-csp` | Zero CSP refusals; scripts executed with external origins still restricted. |

With `rewrite-csp`, the worker/startup bundles ran and reported the next missing
host pieces: `$zlogger.loadShellQos` and `$zwindow`. The runtime's console also
prints `do open indexeddb zlocalstorage undefined` and the same line for
`zsecure-localstorage`; those lines come from its own wrapper *before* it calls
`indexedDB.open` and are not proof of failure (see the IndexedDB check below).
No remote request was attempted or blocked during the observation window, so
network/TLS/cookie behavior is still unknown.

The rewrite is implemented in `src/core/RuntimePagePolicy.{h,cpp}` and covered by
`tests/test_scheme.cpp` (synthetic) plus `tests/test_csp_scheme.cpp` (WebEngine
integration: a same-origin script stays blocked without the rewrite and runs with
it). It modifies the HTML in memory only, leaves `frame-ancestors`, `base-uri`,
`form-action`, sandbox and reporting directives untouched, and does not add
external origins. It is not yet integrated into a production runtime host.

## Diagnostic preload shim

`--stub-preload` injects non-functional diagnostic objects before page scripts
run. It requires `--block-remote`, records calls and returns neutral values; it
does not implement IPC, storage, login or privileged behavior. Without
`--host-api`, `hostApisInjected` remains `false` and `diagnosticStubsInjected`
records the mode. Logged argument values are reduced to types/lengths (only a
small allowlist of non-sensitive Electron path names is kept), so cookies,
tokens, paths and message content are not copied into output.

```bash
QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" \
  --rewrite-csp --block-remote --stub-preload --wait-ms 8000
```

Observed sequence:

| Stage | Result |
| --- | --- |
| No stubs | `$zlogger.loadShellQos` TypeError; `$zwindow` rejection. |
| `$zlogger` + `$zwindow` stubs | `loadShellQos()` completes; next missing global is `$zuri`. |
| Recording proxies for all 46 measured names | Startup enumerates the surface; calls include `$zFileManager.getSentryConfig`, `MetriczGlobal.startCPUMonitor`, `$zMetric.onMessage`, `$zelectron.getAppVersion`, `$zelectron.getPath("userData")`, `$zelectron.getElectronStore` and `$znode.path`/`fs`/`fsExtra`. |
| Minimal structured `$znode.path` + `$zelectron.getPath` | `p.join is not a function` disappears; next calls are `$znode.fsExtra.ensureDir`, `$znode.fs.access` and `$znode.fs.constants`. |
| Namespace stubs for `$zFeatures.*`/`$zTFModels.ss` | Removes the `E[t.name] is not a function` error; calls `$zFeatures.mainless.onRemoteEvents` and `$zwindow.onVisibilityChange`. |
| Remaining error with structured stubs | DI `Cannot read properties of undefined (reading 'bind')`; debugger inspection later identified missing store methods; see the newer run below. |

IndexedDB test: the probe opened `__zalonative_probe` at version 1 and closed it,
so IndexedDB is usable in the `zalo-app://` origin. No remote request was
attempted (`blockedRemoteCount: 0`, only `zalo-app (local runtime)` observed).

This is contract evidence only. The recording proxies would be unsafe in
production because they make unknown APIs appear to succeed; the real bridge must
return explicit unsupported errors instead.

## Bounded host API probe

`--host-api` injects the real `renderer/host/preload.js` and serves requests
through `src/core/HostApi.{h,cpp}`. It requires `--rewrite-csp` and
`--block-remote`, and uses an isolated `QTemporaryDir` as the app-data root, so
no runtime or user file is written.

```bash
QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" \
./build-web/zalo-bootstrap-probe \
  --runtime "$HOME/.local/share/zalo-native/runtime" \
  --rewrite-csp --block-remote --stub-preload --host-api --wait-ms 8000
```

Observed result:

- `hostApisInjected: true`, `hostApiMode: bounded-diagnostic`;
- the page still loaded with title `Zalo - Đăng nhập Zalo`;
- `blockedRemoteCount: 0`, no remote host was contacted;
- `hostRequestCount: 22`, with recorded operations `electron.appVersion`,
  `electron.path`, `store.get`, `store.set`, `os.release`, `fs.mkdir`, `fs.access`, `fs.exists`;
- the stub preload still covered other names; one dependency-injection
  rejection remained.

The test-local WebEngine run also proves path helpers, `$znode.fs` and
`$znode.fsExtra`, exposes `$znode.fsPromise`, and verifies `$zelectron`
paths/version/store, UTF-8 and base64 file I/O, symlink/traversal rejection,
unsupported path-name rejection and explicit `ENOENT`/`EFBIG` failures. See
[BRIDGE.md](BRIDGE.md) for the policy and limitations; this is not a production
Electron IPC implementation.

## What is still unproven

- Official runtime provenance, version pinning against a vendor source and
  runtime update/rollback behavior.
- A production preload bridge covering the full measured Electron/Node/IPC
  contract; only the app-rooted `$znode`/`$zelectron` subset is implemented.
- Production integration and security review of the scoped CSP rewrite and the
  host scheme handler; no global web-security or CSP bypass is accepted.
- Network, TLS, cookie, WebSocket and persistent storage behavior of the real
  renderer in the Qt host.
- QR/login/restart/logout, messaging, files, calls, notifications and tray.

No session, cookie, token, QR value or account identifier was collected.
