import ZaloNative 1.0
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
    title: App.demoMode ? "Zalo Native — Demo"
                        : (App.webMode ? "Zalo Native — Web thử nghiệm" : "Zalo Native")
    color: Theme.background

    readonly property bool wide: width >= 980
    property bool showSidebar: true

    Shortcut {
        sequences: ["Ctrl+K", "Ctrl+F"]
        enabled: App.demoMode
        onActivated: sidebar.focusSearch()
    }

    Shortcut {
        sequence: "Ctrl+R"
        enabled: !App.demoMode
        onActivated: {
            if (realShell.item && realShell.item.reload)
                realShell.item.reload();
            else if (App.runtimeDiagnosticsMode)
                App.refreshRuntime();
        }
    }

    Shortcut {
        sequences: ["Ctrl+N"]
        enabled: App.demoMode
        onActivated: App.reloadDemo()
    }

    Shortcut {
        sequence: "Ctrl+D"
        onActivated: diagnostics.open()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Item {
        anchors.fill: parent

        // ------------------------------------------------------------------
        // Demo shell (explicit opt-in with --demo, used by the offline tests)
        // ------------------------------------------------------------------
        Item {
            anchors.fill: parent
            visible: App.demoMode

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
                    text: "Dữ liệu demo — chưa kết nối Zalo"
                    color: Theme.textMuted
                    font.pixelSize: 11
                }
            }
        }

        // ------------------------------------------------------------------
        // Native C++ backend by default; legacy diagnostics and Web are opt-in.
        // ------------------------------------------------------------------
        Loader {
            id: realShell
            anchors.fill: parent
            visible: !App.demoMode
            source: App.webMode
                    ? (App.rendererHostEnabled ? "WebShell.qml" : "WebUnavailable.qml")
                    : (App.runtimeDiagnosticsMode ? "RuntimeShell.qml" : "NativeShell.qml")

            onStatusChanged: {
                if (status === Loader.Error)
                    console.error("failed to load component: " + source);
            }

            onLoaded: {
                if (item && item.diagnosticsRequested)
                    item.diagnosticsRequested.connect(diagnostics.open);
                if (item && item.view)
                    item.view.forceActiveFocus();
            }
        }
    }

    DiagnosticsDialog {
        id: diagnostics
    }

    Component.onCompleted: {
        if (App.demoMode && wide)
            chat.focusComposer();
    }
}
