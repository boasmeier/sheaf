import QtQuick
import QtTest

// Groups and tabs through the real window: keyboard shortcuts, the sidebar showing the right document, and deleting
// a group. Keys are sent to the application window itself.
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "GroupShortcuts"
        when: windowShown

        function initTestCase() {
            const component = Qt.createComponent(Qt.resolvedUrl("../../qml/Main.qml"))
            compare(component.status, Component.Ready, component.errorString())
            win = component.createObject(null)
            win.width = 1000
            win.height = 700
            win.show()
            win.requestActivate()
            tryVerify(() => win.active, 5000)
        }

        function cleanupTestCase() {
            win.destroy()
        }

        function init() {
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
            tryVerify(() => !win.viewer.hasDocument, 3000)
        }

        function press(key, modifiers) {
            keys.keyClick(win, key, modifiers || Qt.NoModifier)
            wait(100)
        }

        function titles() {
            const result = []
            for (let i = 0; i < win.sidebar.list.count; ++i) {
                const item = win.sidebar.list.itemAtIndex(i)
                result.push(item.isGroup ? "[" + item.title + "]" : item.title)
            }
            return result.join(" ")
        }

        function expectTitles(expected) {
            tryVerify(() => titles() === expected, 2000, "rows were: " + titles() + "\nexpected: " + expected)
        }

        // [One] doc1 doc2   [Two] doc3
        function build() {
            tabs.newGroup("One")
            tabs.openFile(fixtures.many[0])
            tabs.openFile(fixtures.many[1])
            tabs.newGroup("Two")
            tabs.openFile(fixtures.many[2])
            tryVerify(() => win.viewer.hasDocument, 10000)
            expectTitles("[One] doc1 doc2 [Two] doc3")
        }

        function test_ctrl_shift_n_creates_a_group_and_renames_it() {
            build()
            press(Qt.Key_N, Qt.ControlModifier | Qt.ShiftModifier)
            tryVerify(() => win.sidebar.renamingGroupId >= 0, 2000)
            compare(win.sidebar.renamingGroupId, tabs.activeGroupId)
            keys.keyClicks(win, "Exams")
            press(Qt.Key_Return)
            tryCompare(win.sidebar, "renamingGroupId", -1)
            expectTitles("[One] doc1 doc2 [Two] doc3 [Exams]")
            // The new group is where new files go.
            compare(tabs.groups()[2].name, "Exams")
            compare(tabs.activeGroupId, tabs.groups()[2].id)
        }

        function test_f2_renames_the_active_group() {
            build()
            press(Qt.Key_F2)
            tryVerify(() => win.sidebar.renamingGroupId === tabs.activeGroupId, 2000)
            keys.keyClicks(win, "Maths")  // the old name is selected, so typing replaces it
            press(Qt.Key_Return)
            expectTitles("[One] doc1 doc2 [Maths] doc3")
        }

        function test_ctrl_tab_cycles_through_all_groups_in_sidebar_order() {
            build()
            const ids = tabs.tabIds()
            compare(tabs.activeTabId, ids[2])
            press(Qt.Key_Tab, Qt.ControlModifier)
            compare(tabs.activeTabId, ids[0])
            press(Qt.Key_Tab, Qt.ControlModifier)
            compare(tabs.activeTabId, ids[1])
            press(Qt.Key_Tab, Qt.ControlModifier | Qt.ShiftModifier)
            compare(tabs.activeTabId, ids[0])
            tryVerify(() => win.viewer.hasDocument, 5000)
        }

        function test_ctrl_w_closes_the_active_tab() {
            build()
            press(Qt.Key_W, Qt.ControlModifier)
            expectTitles("[One] doc1 doc2 [Two]")
            compare(tabs.activeTitle, "doc2")
        }

        function test_ctrl_shift_arrows_move_the_active_tab() {
            build()
            tabs.activate(tabs.tabIds()[1])  // doc2, last of One
            press(Qt.Key_Down, Qt.ControlModifier | Qt.ShiftModifier)  // into Two, at the top
            expectTitles("[One] doc1 [Two] doc2 doc3")
            compare(tabs.activeTitle, "doc2")
            press(Qt.Key_Up, Qt.ControlModifier | Qt.ShiftModifier)    // and back to the end of One
            expectTitles("[One] doc1 doc2 [Two] doc3")
        }

        function test_files_open_into_the_selected_group() {
            build()
            tabs.selectGroup(tabs.groups()[0].id)  // One
            tabs.openFile(fixtures.many[3])
            expectTitles("[One] doc1 doc2 doc4 [Two] doc3")
        }

        function test_deleting_a_group_with_tabs_asks_first() {
            build()
            const two = tabs.groups()[1].id
            win.deleteGroup(two)
            tryVerify(() => win.deleteGroupDialog.visible, 2000)
            win.deleteGroupDialog.reject()
            tryVerify(() => !win.deleteGroupDialog.visible, 2000)
            expectTitles("[One] doc1 doc2 [Two] doc3")  // declined: nothing happened

            win.deleteGroup(two)
            tryVerify(() => win.deleteGroupDialog.visible, 2000)
            win.deleteGroupDialog.accept()
            expectTitles("[One] doc1 doc2")
            compare(tabs.tabCount(), 2)
            tryVerify(() => win.viewer.hasDocument, 5000)  // a remaining tab is shown
        }

        function test_deleting_an_empty_group_needs_no_question() {
            build()
            const empty = tabs.newGroup("Empty")
            win.deleteGroup(empty)
            verify(!win.deleteGroupDialog.visible)
            expectTitles("[One] doc1 doc2 [Two] doc3")
        }

        function test_deleting_the_last_group_clears_the_viewer() {
            tabs.newGroup("Only")
            tabs.openFile(fixtures.many[0])
            tryVerify(() => win.viewer.hasDocument, 10000)
            win.deleteGroup(tabs.groups()[0].id)
            win.deleteGroupDialog.accept()
            tryVerify(() => !win.viewer.hasDocument, 5000)
            compare(tabs.activeTabId, -1)
        }

        function test_switching_tabs_across_groups_shows_the_right_document() {
            build()
            const ids = tabs.tabIds()
            tabs.activate(ids[0])
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === 3, 5000)
            compare(win.title, "doc1 – sheaf")
            tabs.activate(ids[2])
            tryCompare(win, "title", "doc3 – sheaf", 5000)
        }
    }
}
