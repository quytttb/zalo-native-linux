#include "HostApi.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace ZaloNative;

namespace {

QJsonObject call(HostApi &api, const QString &op, const QJsonObject &args = QJsonObject())
{
    return api.dispatch(QJsonObject{{QStringLiteral("op"), op},
                                    {QStringLiteral("args"), args}});
}

QString errorCode(const QJsonObject &response)
{
    return response.value(QStringLiteral("error"))
        .toObject()
        .value(QStringLiteral("code"))
        .toString();
}

} // namespace

class HostApiTest : public QObject
{
    Q_OBJECT

private slots:
    void electronPathsStayUnderDataRoot();
    void fileRoundTrip();
    void traversalAndAbsolutePathsAreRejected();
    void symlinkEscapeIsRejected();
    void storeRoundTrip();
    void operationsAreRecordedWithoutArguments();
    void unknownOperationFailsExplicitly();
    void missingFilesAndLimitsFailExplicitly();
};

void HostApiTest::electronPathsStayUnderDataRoot()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("26.10.20"));

    const QJsonObject userData
        = call(api, QStringLiteral("electron.path"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("userData")}});
    QVERIFY(userData.value(QStringLiteral("ok")).toBool());
    const QString userDataPath = userData.value(QStringLiteral("value")).toString();
    QVERIFY(userDataPath.startsWith(api.dataRoot()));
    QVERIFY(userDataPath.endsWith(QStringLiteral("/userData")));

    const QJsonObject appPath = call(api, QStringLiteral("electron.appPath"));
    QCOMPARE(appPath.value(QStringLiteral("value")).toString(), api.appRoot());
    const QJsonObject appVersion = call(api, QStringLiteral("electron.appVersion"));
    QCOMPARE(appVersion.value(QStringLiteral("value")).toString(), QStringLiteral("26.10.20"));

    const QJsonObject documents
        = call(api, QStringLiteral("electron.path"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("documents")}});
    QVERIFY(!documents.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(documents), QStringLiteral("ERR_HOST_PATH_UNSUPPORTED"));
}

void HostApiTest::fileRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QJsonObject ensure
        = call(api, QStringLiteral("fs.ensureDir"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/cache")}});
    QVERIFY(ensure.value(QStringLiteral("ok")).toBool());
    QVERIFY(QDir(api.dataRoot() + QStringLiteral("/userData/cache")).exists());

    const QJsonObject write
        = call(api, QStringLiteral("fs.writeFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/cache/hello.txt")},
                           {QStringLiteral("data"), QStringLiteral("xin chào")},
                           {QStringLiteral("encoding"), QStringLiteral("utf8")}});
    QVERIFY(write.value(QStringLiteral("ok")).toBool());

    const QJsonObject read
        = call(api, QStringLiteral("fs.readFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/cache/hello.txt")},
                           {QStringLiteral("encoding"), QStringLiteral("utf8")}});
    QCOMPARE(read.value(QStringLiteral("value")).toString(), QStringLiteral("xin chào"));

    const QJsonObject base64
        = call(api, QStringLiteral("fs.readFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/cache/hello.txt")},
                           {QStringLiteral("encoding"), QStringLiteral("base64")}});
    QCOMPARE(base64.value(QStringLiteral("value")).toString(),
             QString::fromLatin1(QStringLiteral("xin chào").toUtf8().toBase64()));

    const QJsonObject exists
        = call(api, QStringLiteral("fs.exists"),
               QJsonObject{{QStringLiteral("path"),
                            QStringLiteral("userData/cache/hello.txt")}});
    QVERIFY(exists.value(QStringLiteral("value")).toBool());

    const QJsonObject entries
        = call(api, QStringLiteral("fs.readdir"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/cache")}});
    QVERIFY(entries.value(QStringLiteral("ok")).toBool());
    QCOMPARE(entries.value(QStringLiteral("value")).toArray().size(), 1);

    const QJsonObject stat
        = call(api, QStringLiteral("fs.stat"),
               QJsonObject{{QStringLiteral("path"),
                            QStringLiteral("userData/cache/hello.txt")}});
    QVERIFY(stat.value(QStringLiteral("ok")).toBool());
    QVERIFY(stat.value(QStringLiteral("value"))
                .toObject()
                .value(QStringLiteral("isFile"))
                .toBool());
}

void HostApiTest::traversalAndAbsolutePathsAreRejected()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QJsonObject traversal
        = call(api, QStringLiteral("fs.writeFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("../escape.txt")},
                           {QStringLiteral("data"), QStringLiteral("x")}});
    QVERIFY(!traversal.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(traversal), QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));

    const QJsonObject absolute
        = call(api, QStringLiteral("fs.writeFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("/tmp/zalonative-escape.txt")},
                           {QStringLiteral("data"), QStringLiteral("x")}});
    QVERIFY(!absolute.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(absolute), QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));
    QVERIFY(!QFileInfo::exists(QStringLiteral("/tmp/zalonative-escape.txt")));
}

void HostApiTest::symlinkEscapeIsRejected()
{
    QTemporaryDir temp;
    QTemporaryDir outside;
    QVERIFY(temp.isValid());
    QVERIFY(outside.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QString linkPath = api.dataRoot() + QStringLiteral("/escape-link");
    QVERIFY(QFile::link(outside.path(), linkPath));
    QVERIFY(QFileInfo(linkPath).isSymLink());

    const QJsonObject write
        = call(api, QStringLiteral("fs.writeFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("escape-link/escape.txt")},
                           {QStringLiteral("data"), QStringLiteral("x")}});
    QVERIFY(!write.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(write), QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));
    QVERIFY(!QFileInfo::exists(outside.path() + QStringLiteral("/escape.txt")));

    const QJsonObject realpath
        = call(api, QStringLiteral("fs.realpath"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("escape-link")}});
    QVERIFY(!realpath.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(realpath), QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));

    QVERIFY(QDir(api.dataRoot()).mkpath(QStringLiteral("electron-store")));
    const QString outsideStore = outside.path() + QStringLiteral("/store.json");
    QFile storeFile(outsideStore);
    QVERIFY(storeFile.open(QIODevice::WriteOnly));
    storeFile.write("{}");
    storeFile.close();
    const QString storeLink = api.dataRoot() + QStringLiteral("/electron-store/test.json");
    QVERIFY(QFile::link(outsideStore, storeLink));
    const QJsonObject storeSet
        = call(api, QStringLiteral("store.set"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")},
                           {QStringLiteral("value"), 1}});
    QVERIFY(!storeSet.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(storeSet), QStringLiteral("ERR_HOST_OUTSIDE_ROOT"));
}

void HostApiTest::storeRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QJsonObject set
        = call(api, QStringLiteral("store.set"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")},
                           {QStringLiteral("value"),
                            QJsonObject{{QStringLiteral("a"), 1}}}});
    QVERIFY(set.value(QStringLiteral("ok")).toBool());

    const QJsonObject get
        = call(api, QStringLiteral("store.get"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")}});
    QVERIFY(get.value(QStringLiteral("ok")).toBool());
    const QJsonObject value = get.value(QStringLiteral("value")).toObject();
    QVERIFY(value.value(QStringLiteral("found")).toBool());
    QCOMPARE(value.value(QStringLiteral("value")).toObject().value(QStringLiteral("a")).toInt(),
             1);

    const QJsonObject has
        = call(api, QStringLiteral("store.has"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")}});
    QVERIFY(has.value(QStringLiteral("value")).toBool());
    QVERIFY(QFileInfo(api.dataRoot() + QStringLiteral("/electron-store/test.json")).isFile());

    const QJsonObject remove
        = call(api, QStringLiteral("store.delete"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")}});
    QVERIFY(remove.value(QStringLiteral("ok")).toBool());
    const QJsonObject missing
        = call(api, QStringLiteral("store.get"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("test")},
                           {QStringLiteral("key"), QStringLiteral("k")}});
    QVERIFY(!missing.value(QStringLiteral("value"))
                 .toObject()
                 .value(QStringLiteral("found"))
                 .toBool());

    const QJsonObject badName
        = call(api, QStringLiteral("store.set"),
               QJsonObject{{QStringLiteral("name"), QStringLiteral("../escape")},
                           {QStringLiteral("key"), QStringLiteral("k")},
                           {QStringLiteral("value"), 1}});
    QVERIFY(!badName.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(badName), QStringLiteral("EINVAL"));
}

void HostApiTest::operationsAreRecordedWithoutArguments()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    QVERIFY(api.operations().isEmpty());
    call(api, QStringLiteral("fs.writeFile"),
         QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/secret-name.txt")},
                     {QStringLiteral("data"), QStringLiteral("payload")}});
    call(api, QStringLiteral("fs.writeFile"),
         QJsonObject{{QStringLiteral("path"), QStringLiteral("userData/secret-name.txt")},
                     {QStringLiteral("data"), QStringLiteral("payload")}});
    call(api, QStringLiteral("electron.path"),
         QJsonObject{{QStringLiteral("name"), QStringLiteral("userData")}});
    call(api, QStringLiteral("private-secret-operation"), QJsonObject{});

    QCOMPARE(api.operations().count(QStringLiteral("fs.writeFile")), 1);
    QVERIFY(api.operations().contains(QStringLiteral("electron.path")));
    QVERIFY(!api.operations().join(QLatin1Char(' ')).contains(QStringLiteral("secret-name")));
    QVERIFY(!api.operations().contains(QStringLiteral("private-secret-operation")));
}

void HostApiTest::unknownOperationFailsExplicitly()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QJsonObject unknown
        = call(api, QStringLiteral("shell.exec"),
               QJsonObject{{QStringLiteral("command"), QStringLiteral("id")}});
    QVERIFY(!unknown.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(unknown), QStringLiteral("unknown_method"));

    const QJsonObject invalid = api.dispatchJson(QStringLiteral("not json"));
    QVERIFY(!invalid.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(invalid), QStringLiteral("invalid_json"));

    const QJsonObject rootRemove
        = call(api, QStringLiteral("fs.unlink"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral(".")}});
    QVERIFY(!rootRemove.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(rootRemove), QStringLiteral("EPERM"));
}

void HostApiTest::missingFilesAndLimitsFailExplicitly()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    HostApi api(temp.path(), temp.path(), QStringLiteral("test"));

    const QJsonObject missing
        = call(api, QStringLiteral("fs.readFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("missing.txt")},
                           {QStringLiteral("encoding"), QStringLiteral("utf8")}});
    QVERIFY(!missing.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(missing), QStringLiteral("ENOENT"));

    QFile big(api.dataRoot() + QStringLiteral("/big.bin"));
    QVERIFY(big.open(QIODevice::WriteOnly));
    QCOMPARE(big.write(QByteArray(HostApi::kMaxFileBytes + 1, 'x')),
             HostApi::kMaxFileBytes + 1);
    big.close();

    const QJsonObject tooBig
        = call(api, QStringLiteral("fs.readFile"),
               QJsonObject{{QStringLiteral("path"), QStringLiteral("big.bin")},
                           {QStringLiteral("encoding"), QStringLiteral("base64")}});
    QVERIFY(!tooBig.value(QStringLiteral("ok")).toBool());
    QCOMPARE(errorCode(tooBig), QStringLiteral("EFBIG"));
}

QTEST_GUILESS_MAIN(HostApiTest)

#include "test_host_api.moc"
