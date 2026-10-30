# Status — milestone 0

Verified with `ctest` (10/10) on Qt 6.10.2 / g++-15: 5 QtTest suites
(ASAR, bridge, models, runtime, scheme), 3 `zalort` CLI tests
(`inspect`, `list`, `bridge-spec`), plus `--self-test` and an offscreen
`--smoke-test` that loads the full QML module.

## Works

- `zalort inspect/list/extract/bridge-spec` against any ASAR, including the
  official Zalo `app.asar` for read validation.
- Native shell launches with fictional Vietnamese demo data: searchable list,
  pinned-first sorting, unread badges, message threads, working composer
  (local only), diagnostics dialog (overview / manifest / log tabs).
- Bridge router enforces allowlist + payload cap; renderer stub
  (`renderer/stub.html`) exercises ping/publish/log over QWebChannel.
- Conditional WebEngine: builds and links when dev headers exist, degrades
  gracefully without them.

## Does not work yet (by design)

- No Zalo login, no QR flow, no real account, no network calls to Zalo.
- No send path to any server; composer messages stay in local memory.
- No attachment upload/download, no calls, no notifications, no tray.
- No packaging (Flatpak/deb) yet — only `make install` of binaries + desktop
  file.

## Known limitations

- System Qt 6.10 packages lack `Qt6::WebEngineQuick` CMake targets unless the
  `-dev` packages are installed; CI builds with `-DZALO_WEBENGINE=OFF`.
  Local builds can point at an extracted dev prefix (see README).
- ASAR parser is read-only and validates structure, not signatures;
  `zalort verify` against vendor hashes is an M1 item.
