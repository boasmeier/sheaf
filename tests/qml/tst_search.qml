// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest

// Searching in the real window: Ctrl+F, typing, the counter, Enter / F3 / Shift, Esc, the highlights as drawn, the
// state kept per tab, scanned documents without text, and the speed targets of the search.
// Fixtures: "text" has 320 pages with "common" three times per page, "needle" on pages 3, 13, 23 ... and as NEEDLE on
// page 5, and "red fox" over a line break on page 7. "scanned" has only pictures. "beta" has pages of mixed sizes.
Item {
    width: 400
    height: 300

    property var win: null

    TestCase {
        name: "Search"
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

        // ---- helpers ----

        function press(key, modifiers) {
            keys.keyClick(win, key, modifiers || Qt.NoModifier)
            wait(60)
        }

        function openDocument(url, expectedPages) {
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.openFile(url)
            spy.wait(15000)
            spy.destroy()
            tryVerify(() => win.viewer.hasDocument && win.viewer.pageCount === expectedPages, 10000)
            wait(350)  // moves and searches are only reported once the pane has settled
        }

        Component {
            id: spyComponent
            SignalSpy { signalName: "switched" }
        }

        readonly property int textPages: 320

        // Opens the text document at its top: a search starts from where you are reading, and an earlier test may have
        // left its saved position somewhere else.
        function openText() {
            openDocument(fixtures.text, textPages)
            win.viewer.jumpTo({ page: 0, offsetY: 0, zoom: win.viewer.zoom })
            tryVerify(() => { const top = win.viewer.topPosition(); return top.page === 0 && top.offset < 0.01 }, 3000)
            wait(200)
        }

        function search() {
            return win.viewer.pane.search
        }

        function counter() {
            return win.viewer.searchBar.status
        }

        // Types a query into the search bar with the real keys.
        function typeQuery(text) {
            press(Qt.Key_F, Qt.ControlModifier)
            tryVerify(() => win.viewer.searchBar.field.activeFocus, 2000)
            keys.keyClicks(win, text)
        }

        // A search continues from the current match, so after another query the first match shown is not necessarily the
        // first of the document: wait for the total only ("... of 960").
        function waitForTotal(total, timeout) {
            tryVerify(() => counter().endsWith(" of " + total), timeout || 4000,
                      "counter is '" + counter() + "', expected '... of " + total + "'")
        }

        function waitForCounter(expected, timeout) {
            tryVerify(() => counter() === expected, timeout || 4000, "counter is '" + counter() + "', expected '" + expected + "'")
        }

        // Pixels of the colours the highlights are drawn in: a bluish tint on the white page, and the cyan frame
        // round the current match.
        // The page already has a few tinted pixels of its own (anti-aliased text, borders): measure that once and compare
        // against it.
        property int baselineTinted: 0
        property int baselineFrame: 0

        function noteBaseline() {
            const base = highlightPixels()
            baselineTinted = base.tinted
            baselineFrame = base.frame
        }

        function highlightPixels() {
            // grabImage() returns the whole window, so look only at the part where the document is shown (the toolbar
            // and the sidebar have bluish widgets of their own).
            const image = grabImage(win.contentItem)
            const origin = win.viewer.mapToItem(null, 0, 0)
            const left = Math.max(0, Math.floor(origin.x))
            const top = Math.max(0, Math.floor(origin.y))
            const right = Math.min(image.width, Math.floor(origin.x + win.viewer.width))
            const bottom = Math.min(image.height, Math.floor(origin.y + win.viewer.height))
            // The search bar floats over the document and has its own bluish bits; leave it out.
            const bar = win.viewer.searchBar
            const barOrigin = bar.mapToItem(null, 0, 0)
            const barLeft = barOrigin.x - 4  // its place is the same whether it is shown or not
            const barRight = barOrigin.x + bar.width + 4
            const barBottom = barOrigin.y + bar.height + 4
            let tinted = 0
            let frame = 0
            for (let y = top; y < bottom; y += 2) {
                for (let x = left; x < right; x += 2) {
                    if (x >= barLeft && x <= barRight && y <= barBottom)
                        continue
                    const r = image.red(x, y), g = image.green(x, y), b = image.blue(x, y)
                    if (r > 190 && b - r > 15 && b - r < 45 && Math.abs(g - r) < 25)
                        ++tinted
                    else if (r < 90 && g > 190 && b > 190)
                        ++frame
                }
            }
            return { tinted: tinted, frame: frame }
        }

        // ---- the bar and the shortcuts ----

        function test_ctrl_f_opens_the_bar_and_focuses_the_field() {
            openText()
            verify(!win.viewer.searchOpen)
            verify(!win.viewer.searchBar.visible)
            press(Qt.Key_F, Qt.ControlModifier)
            tryVerify(() => win.viewer.searchOpen && win.viewer.searchBar.visible, 2000)
            tryVerify(() => win.viewer.searchBar.field.activeFocus, 2000)
        }

        function test_typing_finds_the_matches_and_shows_the_counter() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            compare(search().count, fixtures.needleCount)
            compare(search().currentNumber, 1)
        }

        function test_the_first_match_is_scrolled_to_and_highlighted() {
            openText()
            noteBaseline()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            tryVerify(() => win.viewer.pane.view.searchResultInView(), 2000)
            compare(search().currentPage, fixtures.firstNeedlePage)  // the match on page 3 is the one shown, and in view
            wait(300)
            const pixels = highlightPixels()
            verify(pixels.tinted > baselineTinted + 20, "the match is not highlighted (" + pixels.tinted + " tinted pixels)")
            verify(pixels.frame > baselineFrame + 10, "the current match has no frame (" + pixels.frame + " frame pixels)")
        }

        function test_all_matches_are_highlighted_not_only_the_current_one() {
            openText()
            noteBaseline()
            typeQuery("needle")  // one match on the page in view
            waitForCounter("1 of " + fixtures.needleCount)
            wait(300)
            const one = highlightPixels().tinted - baselineTinted
            typeQuery("common")  // three matches on every page
            waitForTotal(fixtures.commonCount)
            wait(300)
            const many = highlightPixels().tinted - baselineTinted
            verify(many > 40, "only " + many + " highlighted pixels")
            verify(many > 1.5 * one, "all matches in view must be highlighted, not just the current one (" + many + " vs " + one + ")")
        }

        function test_enter_and_f3_go_to_the_next_match_and_shift_goes_back() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Return)
            waitForCounter("2 of " + fixtures.needleCount)
            press(Qt.Key_Return)
            waitForCounter("3 of " + fixtures.needleCount)
            press(Qt.Key_Return, Qt.ShiftModifier)
            waitForCounter("2 of " + fixtures.needleCount)
            press(Qt.Key_F3)
            waitForCounter("3 of " + fixtures.needleCount)
            press(Qt.Key_F3, Qt.ShiftModifier)
            waitForCounter("2 of " + fixtures.needleCount)
            // the keys work with the focus in the document too
            win.viewer.forceActiveFocus()
            press(Qt.Key_F3)
            waitForCounter("3 of " + fixtures.needleCount)
        }

        function test_stepping_wraps_round_at_both_ends() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Return, Qt.ShiftModifier)  // before the first: the last
            waitForCounter(fixtures.needleCount + " of " + fixtures.needleCount)
            press(Qt.Key_Return)  // after the last: the first
            waitForCounter("1 of " + fixtures.needleCount)
        }

        function test_stepping_scrolls_to_each_match() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            for (let i = 0; i < 6; ++i) {
                press(Qt.Key_Return)
                tryVerify(() => win.viewer.pane.view.searchResultInView(), 2000, "match " + (i + 2) + " is not in view")
                const page = search().currentPage
                verify(Math.abs(win.viewer.topPosition().page - page) <= 1, "view is at page " + win.viewer.topPosition().page + ", match is on " + page)
            }
        }

        function test_a_phrase_over_a_line_break_is_found_and_shown() {
            openText()
            typeQuery("red fox")
            waitForCounter("1 of 1")
            compare(search().currentPage, fixtures.redFoxPage)
        }

        function test_escape_closes_the_bar_and_clears_the_highlights() {
            openText()
            noteBaseline()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            wait(300)
            const shown = highlightPixels().tinted
            verify(shown > baselineTinted + 20)
            press(Qt.Key_Escape)
            tryVerify(() => !win.viewer.searchOpen, 2000)
            verify(!win.viewer.searchBar.visible)
            compare(search().count, 0)
            wait(300)
            const afterEsc = highlightPixels()
            verify(afterEsc.tinted < shown * 0.25 + 10, "highlights are still drawn")
            verify(highlightPixels().frame <= baselineFrame + 3, "the current-match frame is still drawn")
        }

        function test_escape_works_from_the_document_too() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            win.viewer.forceActiveFocus()
            press(Qt.Key_Escape)
            tryVerify(() => !win.viewer.searchOpen, 2000)
        }

        function test_reopening_selects_the_old_query_and_brings_the_highlights_back() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Escape)
            tryVerify(() => !win.viewer.searchOpen, 2000)
            const position = win.viewer.topPosition()

            press(Qt.Key_F, Qt.ControlModifier)
            tryVerify(() => win.viewer.searchOpen, 2000)
            tryVerify(() => win.viewer.searchBar.field.activeFocus, 2000)
            compare(win.viewer.searchBar.field.text, "needle")
            compare(win.viewer.searchBar.field.selectedText, "needle")  // typing replaces it
            waitForCounter("1 of " + fixtures.needleCount)
            compare(win.viewer.topPosition().page, position.page)  // reopening does not scroll
            keys.keyClicks(win, "common")  // replaces the selected query
            waitForTotal(fixtures.commonCount)
        }

        function test_f3_without_a_bar_opens_it_and_shows_the_last_query() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Escape)
            tryVerify(() => !win.viewer.searchOpen, 2000)
            press(Qt.Key_F3)
            tryVerify(() => win.viewer.searchOpen, 2000)
            waitForCounter("1 of " + fixtures.needleCount)
        }

        // ---- messages ----

        function test_no_matches_says_so() {
            openText()
            typeQuery("zzyzxqq")
            waitForCounter("No matches")
        }

        function test_a_scanned_document_without_text_says_so() {
            openDocument(fixtures.scanned, 12)
            typeQuery("anything")
            waitForCounter("No text in this document")
        }

        function test_a_document_with_text_is_not_mistaken_for_a_scan() {
            openText()
            typeQuery("zzyzxqq")
            waitForCounter("No matches")
            verify(counter() !== "No text in this document")
        }

        function test_an_empty_query_clears_everything() {
            openText()
            noteBaseline()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            wait(300)
            verify(highlightPixels().frame > 5, "the current match is framed while searching")
            press(Qt.Key_A, Qt.ControlModifier)
            press(Qt.Key_Backspace)
            tryVerify(() => search().count === 0, 2000)
            compare(counter(), "")
            wait(300)
            verify(highlightPixels().frame <= 3, "the frame of the current match is still drawn")
        }

        // ---- typing ----

        function test_typing_quickly_starts_one_search_not_one_per_key() {
            openText()
            press(Qt.Key_F, Qt.ControlModifier)
            tryVerify(() => win.viewer.searchBar.field.activeFocus, 2000)
            const resets = createTemporaryObject(resetSpyComponent, win, { target: search() })
            keys.keyClicks(win, "needle")  // six keys within a few milliseconds
            waitForCounter("1 of " + fixtures.needleCount)
            wait(400)
            compare(resets.count, 1, "each keystroke started its own search")
            resets.destroy()
        }

        Component {
            id: resetSpyComponent
            SignalSpy { signalName: "highlightsReset" }
        }

        function test_the_query_can_be_refined_while_typing() {
            openText()
            press(Qt.Key_F, Qt.ControlModifier)
            tryVerify(() => win.viewer.searchBar.field.activeFocus, 2000)
            keys.keyClicks(win, "nee")
            tryVerify(() => search().complete && search().count === fixtures.needleCount, 4000)  // "nee" only occurs in "needle"
            keys.keyClicks(win, "dle")
            waitForCounter("1 of " + fixtures.needleCount)
        }

        // ---- per tab ----

        function test_each_tab_keeps_its_own_query_and_current_match() {
            // Tab A: the text document, on the 3rd match of "needle".
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Return)
            press(Qt.Key_Return)
            waitForCounter("3 of " + fixtures.needleCount)
            tryVerify(() => win.viewer.pane.view.searchResultInView(), 2000)
            wait(700)  // let the position be reported
            const where = win.viewer.topPosition()
            const matchPage = search().currentPage
            const idA = tabs.activeTabId
            const framedBefore = highlightPixels().frame
            verify(framedBefore > 5, "the current match is framed before switching away")

            // Tab B: another document, searched for something else.
            openDocument(fixtures.alpha, 150)
            const idB = tabs.activeTabId
            verify(idA !== idB)
            verify(!win.viewer.searchOpen, "a new tab starts without a search")
            typeQuery("page 7")
            tryVerify(() => counter() !== "" && search().complete, 4000)
            const counterB = counter()
            verify(counterB.indexOf(" of ") > 0, "tab B counter: " + counterB)
            wait(300)

            // Back to A: bar open, same query, same current match, and the document where we left it.
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.activate(idA)
            spy.wait(15000)
            spy.destroy()
            tryVerify(() => win.viewer.searchOpen, 3000)
            compare(win.viewer.searchBar.field.text, "needle")
            waitForCounter("3 of " + fixtures.needleCount, 5000)
            compare(search().currentPage, matchPage)
            compare(win.viewer.topPosition().page, where.page)  // restoring the search did not scroll away
            fuzzyCompare(win.viewer.topPosition().offset, where.offset, 0.02)
            wait(300)
            verify(highlightPixels().frame > framedBefore * 0.7, "the current match is framed again")

            // And B has its own.
            const spy2 = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.activate(idB)
            spy2.wait(15000)
            spy2.destroy()
            tryVerify(() => win.viewer.searchOpen, 3000)
            compare(win.viewer.searchBar.field.text, "page 7")
            tryVerify(() => counter() === counterB, 5000, "tab B counter is '" + counter() + "', expected '" + counterB + "'")
        }

        function test_a_closed_bar_stays_closed_in_its_tab() {
            openText()
            typeQuery("needle")
            waitForCounter("1 of " + fixtures.needleCount)
            press(Qt.Key_Escape)
            const idA = tabs.activeTabId
            openDocument(fixtures.alpha, 150)
            const spy = createTemporaryObject(spyComponent, win, { target: win.viewer })
            tabs.activate(idA)
            spy.wait(15000)
            spy.destroy()
            wait(300)
            verify(!win.viewer.searchOpen)
            compare(search().count, 0)
        }

        // ---- speed and responsiveness ----

        function test_the_first_match_is_in_view_within_200_ms_and_the_full_count_within_2_s() {
            openText()
            win.viewer.openSearch()
            win.viewer.searchQuery = "common"
            const timer = Date.now()
            win.viewer.runSearch(true)
            let firstMatch = -1
            let complete = -1
            for (let i = 0; i < 2000 && complete < 0; ++i) {
                wait(2)
                const now = Date.now() - timer
                if (firstMatch < 0 && search().currentPage >= 0 && win.viewer.pane.view.searchResultInView())
                    firstMatch = now
                if (search().complete)
                    complete = now
            }
            console.log("first match in view after", firstMatch, "ms; complete after", complete, "ms;", search().count, "matches")
            verify(firstMatch >= 0 && firstMatch < 200, "first match took " + firstMatch + " ms")
            verify(complete >= 0 && complete < 2000, "the full count took " + complete + " ms")
        }

        function test_scrolling_stays_smooth_while_a_search_runs() {
            openText()
            win.viewer.openSearch()
            win.viewer.searchQuery = "common"
            let longestGap = 0
            let last = Date.now()
            win.viewer.runSearch(false)
            const flick = win.viewer.pane.flickable()
            for (let i = 0; i < 60 && !search().complete; ++i) {
                flick.contentY += 120  // keep scrolling like a user would
                wait(5)
                const now = Date.now()
                longestGap = Math.max(longestGap, now - last)
                last = now
            }
            console.log("longest gap while scrolling during a search:", longestGap, "ms")
            verify(longestGap < 100, "the interface froze for " + longestGap + " ms")
        }

        function test_a_search_in_a_document_with_pages_of_different_sizes_shows_the_match() {
            openDocument(fixtures.beta, 180)
            typeQuery("page 170")
            tryVerify(() => search().count >= 1, 4000)
            tryVerify(() => win.viewer.pane.view.searchResultInView(), 3000, "the match is not in view")
            compare(search().currentPage, 169)
        }
    }
}
