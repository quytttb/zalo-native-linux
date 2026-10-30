# Quyết định kiến trúc — thay thế plan giao thức native cũ

**Theo quyết định của người dùng, dự án dùng mô hình Olaz: lõi Zalo PC chính
thức trong Qt WebEngine + giao diện Qt 6 Quick/QML.** Không tự reverse-engineer
hay triển khai lại giao thức nhắn tin, mã hóa hoặc cuộc gọi.

Roadmap duy nhất hiện hành là [PLAN.md](PLAN.md); checklist ở [TODO.md](TODO.md).
File này giữ đường dẫn cũ để các liên kết không bị hỏng, không phải roadmap thứ hai.

## Thay đổi so với kế hoạch trước

- “Native” nói về giao diện Qt/QML và tích hợp Linux, không có nghĩa không dùng
  browser engine. Qt WebEngine là phụ thuộc có chủ đích của runtime host.
- Auth, xử lý chat, network/protocol và đồng bộ tận dụng lõi Zalo chính thức.
  Host C++ chỉ bổ sung API preload/IPC, model và dịch vụ hệ điều hành cần thiết.
- Không dùng cách chỉ hiển thị `chat.zalo.me` làm sản phẩm chính.
- `zalo-qr-probe`/`ProtocolCrypto` là thử nghiệm lịch sử đã dừng, không tiếp tục
  thành backend C++ trực tiếp và không chuyển cookie của probe sang runtime.
- Không tự xây call engine; tích hợp backend Linux sẵn có được phép dùng nếu
  khả thi, nếu không thì ghi chưa hỗ trợ.

Mã thử nghiệm được giữ trong `src/probe/` nhưng đã ra khỏi build mặc định
(`ZALO_BUILD_QR_PROBE=OFF`). Chi tiết kết quả trước đây ở [M1-PROBE.md](M1-PROBE.md).
