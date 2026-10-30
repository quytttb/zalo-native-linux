// Experimental manual QR probe. Protocol reference: zca-js (MIT),
// https://github.com/RFS-ADRENO/zca-js/tree/2a7e360257dfe98e2eb48dc46bbf389834be7e0b
// This implementation is independently written; no zca-js source is bundled.
#include "QrProtocol.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <functional>

class Probe : public QDialog
{
public:
    explicit Probe(bool startImmediately = false)
    {
        setWindowTitle(QStringLiteral("Zalo native — QR feasibility probe"));
        auto *layout = new QVBoxLayout(this);
        m_status = new QLabel(QStringLiteral("Thử nghiệm không chính thức. Phiên chỉ nằm trong bộ nhớ.\n"
                                             "Quét QR có thể ảnh hưởng phiên Zalo đang dùng."));
        m_status->setWordWrap(true);
        m_image = new QLabel;
        m_image->setAlignment(Qt::AlignCenter);
        auto *start = new QPushButton(QStringLiteral("Tạo QR thử nghiệm"));
        m_start = start;
        auto *close = new QPushButton(QStringLiteral("Đóng / hủy"));
        layout->addWidget(m_status);
        layout->addWidget(m_image);
        layout->addWidget(start);
        layout->addWidget(close);
        connect(start, &QPushButton::clicked, this, [this] { begin(); });
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
        m_expiry.setSingleShot(true);
        connect(&m_expiry, &QTimer::timeout, this, [this] { fail(QStringLiteral("QR hết hạn; hãy tạo mã mới.")); });
        resize(420, 300);
        if (startImmediately)
            QTimer::singleShot(0, this, [this] { begin(); });
    }

private:
    using Callback = std::function<void(const QByteArray &)>;
    QNetworkAccessManager m_network;
    QTimer m_expiry;
    QLabel *m_status = nullptr;
    QLabel *m_image = nullptr;
    QPushButton *m_start = nullptr;
    QNetworkReply *m_reply = nullptr;
    QString m_version;
    QString m_code;
    int m_generation = 0;

    void fail(const QString &message)
    {
        ++m_generation;
        m_expiry.stop();
        if (m_reply) {
            auto *reply = m_reply;
            m_reply = nullptr;
            reply->abort();
        }
        m_code.clear();
        m_image->clear();
        m_status->setText(message);
        m_start->setEnabled(true);
    }

    void request(const QUrl &url, const QByteArray &body, bool post, Callback callback, int redirects = 0)
    {
        // Redirects are not followed: credentials must not leave the expected host.
        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36"));
        req.setRawHeader("Accept", "*/*");
        req.setRawHeader("Referer", url.host() == QStringLiteral("jr.chat.zalo.me")
                                       ? "https://chat.zalo.me/"
                                       : "https://id.zalo.me/account?continue=https%3A%2F%2Fchat.zalo.me%2F");
        req.setTransferTimeout(35000);
        if (post)
            req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
        QNetworkReply *reply = post ? m_network.post(req, body) : m_network.get(req);
        m_reply = reply;
        const int generation = m_generation;
        connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
            if (reply->bytesAvailable() > 2 * 1024 * 1024)
                fail(QStringLiteral("Phản hồi quá lớn."));
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, generation, callback, redirects, url, post] {
            if (m_reply == reply)
                m_reply = nullptr;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto error = reply->error();
            const QUrl redirect = url.resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            const QByteArray payload = reply->readAll();
            reply->deleteLater();
            if (generation != m_generation)
                return;
            if (error == QNetworkReply::NoError && status >= 300 && status < 400 && redirects > 0 && !post) {
                const QString host = redirect.host().toLower();
                const bool trusted = host == QStringLiteral("id.zalo.me") || host == QStringLiteral("chat.zalo.me")
                    || host == QStringLiteral("jr.chat.zalo.me") || host == QStringLiteral("jr.zaloapp.com");
                if (!redirect.isValid() || redirect.scheme() != QLatin1String("https") || !trusted
                    || !redirect.userInfo().isEmpty() || (redirect.port() != -1 && redirect.port() != 443)) {
                    fail(QStringLiteral("Đã chặn chuyển hướng ngoài các host đăng nhập được phép (host: %1).")
                             .arg(host.isEmpty() ? QStringLiteral("không có") : host.left(253).toHtmlEscaped()));
                    return;
                }
                request(redirect, {}, false, callback, redirects - 1);
                return;
            }
            if (error != QNetworkReply::NoError || status < 200 || status >= 300 || payload.size() > 2 * 1024 * 1024) {
                fail(QStringLiteral("Yêu cầu thất bại (HTTP %1, mã mạng %2). Không ghi nội dung phản hồi.")
                         .arg(status).arg(int(error)));
                return;
            }
            callback(payload);
        });
    }

    QByteArray form(bool pc = true) const
    {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("continue"), pc ? QStringLiteral("https://zalo.me/pc")
                                                          : QStringLiteral("https://chat.zalo.me/"));
        query.addQueryItem(QStringLiteral("v"), m_version);
        return query.toString(QUrl::FullyEncoded).toUtf8();
    }

    void post(const QString &path, const QByteArray &body, Callback callback)
    {
        request(QUrl(QStringLiteral("https://id.zalo.me") + path), body, true, std::move(callback));
    }

    void begin()
    {
        ++m_generation;
        m_network.setCookieJar(new QNetworkCookieJar(&m_network));
        m_start->setEnabled(false);
        m_status->setText(QStringLiteral("Đang chuẩn bị QR…"));
        request(QUrl(QStringLiteral("https://id.zalo.me/account?continue=https%3A%2F%2Fchat.zalo.me%2F")), {}, false,
                [this](const QByteArray &html) {
            m_version = ZaloNative::QrProtocol::loginVersion(html);
            if (m_version.isEmpty()) {
                fail(QStringLiteral("Không tìm thấy phiên bản login; giao thức có thể đã đổi."));
                return;
            }
            post(QStringLiteral("/account/logininfo"), form(), [this](const QByteArray &payload) {
                if (!QJsonDocument::fromJson(payload).isObject()) {
                    fail(QStringLiteral("Phản hồi logininfo không hợp lệ."));
                    return;
                }
                post(QStringLiteral("/account/verify-client"), form() + "&type=device", [this](const QByteArray &payload) {
                    if (!QJsonDocument::fromJson(payload).isObject()) {
                        fail(QStringLiteral("Phản hồi verify-client không hợp lệ."));
                        return;
                    }
                    post(QStringLiteral("/account/authen/qr/generate"), form(), [this](const QByteArray &json) {
                        QByteArray png;
                        if (!ZaloNative::QrProtocol::qrResponse(json, &m_code, &png)) {
                            fail(QStringLiteral("Không tạo được QR; giao thức có thể đã đổi."));
                            return;
                        }
                        QPixmap pixmap;
                        if (!pixmap.loadFromData(png, "PNG")) {
                            fail(QStringLiteral("Ảnh QR không hợp lệ."));
                            return;
                        }
                        m_image->setPixmap(pixmap.scaled(250, 250, Qt::KeepAspectRatio, Qt::FastTransformation));
                        m_status->setText(QStringLiteral("Quét QR bằng điện thoại, sau đó xác nhận đăng nhập."));
                        m_expiry.start(100000);
                        wait(false);
                    });
                });
            });
        });
    }

    void wait(bool confirm)
    {
        QUrlQuery code;
        code.addQueryItem(QStringLiteral("code"), m_code);
        if (confirm) {
            code.addQueryItem(QStringLiteral("gToken"), QString());
            code.addQueryItem(QStringLiteral("gAction"), QStringLiteral("CONFIRM_QR"));
        }
        const QByteArray body = form(false) + '&' + code.toString(QUrl::FullyEncoded).toUtf8();
        const QString path = confirm ? QStringLiteral("/account/authen/qr/waiting-confirm")
                                     : QStringLiteral("/account/authen/qr/waiting-scan");
        post(path, body, [this, confirm](const QByteArray &json) {
            const auto root = QJsonDocument::fromJson(json).object();
            if (!root.value(QStringLiteral("error_code")).isDouble()) {
                fail(QStringLiteral("Phản hồi login không hợp lệ."));
                return;
            }
            const int result = root.value(QStringLiteral("error_code")).toInt();
            if (result == 8) {
                const int generation = m_generation;
                QTimer::singleShot(1000, this, [this, confirm, generation] {
                    if (generation == m_generation)
                        wait(confirm);
                });
            } else if (result != 0 || (!confirm && !root.value(QStringLiteral("data")).isObject())) {
                fail(QStringLiteral("Đăng nhập bị từ chối hoặc hết hạn (mã %1).").arg(result));
            } else if (!confirm) {
                m_image->clear();
                m_status->setText(QStringLiteral("Đã quét. Hãy xác nhận trên điện thoại."));
                wait(true);
            } else {
                m_expiry.stop();
                m_code.clear();
                m_status->setText(QStringLiteral("Đang kiểm tra phiên…"));
                request(QUrl(QStringLiteral("https://id.zalo.me/account/checksession?continue=https%3A%2F%2Fchat.zalo.me%2Findex.html")), {}, false,
                        [this](const QByteArray &) {
                    request(QUrl(QStringLiteral("https://jr.chat.zalo.me/jr/userinfo")), {}, false,
                            [this](const QByteArray &payload) {
                        const auto root = QJsonDocument::fromJson(payload).object();
                        const auto data = root.value(QStringLiteral("data")).toObject();
                        if (!data.value(QStringLiteral("logged")).toBool()) {
                            const auto errorCode = root.value(QStringLiteral("error_code"));
                            fail(QStringLiteral("Không xác nhận được phiên chat (mã API: %1; data: %2).")
                                     .arg(errorCode.isDouble() ? QString::number(errorCode.toInt()) : QStringLiteral("không có"),
                                          root.value(QStringLiteral("data")).isObject() ? QStringLiteral("có") : QStringLiteral("không")));
                            return;
                        }
                        m_status->setText(QStringLiteral("Đã xác nhận phiên web trong bộ nhớ. Chưa hỗ trợ danh sách hoặc nhắn tin."));
                        m_start->setEnabled(true);
                    });
                }, 5);
            }
        });
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Experimental native QR login probe"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("start"), QStringLiteral("Start the QR request immediately.")});
    parser.process(app);
    Probe probe(parser.isSet(QStringLiteral("start")));
    probe.show();
    return app.exec();
}
