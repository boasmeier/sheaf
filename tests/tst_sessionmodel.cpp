#include "app/SessionModel.h"

#include <QAbstractItemModelTester>
#include <QRandomGenerator>
#include <QSet>
#include <QtTest>

namespace {

using sheaf::Row;
using sheaf::RowKind;

sheaf::Tab MakeTab(const QString& name) {
  return {.id = 0, .path = "/" + name + ".pdf", .file_key = name, .title = name, .history = {}};
}

QStringList Titles(const sheaf::SessionModel& model) {
  QStringList titles;
  for (int row = 0; row < model.rowCount(); ++row) {
    const QModelIndex index = model.index(row);
    titles << (model.data(index, sheaf::SessionModel::KindRole).toString() == "group"
                   ? "[" + model.data(index, Qt::DisplayRole).toString() + "]"
                   : model.data(index, Qt::DisplayRole).toString());
  }
  return titles;
}

// The visible rows computed from scratch from the public queries, to compare against the model's own.
QVector<Row> ExpectedRows(const sheaf::SessionModel& model) {
  QVector<Row> rows;
  for (int g = 0; g < model.GroupCount(); ++g) {
    const sheaf::Group* group = model.FindGroup(model.GroupIdAt(g));
    rows.append({RowKind::kGroup, group->id});
    if (!group->collapsed) {
      for (const sheaf::Tab& tab : group->tabs) {
        rows.append({.kind = RowKind::kTab, .id = tab.id});
      }
    }
  }
  return rows;
}

}  // namespace

class TstSessionModel : public QObject {
  Q_OBJECT

private:
  // Everything that must hold after any sequence of operations.
  static void VerifyConsistent(const sheaf::SessionModel& model, int expected_tabs = -1) {
    QCOMPARE(model.Rows(), ExpectedRows(model));
    QCOMPARE(model.rowCount(), static_cast<int>(model.Rows().size()));
    QSet<int> group_ids;
    QSet<int> tab_ids;
    int tabs = 0;
    for (int g = 0; g < model.GroupCount(); ++g) {
      const sheaf::Group* group = model.FindGroup(model.GroupIdAt(g));
      QVERIFY(group != nullptr);
      QVERIFY2(!group_ids.contains(group->id), "duplicate group id");
      group_ids.insert(group->id);
      for (int i = 0; i < group->tabs.size(); ++i) {
        const sheaf::Tab& tab = group->tabs.at(i);
        QVERIFY2(!tab_ids.contains(tab.id), "tab in two places or duplicate id");
        tab_ids.insert(tab.id);
        QCOMPARE(model.GroupOfTab(tab.id), group->id);
        QCOMPARE(model.IndexInGroup(tab.id), i);
        ++tabs;
      }
    }
    QCOMPARE(model.TabCount(), tabs);
    QCOMPARE(model.TabIds().size(), tabs);
    if (expected_tabs >= 0) {
      QCOMPARE(tabs, expected_tabs);
    }
    if (model.ActiveTabId() >= 0) {
      QVERIFY(model.FindTab(model.ActiveTabId()) != nullptr);
    }
  }

  // NOLINTNEXTLINE(readability-redundant-access-specifiers): moc needs `private slots:` after the helper
private slots:
  void GroupsAndTabsAppearAsFlatRows() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    model.AddTab(a, MakeTab("a1"));
    model.AddTab(a, MakeTab("a2"));
    model.AddTab(b, MakeTab("b1"));
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "a2", "[B]", "b1"}));
    VerifyConsistent(model, 3);
  }

  void CollapsingHidesTabsAndKeepsTheirPlace() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    model.AddTab(a, MakeTab("a1"));
    model.AddTab(a, MakeTab("a2"));
    const int b = model.AddGroup("B");
    model.AddTab(b, MakeTab("b1"));

    const QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QVERIFY(model.SetCollapsed(a, true));
    QCOMPARE(Titles(model), (QStringList{"[A]", "[B]", "b1"}));
    QCOMPARE(removed.count(), 1);  // a precise removal, not a reset
    QCOMPARE(model.TabCount(), 3);
    QVERIFY(!model.SetCollapsed(a, true));  // no change, no signal

    QVERIFY(model.SetCollapsed(a, false));
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "a2", "[B]", "b1"}));
    VerifyConsistent(model, 3);
  }

  void ChangesAreReportedWithoutResetting() {
    sheaf::SessionModel model;
    const QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    const int a = model.AddGroup("A");
    const int tab = model.AddTab(a, MakeTab("a1"));
    model.AddTab(a, MakeTab("a2"));
    model.RenameGroup(a, "Renamed");
    model.MoveTab(tab, a, 1);
    model.SetActiveTab(tab);
    model.RemoveTab(tab);
    QCOMPARE(reset.count(), 0);
  }

  void RenameGroup() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(model.RenameGroup(a, "Lecture notes"));
    QVERIFY(!model.RenameGroup(a, "Lecture notes"));
    QVERIFY(!model.RenameGroup(999, "x"));
    QCOMPARE(Titles(model), (QStringList{"[Lecture notes]"}));
    QVERIFY(changed.count() > 0);
  }

  void RemovingAGroupRemovesItsTabs() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int a1 = model.AddTab(a, MakeTab("a1"));
    model.AddTab(b, MakeTab("b1"));
    model.SetActiveTab(a1);
    QVERIFY(model.RemoveGroup(a));
    QCOMPARE(Titles(model), (QStringList{"[B]", "b1"}));
    QVERIFY(model.FindTab(a1) == nullptr);
    QCOMPARE(model.ActiveTabId(), -1);
    QVERIFY(!model.RemoveGroup(a));
    VerifyConsistent(model, 1);
  }

  void TabsKeepTheirIdsWhenMoved() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int t1 = model.AddTab(a, MakeTab("t1"));
    const int t2 = model.AddTab(a, MakeTab("t2"));
    const int t3 = model.AddTab(b, MakeTab("t3"));
    QVERIFY(t1 != t2 && t2 != t3);
    QVERIFY(model.MoveTab(t1, b, 0));
    QCOMPARE(model.GroupOfTab(t1), b);
    QCOMPARE(model.FindTab(t1)->title, QString("t1"));
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "[B]", "t1", "t3"}));
    QVERIFY(model.MoveTab(t3, b, 0));
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "[B]", "t3", "t1"}));
    QVERIFY(!model.MoveTab(t3, b, 0));  // already there
    QVERIFY(!model.MoveTab(12345, a, 0));
    QVERIFY(!model.MoveTab(t3, 12345, 0));
  }

  void MovingTabsClampsTheIndex() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const int t1 = model.AddTab(a, MakeTab("t1"));
    model.AddTab(a, MakeTab("t2"));
    QVERIFY(model.MoveTab(t1, a, 100));
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "t1"}));
    QVERIFY(model.MoveTab(t1, a, -5));
    QCOMPARE(Titles(model), (QStringList{"[A]", "t1", "t2"}));
  }

  void MovingGroupsReordersThemWithTheirTabs() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int c = model.AddGroup("C");
    model.AddTab(a, MakeTab("a1"));
    model.AddTab(c, MakeTab("c1"));
    QVERIFY(model.MoveGroup(c, 0));
    QCOMPARE(Titles(model), (QStringList{"[C]", "c1", "[A]", "a1", "[B]"}));
    QVERIFY(model.MoveGroup(c, 2));
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "[B]", "[C]", "c1"}));
    QVERIFY(!model.MoveGroup(c, 2));
    QVERIFY(model.MoveGroup(b, 99));
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "[C]", "c1", "[B]"}));
    VerifyConsistent(model, 2);
  }

  void MoveTabByWalksAcrossGroupBorders() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int t1 = model.AddTab(a, MakeTab("t1"));
    model.AddTab(a, MakeTab("t2"));
    model.AddTab(b, MakeTab("t3"));
    QVERIFY(model.MoveTabBy(t1, 1));
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "t1", "[B]", "t3"}));
    QVERIFY(model.MoveTabBy(t1, 1));  // leaves A, enters B at the top
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "[B]", "t1", "t3"}));
    QVERIFY(model.MoveTabBy(t1, 1));
    QVERIFY(model.MoveTabBy(t1, 1) == false);  // last of the last group
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "[B]", "t3", "t1"}));
    QVERIFY(model.MoveTabBy(t1, -1));
    QVERIFY(model.MoveTabBy(t1, -1));  // back into A, at the end
    QCOMPARE(Titles(model), (QStringList{"[A]", "t2", "t1", "[B]", "t3"}));
    const int top = model.TabIds().first();
    QVERIFY(model.MoveTabBy(top, -1) == false);  // first of the first group
    VerifyConsistent(model, 3);
  }

  void DropTabOnTabPlacesItBeforeOrAfter() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int a1 = model.AddTab(a, MakeTab("a1"));
    model.AddTab(a, MakeTab("a2"));
    const int b1 = model.AddTab(b, MakeTab("b1"));
    // rows: [A] a1 a2 [B] b1
    QVERIFY(model.Drop({RowKind::kTab, b1}, 1, false));  // b1 above a1
    QCOMPARE(Titles(model), (QStringList{"[A]", "b1", "a1", "a2", "[B]"}));
    QVERIFY(model.Drop({RowKind::kTab, b1}, 3, true));  // b1 below a2
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "a2", "b1", "[B]"}));
    QVERIFY(model.Drop({RowKind::kTab, a1}, 2, true));  // a1 below a2, within the group
    QCOMPARE(Titles(model), (QStringList{"[A]", "a2", "a1", "b1", "[B]"}));
    QVERIFY(!model.Drop({RowKind::kTab, a1}, 2, false));  // dropped on itself
    VerifyConsistent(model, 3);
  }

  void DropTabOnGroupHeaderAppendsToThatGroup() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int a1 = model.AddTab(a, MakeTab("a1"));
    model.AddTab(b, MakeTab("b1"));
    model.AddTab(b, MakeTab("b2"));
    QVERIFY(model.Drop({RowKind::kTab, a1}, 2, false));  // on the header of B
    QCOMPARE(Titles(model), (QStringList{"[A]", "[B]", "b1", "b2", "a1"}));
    // Dropping on the header of its own group moves it to the end of that group.
    QVERIFY(model.Drop({RowKind::kTab, model.FindGroup(b)->tabs.first().id}, 1, false));
    QCOMPARE(Titles(model), (QStringList{"[A]", "[B]", "b2", "a1", "b1"}));
    VerifyConsistent(model, 3);
  }

  void DropTabOnCollapsedGroupAddsItWithoutShowingIt() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int a1 = model.AddTab(a, MakeTab("a1"));
    model.AddTab(b, MakeTab("b1"));
    model.SetCollapsed(b, true);
    QVERIFY(model.Drop({RowKind::kTab, a1}, 2, false));
    QCOMPARE(Titles(model), (QStringList{"[A]", "[B]"}));
    QCOMPARE(model.FindGroup(b)->tabs.size(), 2);
    VerifyConsistent(model, 2);
  }

  void DropGroupMovesItBeforeOrAfterAnotherGroup() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int b = model.AddGroup("B");
    const int c = model.AddGroup("C");
    model.AddTab(a, MakeTab("a1"));
    model.AddTab(b, MakeTab("b1"));
    // rows: [A] a1 [B] b1 [C]
    QVERIFY(model.Drop({RowKind::kGroup, c}, 0, false));  // above A
    QCOMPARE(Titles(model), (QStringList{"[C]", "[A]", "a1", "[B]", "b1"}));
    QVERIFY(model.Drop({RowKind::kGroup, c}, 4, false));  // on b1, upper half: before B
    QCOMPARE(Titles(model), (QStringList{"[A]", "a1", "[C]", "[B]", "b1"}));
    QVERIFY(model.Drop({RowKind::kGroup, a}, 4, true));  // on b1, lower half: after B
    QCOMPARE(Titles(model), (QStringList{"[C]", "[B]", "b1", "[A]", "a1"}));
    QVERIFY(!model.Drop({RowKind::kGroup, b}, 2, true));  // onto one of its own tabs
    VerifyConsistent(model, 2);
  }

  void DropWithInvalidArgumentsDoesNothing() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const int t = model.AddTab(a, MakeTab("t"));
    QVERIFY(!model.Drop({RowKind::kTab, t}, -1, false));
    QVERIFY(!model.Drop({RowKind::kTab, t}, 99, false));
    QVERIFY(!model.Drop({RowKind::kTab, 999}, 0, false));
    QVERIFY(!model.Drop({RowKind::kGroup, 999}, 0, false));
    VerifyConsistent(model, 1);
  }

  void ActiveTabIsTrackedAndNotifiesTheRows() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    const int a = model.AddGroup("A");
    const int t1 = model.AddTab(a, MakeTab("t1"));
    const int t2 = model.AddTab(a, MakeTab("t2"));
    const QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.SetActiveTab(t2);
    QVERIFY(changed.count() > 0);
    QVERIFY(model.data(model.index(model.RowOf({RowKind::kTab, t2})), sheaf::SessionModel::ActiveRole).toBool());
    QVERIFY(!model.data(model.index(model.RowOf({RowKind::kTab, t1})), sheaf::SessionModel::ActiveRole).toBool());
    model.SetActiveTab(12345);  // unknown tab: no active tab
    QCOMPARE(model.ActiveTabId(), -1);
  }

  void TabLookups() {
    sheaf::SessionModel model;
    const int a = model.AddGroup("A");
    const int t1 = model.AddTab(a, MakeTab("one"));
    QCOMPARE(model.TabIdWithKey("one"), t1);
    QCOMPARE(model.TabIdWithKey("missing"), -1);
    QCOMPARE(model.GroupOfTab(12345), -1);
    QCOMPARE(model.IndexOfGroup(12345), -1);
    QVERIFY(model.FindTab(12345) == nullptr);
    QVERIFY(model.FindGroup(12345) == nullptr);
    QCOMPARE(model.AddTab(12345, MakeTab("x")), -1);
  }

  // Thousands of random operations, valid or not: the tree must stay consistent after every single one and the model
  // must report every change correctly (the tester checks each signal).
  void RandomOperationsKeepTheTreeConsistent() {
    sheaf::SessionModel model;
    const QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    QRandomGenerator random(12345);
    const auto pick = [&](int bound) { return bound <= 0 ? 0 : random.bounded(bound); };
    const auto random_group = [&] { return model.GroupCount() == 0 ? 999 : model.GroupIdAt(pick(model.GroupCount())); };
    const auto random_tab = [&] {
      const QVector<int> ids = model.TabIds();
      return ids.isEmpty() ? 999 : ids.at(pick(static_cast<int>(ids.size())));
    };

    int added = 0;
    for (int step = 0; step < 4000; ++step) {
      switch (pick(12)) {
        case 0:
          model.AddGroup(QString("g%1").arg(step));
          break;
        case 1:
        case 2:
          if (model.AddTab(random_group(), MakeTab(QString("t%1").arg(step)), pick(6) - 1) >= 0) {
            ++added;
          }
          break;
        case 3:
          model.RemoveTab(random_tab());
          break;
        case 4:
          model.MoveTab(random_tab(), random_group(), pick(8) - 1);
          break;
        case 5:
          model.MoveGroup(random_group(), pick(6) - 1);
          break;
        case 6:
          model.SetCollapsed(random_group(), pick(2) == 0);
          break;
        case 7:
          model.MoveTabBy(random_tab(), pick(2) == 0 ? 1 : -1);
          break;
        case 8:
        case 9: {
          const bool group = pick(3) == 0;
          model.Drop({group ? RowKind::kGroup : RowKind::kTab, group ? random_group() : random_tab()},
                     pick(model.rowCount() + 1) - 1, pick(2) == 0);
          break;
        }
        case 10:
          model.SetActiveTab(random_tab());
          break;
        default:
          if (model.GroupCount() > 3 && pick(4) == 0) {
            model.RemoveGroup(random_group());
          }
          break;
      }
      VerifyConsistent(model);
      if (QTest::currentTestFailed()) {
        QFAIL(qPrintable(QString("inconsistent after step %1").arg(step)));
      }
    }
    QVERIFY(added > 100);
  }
};

QTEST_GUILESS_MAIN(TstSessionModel)
#include "tst_sessionmodel.moc"
