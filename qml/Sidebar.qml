import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    signal diagnosticsRequested()

    color: Theme.surface

    function focusSearch() {
        search.forceActiveFocus();
        search.selectAll();
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 66

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Zalo Native"
                    color: Theme.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    implicitWidth: 42
                    implicitHeight: 18
                    radius: 9
                    color: Theme.accentSoft

                    Text {
                        anchors.centerIn: parent
                        text: "BETA"
                        color: Theme.accentHover
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        font.letterSpacing: 0.6
                    }
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    icon: "info"
                    tooltip: "Chẩn đoán"
                    onClicked: root.diagnosticsRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 8
            radius: 10
            color: Theme.surfaceAlt
            border.width: 1
            border.color: search.activeFocus ? Theme.accent : "transparent"

            Icon {
                name: "search"
                color: Theme.textFaint
                size: 16
                anchors.left: parent.left
                anchors.leftMargin: 11
                anchors.verticalCenter: parent.verticalCenter
            }

            TextField {
                id: search
                anchors.fill: parent
                anchors.leftMargin: 32
                anchors.rightMargin: 26
                placeholderText: "Tìm kiếm"
                placeholderTextColor: Theme.textFaint
                color: Theme.text
                font.pixelSize: 13
                background: null
                onTextChanged: App.setSearchText(text)

                Keys.onEscapePressed: {
                    search.clear();
                    App.setSearchText("");
                }
            }

            IconButton {
                visible: search.text.length > 0
                icon: "close"
                iconSize: 14
                implicitWidth: 24
                implicitHeight: 24
                anchors.right: parent.right
                anchors.rightMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    search.clear();
                    App.setSearchText("");
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.conversations
            delegate: ConversationDelegate {}
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                visible: list.count === 0
                text: search.text.length > 0
                      ? "Không tìm thấy hội thoại phù hợp."
                      : "Chưa có hội thoại."
                color: Theme.textMuted
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 52

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                    width: 7
                    height: 7
                    radius: 3.5
                    color: App.runtimeDetected ? Theme.warning : Theme.textFaint
                }

                Text {
                    Layout.fillWidth: true
                    text: App.demoMode ? "Chế độ demo" : "Đã kết nối"
                    color: Theme.textMuted
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                Text {
                    text: "Chẩn đoán"
                    color: Theme.accentHover
                    font.pixelSize: 12
                    font.weight: Font.Medium

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.diagnosticsRequested()
                    }
                }
            }
        }
    }
}
