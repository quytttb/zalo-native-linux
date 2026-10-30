#include "LocalScheme.h"
#include "RuntimePagePolicy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using namespace ZaloNative::LocalScheme;

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(contents) == contents.size();
}

} // namespace

class LocalSchemeTest : public QObject
{
    Q_OBJECT

private slots:
    void mapsIndexAndAssets();
    void rejectsTraversalAndMissingFiles();
    void rejectsSymlinkEscapes();
    void reportsMimeTypes();
    void rewritesOnlyLocalFetchDirectives();
    void rewritesEveryCspMetaTag();
    void leavesUnchangedWithoutCspOrValidScheme();
    void rewritesSingleQuotedMetaIdempotently();
};

void LocalSchemeTest::mapsIndexAndAssets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath(QStringLiteral("index.html")), "<html></html>"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("sub/app.js")), "console.log(1);"));

    QString error;
    const auto index = resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app/")), &error);
    QVERIFY2(index.has_value(), qPrintable(error));
    QCOMPARE(QFileInfo(index->absolutePath).fileName(), QStringLiteral("index.html"));
    QCOMPARE(index->mimeType, QStringLiteral("text/html"));

    const auto bare = resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app")), &error);
    QVERIFY2(bare.has_value(), qPrintable(error));
    QCOMPARE(bare->absolutePath, QFileInfo(dir.filePath(QStringLiteral("index.html")))
                                      .canonicalFilePath());

    const auto asset = resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app/sub/app.js")), &error);
    QVERIFY2(asset.has_value(), qPrintable(error));
    QVERIFY(asset->mimeType.contains(QStringLiteral("javascript")));

    const auto dotted = resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app/./sub//app.js")), &error);
    QVERIFY2(dotted.has_value(), qPrintable(error));
}

void LocalSchemeTest::rejectsTraversalAndMissingFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath(QStringLiteral("index.html")), "<html></html>"));
    QVERIFY(writeFile(dir.filePath(QStringLiteral("secret.txt")), "secret"));

    QString error;
    QVERIFY(!resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app/../secret.txt")), &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!resolve(dir.path(), QUrl(QStringLiteral("zalo-app://app/missing.html")), &error));
    QVERIFY(error.contains(QStringLiteral("not found")));

    error.clear();
    QVERIFY(!resolve(dir.filePath(QStringLiteral("not-a-dir")),
                     QUrl(QStringLiteral("zalo-app://app/index.html")), &error));
}

void LocalSchemeTest::rejectsSymlinkEscapes()
{
    QTemporaryDir root;
    QTemporaryDir outside;
    QVERIFY(root.isValid());
    QVERIFY(outside.isValid());

    QVERIFY(writeFile(root.filePath(QStringLiteral("index.html")), "<html></html>"));
    const QString outsideFile = outside.filePath(QStringLiteral("private.txt"));
    QVERIFY(writeFile(outsideFile, "private"));

    const QString link = root.filePath(QStringLiteral("leak.txt"));
    if (!QFile::link(outsideFile, link))
        QSKIP("this filesystem cannot create symlinks");

    QString error;
    QVERIFY(!resolve(root.path(), QUrl(QStringLiteral("zalo-app://app/leak.txt")), &error));
    QVERIFY2(error.contains(QStringLiteral("escapes")), qPrintable(error));
}

void LocalSchemeTest::reportsMimeTypes()
{
    QCOMPARE(mimeTypeForPath(QStringLiteral("index.html")), QStringLiteral("text/html"));
    QCOMPARE(mimeTypeForPath(QStringLiteral("style.css")), QStringLiteral("text/css"));
    QCOMPARE(mimeTypeForPath(QStringLiteral("data.json")), QStringLiteral("application/json"));
    QVERIFY(mimeTypeForPath(QStringLiteral("image.unknown-ext"))
                .contains(QStringLiteral("octet-stream")));
}

void LocalSchemeTest::rewritesOnlyLocalFetchDirectives()
{
    const QByteArray html = R"(<html><head><meta http-equiv="Content-Security-Policy" content="upgrade-insecure-requests; default-src 'none'; script-src 'self' https://example.com; style-src 'self'; img-src * data:; connect-src 'self'; child-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'; report-uri https://example.com/r"></head><body></body></html>)";

    const auto result = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        html, QByteArrayLiteral("zalo-app"));
    QVERIFY(result.changed);
    QVERIFY(result.html.contains("default-src zalo-app:"));
    QVERIFY(result.html.contains("script-src 'self' https://example.com zalo-app:"));
    QVERIFY(result.html.contains("style-src 'self' zalo-app:"));
    QVERIFY(result.html.contains("img-src * data: zalo-app:"));
    QVERIFY(result.html.contains("connect-src 'self' zalo-app:"));
    QVERIFY(result.html.contains("child-src 'self' zalo-app:"));
    QVERIFY(result.html.contains("frame-ancestors 'none'"));
    QVERIFY(result.html.contains("base-uri 'none'"));
    QVERIFY(result.html.contains("form-action 'none'"));
    QVERIFY(result.html.contains("report-uri https://example.com/r"));
    QVERIFY(result.html.contains("upgrade-insecure-requests"));
    QVERIFY(!result.html.contains("default-src 'none'"));
}

void LocalSchemeTest::rewritesEveryCspMetaTag()
{
    const QByteArray html = R"(<html><head><meta http-equiv="Content-Security-Policy" content="default-src 'none'"><meta http-equiv='Content-Security-Policy' content="img-src 'self'"></head></html>)";

    const auto result = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        html, QByteArrayLiteral("zalo-app"));
    QVERIFY(result.changed);
    QVERIFY(result.html.contains("default-src zalo-app:"));
    QVERIFY(result.html.contains("img-src 'self' zalo-app:"));
    QVERIFY(!result.html.contains("default-src 'none'"));
}

void LocalSchemeTest::leavesUnchangedWithoutCspOrValidScheme()
{
    const QByteArray plain = "<html><body>no policy</body></html>";
    const auto noPolicy = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        plain, QByteArrayLiteral("zalo-app"));
    QVERIFY(!noPolicy.changed);
    QCOMPARE(noPolicy.html, plain);

    const QByteArray policy = R"(<meta http-equiv="Content-Security-Policy" content="script-src 'self'">)";
    const auto invalid = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        policy, QByteArrayLiteral("bad scheme"));
    QVERIFY(!invalid.changed);
    QCOMPARE(invalid.html, policy);

    const auto already = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        R"(<meta http-equiv="Content-Security-Policy" content="script-src zalo-app:">)",
        QByteArrayLiteral("zalo-app"));
    QVERIFY(!already.changed);
}

void LocalSchemeTest::rewritesSingleQuotedMetaIdempotently()
{
    const QByteArray html = R"(<meta http-equiv='Content-Security-Policy' content="default-src 'none'; script-src 'self'">)";
    const auto first = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        html, QByteArrayLiteral("zalo-app"));
    QVERIFY(first.changed);
    QVERIFY(first.html.contains("default-src zalo-app:"));
    QVERIFY(first.html.contains("script-src 'self' zalo-app:"));

    const auto second = ZaloNative::RuntimePagePolicy::allowSchemeInCsp(
        first.html, QByteArrayLiteral("zalo-app"));
    QVERIFY(!second.changed);
    QCOMPARE(second.html, first.html);
}

QTEST_GUILESS_MAIN(LocalSchemeTest)
#include "test_scheme.moc"
