import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property bool selected: App.currentId === model.id

    width: ListView.view ? ListView.view.width : 0
    height: 74

    Rectangle {
        anchors.fill: parent
        color: root.selected ? Theme.surfaceAlt
                             : (hover.containsMouse ? Theme.surfaceHover : "transparent")

        Rectangle {
            visible: root.selected
            width: 3
            height: parent.height - 20
            radius: 1.5
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accent
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        Avatar {
            text: model.avatarText
            tint: model.avatarColor
            size: 44
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: model.title
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: model.unread > 0 ? Font.DemiBold : Font.Medium
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: model.pinned
                    width: 5
                    height: 5
                    radius: 2.5
                    color: Theme.textFaint
                }

                Text {
                    text: model.time
                    color: model.unread > 0 ? Theme.accentHover : Theme.textFaint
                    font.pixelSize: 11
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: model.snippet
                    color: Theme.textMuted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: model.unread > 0 && !model.muted
                    implicitWidth: Math.max(19, label.implicitWidth + 10)
                    implicitHeight: 19
                    radius: 9.5
                    color: Theme.accent

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: model.unread > 99 ? "99+" : String(model.unread)
                        color: "white"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: App.openConversation(model.id)
    }
}
