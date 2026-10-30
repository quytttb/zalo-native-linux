import ZaloNative 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebEngine

// Native frame around the official Zalo Web client. The Electron renderer from
// the Zalo PC package cannot run in Qt WebEngine (it needs Electron's preload
// IPC and native modules), so this host loads the same official web client that
// Zalo publishes for browsers. The session lives in a dedicated persistent
// profile and is never written to logs.
Item {
    id: root

    signal diagnosticsRequested()

    property string loadError: ""

    readonly property string stateLabel: {
        switch (App.rendererState) {
        case "loading": return "Đang tải…";
        case "login": return "Đăng nhập bằng mã QR";
        case "ready": return "Trang Zalo Web đã tải";
        case "browsing": return "Đang mở trang ngoài";
        case "error": return "Không tải được";
        default: return "Sẵn sàng";
        }
    }

    WebEngineProfilePrototype {
        id: profilePrototype
        storageName: "ZaloNative"
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        httpCacheType: WebEngineProfile.DiskHttpCache
        httpCacheMaximumSize: 512 * 1024 * 1024
    }

    readonly property color stateColor: {
        switch (App.rendererState) {
        case "ready": return Theme.online;
        case "login": return Theme.warning;
        case "error": return Theme.danger;
        case "loading": return Theme.accentHover;
        default: return Theme.textFaint;
        }
    }

    function reload() {
        root.loadError = "";
        view.reload();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Theme.surface
            z: 2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 4

                Text {
                    text: "Zalo"
                    color: "#3D8BFF"
                    font.pixelSize: 21
                    font.weight: Font.Bold
                    font.letterSpacing: -0.4
                }

                Rectangle {
                    width: webLabel.implicitWidth + 12
                    height: 20
                    radius: 6
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.warning

                    Text {
                        id: webLabel
                        anchors.centerIn: parent
                        text: "Web thử nghiệm"
                        color: Theme.warning
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }

                Item { Layout.preferredWidth: 10 }

                IconButton {
                    icon: "back"
                    tooltip: "Quay lại"
                    enabled: view.canGoBack
                    onClicked: view.goBack()
                }

                IconButton {
                    icon: "forward"
                    tooltip: "Tiến tới"
                    enabled: view.canGoForward
                    onClicked: view.goForward()
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    height: 28
                    width: statusRow.implicitWidth + 20
                    radius: 9
                    color: Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border

                    RowLayout {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 7

                        Rectangle {
                            width: 7
                            height: 7
                            radius: 3.5
                            color: root.stateColor
                        }

                        Text {
                            text: root.stateLabel
                            color: Theme.textMuted
                            font.pixelSize: 11
                        }
                    }
                }

                IconButton {
                    icon: "reload"
                    tooltip: "Tải lại (Ctrl+R)"
                    onClicked: root.reload()
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
            Layout.preferredHeight: 2
            color: Theme.border
            visible: view.loading || view.loadProgress < 100 && view.loadProgress > 0

            Rectangle {
                width: parent.width * Math.max(0.02, Math.min(1, view.loadProgress / 100))
                height: parent.height
                color: Theme.accent

                Behavior on width {
                    NumberAnimation { duration: 120 }
                }
            }
        }

        WebEngineView {
            id: view

            Layout.fillWidth: true
            Layout.fillHeight: true

            url: ""

            settings.javascriptEnabled: true
            settings.localStorageEnabled: true
            settings.javascriptCanOpenWindows: true
            settings.javascriptCanAccessClipboard: true
            settings.fullScreenSupportEnabled: true
            settings.playbackRequiresUserGesture: false
            settings.autoLoadImages: true
            settings.webGLEnabled: true
            settings.errorPageEnabled: false
            settings.focusOnNavigationEnabled: true

            onLoadingChanged: function(loadRequest) {
                if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                    root.loadError = "";
                    App.onRendererLoadingChanged(true);
                } else if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                    App.onRendererLoaded();
                    App.onRendererPageChanged(url.toString(), title);
                } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                    root.loadError = loadRequest.errorString;
                    App.onRendererFailed(loadRequest.errorString);
                }
            }

            onUrlChanged: App.onRendererPageChanged(url.toString(), title)
            onTitleChanged: App.onRendererPageChanged(url.toString(), title)

            onFeaturePermissionRequested: function(securityOrigin, feature) {
                const origin = securityOrigin.toString();
                const trusted = origin.indexOf("zalo.me") >= 0
                        || origin.indexOf("zaloapp.com") >= 0;
                const allowed = feature === WebEngineView.Notifications
                        || feature === WebEngineView.ClipboardReadWrite
                        || feature === WebEngineView.MediaAudioCapture
                        || feature === WebEngineView.MediaVideoCapture
                        || feature === WebEngineView.MediaAudioVideoCapture;
                const grant = trusted && allowed;
                grantFeaturePermission(securityOrigin, feature, grant);
                App.logRenderer("permission " + (grant ? "granted" : "denied")
                                + " (" + feature + ") for " + origin);
            }

            onNewWindowRequested: function(request) {
                const target = request.requestedUrl.toString();
                let origin = "";
                try { origin = new URL(target).origin; } catch (e) { origin = "(invalid URL)"; }
                // Query strings can carry session hand-off parameters: log only
                // the origin, never the full URL.
                App.logRenderer("new window request: " + origin);
                if (target.indexOf("https://") === 0 || target.indexOf("http://") === 0)
                    Qt.openUrlExternally(target);
            }

            onRenderProcessTerminated: function(terminationStatus, exitCode) {
                root.loadError = "Tiến trình hiển thị đã dừng (mã " + exitCode + ").";
                App.onRendererFailed("render process terminated: status "
                                     + terminationStatus + ", exit " + exitCode);
            }

            onFullScreenRequested: function(request) {
                request.accept();
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 50
        visible: root.loadError.length > 0
        color: "#CC0E1015"

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(520, root.width - 80)
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: "Không tải được Zalo"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: root.loadError + "\n\nKiểm tra kết nối mạng rồi thử lại."
                color: Theme.textMuted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Button {
                    text: "Thử lại"
                    onClicked: root.reload()
                }

                Button {
                    text: "Mở bằng trình duyệt"
                    onClicked: Qt.openUrlExternally(App.currentPageUrl.length > 0
                                                    ? App.currentPageUrl
                                                    : App.rendererUrl)
                }
            }
        }
    }

    Component.onCompleted: {
        // Qt 6.9+: create a fully configured profile before the first page is
        // created. The legacy `WebEngineProfile {}` element is deprecated.
        const profile = profilePrototype.instance();
        if (profile)
            view.profile = profile;
        view.url = App.rendererUrl;
    }

}
