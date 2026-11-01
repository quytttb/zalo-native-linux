# Historical legal notes — runtime host

**Superseded 2026-11-02.** The current architecture and legal notes are in
[LEGAL.md](LEGAL.md). This document preserves the earlier runtime-host context.

- This project is **unofficial** and not affiliated with, endorsed by, or
  connected to VNG Corporation or Zalo.
- The repository contains **no Zalo code, binaries, branding assets, session
  data, or credentials**. Demo content is fictional and written for this
  project.
- Users supply their own official Zalo runtime (installed from Zalo's own
  distribution channels). Runtime inspection/import does not execute it. The
  diagnostic bootstrap probe loads the entry document without host APIs by
  default; its opt-in diagnostic modes only exercise bounded local filesystem/
  store calls in an isolated temporary root, and the
  chosen product architecture will execute the supplied JavaScript core inside Qt
  WebEngine. Import/download, execution and redistribution have separate legal
  conditions; do not assume a right to redistribute the runtime, branding or
  assets.
- Using a third-party client may violate Zalo's Terms of Service. Account
  restrictions, forced logout, or loss of functionality are the user's risk.
- Do not commit session files, tokens, cookies, phone numbers, or chat
  exports. Do not automate login, scraping, spam, or bulk messaging.
- The chosen direction reuses the official core, not an independently recovered
  Zalo protocol. Host integration may require understanding preload/IPC contracts;
  keep that work within applicable law and license terms. There is no roadmap
  to reverse-engineer chat encryption, synchronization or a call protocol.
- Olaz is an architectural reference. Its distributed scripts are not a grant
  of permission to copy or redistribute them. Implement or license the required
  host/adapters rather than copying unlicensed source.
- Preserve the project's current GPLv2 terms; review the license/provenance of
  each dependency and any existing Linux helper before integrating or shipping.
  The archived GPLv3 Zalo10 study is not permission to combine its code with
  GPL-2.0-only code. Do not relicense the project without authorization.
- The direct-protocol QR probe is a discontinued historical experiment. It is
  not the chosen backend and its temporary session is not a runtime profile.
