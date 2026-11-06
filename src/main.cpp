#include "storage/Database.h"

#include <QCommandLineParser>
#include <QDir>
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

  QCommandLineParser parser;
  parser.setApplicationDescription("A minimal, fast PDF viewer");
  parser.addHelpOption();
  parser.addPositionalArgument("file", "PDF file to open.", "[file]");
  parser.process(app);

  QUrl initial_file;
  if (!parser.positionalArguments().isEmpty()) {
    initial_file =
        QUrl::fromUserInput(parser.positionalArguments().first(), QDir::currentPath(), QUrl::AssumeLocalFile);
  }

  QQmlApplicationEngine engine;
  engine.setInitialProperties({{"initialFile", initial_file}});
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
      Qt::QueuedConnection);
  engine.loadFromModule("Sheaf", "Main");

  return QGuiApplication::exec();
}
