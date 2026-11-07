// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/QmlTypes.h"
#include "app/TabController.h"
#include "app/UnixSignals.h"
#include "storage/Database.h"
#include "storage/PositionStore.h"
#include "storage/SessionStore.h"

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

  sheaf::InstallQuitSignalHandlers();
  sheaf::RegisterQmlTypes();

  sheaf::PositionStore positions(database);
  sheaf::SessionStore session(database);
  sheaf::TabController tabs(positions, session);
  tabs.Restore();  // the groups and tabs of last time
  QObject::connect(&app, &QCoreApplication::aboutToQuit, &tabs, [&tabs] { tabs.Flush(); });

  // Open every file given; the first one is the one shown.
  int first_cli_tab = -1;
  for (const QString& argument : parser.positionalArguments()) {
    const QUrl url = QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
    if (!tabs.openFile(url)) {
      qWarning() << "Cannot open" << argument;
    }
    else if (first_cli_tab < 0) {
      first_cli_tab = tabs.activeTabId();
    }
  }
  if (first_cli_tab >= 0) {
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
