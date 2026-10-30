#include "AsarArchive.h"
#include "BridgeRouter.h"
#include "RuntimeLocator.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTextStream>

#include <cstdio>

namespace {

int fail(const QString &message)
{
    std::fprintf(stderr, "zalort: %s\n", qUtf8Printable(message));
    return 1;
}

QString resolveArchivePath(const QString &argument)
{
    const QFileInfo info(argument);
    if (info.isFile())
        return info.absoluteFilePath();
    const QStringList candidates = ZaloNative::RuntimeLocator::candidateAsars(argument);
    return candidates.isEmpty() ? QString() : candidates.first();
}

int commandInspect(const QString &argument)
{
    QString error;
    const auto manifest = ZaloNative::RuntimeLocator::locate(argument, &error);
    if (!manifest)
        return fail(error);

    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(manifest->toJson()).toJson(QJsonDocument::Indented));
    return 0;
}

int commandList(const QString &argument, const QString &prefix)
{
    const QString path = resolveArchivePath(argument);
    if (path.isEmpty())
        return fail(QStringLiteral("no app.asar found at %1").arg(argument));

    QString error;
    const auto archive = ZaloNative::AsarArchive::open(path, &error);
    if (!archive)
        return fail(error);

    const QString normalizedPrefix = prefix.startsWith(QLatin1Char('/'))
        ? prefix.mid(1)
        : prefix;

    QTextStream out(stdout);
    int count = 0;
    for (const QString &entry : archive->entries()) {
        if (!normalizedPrefix.isEmpty() && !entry.startsWith(normalizedPrefix))
            continue;
        out << entry << '\n';
        ++count;
    }
    std::fprintf(stderr, "zalort: %d of %d entries\n", count, archive->nodeCount());
    return 0;
}

int commandExtract(const QString &argument, const QString &entry, const QString &output)
{
    const QString path = resolveArchivePath(argument);
    if (path.isEmpty())
        return fail(QStringLiteral("no app.asar found at %1").arg(argument));

    QString error;
    const auto archive = ZaloNative::AsarArchive::open(path, &error);
    if (!archive)
        return fail(error);

    QByteArray data;
    if (!archive->readFile(entry, &data, &error))
        return fail(error);

    QFile file(output);
    const QFileInfo outputInfo(output);
    if (!outputInfo.absoluteDir().exists())
        return fail(QStringLiteral("output directory does not exist: %1")
                        .arg(outputInfo.absolutePath()));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return fail(QStringLiteral("cannot write %1: %2").arg(output, file.errorString()));
    if (file.write(data) != data.size())
        return fail(QStringLiteral("short write to %1").arg(output));

    std::fprintf(stderr, "zalort: extracted %s (%lld bytes)\n",
                 qUtf8Printable(entry), static_cast<long long>(data.size()));
    return 0;
}

int commandBridgeSpec()
{
    ZaloNative::BridgeRouter router;
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(router.spec()).toJson(QJsonDocument::Indented));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("zalort"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ZALO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Inspect and extract Electron ASAR runtimes for Zalo Native."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QStringLiteral("command"),
        QStringLiteral("inspect <path> | list <path> [prefix] | extract <path> <entry> <output> | bridge-spec"));

    parser.process(app);
    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        parser.showHelp(1);
        return 1;
    }

    const QString command = positional.at(0);
    if (command == QLatin1String("inspect")) {
        if (positional.size() != 2)
            return fail(QStringLiteral("usage: zalort inspect <runtime-dir|app.asar>"));
        return commandInspect(positional.at(1));
    }
    if (command == QLatin1String("list")) {
        if (positional.size() < 2 || positional.size() > 3)
            return fail(QStringLiteral("usage: zalort list <runtime-dir|app.asar> [prefix]"));
        return commandList(positional.at(1), positional.value(2));
    }
    if (command == QLatin1String("extract")) {
        if (positional.size() != 4)
            return fail(QStringLiteral("usage: zalort extract <runtime-dir|app.asar> <entry> <output>"));
        return commandExtract(positional.at(1), positional.at(2), positional.at(3));
    }
    if (command == QLatin1String("bridge-spec"))
        return commandBridgeSpec();

    return fail(QStringLiteral("unknown command: %1").arg(command));
}
