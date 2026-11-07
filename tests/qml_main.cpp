// Runs the QML tests in tests/qml against the real QML sources (qml/) with generated fixture PDFs and a real
// TabController, so they exercise the same code paths as the app.
#include "Fixtures.h"
#include "app/TabController.h"
#include "storage/Database.h"
#include "storage/PositionStore.h"

#include <QCoreApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>
#include <QtQuickTest>

#include <memory>

class Setup;
namespace {
Setup* g_setup = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): used by a C post routine
void ReleaseSetup();
}  // namespace

class Setup : public QObject {
  Q_OBJECT

public slots:
  // NOLINTNEXTLINE(readability-identifier-naming): name required by QUICK_TEST_MAIN_WITH_SETUP
  void qmlEngineAvailable(QQmlEngine* engine) {
    if (!dir_.isValid() || !database_.Open(":memory:")) {
      qFatal("Cannot set up test environment");
    }
    // Two documents of different length; beta has mixed page sizes.
    const QString alpha = dir_.filePath("alpha.pdf");
    const QString beta = dir_.filePath("beta.pdf");
    if (!sheaf::testing::WritePdfFixture(alpha, "ALPHA", 150, false) ||
        !sheaf::testing::WritePdfFixture(beta, "BETA", 180, true)) {
      qFatal("Cannot write fixture PDFs");
    }
    // QUICK_TEST_MAIN destroys the application object before this one, and the database needs it to close.
    g_setup = this;
    qAddPostRoutine(ReleaseSetup);
    positions_ = std::make_unique<sheaf::PositionStore>(database_);
    tabs_ = std::make_unique<sheaf::TabController>(*positions_);

    // Optional: real documents for tests/qml/tst_realpdfs.qml, e.g. SHEAF_TEST_PDFS="/a/one.pdf:/b/two.pdf".
    QVariantList real_pdfs;
    for (const QString& path : qEnvironmentVariable("SHEAF_TEST_PDFS").split(':', Qt::SkipEmptyParts)) {
      real_pdfs.append(QUrl::fromLocalFile(path));
    }
    engine->rootContext()->setContextProperty("realPdfs", real_pdfs);

    engine->rootContext()->setContextProperty("tabs", tabs_.get());
    engine->rootContext()->setContextProperty(
        "fixtures", QVariantMap{{"alpha", QUrl::fromLocalFile(alpha)}, {"beta", QUrl::fromLocalFile(beta)}});
  }

  // NOLINTNEXTLINE(readability-redundant-access-specifiers): the slots above are public too
public:
  void Release() {
    tabs_.reset();
    positions_.reset();
    database_.Close();
  }

private:
  QTemporaryDir dir_;
  sheaf::Database database_{"qmltests"};
  std::unique_ptr<sheaf::PositionStore> positions_;
  std::unique_ptr<sheaf::TabController> tabs_;
};

namespace {
void ReleaseSetup() {
  if (g_setup != nullptr) {
    g_setup->Release();
    g_setup = nullptr;
  }
}
}  // namespace

QUICK_TEST_MAIN_WITH_SETUP(qml, Setup)

#include "qml_main.moc"
