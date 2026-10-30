import ZaloNative 1.0
import QtQuick

Rectangle {
    id: root

    property string icon: "more"
    property string tooltip: ""
    property color iconColor: Theme.textMuted
    property int iconSize: 20

    signal clicked()

    implicitWidth: 36
    implicitHeight: 36
    radius: 10
    color: hover.containsMouse ? Theme.surfaceHover : "transparent"
    opacity: enabled ? 1.0 : 0.45

    Icon {
        anchors.centerIn: parent
        name: root.icon
        color: root.iconColor
        size: root.iconSize
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
