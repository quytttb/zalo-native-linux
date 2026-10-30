import ZaloNative 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property string page: "overview"

    title: "Chẩn đoán"
    modal: true
    anchors.centerIn: parent
    width: Math.min(720, parent ? parent.width - 80 : 720)
    height: Math.min(620, parent ? parent.height - 80 : 620)
    padding: 0
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        // Header -------------------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: root.title
                    color: Theme.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    icon: "close"
                    onClicked: root.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        // Tabs ---------------------------------------------------------------
        Row {
            Layout.leftMargin: 20
            Layout.topMargin: 12
            spacing: 6

            Repeater {
                model: [
                    { key: "overview", label: "Tổng quan" },
                    { key: "manifest", label: "Runtime" },
                    { key: "log", label: "Nhật ký" }
                ]

                Rectangle {
                    required property var modelData

                    width: tabLabel.implicitWidth + 24
                    height: 30
                    radius: 9
                    color: root.page === modelData.key ? Theme.accentSoft : "transparent"

                    Text {
                        id: tabLabel
                        anchors.centerIn: parent
                        text: modelData.label
                        color: root.page === modelData.key ? Theme.accentHover : Theme.textMuted
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.page = modelData.key
                    }
                }
            }
        }

        // Pages --------------------------------------------------------------
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            currentIndex: root.page === "overview" ? 0 : (root.page === "manifest" ? 1 : 2)

            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: statusColumn.implicitHeight + 32
                        radius: 12
                        color: Theme.surfaceAlt
                        border.width: 1
                        border.color: App.runtimeDetected ? Theme.warning : Theme.border

                        ColumnLayout {
                            id: statusColumn
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 6

                            RowLayout {
                                spacing: 8

                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: App.runtimeDetected ? Theme.warning : Theme.textFaint
                                }

                                Text {
                                    text: App.runtimeDetected ? "Đã phát hiện runtime Zalo" : "Chưa phát hiện runtime"
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: App.runtimeStatus
                                color: Theme.textMuted
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 10

                        component FieldLabel: Text {
                            color: Theme.textFaint
                            font.pixelSize: 12
                        }

                        component FieldValue: Text {
                            Layout.fillWidth: true
                            color: Theme.text
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        FieldLabel { text: "Chế độ" }
                        FieldValue {
                            text: App.demoMode ? "Demo (dữ liệu giả lập)" : "Runtime"
                        }

                        FieldLabel { text: "Renderer host" }
                        FieldValue {
                            text: App.rendererHostEnabled
                                  ? "Bật — Qt WebEngine"
                                  : "Không có trong bản build này"
                        }

                        FieldLabel { text: "Giao thức bridge" }
                        FieldValue { text: "zalonative.bridge/1" }

                        FieldLabel { text: "Hội thoại / tin nhắn" }
                        FieldValue {
                            text: App.conversationCount + " / " + App.messageCount
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: "Bản dựng thử nghiệm, không liên kết với VNG. Không đăng nhập tài khoản thật trong mốc 0."
                        color: Theme.textFaint
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                }
            }

            ScrollView {
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    readOnly: true
                    text: App.runtimeDetected
                          ? App.runtimeManifestJson
                          : "Không có manifest. Đặt runtime đã giải nén vào:\n" + "$XDG_DATA_HOME/zalo-native/runtime\nhoặc truyền --runtime <path>."
                    color: Theme.text
                    font.family: "monospace"
                    font.pixelSize: 12
                    wrapMode: TextArea.WrapAnywhere
                    selectByMouse: true
                    background: Rectangle {
                        color: Theme.background
                        radius: 10
                        border.width: 1
                        border.color: Theme.border
                    }
                }
            }

            ColumnLayout {
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: "Hoạt động gần đây của shell, runtime và bridge."
                        color: Theme.textMuted
                        font.pixelSize: 12
                    }

                    Button {
                        text: "Xóa"
                        enabled: App.diagnosticsText.length > 0
                        onClicked: App.clearDiagnostics()

                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? Theme.text : Theme.textFaint
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            implicitWidth: 64
                            implicitHeight: 30
                            radius: 8
                            color: parent.hovered ? Theme.surfaceHover : Theme.surfaceAlt
                            border.width: 1
                            border.color: Theme.border
                        }
                    }
                }

                TextArea {
                    id: logArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    readOnly: true
                    text: App.diagnosticsText.length > 0 ? App.diagnosticsText : "(trống)"
                    color: Theme.textMuted
                    font.family: "monospace"
                    font.pixelSize: 11
                    wrapMode: TextArea.WrapAnywhere
                    selectByMouse: true
                    background: Rectangle {
                        color: Theme.background
                        radius: 10
                        border.width: 1
                        border.color: Theme.border
                    }

                    onTextChanged: cursorPosition = length
                    Connections {
                        target: App
                        function onDiagnosticsChanged() {
                            logArea.cursorPosition = logArea.length;
                        }
                    }
                }
            }
        }
    }
}
