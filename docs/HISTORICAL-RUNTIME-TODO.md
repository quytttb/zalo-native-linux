# Historical checklist — official runtime + Qt WebEngine

**Superseded 2026-11-02.** The checkboxes below record the previous direction;
they are not milestones of the current Qt/C++ backend. See [TODO.md](TODO.md).

Theo quyết định người dùng: **hướng Olaz là hướng chính duy nhất**, không tự
viết lại/reverse-engineer giao thức. Roadmap hiện hành: [PLAN.md](PLAN.md);
bằng chứng: [STATUS.md](STATUS.md), [WALKTHROUGH.md](WALKTHROUGH.md) và
[RUNTIME-EVIDENCE.md](RUNTIME-EVIDENCE.md).

`[x]` nghĩa là đã thực hiện và có bằng chứng; `[ ]` là chưa làm, chưa xác minh
hoặc mới làm một phần. Chỉ tick **cả phase** khi tất cả việc và điều kiện đạt
của phase đó hoàn tất. Các thay đổi mới hiện vẫn ở working tree, chưa mặc định
coi là đã commit hay phát hành. Kết quả QR của probe cũ không chứng minh login
runtime chính thức; số M1–M6 dưới đây được cập nhật theo plan hiện hành.

## Nền tảng hiện có — hoàn thành ở mức demo

- [x] Tạo repo local và GitHub `quytttb/zalo-native-linux`.
- [x] Shell Qt 6/QML với dữ liệu hội thoại giả, model và diagnostics.
- [x] Công cụ đọc ASAR, runtime manifest/locator và CLI `zalort`.
- [x] Bridge demo với allowlist và giới hạn payload; test cho model và core.
- [x] Tài liệu plan, walkthrough, giới hạn pháp lý và trạng thái thử nghiệm.
- [ ] Chứng minh các thành phần trên hoạt động với dữ liệu Zalo thật — hiện chỉ
  là nền tảng/demo, không phải client chat hoàn chỉnh.

## M0 — nền và kiểm thử: đang thực hiện

- [x] Kiểm tra repo local, remote và xác định commit remote mới hơn local.
- [x] Giữ nguyên công việc Web wrapper đang dở, không reset hoặc pull đè.
- [ ] Quyết định và thực hiện cách tách/lưu WIP Web khỏi nhánh native; tích hợp
  commit remote an toàn sau khi đối chiếu (không tự tách hoặc xóa mã ở lượt sửa docs).
- [x] Tách smoke test demo, native và Web; bổ sung phát hiện lỗi Loader/QML.
- [x] Sửa lỗi QML `onDownloadRequested` đã khiến smoke test trước đó PASS sai.
- [x] Thử âm tính độc lập với cảnh báo `Cannot assign`: CTest báo lỗi.
- [ ] Mở rộng kiểm thử để phát hiện thêm lỗi QML/Loader không khớp regex hiện có.
- [x] Không hiển thị kết luận “đã đăng nhập” chỉ từ URL Zalo Web.
- [x] Phân biệt chế độ demo, Web thử nghiệm và runtime native; Web chỉ mở khi
  có `--web`, còn mặc định là màn hình chẩn đoán runtime native.
- [ ] Kiểm tra bảo mật đầy đủ trước khi dùng Web wrapper với tài khoản thật.
- [x] Build và chạy **20/20** test với WebEngine, **17/17** không WebEngine
  (2026-10-30); test offline dùng profile test thay vì profile thật.
- [x] Chạy lại toàn bộ build/test có WebEngine sau các thay đổi mới nhất.
- [x] Thống nhất tài liệu theo lõi Zalo chính thức + Qt/QML, dừng roadmap giao thức riêng.
- [x] Đưa probe lịch sử ra khỏi build mặc định (`ZALO_BUILD_QR_PROBE=OFF`) và
  giữ mã trong `src/probe/` làm tham chiếu lịch sử.
- [ ] **Hoàn thành M0:** hoàn tất các mục còn mở và xác nhận luồng test đáng tin cậy.

## M1 — runtime provisioning và khả năng host: chưa đạt

- [x] Khảo sát tĩnh mô hình Olaz và nhận diện layout renderer bên ngoài ASAR
  launcher của AppImage cộng đồng (`AppDir/app/pc-dist/`).
- [x] Chọn DMG Zalo macOS 26.10.20 làm baseline khảo sát, ghim hash file tải;
  hash cục bộ không phải chữ ký hay hash được vendor xác thực.
- [x] Tải DMG Zalo macOS 26.10.20 qua link trang Zalo PC, ghi hash cục bộ và
  metadata bundle; chưa xác minh chữ ký/vendor hash hoặc import runtime đầy đủ.
- [x] Trích riêng renderer/preload từ DMG vào cache ngoài repo và chạy probe
  offline: trang load, IndexedDB mở, 19 lời gọi host, còn một lỗi DI. Đây là
  extraction một phần và có diagnostic stub, không phải bootstrap hoàn chỉnh.
- [ ] Xử lý an toàn việc import toàn bộ runtime DMG: 52/101 file unpacked
  khác cả size và SHA-256 trong header ASAR; chưa rõ nguyên nhân, không bỏ qua
  kiểm tra hoặc tuyên bố signature đã xác minh.
- [x] Đối chiếu giới hạn một Mach-O: CMS và 18 code-page hash khớp; kiểm tra
  chuỗi cần `-ignore_critical`, chưa phải xác minh macOS/bundle đầy đủ.
- [x] Chặn ASAR unpacked file trỏ ra ngoài root và giới hạn dữ liệu đọc; test
  symlink ngoài root và size mismatch (chưa loại bỏ race tiến trình local).
- [x] Thêm `zalort check-unpacked`: kiểm tra size/SHA-256 theo metadata ASAR,
  chỉ báo số lượng; test khớp, sai hash/size, thiếu integrity và giới hạn đọc.
- [x] Nhận diện đúng ASAR/app đã extract, entry HTML và native dependencies;
  `inspect` đọc được cả `pc-dist/index.html` và danh sách `.node`/`.so`.
- [x] Import riêng, lưu nguồn/version/hash và xác minh tính toàn vẹn cục bộ;
  `runtime.json` ghi `sourceVerified: false`, hash không phải xác thực vendor.
- [x] Kiểm kê API preload, IPC channel, lời gọi sync và native library cần host.
- [x] Thêm shim preload chẩn đoán (`--stub-preload`) để lộ contract khởi động:
  `$zlogger`/`$zwindow`, vòng quét toàn bộ tên preload, `$zelectron.getPath`,
  `$znode.path`/`fs`/`fsExtra`; xác nhận IndexedDB mở được; không login/mạng.
- [x] Host API hẹp có policy: `src/core/HostApi` + `renderer/host/preload.js`
  cho `$znode.path`, `fs`/`fsExtra`/`fsPromise`, `$zelectron` path/version/store;
  probe `--host-api` phục vụ 22 lời gọi trong data root tạm và test traversal/
  symlink/EFBIG/unsupported. Chưa phải IPC production.
- [x] Chặn symlink có đích chưa tồn tại trong `HostApi`; test ghi/ghi nối và
  đường dẫn con không tạo file ngoài root. Đây chưa giải quyết race symlink.
- [x] Áp dụng giới hạn file khi append/copy và giới hạn lượng đọc file/store;
  test vượt 8 MiB và xác nhận copy lỗi không xóa file đích.
- [x] Store preload xử lý tên store và khóa `__proto__` bằng map không có
  prototype; test WebEngine kiểm tra cả snapshot và listener cục bộ.
- [x] Nhận `getElectronStore({name, watch:true})`, tách store theo tên và theo
  dõi thay đổi bằng polling giới hạn; test dùng backend thứ hai, unsubscribe,
  không lặp event local và giới hạn số watcher.
- [ ] Hoàn thiện semantics store defaults/reset và đồng bộ production; polling
  hiện có thể bỏ lỡ thay đổi giữa hai lần đọc và chưa thay thế watcher thật.
- [ ] Điều tra token DI `system-time` chưa đăng ký trong shared-database
  detection; đã quan sát `getResourceUsage` unsupported khi module ảnh khởi tạo,
  nhưng chưa kết luận quan hệ nhân quả.
- [x] Cung cấp `os.release` theo kernel thật và `util.promisify` error-first;
  test callback, lỗi, receiver và custom symbol, không giả lập toàn bộ Node util.
- [ ] Thiết kế backend Qt cho `zelectronNative.getResourceUsage` hoặc phương án
  tương thích được xác minh; không báo image cache bằng 0 giả.
- [x] Bổ sung `toString()` UTF-8/base64/hex cho byte trả từ `readFileSync`
  không encoding khi thiếu Buffer, có test BOM và byte UTF-8 lỗi.
- [ ] Kiểm chứng và bổ sung tương thích Node Buffer đầy đủ nếu runtime cần;
  byte hiện chỉ là `Uint8Array`, không hỗ trợ slice decoding hoặc mọi encoding.
- [ ] Phân loại API preload/IPC thiết yếu và quyết định backend Linux thay thế
  cho từng dependency (đã có bảng quyết định và một subset host chạy được trong
  probe; dependency injection và phần lớn IPC/native vẫn chưa có backend).
- [x] Spike lời gọi sync: XHR đồng bộ từ JS sang custom scheme của Qt trả lời
  được (`sync:200:pong`); QWebChannel bất đồng bộ không thay thế trực tiếp.
- [x] Spike bootstrap: load `pc-dist/index.html` trong Qt WebEngine, ghi blocker
  CSP/preload/network và xác nhận localStorage/IndexedDB khả dụng; không login.
- [x] Xác định chiến lược CSP hẹp: thêm scheme local vào fetch directive trong
  bộ nhớ cho script chạy; `standard` không đủ và `csp-ignored` chỉ là chẩn đoán.
- [ ] Tích hợp CSP rewrite vào host production, review origin/frame và không
  dùng bypass CSP toàn cục.
- [x] Test runtime lỗi, traversal/symlink, thiếu entry và version/hash không khớp.
- [ ] **Hoàn thành M1:** runtime định danh được, bootstrap tối thiểu chạy và có
  kết luận rõ cho sync/network/storage; không tự xây giao thức Zalo.

## M2 — host renderer ẩn, bridge và QR của runtime: chưa làm

- [ ] Đăng ký scheme/Qt handler production (probe đã có handler chẩn đoán), chỉ
  phục vụ runtime và origin đã chọn.
- [ ] Inject bridge trước bootstrap, sandbox và allowlist theo API/frame/origin.
- [ ] Chỉ host API Electron cần thiết; không cho renderer dùng shell/fs tùy ý.
- [ ] Network HTTP/WebSocket, TLS/cookie và storage nhất quán cho lõi chính thức.
- [ ] Kiểm chứng IndexedDB/SQLite và backend Linux, không dùng fallback chưa thử.
- [ ] Chuyển QR/trạng thái do runtime cung cấp sang QML; profile riêng, xác nhận
  thủ công, không nhập cookie từ app/probe khác.
- [ ] Test bridge với lỗi, hủy, giới hạn payload/input nhị phân và log không lộ bí mật.
- [ ] **Hoàn thành M2:** login → restart → logout thật qua runtime trong Qt host.

## M3 — đọc hội thoại/tin bằng Qt/QML

- [ ] Adapter lấy state/service của lõi chính thức, không tự gọi giao thức Zalo.
- [ ] Model C++ dùng ID chuỗi, cập nhật tăng dần, phân trang và giới hạn payload.
- [ ] Danh sách hội thoại, unread và tìm kiếm cục bộ với dữ liệu thật.
- [ ] Đọc tin nhắn theo trang và hiển thị trạng thái đồng bộ.
- [ ] Lõi chính thức là nguồn chuẩn; hạn chế cache riêng, nếu có phải bảo vệ/xóa an toàn.
- [ ] Giữ demo tách biệt, không trộn với dữ liệu thật.
- [ ] Kiểm tra phím tắt, bộ gõ tiếng Việt trên Wayland và X11.
- [ ] **Hoàn thành M3:** đọc dữ liệu thật, reconnect không mất/trùng state hiển thị.

## M4 — file, lịch sử và desktop

- [ ] Gửi text qua service runtime, có xác nhận/lỗi thật; không tự cài API giao thức.
- [ ] Upload/download ảnh, file: chọn nơi lưu, tiến độ, hủy và báo lỗi.
- [ ] Reply, reaction, sticker theo khả năng được kiểm chứng.
- [ ] Kiểm tra đồng bộ điện thoại và E2EE do lõi Zalo xử lý; ghi rõ giới hạn.
- [ ] Notifications có tùy chọn ẩn nội dung; tray không bắt buộc để chạy.
- [ ] Bảo vệ dữ liệu local; thực hiện logout và xóa dữ liệu an toàn.
- [ ] Kiểm tra reconnect không làm mất hoặc trùng dữ liệu; thử file và sync thật.
- [ ] **Hoàn thành M4:** chat/file thật qua QML và lõi chính thức, có test và bằng chứng.

## M5 — cuộc gọi qua backend Linux có sẵn

- [x] Xác định một nguồn tham khảo call engine Linux cộng đồng; chưa kiểm chứng tích hợp.
- [ ] Kiểm tra nguồn gốc, giấy phép và khả năng dùng lại từng thành phần.
- [ ] Chọn/tích hợp helper hoặc backend Linux sẵn có được phép dùng; không tự
  viết signaling, codec/media hay reverse-engineer giao thức gọi.
- [ ] Nếu không có backend phù hợp, ghi chưa hỗ trợ thay vì tự viết lại.
- [ ] Chứng minh gọi thoại hai chiều, sau đó video và screen sharing.
- [ ] Ghi rõ tính năng chưa hỗ trợ; không hiển thị thành công giả.
- [ ] **Hoàn thành M5:** tính năng gọi đã tuyên bố được kiểm chứng thực tế.

## M6 — đóng gói, phát hành và đánh giá

- [ ] Pin runtime/adapter, update và rollback an toàn, báo version không tương thích.
- [ ] Package chạy thật phải có Qt WebEngine/WebChannel; bản OFF chỉ demo/tooling.
- [ ] Kiểm tra Ubuntu 26.04 amd64/KDE Wayland, thêm GNOME và X11.
- [ ] Build/test binary trong package; ưu tiên `.deb`, sau đó AppImage/Flatpak.
- [ ] Đo cold start, RAM tổng tiến trình, CPU idle, reconnect và chạy dài.
- [ ] So sánh với bản port hiện tại bằng cùng kịch bản, không giả định native nhẹ hơn.
- [ ] Kiểm tra license/attribution theo từng thành phần và quy trình phân phối.
- [ ] Đảm bảo package/repo không chứa runtime độc quyền, token hoặc dữ liệu tài khoản.
- [ ] **Hoàn thành M6:** chỉ phát hành khả năng đã test và mô tả đúng giới hạn.

## Lịch sử — probe giao thức trực tiếp đã dừng

Không tiếp tục các task backend/API/crypto/WebSocket trực tiếp. Đây không phải
hướng dự phòng và không phải M1 của plan hiện hành.

- [x] Probe tạo/quét/xác nhận QR và `userinfo.logged` của một phiên Web thật.
- [x] Cookie RAM, TLS mặc định, giới hạn redirect và test parser/crypto giả.
- [x] Ghi kết quả tại [M1-PROBE.md](M1-PROBE.md), đánh dấu dừng phát triển.

Mã và build option cũ vẫn còn trong tree; chưa xóa hoặc sửa default trong đợt
cập nhật tài liệu này. Không chuyển phiên probe sang runtime.
