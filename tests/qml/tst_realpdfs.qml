import QtQuick
import QtTest
import QtQuick.Pdf
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

        Component {
            id: linkModelComponent
            PdfLinkModel {}
        }

        // The first internal link found on pages [from, from + 80): {page, link: {rect, targetPage}}, or null.
        function findInternalLink(document, from) {
            const model = createTemporaryObject(linkModelComponent, win, { document: document })
            // Role ids of QPdfLinkModel::Role: Link = Qt.UserRole, Rectangle, Url, Page, Location, Zoom.
            const rectangle = Qt.UserRole + 1
            const page = Qt.UserRole + 3
            for (let p = from; p < Math.min(document.pageCount, from + 80); ++p) {
                model.page = p
                for (let row = 0; row < model.rowCount(); ++row) {
                    const target = model.data(model.index(row, 0), page)
                    if (target >= 0 && target !== p)
                        return { page: p, rect: model.data(model.index(row, 0), rectangle), targetPage: target }
                }
            }
            return null
        }

        function test_following_a_real_link_and_going_back() {
            if (realPdfs.length < 1)
                skip("no real PDFs given")
            while (tabs.model.rowCount() > 0)
                tabs.closeTab(0)
            tryVerify(() => !win.viewer.hasDocument, 5000)
            waitForSwitch(() => tabs.openFile(realPdfs[0]))

            const found = findInternalLink(win.viewer.pane.document, 1)
            verify(found !== null, "no internal link in the first pages of " + realPdfs[0])
            // Put the link in view: its page, scrolled so the link is a little below the top of the viewport.
            const scale = win.viewer.zoom
            const pageHeight = win.viewer.pane.document.pagePointSize(found.page).height
            win.viewer.jumpTo({ page: found.page, offsetY: Math.max(0, found.rect.y / pageHeight - 0.1), zoom: scale })
            tryCompare(win.viewer, "currentPage", found.page, 5000)
            wait(1500)
            const before = { contentY: win.viewer.pane.flickable().contentY, zoom: win.viewer.zoom }

            const holder = win.viewer.pane.flickable().itemAtCell(0, found.page)
            const paper = holder.children.find(child => child.pagePointSize !== undefined)
            const point = paper.mapToItem(win.viewer, (found.rect.x + found.rect.width / 2) * scale,
                                          (found.rect.y + found.rect.height / 2) * scale)
            mouseClick(win.viewer, point.x, point.y)
            tryVerify(() => tabs.canGoBack, 5000)
            wait(1500)
            verify(Math.abs(win.viewer.currentPage - found.targetPage) <= 1,
                   "link to page " + found.targetPage + " landed on " + win.viewer.currentPage)
            fuzzyCompare(win.viewer.zoom, scale, 0.0001, "following a link must keep the zoom")

            win.navigateBack()
            wait(1500)
            fuzzyCompare(win.viewer.pane.flickable().contentY, before.contentY, 2.5, "exact position after going back")
            fuzzyCompare(win.viewer.zoom, before.zoom, 0.0001)
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
                // The first visit shows whatever an earlier test left stored (or page 0); later visits our own page.
                const stored = tabs.activePosition
                if (i < 2)
                    remembered[tab] = stored.valid ? stored.page : 0
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
