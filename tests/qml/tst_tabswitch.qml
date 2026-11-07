import QtQuick
import QtTest
import "helpers.js" as Helpers

// The real application window with the real TabController: open two documents and switch between them, as a user
// does. Every switch must come back to the page that was left, and show exactly that page.
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "TabSwitch"
        when: windowShown

        function initTestCase() {
            const component = Qt.createComponent(Qt.resolvedUrl("../../qml/Main.qml"))
            compare(component.status, Component.Ready, component.errorString())
            win = component.createObject(null)
            win.width = 1000
            win.height = 700
            win.show()
        }

        function cleanupTestCase() {
            win.destroy()
        }

        function waitForSwitch(action) {
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            action()
            spy.wait(10000)
            spy.destroy()
        }

        Component {
            id: spyComponent
            SignalSpy { signalName: "switched" }
        }

        function freshFingerprint() {
            const flick = win.viewer.pane.flickable()
            const y = flick.contentY
            flick.contentY = y + 6000
            wait(400)
            flick.contentY = y
            wait(600)
            return Helpers.fingerprint(grabImage(win.contentItem))
        }

        function test_switching_tabs_restores_and_renders_the_left_page() {
            tabs.openFile(fixtures.alpha)
            tryVerify(() => win.viewer.hasDocument, 10000)
            waitForSwitch(() => tabs.openFile(fixtures.beta))
            compare(tabs.model.rowCount(), 2)
            compare(tabs.activeIndex, 1)

            const remembered = [0, 0]
            const targets = [60, 120, 30, 140, 85, 170, 12, 99, 150, 7]
            for (let i = 0; i < 12; ++i) {
                const tab = i % 2          // 0 = alpha, 1 = beta; the active tab is always the other one
                waitForSwitch(() => tabs.activate(tab))
                compare(tabs.activeIndex, tab)
                const top = win.viewer.topPosition()
                compare(top.page, remembered[tab], "tab " + tab + " did not return to its page (switch " + i + ")")
                compare(win.viewer.currentPage, remembered[tab])

                // Nothing wrong on screen the moment the switch completes.
                const immediate = Helpers.fingerprint(grabImage(win.contentItem))
                compare(immediate, freshFingerprint(), "wrong page image after switch " + i)

                // Move somewhere new and let the position settle and be reported.
                const page = targets[i % targets.length] % (tab === 0 ? 150 : 180)
                win.viewer.goToPage(page)
                tryCompare(win.viewer, "currentPage", page, 3000)
                wait(400)
                remembered[tab] = win.viewer.topPosition().page
                compare(remembered[tab], page)
            }
        }

        function test_closing_all_tabs_clears_the_viewer() {
            tabs.closeTab(1)
            tabs.closeTab(0)
            compare(tabs.activeIndex, -1)
            tryVerify(() => !win.viewer.hasDocument, 3000)
        }
    }
}
