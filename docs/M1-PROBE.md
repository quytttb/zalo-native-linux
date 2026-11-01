# Historical standalone direct-protocol QR probe

The standalone probe remains historical and is excluded from the default build
(`ZALO_BUILD_QR_PROBE=OFF`). On 2026-11-02 the project selected a native C++
backend again; its new Web QR state machine was adapted from this old probe.
The old live observation below does not verify the new code or establish PC/chat
session compatibility. See [PLAN.md](PLAN.md) and [STATUS.md](STATUS.md).

`zalo-qr-probe` is a separate experimental C++/Qt Widgets program. It does not
use the web wrapper, Electron, Node, Python or the existing Zalo profile. It is
not installed or included in release packages. It implements the QR generation,
scan, confirmation and session-check sequence using Qt Network and an in-memory
cookie jar; nothing is persisted. TLS verification is left enabled, redirects
are followed only for session checks and only to specific HTTPS Zalo hosts,
with a five-hop limit. Requests time out, and the UI does not show
or log QR codes, tokens, cookies or response contents as text.

Historical reproduction only, not the recommended product setup (requires an
explicit decision to retry live login; normally leave the probe disabled):

```sh
cmake -S . -B /tmp/opencode/zalo-qr-archive -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-15 -DZALO_WEBENGINE=OFF -DZALO_BUILD_QR_PROBE=ON
cmake --build /tmp/opencode/zalo-qr-archive
ctest --test-dir /tmp/opencode/zalo-qr-archive --output-on-failure
# Live reproduction only if deliberately requested:
# /tmp/opencode/zalo-qr-archive/zalo-qr-probe
```

The probe build requires Qt Widgets/Network and OpenSSL development headers.
The separate `ProtocolCrypto` experiment implements the protocol's MD5 signing
and AES-CBC primitives with synthetic tests. It is not yet wired to login or
messaging, and the zero IV is a compatibility requirement, not a recommendation
for new cryptographic designs.

The probe only contacts Zalo after clicking **Tạo QR thử nghiệm**. Scanning and
confirming may affect another Zalo Web session or trigger account restrictions;
use an authorized test account and decide explicitly before scanning. Close the
window to cancel. The logged-in label
is only displayed if userinfo reports `logged=true`. No session recovery,
conversation listing, WebSocket, sending or receiving exists. The discontinued
direct-protocol experiment never reached a complete chat client. There is no
production readiness or E2EE claim.

Reference for endpoint sequence and response fields: [zca-js loginQR.ts, MIT,
commit 2a7e360257dfe98e2eb48dc46bbf389834be7e0b](https://github.com/RFS-ADRENO/zca-js/blob/2a7e360257dfe98e2eb48dc46bbf389834be7e0b/src/apis/loginQR.ts).
No upstream source files are bundled. The GPLv3 Zalo10 client and Olaz scripts
were studied for architecture only and are not copied into this probe.

The old standalone probe lacked offline HTTP/TLS state-machine tests, session
persistence, integrated protocol codec, conversations, realtime receive, send
and reconnect. The new backend has synthetic state-machine tests, but persistence
and chat remain open in [TODO.md](TODO.md); the old executable remains separate.

## Verification on 2026-10-30

Fresh builds with GCC 15 and Qt 6.10.2 passed **20/20** CTest tests with
WebEngine and **17/17** without it; the sync-scheme, CSP-scheme and host-preload
tests are WebEngine-only.
These tests exercise fictional data only. A separate negative CTest experiment
emitting the previously missed `Cannot assign` warning failed as intended; it
validates the failure expression, not every possible QML error. The probe's
parser has synthetic tests, but the network state machine and TLS failure cases
are not yet covered by an offline mock server. The archived probe and its parser
tests are built only with `ZALO_BUILD_QR_PROBE=ON`, not in the default suite.

User-assisted live check: QR generation, scan, confirmation, session redirects
and `userinfo.logged` succeeded after allowing the exact `jr.zaloapp.com` host.
This demonstrates a Web login only; it does not establish messaging or prove
session persistence. No account identifiers, cookies, or QR values were recorded.
