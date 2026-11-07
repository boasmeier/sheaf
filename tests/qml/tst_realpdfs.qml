import QtQuick
import QtTest
import "helpers.js" as Helpers

// The tab-switch scenario on real documents. Skipped unless SHEAF_TEST_PDFS lists at least two PDFs, e.g.
//   SHEAF_TEST_PDFS="/path/one.pdf:/path/two.pdf" ctest --test-dir build -R tst_qml --output-on-failure
// Real course PDFs found bugs that generated fixtures did not (a page image stuck on page 1 after a jump).
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "RealPdfs"
        when: windowShown

        function initTestCase() {
            if (realPdfs.length < 2)
                skip("set SHEAF_TEST_PDFS=/path/one.pdf:/path/two.pdf to run")
            const component = Qt.createComponent(Qt.resolvedUrl("../../qml/Main.qml"))
            compare(component.status, Component.Ready, component.errorString())
            win = component.createObject(null)
            win.width = 1100
            win.height = 750
            win.show()
        }

        function cleanupTestCase() {
            if (win)
                win.destroy()
        }

        Component {
            id: spyComponent
            SignalSpy { signalName: "switched" }
        }

        function waitForSwitch(action) {
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            action()
            spy.wait(30000)
            spy.destroy()
        }

        function freshFingerprint() {
            const flick = win.viewer.pane.flickable()
            const y = flick.contentY
            flick.contentY = y + 8000
            wait(800)
            flick.contentY = y
            wait(1500)
            return Helpers.fingerprint(grabImage(win.contentItem))
        }

        function test_switching_between_real_documents() {
            if (realPdfs.length < 2)
                skip("no real PDFs given")
            tabs.openFile(realPdfs[0])
            tryVerify(() => win.viewer.hasDocument, 30000)
            waitForSwitch(() => tabs.openFile(realPdfs[1]))

            const remembered = [0, 0]
            for (let i = 0; i < 12; ++i) {
                const tab = i % 2
                waitForSwitch(() => tabs.activate(tab))
                compare(win.viewer.topPosition().page, remembered[tab], "tab " + tab + " lost its page (switch " + i + ")")

                const immediate = Helpers.fingerprint(grabImage(win.contentItem))
                compare(immediate, freshFingerprint(), "wrong page image on tab " + tab + " (switch " + i + ")")

                const page = 20 + (i * 47) % Math.max(1, win.viewer.pageCount - 40)
                win.viewer.goToPage(page)
                tryCompare(win.viewer, "currentPage", page, 5000)
                wait(500)
                remembered[tab] = win.viewer.topPosition().page
                compare(remembered[tab], page)
            }
        }
    }
}
