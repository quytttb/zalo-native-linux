# Zalo Native — Qt/QML + C++ backend for Linux

An **unofficial, experimental** Zalo client: native Qt 6 Quick/QML UI with a
direct Qt/C++ backend. The default build does not need Chromium, Electron, a
Zalo PC runtime or a Rust sidecar. The backend implements unofficial protocol
compatibility, which may break as Zalo changes and may affect an account.

## Current status: not a working chat client

- The default screen contains an experimental C++ **Web QR** sequence, adapted
  from this project's old probe. Network access starts only when the user clicks
  the QR button; cookies and the QR are held in memory, not saved by the app.
- Its state machine and error handling have offline fake-network tests. The new
  implementation has **not** been verified with a real account. A confirmed Web
  session is not a PC session and does not establish chat compatibility.
- Conversation/message UI is **fictional demo data only** (`--demo`). Persistent
  sessions, real chat, synchronization, E2EE and calls are not implemented.
- [ctdozlite](https://github.com/ctdoteam1110/ctdozlite) is a protocol reference.
  A small C++ PC QR request/ticket codec has synthetic offline tests, but is not
  connected to the UI or network. No upstream Rust file is bundled. Previous
  WebEngine/runtime research is preserved as optional historical tooling.

See the [roadmap](docs/PLAN.md), [TODO](docs/TODO.md),
[status](docs/STATUS.md), [walkthrough](docs/WALKTHROUGH.md) and
[legal notes](docs/LEGAL.md).

## Build and test offline

Requirements: CMake ≥3.24, C++20, Qt ≥6.5 Core/Gui/Network/Qml/Quick/
QuickControls2 and Qt Test. On the development machine GCC 15 and Qt 6.10.2
are used; select a compatible compiler and Qt installation.

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-15 \
  -DZALO_WEBENGINE=OFF -DZALO_BUILD_QR_PROBE=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/zalonative
```

The tests use synthetic data and a fake network manager, with no Zalo login.
The QR screen does not automatically start a request. Live use may affect
existing Zalo sessions or violate service terms; review the risks first. Local
cancellation does not log out a server session.

## Other modes and retained research

- `./build/zalonative --demo`: fictional conversations and local-only composer.
- `./build/zalonative --runtime-diagnostics`: historical runtime inventory.
- `./build/zalonative --web`: legacy visible Web wrapper, available only in an
  explicit `-DZALO_WEBENGINE=ON` build; it is not the native product backend.
- `zalort`, `src/probe/` and `zalo-bootstrap-probe` remain research tools. The
  old standalone QR probe stays excluded by default. See the
  [historical walkthrough](docs/HISTORICAL-RUNTIME-WALKTHROUGH.md).

No Zalo runtime, proprietary assets, account data or session secrets are included.
Source/license attribution and account/data risks are described in
[LEGAL.md](docs/LEGAL.md). This project is not affiliated with VNG/Zalo.
