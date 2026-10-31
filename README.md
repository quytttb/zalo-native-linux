# Zalo Native — official Zalo core, Qt 6/QML interface for Linux

An unofficial, experimental Linux client following the **Olaz architecture**:
reuse the Zalo PC JavaScript/renderer core supplied by the user in a hidden
**Qt WebEngine** host, with a new **Qt 6 Quick/QML native interface** and bounded
C++ host services.

**This is the chosen direction, not a fallback.** We are not reimplementing the
Zalo wire protocol, chat encryption or call engine. A browser engine is an
intentional dependency; this is not a browser-engine-free client. Nor is the
intended product just a visible wrapper around `chat.zalo.me`.

## Current status — provisioning/diagnostics, not a working client

- Native QML demo, models, ASAR tooling and a small demo bridge exist.
- `zalort inspect/import/verify` detects extracted app directories, records
  preload/native inventory, copies into an isolated runtime directory and pins
  version/tree hash. Local hashes do not authenticate the vendor.
- The default application mode is a native runtime diagnostics screen. `--demo`
  shows fictional data; the visible Zalo Web wrapper only opens with explicit
  `--web`.
- `zalo-bootstrap-probe` loads a runtime entry HTML in Qt WebEngine without host
  APIs by default. On the available community runtime, CSP initially blocked
  local scripts; a scoped in-memory rewrite now lets scripts run, leaving missing
  preload APIs (`$zlogger.loadShellQos`, `$zwindow`) as the next blocker. The
  opt-in `--stub-preload` mode is diagnostic only. The opt-in `--host-api` mode
  injects a bounded preload and served 22 host calls in an isolated data root;
  a dependency-injection failure remains. Neither mode is a production bridge.
- Official runtime bootstrap, preload compatibility, QR login through that
  runtime, and real chat in the QML interface are **not implemented/verified**.
- A separate direct-protocol C++ QR probe successfully confirmed a Web session.
  That experiment is **discontinued**, not the product backend. Its code remains
  for historical reference but is excluded from the default build.

The project has examined a **community AppImage** and a partial renderer
extraction from a macOS DMG downloaded through Zalo's site. Neither is a working
Linux host; full archive/signature verification remains incomplete. See
[runtime evidence](docs/RUNTIME-EVIDENCE.md).

Start with [the roadmap](docs/PLAN.md), [the checklist](docs/TODO.md),
[current status](docs/STATUS.md) and [the walkthrough](docs/WALKTHROUGH.md).

## Build the foundation

Requirements: CMake ≥3.24, a C++20 compiler, Qt ≥6.5 (Core, Gui, Network, Qml,
Quick, QuickControls2; Test for tests). Use compiler/libraries compatible with
your Qt installation. The development machine uses GCC 15 / Qt 6.10.2; the
existing CI uses Qt 6.8.3. Distribution package availability differs.

**Qt WebEngineQuick and WebChannel are required for the chosen runtime host.**
On supported Ubuntu/Debian releases the development packages include
`qt6-webengine-dev`, `qt6-webchannel-dev` and `qt6-positioning-dev`, alongside Qt
base/declarative development and QML runtime modules. Install matching versions;
do not mix incompatible Qt SDKs.

```bash
cmake -S . -B build-web -G Ninja -DCMAKE_CXX_COMPILER=g++-15 \
  -DZALO_WEBENGINE=ON -DZALO_BUILD_QR_PROBE=OFF
cmake --build build-web --parallel 4
ctest --test-dir build-web --output-on-failure
./build-web/zalonative --demo
```

Replace `g++-15` as appropriate. CMake can still degrade to a build without a
renderer if WebEngine is missing: **a successful configure alone does not
establish runtime-host support**. `-DZALO_WEBENGINE=OFF` builds core/demo/tools
only, not the intended functional client. The archived QR probe is excluded by
default; pass `-DZALO_BUILD_QR_PROBE=ON` only for historical reproduction.

## Inspect, import or smoke-test without logging in

```bash
./build-web/src/cli/zalort inspect /path/to/AppDir/app
./build-web/src/cli/zalort import /path/to/AppDir/app \
  "$HOME/.local/share/zalo-native/runtime" --expected-version 26.10.20
./build-web/src/cli/zalort verify "$HOME/.local/share/zalo-native/runtime"

./build-web/zalonative --self-test
./build-web/zalonative --smoke-test --demo
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build-web/zalonative --smoke-test --demo
```

The demo uses fictional data; the composer only changes local memory. Launching
without `--demo` opens the native runtime diagnostics screen, **not** the hidden
official-core host and not Zalo Web. `--runtime <path>` currently adds diagnostics
only; it does not execute the official renderer. Use `--web` to opt into the
experimental visible Web wrapper. See [runtime handling](docs/RUNTIME.md).

## Layout and binaries

- `zalonative`: native shell, demo and explicit Web mode (`src/app/`, `qml/`).
- `zalo-bootstrap-probe`: offscreen runtime bootstrap diagnostic; no login.
- `zalort`: runtime CLI: `inspect`, `import`, `verify`, `list`, `extract`,
  `bridge-spec` (`src/cli/`).
- `src/core/`: models, ASAR reader, locator/manifest, importer, preload inventory,
  scheme resolver, CSP page policy, bounded host API, demo bridge.
- `renderer/`: bridge stub, bounded host preload and fictional state; no official
  runtime bundled.
- `src/probe/`: archived direct-protocol experiment, not the chosen backend.
- `tests/`: synthetic fixtures, QtTest/CLI/UI checks.
- `docs/`: [architecture](docs/ARCHITECTURE.md), [bridge](docs/BRIDGE.md),
  [runtime](docs/RUNTIME.md), [evidence](docs/RUNTIME-EVIDENCE.md),
  [plan](docs/PLAN.md), [TODO](docs/TODO.md), [status](docs/STATUS.md).

## Packaging and legal

Existing Debian workflows build with WebEngine disabled; their artifacts do
**not** provide the intended runtime-backed client. Runtime import, compatible
adapter versions and packaging with WebEngine remain roadmap tasks.

No official Zalo runtime, branding assets or session data is supplied in this
repository. Users must obtain their own runtime from legitimate Zalo channels;
the distribution/import flow must respect its terms. Olaz is an architectural
reference, not a code donor without permission. This project is not affiliated
with VNG/Zalo. Read [LEGAL.md](docs/LEGAL.md) before using a real runtime.
