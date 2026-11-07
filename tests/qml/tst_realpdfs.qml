// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

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

        function freshImage() {
            const flick = win.viewer.pane.flickable()
            const y = flick.contentY
            flick.contentY = y + 8000
            wait(800)
            flick.contentY = y
            wait(1500)
            return grabImage(win.contentItem)
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
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
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

        // The search targets on real course documents: the first match in view within 200 ms, the full count soon after.
        // The words are common ones for lecture notes; SHEAF_TEST_WORDS="word one,word two" picks others.
        function test_search_meets_its_targets_on_real_documents() {
            if (realPdfs.length < 1)
                skip("no real PDFs given")
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
            tryVerify(() => !win.viewer.hasDocument, 5000)
            for (let d = 0; d < realPdfs.length; ++d) {
                waitForSwitch(() => tabs.openFile(realPdfs[d]))
                wait(600)
                win.viewer.jumpTo({ page: 0, offsetY: 0, zoom: win.viewer.zoom })
                wait(400)
                for (const word of ["the", "of the", "Gaussian"]) {
                    win.viewer.closeSearch()
                    win.viewer.openSearch()
                    win.viewer.searchQuery = word
                    const search = win.viewer.pane.search
                    const timer = Date.now()
                    win.viewer.runSearch(true)
                    let firstMatch = -1
                    let complete = -1
                    for (let i = 0; i < 4000 && complete < 0; ++i) {
                        wait(2)
                        const now = Date.now() - timer
                        if (firstMatch < 0 && search.currentPage >= 0 && win.viewer.pane.view.searchResultInView())
                            firstMatch = now
                        if (search.complete)
                            complete = now
                    }
                    console.log("REAL SEARCH", realPdfs[d].toString().slice(-28), "pages", win.viewer.pageCount, "'" + word + "':",
                                search.count, "matches; first in view after", firstMatch, "ms; complete after", complete, "ms")
                    if (search.count > 0)
                        verify(firstMatch >= 0 && firstMatch < 200, "first match of '" + word + "' took " + firstMatch + " ms")
                    verify(complete >= 0 && complete < 2500, "'" + word + "' took " + complete + " ms to count")
                    // A second search of the same text is served from memory.
                    win.viewer.runSearch(false)
                    const again = Date.now()
                    for (let i = 0; i < 2000 && !search.complete; ++i)
                        wait(2)
                    verify(Date.now() - again < 1500)
                }
                win.viewer.closeSearch()
            }
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
                waitForSwitch(() => tabs.activate(tabs.tabIds()[tab]))
                // The first visit shows whatever an earlier test left stored (or page 0); later visits our own page.
                const stored = tabs.activePosition
                if (i < 2)
                    remembered[tab] = stored.valid ? stored.page : 0
                compare(win.viewer.topPosition().page, remembered[tab], "tab " + tab + " lost its page (switch " + i + ")")

                const immediate = grabImage(win.contentItem)
                const fraction = Helpers.differingFraction(immediate, freshImage())
                verify(fraction < Helpers.sameThreshold, "wrong page image on tab " + tab + " (switch " + i + "): "
                       + (fraction * 100).toFixed(1) + "% of pixels differ")

                const page = 20 + (i * 47) % Math.max(1, win.viewer.pageCount - 40)
                win.viewer.jumpTo({ page: page, offsetY: 0, zoom: win.viewer.zoom })
                tryVerify(() => win.viewer.topPosition().page === page, 5000)
                wait(500)
                remembered[tab] = win.viewer.topPosition().page
                compare(remembered[tab], page)
            }
        }
    }
}
