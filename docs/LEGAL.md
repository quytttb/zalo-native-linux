# Legal and provenance

- This is an unofficial client, not affiliated with or endorsed by VNG/Zalo.
  Use of an unofficial client may violate service terms or lead to session/account
  restrictions. The user decides whether to test with an account.
- The chosen architecture is a direct Qt/C++ implementation of unofficial
  protocol compatibility, not a supplied official renderer. Do not claim Zalo
  approval, complete compatibility or E2EE without evidence.
- The current Web QR sequence is adapted from this project's historical C++
  probe. That independent probe cited [zca-js `loginQR.ts` (MIT), commit
  `2a7e360257dfe98e2eb48dc46bbf389834be7e0b`](https://github.com/RFS-ADRENO/zca-js/blob/2a7e360257dfe98e2eb48dc46bbf389834be7e0b/src/apis/loginQR.ts).
  No upstream zca-js source is bundled.
- [ctdozlite](https://github.com/ctdoteam1110/ctdozlite) commit
  `1345f5d0cfd24fdb7d54e5caf3a88e3e3b2acd4a` is a reference, published as
  MIT OR Apache-2.0. The PC QR request/ticket codec is independently written in
  C++ using its documented contract; no Rust source file is bundled. The MIT
  copyright and license are retained in `third_party/ctdozlite/LICENSE-MIT` and
  installed with the app. If more source is adapted or translated, document
  provenance, select applicable
  terms, preserve notices and check all dependencies. MIT is the branch to
  evaluate for this GPLv2-only project; Apache-2.0 code combined with it may be
  incompatible. A repository's top-level license is not a substitute for
  reviewing individual files and transitive dependencies.
- Retain GPLv2 terms in [NOTICE.md](../NOTICE.md); do not relicense without
  authorization. Review Qt, TLS/crypto and packaging licenses before release;
  the old optional OpenSSL-based probe does not settle that review.
- Never commit proprietary Zalo/Olaz code or assets, DMGs, user data, QR payloads,
  cookies, session tokens, encryption keys or logs containing them. Do not
  automate unattended login, scraping, spam or bulk messaging.
- The historical WebEngine runtime/Olaz material remains separate. Its presence
  is not permission to copy or redistribute proprietary runtime or Olaz code.
  See [HISTORICAL-RUNTIME-LEGAL.md](HISTORICAL-RUNTIME-LEGAL.md).
