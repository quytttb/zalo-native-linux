// Adapted from this project's independently written historical Web QR probe
// (src/probe/main.cpp and QrProtocol.cpp). Its protocol reference was zca-js,
// MIT, commit 2a7e360257dfe98e2eb48dc46bbf389834be7e0b. No upstream source is
// bundled. This is not the PC QR flow implemented by ctdozlite.
#include "NativeQrLogin.h"

#include <QBuffer>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

#include <utility>

namespace ZaloNative {

namespace {

constexpr qint64 kMaxResponseBytes = 2 * 1024 * 1024;
constexpr int kMaxQrDimension = 2048;

QString loginVersion(const QByteArray &html)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(https://stc-zlogin\.zdn\.vn/main-([0-9]+(?:\.[0-9]+)+)\.js)"));
    const QString version = pattern.match(QString::fromUtf8(html)).captured(1);
    return version.size() <= 64 ? version : QString();
}

bool allowedRedirect(const QUrl &url)
{
    static const QSet<QString> hosts{
        QStringLiteral("id.zalo.me"), QStringLiteral("chat.zalo.me"),
        QStringLiteral("jr.chat.zalo.me"), QStringLiteral("jr.zaloapp.com")};
    return url.isValid() && url.scheme() == QLatin1String("https")
        && hosts.contains(url.host().toLower()) && url.userInfo().isEmpty()
        && !url.hasFragment() && (url.port() == -1 || url.port() == 443);
}

bool initialResponseOk(const QByteArray &payload)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return false;
    const QJsonValue code = doc.object().value(QStringLiteral("error_code"));
    // The historical Web probe accepted an object without an error_code here.
    // An explicit error or malformed code must never be treated as success.
    return code.isUndefined() || (code.isDouble() && code.toInteger(-1) == 0);
}

bool parseQr(const QByteArray &payload, QString *code, QImage *image)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return false;
    const QJsonObject root = doc.object();
    const QJsonValue errorCode = root.value(QStringLiteral("error_code"));
    if (!errorCode.isDouble() || errorCode.toInteger(-1) != 0
        || !root.value(QStringLiteral("data")).isObject())
        return false;
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QString qrCode = data.value(QStringLiteral("code")).toString();
    QString encoded = data.value(QStringLiteral("image")).toString();
    const QString prefix = QStringLiteral("data:image/png;base64,");
    if (encoded.startsWith(prefix))
        encoded.remove(0, prefix.size());
    const auto decoded = QByteArray::fromBase64Encoding(encoded.toLatin1(),
                                                        QByteArray::AbortOnBase64DecodingErrors);
    if (qrCode.isEmpty() || qrCode.size() > 4096 || !decoded
        || !decoded.decoded.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))
        return false;

    QBuffer buffer;
    buffer.setData(decoded.decoded);
    if (!buffer.open(QIODevice::ReadOnly))
        return false;
    QImageReader reader(&buffer, "png");
    const QSize size = reader.size();
    if (!size.isValid() || size.width() > kMaxQrDimension || size.height() > kMaxQrDimension)
        return false;
    QImage qr = reader.read();
    if (qr.isNull())
        return false;
    *code = qrCode;
    *image = std::move(qr);
    return true;
}

} // namespace

NativeQrLogin::NativeQrLogin(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    initialize();
}

NativeQrLogin::NativeQrLogin(QNetworkAccessManager *network, Timing timing, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_timing(timing)
{
    Q_ASSERT(m_network);
    initialize();
}

void NativeQrLogin::initialize()
{
    m_status = QStringLiteral("Chưa kết nối. Chỉ bắt đầu khi bạn chọn tạo QR.");
    m_expiry.setSingleShot(true);
    m_poll.setSingleShot(true);
    m_requestDeadline.setSingleShot(true);
    connect(&m_expiry, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("QR đã hết hạn; hãy tạo mã mới."));
    });
    connect(&m_requestDeadline, &QTimer::timeout, this, [this] {
        fail(QStringLiteral("Yêu cầu đã hết thời gian chờ."));
    });
}

QString NativeQrLogin::state() const
{
    switch (m_state) {
    case State::Idle: return QStringLiteral("idle");
    case State::Preparing: return QStringLiteral("preparing");
    case State::WaitingScan: return QStringLiteral("waiting_scan");
    case State::WaitingConfirm: return QStringLiteral("waiting_confirm");
    case State::Checking: return QStringLiteral("checking");
    case State::WebSession: return QStringLiteral("web_session");
    case State::Failed: return QStringLiteral("failed");
    }
    return QStringLiteral("failed");
}

bool NativeQrLogin::busy() const
{
    return m_state == State::Preparing || m_state == State::WaitingScan
        || m_state == State::WaitingConfirm || m_state == State::Checking;
}

bool NativeQrLogin::webSessionReady() const
{
    return m_state == State::WebSession;
}

QImage NativeQrLogin::qrImage() const
{
    QMutexLocker lock(&m_imageMutex);
    return m_image;
}

void NativeQrLogin::setImage(const QImage &image)
{
    {
        QMutexLocker lock(&m_imageMutex);
        m_image = image;
    }
    m_hasQr = !image.isNull();
    // Revision only changes the image-provider URL; it contains no QR data.
    m_imageRevision = (m_imageRevision == 1000000) ? 0 : m_imageRevision + 1;
}

void NativeQrLogin::setState(State state, const QString &status)
{
    m_state = state;
    m_status = status;
    emit changed();
}

void NativeQrLogin::reset()
{
    ++m_generation;
    m_expiry.stop();
    m_poll.stop();
    m_requestDeadline.stop();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    m_code.clear();
    m_version.clear();
    m_response.clear();
    m_network->setCookieJar(new QNetworkCookieJar(m_network));
    setImage(QImage());
}

void NativeQrLogin::fail(const QString &status)
{
    reset();
    setState(State::Failed, status);
}

void NativeQrLogin::cancel()
{
    reset();
    setState(State::Idle, QStringLiteral("Đã xóa trạng thái cục bộ trong bộ nhớ."));
}

bool NativeQrLogin::consumeReply(QNetworkReply *reply, quint64 generation)
{
    if (generation != m_generation || reply != m_reply)
        return false;
    const qint64 remaining = kMaxResponseBytes - m_response.size();
    if (reply->bytesAvailable() > remaining) {
        fail(QStringLiteral("Phản hồi vượt giới hạn cho phép."));
        return false;
    }
    m_response.append(reply->readAll());
    return true;
}

void NativeQrLogin::request(const QUrl &url, const QByteArray &body, bool isPost,
                            Callback callback, int redirects)
{
    // All origins, including server-provided redirects, pass the same policy.
    if (!allowedRedirect(url)) {
        fail(QStringLiteral("Đã chặn địa chỉ không được phép."));
        return;
    }
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/130.0.0.0 Safari/537.36"));
    req.setRawHeader("Accept", "*/*");
    req.setRawHeader("Referer", url.host() == QLatin1String("jr.chat.zalo.me")
                                  ? "https://chat.zalo.me/"
                                  : "https://id.zalo.me/account?continue=https%3A%2F%2Fchat.zalo.me%2F");
    req.setTransferTimeout(m_timing.requestTimeoutMs);
    if (isPost)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    m_response.clear();
    QNetworkReply *reply = isPost ? m_network->post(req, body) : m_network->get(req);
    m_reply = reply;
    reply->setReadBufferSize(kMaxResponseBytes + 1);
    const quint64 generation = m_generation;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, generation] {
        consumeReply(reply, generation);
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation, callback = std::move(callback), redirects, url, isPost] {
        if (!consumeReply(reply, generation)) {
            reply->deleteLater();
            return;
        }
        m_reply = nullptr;
        m_requestDeadline.stop();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        const QByteArray payload = std::move(m_response);
        reply->deleteLater();

        if (status >= 300 && status < 400) {
            const QUrl target = url.resolved(redirect.toUrl());
            if (error != QNetworkReply::NoError || isPost || redirects <= 0
                || !redirect.isValid() || redirect.toUrl().isEmpty() || !allowedRedirect(target)) {
                fail(QStringLiteral("Đã chặn chuyển hướng phiên không an toàn."));
                return;
            }
            request(target, {}, false, callback, redirects - 1);
            return;
        }
        if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
            fail(QStringLiteral("Yêu cầu thất bại; hãy kiểm tra mạng hoặc thử lại."));
            return;
        }
        callback(payload);
    });
    m_requestDeadline.start(m_timing.requestTimeoutMs);
}

QByteArray NativeQrLogin::form(bool pc) const
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("continue"), pc ? QStringLiteral("https://zalo.me/pc")
                                                      : QStringLiteral("https://chat.zalo.me/"));
    query.addQueryItem(QStringLiteral("v"), m_version);
    return query.toString(QUrl::FullyEncoded).toUtf8();
}

void NativeQrLogin::post(const QString &path, const QByteArray &body, Callback callback)
{
    request(QUrl(QStringLiteral("https://id.zalo.me") + path), body, true, std::move(callback));
}

void NativeQrLogin::start()
{
    if (busy())
        return;
    reset();
    const quint64 generation = m_generation;
    setState(State::Preparing, QStringLiteral("Đang chuẩn bị QR thử nghiệm…"));
    if (generation != m_generation)
        return;
    request(QUrl(QStringLiteral("https://id.zalo.me/account?continue=https%3A%2F%2Fchat.zalo.me%2F")),
            {}, false, [this](const QByteArray &html) {
        m_version = loginVersion(html);
        if (m_version.isEmpty()) {
            fail(QStringLiteral("Không nhận diện được phiên bản đăng nhập."));
            return;
        }
        post(QStringLiteral("/account/logininfo"), form(true), [this](const QByteArray &payload) {
            if (!initialResponseOk(payload)) {
                fail(QStringLiteral("Không thể chuẩn bị phiên đăng nhập."));
                return;
            }
            post(QStringLiteral("/account/verify-client"), form(true) + "&type=device",
                 [this](const QByteArray &payload) {
                if (!initialResponseOk(payload)) {
                    fail(QStringLiteral("Không thể xác minh client đăng nhập."));
                    return;
                }
                post(QStringLiteral("/account/authen/qr/generate"), form(true),
                     [this](const QByteArray &payload) {
                    QImage image;
                    if (!parseQr(payload, &m_code, &image)) {
                        fail(QStringLiteral("Không tạo được QR hợp lệ."));
                        return;
                    }
                    setImage(image);
                    const quint64 generation = m_generation;
                    setState(State::WaitingScan,
                             QStringLiteral("Quét QR bằng điện thoại, sau đó xác nhận đăng nhập."));
                    if (generation != m_generation)
                        return;
                    m_expiry.start(m_timing.qrLifetimeMs);
                    waitForPhone(false);
                });
            });
        });
    });
}

void NativeQrLogin::waitForPhone(bool confirm)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("code"), m_code);
    if (confirm) {
        query.addQueryItem(QStringLiteral("gToken"), QString());
        query.addQueryItem(QStringLiteral("gAction"), QStringLiteral("CONFIRM_QR"));
    }
    const QByteArray body = form(false) + '&' + query.toString(QUrl::FullyEncoded).toUtf8();
    const QString path = confirm ? QStringLiteral("/account/authen/qr/waiting-confirm")
                                 : QStringLiteral("/account/authen/qr/waiting-scan");
    post(path, body, [this, confirm](const QByteArray &payload) {
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        const QJsonObject root = doc.object();
        const QJsonValue error = root.value(QStringLiteral("error_code"));
        if (!doc.isObject() || !error.isDouble()) {
            fail(QStringLiteral("Phản hồi QR không hợp lệ."));
            return;
        }
        const qint64 code = error.toInteger(-1);
        if (code == 8) {
            const quint64 generation = m_generation;
            m_poll.disconnect(this);
            connect(&m_poll, &QTimer::timeout, this, [this, confirm, generation] {
                if (generation == m_generation)
                    waitForPhone(confirm);
            });
            m_poll.start(m_timing.pollIntervalMs);
        } else if (code != 0 || (!confirm && !root.value(QStringLiteral("data")).isObject())) {
            fail(QStringLiteral("QR bị từ chối hoặc đã hết hạn."));
        } else if (!confirm) {
            setImage(QImage());
            const quint64 generation = m_generation;
            setState(State::WaitingConfirm, QStringLiteral("Đã quét; hãy xác nhận trên điện thoại."));
            if (generation != m_generation)
                return;
            waitForPhone(true);
        } else {
            m_expiry.stop();
            m_code.clear();
            const quint64 generation = m_generation;
            setState(State::Checking, QStringLiteral("Đang kiểm tra phiên Web…"));
            if (generation != m_generation)
                return;
            checkWebSession();
        }
    });
}

void NativeQrLogin::checkWebSession()
{
    request(QUrl(QStringLiteral("https://id.zalo.me/account/checksession?continue=https%3A%2F%2Fchat.zalo.me%2Findex.html")),
            {}, false, [this](const QByteArray &) {
        request(QUrl(QStringLiteral("https://jr.chat.zalo.me/jr/userinfo")), {}, false,
                [this](const QByteArray &payload) {
            const QJsonDocument doc = QJsonDocument::fromJson(payload);
            const QJsonObject root = doc.object();
            const QJsonObject data = root.value(QStringLiteral("data")).toObject();
            const QJsonValue error = root.value(QStringLiteral("error_code"));
            if (!doc.isObject()
                || (!error.isUndefined() && (!error.isDouble() || error.toInteger(-1) != 0))
                || !data.value(QStringLiteral("logged")).isBool()
                || !data.value(QStringLiteral("logged")).toBool()) {
                fail(QStringLiteral("Không xác nhận được phiên Web."));
                return;
            }
            setState(State::WebSession,
                     QStringLiteral("Đã xác nhận phiên Web trong bộ nhớ. Chat và lưu phiên chưa hỗ trợ."));
        });
    }, 5);
}

} // namespace ZaloNative
