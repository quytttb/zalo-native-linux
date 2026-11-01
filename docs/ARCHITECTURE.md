# Architecture — Qt/QML and native Qt/C++ backend

The chosen direction since 2026-11-02 is a direct, unofficial C++/Qt protocol
backend, with Qt Quick/QML for the UI. There is no Chromium, official renderer or
Rust backend on the default path. QML may use JavaScript for UI bindings.

```text
Qt Quick / QML UI
       | QObject properties, commands and signals
C++ controllers and models
       | bounded asynchronous operations
Qt/C++ backend (login, session, chat, storage — incrementally implemented)
       | Qt Network / future WebSocket and crypto components
Zalo services (unofficial compatibility)
```

`src/backend/NativeQrLogin` implements an **experimental Web QR sequence** based
on the project's old independently written probe. It owns an in-memory cookie
jar, never starts automatically, bounds replies and redirects, and has offline
fake-network tests. `web_session` means only that a userinfo response said
`logged: true`; no persistent session, PC login, chat or E2EE is implied. A local
cancel clears local state but does not revoke a server session.

The PC QR and session bootstrap in ctdozlite are a separate flow. The pure
`PcQrProtocol` codec serializes one QR request and validates a ticket with
synthetic offline tests, but has no transport or UI and does not bootstrap a
session. This is not a verified compatible backend. A later
implementation must handle source attribution, library licenses, secret storage,
reconnect and security review. See [PLAN.md](PLAN.md) and [LEGAL.md](LEGAL.md).

`--demo`, `--web`, and `--runtime-diagnostics` remain distinct; WebEngine and
runtime provisioning are historical opt-in tooling, not part of this backend.
The [previous architecture](HISTORICAL-RUNTIME-ARCHITECTURE.md) is preserved.
