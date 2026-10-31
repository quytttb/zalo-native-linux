# Architecture — official Zalo core with native Qt/QML UI

## Decision

Follow Olaz's architectural model, **without copying its unlicensed-for-reuse adapters**: keep Zalo PC's
official JavaScript/renderer logic, host it inside Qt WebEngine, and expose a
native Qt 6 Quick/QML interface. This is the chosen product architecture.

Do not reimplement Zalo's wire protocol, encryption, synchronization or call
engine. Compatibility work is confined to the runtime host, preload/IPC bridge,
state/action adapters and Linux services. A missing host dependency is a blocker
or an unsupported feature, not a mandate to rewrite the protocol.

## Target flow — not yet implemented end to end

```text
Qt 6 Quick / QML interface
          | structured state / explicit UI actions
C++ controller and models
          | app state/action bridge
Runtime adapter <-> official Zalo JS core in hidden Qt WebEngine
                          | bounded preload/IPC host bridge
                   C++ Linux host services
                   storage, network if needed, files, desktop integration

User-supplied official runtime -> inspect / verify / pin -> isolated host profile
```

- **Official core:** owns login/session behavior, chat services and protocol,
  synchronization and supported encryption logic. It remains the source of truth.
- **QML:** owns visible conversations, messages, composer, login/status and settings.
  Do not use the visible official Web page as the final chat interface.
- **C++ host:** implements needed Electron/preload capabilities with explicit
  permissions, model updates and Linux integration. File picker grants, not
  renderer-selected arbitrary paths, authorize file operations.
- **Adapter:** maps runtime state/services and actions to stable app types; pin
  adapter compatibility to runtime versions and fail clearly on version drift.
- **Calls:** integrate an existing compatible Linux backend if available and
  legally usable. The macOS `ZaloCall` helper cannot simply run on Linux.

## Existing implementation vs target

The tree already has `AsarArchive`, app-directory/ASAR `RuntimeManifest` and
`RuntimeLocator`, isolated `RuntimeImporter` with tree hashes, a static
`PreloadInventory`, the `LocalScheme::resolve` helper, a six-method demo
`BridgeRouter`, QML models and fixtures.

The application now separates three modes: native runtime diagnostics by default,
fictional demo with `--demo`, and the visible public Zalo Web wrapper only with
`--web`. `RuntimeShell.qml` shows detected runtime metadata; `WebShell.qml` is an
experiment, not the hidden official core. `src/probe/` directly implemented QR
Web login and crypto primitives; that path is discontinued, excluded from the
default build, and not part of this architecture.

`zalo-bootstrap-probe` registers a `zalo-app://` scheme handler and loads the
entry HTML offscreen; host injection is off by default. The document loads, but
the runtime's CSP initially refuses its own local stylesheets and scripts under
that scheme; omitting the `LocalScheme` flag does not help. An in-memory rewrite
that adds only the local scheme to CSP fetch directives makes the scripts and
workers run without a global bypass. That helper is
`RuntimePagePolicy::allowSchemeInCsp` and is tested, but not yet integrated into a
production host. With it, the next blockers are preload members
(`$zlogger.loadShellQos`, `$zwindow`). The optional `--stub-preload` mode is
diagnostic only: it shows startup calling `$zelectron.getPath`,
`$znode.path`/`fs`/`fsExtra` and telemetry APIs, and that IndexedDB opens
successfully. `--host-api` goes one step further with the bounded
`renderer/host/preload.js` and `HostApi`; it served 22 host calls in the latest recorded run, but a dependency-injection
failure remains. This is concrete
bootstrap evidence, not a successful host.

## Integration gates

- Integrate and security-review the scoped CSP rewrite, then define the exact
  origin/frame policy. Do not globally disable web security or use a blanket CSP
  bypass as the default.
- Determine essential Electron/preload APIs and storage/native dependencies for
  a pinned official runtime; importing HTML alone is insufficient.
- QWebChannel is asynchronous even when its C++ dispatch returns synchronously.
  The custom-scheme sync-XHR spike proves transport only; a safe strategy for
  essential synchronous preload calls is still required, including the 19
  computed channels.
- Establish consistent TLS/cookies/storage and background execution. Merely hiding
  a WebEngine view must not be assumed to preserve timers, sync or realtime work.
- Prefer existing core network behavior; if Qt host transport is required, keep
  it bounded and faithful to request/cookie semantics without rebuilding Zalo APIs.
- Test IndexedDB/SQLite compatibility and the 15 measured native library files;
  do not assume Olaz's macOS fallback works unchanged in Linux or newer versions.

## Security and distribution

- Treat the runtime and received messages as untrusted inputs. Keep Chromium's
  sandbox and TLS validation; no blanket `--no-sandbox`/web-security bypass.
- Serve only the pinned runtime root via a private scheme, with traversal/symlink
  checks and exact origin/frame validation before exposing host APIs.
- Use separate profiles; do not import sessions from other Zalo installations or
  the archived probe. Redact credentials, redirect URLs and chat data from logs.
- Do not duplicate plaintext secrets in host settings. Establish keyring/profile
  protection and explicit logout/data deletion before persistent sessions ship.
- Do not bundle proprietary Zalo/Olaz code or assets without permission. Import
  from legitimate official sources, record provenance and review redistribution.

Native here means **native UI**, not a browser-engine-free backend or guaranteed
lower resource use. See [PLAN.md](PLAN.md), [BRIDGE.md](BRIDGE.md) and [LEGAL.md](LEGAL.md).
