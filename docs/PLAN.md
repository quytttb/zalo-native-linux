# Roadmap — Qt/QML + backend Qt/C++

## Quyết định 2026-11-02

Giao diện dùng Qt 6 Quick/QML; backend là C++/Qt trực tiếp, không cần Chromium,
Electron, Node, Rust sidecar hay renderer Zalo PC. Đây là client không chính
thức: giao thức tự triển khai phải được bảo trì khi Zalo thay đổi, có thể vi
phạm điều khoản hoặc ảnh hưởng tài khoản. Không hứa tương đương Zalo PC.

Tham khảo [ctdozlite](https://github.com/ctdoteam1110/ctdozlite) tại commit
`1345f5d0cfd24fdb7d54e5caf3a88e3e3b2acd4a`: nguồn được công bố MIT OR
Apache-2.0. Khi port mã cần chọn điều kiện MIT, giữ bản quyền/thông báo và kiểm
tra dependency phù hợp GPLv2 của repo. Luồng QR Web đang dựa trên probe C++
trước đây; phần tạo request/đọc ticket PC được viết C++ theo contract ctdozlite,
có lưu thông báo MIT và test offline, không kèm file Rust. Luồng Web khác luồng
PC và `getLoginInfo`; không trộn khóa hay kết luận tương thích nếu chưa kiểm chứng.

Mã, tài liệu, bằng chứng WebEngine/ASAR được giữ làm lịch sử và công cụ tùy
chọn, không phải đường chạy sản phẩm. [Plan cũ](HISTORICAL-RUNTIME-PLAN.md),
[checklist cũ](HISTORICAL-RUNTIME-TODO.md), [runtime evidence](RUNTIME-EVIDENCE.md).

## M0 — nền và quyết định

- Giữ giao diện/model Qt, tách demo, chẩn đoán runtime và Web wrapper.
- Build mặc định không dùng WebEngine; giữ probe QR lịch sử ngoài build mặc định.
- Chốt provenance và license trước khi dùng hoặc port từng thành phần.
- Chỉ công bố chức năng được test; không đưa mã/tài nguyên độc quyền hoặc bí mật
  vào repo. Không tự chạy đăng nhập thật khi chưa có người dùng xác nhận.

## M1 — QR và phiên thử nghiệm

- Đưa luồng QR Web lịch sử vào backend C++ bất đồng bộ; QR/phiên chỉ trong RAM,
  bắt đầu khi người dùng chủ động bấm, có hủy và timeout.
- Giới hạn kích thước, TLS mặc định, chặn redirect ngoài HTTPS allowlist; test
  transport/state bằng phản hồi giả offline, không log QR/cookie/token.
- Phân tích luồng QR PC, `getLoginInfo`/khóa phiên và tính tương thích: **chưa
  giả định** phiên Web đã đủ cho API chat.
- Thiết kế bảo vệ phiên trong keyring, phục hồi, lỗi mạng và đăng xuất thực;
  xóa phiên cục bộ không được gọi là revoke ở server.
- Điều kiện đạt: QR → xác nhận → backend chat có phiên hợp lệ → restart/logout
  được kiểm chứng thật, sau review và khi người dùng cho phép.

## M2 — hội thoại và chat cơ bản

- HTTP, phiên/cookie và codec theo contract đã kiểm chứng; kiểm thử fixture và
  lỗi với transport giả. Đánh giá license từng thư viện crypto trước phân phối.
- Danh sách hội thoại, lịch sử có phân trang và model C++/QML; ID chuỗi, giới
  hạn payload, phân biệt dữ liệu thật với demo.
- WebSocket/reconnect, nhận và gửi text có xác nhận lỗi, không báo gửi thành công
  giả; không tự động gửi tin trong test.
- Điều kiện đạt: đọc/gửi/nhận/reconnect với tài khoản thử do người dùng duyệt.

## M3 — tính năng, bảo mật và đóng gói

- Ảnh/file, reaction, reply, sticker, cache, desktop notifications theo mức đã
  kiểm chứng. Lịch sử/phiên phải có chính sách bảo vệ và xóa riêng.
- Kiểm chứng E2EE/sync trước khi tuyên bố hỗ trợ; mã hóa transport không chứng
  minh mã hóa đầu cuối. Cuộc gọi là hạng mục riêng, chưa mặc định có.
- Đo RAM/CPU, đóng gói native, test Wayland/X11 và rà soát security/license.

Checklist bằng chứng chi tiết: [TODO.md](TODO.md); hướng dẫn: [WALKTHROUGH.md](WALKTHROUGH.md).
