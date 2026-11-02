# Walkthrough — backend Qt/C++

Roadmap: [PLAN.md](PLAN.md). Status: [STATUS.md](STATUS.md). The app is an
experimental, unofficial client and **cannot yet read or send real messages**.

## Build and offline tests

Requirements: CMake ≥3.24, C++20, Qt ≥6.5 Core/Gui/Network/Qml/Quick/
QuickControls2 and Qt Test. Qt WebEngine is optional and disabled by default.

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-15 \
  -DZALO_WEBENGINE=OFF -DZALO_BUILD_QR_PROBE=OFF
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

`test_native_qr` substitutes a fake `QNetworkAccessManager`; its responses,
credentials and QR image are fictional. It covers expected transitions,
pending-poll retry, unsuccessful session check, cancel and cookie clearing,
deadline/expiry, response limits, redirect rejection and TLS errors. Passing it
does not validate the real server or encryption. UI smoke tests do not click QR.
`test_pc_qr_protocol` exercises a separate C++ PC QR request/ticket codec using
fictional values; it makes no network request and is not connected to the app UI.
`test_native_qr_qml` loads the real QR shell and provider with a synthetic image,
starts and cancels using fake network responses, and checks no request is made
before the button is activated. On 2026-11-02 a fresh default build passed
**22/22** CTest tests and the optional WebEngine build passed **25/25**.
`test_app_modes` checks that native mode does not expose the demo bridge, ignores
unsolicited demo state and cannot report a local demo send as a real message.
The three new QR tests also passed under ASan/UBSan with leak detection disabled;
this does not establish absence of leaks or prove security. The Qt installation
on the development machine emits QTP0004 and QML plugin link-target configure
warnings; smoke tests pass locally, but packaged QML dependencies still need
verification on target systems.

The earlier local pass used an ignored `.so` fixture that was absent on GitHub,
causing `test_runtime` to fail there. The test now creates a fictional library
entry in a temporary directory. A clean Git-tracked source export plus the test
change passed **22/22** with no `.so` in its source fixtures; consult GitHub
Actions for the latest remote CI result.

## App modes

```bash
./build/zalonative                       # native C++ experimental QR screen
./build/zalonative --demo                # fictional UI data only
./build/zalonative --runtime-diagnostics # historical runtime inspection
./build/zalonative --web                 # historical Web wrapper; optional WebEngine
```

The native screen does not initiate network requests on startup. **Tạo QR thử
nghiệm** explicitly starts the historical Web QR sequence. It has not been
retested live after integration; do not interpret it as PC login or usable chat.
Scanning may affect existing sessions or the account. Consider a dedicated test
account and review the implementation first; do not send credentials to the
assistant or share the QR. The app stores the QR and cookie jar only in memory,
does not securely wipe all memory, and does not revoke a server session when
cancelled. Manage/revoke sessions from Zalo if needed.

The retained probe `ZALO_BUILD_QR_PROBE=ON` is a separate historical executable;
its previous live result does not verify this new implementation. WebEngine
diagnostics can be built with `-DZALO_WEBENGINE=ON` and retain their existing
offline-only guardrails. See [historical walkthrough](HISTORICAL-RUNTIME-WALKTHROUGH.md).

No test or build step should use a real Zalo profile or log QR values, cookies,
tokens, account identifiers, message contents or private paths.
