import ZaloNative 1.0
import QtQuick

Item {
    id: root

    width: ListView.view ? ListView.view.width : 0
    height: bubble.height + 14

    readonly property bool outgoing: model.outgoing
    readonly property bool hasAttachment: model.kind === "file" || model.kind === "image"

    Item {
        id: row

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        height: bubble.height

        Rectangle {
            id: bubble

            anchors.right: root.outgoing ? parent.right : undefined
            anchors.left: root.outgoing ? undefined : parent.left
            radius: 14
            color: root.outgoing ? Theme.accent : Theme.surfaceAlt
            border.width: root.outgoing ? 0 : 1
            border.color: Theme.border

            width: Math.min(content.implicitWidth + 24, root.width * 0.68)
            height: content.implicitHeight + 18

            Column {
                id: content

                anchors.centerIn: parent
                spacing: 4
                width: parent.width - 24

                Text {
                    visible: !root.outgoing && model.sender.length > 0
                    text: model.sender
                    color: Theme.accentHover
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }

                Row {
                    visible: root.hasAttachment
                    spacing: 10

                    Rectangle {
                        width: 34
                        height: 34
                        radius: 9
                        color: root.outgoing ? Qt.rgba(1, 1, 1, 0.18) : Theme.surfaceHover

                        Text {
                            anchors.centerIn: parent
                            text: model.kind === "image" ? "IMG" : "FILE"
                            color: root.outgoing ? "white" : Theme.textMuted
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }

                    Column {
                        spacing: 2

                        Text {
                            text: model.attachmentName
                            color: root.outgoing ? "white" : Theme.text
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                            width: bubble.width - 82
                        }

                        Text {
                            text: root.formatSize(model.attachmentSize)
                            color: root.outgoing ? Qt.rgba(1, 1, 1, 0.7) : Theme.textFaint
                            font.pixelSize: 11
                        }
                    }
                }

                Text {
                    visible: model.text.length > 0
                    width: parent.width
                    text: model.text
                    color: root.outgoing ? "white" : Theme.text
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }

    Row {
        anchors.top: row.top
        anchors.topMargin: -14
        anchors.right: root.outgoing ? row.right : undefined
        anchors.left: root.outgoing ? undefined : row.left
        spacing: 5

        Text {
            text: model.time
            color: Theme.textFaint
            font.pixelSize: 10
        }

        Icon {
            visible: root.outgoing
            name: "check"
            size: 11
            color: Theme.online
        }
    }

    function formatSize(bytes) {
        if (!bytes)
            return "";
        if (bytes < 1024)
            return bytes + " B";
        if (bytes < 1024 * 1024)
            return (bytes / 1024).toFixed(1) + " KB";
        return (bytes / (1024 * 1024)).toFixed(1) + " MB";
    }
}
