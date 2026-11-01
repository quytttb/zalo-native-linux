#pragma once

#include <QByteArray>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace ZaloNative {

// Experimental Web QR login. A confirmed Web session is NOT a PC session and
// does not establish that chat APIs, persistence, or E2EE work. All credentials
// and QR data stay in memory; no request is made until start() is called.
class NativeQrLogin : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasQr READ hasQr NOTIFY changed)
    Q_PROPERTY(bool webSessionReady READ webSessionReady NOTIFY changed)
    Q_PROPERTY(int imageRevision READ imageRevision NOTIFY changed)

public:
    struct Timing
    {
        int requestTimeoutMs = 35000;
        int qrLifetimeMs = 100000;
        int pollIntervalMs = 1000;
    };

    explicit NativeQrLogin(QObject *parent = nullptr);
    // Dependency injection is for offline tests. The caller owns the manager.
    NativeQrLogin(QNetworkAccessManager *network, Timing timing, QObject *parent = nullptr);

    QString state() const;
    QString status() const { return m_status; }
    bool busy() const;
    bool hasQr() const { return m_hasQr; }
    bool webSessionReady() const;
    int imageRevision() const { return m_imageRevision; }
    QImage qrImage() const;

    Q_INVOKABLE void start();
    // Local cancellation only; it does not revoke a session on Zalo's servers.
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    enum class State { Idle, Preparing, WaitingScan, WaitingConfirm, Checking, WebSession, Failed };
    using Callback = std::function<void(const QByteArray &)>;

    void initialize();
    void reset();
    void setState(State state, const QString &status);
    void setImage(const QImage &image);
    void fail(const QString &status);
    void request(const QUrl &url, const QByteArray &body, bool post, Callback callback,
                 int redirects = 0);
    bool consumeReply(QNetworkReply *reply, quint64 generation);
    void post(const QString &path, const QByteArray &body, Callback callback);
    QByteArray form(bool pc) const;
    void waitForPhone(bool confirm);
    void checkWebSession();

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    Timing m_timing;
    QTimer m_expiry;
    QTimer m_poll;
    QTimer m_requestDeadline;
    quint64 m_generation = 0;
    State m_state = State::Idle;
    QString m_status;
    QString m_version;
    QString m_code;
    QByteArray m_response;
    mutable QMutex m_imageMutex;
    QImage m_image;
    bool m_hasQr = false;
    int m_imageRevision = 0;
};

} // namespace ZaloNative
