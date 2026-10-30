import ZaloNative 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Native-runtime mode. This screen reports what the host found in the
// user-supplied Zalo desktop runtime. It deliberately does not pretend to be a
// chat UI: the official renderer/bootstrap is not hosted yet.
Item {
    id: root

    signal diagnosticsRequested()

    function runtimeFolderUrl() {
        if (App.runtimeAppDir.length === 0)
            return "";
        return "file://" + App.runtimeAppDir;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header -------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: "Zalo"
                    color: "#3D8BFF"
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    font.letterSpacing: -0.4
                }

                Rectangle {
                    width: nativeLabel.implicitWidth + 14
                    height: 20
                    radius: 6
                    color: Theme.accentSoft
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        id: nativeLabel
                        anchors.centerIn: parent
                        text: "Runtime native"
                        color: Theme.accentHover
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    icon: "reload"
                    tooltip: "Đọc lại runtime"
                    onClicked: App.refreshRuntime()
                }

                IconButton {
                    icon: "info"
                    tooltip: "Chẩn đoán (Ctrl+D)"
                    onClicked: root.diagnosticsRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: Math.min(860, root.width - 48)
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 28
                spacing: 14

                Text {
                    Layout.fillWidth: true
                    text: App.runtimeHasCore ? "Lõi Zalo PC đã được nhận diện"
                                             : "Chưa sẵn sàng để host lõi Zalo"
                    color: Theme.text
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    text: App.modeLabel
                    color: Theme.textMuted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                }

                // Status card -------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: statusText.implicitHeight + 32
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: App.runtimeHasCore ? Theme.warning
                               : (App.runtimeDetected ? Theme.danger : Theme.border)

                    Text {
                        id: statusText
                        anchors.fill: parent
                        anchors.margins: 16
                        text: App.runtimeStatus
                        color: Theme.text
                        font.pixelSize: 13
                        lineHeight: 1.25
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                    }
                }

                // Runtime facts ----------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: App.runtimeDetected
                    Layout.preferredHeight: facts.implicitHeight + 32
                    radius: Theme.radius
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border

                    GridLayout {
                        id: facts
                        anchors.fill: parent
                        anchors.margins: 16
                        columns: root.width > 700 ? 2 : 1
                        columnSpacing: 24
                        rowSpacing: 9

                        component FactLabel: Text {
                            color: Theme.textFaint
                            font.pixelSize: 12
                        }

                        component FactValue: Text {
                            Layout.fillWidth: true
                            color: Theme.text
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }

                        FactLabel { text: "Phiên bản" }
                        FactValue { text: App.runtimeAppVersion.length > 0 ? App.runtimeAppVersion : "(không rõ)" }

                        FactLabel { text: "Nguồn" }
                        FactValue {
                            text: App.runtimeSourceKind === "app-dir"
                                  ? "Thư mục ứng dụng đã giải nén (nguồn do người dùng cung cấp, chưa xác minh)"
                                  : "ASAR " + (App.runtimeHasCore ? "có entry" : "launcher — không host được")
                        }

                        FactLabel { text: "Entry HTML" }
                        FactValue { text: App.runtimeEntryHtml.length > 0 ? App.runtimeEntryHtml : "(không có)" }

                        FactLabel { text: "Thư mục lõi" }
                        FactValue { text: App.runtimeAppDir.length > 0 ? App.runtimeAppDir : "(không có)" }

                        FactLabel { text: "API preload (static)" }
                        FactValue { text: App.runtimeExposedApiCount + " tên exposeInMainWorld" }

                        FactLabel { text: "Lời gọi sync" }
                        FactValue {
                            text: App.runtimeSyncCallSites + " call site, "
                                  + App.runtimeDynamicSyncCalls + " kênh động"
                        }

                        FactLabel { text: "Native library" }
                        FactValue { text: App.runtimeNativeLibraryCount + " file .node/.so/helper" }
                    }
                }

                // Host blocker ------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: App.runtimeHasCore
                    Layout.preferredHeight: blockerText.implicitHeight + 32
                    radius: Theme.radius
                    color: "#2A2318"
                    border.width: 1
                    border.color: Theme.warning

                    Text {
                        id: blockerText
                        anchors.fill: parent
                        anchors.margins: 16
                        text: "Qt host chưa bootstrap lõi này. Màn hình này không phải chat và "
                              + "không có phiên đăng nhập.\n\n"
                              + "Blocker M1/M2: preload dùng ipcRenderer.sendSync và nhiều "
                              + "API contextBridge; QWebChannel chỉ bất đồng bộ. Cần chứng minh "
                              + "một cơ chế sync an toàn (ví dụ custom scheme + sync XHR cùng "
                              + "origin) trước khi nối UI chat."
                        color: Theme.text
                        font.pixelSize: 12
                        lineHeight: 1.25
                        wrapMode: Text.Wrap
                    }
                }

                // Exposed APIs ------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: App.runtimeExposedApis.length > 0
                    Layout.preferredHeight: apiColumn.implicitHeight + 28
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: apiColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        Text {
                            text: "API preload phát hiện được (chỉ là inventory tĩnh)"
                            color: Theme.textMuted
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: App.runtimeExposedApis

                                Rectangle {
                                    required property string modelData
                                    width: apiLabel.implicitWidth + 14
                                    height: 24
                                    radius: 7
                                    color: Theme.surfaceAlt
                                    border.width: 1
                                    border.color: Theme.border

                                    Text {
                                        id: apiLabel
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: Theme.textMuted
                                        font.family: "monospace"
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }
                }

                // Warnings ----------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: App.runtimeWarnings.length > 0
                    Layout.preferredHeight: warningsColumn.implicitHeight + 28
                    radius: Theme.radius
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: warningsColumn
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Text {
                            text: "Cảnh báo của bộ đọc runtime"
                            color: Theme.textMuted
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Repeater {
                            model: App.runtimeWarnings

                            Text {
                                required property string modelData
                                Layout.fillWidth: true
                                text: "• " + modelData
                                color: Theme.textFaint
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                            }
                        }
                    }
                }

                // Empty state / commands -------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: !App.runtimeDetected
                    Layout.preferredHeight: emptyColumn.implicitHeight + 32
                    radius: Theme.radius
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: emptyColumn
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Text {
                            text: "Cách cung cấp runtime"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "1. Giải nén bộ cài Zalo PC chính thức do bạn tự cung cấp.\n"
                                  + "2. Trỏ tới thư mục ứng dụng có pc-dist/main-dist (không phải ASAR launcher).\n"
                                  + "3. Import vào profile riêng hoặc chạy trực tiếp với --runtime."
                            color: Theme.textMuted
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 8
                            color: Theme.background
                            border.width: 1
                            border.color: Theme.border

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                text: "zalort import <app-dir> ~/.local/share/zalo-native/runtime"
                                color: Theme.textMuted
                                font.family: "monospace"
                                font.pixelSize: 11
                            }
                        }

                        Button {
                            text: "Sao chép lệnh import"
                            onClicked: {
                                importClipboard.text = "zalort import <app-dir> ~/.local/share/zalo-native/runtime";
                                importClipboard.selectAll();
                                importClipboard.copy();
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    spacing: 8

                    Button {
                        text: "Mở thư mục runtime"
                        enabled: App.runtimeAppDir.length > 0
                        onClicked: Qt.openUrlExternally(root.runtimeFolderUrl())
                    }

                    Button {
                        text: "Đọc lại"
                        onClicked: App.refreshRuntime()
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Chẩn đoán"
                        onClicked: root.diagnosticsRequested()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 24
                    text: "Ứng dụng không chính thức, không liên kết với VNG/Zalo. Không có mã, "
                          + "binary hay session Zalo trong repo. Runtime do người dùng cung cấp; "
                          + "hash cục bộ không xác thực nhà phát hành."
                    color: Theme.textFaint
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    TextEdit {
        id: importClipboard
        visible: false
    }
}
