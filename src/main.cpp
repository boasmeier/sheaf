#include "storage/Database.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtLogging>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setApplicationName("sheaf");
  QGuiApplication::setOrganizationName("sheaf");

  sheaf::Database database("main");
  if (!database.Open(sheaf::Database::DefaultPath())) {
    qCritical() << "Cannot open database:" << database.Error();
    return 1;
  }

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
      Qt::QueuedConnection);
  engine.loadFromModule("Sheaf", "Main");

  return QGuiApplication::exec();
}
