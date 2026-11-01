#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <cstring>
#include <utility>

// An in-memory network double: it must never fall through to real network.
struct Response
{
    int status = 200;
    QByteArray body;
    QUrl redirect;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;
    bool hang = false;
};

class FakeReply : public QNetworkReply
{
public:
    FakeReply(const QNetworkRequest &request, QNetworkAccessManager::Operation operation,
              Response response, QObject *parent)
        : QNetworkReply(parent), m_response(std::move(response))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(operation);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        if (!m_response.hang)
            QTimer::singleShot(0, this, [this] { finish(); });
    }

    void abort() override
    {
        if (isFinished())
            return;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("synthetic cancellation"));
        setFinished(true);
        emit finished();
    }

    qint64 bytesAvailable() const override
    {
        return m_response.body.size() - m_offset + QNetworkReply::bytesAvailable();
    }

    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 size = qMin(maxSize, qint64(m_response.body.size()) - m_offset);
        if (size <= 0)
            return -1;
        memcpy(data, m_response.body.constData() + m_offset, size_t(size));
        m_offset += size;
        return size;
    }

private:
    void finish()
    {
        if (isFinished())
            return;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, m_response.status);
        if (!m_response.redirect.isEmpty())
            setAttribute(QNetworkRequest::RedirectionTargetAttribute, m_response.redirect);
        if (m_response.error != QNetworkReply::NoError)
            setError(m_response.error, QStringLiteral("synthetic network error"));
        if (!m_response.body.isEmpty())
            emit readyRead();
        if (!isFinished()) {
            setFinished(true);
            emit finished();
        }
    }

    Response m_response;
    qint64 m_offset = 0;
};

class FakeNetwork : public QNetworkAccessManager
{
public:
    QList<Response> responses;
    QList<QNetworkRequest> requests;
    QList<Operation> operations;
    QList<QByteArray> bodies;

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoing) override
    {
        requests.append(request);
        operations.append(op);
        bodies.append(outgoing ? outgoing->readAll() : QByteArray());
        if (responses.isEmpty())
            return new FakeReply(request, op, {500, "unexpected request"}, this);
        const Response response = responses.takeFirst();
        return new FakeReply(request, op, response, this);
    }
};
