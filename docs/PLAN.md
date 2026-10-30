# Milestone plan

## M0 — Foundation (done, this tree)

- Read-only ASAR parser with traversal/unpack hardening.
- Runtime manifest + locator (`package.json` name/version, Electron version,
  entry HTML, native `.node` modules).
- Transport-independent JSON bridge router (`zalonative.bridge/1`) with a
  6-method allowlist (`app.ping`, `app.info`, `runtime.info`, `state.publish`,
  `state.snapshot`, `log.write`) and a 1 MiB payload cap.
- Native shell: `AppController`, two-pane QML UI (search, pinned sorting,
  unread badges, composer, diagnostics).
- `zalort` CLI (`inspect`, `list`, `extract`, `bridge-spec`).
- 10-test suite: QtTest ×5, CLI ×3, self-test, offscreen smoke test.

## M1 — Runtime provisioning

- Detect official Zalo AppImage/deb installs in `candidateAsars`-style order.
- Extract-and-verify flow: ASAR + `.unpacked` into `$XDG_DATA_HOME/zalo-native/runtime`.
- Hash + signature capture in manifest; `zalort verify`.
- Runtime version tracking and update prompts.

## M2 — WebEngine host + QR login

- Serve runtime via private `zalo-app://` scheme (uses `LocalScheme::resolve`).
- `RendererHost.qml` → real entry HTML; bridge `state.publish` from adapter
  injected at `document_start`.
- Native QR-code pane; no credential handling in the shell.

## M3 — Native conversation / chat read path

- Renderer adapter publishes conversation list + message windows.
- Native unread counts, search, message rendering, attachment placeholders.
- Offline snapshot cache in `QStandardPaths::AppDataLocation`.

## M4 — Send, attachments, notifications, tray

- Outgoing text through bridge; attachment picker; download progress.
- Freedesktop notifications for mentions/DMs; tray icon with unread count.
- Flatpak manifest + AppStream metadata.

## M5 — Calls

- Evaluate `QWebEngineView` WebRTC audio/video or `QtMultimedia` capture with
  a native call window. Feasibility spike first; may stay renderer-hosted.
