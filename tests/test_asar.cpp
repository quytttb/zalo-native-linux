#include "AsarArchive.h"
#include "RuntimeLocator.h"
#include "RuntimeManifest.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

using namespace ZaloNative;

namespace {

struct FileSpec
{
    QString path;
    QByteArray data;
    bool unpacked = false;
    QString link;
    bool includeIntegrity = true;
};

QJsonObject nodeFor(const FileSpec &spec, qint64 offset)
{
    QJsonObject node;
    if (!spec.link.isEmpty()) {
        node.insert(QStringLiteral("link"), spec.link);
        return node;
    }
    node.insert(QStringLiteral("size"), spec.data.size());
    if (spec.unpacked) {
        node.insert(QStringLiteral("unpacked"), true);
        if (spec.includeIntegrity) {
            node.insert(QStringLiteral("integrity"), QJsonObject{
                {QStringLiteral("algorithm"), QStringLiteral("SHA256")},
                {QStringLiteral("hash"), QString::fromLatin1(
                     QCryptographicHash::hash(spec.data, QCryptographicHash::Sha256).toHex())}});
        }
    } else {
        node.insert(QStringLiteral("offset"), QString::number(offset));
    }
    return node;
}

void insertAt(QJsonObject &root, const QStringList &parts, const QJsonObject &leaf)
{
    if (parts.size() == 1) {
        QJsonObject files = root.value(QStringLiteral("files")).toObject();
        files.insert(parts.first(), leaf);
        root.insert(QStringLiteral("files"), files);
        return;
    }

    QJsonObject files = root.value(QStringLiteral("files")).toObject();
    QJsonObject child = files.value(parts.first()).toObject();
    insertAt(child, parts.mid(1), leaf);
    files.insert(parts.first(), child);
    root.insert(QStringLiteral("files"), files);
}

QString writeArchive(const QTemporaryDir &dir, const QList<FileSpec> &specs,
                     bool createUnpacked = true)
{
    QJsonObject root;
    QByteArray payload;
    QList<FileSpec> ordered = specs;

    for (const FileSpec &spec : ordered) {
        if (!spec.link.isEmpty())
            continue;
        if (spec.unpacked) {
            insertAt(root, spec.path.split(QLatin1Char('/')), nodeFor(spec, 0));
            if (createUnpacked) {
                const QString target = dir.filePath(QStringLiteral("sample.asar.unpacked/")
                                                    + spec.path);
                QDir().mkpath(QFileInfo(target).absolutePath());
                QFile file(target);
                if (!file.open(QIODevice::WriteOnly))
                    return {};
                file.write(spec.data);
            }
            continue;
        }
        insertAt(root, spec.path.split(QLatin1Char('/')), nodeFor(spec, payload.size()));
        payload.append(spec.data);
    }

    for (const FileSpec &spec : ordered) {
        if (spec.link.isEmpty())
            continue;
        insertAt(root, spec.path.split(QLatin1Char('/')), nodeFor(spec, 0));
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const quint32 headerLength = quint32(json.size() + 8);
    const quint32 payloadLength = quint32(json.size() + 4);
    const quint32 jsonLength = quint32(json.size());

    QByteArray prefix(16, '\0');
    qToLittleEndian<quint32>(4, prefix.data());
    qToLittleEndian<quint32>(headerLength, prefix.data() + 4);
    qToLittleEndian<quint32>(payloadLength, prefix.data() + 8);
    qToLittleEndian<quint32>(jsonLength, prefix.data() + 12);

    const QString path = dir.filePath(QStringLiteral("sample.asar"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(prefix);
    file.write(json);
    file.write(payload);
    return path;
}

} // namespace

class AsarArchiveTest : public QObject
{
    Q_OBJECT

private slots:
    void readsNestedFilesAndLinks();
    void rejectsUnsafePaths();
    void rejectsMalformedArchives();
    void requiresUnpackedFiles();
    void rejectsUnsafeUnpackedFiles();
    void rejectsUnpackedSizeMismatch();
    void verifiesUnpackedMetadata();
    void readsCheckedInFixture();
};

void AsarArchiveTest::readsNestedFilesAndLinks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QList<FileSpec> specs = {
        {QStringLiteral("index.html"), QByteArray("<html>hello</html>"), false, {}},
        {QStringLiteral("assets/app.js"), QByteArray("console.log(1);"), false, {}},
        {QStringLiteral("alias.html"), {}, false, QStringLiteral("index.html")},
    };
    const QString path = writeArchive(dir, specs);
    QVERIFY(!path.isEmpty());

    QString error;
    const auto archive = AsarArchive::open(path, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));
    QVERIFY(archive->isOpen());
    QVERIFY(archive->isFile(QStringLiteral("index.html")));
    QVERIFY(archive->isDirectory(QStringLiteral("assets")));
    QVERIFY(!archive->isFile(QStringLiteral("assets")));
    QCOMPARE(archive->nodeCount(), 4);

    QByteArray data;
    QVERIFY2(archive->readFile(QStringLiteral("/index.html"), &data, &error), qPrintable(error));
    QCOMPARE(data, QByteArray("<html>hello</html>"));

    QVERIFY2(archive->readFile(QStringLiteral("./assets//app.js"), &data, &error),
             qPrintable(error));
    QCOMPARE(data, QByteArray("console.log(1);"));

    QVERIFY2(archive->readFile(QStringLiteral("alias.html"), &data, &error), qPrintable(error));
    QCOMPARE(data, QByteArray("<html>hello</html>"));

    QVERIFY(!archive->readFile(QStringLiteral("assets"), &data, &error));
    QVERIFY(!archive->readFile(QStringLiteral("index.html"), &data, &error, 4));
}

void AsarArchiveTest::rejectsUnsafePaths()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeArchive(dir, {
        {QStringLiteral("index.html"), QByteArray("ok"), false, {}},
    });
    QVERIFY(!path.isEmpty());

    QString error;
    const auto archive = AsarArchive::open(path, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));

    QByteArray data;
    QVERIFY(!archive->readFile(QStringLiteral("../etc/passwd"), &data, &error));
    QVERIFY(!archive->readFile(QStringLiteral("assets/../../secret"), &data, &error));
    QVERIFY(!archive->readFile(QStringLiteral("missing.html"), &data, &error));
    QVERIFY(!archive->readFile(QString(), &data, &error));
    QVERIFY(!archive->contains(QStringLiteral("..")));
}

void AsarArchiveTest::rejectsMalformedArchives()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile missing(dir.filePath(QStringLiteral("missing.asar")));
    QVERIFY(!AsarArchive::open(missing.fileName()));

    QFile tiny(dir.filePath(QStringLiteral("tiny.asar")));
    QVERIFY(tiny.open(QIODevice::WriteOnly));
    tiny.write("nope");
    tiny.close();
    QVERIFY(!AsarArchive::open(tiny.fileName()));

    const QString valid = writeArchive(dir, {
        {QStringLiteral("index.html"), QByteArray("ok"), false, {}},
    });
    QVERIFY(!valid.isEmpty());

    QFile source(valid);
    QVERIFY(source.open(QIODevice::ReadOnly));
    QByteArray bytes = source.readAll();
    source.close();

    auto writeVariant = [&dir](const QString &name, const QByteArray &contents) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QString();
        file.write(contents);
        file.close();
        return file.fileName();
    };

    QVERIFY(!AsarArchive::open(writeVariant(QStringLiteral("truncated.asar"), bytes.left(20))));
    QVERIFY(!AsarArchive::open(writeVariant(QStringLiteral("empty.asar"), {})));

    QByteArray badPrefix = bytes;
    qToLittleEndian<quint32>(7, badPrefix.data());
    QVERIFY(!AsarArchive::open(writeVariant(QStringLiteral("badprefix.asar"), badPrefix)));

    QByteArray badJson = bytes;
    badJson[16] = '{';
    badJson[17] = ']';
    QVERIFY(!AsarArchive::open(writeVariant(QStringLiteral("badjson.asar"), badJson)));
}

void AsarArchiveTest::requiresUnpackedFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = writeArchive(dir, {
        {QStringLiteral("index.html"), QByteArray("ok"), false, {}},
        {QStringLiteral("build/native.node"), QByteArray("ELF"), true, {}},
    }, false);
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY(!AsarArchive::open(path, &error));
    QVERIFY(error.contains(QStringLiteral("unpacked")));
}

void AsarArchiveTest::rejectsUnsafeUnpackedFiles()
{
    QTemporaryDir dir;
    QTemporaryDir outside;
    QVERIFY(dir.isValid());
    QVERIFY(outside.isValid());
    const QString path = writeArchive(dir, {
        {QStringLiteral("native/file.node"), QByteArray("safe"), true, {}},
    });
    QVERIFY(!path.isEmpty());
    QString error;
    auto archive = AsarArchive::open(path, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));
    const QString outsidePath = outside.filePath(QStringLiteral("file.node"));
    QFile external(outsidePath);
    QVERIFY(external.open(QIODevice::WriteOnly));
    QCOMPARE(external.write("evil"), 4);
    external.close();

    const QString unpacked = dir.filePath(QStringLiteral("sample.asar.unpacked/native/file.node"));
    QVERIFY(QFile::remove(unpacked));
    QVERIFY(QFile::link(outsidePath, unpacked));
    QByteArray data;
    QVERIFY(!archive->readFile(QStringLiteral("native/file.node"), &data, &error));
    QVERIFY(error.contains(QStringLiteral("unsafe")));
    QVERIFY(!AsarArchive::open(path, &error));
    QVERIFY(error.contains(QStringLiteral("unsafe")));
}

void AsarArchiveTest::rejectsUnpackedSizeMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeArchive(dir, {
        {QStringLiteral("native/file.node"), QByteArray("safe"), true, {}},
    });
    QVERIFY(!path.isEmpty());
    QFile unpacked(dir.filePath(QStringLiteral("sample.asar.unpacked/native/file.node")));
    QVERIFY(unpacked.open(QIODevice::Append));
    QCOMPARE(unpacked.write("changed"), 7);
    unpacked.close();
    QString error;
    const auto archive = AsarArchive::open(path, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));
    QByteArray data;
    QVERIFY(!archive->readFile(QStringLiteral("native/file.node"), &data, &error));
    QVERIFY(error.contains(QStringLiteral("size mismatch")));
    QVERIFY(data.isEmpty());
}

void AsarArchiveTest::verifiesUnpackedMetadata()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = writeArchive(dir, {
        {QStringLiteral("native/good"), QByteArray("good"), true, {}},
        {QStringLiteral("native/hash"), QByteArray("abcd"), true, {}},
        {QStringLiteral("native/size"), QByteArray("xy"), true, {}},
        {QStringLiteral("native/nohash"), QByteArray("ok"), true, {}, false},
    });
    QVERIFY(!path.isEmpty());
    QFile changed(dir.filePath(QStringLiteral("sample.asar.unpacked/native/hash")));
    QVERIFY(changed.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(changed.write("wxyz"), 4);
    changed.close();
    QFile grown(dir.filePath(QStringLiteral("sample.asar.unpacked/native/size")));
    QVERIFY(grown.open(QIODevice::Append));
    QCOMPARE(grown.write("z"), 1);
    grown.close();

    QString error;
    const auto archive = AsarArchive::open(path, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));
    const QJsonObject result = archive->verifyUnpacked();
    QVERIFY(!result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("total")).toInt(), 4);
    QCOMPARE(result.value(QStringLiteral("verified")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("sizeMismatch")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("hashMismatch")).toInt(), 2);
    QCOMPARE(result.value(QStringLiteral("missingIntegrity")).toInt(), 1);
    QCOMPARE(result.value(QStringLiteral("unreadable")).toInt(), 0);
    const QJsonObject bounded = archive->verifyUnpacked(2);
    QCOMPARE(bounded.value(QStringLiteral("tooLarge")).toInt(), 3);
    QVERIFY(!bounded.value(QStringLiteral("ok")).toBool());
}

void AsarArchiveTest::readsCheckedInFixture()
{
    const QString fixture = QStringLiteral(ZALO_TEST_FIXTURES "/sample.asar");
    QVERIFY2(QFileInfo::exists(fixture), qPrintable(fixture));

    QString error;
    const auto archive = AsarArchive::open(fixture, &error);
    QVERIFY2(archive.has_value(), qPrintable(error));

    QByteArray packageJson;
    QVERIFY2(archive->readFile(QStringLiteral("package.json"), &packageJson, &error),
             qPrintable(error));
    const QJsonObject package = QJsonDocument::fromJson(packageJson).object();
    QCOMPARE(package.value(QStringLiteral("name")).toString(),
             QStringLiteral("sample-zalo-runtime"));

    QByteArray native;
    QVERIFY2(archive->readFile(QStringLiteral("node_modules/fake-native/build/Release/fake.node"),
                               &native, &error), qPrintable(error));
    QVERIFY(native.startsWith("FAKE-NATIVE-MODULE-PAYLOAD"));

    const auto manifest = RuntimeManifest::fromArchive(*archive, &error);
    QVERIFY2(manifest.has_value(), qPrintable(error));
    QCOMPARE(manifest->entryHtml, QStringLiteral("index.html"));
    QCOMPARE(manifest->electronVersion, QStringLiteral("39.0.0-demo"));
    QVERIFY(manifest->nativeModules.contains(QStringLiteral("fake-native")));
}

QTEST_GUILESS_MAIN(AsarArchiveTest)
#include "test_asar.moc"
