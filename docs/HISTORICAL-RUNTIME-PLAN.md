# Historical roadmap — official runtime + Qt WebEngine

**Superseded 2026-11-02.** This records the previous direction, not the current
plan. The active Qt/C++ protocol roadmap is [PLAN.md](PLAN.md).

**Quyết định hiện hành:** theo mô hình kiến trúc Olaz: tận dụng lõi JavaScript /
renderer của Zalo PC chính thức, host bên trong Qt WebEngine, hiển thị giao diện
native bằng Qt 6 Quick/QML. Đây là hướng chính duy nhất, không phải dự phòng.

Không tự viết lại giao thức Zalo, mã hóa chat, signaling hay media engine.
C++ đảm nhiệm host, bridge có kiểm soát, model và tích hợp Linux. Nếu thiếu
backend tương thích, ghi blocker hoặc tạm chưa hỗ trợ tính năng; không chuyển
sang tự reverse-engineer giao thức. Hiểu API preload/IPC đủ để nối host là công
việc tích hợp runtime, không phải xây một client giao thức độc lập.

Không chỉ mở `chat.zalo.me` và hiển thị giao diện Web. Browser engine là phụ
thuộc có chủ đích để chạy lõi Zalo; không quảng bá ứng dụng là không Chromium.
Mô hình Olaz là tham khảo kiến trúc, không phải quyền sao chép adapter của Olaz.

Checklist thực hiện: [TODO.md](TODO.md). Bằng chứng hiện có: [STATUS.md](STATUS.md)
và [WALKTHROUGH.md](WALKTHROUGH.md). Các mốc dưới đây áp dụng cho hướng lõi chính
thức; kết quả QR của probe cũ không được tính là hoàn thành login runtime.

## M0 — nền Qt/QML và kiểm thử (đã có nền, còn việc hoàn thiện)

- Giữ ASAR reader, runtime locator/manifest, bridge demo, model, shell QML và CLI.
- Bảo toàn WIP, đối chiếu remote trước khi tích hợp, không xóa probe cũ trong
  đợt sửa tài liệu. Tách demo, Web wrapper thử nghiệm và runtime host đích.
- Smoke test phải phát hiện lỗi QML/Loader; test chạy trên profile riêng.
- Chốt license, nguồn gốc và điều kiện sử dụng từng thành phần tích hợp.
- Điều kiện đạt: shell/demo và test ổn định, không giả định demo là chat thật.

## M1 — runtime provisioning và kiểm chứng khả năng host

- Dùng runtime người dùng cung cấp từ bộ cài Zalo chính thức; ưu tiên một phiên
  bản macOS đã ghim để khảo sát. AppImage cộng đồng chỉ là nguồn tham khảo layout,
  không được gọi là bản Zalo Linux chính thức.
- Nhận diện đúng ASAR và thư mục app đã extract (`pc-dist`, `main-dist`, native
  dependencies); không nhầm ASAR launcher với runtime đầy đủ.
- Extract vào thư mục riêng; lưu nguồn, version, hash và kết quả xác minh nguồn.
  Hash cục bộ không thay thế chữ ký hay hash vendor tin cậy.
- Liệt kê API preload/IPC, storage và network thiết yếu; xác định dependency
  macOS nào cần thay bằng host Linux hoặc implementation sẵn có được phép dùng.
- Spike API đồng bộ: QWebChannel chỉ bất đồng bộ. XHR đồng bộ tới custom scheme
  Qt đã trả lời được trong test transport; vẫn phải thiết kế `sendSync`/IPC an
  toàn cho các kênh thực tế và không trả thành công giả.
- Điều kiện đạt: runtime đã định danh, bootstrap tối thiểu chạy trong host thử
  và các blocker sync/network/storage được ghi rõ trước khi làm UI thật.

## M2 — host renderer ẩn, bridge và QR của runtime

- Đăng ký scheme riêng và handler Qt WebEngine, chỉ phục vụ runtime đã chọn.
- Host bridge thay các API Electron/preload cần thiết, inject trước bootstrap;
  phân quyền theo origin/frame/API, không cung cấp Node hoặc shell tùy ý.
- Giữ HTTP/WebSocket, TLS, cookie và storage nhất quán cho lõi chính thức. Chỉ
  thêm transport shim có giới hạn nếu host cần; không tự cài giao thức Zalo.
- Kiểm chứng IndexedDB/SQLite và phụ thuộc Linux; không ép nhánh fallback chưa
  được thử chỉ vì Olaz đã dùng nó ở macOS.
- Lõi Zalo thực hiện login; QML hiển thị QR và trạng thái qua adapter. Profile
  riêng, xác nhận thủ công trên điện thoại, không lấy cookie từ app khác.
- Điều kiện đạt: QR, đăng nhập, restart và đăng xuất trên runtime chính thức;
  không dùng URL hoặc kết quả probe cũ để kết luận đã đăng nhập.

## M3 — đọc hội thoại và tin bằng UI native

- Adapter lấy trạng thái/service của runtime, chuyển dữ liệu có cấu trúc vào
  model C++; QML hiển thị danh sách, unread, tìm kiếm, tin và phân trang.
- ID chuỗi, cập nhật tăng dần, giới hạn payload và xử lý trạng thái lỗi; không
  sao chép cả DOM vào giao diện chính.
- Lõi chính thức là nguồn dữ liệu chuẩn; hạn chế tạo thêm kho chat riêng.
  Cache nếu cần phải có chính sách bảo vệ/xóa dữ liệu.
- Điều kiện đạt: đọc được hội thoại/tin thật, reconnect không trùng hoặc mất
  trạng thái hiển thị; không trộn fixture demo với phiên thật.

## M4 — thao tác chat, file và desktop

- Gửi text qua service chính thức, có xác nhận/lỗi; không gọi API giao thức tự viết.
- Ảnh/file, reply, reaction, sticker theo khả năng runtime và bridge đã kiểm chứng.
- Đồng bộ điện thoại và E2EE do lõi Zalo xử lý; kiểm chứng đầy đủ hoặc ghi chưa
  hỗ trợ, không hứa tính năng chỉ vì đã tải được renderer.
- Qt file picker, tải lên/xuống có xác nhận/tiến độ, notifications có chế độ ẩn
  nội dung, tray, bộ gõ Việt, phím tắt Wayland/X11, logout và xóa dữ liệu an toàn.
- Điều kiện đạt: luồng chat/file thật hoạt động qua UI QML và lõi chính thức.

## M5 — cuộc gọi qua backend Linux có sẵn

- Xác định helper/backend Linux tương thích, được phép sử dụng; ưu tiên thành
  phần chính thức có thể chạy hoặc implementation cộng đồng đã có và phù hợp license.
- `ZaloCall` macOS không chạy trực tiếp trên Linux. WebRTC trong Qt WebEngine
  không tự động tương thích với backend gọi của Zalo PC.
- Không tự dựng lại signaling, codec hay giao thức gọi. Nếu chưa có giải pháp
  tích hợp khả thi, để cuộc gọi chưa hỗ trợ và tiếp tục bản chat.
- Nếu có backend: thử thoại, video, thiết bị và screen sharing với UI QML.
- Điều kiện đạt: các tính năng gọi được tuyên bố đều có kiểm chứng thực tế.

## M6 — đóng gói, tương thích và hiệu năng

- Pin runtime/adapter theo phiên bản, kiểm tra cập nhật và rollback an toàn;
  lỗi tương thích phải báo rõ, không tự chạy runtime chưa được hỗ trợ.
- `.deb` trước, AppImage/Flatpak và AppStream sau. Package chạy thật cần Qt
  WebEngine/WebChannel; build không WebEngine chỉ dùng core/demo/tooling.
- Kiểm thử binary đã đóng gói trên Ubuntu 26.04 amd64/KDE Wayland rồi GNOME/X11;
  kiểm tra sandbox, profile, quyền thiết bị và đường dẫn XDG.
- Đo tổng RAM/CPU mọi tiến trình, cold start, reconnect và chạy dài so với port
  hiện tại. Native UI không bảo đảm nhẹ hơn khi vẫn chạy lõi renderer.
- Không commit/phân phối lõi độc quyền, assets Zalo hoặc bí mật khi chưa có quyền;
  tải/import runtime riêng từ nguồn hợp lệ. Kiểm tra license trước release.
- Điều kiện đạt: chỉ phát hành tính năng đã thử, package không bị coi nhầm là
  client hoàn chỉnh khi mới chạy demo.
