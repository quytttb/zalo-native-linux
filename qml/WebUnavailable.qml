import ZaloNative 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Shown when the binary was built without Qt WebEngine. Never fall back to the
// demo conversations here: that would look like a working client again.
Item {
    id: root

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, root.width - 80)
        spacing: 14

        Text {
            Layout.fillWidth: true
            text: "Bản build này không có Qt WebEngine"
            color: Theme.text
            font.pixelSize: 22
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            text: "Chế độ Web thử nghiệm cần Qt WebEngine. Backend Qt/C++ mặc định "
                  + "không cần WebEngine; chạy lại ứng dụng mà không có --web."
            color: Theme.textMuted
            font.pixelSize: 13
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: "Chế độ demo chỉ dùng để kiểm thử giao diện và không cần WebEngine: "
                  + "chạy zalonative --demo"
            color: Theme.textFaint
            font.pixelSize: 12
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
