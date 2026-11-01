Zalo Native — experimental native client for Linux.
Copyright (C) 2026 the Zalo Native contributors.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation. This program is distributed WITHOUT ANY WARRANTY;
see LICENSE for details.

This is an unofficial project, not affiliated with VNG/Zalo (see docs/LEGAL.md).

The chosen architecture uses a native Qt/QML UI and a direct Qt/C++ backend.
The experimental Web QR sequence is adapted from this project's independently
written historical probe, which cited zca-js (MIT) as a protocol reference; no
zca-js code is bundled. The PC QR request/ticket codec references ctdozlite
(MIT OR Apache-2.0); its MIT notice is in third_party/ctdozlite/LICENSE-MIT. No
upstream Rust source file is bundled. See docs/LEGAL.md.
The earlier official-runtime/Olaz host remains historical optional tooling;
no Zalo runtime or Olaz code is included or licensed by this notice.
