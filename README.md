# Zalo Native — experimental native client for Linux

An unofficial, experimental **native Qt 6 / QML shell** plus **runtime tooling**
for a Zalo desktop client on Linux. This project takes the same approach as
Olaz (native macOS UI over the official Zalo renderer), but targets Linux and
avoids any Electron/Chromium wrapper packaging.

**Milestone 0 (this tree):** foundation only. The app runs with fictional demo
data. There is no Zalo login, no real account connection, and no official Zalo
code or assets anywhere in this repository.

## Quick start

### Dependencies

Ubuntu 24.04+ / Debian Trixie:

```bash
apt install g++-15 cmake ninja-build \
    qt6-base-dev qt6-declarative-dev \
    qt6-tools-dev qt6-tools-dev-tools
```

Optional (enables `RendererHost.qml`, the Qt WebEngine renderer host):

```bash
apt install qt6-webengine-dev qt6-webchannel-dev qt6-positioning-dev
```

Without WebEngine dev headers, the build prints a warning and continues without
the renderer host — shell, tools, and tests all still work (`-DZALO_WEBENGINE=OFF`
silences the warning).

### Build and test

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-15
cmake --build build
ctest --test-dir build --output-on-failure
```

Fast paths:

```bash
./build/zalort inspect <runtime-dir-or-app.asar>
./build/zalonative --self-test
./build/zalonative --smoke-test
QT_QPA_PLATFORM=offscreen ./build/zalonative   # headless run
```

### Run with demo data

```bash
./build/zalonative
```

Opens the two-pane native shell (searchable conversation list, chat pane,
diagnostics dialog) fed by `renderer/demo-state.json`.

### Run against a real runtime (milestone 2+)

```bash
./build/zalonative --runtime ~/.local/share/zalo-native/runtime
```

Currently only produces a manifest and diagnostics — the WebEngine host wiring
to the official runtime is a later milestone.

## Binaries

| Binary       | Purpose                                              |
|--------------|------------------------------------------------------|
| `zalonative` | Native Qt 6/QML shell (`src/app`, `qml/`)           |
| `zalort`     | CLI: `inspect`, `list`, `extract`, `bridge-spec`     |

## Layout

```
src/core/      ASAR reader, runtime manifest/locator, local scheme resolver,
               conversation/message models, bridge router
src/app/       AppController, main.cpp, QML shell
src/cli/       zalort tool
qml/           native UI: Main, Sidebar, ChatPane, MessageDelegate, …
renderer/      stub.html / stub.js (bridge demo), demo-state.json (fixture)
tests/         QtTest suites + checked-in sample.asar fixture
docs/          PLAN, ARCHITECTURE, BRIDGE, RUNTIME, STATUS, LEGAL
```

See `docs/PLAN.md` for the milestone roadmap, `docs/STATUS.md` for what works
today, and `docs/LEGAL.md` before doing anything with a real Zalo build.

## Legal

Unofficial, not affiliated with VNG/Zalo. No Zalo code, branding assets, or
session data in this repo. Users supply their own runtime. See `docs/LEGAL.md`.
