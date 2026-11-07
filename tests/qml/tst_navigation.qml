// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest

// Link navigation in the real window with the real tab controller: internal links, a back/forward history per tab,
// returning to the exact starting position, mouse and keyboard shortcuts, and asking before opening external links.
// The fixture "linked" has an A4 page per link target; page i links to page (i + 11) % 60 and to an external URL.
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "Navigation"
        when: windowShown

        // Link rectangles of the fixture in PDF points, origin bottom-left (see Fixtures.h).
        readonly property var internalRect: [50, 700, 300, 780]
        readonly property var externalRect: [50, 580, 300, 660]
        readonly property int pageHeight: 842

        function initTestCase() {
            const component = Qt.createComponent(Qt.resolvedUrl("../../qml/Main.qml"))
            compare(component.status, Component.Ready, component.errorString())
            win = component.createObject(null)
            win.width = 1000
            win.height = 700
            win.show()
            // Keyboard shortcuts only work in the active window.
            win.requestActivate()
            tryVerify(() => win.active, 5000)
        }

        function cleanupTestCase() {
            win.destroy()
        }

        Component {
            id: spyComponent
            SignalSpy { signalName: "switched" }
        }

        function init() {
            for (const group of tabs.groups())
                tabs.deleteGroup(group.id)
            tryVerify(() => !win.viewer.hasDocument, 3000)
        }

        function openLinked(page, scrollPixels, zoom) {
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.openFile(fixtures.linked)
            spy.wait(10000)
            spy.destroy()
            win.viewer.jumpTo({ page: page, offsetY: 0, zoom: zoom > 0 ? zoom : win.viewer.zoom })
            tryCompare(win.viewer, "currentPage", page, 3000)
            if (scrollPixels > 0)
                win.viewer.pane.scrollBy(scrollPixels)
            wait(400)
        }

        // Where to click, in window coordinates, to hit a link of the page that fills the middle of the view.
        function linkPoint(rect) {
            const flick = win.viewer.pane.flickable()
            const holder = flick.itemAtCell(0, win.viewer.currentPage)
            verify(holder, "page item not loaded")
            const paper = holder.children.find(child => child.pagePointSize !== undefined)
            const scale = win.viewer.zoom
            const x = (rect[0] + rect[2]) / 2 * scale
            const y = (pageHeight - (rect[1] + rect[3]) / 2) * scale
            return paper.mapToItem(win.viewer, x, y)
        }

        function clickInternalLink() {
            const point = linkPoint(internalRect)
            mouseClick(win.viewer, point.x, point.y)
        }

        // Exactly where the view is: the scroll offset in pixels, the page at the top and the zoom.
        function snapshot() {
            const top = win.viewer.topPosition()
            return { contentY: win.viewer.pane.flickable().contentY, page: top.page, offset: top.offset, zoom: win.viewer.zoom }
        }

        function verifySame(actual, expected, label) {
            compare(actual.page, expected.page, label + ": page")
            // A couple of pixels: rows are laid out at fractional heights and rounded.
            fuzzyCompare(actual.contentY, expected.contentY, 2.5, label + ": scroll position in pixels")
            fuzzyCompare(actual.zoom, expected.zoom, 0.0001, label + ": zoom")
        }

        function test_following_links_and_going_back_returns_to_the_exact_start() {
            openLinked(20, 70, 1.3)
            const start = snapshot()
            const jump = fixtures.linkJump
            const trail = [start]
            verify(!tabs.canGoBack)

            for (let i = 1; i <= 3; ++i) {
                clickInternalLink()
                tryCompare(win.viewer, "currentPage", 20 + i * jump, 3000)
                wait(300)
                verify(tabs.canGoBack, "no history after following link " + i)
                trail.push(snapshot())
            }

            for (let i = 2; i >= 0; --i) {
                win.navigateBack()
                wait(300)
                verifySame(snapshot(), trail[i], "back to step " + i)
            }
            verifySame(snapshot(), start, "after three backs")
            verify(!tabs.canGoBack)
            verify(tabs.canGoForward)
        }

        function test_forward_replays_the_links() {
            openLinked(5, 40, 0)
            const trail = [snapshot()]
            for (let i = 1; i <= 3; ++i) {
                clickInternalLink()
                tryCompare(win.viewer, "currentPage", 5 + i * fixtures.linkJump, 3000)
                wait(300)
                trail.push(snapshot())
            }
            for (let i = 0; i < 3; ++i) {
                win.navigateBack()
                wait(400)
            }
            verifySame(snapshot(), trail[0], "back at the start")
            for (let i = 1; i <= 3; ++i) {
                win.navigateForward()
                wait(400)
                verifySame(snapshot(), trail[i], "forward to step " + i)
            }
            verify(!tabs.canGoForward)
        }

        function test_a_new_link_after_going_back_discards_forward_history() {
            openLinked(5, 40, 0)
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 5 + fixtures.linkJump, 3000)
            wait(300)
            win.navigateBack()
            wait(400)
            verify(tabs.canGoForward)
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 5 + fixtures.linkJump, 3000)
            verify(!tabs.canGoForward)
            verify(tabs.canGoBack)
        }

        function test_history_survives_switching_tabs_and_stays_per_tab() {
            openLinked(10, 30, 0)
            const start = snapshot()
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 10 + fixtures.linkJump, 3000)
            wait(300)
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 10 + 2 * fixtures.linkJump, 3000)
            wait(300)
            verify(tabs.canGoBack)

            // Another tab: its own, empty history; the panes are rebuilt on every switch.
            let spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.openFile(fixtures.alpha)
            spy.wait(10000)
            spy.destroy()
            verify(!tabs.canGoBack, "a new tab must not inherit another tab's history")
            win.navigateBack()  // nothing to go back to
            wait(200)

            spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.activate(tabs.tabIds()[0])
            spy.wait(10000)
            spy.destroy()
            verify(tabs.canGoBack, "history lost when switching tabs")
            wait(300)
            win.navigateBack()
            wait(500)
            compare(win.viewer.currentPage, 10 + fixtures.linkJump)
            win.navigateBack()
            wait(500)
            verifySame(snapshot(), start, "start position after switching tabs and going back twice")
            win.navigateForward()
            win.navigateForward()
            wait(500)
            compare(win.viewer.currentPage, 10 + 2 * fixtures.linkJump)
        }

        function test_closing_a_tab_discards_its_history() {
            openLinked(10, 0, 0)
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 10 + fixtures.linkJump, 3000)
            verify(tabs.canGoBack)
            tabs.closeTab(tabs.tabIds()[0])
            tryVerify(() => !win.viewer.hasDocument, 3000)
            verify(!tabs.canGoBack)
        }

        function test_mouse_buttons_and_keys_navigate() {
            openLinked(15, 50, 0)
            const start = snapshot()
            clickInternalLink()
            tryCompare(win.viewer, "currentPage", 15 + fixtures.linkJump, 3000)
            wait(300)
            const target = snapshot()

            mouseClick(win.viewer, 10, 10, Qt.BackButton)
            wait(500)
            verifySame(snapshot(), start, "mouse Back button")
            mouseClick(win.viewer, 10, 10, Qt.ForwardButton)
            wait(500)
            verifySame(snapshot(), target, "mouse Forward button")

            keys.keyClick(win, Qt.Key_Left, Qt.AltModifier)
            wait(500)
            verifySame(snapshot(), start, "Alt+Left")
            keys.keyClick(win, Qt.Key_Right, Qt.AltModifier)
            wait(500)
            verifySame(snapshot(), target, "Alt+Right")
        }

        function test_external_links_ask_before_opening() {
            openLinked(15, 0, 0)
            verify(!win.externalLinkDialog.visible)
            const point = linkPoint(externalRect)
            mouseClick(win.viewer, point.x, point.y)
            tryVerify(() => win.externalLinkDialog.visible, 3000)
            compare(win.pendingExternalUrl.toString(), fixtures.externalUrl)
            // Declining must not navigate or add history.
            win.externalLinkDialog.reject()
            tryVerify(() => !win.externalLinkDialog.visible, 3000)
            verify(!tabs.canGoBack)
            compare(win.viewer.currentPage, 15)
        }

        function test_only_web_and_mail_links_are_offered() {
            win.askToOpenExternally("file:///etc/passwd")
            win.askToOpenExternally("javascript:alert(1)")
            win.askToOpenExternally("ssh://host/")
            wait(100)
            verify(!win.externalLinkDialog.visible)
            win.askToOpenExternally("mailto:someone@example.com")
            tryVerify(() => win.externalLinkDialog.visible, 2000)
            win.externalLinkDialog.reject()
        }
    }
}
