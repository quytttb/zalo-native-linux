#include "HostApi.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QSysInfo>

#include <utility>

namespace ZaloNative {

namespace {

QString canonicalOrAbsolute(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

} // namespace

HostApi::HostApi(QString dataRoot, QString appRoot, QString appVersion)
    : m_appVersion(std::move(appVersion))
{
    const QString absoluteData
        = QDir(dataRoot.isEmpty() ? QStringLiteral(".") : dataRoot).absolutePath();
    QDir().mkpath(absoluteData);
    m_dataRoot = canonicalOrAbsolute(absoluteData);
    if (m_dataRoot.isEmpty())
        m_dataRoot = QDir::cleanPath(absoluteData);

    const QString absoluteApp = QDir(appRoot.isEmpty() ? absoluteData : appRoot).absolutePath();
    m_appRoot = canonicalOrAbsolute(absoluteApp);
    if (m_appRoot.isEmpty())
        m_appRoot = QDir::cleanPath(absoluteApp);
}

QJsonObject HostApi::dispatch(const QJsonObject &request) const
{
    const QString op = request.value(QStringLiteral("op")).toString();
    if (op.isEmpty())
        return error(QStringLiteral("invalid_request"), QStringLiteral("missing op"));

    // Diagnostic telemetry only: names, never arguments. Bounded so a noisy
    // renderer cannot grow the list without limit.
    // Never record arbitrary renderer-supplied strings as operation names.
    static const QStringList knownOperations = {
        QStringLiteral("electron.path"), QStringLiteral("electron.appPath"),
        QStringLiteral("electron.appVersion"), QStringLiteral("fs.ensureDir"),
        QStringLiteral("os.release"),
        QStringLiteral("fs.access"), QStringLiteral("fs.exists"), QStringLiteral("fs.stat"),
        QStringLiteral("fs.lstat"), QStringLiteral("fs.readFile"), QStringLiteral("fs.writeFile"),
        QStringLiteral("fs.appendFile"), QStringLiteral("fs.mkdir"), QStringLiteral("fs.readdir"),
        QStringLiteral("fs.unlink"), QStringLiteral("fs.rmdir"), QStringLiteral("fs.rename"),
        QStringLiteral("fs.copyFile"), QStringLiteral("fs.realpath"), QStringLiteral("store.get"),
        QStringLiteral("store.set"), QStringLiteral("store.has"), QStringLiteral("store.delete"),
        QStringLiteral("store.clear"), QStringLiteral("store.keys"),
        QStringLiteral("store.snapshot")};
    if (knownOperations.contains(op) && !m_operations.contains(op)
        && m_operations.size() < kMaxRecordedOperations)
        m_operations.append(op);

    const QJsonValue argsValue = request.value(QStringLiteral("args"));
    if (!argsValue.isUndefined() && !argsValue.isObject())
        return error(QStringLiteral("invalid_request"), QStringLiteral("args must be an object"));

    return handle(op, argsValue.toObject());
}

QJsonObject HostApi::dispatchJson(const QString &json) const
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return error(QStringLiteral("invalid_json"), QStringLiteral("request is not a JSON object"));
    return dispatch(document.object());
}

QJsonObject HostApi::ok(const QJsonValue &value)
{
    return QJsonObject{{QStringLiteral("ok"), true}, {QStringLiteral("value"), value}};
}

QJsonObject HostApi::error(const QString &code, const QString &message)
{
    return QJsonObject{
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"),
         QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}}};
}

QString HostApi::resolvePath(const QString &input, bool mustExist, QString *code,
                             QString *message) const
{
    const auto fail = [code, message](const QString &errorCode, const QString &errorMessage) {
        if (code)
            *code = errorCode;
        if (message)
            *message = errorMessage;
        return QString();
    };

    if (input.isEmpty())
        return fail(QStringLiteral("EINVAL"), QStringLiteral("path must not be empty"));
    if (input.contains(QChar::Null))
        return fail(QStringLiteral("EINVAL"), QStringLiteral("path contains a NUL byte"));
    if (input.contains(QLatin1Char('\\')))
        return fail(QStringLiteral("EINVAL"),
                    QStringLiteral("backslash is not allowed in a POSIX host path"));

    QString path = input;
    if (!QDir::isAbsolutePath(path))
        path = m_dataRoot + QLatin1Char('/') + path;
    path = QDir::cleanPath(path);

    if (path == m_dataRoot)
        return path;

    const QString prefix = m_dataRoot.endsWith(QLatin1Char('/'))
        ? m_dataRoot
        : m_dataRoot + QLatin1Char('/');
    if (!path.startsWith(prefix))
        return fail(QStringLiteral("ERR_HOST_OUTSIDE_ROOT"),
                    QStringLiteral("path is outside the app data root"));

    // Canonicalize the longest existing ancestor. This rejects symlinks that
    // point outside the allowed root, including dangling final symlinks.
    QString probe = path;
    QFileInfo probeInfo(probe);
    while (!probeInfo.exists() && probe != m_dataRoot) {
        // exists() follows symlinks, so a dangling link must be rejected
        // before walking to its parent: creating through it could otherwise
        // create a file outside the allowed root.
        if (probeInfo.isSymLink())
            return fail(QStringLiteral("ERR_HOST_OUTSIDE_ROOT"),
                        QStringLiteral("cannot resolve a dangling symbolic link"));
        const QString parent = probeInfo.absolutePath();
        if (parent.isEmpty() || parent == probe)
            break;
        probe = parent;
        probeInfo = QFileInfo(probe);
    }

    const QString canonicalProbe = probeInfo.canonicalFilePath();
    if (canonicalProbe.isEmpty())
        return fail(QStringLiteral("ERR_HOST_OUTSIDE_ROOT"),
                    QStringLiteral("cannot resolve the app data path"));

    const QString canonicalRoot = QFileInfo(m_dataRoot).canonicalFilePath();
    if (canonicalRoot.isEmpty())
        return fail(QStringLiteral("EIO"), QStringLiteral("cannot resolve the app data root"));
    if (canonicalProbe != canonicalRoot
        && !canonicalProbe.startsWith(canonicalRoot + QLatin1Char('/')))
        return fail(QStringLiteral("ERR_HOST_OUTSIDE_ROOT"),
                    QStringLiteral("path escapes the app data root"));

    if (mustExist && !QFileInfo::exists(path))
        return fail(QStringLiteral("ENOENT"), QStringLiteral("no such file or directory"));

    return path;
}

QString HostApi::storePath(const QString &name, QString *code, QString *message) const
{
    static const QRegularExpression validName(QStringLiteral("^[A-Za-z0-9._-]{1,64}$"));
    const QString cleanName = name.isEmpty() ? QStringLiteral("default") : name;
    if (!validName.match(cleanName).hasMatch() || cleanName == QLatin1String(".")
        || cleanName == QLatin1String("..")) {
        if (code)
            *code = QStringLiteral("EINVAL");
        if (message)
            *message = QStringLiteral("invalid electron-store name");
        return QString();
    }

    const QString candidate
        = m_dataRoot + QStringLiteral("/electron-store/") + cleanName + QStringLiteral(".json");
    QString pathCode;
    QString pathMessage;
    const QString resolved = resolvePath(candidate, false, &pathCode, &pathMessage);
    if (resolved.isEmpty()) {
        if (code)
            *code = pathCode;
        if (message)
            *message = pathMessage;
        return QString();
    }
    return resolved;
}

QJsonObject HostApi::readStore(const QString &name) const
{
    QString code;
    QString message;
    const QString path = storePath(name, &code, &message);
    if (path.isEmpty())
        return error(code, message);

    const QFileInfo info(path);
    if (!info.exists())
        return ok(QJsonObject{});
    if (!info.isFile())
        return error(QStringLiteral("EISDIR"), QStringLiteral("electron-store path is not a file"));
    if (info.size() > kMaxStoreBytes)
        return error(QStringLiteral("EFBIG"), QStringLiteral("electron-store file is too large"));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return error(QStringLiteral("EACCES"), QStringLiteral("cannot read electron-store file"));
    const QByteArray data = file.read(kMaxStoreBytes + 1);
    if (data.size() > kMaxStoreBytes)
        return error(QStringLiteral("EFBIG"), QStringLiteral("electron-store file is too large"));

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return error(QStringLiteral("EJSONPARSE"),
                     QStringLiteral("electron-store file is not a JSON object"));
    return ok(document.object());
}

QJsonObject HostApi::writeStore(const QString &name, const QJsonObject &value) const
{
    QString code;
    QString message;
    const QString path = storePath(name, &code, &message);
    if (path.isEmpty())
        return error(code, message);

    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory))
        return error(QStringLiteral("EIO"), QStringLiteral("cannot create electron-store directory"));

    const QByteArray data = QJsonDocument(value).toJson(QJsonDocument::Compact);
    if (data.size() > kMaxStoreBytes)
        return error(QStringLiteral("EFBIG"), QStringLiteral("electron-store data is too large"));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return error(QStringLiteral("EACCES"), QStringLiteral("cannot write electron-store file"));
    if (file.write(data) != data.size() || !file.commit())
        return error(QStringLiteral("EIO"), QStringLiteral("cannot commit electron-store file"));
    return ok(true);
}

QJsonObject HostApi::handle(const QString &op, const QJsonObject &args) const
{
    QString code;
    QString message;

    if (op == QLatin1String("electron.path")) {
        const QString name = args.value(QStringLiteral("name")).toString();
        if (name == QLatin1String("userData"))
            return ok(m_dataRoot + QStringLiteral("/userData"));
        if (name == QLatin1String("appData"))
            return ok(m_dataRoot + QStringLiteral("/appData"));
        if (name == QLatin1String("temp"))
            return ok(m_dataRoot + QStringLiteral("/temp"));
        if (name == QLatin1String("logs"))
            return ok(m_dataRoot + QStringLiteral("/logs"));
        if (name == QLatin1String("crashDumps"))
            return ok(m_dataRoot + QStringLiteral("/crashDumps"));
        if (name == QLatin1String("sessionData"))
            return ok(m_dataRoot + QStringLiteral("/sessionData"));
        if (name == QLatin1String("home"))
            return ok(m_dataRoot);
        return error(QStringLiteral("ERR_HOST_PATH_UNSUPPORTED"),
                     QStringLiteral("path name is not provided by this host: %1").arg(name));
    }

    if (op == QLatin1String("electron.appPath"))
        return ok(m_appRoot);
    if (op == QLatin1String("electron.appVersion"))
        return ok(m_appVersion);

    if (op == QLatin1String("os.release")) {
        const QString release = QSysInfo::kernelVersion();
        if (release.isEmpty())
            return error(QStringLiteral("ERR_HOST_UNSUPPORTED"),
                         QStringLiteral("kernel release is unavailable"));
        return ok(release);
    }

    if (op == QLatin1String("fs.ensureDir")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), false,
                                         &code, &message);
        if (path.isEmpty())
            return error(code, message);
        if (path == m_dataRoot)
            return ok(true);
        if (!QDir().mkpath(path))
            return error(QStringLiteral("EIO"), QStringLiteral("cannot create directory"));
        return ok(true);
    }

    if (op == QLatin1String("fs.access")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        const int mode = args.value(QStringLiteral("mode")).toInt(0);
        const QFileInfo info(path);
        if ((mode & 4) && !info.isReadable())
            return error(QStringLiteral("EACCES"), QStringLiteral("path is not readable"));
        if ((mode & 2) && !info.isWritable())
            return error(QStringLiteral("EACCES"), QStringLiteral("path is not writable"));
        if ((mode & 1) && !info.isExecutable())
            return error(QStringLiteral("EACCES"), QStringLiteral("path is not executable"));
        return ok(true);
    }

    if (op == QLatin1String("fs.exists")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), false,
                                         &code, &message);
        if (path.isEmpty())
            return error(code, message);
        return ok(QFileInfo::exists(path));
    }

    if (op == QLatin1String("fs.stat")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        const QFileInfo info(path);
        return ok(QJsonObject{
            {QStringLiteral("isFile"), info.isFile()},
            {QStringLiteral("isDirectory"), info.isDir()},
            {QStringLiteral("isSymbolicLink"), info.isSymLink()},
            {QStringLiteral("size"), static_cast<double>(info.size())},
            {QStringLiteral("mtimeMs"),
             static_cast<double>(info.lastModified().toMSecsSinceEpoch())},
            {QStringLiteral("mode"), static_cast<int>(info.permissions())}});
    }

    if (op == QLatin1String("fs.lstat")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        const QFileInfo info(path);
        const bool isLink = info.isSymLink();
        return ok(QJsonObject{
            {QStringLiteral("isFile"), isLink ? false : info.isFile()},
            {QStringLiteral("isDirectory"), isLink ? false : info.isDir()},
            {QStringLiteral("isSymbolicLink"), isLink},
            {QStringLiteral("size"), static_cast<double>(info.size())},
            {QStringLiteral("mtimeMs"),
             static_cast<double>(info.lastModified().toMSecsSinceEpoch())},
            {QStringLiteral("mode"), static_cast<int>(info.permissions())}});
    }

    if (op == QLatin1String("fs.readFile")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        const QFileInfo info(path);
        if (info.isDir())
            return error(QStringLiteral("EISDIR"), QStringLiteral("cannot read a directory"));
        if (!info.isFile())
            return error(QStringLiteral("EINVAL"), QStringLiteral("not a regular file"));
        if (info.size() > kMaxFileBytes)
            return error(QStringLiteral("EFBIG"), QStringLiteral("file is too large"));

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return error(QStringLiteral("EACCES"), QStringLiteral("cannot read file"));
        const QByteArray data = file.read(kMaxFileBytes + 1);
        if (data.size() > kMaxFileBytes)
            return error(QStringLiteral("EFBIG"), QStringLiteral("file is too large"));

        const QString encoding
            = args.value(QStringLiteral("encoding")).toString(QStringLiteral("base64")).toLower();
        if (encoding == QLatin1String("utf8") || encoding == QLatin1String("utf-8"))
            return ok(QString::fromUtf8(data));
        if (encoding == QLatin1String("base64"))
            return ok(QString::fromLatin1(data.toBase64()));
        return error(QStringLiteral("EINVAL"), QStringLiteral("unsupported encoding"));
    }

    if (op == QLatin1String("fs.writeFile") || op == QLatin1String("fs.appendFile")) {
        const bool append = op == QLatin1String("fs.appendFile");
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), false,
                                         &code, &message);
        if (path.isEmpty())
            return error(code, message);
        if (path == m_dataRoot || QFileInfo(path).isDir())
            return error(QStringLiteral("EISDIR"), QStringLiteral("cannot write a directory"));

        const QString parent = QFileInfo(path).absolutePath();
        if (!QFileInfo(parent).isDir())
            return error(QStringLiteral("ENOENT"), QStringLiteral("parent directory is missing"));

        const QString encoding = args.value(QStringLiteral("encoding"))
                                     .toString(QStringLiteral("utf8"))
                                     .toLower();
        QByteArray data;
        if (encoding == QLatin1String("utf8") || encoding == QLatin1String("utf-8"))
            data = args.value(QStringLiteral("data")).toString().toUtf8();
        else if (encoding == QLatin1String("base64"))
            data = QByteArray::fromBase64(
                args.value(QStringLiteral("data")).toString().toLatin1());
        else
            return error(QStringLiteral("EINVAL"), QStringLiteral("unsupported encoding"));
        if (data.size() > kMaxFileBytes)
            return error(QStringLiteral("EFBIG"), QStringLiteral("file is too large"));

        if (append) {
            const QFileInfo existing(path);
            if (existing.exists() && !existing.isFile())
                return error(QStringLiteral("EINVAL"), QStringLiteral("not a regular file"));
            QFile file(path);
            if (!file.open(QIODevice::Append))
                return error(QStringLiteral("EACCES"), QStringLiteral("cannot append to file"));
            if (file.size() > kMaxFileBytes - data.size())
                return error(QStringLiteral("EFBIG"), QStringLiteral("file is too large"));
            if (file.write(data) != data.size())
                return error(QStringLiteral("EIO"), QStringLiteral("cannot append to file"));
            return ok(true);
        }

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return error(QStringLiteral("EACCES"), QStringLiteral("cannot write file"));
        if (file.write(data) != data.size() || !file.commit())
            return error(QStringLiteral("EIO"), QStringLiteral("cannot commit file"));
        return ok(true);
    }

    if (op == QLatin1String("fs.mkdir")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), false,
                                         &code, &message);
        if (path.isEmpty())
            return error(code, message);
        const bool recursive = args.value(QStringLiteral("recursive")).toBool(false);
        if (QFileInfo::exists(path)) {
            if (recursive && QFileInfo(path).isDir())
                return ok(true);
            return error(QStringLiteral("EEXIST"), QStringLiteral("path already exists"));
        }
        const bool created = recursive ? QDir().mkpath(path) : QDir().mkdir(path);
        if (!created)
            return error(QStringLiteral("EIO"), QStringLiteral("cannot create directory"));
        return ok(true);
    }

    if (op == QLatin1String("fs.readdir")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        if (!QFileInfo(path).isDir())
            return error(QStringLiteral("ENOTDIR"), QStringLiteral("not a directory"));
        const QStringList entries = QDir(path).entryList(
            QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name);
        return ok(QJsonArray::fromStringList(entries));
    }

    if (op == QLatin1String("fs.unlink")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        if (path == m_dataRoot)
            return error(QStringLiteral("EPERM"), QStringLiteral("cannot remove the data root"));
        const QFileInfo info(path);
        if (info.isDir()) {
            if (!args.value(QStringLiteral("recursive")).toBool(false))
                return error(QStringLiteral("EISDIR"), QStringLiteral("path is a directory"));
            if (!QDir(path).removeRecursively())
                return error(QStringLiteral("EIO"), QStringLiteral("cannot remove directory"));
            return ok(true);
        }
        if (!QFile::remove(path))
            return error(QStringLiteral("EACCES"), QStringLiteral("cannot remove file"));
        return ok(true);
    }

    if (op == QLatin1String("fs.rmdir")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        if (path == m_dataRoot)
            return error(QStringLiteral("EPERM"), QStringLiteral("cannot remove the data root"));
        if (!QDir().rmdir(path))
            return error(QStringLiteral("ENOTEMPTY"),
                         QStringLiteral("directory is not empty or cannot be removed"));
        return ok(true);
    }

    if (op == QLatin1String("fs.rename")) {
        const QString from = resolvePath(args.value(QStringLiteral("from")).toString(), true, &code,
                                         &message);
        if (from.isEmpty())
            return error(code, message);
        const QString to = resolvePath(args.value(QStringLiteral("to")).toString(), false, &code,
                                       &message);
        if (to.isEmpty())
            return error(code, message);
        if (from == m_dataRoot || to == m_dataRoot)
            return error(QStringLiteral("EPERM"), QStringLiteral("cannot rename the data root"));
        if (QFileInfo::exists(to)) {
            if (QFileInfo(to).isDir())
                return error(QStringLiteral("EISDIR"), QStringLiteral("destination is a directory"));
            QFile::remove(to);
        }
        if (!QFile::rename(from, to) && !QDir().rename(from, to))
            return error(QStringLiteral("EXDEV"), QStringLiteral("cannot rename path"));
        return ok(true);
    }

    if (op == QLatin1String("fs.copyFile")) {
        const QString from = resolvePath(args.value(QStringLiteral("from")).toString(), true, &code,
                                         &message);
        if (from.isEmpty())
            return error(code, message);
        const QString to = resolvePath(args.value(QStringLiteral("to")).toString(), false, &code,
                                       &message);
        if (to.isEmpty())
            return error(code, message);
        if (!QFileInfo(from).isFile())
            return error(QStringLiteral("EISDIR"), QStringLiteral("source is not a file"));
        if (QFileInfo(from).size() > kMaxFileBytes)
            return error(QStringLiteral("EFBIG"), QStringLiteral("file is too large"));
        if (to == m_dataRoot || QFileInfo(to).isDir())
            return error(QStringLiteral("EISDIR"), QStringLiteral("destination is a directory"));
        if (QFileInfo::exists(to))
            QFile::remove(to);
        if (!QFile::copy(from, to))
            return error(QStringLiteral("EIO"), QStringLiteral("cannot copy file"));
        return ok(true);
    }

    if (op == QLatin1String("fs.realpath")) {
        const QString path = resolvePath(args.value(QStringLiteral("path")).toString(), true, &code,
                                         &message);
        if (path.isEmpty())
            return error(code, message);
        const QString canonical = QFileInfo(path).canonicalFilePath();
        if (canonical.isEmpty())
            return error(QStringLiteral("ENOENT"), QStringLiteral("no such file or directory"));
        const QString rootCanonical = QFileInfo(m_dataRoot).canonicalFilePath();
        if (canonical != rootCanonical
            && !canonical.startsWith(rootCanonical + QLatin1Char('/')))
            return error(QStringLiteral("ERR_HOST_OUTSIDE_ROOT"),
                         QStringLiteral("real path is outside the app data root"));
        return ok(canonical);
    }

    if (op.startsWith(QLatin1String("store."))) {
        const QString name = args.value(QStringLiteral("name")).toString();
        if (op == QLatin1String("store.clear"))
            return writeStore(name, QJsonObject{});

        if (op == QLatin1String("store.snapshot"))
            return readStore(name);

        if (op == QLatin1String("store.keys")) {
            QJsonObject store = readStore(name);
            if (!store.value(QStringLiteral("ok")).toBool())
                return store;
            const QJsonObject data = store.value(QStringLiteral("value")).toObject();
            return ok(QJsonArray::fromStringList(data.keys()));
        }

        const QString key = args.value(QStringLiteral("key")).toString();
        if (key.isEmpty())
            return error(QStringLiteral("EINVAL"), QStringLiteral("store key must not be empty"));

        QJsonObject store = readStore(name);
        if (!store.value(QStringLiteral("ok")).toBool())
            return store;
        QJsonObject data = store.value(QStringLiteral("value")).toObject();

        if (op == QLatin1String("store.get")) {
            if (!data.contains(key))
                return ok(QJsonObject{{QStringLiteral("found"), false}});
            return ok(QJsonObject{{QStringLiteral("found"), true},
                                  {QStringLiteral("value"), data.value(key)}});
        }
        if (op == QLatin1String("store.has"))
            return ok(data.contains(key));
        if (op == QLatin1String("store.set")) {
            data.insert(key, args.contains(QStringLiteral("value"))
                                 ? args.value(QStringLiteral("value"))
                                 : QJsonValue(QJsonValue::Null));
            return writeStore(name, data);
        }
        if (op == QLatin1String("store.delete")) {
            data.remove(key);
            return writeStore(name, data);
        }
        return error(QStringLiteral("unknown_method"),
                     QStringLiteral("unsupported store operation: %1").arg(op));
    }

    return error(QStringLiteral("unknown_method"),
                 QStringLiteral("unsupported host operation: %1").arg(op));
}

} // namespace ZaloNative
