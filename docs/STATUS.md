# Status — native Qt/C++ backend

As of 2026-11-02 this is an **experimental client foundation, not a working chat
client**. The default build does not require Qt WebEngine.

Implemented:

- Qt/QML demo with fictional conversations and C++ models.
- C++ asynchronous Web QR state machine, exposed to the native QML shell. It only
  contacts the network after an explicit action, and keeps QR/cookies in memory.
- Offline fake-network coverage of state transitions, failures, bounds, cancel,
  expiry, TLS error handling and redirect policy. These tests do not contact Zalo.
- The actual QML QR screen/image provider is tested with a synthetic image and
  fake network; the test also checks that startup makes no request.
- The native mode rejects the demo bridge/state and refuses the demo-only send
  action; a separate mode test verifies that fictional data remains opt-in.
- A separate C++ PC QR request/ticket codec based on ctdozlite, with synthetic
  tests and MIT attribution. It has no network/UI integration or login result.
- Historical runtime/ASAR tools, Web wrapper and bootstrap probe retained
  separately. Their results are recorded in [RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md).

Not implemented or not verified:

- The new QR flow has not been tested with a real account. Its `web_session`
  state means only that Web userinfo returned `logged: true`; it is not a PC
  session or a working chat backend.
- PC QR transport/getLoginInfo, protected persistent session, server logout, conversations,
  messages, WebSocket, files, E2EE and calls.
- Security/license review and production packaging. No RAM or performance claim
  has been measured for this architecture.

On 2026-11-02, a fresh default native build passed **22/22** CTest tests; the
optional historical WebEngine configuration passed **25/25**. The default
binary did not link Qt WebEngine or WebChannel in the `ldd` check. The historical
offline DMG probe still loaded and opened IndexedDB but retained the previously
observed `system-time` rejection; that result is not native-backend evidence.
The three new QR tests also passed under ASan/UBSan with leak detection disabled;
this is not a leak or security audit.

See [TODO.md](TODO.md) and [WALKTHROUGH.md](WALKTHROUGH.md). The old status is
preserved at [HISTORICAL-RUNTIME-STATUS.md](HISTORICAL-RUNTIME-STATUS.md).
