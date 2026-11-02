# To-do — backend Qt/C++ trực tiếp

`[x]` chỉ nghĩa là đã làm và có bằng chứng, không chứng minh tương thích thật
nếu chỉ test fixture. Roadmap: [PLAN.md](PLAN.md). Checklist WebEngine cũ được
giữ tại [HISTORICAL-RUNTIME-TODO.md](HISTORICAL-RUNTIME-TODO.md).

## M0 — đổi kiến trúc

- [x] Chốt Qt/QML + backend C++ theo yêu cầu; không dùng Rust sidecar hay
  renderer chính thức trong đường chạy mặc định.
- [x] Tách chế độ backend native, demo, Web wrapper và chẩn đoán runtime lịch sử.
- [x] Chặn bridge/state demo và gửi tin giả trong chế độ native; có test tách mode.
- [x] WebEngine mặc định `OFF`; giữ `ZALO_BUILD_QR_PROBE=OFF` và mã nghiên cứu cũ.
- [x] Fresh default build/test **22/22**; build WebEngine lịch sử **25/25**
  (2026-11-02). Kiểm tra binary default không link WebEngine/WebChannel. Lượt
  test này còn phụ thuộc fixture `.so` bị Git bỏ qua nên không chứng minh CI sạch.
- [x] Sửa `test_runtime` tự tạo fixture `.so` trong thư mục tạm; kiểm tra lại
  bản xuất chỉ gồm file Git cùng bản sửa test: **22/22**, không có `.so` trong
  source fixture. Xem GitHub Actions để biết trạng thái CI mới nhất.
- [x] Ghi nguồn ctdozlite commit `1345f5d`, giữ thông báo MIT cho contract QR PC;
  chưa khẳng định toàn bộ dependency phù hợp license.
- [ ] Rà soát bảo mật, license Qt/TLS/crypto và đóng gói trước phát hành.

## M1 — QR và phiên

- [x] Triển khai state machine C++ cho luồng QR **Web** lịch sử; QML hiển thị
  ảnh trong RAM, yêu cầu do người dùng chủ động bắt đầu, có hủy và hết hạn.
- [x] Test offline với network double: quét/chờ/xác nhận/kiểm tra phiên giả,
  timeout, hủy, redirect, lỗi TLS giả, giới hạn response và ảnh lỗi.
- [x] Test QML shell/provider thật với QR giả; không request trước thao tác bấm.
- [x] Chạy ba test QR bằng ASan/UBSan với leak detection tắt; chưa kiểm tra rò rỉ
  bộ nhớ hoặc audit bảo mật.
- [x] Chỉ báo phiên Web khi userinfo có `logged: true`; không gọi đó là login
  backend chat. Không log QR/cookie/token hay nội dung server.
- [ ] Review và kiểm thử live có người dùng xác nhận; luồng mới chưa được thử
  với tài khoản thật hoặc xác minh còn tương thích dịch vụ hiện tại.
- [x] Xây codec C++ tạo signed request và đọc ticket QR PC theo contract
  ctdozlite; test input giả, giới hạn và URL an toàn, chưa gọi mạng.
- [ ] Phân tích `getLoginInfo` của ctdozlite; xác định khóa, domain, cookie và
  khác biệt phiên Web/PC trước khi nối chat. Codec PC chưa nối vào UI/network.
- [ ] Thiết kế lưu phiên có bảo vệ keyring, phục hồi và phân biệt lỗi mạng với
  hết hạn. Hiện chỉ trong RAM; hủy chỉ xóa cục bộ, không revoke server.
- [ ] Xác minh QR → phiên chat → restart/logout thật sau khi được cho phép.

## M2 — chat native

- [ ] Thiết kế API backend bất đồng bộ, transport và codec có giới hạn.
- [ ] Test vector/malformed input offline và rà soát license crypto; không tự
  tuyên bố bảo mật/E2EE chỉ vì có mã hóa giao thức.
- [ ] Đọc danh sách hội thoại, lịch sử và phân trang bằng backend thật.
- [ ] Nhận sự kiện WebSocket/reconnect; gửi text và hiển thị kết quả thật.
- [ ] Test bằng fixture và sau đó thử nghiệm có kiểm soát bằng tài khoản phụ.

## M3 — hoàn thiện

- [ ] Ảnh/file, reaction, reply, sticker, cache và thông báo theo khả năng đã thử.
- [ ] Đánh giá đồng bộ lịch sử, E2EE và gọi thoại/video trước khi tuyên bố hỗ trợ.
- [ ] Keyring, chính sách xóa dữ liệu, bảo mật log, đóng gói và benchmark thực tế.
