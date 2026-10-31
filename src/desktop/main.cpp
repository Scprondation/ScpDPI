#include "dashboard_controller.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ScpDPI"));
    QGuiApplication::setOrganizationName(QStringLiteral("ScpDPI"));
    DashboardController dashboard;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("dashboard"), &dashboard);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("ScpDPI"), QStringLiteral("Main"));
    return app.exec();
}
