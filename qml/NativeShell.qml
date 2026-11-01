import ZaloNative 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    signal diagnosticsRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 14

                Text {
                    text: "Zalo Native"
                    color: Theme.text
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Qt/C++ thử nghiệm"
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                IconButton {
                    icon: "info"
                    tooltip: "Chẩn đoán (Ctrl+D)"
                    onClicked: root.diagnosticsRequested()
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.min(620, root.width - 60)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 14

                Item { Layout.preferredHeight: 18 }
                Text {
                    Layout.fillWidth: true
                    text: "Đăng nhập QR thử nghiệm"
                    color: Theme.text
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    text: "Ứng dụng chỉ kết nối khi bạn bấm tạo QR. Đây là luồng phiên Web "
                          + "đang được kiểm chứng, chưa phải đăng nhập chat hoàn chỉnh."
                    color: Theme.textMuted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 260
                    radius: Theme.radius
                    color: App.qrLogin.hasQr ? "white" : Theme.surface
                    border.color: Theme.border

                    Image {
                        objectName: "nativeQrImage"
                        anchors.fill: parent
                        anchors.margins: 12
                        visible: App.qrLogin.hasQr
                        source: App.qrLogin.hasQr ? "image://nativeqr/current?rev=" + App.qrLogin.imageRevision : ""
                        cache: false
                        asynchronous: false
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: !App.qrLogin.hasQr
                        text: App.qrLogin.busy ? "Đang xử lý…" : "QR sẽ hiển thị tại đây"
                        color: Theme.textFaint
                        font.pixelSize: 13
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: App.qrLogin.status
                    textFormat: Text.PlainText
                    color: App.qrLogin.state === "failed" ? Theme.danger : Theme.text
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10
                    Button {
                        objectName: "nativeQrStart"
                        text: "Tạo QR thử nghiệm"
                        enabled: !App.qrLogin.busy
                        onClicked: App.qrLogin.start()
                    }
                    Button {
                        objectName: "nativeQrCancel"
                        text: App.qrLogin.webSessionReady ? "Xóa phiên cục bộ" : "Hủy"
                        enabled: App.qrLogin.busy || App.qrLogin.webSessionReady || App.qrLogin.state === "failed"
                        onClicked: App.qrLogin.cancel()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Không lưu cookie hay QR vào đĩa. Quét QR có thể ảnh hưởng phiên "
                          + "hoặc tài khoản Zalo. Xóa cục bộ không đăng xuất trên máy chủ; "
                          + "quản lý phiên từ ứng dụng Zalo nếu cần. Chưa có danh sách hay gửi tin."
                    color: Theme.textFaint
                    font.pixelSize: 12
                    lineHeight: 1.25
                    wrapMode: Text.Wrap
                }
                Item { Layout.preferredHeight: 18 }
            }
        }
    }
}
