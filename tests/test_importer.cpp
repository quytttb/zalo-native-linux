#include "RuntimeImporter.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace ZaloNative;

namespace {

QString fixtureApp()
{
    return QStringLiteral(ZALO_TEST_FIXTURES "/app-runtime");
}

void copyDirectory(const QString &source, const QString &destination)
{
    QDir().mkpath(destination);
    QDirIterator iterator(source,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        const QString relative = QDir(source).relativeFilePath(path);
        const QString target = QDir(destination).absoluteFilePath(relative);
        if (info.isDir()) {
            QDir().mkpath(target);
        } else if (info.isFile()) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::copy(path, target);
        }
    }
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(data) == data.size();
}

} // namespace

class RuntimeImporterTest : public QObject
{
    Q_OBJECT

private slots:
    void importsAndVerifies();
    void detectsTampering();
    void refusesEscapingSymlink();
    void refusesNonEmptyDestination();
    void refusesLauncherOnlySource();
    void enforcesExpectedVersion();
    void enforcesExpectedTreeHash();
    void overwriteOnlyImportedRuntime();
    void refusesNestedDestination();
};

void RuntimeImporterTest::importsAndVerifies()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString destination = work.path() + QStringLiteral("/runtime");

    RuntimeImportOptions options;
    QString error;
    const auto result = RuntimeImporter::importAppDirectory(fixtureApp(), destination, options,
                                                           &error);
    QVERIFY2(result.has_value(), qPrintable(error));

    QVERIFY(QFileInfo::exists(destination + QStringLiteral("/runtime.json")));
    QVERIFY(QFileInfo::exists(destination + QStringLiteral("/pc-dist/index.html")));
    QVERIFY(result->treeSha256.size() == 64);
    QCOMPARE(result->manifest.appVersion, QStringLiteral("9.9.9-fixture"));
    QCOMPARE(result->manifest.entryHtml, QStringLiteral("pc-dist/index.html"));
    QVERIFY(result->fileCount >= 7);

    QFile manifestFile(destination + QStringLiteral("/runtime.json"));
    QVERIFY(manifestFile.open(QIODevice::ReadOnly));
    const QJsonObject document = QJsonDocument::fromJson(manifestFile.readAll()).object();
    QCOMPARE(document.value(QStringLiteral("schema")).toString(),
             QStringLiteral("zalonative.runtime/1"));
    QCOMPARE(document.value(QStringLiteral("treeSha256")).toString(), result->treeSha256);
    QVERIFY(!document.value(QStringLiteral("keyFiles")).toArray().isEmpty());
    QCOMPARE(document.value(QStringLiteral("verification")).toObject()
                 .value(QStringLiteral("status")).toString(),
             QStringLiteral("unverified"));

    QString verifyError;
    const auto verification = RuntimeImporter::verify(destination, &verifyError);
    QVERIFY2(verification.has_value(), qPrintable(verifyError));
    QVERIFY(verification->value(QStringLiteral("ok")).toBool());
    QCOMPARE(verification->value(QStringLiteral("actualTreeSha256")).toString(),
             result->treeSha256);
    QVERIFY(!verification->value(QStringLiteral("sourceVerified")).toBool());
}

void RuntimeImporterTest::detectsTampering()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString destination = work.path() + QStringLiteral("/runtime");

    QString error;
    QVERIFY(RuntimeImporter::importAppDirectory(fixtureApp(), destination, {}, &error)
                .has_value());

    QVERIFY(writeFile(destination + QStringLiteral("/pc-dist/index.html"),
                      "tampered\n"));

    const auto verification = RuntimeImporter::verify(destination, &error);
    QVERIFY2(verification.has_value(), qPrintable(error));
    QVERIFY(!verification->value(QStringLiteral("ok")).toBool());
}

void RuntimeImporterTest::refusesEscapingSymlink()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString source = work.path() + QStringLiteral("/source");
    copyDirectory(fixtureApp(), source);
    QVERIFY(QFile::link(QStringLiteral("/etc/passwd"), source + QStringLiteral("/leak")));

    QString error;
    const auto result = RuntimeImporter::importAppDirectory(
        source, work.path() + QStringLiteral("/runtime"), {}, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("symlink escapes")), qPrintable(error));
}

void RuntimeImporterTest::refusesNonEmptyDestination()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString destination = work.path() + QStringLiteral("/runtime");
    QVERIFY(writeFile(destination + QStringLiteral("/keep.txt"), "keep"));

    QString error;
    const auto result = RuntimeImporter::importAppDirectory(fixtureApp(), destination, {}, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("not empty")), qPrintable(error));

    RuntimeImportOptions overwrite;
    overwrite.overwrite = true;
    const auto forced = RuntimeImporter::importAppDirectory(fixtureApp(), destination, overwrite,
                                                            &error);
    QVERIFY(!forced.has_value());
    QVERIFY2(error.contains(QStringLiteral("runtime.json")), qPrintable(error));
    QVERIFY(QFileInfo::exists(destination + QStringLiteral("/keep.txt")));
}

void RuntimeImporterTest::refusesLauncherOnlySource()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString source = work.path() + QStringLiteral("/launcher");
    QVERIFY(QDir().mkpath(source + QStringLiteral("/main-dist")));
    QVERIFY(writeFile(source + QStringLiteral("/package.json"),
                      R"({"name":"launcher","version":"1.0.0"})"));

    QString error;
    const auto result = RuntimeImporter::importAppDirectory(
        source, work.path() + QStringLiteral("/runtime"), {}, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("no HTML entry")), qPrintable(error));
}

void RuntimeImporterTest::enforcesExpectedVersion()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());

    RuntimeImportOptions options;
    options.expectedVersion = QStringLiteral("0.0.0-other");
    QString error;
    const auto result = RuntimeImporter::importAppDirectory(
        fixtureApp(), work.path() + QStringLiteral("/runtime"), options, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("does not match the pinned version")),
             qPrintable(error));
}

void RuntimeImporterTest::enforcesExpectedTreeHash()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());

    RuntimeImportOptions options;
    options.expectedTreeSha256 = QString(64, QLatin1Char('0'));
    QString error;
    const auto result = RuntimeImporter::importAppDirectory(
        fixtureApp(), work.path() + QStringLiteral("/runtime"), options, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("does not match the expected hash")),
             qPrintable(error));
}

void RuntimeImporterTest::overwriteOnlyImportedRuntime()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString destination = work.path() + QStringLiteral("/runtime");

    QString error;
    const auto first = RuntimeImporter::importAppDirectory(fixtureApp(), destination, {}, &error);
    QVERIFY2(first.has_value(), qPrintable(error));

    RuntimeImportOptions options;
    options.overwrite = true;
    const auto second = RuntimeImporter::importAppDirectory(fixtureApp(), destination, options,
                                                            &error);
    QVERIFY2(second.has_value(), qPrintable(error));
    QCOMPARE(second->treeSha256, first->treeSha256);
}

void RuntimeImporterTest::refusesNestedDestination()
{
    QTemporaryDir work;
    QVERIFY(work.isValid());
    const QString source = work.path() + QStringLiteral("/source");
    copyDirectory(fixtureApp(), source);

    QString error;
    const auto result = RuntimeImporter::importAppDirectory(
        source, source + QStringLiteral("/imported"), {}, &error);
    QVERIFY(!result.has_value());
    QVERIFY2(error.contains(QStringLiteral("must not contain each other")), qPrintable(error));
}

QTEST_GUILESS_MAIN(RuntimeImporterTest)
#include "test_importer.moc"
