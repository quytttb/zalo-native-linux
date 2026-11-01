#include "AppController.h"
#include "QrImageProvider.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>
#include <qqml.h>

#include <cstdio>

#ifdef ZALO_HAVE_WEBENGINE
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

namespace {

struct QmlWarningState
{
    int count = 0;
    QtMessageHandler previous = nullptr;
    bool strict = false;
};

QmlWarningState g_qmlWarnings;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (g_qmlWarnings.strict
        && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)) {
        const QString category = context.category ? QString::fromLatin1(context.category)
                                                  : QString();
        const bool looksLikeQml = category.startsWith(QLatin1String("qt.qml"))
            || message.contains(QLatin1String("QML"))
            || message.contains(QLatin1String("qrc:/qt/qml"))
            || message.contains(QLatin1String("Cannot assign"))
            || message.contains(QLatin1String("Unable to assign"))
            || message.contains(QLatin1String("Cannot anchor"))
            || message.contains(QLatin1String("is not a type"));
        if (looksLikeQml) {
            ++g_qmlWarnings.count;
            std::fprintf(stderr, "qml-warning: %s\n", qUtf8Printable(message));
        }
    }

    if (g_qmlWarnings.previous)
        g_qmlWarnings.previous(type, context, message);
}

} // namespace

int main(int argc, char *argv[])
{
    bool smokeTest = false;
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        const QLatin1String arg(argv[i]);
        if (arg == QLatin1String("--self-test")) {
            selfTest = true;
            QStandardPaths::setTestModeEnabled(true);
        } else if (arg == QLatin1String("--smoke-test")) {
            smokeTest = true;
            QStandardPaths::setTestModeEnabled(true);
        }
    }

#ifdef ZALO_HAVE_WEBENGINE
    // Must run before QGuiApplication is constructed.
    QtWebEngineQuick::initialize();
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("quytttb"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com"));
    QCoreApplication::setApplicationName(QStringLiteral("ZaloNative"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ZALO_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Zalo Native"));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.quytttb.ZaloNative"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Unofficial experimental native Qt 6/QML Zalo client. "
                       "The WebEngine modes are historical diagnostics."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("runtime"),
                      QStringLiteral("Inspect an extracted Zalo application directory or app.asar "
                                     "in historical runtime diagnostics mode."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("runtime-diagnostics"),
                      QStringLiteral("Open historical runtime inspection instead of the native backend.")});
    parser.addOption({QStringLiteral("demo"),
                      QStringLiteral("Show bundled fictional demonstration conversations.")});
    parser.addOption({QStringLiteral("web"),
                      QStringLiteral("Open the experimental visible Zalo Web wrapper "
                                     "(chat.zalo.me). This is not the target native UI.")});
    parser.addOption({QStringLiteral("url"),
                      QStringLiteral("URL for --web (defaults to https://chat.zalo.me/)."),
                      QStringLiteral("url")});
    parser.addOption({QStringLiteral("no-demo"),
                      QStringLiteral("Kept for compatibility; demo is opt-in via --demo.")});
    parser.addOption({QStringLiteral("self-test"),
                      QStringLiteral("Validate the core pipeline headlessly and exit.")});
    parser.addOption({QStringLiteral("smoke-test"),
                      QStringLiteral("Load the QML interface, then quit after 1.5 seconds. "
                                     "QML warnings make the process fail.")});
    parser.process(app);

    if (smokeTest) {
        g_qmlWarnings.strict = true;
        g_qmlWarnings.previous = qInstallMessageHandler(messageHandler);
    }

    ZaloNative::AppController::Options options;
    options.runtimeRoot = parser.value(QStringLiteral("runtime"));
    options.demo = parser.isSet(QStringLiteral("demo"));
    options.webMode = parser.isSet(QStringLiteral("web"));
    options.runtimeDiagnostics = parser.isSet(QStringLiteral("runtime-diagnostics"))
        || parser.isSet(QStringLiteral("runtime"));
    if (options.webMode)
        options.rendererUrl = parser.value(QStringLiteral("url"));
    else if (!parser.value(QStringLiteral("url")).isEmpty())
        qWarning("--url is ignored without --web.");

    if (selfTest) {
        options.demo = true;
        options.webMode = false;
        options.runtimeDiagnostics = false;
        ZaloNative::AppController controller(options);
        controller.reloadDemo();
        const bool ok = controller.conversationCount() > 0 && controller.messageCount() > 0;
        qInfo().noquote() << QStringLiteral("self-test: %1 conversations, %2 messages in the first thread — %3")
                                 .arg(controller.conversationCount())
                                 .arg(controller.messageCount())
                                 .arg(ok ? QStringLiteral("OK") : QStringLiteral("FAILED"));
        return ok ? 0 : 1;
    }

    if (smokeTest && options.webMode && options.rendererUrl.isEmpty())
        options.rendererUrl = QStringLiteral("about:blank");

    ZaloNative::AppController controller(options);
    qmlRegisterSingletonInstance("ZaloNative", 1, 0, "App", &controller);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("nativeqr"), new ZaloNative::QrImageProvider(controller.nativeQrLogin()));
    engine.loadFromModule("ZaloNative", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("Could not load the ZaloNative QML module.");
        return 1;
    }

    if (smokeTest) {
        if (qEnvironmentVariableIsSet("ZALO_TEST_QML_WARNING"))
            qWarning("QML synthetic warning used to verify the strict smoke-test handler");

        QTimer::singleShot(1500, &app, [&app]() {
            if (g_qmlWarnings.count > 0) {
                std::fprintf(stderr, "smoke-test: %d QML warning(s) detected\n",
                             g_qmlWarnings.count);
                app.exit(2);
                return;
            }
            app.quit();
        });
    }

    return app.exec();
}
