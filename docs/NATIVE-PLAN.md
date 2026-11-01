# Quyết định kiến trúc — đường dẫn lịch sử

**Cập nhật 2026-11-02:** theo quyết định mới của người dùng, dự án chuyển sang
backend giao thức Qt/C++ trực tiếp và giao diện Qt/QML. Không dùng WebEngine hay
Rust sidecar trong đường chạy mặc định. Đây là client không chính thức.

Roadmap duy nhất hiện hành là [PLAN.md](PLAN.md); checklist ở [TODO.md](TODO.md).
File này giữ đường dẫn cũ để các liên kết không bị hỏng, không phải roadmap thứ hai.

Plan WebEngine trước đó được lưu tại [HISTORICAL-RUNTIME-PLAN.md](HISTORICAL-RUNTIME-PLAN.md).
Probe QR cũ vẫn tách riêng và mặc định tắt; backend QR mới được xây dựng từ probe
đó nhưng chưa được kiểm thử live. Không chuyển cookie từ probe vào app mới.
