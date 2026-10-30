import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string draft: ""

    function focusComposer() {
        composer.forceActiveFocus();
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header -------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 16
                spacing: 12

                Avatar {
                    text: App.currentAvatarText
                    tint: App.currentAvatarColor
                    size: 38
                    visible: App.currentId.length > 0
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: App.currentId.length > 0 ? App.currentTitle : "Chọn một hội thoại"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    RowLayout {
                        spacing: 5
                        visible: App.currentId.length > 0

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: App.currentOnline ? Theme.online : Theme.textFaint
                        }

                        Text {
                            text: App.currentSubtitle
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }
                    }
                }

                IconButton {
                    icon: "phone"
                    tooltip: "Cuộc gọi chưa được hỗ trợ"
                    enabled: false
                }

                IconButton {
                    icon: "more"
                    tooltip: "Tùy chọn"
                    onClicked: optionsMenu.open()
                }
            }
        }

        // Message list -------------------------------------------------------
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: App.currentId.length > 0 && App.messages.count > 0 ? 0 : 1

            ListView {
                id: messageList
                clip: true
                model: App.messages
                delegate: MessageDelegate {}
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                topMargin: 26
                bottomMargin: 12

                onCountChanged: Qt.callLater(function() {
                    if (messageList.count > 0)
                        messageList.positionViewAtEnd();
                })

                Component.onCompleted: Qt.callLater(function() {
                    if (messageList.count > 0)
                        messageList.positionViewAtEnd();
                })
            }

            Item {
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 80
                    text: App.currentId.length > 0
                          ? "Chưa có tin nhắn trong hội thoại này."
                          : "Chọn một hội thoại ở thanh bên trái để bắt đầu.\n\nĐây là giao diện demo; chưa kết nối tài khoản Zalo."
                    color: Theme.textMuted
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                }
            }
        }

        // Composer -----------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            color: Theme.surface

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 10

                IconButton {
                    icon: "attach"
                    tooltip: "Gửi tệp chưa được hỗ trợ"
                    enabled: false
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 12
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: composer.activeFocus ? Theme.accent : Theme.border

                    TextField {
                        id: composer
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        placeholderText: "Nhập tin nhắn…"
                        placeholderTextColor: Theme.textFaint
                        color: Theme.text
                        font.pixelSize: 14
                        background: null
                        enabled: App.currentId.length > 0
                        selectByMouse: true

                        onAccepted: root.send()

                        Keys.onEscapePressed: composer.clear()

                        Connections {
                            target: App
                            function onCurrentChanged() {
                                composer.clear();
                                composer.forceActiveFocus();
                            }
                        }
                    }
                }

                Rectangle {
                    width: 42
                    height: 42
                    radius: 12
                    color: composer.text.trim().length > 0 ? Theme.accent : Theme.surfaceAlt

                    Icon {
                        anchors.centerIn: parent
                        name: "send"
                        size: 18
                        color: composer.text.trim().length > 0 ? "white" : Theme.textFaint
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.send()
                    }
                }
            }
        }
    }

    Menu {
        id: optionsMenu

        MenuItem {
            text: "Tải lại dữ liệu demo"
            onTriggered: App.reloadDemo()
        }

        MenuItem {
            text: "Xóa nhật ký chẩn đoán"
            onTriggered: App.clearDiagnostics()
        }

        MenuSeparator {}

        MenuItem {
            text: "Tin nhắn chỉ lưu cục bộ trong bản demo"
            enabled: false
        }
    }

    function send() {
        const text = composer.text;
        if (!text.trim())
            return;
        if (App.sendMessage(text)) {
            composer.clear();
            composer.forceActiveFocus();
        }
    }

    Component.onCompleted: composer.forceActiveFocus()
}
