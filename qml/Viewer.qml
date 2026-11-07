// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

// Shows one PDF at a time. Every open() builds a brand-new PdfPane (and so a new PdfDocument and view) in the
// background, restores the position, waits until the visible pages are rendered and only then swaps it in. Until
// then the previous pane stays on screen, so there is never a wrong or half-rendered page. The old pane is destroyed
// afterwards, which also releases its document's memory.
Item {
    id: root

    // A zoomed page is far wider than the viewer; Flickable does not clip its content by default, so without this the
    // page would be drawn over the sidebar next to it.
    clip: true

    // Two slots: one shows a pane, the other loads the next one.
    property bool firstSlotShown: true
    readonly property Loader shownLoader: firstSlotShown ? slotA : slotB
    readonly property Loader loadingLoader: firstSlotShown ? slotB : slotA

    readonly property PdfPane pane: shownLoader.item as PdfPane
    readonly property bool hasDocument: pane !== null && pane.hasDocument
    readonly property int pageCount: pane ? pane.pageCount : 0
    readonly property int currentPage: pane ? pane.currentPage : 0
    readonly property real zoom: pane ? pane.zoom : 1

    // Emitted when scroll position or zoom changed after the document was shown and restored; use topPosition().
    signal viewMoved
    // Emitted when a newly opened document replaced the previous one on screen.
    signal switched
    // An internal link is about to be followed from `from` ({page, offsetY, zoom}); record it for Back.
    signal linkFollowed(var from)
    // An external link was tapped; ask the user before opening it.
    signal externalLinkRequested(url url)
    // The mouse's Back / Forward buttons were pressed.
    signal backRequested
    signal forwardRequested

    property url pendingUrl
    property var pendingPosition: null
    property var pendingSearch: null

    // --- Search ---

    // Is the search bar open, and what is in its field. Kept per tab by Main; set again when a tab is shown.
    property bool searchOpen: false
    property string searchQuery: ""
    readonly property int searchCurrentPage: pane ? pane.search.currentPage : -1
    readonly property int searchCurrentStart: pane ? pane.search.currentStart : -1
    readonly property alias searchBar: searchBar
    // The search of the active tab changed (opened, closed, another query, another current match): remember it.
    signal searchStateChanged

    // The search as it is right now, read from the session itself. Not from searchCurrentPage / searchCurrentStart:
    // those are bindings that may not have caught up yet when a signal of the session is being handled.
    function searchState() {
        return {
            open: searchOpen,
            query: searchQuery,
            page: pane ? pane.search.currentPage : -1,
            start: pane ? pane.search.currentStart : 0
        }
    }

    function reportSearchState() {
        if (pane && !pane.frozen)
            searchStateChanged()
    }

    function openSearch() {
        if (!pane)
            return
        searchOpen = true
        // Highlights come back with the bar if a query is still in the field; the document does not scroll for it.
        if (searchQuery.trim() !== "" && !pane.search.searching && !pane.search.complete)
            pane.startSearch(searchQuery, false)
        Qt.callLater(searchBar.focusField)
        reportSearchState()
    }

    function closeSearch() {
        searchDebounce.stop()
        searchOpen = false
        if (pane)
            pane.clearSearch()
        reportSearchState()
        root.forceActiveFocus()
    }

    // Runs the search for what is in the field (immediately; typing goes through searchDebounce).
    function runSearch(reveal) {
        if (!pane)
            return
        if (searchQuery.trim() === "")
            pane.clearSearch()
        else
            pane.startSearch(searchQuery, reveal)
        reportSearchState()
    }

    // Next (or previous) match. With the bar closed, opens it and shows the matches of the last query.
    function searchStep(forward) {
        if (!pane)
            return
        if (!searchOpen) {
            openSearch()
            if (searchQuery.trim() !== "")
                runSearch(true)
            return
        }
        if (searchDebounce.running) {  // typed and pressed Enter at once: search now
            searchDebounce.stop()
            runSearch(true)
        } else if (forward) {
            pane.search.next()
        } else {
            pane.search.previous()
        }
    }

    // A search typed in the field starts about 150 ms after the last keystroke, not with every key.
    Timer {
        id: searchDebounce
        interval: 150
        onTriggered: root.runSearch(true)
    }

    Connections {
        target: root.pane ? root.pane.search : null
        function onCurrentChanged() { root.reportSearchState() }
    }

    // Shows `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    // `searchState` is the tab's search ({open, query, page, start}) to bring back with the document.
    function open(url, position, searchState) {
        // The outgoing pane must not report positions that would be saved for the incoming document's tab.
        if (pane)
            pane.freeze()
        pendingUrl = url
        pendingPosition = position
        searchDebounce.stop()
        const state = searchState || { open: false, query: "", page: -1, start: 0 }
        searchOpen = state.open
        searchQuery = state.query
        pendingSearch = state.open && state.query !== "" ? state : null
        loadingLoader.sourceComponent = null
        loadingLoader.sourceComponent = paneComponent
    }

    function close() {
        searchDebounce.stop()
        searchOpen = false
        searchQuery = ""
        if (pane)
            pane.freeze()
        slotA.sourceComponent = null
        slotB.sourceComponent = null
    }

    function topPosition() {
        return pane ? pane.topPosition() : { page: 0, offset: 0 }
    }

    function jumpTo(position) {
        if (pane)
            pane.jumpTo(position)
    }

    function goToPage(page) {
        if (pane)
            pane.goToPage(page)
    }

    function zoomIn() {
        if (pane)
            pane.zoomIn()
    }

    function zoomOut() {
        if (pane)
            pane.zoomOut()
    }

    function fitWidth() {
        if (pane)
            pane.fitWidth()
    }

    function showLoadedPane() {
        const previous = shownLoader
        firstSlotShown = !firstSlotShown
        pane.shown = true
        previous.sourceComponent = null
        switched()
    }

    Component {
        id: paneComponent
        PdfPane {
            id: candidate
            onViewMoved: root.viewMoved()
            onLinkFollowed: from => root.linkFollowed(from)
            onExternalLinkRequested: url => root.externalLinkRequested(url)
            onReadyChanged: {
                if (ready && root.loadingLoader.item === candidate)
                    root.showLoadedPane()
            }
        }
    }

    Loader {
        id: slotA
        anchors.fill: parent
        onLoaded: item.open(root.pendingUrl, root.pendingPosition, root.pendingSearch)
    }

    Loader {
        id: slotB
        anchors.fill: parent
        onLoaded: item.open(root.pendingUrl, root.pendingPosition, root.pendingSearch)
    }

    // Clicking the document gives it the keyboard focus (so that Esc closes the search bar from there).
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.forceActiveFocus()
    }

    Keys.onEscapePressed: event => {
        if (root.searchOpen)
            root.closeSearch()
        else
            event.accepted = false
    }

    SearchBar {
        id: searchBar
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        width: Math.min(400, parent.width - 24)
        visible: root.searchOpen && root.pane !== null
        session: root.pane ? root.pane.search : null
        query: root.searchQuery
        onQueryEdited: text => {
            root.searchQuery = text
            searchDebounce.restart()
            root.reportSearchState()
        }
        onNextRequested: root.searchStep(true)
        onPreviousRequested: root.searchStep(false)
        onCloseRequested: root.closeSearch()
    }

    TapHandler {
        acceptedButtons: Qt.BackButton
        onTapped: root.backRequested()
    }

    TapHandler {
        acceptedButtons: Qt.ForwardButton
        onTapped: root.forwardRequested()
    }

    Label {
        anchors.centerIn: parent
        visible: !root.hasDocument
        color: palette.placeholderText
        text: root.pane && root.pane.status === PdfDocument.Error ? qsTr("Cannot open this PDF: %1").arg(root.pane.error)
            : root.loadingLoader.item !== null ? qsTr("Loading…")
            : qsTr("Open a PDF with Ctrl+O")
    }
}
