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
                        border.color: App.demoMode
                                      ? Theme.border
                                      : (App.runtimeHasCore ? Theme.warning
                                         : (App.runtimeDetected ? Theme.danger : Theme.border))

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
                                    color: App.demoMode
                                           ? Theme.textFaint
                                           : (App.runtimeHasCore ? Theme.warning
                                              : (App.runtimeDetected ? Theme.danger : Theme.border))
                                }

                                Text {
                                    text: App.demoMode
                                          ? "Đang chạy dữ liệu demo"
                                          : (App.nativeMode
                                             ? "Backend Qt/C++ thử nghiệm"
                                             : (App.webMode
                                             ? (App.rendererState === "ready" ? "Trang Zalo Web đã tải"
                                                : (App.rendererState === "error" ? "Không tải được Zalo"
                                                   : "Đang mở Zalo Web"))
                                             : (App.runtimeHasCore ? "Đã nhận diện lõi Zalo PC"
                                                : (App.runtimeDetected ? "Runtime không host được"
                                                   : "Chưa có runtime"))))
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: App.nativeMode ? App.qrLogin.status : App.modeLabel
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
                        FieldValue { text: App.modeLabel }

                        FieldLabel { text: App.nativeMode ? "Backend" : "Khung hiển thị" }
                        FieldValue {
                            text: App.nativeMode ? "Qt/C++ trực tiếp, không cần renderer Zalo PC"
                                  : (App.rendererHostEnabled ? "Qt WebEngine có trong bản build"
                                     : "Không có trong bản build này")
                        }

                        FieldLabel { text: "Phiên bản runtime"; visible: App.runtimeDiagnosticsMode }
                        FieldValue {
                            visible: App.runtimeDiagnosticsMode
                            text: App.runtimeAppVersion.length > 0
                                  ? App.runtimeAppVersion : "(chưa nhận diện)"
                        }

                        FieldLabel { text: "Entry HTML"; visible: App.runtimeDiagnosticsMode }
                        FieldValue {
                            visible: App.runtimeDiagnosticsMode
                            text: App.runtimeEntryHtml.length > 0
                                  ? App.runtimeEntryHtml : "(không có)"
                        }

                        FieldLabel { text: "Preload / sync"; visible: App.runtimeDiagnosticsMode }
                        FieldValue {
                            visible: App.runtimeDiagnosticsMode
                            text: App.runtimeExposedApiCount + " API · "
                                  + App.runtimeSyncCallSites + " sync call site ("
                                  + App.runtimeDynamicSyncCalls + " kênh động)"
                        }

                        FieldLabel { text: "Trang hiện tại" }
                        FieldValue {
                            text: App.webMode
                                  ? (App.currentPageUrl.length > 0 ? App.currentPageUrl : "(chưa tải)")
                                  : "Không áp dụng"
                        }

                        FieldLabel { text: "Hội thoại / tin nhắn" }
                        FieldValue {
                            text: App.demoMode
                                  ? App.conversationCount + " / " + App.messageCount
                                  : "Chưa có dữ liệu chat thật"
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        text: "Ứng dụng không chính thức, không liên kết với VNG. "
                               + (App.webMode
                                  ? "Chế độ Web thử nghiệm mở trang Zalo Web chính thức; cookie phiên chỉ nằm trong profile cục bộ và không được ghi vào nhật ký."
                                  : (App.runtimeDiagnosticsMode
                                     ? "Runtime do người dùng cung cấp; hash cục bộ không xác thực nhà phát hành."
                                     : "Backend C++ đang thử nghiệm, chưa hỗ trợ chat thật."))
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
                    text: App.runtimeDiagnosticsMode
                          ? (App.runtimeDetected ? App.runtimeManifestJson
                             : "Không có manifest runtime. Đây là công cụ nghiên cứu lịch sử; "
                               + "dùng `zalort inspect <app-dir>` để kiểm tra nguồn do người dùng cung cấp.")
                          : "Backend Qt/C++ mặc định không cần runtime Zalo PC. "
                            + "Chạy `zalonative --runtime-diagnostics` để mở công cụ nghiên cứu lịch sử."
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
                        text: "Hoạt động gần đây của shell. QR, cookie và token không được ghi."
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
