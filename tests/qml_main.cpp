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
#include <QTest>
#include <QUrl>
#include <QVariantMap>
#include <QWindow>
#include <QtQuickTest>

#include <memory>

// Lets QML tests send key presses to a window other than the test's own (e.g. the application window), so keyboard
// shortcuts can be tested for real.
class KeyInjector : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;

  // NOLINTNEXTLINE(readability-identifier-naming): QML-facing API
  Q_INVOKABLE void keyClick(QWindow* window, int key, int modifiers) const {
    QTest::keyClick(window, static_cast<Qt::Key>(key), static_cast<Qt::KeyboardModifiers>(modifiers));
  }

  // Types `text` into the window's focused item.
  // NOLINTNEXTLINE(readability-identifier-naming): QML-facing API
  Q_INVOKABLE void keyClicks(QWindow* window, const QString& text) const {
    for (const QChar character : text) {
      QTest::keyClick(window, character.toLatin1());
    }
  }
};

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
    const QString linked = dir_.filePath("linked.pdf");
    if (!sheaf::testing::WritePdfFixture(alpha, "ALPHA", 150, false) ||
        !sheaf::testing::WritePdfFixture(beta, "BETA", 180, true) || !sheaf::testing::WritePdfWithLinks(linked, 60)) {
      qFatal("Cannot write fixture PDFs");
    }
    // QUICK_TEST_MAIN destroys the application object before this one, and the database needs it to close.
    g_setup = this;
    qAddPostRoutine(ReleaseSetup);
    // Small distinct documents for tests that need many tabs.
    QVariantList many;
    for (int i = 1; i <= 8; ++i) {
      const QString path = dir_.filePath(QString("doc%1.pdf").arg(i));
      if (!sheaf::testing::WritePdfFixture(path, QString("DOC%1").arg(i), 3, false)) {
        qFatal("Cannot write fixture PDFs");
      }
      many.append(QUrl::fromLocalFile(path));
    }
    positions_ = std::make_unique<sheaf::PositionStore>(database_);
    tabs_ = std::make_unique<sheaf::TabController>(*positions_);

    // Optional: real documents for tests/qml/tst_realpdfs.qml, e.g. SHEAF_TEST_PDFS="/a/one.pdf:/b/two.pdf".
    QVariantList real_pdfs;
    for (const QString& path : qEnvironmentVariable("SHEAF_TEST_PDFS").split(':', Qt::SkipEmptyParts)) {
      real_pdfs.append(QUrl::fromLocalFile(path));
    }
    engine->rootContext()->setContextProperty("realPdfs", real_pdfs);

    engine->rootContext()->setContextProperty("keys", &keys_);
    engine->rootContext()->setContextProperty("tabs", tabs_.get());
    engine->rootContext()->setContextProperty("fixtures", QVariantMap{{"alpha", QUrl::fromLocalFile(alpha)},
                                                                      {"beta", QUrl::fromLocalFile(beta)},
                                                                      {"linked", QUrl::fromLocalFile(linked)},
                                                                      {"many", many},
                                                                      {"externalUrl", sheaf::testing::kExternalLinkUrl},
                                                                      {"linkJump", sheaf::testing::kLinkJump}});
  }

  // NOLINTNEXTLINE(readability-redundant-access-specifiers): the slots above are public too
public:
  void Release() {
    tabs_.reset();
    positions_.reset();
    database_.Close();
  }

private:
  KeyInjector keys_;
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
