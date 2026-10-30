#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace ZaloNative {

// Bounded, app-rooted backend for the renderer's Electron/Node host APIs.
//
// This is a policy layer, not an OS sandbox. It never exposes the user's whole
// home directory, a shell or child processes, and every filesystem path is
// resolved below dataRoot() before an operation is attempted. A hostile local
// process could still race symlinks between validation and use; production
// integration must keep the renderer origin trusted and should replace this
// with descriptor-relative operations where practical.
class HostApi
{
public:
    static constexpr qint64 kMaxFileBytes = 8 * 1024 * 1024;
    static constexpr qint64 kMaxStoreBytes = 1024 * 1024;
    static constexpr int kMaxRecordedOperations = 64;

    HostApi(QString dataRoot, QString appRoot, QString appVersion);

    QString dataRoot() const { return m_dataRoot; }
    QString appRoot() const { return m_appRoot; }
    QString appVersion() const { return m_appVersion; }

    // Diagnostic-only: names of operations dispatched so far, without
    // arguments. Safe to include in probe output; never contains paths/data.
    QStringList operations() const { return m_operations; }

    // Request:  {"op": "fs.ensureDir", "args": {"path": "userData/cache"}}
    // Success:  {"ok": true, "value": ...}
    // Failure:  {"ok": false, "error": {"code": "...", "message": "..."}}
    QJsonObject dispatch(const QJsonObject &request) const;
    QJsonObject dispatchJson(const QString &json) const;

private:
    QJsonObject handle(const QString &op, const QJsonObject &args) const;

    QString resolvePath(const QString &input, bool mustExist, QString *code,
                        QString *message) const;
    QString storePath(const QString &name, QString *code, QString *message) const;
    QJsonObject readStore(const QString &name) const;
    QJsonObject writeStore(const QString &name, const QJsonObject &value) const;

    static QJsonObject ok(const QJsonValue &value = QJsonValue(true));
    static QJsonObject error(const QString &code, const QString &message);

    QString m_dataRoot;
    QString m_appRoot;
    QString m_appVersion;
    mutable QStringList m_operations;
};

} // namespace ZaloNative
