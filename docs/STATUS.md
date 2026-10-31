# Status — official-runtime direction selected

The chosen architecture is **Zalo PC's official JavaScript/renderer core in a
hidden Qt WebEngine host + Qt 6/QML native UI**, following Olaz's model. There is
no active direct-protocol roadmap. See [PLAN.md](PLAN.md), [TODO.md](TODO.md)
and the latest runtime evidence in [RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md).

## Implemented foundation

- ASAR reader, app-directory runtime detection, manifest/locator, local-path
  resolver and `zalort` CLI (`inspect`, `import`, `verify`, `list`, `extract`,
  `check-unpacked`, `bridge-spec`).
- Static preload inventory: exposed APIs, synchronous call sites, Linux IPC
  channels, native `.node`/`.so` dependencies and warnings.
- Isolated runtime import with per-file/tree SHA-256, `runtime.json`, expected
  version/hash checks, overwrite protection and symlink containment.
- Explicit application modes: native runtime shell by default, fictional demo
  via `--demo`, and the experimental visible Zalo Web wrapper only via `--web`.
- Native `RuntimeShell.qml` diagnostics; `WebShell.qml` is not the target UI.
- Strict QML smoke-test warning collector with a negative-control CTest.
- Scoped in-memory CSP rewrite for the local runtime scheme, with synthetic and
  WebEngine integration tests (blocked without rewrite, runs with rewrite).
- `zalo-bootstrap-probe`: offscreen WebEngine load with no injected host APIs by
  default, recording CSP, JavaScript, network, storage and preload blockers;
  optional `--stub-preload` contract-discovery mode and `--host-api` bounded
  app-data host mode.
- Bounded host layer (`HostApi` + `renderer/host/preload.js`) for pure-JS
  `$znode.path`; app-rooted `$zelectron` paths/version/store; restricted
  `$znode.fs`/`fsExtra`/`fsPromise`, with explicit unsupported errors.

These establish provisioning/diagnostics and a demo. They do **not** make a
working official Zalo runtime host.

## Evidence from the available runtime

One reference runtime is an extracted **community AppImage**, not an official
Zalo Linux release. It is app version `26.10.20`, build commit
`12ef36d`, entry `pc-dist/index.html`. Static inspection found 46 exposed preload
names, 21 synchronous call sites (19 with computed channels), 12 Linux IPC
channels and 15 native library files. The imported copy contains 13,369 files
(255,047,692 bytes) with tree SHA-256
`f46e7998416732498476f6134234edf0d20c3b75fe1d8e54ef5d7dd84a3d2a3a`.

`zalort verify` reports local integrity only; `sourceVerified` remains `false`.
A macOS DMG downloaded through Zalo's site has also been partially extracted
for offline probing. Its full import is blocked by 52 unpacked-file metadata
mismatches, and its complete vendor signature/bundle has not been verified.

The bootstrap probe initially produced no usable renderer: the page document
loaded, but the runtime's CSP refused its own local stylesheets and scripts under
the custom scheme. Two follow-up experiments isolated the behavior: omitting the
`LocalScheme` flag did not help, while an in-memory rewrite that adds only the
`zalo-app:` scheme to CSP fetch directives let the scripts and workers run
without a global CSP bypass. The helper is `RuntimePagePolicy::allowSchemeInCsp`
with synthetic tests; it is not yet integrated into a production host.

With the scoped CSP rewrite, the next blockers are preload members:
`$zlogger.loadShellQos` and `$zwindow`. A diagnostic `--stub-preload` mode then
shows the startup enumerating the preload surface and calling `$zelectron.getPath`,
`$znode.path`/`fs`/`fsExtra` and telemetry APIs. IndexedDB itself opens at version
1 in the `zalo-app://` origin; the runtime's `do open indexeddb ...` lines are its
own wrapper logging, not a demonstrated failure. No remote request was attempted
during the observation window, so network/TLS/cookie behavior is still unknown.
`localStorage`/IndexedDB/caches/service-worker APIs exist, but persistence, quota,
SQLite/native modules and cross-origin isolation are not verified.

The synchronous-XHR spike returns `sync:200:pong` through a Qt custom scheme.
That is transport proof only, not a complete Electron `sendSync`/IPC bridge.

The bounded `--host-api` integration now serves the real preload from an isolated
temporary data root. In the recorded run it handled 22 calls
(`electron.appVersion`, `electron.path`, `store.get`, `store.set`, `os.release`, `fs.mkdir`,
`fs.access`, `fs.exists`); no remote request reached the interceptor. A
dependency-injection rejection remains, so the subset is not a working bridge. Unit/WebEngine tests
cover path helpers, file/store round-trips, `fsPromise` availability,
traversal/symlink containment and explicit unsupported/limit errors. This remains
a policy layer, not an OS sandbox or a production IPC implementation.

## Not implemented or not verified

- Full import and integrity/signature assessment of the selected macOS baseline.
- Production integration and security review of the scoped CSP rewrite; no
  global web-security or CSP bypass is accepted.
- A production preload/IPC bridge covering the required Electron/Node APIs,
  including the 19 computed synchronous channels; only the app-rooted
  `$znode`/`$zelectron` subset is implemented.
- Official bootstrap, network/TLS/cookie behavior and persistent storage in the
  Qt host.
- QR/login/restart/logout through the official runtime.
- Live conversation/message state and sending from native QML.
- Attachments, phone history/E2EE compatibility, calls, notifications and tray.
- A release package containing the host dependencies and runtime import flow.

Known detection behavior: a launcher ASAR can be reported as present while it has
no HTML entry; the actual application lives separately in an extracted app
directory such as `AppDir/app/pc-dist/`. `inspect` distinguishes launcher-only
from a runtime core. This is structural detection, not compatibility proof.

## Recorded tests

On 2026-10-30, GCC 15 / Qt 6.10.2 passed:

- **20/20** CTest with WebEngine enabled;
- **17/17** CTest with WebEngine disabled (the sync-scheme, CSP-scheme and
  host-preload tests are excluded).

The suite uses synthetic fixtures and test profiles. The negative-control test
emitting `Cannot assign` fails as intended; it validates that expression, not
every possible QML failure. Test success does not prove runtime compatibility.

CI/package workflows still build with WebEngine disabled; their packages cannot
be treated as runtime-backed client releases. The ASAR reader does not verify
vendor authenticity; runtime version pinning against an official source remains
pending.

## Archived experiment — not runtime-host evidence

`zalo-qr-probe` successfully confirmed a live Zalo Web session with the user's
participation. It has no chat/realtime/persistence; its cookie jar was in RAM.
The experiment is discontinued under the current decision. Its code remains for
historical reference but is excluded from the default build
(`ZALO_BUILD_QR_PROBE=OFF`). See [M1-PROBE.md](M1-PROBE.md).
