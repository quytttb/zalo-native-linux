import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: window

    visible: true
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 560
    title: "Zalo Native — bản thử nghiệm"
    color: Theme.background

    readonly property bool wide: width >= 980
    property bool showSidebar: true

    Shortcut {
        sequences: ["Ctrl+K", "Ctrl+F"]
        onActivated: sidebar.focusSearch()
    }

    Shortcut {
        sequences: ["Ctrl+N"]
        onActivated: App.reloadDemo()
    }

    Shortcut {
        sequences: ["Ctrl+D"]
        onActivated: diagnostics.open()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Item {
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                id: sidebar
                Layout.preferredWidth: window.wide ? 328 : 280
                Layout.fillHeight: true
                visible: window.wide || window.showSidebar
                onDiagnosticsRequested: diagnostics.open()
            }

            ChatPane {
                id: chat
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        // Persistent banner so nobody mistakes an early build for a working client.
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12
            width: bannerLabel.implicitWidth + 22
            height: 26
            radius: 13
            color: Theme.surfaceAlt
            border.width: 1
            border.color: Theme.border
            opacity: 0.9

            Text {
                id: bannerLabel
                anchors.centerIn: parent
                text: App.demoMode ? "Dữ liệu demo — chưa kết nối Zalo" : "Runtime thử nghiệm"
                color: Theme.textMuted
                font.pixelSize: 11
            }
        }
    }

    DiagnosticsDialog {
        id: diagnostics
    }

    Component.onCompleted: {
        if (wide)
            chat.focusComposer();
    }
}
