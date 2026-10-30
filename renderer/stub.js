/* Bridge demo for milestone 0. No Zalo code, cookies or account data. */
"use strict";

(function () {
    const statusDot = document.getElementById("status-dot");
    const statusText = document.getElementById("status-text");
    const output = document.getElementById("output");

    const fields = {
        ping: document.getElementById("ping"),
        protocol: document.getElementById("protocol"),
        runtime: document.getElementById("runtime"),
        snapshot: document.getElementById("snapshot"),
    };

    let bridge = null;
    let sequence = 0;

    function log(message) {
        const stamp = new Date().toLocaleTimeString();
        output.textContent = stamp + "  " + message + "\n" + output.textContent;
        output.textContent = output.textContent.split("\n").slice(0, 40).join("\n");
    }

    function call(path, args) {
        if (!bridge) {
            return Promise.reject(new Error("bridge chưa sẵn sàng"));
        }

        const request = {
            id: "web-" + (++sequence),
            kind: "call",
            path: path,
            args: args || [],
        };

        return new Promise((resolve, reject) => {
            let parsed;
            try {
                parsed = JSON.parse(bridge.requestJson(JSON.stringify(request)));
            } catch (error) {
                reject(error);
                return;
            }
            if (!parsed.ok) {
                const detail = parsed.error || {};
                reject(new Error(detail.code + ": " + detail.message));
                return;
            }
            resolve(parsed.value);
        });
    }

    function describe(value) {
        if (value === null || value === undefined) {
            return "—";
        }
        if (typeof value === "string") {
            return value;
        }
        return JSON.stringify(value);
    }

    function connect() {
        if (typeof QWebChannel === "undefined" || !window.qt || !window.qt.webChannelTransport) {
            statusText.textContent = "Không có QWebChannel transport";
            log("Trang đang chạy ngoài Qt WebEngine; bridge không khả dụng.");
            return;
        }

        new QWebChannel(window.qt.webChannelTransport, function (channel) {
            bridge = channel.objects.bridge;
            if (!bridge) {
                statusText.textContent = "Không thấy đối tượng bridge";
                return;
            }

            statusDot.classList.add("online");
            statusText.textContent = "Đã kết nối shell native";
            log("QWebChannel đã kết nối.");

            if (bridge.event && bridge.event.connect) {
                bridge.event.connect(function (payload) {
                    log("event: " + payload);
                });
            }

            Promise.all([
                call("app.ping"),
                call("app.info"),
                call("runtime.info"),
                call("state.snapshot"),
            ]).then(function (results) {
                fields.ping.textContent = describe(results[0]);
                fields.protocol.textContent = describe(results[1].protocol);
                fields.runtime.textContent = results[2] && results[2].valid
                    ? (results[2].name || "Zalo") + " " + (results[2].appVersion || "?")
                    : "chưa cấu hình";
                const snapshot = results[3] || {};
                fields.snapshot.textContent = Array.isArray(snapshot.conversations)
                    ? snapshot.conversations.length + " hội thoại"
                    : "trống";
            }).catch(function (error) {
                log("✗ " + error.message);
            });
        });
    }

    document.getElementById("ping-button").addEventListener("click", function () {
        call("app.ping")
            .then(function (value) {
                fields.ping.textContent = describe(value);
                log("app.ping → " + describe(value));
            })
            .catch(function (error) {
                log("✗ " + error.message);
            });
    });

    document.getElementById("publish-button").addEventListener("click", function () {
        const state = {
            source: "renderer-stub",
            conversations: [
                {
                    id: "renderer-demo",
                    title: "Tin nhắn từ renderer",
                    snippet: "Trạng thái do trang web tự công bố",
                    time: "bây giờ",
                    unread: 0,
                    pinned: false,
                    muted: false,
                    online: false,
                    avatarText: "R",
                    avatarColor: "#3D6DFF",
                    lastActivityTs: Math.floor(Date.now() / 1000),
                },
            ],
            messages: {
                "renderer-demo": [
                    {
                        id: "renderer-1",
                        sender: "Renderer",
                        text: "Đây là dữ liệu do stub.js công bố qua state.publish.",
                        time: "bây giờ",
                        outgoing: false,
                        status: "sent",
                        kind: "text",
                    },
                ],
            },
        };

        call("state.publish", [state])
            .then(function (value) {
                fields.snapshot.textContent = value.conversations + " hội thoại";
                log("state.publish → " + describe(value));
            })
            .catch(function (error) {
                log("✗ " + error.message);
            });
    });

    document.getElementById("log-button").addEventListener("click", function () {
        call("log.write", ["xin chào từ renderer stub"])
            .then(function () {
                log("log.write → ok");
            })
            .catch(function (error) {
                log("✗ " + error.message);
            });
    });

    connect();
})();
