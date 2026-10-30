import QtQuick

Rectangle {
    id: root

    property string text: "?"
    property color tint: Theme.accent
    property int size: 44

    width: size
    height: size
    radius: size / 2
    color: root.tint

    Text {
        anchors.centerIn: parent
        text: root.text
        color: "white"
        font.pixelSize: Math.max(10, Math.round(root.size * 0.34))
        font.weight: Font.DemiBold
    }
}
