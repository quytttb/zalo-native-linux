#include "AppController.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>
#include <qqml.h>

#ifdef ZALO_HAVE_WEBENGINE
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

int main(int argc, char *argv[])
{
#ifdef ZALO_HAVE_WEBENGINE
    // Must run before QGuiApplication is constructed.
    QtWebEngineQuick::initialize();
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("quytttb"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("github.com"));
    QCoreApplication::setApplicationName(QStringLiteral("ZaloNative"));
    QCoreApplication::setApplicationVersion(QStringLiteral(ZALO_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.quytttb.ZaloNative"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Experimental native Qt 6/QML shell for the Zalo desktop renderer."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("runtime"),
                      QStringLiteral("Path to an extracted Zalo runtime or app.asar."),
                      QStringLiteral("path")});
    parser.addOption({QStringLiteral("no-demo"),
                      QStringLiteral("Do not load the bundled demonstration conversations.")});
    parser.addOption({QStringLiteral("self-test"),
                      QStringLiteral("Validate the core pipeline headlessly and exit.")});
    parser.addOption({QStringLiteral("smoke-test"),
                      QStringLiteral("Load the QML interface, then quit after 1.5 seconds.")});
    parser.process(app);

    ZaloNative::AppController::Options options;
    options.runtimeRoot = parser.value(QStringLiteral("runtime"));
    options.demo = !parser.isSet(QStringLiteral("no-demo"));

    if (parser.isSet(QStringLiteral("self-test"))) {
        options.demo = false;
        ZaloNative::AppController controller(options);
        controller.reloadDemo();
        const bool ok = controller.conversationCount() > 0 && controller.messageCount() > 0;
        qInfo().noquote() << QStringLiteral("self-test: %1 conversations, %2 messages in the first thread — %3")
                                 .arg(controller.conversationCount())
                                 .arg(controller.messageCount())
                                 .arg(ok ? QStringLiteral("OK") : QStringLiteral("FAILED"));
        return ok ? 0 : 1;
    }

    ZaloNative::AppController controller(options);
    qmlRegisterSingletonInstance("ZaloNative", 1, 0, "App", &controller);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    engine.loadFromModule("ZaloNative", "Main");
    if (engine.rootObjects().isEmpty()) {
        qCritical("Could not load the ZaloNative QML module.");
        return 1;
    }

    if (parser.isSet(QStringLiteral("smoke-test")))
        QTimer::singleShot(1500, &app, &QCoreApplication::quit);

    return app.exec();
}
