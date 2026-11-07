import QtQuick
import QtTest
import "../../qml" as App

// The sidebar on its own with the real TabController: clicking, renaming, drag and drop, and the keyboard.
Item {
    width: 300
    height: 500

    App.Sidebar {
        id: sidebar
        anchors.fill: parent
        controller: tabs
    }

    SignalSpy { id: deleteSpy; target: sidebar; signalName: "deleteGroupRequested" }

    TestCase {
        name: "Sidebar"
        when: windowShown

        function init() {
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
            deleteSpy.clear()
            sidebar.renamingGroupId = -1
        }

        // Builds   [A] a1 a2   [B] b1   [C]   and returns {a, b, c, a1, a2, b1} (group and tab ids).
        function buildTree() {
            const a = tabs.newGroup("A")
            tabs.openFile(fixtures.many[0])
            const a1 = tabs.activeTabId
            tabs.openFile(fixtures.many[1])
            const a2 = tabs.activeTabId
            const b = tabs.newGroup("B")
            tabs.openFile(fixtures.many[2])
            const b1 = tabs.activeTabId
            const c = tabs.newGroup("C")
            wait(50)
            return { a: a, b: b, c: c, a1: a1, a2: a2, b1: b1 }
        }

        function titles() {
            const result = []
            for (let i = 0; i < sidebar.list.count; ++i) {
                const item = sidebar.list.itemAtIndex(i)
                result.push(item.isGroup ? "[" + item.title + "]" : item.title)
            }
            return result.join(" ")
        }

        // The list updates its delegates a moment after the model changes.
        function expectTitles(expected) {
            tryVerify(() => titles() === expected, 2000, "rows were: " + titles() + "\nexpected: " + expected)
        }

        function rowCenter(row) {
            const item = sidebar.list.itemAtIndex(row)
            verify(item, "row " + row + " is not shown")
            return item.mapToItem(sidebar, item.width / 2, item.height / 2)
        }

        function clickRow(row) {
            const p = rowCenter(row)
            mouseClick(sidebar, p.x, p.y)
        }

        // A drag the way a user does it: press, move in steps, release.
        function drag(fromRow, toRow, toLowerHalf) {
            const from = rowCenter(fromRow)
            const target = sidebar.list.itemAtIndex(toRow)
            const to = target.mapToItem(sidebar, target.width / 2, toLowerHalf ? target.height * 0.8 : target.height * 0.2)
            mousePress(sidebar, from.x, from.y)
            for (let i = 1; i <= 8; ++i)
                mouseMove(sidebar, from.x, from.y + (to.y - from.y) * i / 8)
            wait(60)
            mouseRelease(sidebar, to.x, to.y)
            wait(60)
        }

        function test_rows_show_groups_with_their_tabs() {
            buildTree()
            expectTitles("[A] doc1 doc2 [B] doc3 [C]")
        }

        function test_clicking_a_tab_shows_it_and_clicking_a_group_targets_it() {
            const t = buildTree()
            clickRow(1)  // doc1
            compare(tabs.activeTabId, t.a1)
            clickRow(3)  // [B]
            compare(tabs.activeGroupId, t.b)
            compare(tabs.activeTabId, t.a1)  // selecting a group keeps the open document
        }

        function test_the_arrow_collapses_and_expands() {
            const t = buildTree()
            const header = sidebar.list.itemAtIndex(0)
            const arrow = header.mapToItem(sidebar, 14, header.height / 2)
            mouseClick(sidebar, arrow.x, arrow.y)
            tryCompare(sidebar.list, "count", 4)
            expectTitles("[A] [B] doc3 [C]")
            compare(tabs.model.groupTabCount(t.a), 2)  // collapsed, not closed
            mouseClick(sidebar, arrow.x, arrow.y)
            tryCompare(sidebar.list, "count", 6)
            expectTitles("[A] doc1 doc2 [B] doc3 [C]")
        }

        function test_double_click_renames_a_group() {
            const t = buildTree()
            const p = rowCenter(0)
            mouseClick(sidebar, p.x + 40, p.y)
            mouseClick(sidebar, p.x + 40, p.y)
            tryCompare(sidebar, "renamingGroupId", t.a)
            keyClick(Qt.Key_A, Qt.ControlModifier)  // select all
            keyClick("N")
            keyClick("e")
            keyClick("w")
            keyClick(Qt.Key_Return)
            tryCompare(sidebar, "renamingGroupId", -1)
            expectTitles("[New] doc1 doc2 [B] doc3 [C]")
        }

        function test_escape_cancels_renaming() {
            const t = buildTree()
            sidebar.beginRename(t.b)
            tryCompare(sidebar, "renamingGroupId", t.b)
            keyClick("x")
            keyClick(Qt.Key_Escape)
            tryCompare(sidebar, "renamingGroupId", -1)
            expectTitles("[A] doc1 doc2 [B] doc3 [C]")
        }

        function test_new_group_starts_in_rename_mode() {
            buildTree()
            sidebar.newGroup()
            tryVerify(() => sidebar.renamingGroupId >= 0, 2000)
            compare(sidebar.renamingGroupId, tabs.activeGroupId)
            keyClick(Qt.Key_Escape)
        }

        function test_dragging_a_tab_onto_another_group_header_moves_it_there() {
            const t = buildTree()
            // rows: 0 [A], 1 doc1, 2 doc2, 3 [B], 4 doc3, 5 [C]
            drag(1, 5, false)  // doc1 onto the header of C
            expectTitles("[A] doc2 [B] doc3 [C] doc1")
            compare(tabs.model.groupTabCount(t.c), 1)
            compare(tabs.model.groupTabCount(t.a), 1)
        }

        function test_dragging_a_tab_between_tabs_of_another_group() {
            buildTree()
            drag(1, 4, false)  // doc1 onto the upper half of doc3
            expectTitles("[A] doc2 [B] doc1 doc3 [C]")
            drag(4, 1, true)  // doc3 (row 4) onto the lower half of doc2 (row 1): into group A after doc2
            expectTitles("[A] doc2 doc3 [B] doc1 [C]")
        }

        function test_dragging_within_a_group_reorders() {
            buildTree()
            drag(1, 2, true)  // doc1 below doc2
            expectTitles("[A] doc2 doc1 [B] doc3 [C]")
        }

        function test_dragging_a_group_reorders_groups_with_their_tabs() {
            buildTree()
            drag(0, 5, true)  // [A] below [C]
            expectTitles("[B] doc3 [C] [A] doc1 doc2")
            drag(3, 0, false)  // [A] (row 3 now) above [B]
            expectTitles("[A] doc1 doc2 [B] doc3 [C]")
        }

        function test_a_short_drag_is_a_click_not_a_move() {
            const t = buildTree()
            const p = rowCenter(2)
            mousePress(sidebar, p.x, p.y)
            mouseMove(sidebar, p.x, p.y + 1)
            mouseRelease(sidebar, p.x, p.y + 1)
            expectTitles("[A] doc1 doc2 [B] doc3 [C]")
            compare(tabs.activeTabId, t.a2)
        }

        function test_keyboard_navigation() {
            const t = buildTree()
            sidebar.list.forceActiveFocus()
            clickRow(0)                       // cursor on [A], list focused
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Return)           // doc1
            compare(tabs.activeTabId, t.a1)
            keyClick(Qt.Key_Down)
            keyClick(Qt.Key_Space)            // doc2
            compare(tabs.activeTabId, t.a2)

            keyClick(Qt.Key_Left)             // from a tab: to its group header
            compare(sidebar.list.currentIndex, 0)
            keyClick(Qt.Key_Left)             // on the header: collapse
            tryCompare(sidebar.list, "count", 4)
            keyClick(Qt.Key_Right)            // expand again
            tryCompare(sidebar.list, "count", 6)
            keyClick(Qt.Key_Return)           // Return on a header folds it
            tryCompare(sidebar.list, "count", 4)
            keyClick(Qt.Key_Return)
            tryCompare(sidebar.list, "count", 6)
        }

        function test_keyboard_rename_and_delete() {
            const t = buildTree()
            clickRow(3)                       // [B]
            sidebar.list.forceActiveFocus()
            keyClick(Qt.Key_F2)
            tryCompare(sidebar, "renamingGroupId", t.b)
            keyClick(Qt.Key_A, Qt.ControlModifier)
            keyClick("Z")
            keyClick(Qt.Key_Return)
            expectTitles("[A] doc1 doc2 [Z] doc3 [C]")

            sidebar.list.forceActiveFocus()
            clickRow(4)                       // doc3
            sidebar.list.forceActiveFocus()
            keyClick(Qt.Key_Delete)           // closes the tab
            expectTitles("[A] doc1 doc2 [Z] [C]")

            wait(450)                         // not a double-click on the same row
            clickRow(3)                       // [Z]
            sidebar.list.forceActiveFocus()
            keyClick(Qt.Key_Delete)           // asks to delete the group
            compare(deleteSpy.count, 1)
            compare(deleteSpy.signalArguments[0][0], t.b)
        }

        function test_the_cursor_follows_the_active_tab() {
            const t = buildTree()
            tabs.activate(t.a2)
            compare(sidebar.list.currentIndex, 2)
            tabs.activate(t.b1)
            compare(sidebar.list.currentIndex, 4)
        }

        function test_many_tabs_scroll() {
            tabs.newGroup("Big")
            for (const url of fixtures.many)
                tabs.openFile(url)
            for (let i = 0; i < 3; ++i)
                tabs.newGroup("G" + i)
            wait(50)
            verify(sidebar.list.contentHeight > 0)
            sidebar.list.positionViewAtEnd()
            verify(sidebar.list.itemAtIndex(sidebar.list.count - 1) !== null)
        }
    }
}
