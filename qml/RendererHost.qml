import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtWebChannel
import QtWebEngine

// Optional Qt WebEngine host. Milestone 0 only loads the bundled bridge demo;
// milestone 2 points `url` at the real Zalo entry document served through the
// private `zalo-app://` scheme. This component is only compiled when the
// WebEngine development files are available (ZALO_WEBENGINE=ON).
Item {
    id: root

    property url url: App.rendererUrl
    readonly property alias view: view

    WebEngineView {
        id: view
        anchors.fill: parent
        url: root.url
        settings.javascriptEnabled: true
        settings.localContentCanAccessRemoteUrls: false
        settings.localContentCanAccessFileUrls: false
        settings.errorPageEnabled: true

        webChannel: WebChannel {
            id: channel

            Component.onCompleted: {
                // Imperative registration is required because App.bridgeObject is
                // a C++ object and cannot carry the WebChannel.id attached property.
                channel.registerObject("bridge", App.bridgeObject);
            }
        }

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                App.onRendererLoaded();
                App.logRenderer("loaded " + loadRequest.url);
            } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                App.onRendererFailed(loadRequest.errorString + " (" + loadRequest.url + ")");
            }
        }

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            App.onRendererFailed("render process terminated: status " + terminationStatus
                                 + ", exit " + exitCode);
        }

        onNewWindowRequested: function(request) {
            App.logRenderer("blocked a new window request: " + request.requestedUrl);
        }
    }
}
