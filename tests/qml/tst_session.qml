import QtQuick
import QtQuick.Controls
import QtTest

// Session restore through the real window: quit and start again (the controller is rebuilt from the database exactly
// as at start-up) and the sidebar, the active document and its position must be back; a document whose file went
// missing shows "File not found" instead of failing.
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "SessionRestore"
        when: windowShown

        function init() {
            closeWindow()
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
        }

        function cleanup() {
            closeWindow()
        }

        // `keepSavedSize`: leave the size to the remembered window state instead of setting one.
        function openWindow(keepSavedSize) {
            const component = Qt.createComponent(Qt.resolvedUrl("../../qml/Main.qml"))
            compare(component.status, Component.Ready, component.errorString())
            win = component.createObject(null)
            if (!keepSavedSize) {
                win.width = 1000
                win.height = 700
            }
            win.show()
            return win
        }

        function closeWindow() {
            if (win) {
                win.destroy()
                win = null
                wait(50)  // destruction is deferred; nothing may still be bound to the controller when it is replaced
            }
        }

        // Quit and start again.
        function restart() {
            closeWindow()
            control.restart()
            return openWindow()
        }

        function titles() {
            const result = []
            for (let i = 0; i < win.sidebar.list.count; ++i) {
                const item = win.sidebar.list.itemAtIndex(i)
                if (!item)
                    continue
                result.push(item.isGroup ? "[" + item.title + (item.collapsed ? "+" : "") + "]" : item.title + (item.missing ? "!" : ""))
            }
            return result.join(" ")
        }

        function expectTitles(expected) {
            tryVerify(() => titles() === expected, 3000, "rows were: " + titles() + "\nexpected: " + expected)
        }

        function activeRow() {
            for (let i = 0; i < win.sidebar.list.count; ++i) {
                const item = win.sidebar.list.itemAtIndex(i)
                if (item && item.active)
                    return item.title
            }
            return ""
        }

        function test_restart_restores_sidebar_active_tab_and_position() {
            openWindow()
            tabs.newGroup("Probabilistic AI")
            tabs.openFile(fixtures.many[0])
            tabs.openFile(fixtures.alpha)
            tabs.newGroup("Maths")
            tabs.openFile(fixtures.many[1])
            const maths = tabs.activeGroupId
            tabs.newGroup("Archive")
            tabs.openFile(fixtures.beta)
            tabs.setCollapsed(tabs.groups()[2].id, true)
            tabs.activate(tabs.tabIds()[1])  // alpha
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === 150, 10000)
            wait(300)
            win.viewer.jumpTo({ page: 42, offsetY: 0.3, zoom: 1.25 })
            tryVerify(() => win.viewer.topPosition().page === 42, 3000)
            wait(500)  // the position is reported once the view has been still for a moment
            expectTitles("[Probabilistic AI] doc1 alpha [Maths] doc2 [Archive+]")
            const before = titles()
            const groupsBefore = JSON.stringify(tabs.groups().map(g => g.name))

            restart()  // quit and start again

            expectTitles(before)
            compare(JSON.stringify(tabs.groups().map(g => g.name)), groupsBefore)
            compare(activeRow(), "alpha")
            compare(tabs.activeTitle, "alpha")
            compare(win.title, "alpha – sheaf")
            // The document is shown again at the remembered place and zoom.
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === 150, 10000)
            compare(win.viewer.topPosition().page, 42)
            fuzzyCompare(win.viewer.topPosition().offset, 0.3, 0.02)
            fuzzyCompare(win.viewer.zoom, 1.25, 0.001)
        }

        function test_restarting_twice_changes_nothing() {
            openWindow()
            tabs.newGroup("One")
            tabs.openFile(fixtures.many[0])
            tabs.openFile(fixtures.many[1])
            tabs.newGroup("Two")
            tabs.openFile(fixtures.many[2])
            wait(300)
            expectTitles("[One] doc1 doc2 [Two] doc3")
            restart()
            expectTitles("[One] doc1 doc2 [Two] doc3")
            restart()
            expectTitles("[One] doc1 doc2 [Two] doc3")
            compare(activeRow(), "doc3")
        }

        function test_an_empty_session_starts_empty() {
            restart()
            compare(win.sidebar.list.count, 0)
            verify(!win.viewer.hasDocument)
            verify(!win.missingPanel.visible)
        }

        function test_only_the_active_document_is_loaded_after_a_restart() {
            openWindow()
            tabs.newGroup("G")
            for (let i = 0; i < 8; ++i)
                tabs.openFile(fixtures.many[i])
            tabs.activate(tabs.tabIds()[3])
            wait(300)
            restart()
            tryVerify(() => win.viewer.hasDocument, 10000)
            compare(tabs.tabCount(), 8)
            compare(win.viewer.pageCount, 3)
            compare(tabs.activeTitle, "doc4")
        }

        // ---- Missing files ----

        // A working copy of a fixture that the test may delete or move.
        function workFile(name, fixture) {
            const path = control.tempPath(name)
            verify(control.copyFile(fixture, path))
            return path
        }

        function test_a_deleted_file_shows_file_not_found_after_a_restart() {
            const lost = workFile("lost.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile(fixtures.many[0])
            tabs.openFile("file://" + lost)
            tryVerify(() => win.viewer.hasDocument, 10000)
            wait(400)  // moves are reported once the pane has settled
            win.viewer.jumpTo({ page: 17, offsetY: 0, zoom: 1.0 })
            tryVerify(() => win.viewer.topPosition().page === 17, 3000)
            wait(500)
            verify(control.removeFile(lost))

            restart()  // must not fail because of the missing file

            expectTitles("[G] doc1 work-lost!")
            compare(tabs.activeTitle, "work-lost")
            verify(tabs.activeTabMissing)
            verify(win.missingPanel.visible)
            verify(!win.viewer.hasDocument)
            // The other tab still works.
            tabs.activate(tabs.tabIds()[0])
            tryVerify(() => win.viewer.hasDocument, 10000)
            verify(!win.missingPanel.visible)
        }

        function test_locate_relinks_a_moved_file_and_returns_to_the_place() {
            const lost = workFile("moved.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile("file://" + lost)
            tryVerify(() => win.viewer.hasDocument, 10000)
            wait(400)  // moves are reported once the pane has settled
            win.viewer.jumpTo({ page: 23, offsetY: 0.2, zoom: 1.5 })
            tryVerify(() => win.viewer.topPosition().page === 23, 3000)
            wait(500)
            const newPath = control.tempPath("moved-to-new-name.pdf")
            verify(control.renameFile(lost, newPath))

            restart()
            verify(win.missingPanel.visible)
            win.locateActiveTab("file://" + newPath)

            verify(!tabs.activeTabMissing)
            tryVerify(() => !win.missingPanel.visible, 3000)
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === 150, 10000)
            compare(win.viewer.topPosition().page, 23)  // the same file, so the same remembered place
            fuzzyCompare(win.viewer.zoom, 1.5, 0.001)
            expectTitles("[G] work-moved-to-new-name")
        }

        function test_locate_asks_before_accepting_a_different_file() {
            const lost = workFile("original.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile("file://" + lost)
            tryVerify(() => win.viewer.hasDocument, 10000)
            wait(300)
            verify(control.removeFile(lost))

            restart()
            win.locateActiveTab(fixtures.beta)  // a different document
            tryVerify(() => win.mismatchDialog.visible, 2000)
            verify(tabs.activeTabMissing)  // nothing changed yet
            win.mismatchDialog.reject()
            tryVerify(() => !win.mismatchDialog.visible, 2000)
            verify(tabs.activeTabMissing)
            verify(win.missingPanel.visible)

            win.locateActiveTab(fixtures.beta)
            tryVerify(() => win.mismatchDialog.visible, 2000)
            win.mismatchDialog.accept()
            tryVerify(() => !tabs.activeTabMissing, 3000)
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === 180, 10000)
        }

        function test_locate_reports_an_unreadable_file() {
            const lost = workFile("gone.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile("file://" + lost)
            tryVerify(() => win.viewer.hasDocument, 10000)
            wait(300)
            verify(control.removeFile(lost))
            restart()
            win.locateActiveTab("file:///nonexistent/nothing.pdf")
            tryVerify(() => win.infoDialog.visible, 2000)
            verify(tabs.activeTabMissing)
            win.infoDialog.accept()
        }

        function test_a_missing_tab_can_be_removed() {
            const lost = workFile("remove-me.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile(fixtures.many[0])
            tabs.openFile("file://" + lost)
            tryVerify(() => win.viewer.hasDocument, 10000)
            wait(300)
            verify(control.removeFile(lost))
            restart()
            verify(win.missingPanel.visible)
            tabs.closeTab(tabs.activeTabId)  // what the "Remove tab" button does
            expectTitles("[G] doc1")
            tryVerify(() => !win.missingPanel.visible, 3000)
            tryVerify(() => win.viewer.hasDocument, 10000)
        }

        function test_a_file_that_disappears_while_the_app_runs_is_noticed_when_its_tab_is_shown() {
            const lost = workFile("vanishes.pdf", fixtures.alpha)
            openWindow()
            tabs.newGroup("G")
            tabs.openFile("file://" + lost)
            tabs.openFile(fixtures.many[0])
            tryVerify(() => win.viewer.hasDocument, 10000)
            verify(control.removeFile(lost))
            tabs.activate(tabs.tabIds()[0])
            tryVerify(() => win.missingPanel.visible, 5000)
            expectTitles("[G] work-vanishes! doc1")
        }

        // ---- Window state ----

        function test_window_size_and_sidebar_width_are_remembered() {
            openWindow()
            win.width = 777
            win.height = 555
            win.sidebar.SplitView.preferredWidth = 310
            wait(300)
            win.saveWindowState()
            compare(Number(tabs.value("window_width", 0)), 777)
            compare(Number(tabs.value("window_height", 0)), 555)
            closeWindow()

            openWindow(true)  // a new start
            tryCompare(win, "width", 777, 3000)
            compare(win.height, 555)
            tryVerify(() => Math.abs(win.sidebar.width - 310) < 2, 3000, "sidebar width is " + win.sidebar.width)  // after layout
        }

        function test_nonsense_saved_geometry_is_ignored() {
            tabs.setValue("window_width", 3)       // far too small
            tabs.setValue("window_height", 99999)
            tabs.setValue("sidebar_width", "abc")
            openWindow()
            verify(win.width >= 480)
            verify(win.sidebar.width >= 140)
            tabs.setValue("window_width", 1000)
            tabs.setValue("window_height", 700)
            tabs.setValue("sidebar_width", 240)
        }
    }
}
