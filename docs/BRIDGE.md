# Bridge — demo contract and official-runtime integration

The chosen architecture uses the official Zalo core with Qt 6/QML UI. Two
boundaries are needed: an app state/action bridge between the adapter and native
models, and a bounded host bridge supplying essential Electron/preload APIs.
The six-method `zalonative.bridge/1` contract below is **existing demo scaffolding**,
not an implementation of the full official preload surface.

## Demo bridge (implemented)

`BridgeRouter::dispatch` returns synchronously in C++. Calls over QWebChannel
(`BridgeObject::requestJson`) are **asynchronous** from JavaScript. This does not
solve synchronous Electron/preload requirements.

### Envelope

```jsonc
// request
{"id": "web-1", "kind": "call", "path": "state.publish", "args": [{...}]}
// success
{"id": "web-1", "ok": true, "value": {...}}
// failure
{"id": "web-1", "ok": false, "error": {"code": "unknown_method", "message": "..."}}
```

- `id` is echoed when present.
- `kind` is `call` (`invoke` accepted as an alias); anything else is rejected.
- Requests are capped at 1 MiB of compact JSON.

### Methods

- `app.ping`: returns `{pong, time}`.
- `app.info`: returns `{protocol, role}`.
- `runtime.info`: returns manifest JSON or `{}`.
- `state.publish`: accepts one `{conversations[], messages{}}` object and returns
  `{accepted, conversations}`.
- `state.snapshot`: returns the last published state or `{}`.
- `log.write`: accepts a non-empty string (logged up to 2000 characters) and
  returns `{written: true}`. This is not an automatic secret-redaction mechanism.

Everything else → `unknown_method`. The renderer can never spawn processes,
read files, or open sockets through this bridge.

### Events

`BridgeObject.event(json)` carries `{kind:"event", path:"state.changed", value}`.
`stub.js` subscribes via QWebChannel when available.

### Error codes

`invalid_json`, `invalid_request`, `invalid_args`, `unknown_method`,
`unsupported_kind`, `payload_too_large`.

## Official preload surface (measured, not implemented)

Static inspection of the available `26.10.20` app directory found 46 exposed
preload names, including `$zapp`, `$zlogin`, `$zdb`, `$zsqlite`, `$znetwork`,
`$znoti`, `$zcall`, `$znode`, `$zelectron`, `$zelectronNative`,
`$zFileManager`, `$zsessionManager`, `$zsharedWorker`, `$zscreencap`,
`__SENTRY_IPC__`, `__zaloFlatpakTransfer` and `__zaloOfflineStorage`.

It also found 21 synchronous `sendSync` call sites:

- 2 use literal channels: `electron-store-get-data` and
  `zalo-wayland-titlebar:enabled`;
- 19 pass a computed channel, so a fixed allowlist cannot cover them without
  understanding the caller.

Twelve Linux-specific channels are already expected by the core:
`zalo-linux-resolve-transferred-files`, `zalo-linux-auto-paste-screenshot`,
`zalo-linux-get-theme`, `zalo-linux-theme-change`, the four
`zalo-wayland-titlebar:*` channels, `zalo-wayland-controls` and the four
`linux-offline:*` channels.

These are inventory results. No preload member is implemented or proven safe.

## Synchronous transport spike

`tests/test_sync_scheme.cpp` shows that a synchronous XHR from page JavaScript to
a same-origin Qt custom scheme completes and returns `sync:200:pong`. This proves
a synchronous request/response transport is possible in Qt WebEngine.

It does **not** implement Electron `ipcRenderer.sendSync`: channel authorisation,
payload encoding, nested values, error propagation, main-thread blocking policy
and the 19 computed channels remain open. It must not be presented as a working
preload bridge.

## Bootstrap result relevant to the bridge

`zalo-bootstrap-probe` first loaded the entry HTML with **no** host APIs injected.
The runtime's CSP initially refused its own local scripts under the custom scheme.
A scoped in-memory CSP rewrite (adding only `zalo-app:` to fetch directives) let
the scripts and workers run without a blanket bypass. The next errors were
preload contract failures:

- `$zlogger.loadShellQos` is required immediately by the startup page;
- `$zwindow` is referenced by a startup/shared-worker bundle.

No remote host was attempted during the observation window.

## Diagnostic preload shim (not a bridge)

The probe has an optional `--stub-preload` mode. It injects **non-functional**
`$zlogger`/`$zwindow` objects, plus recording proxies for the remaining measured
preload names. Every call is logged; nothing is persisted, authenticated or sent
over the network. This mode is for discovering the contract, not for claiming
compatibility.

Observed sequence with `--rewrite-csp --block-remote --stub-preload`:

1. `$zlogger.loadShellQos()` is called and the page's first TypeError disappears.
2. The startup bundle enumerates the preload surface and stops at the first
   missing global. Stubbing `$zuri` only moves the failure to the next name, so
   the whole measured list is needed for enumeration to continue.
3. With recording proxies for all names, the runtime asks for:
   `$zFileManager.getSentryConfig`, `MetriczGlobal.startCPUMonitor`,
   `$zMetric.onMessage`/`sendMessage`, `$zelectron.getAppVersion`,
   `$zelectron.getPath("userData")`, `$zelectron.getElectronStore` and
   `$znode.path`/`$znode.fs`/`$znode.fsExtra`.
4. A generic proxy returns a function for every property, so path-like code fails
   with `p.join is not a function`. Adding a minimal in-memory `$znode.path` and
   `$zelectron.getPath` removes that error and exposes the next calls:
   `$znode.fsExtra.ensureDir`, `$znode.fs.access`, `$znode.fs.constants`.
5. Namespace-shaped stubs for `$zFeatures.*`/`$zTFModels.ss` remove the
   `E[t.name] is not a function` error. The remaining blocker is a dependency-
   injection failure (`Cannot read properties of undefined (reading 'bind')`) in
   `vendors-login...`. Debugger inspection identified a missing store method in that run; later
   probe results below supersede it.

IndexedDB itself works in the `zalo-app://` origin: the probe opened a test
database at version 1. The repeated `do open indexeddb zlocalstorage undefined`
console lines are emitted by the runtime's own wrapper before it calls
`indexedDB.open`; they are not evidence that the database failed to open.

## Implemented bounded host layer (diagnostic)

`renderer/host/preload.js` plus `src/core/HostApi.{h,cpp}` implement the
app-rooted subset that the probe measured; they are wired into
`zalo-bootstrap-probe --host-api` against an isolated `QTemporaryDir`. This is a
policy layer and a diagnostic integration, not a production Electron/Node
emulation.

Debugger inspection of the earlier `bind` error identified a store wrapper
binding methods absent from the original preload. The runtime calls
`getElectronStore` with named options and `watch: true`. The bounded preload
supports named stores and best-effort external-change notifications: it polls an
app-rooted snapshot every two seconds while listeners exist, with at most 16
active watched stores per renderer. It stops polling when listeners unsubscribe.
Tests exercise changes made by a second backend instance, no-op writes and local
notification deduplication. Changes made and reverted between polls can be missed;
file-system races, large-store cost and production origin/frame policy remain
unresolved. Defaults and reset remain explicitly unsupported, and this is not
full electron-store watch compatibility.
Store names and snapshot keys are held in prototype-free maps, including keys
such as `__proto__`. Filesystem containment also rejects dangling symlinks before
write attempts; this does not eliminate the documented symlink race with another
local process.
The file limit is checked on reads, append totals and copy sources; bounded
reads avoid loading an arbitrarily large file into memory. Local concurrent file
modification can still race these checks, so this is not an OS-enforced quota.
With no global `Buffer`, an unencoded filesystem read returns a `Uint8Array`
with whole-value `toString()` support for UTF-8, base64 and hex. It is not a
Node Buffer: slice decoding and other encodings fail explicitly, and Buffer
methods and identity are not emulated. A WebEngine test covers UTF-8, invalid
bytes, BOM, hex and base64; this does not establish runtime-wide compatibility.
`reset` fails explicitly because defaults are not implemented. Resource usage,
CPU details, cache clearing, clipboard and zoom methods also fail explicitly;
diagnostic stub values from earlier experiments are not production evidence.
`$znode.util.promisify` provides error-first callback conversion and supports
the custom promisify symbol; it is not a complete Node `util` implementation.
`$znode.os.release` reads the actual kernel version. Hostname and network
interfaces remain unsupported rather than silently exposing identifying data.
The probe now omits raw console messages and classifies exceptions, rather than
logging potentially sensitive runtime values.

Measured run with `--rewrite-csp --block-remote --stub-preload --host-api`:

- the page still loads (`Zalo - Đăng nhập Zalo`), with no network requests;
- 22 synchronous host requests were served and recorded as
  `electron.appVersion`, `electron.path`, `store.get`, `store.set`, `os.release`, `fs.mkdir`, `fs.access`,
  `fs.exists`;
- the preload self-test in `test_host_preload` additionally proves path helpers,
  UTF-8/base64 file round-trips, `fsPromise`, electron-rooted paths, store
  round-trips, outside-root rejection, unsupported path-name rejection and
  `ENOENT`;
- one dependency-injection rejection remains; the host layer does not supply
  the missing IPC-backed preload modules.

The synchronous transport is a bounded first step: Qt's custom-scheme handler
received an empty body for synchronous POST, so requests travel as a
size-capped `GET /__zalonative_host?request=<json>` (512 KiB request cap; 8 MiB
per file and 1 MiB per store file). Large or binary asynchronous work needs the
async IPC path, which is not implemented.

## Host API decisions from the probe

These are design decisions for the producing bridge, not claims that every
surface exists. The app-rooted subset above is implemented; the remaining names
are still unsupported.

| Surface | Decision |
| --- | --- |
| `$znode.path` | Implement as a pure-JS POSIX helper in the preload; no native privilege. |
| `$zelectron.getPath`/`getAppPath`/`getAppVersion` | Bounded, read-only host values rooted in the app data directory; never the user's whole home. |
| `$zelectron.getElectronStore` | Restricted key-value store under app data; no arbitrary path access. |
| `$znode.fs`/`$znode.fsExtra`/`$znode.fsPromise` | Only allowlisted app/data directories and explicit errors elsewhere; no shell, child process, or unrestricted filesystem. `fsPromise` is an alias of the same bounded promises. |
| `$zlogger` | Bounded logging with secret redaction; never log cookies, tokens or message content by default. |
| `$zwindow` | Native window operations; file dialogs require explicit user action and return cancellation by default. |
| `$zMetric`, `MetriczGlobal`, `$zFileManager.getSentryConfig` | Telemetry/Sentry disabled by default; no-op or explicit disabled response. |
| `$zFeatures` | Fixed, reviewable feature flags; unknown features stay off. |
| `$zuri`, `$zapp`, `$zdb`, `$znetwork`, `$znoti`, `$zcall`, `$zsqlite`, `$zsessionManager`, `$zsharedWorker`, `$zcloud`, `$zMsgChannel`, `$zInAppPayment`, `$zMigrateDrive`, `$zTFModels`, `$zscreencap`, `$zlogin`, `$zbackup`, `$zscript`, `$capp`, `$zprotocol`, `$zpreload`, `$zsqlitebw`, `$zsafeStorage`, `$zelectronNative` | Unsupported until a bounded IPC/backend exists; return explicit unsupported errors, never generic success. |

`$zsub` is not a host-provided global. The startup shared-worker bundle itself
runs `globalThis.$zsub = {}` and registers per-API subscription callbacks through
it. The host obligation is the underlying globals those callbacks invoke (for
example `$zwindow` and `$zuri`), not `$zsub`.

The production preload must **not** use a catch-all Proxy: silent success hides
missing capabilities and can corrupt runtime state. Unknown members must fail
explicitly.

## Planned runtime bridge

- Identify essential preload members for a pinned official runtime and implement
  only the required subset explicitly; never report success for a stub.
- Restrict calls to the trusted runtime origin/frame; use separate capabilities
  for state publication, network and user-authorized file/storage operations.
- Handle async replies, errors, cancellation, events, binary values and payload
  limits. Design synchronous members separately without blocking the UI thread
  indefinitely.
- Publish structured state into native models and forward explicit QML actions
  to official services. Do not implement Zalo's wire protocol in this bridge.
- Keep sessions and sensitive storage under runtime/host policy; never expose
  unrestricted filesystem, shell execution or arbitrary network proxy APIs.
- Test against synthetic inputs before using a real account. Pin compatibility
  to runtime version and reject unsupported versions or missing capabilities.

No Olaz adapter source is licensed for reuse by this document. See
[ARCHITECTURE.md](ARCHITECTURE.md), [RUNTIME.md](RUNTIME.md) and [PLAN.md](PLAN.md).
