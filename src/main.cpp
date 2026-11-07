#include "app/TabController.h"
#include "storage/Database.h"
#include "storage/PositionStore.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
  parser.addPositionalArgument("files", "PDF files to open.", "[files...]");
  parser.process(app);

  sheaf::PositionStore positions(database);
  sheaf::TabController tabs(positions);
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &tabs, [&tabs] { tabs.Flush(); });

  const int first_cli_tab = tabs.model()->Count();
  for (const QString& argument : parser.positionalArguments()) {
    const QUrl url = QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
    if (!tabs.openFile(url)) {
      qWarning() << "Cannot open" << argument;
    }
  }
  if (tabs.model()->Count() > first_cli_tab) {
    tabs.activate(first_cli_tab);
  }

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("tabs", &tabs);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
      Qt::QueuedConnection);
  engine.loadFromModule("Sheaf", "Main");

  return QGuiApplication::exec();
}
