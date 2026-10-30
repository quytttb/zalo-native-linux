# Architecture

Native shell first, web content last. The C++ core does everything that can be
done natively; Qt WebEngine exists only to host the official Zalo renderer in
later milestones.

```
┌────────────────────────────────────────────────────────┐
│  QML shell (qml/)        AppController (src/app/)      │
│  Sidebar / ChatPane /    conversation + message models │
│  DiagnosticsDialog        filter proxy, demo loader    │
└──────────────┬────────────────────────┬────────────────┘
               │ QAbstractItemModel      │ QObject bridge
               ▼                         ▼
┌────────────────────────────────────────────────────────┐
│  BridgeRouter (src/core)  zalonative.bridge/1          │
│  allowlist routing, 1 MiB cap, JSON in/out             │
└──────────────────────────┬─────────────────────────────┘
                           │ state.publish / runtime.info
               ┌───────────┴────────────┐
               ▼                        ▼
   WebEngine renderer host   demo-state.json / stub.html
   (M2+, zalo-app://)       (M0 fixture)
└────────────────────────────────────────────────────────┘
               │
               ▼
┌────────────────────────────────────────────────────────┐
│  Runtime layer: AsarArchive → RuntimeManifest →         │
│  RuntimeLocator; LocalScheme allowlists file access     │
└────────────────────────────────────────────────────────┘
```

Key rules:

- The renderer is untrusted content. It may publish *structured state* through
  the allowlisted bridge; it never sends file paths and never receives shell
  privileges.
- `zalo-app://` serves only files below the extracted runtime root, rejects
  `..`, backslashes, NUL bytes, and symlinks that escape the root.
- `zalort` is the field tool: inspect a runtime before ever pointing the app
  at it.
