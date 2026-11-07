// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Pdf
import SheafBackend 1.0

// One PDF document in a continuous-scroll view with zoom. Wraps Qt PDF's PdfMultiPageView.
// Single use: Viewer creates a fresh pane per opened document, because reusing a PdfDocument/PdfMultiPageView for
// another file leaves stale page images and layout behind.
Item {
    id: root

    readonly property alias document: document
    readonly property bool hasDocument: document.status === PdfDocument.Ready
    readonly property alias status: document.status
    readonly property alias error: document.error
    readonly property int pageCount: document.pageCount
    // Page at the centre of the viewport. PdfMultiPageView's own currentPage only updates when its scroll bar
    // is used, not while scrolling with the wheel, so track the visible page ourselves.
    property int currentPage: 0
    // The PdfMultiPageView, created only once the document is ready (see viewLoader).
    readonly property var view: viewLoader.item
    readonly property real zoom: view ? view.renderScale : 1

    readonly property real minZoom: 0.1
    readonly property real maxZoom: 10
    readonly property real zoomStep: 1.25
    // One mouse wheel notch zooms this many times less than a key press: the wheel is used in a stream of notches, so
    // the keyboard's step would be far too coarse. A notch multiplies the zoom by zoomStep^(1/wheelZoomDivisor).
    readonly property real wheelZoomDivisor: 5
    // Pixels scrolled per mouse wheel notch. Qt's default for flickables is far too small for page-sized content.
    property real wheelScrollPixels: 180

    // Emitted when scroll position or zoom changed after the document was shown and restored; use topPosition().
    signal viewMoved
    // Emitted just before an internal link is followed, with the position being left ({page, offsetY, zoom}).
    signal linkFollowed(var from)
    // Emitted when an external link was tapped; the app asks the user before opening it.
    signal externalLinkRequested(url url)

    // True once the requested position has been restored; changes before that are not the user's.
    property bool settled: false
    // True once the restored position is on screen with its page images rendered. Viewer keeps this pane hidden (and
    // the previous one visible) until then, so no half-rendered or wrong page is ever shown.
    property bool ready: false
    // Set by Viewer when this pane is the one on screen.
    property bool shown: false
    // Set when another document is about to replace this one: it must not report anything any more.
    property bool frozen: false
    // The text search of this document (see PdfSearchSession).
    readonly property alias search: search
    // The search of the tab to bring back when this pane has been built, {open, query, page, start}, or null.
    property var pendingSearch: null
    opacity: shown ? 1 : 0
    z: shown ? 1 : 0
    property var pendingRestore: null
    // Where a restore is heading ({page, offset}), or null; the pane is not ready before the view is there.
    property var restoreTarget: null

    // Is the view at the restore target? The view finishes jumps a little later by itself, and a target near the end of
    // the document can never be reached exactly, so being at the very bottom counts as arrived.
    function restoreReached() {
        if (!restoreTarget)
            return true
        const flick = flickable()
        const top = topPosition()
        if (top.page === restoreTarget.page && Math.abs(top.offset - restoreTarget.offset) < 0.015)
            return true
        return flick.contentY >= flick.originY + flick.contentHeight - flick.height - 1
    }

    // Loads `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    function open(url, position, searchState) {
        pendingRestore = position
        pendingSearch = searchState
        document.source = url
    }

    // Scrolls to `position` ({page, offsetY, zoom}): the page, how far (0..1) it is scrolled, and the zoom.
    function jumpTo(position) {
        if (!hasDocument || !view)
            return
        const page = Math.max(0, Math.min(position.page, pageCount - 1))
        const scale = Math.max(minZoom, Math.min(maxZoom, position.zoom))
        const pageHeight = document.pagePointSize(page).height
        // Apply the zoom and rebuild the row heights first, or the jump lands in the wrong place.
        view.renderScale = scale
        flickable().forceLayout()
        // The view keeps a 10 px margin above the requested location.
        view.goToLocation(page, Qt.point(0, position.offsetY * pageHeight + 10 / scale), scale)
    }

    // An internal link was tapped: remember where we were, then go there.
    function followLink(page, location, zoom) {
        const from = topPosition()
        linkFollowed({ page: from.page, offsetY: from.offset, zoom: root.zoom })
        // Keep the current zoom: Qt reports zoom 1 for links that do not set one, which would reset it on every jump.
        view.goToLocation(page, location, 0)
    }

    // Stops reporting position changes; used when another document is about to replace this one.
    function freeze() {
        frozen = true
        settled = false
        moveTimer.stop()
    }

    // --- Search ---

    // Searches for `query`, continuing from the current match if there is one, else from the page being read. `reveal`
    // scrolls to the first match found.
    function startSearch(query, reveal) {
        const current = search.currentPage >= 0
        search.start(query, current ? search.currentPage : topPosition().page, current ? search.currentStart : 0, reveal)
    }

    // Brings a tab's earlier search back (highlights and current match) without scrolling anywhere.
    function restoreSearch(state) {
        if (!state || !state.query)
            return
        const known = state.page >= 0
        search.start(state.query, known ? state.page : topPosition().page, known ? state.start : 0, false)
    }

    function clearSearch() {
        search.clear()
    }

    // Scrolls the current match into view: in one move to the position worked out from the page sizes, which is right
    // for most documents. If the match is not in view a few frames later (pages of different sizes can throw that
    // calculation off until the table has laid out the pages in between), the table's own positioning takes over.
    function revealSearchResult() {
        if (!view)
            return
        view.revealCurrentSearchResult()
        revealCheckTimer.ticks = 0
        revealCheckTimer.restart()
    }

    Timer {
        id: revealCheckTimer
        interval: 16
        property int ticks: 0
        onTriggered: {
            if (!root.view || root.view.searchResultInView() || ++ticks > 80)
                return
            // From the fourth frame on, ask the table every few frames (asking every frame would only restart its layout).
            if (ticks >= 4 && ticks % 5 === 0)
                root.view.revealCurrentSearchResultByCell()
            restart()
        }
    }

    PdfSearchSession {
        id: search
        source: root.document.source
        onRevealRequested: root.revealSearchResult()
    }


    function applyPending() {
        if (!hasDocument || !view)
            return
        const position = pendingRestore
        pendingRestore = null
        if (position && position.valid) {
            restoreTarget = { page: Math.max(0, Math.min(position.page, pageCount - 1)), offset: position.offsetY }
            jumpTo(position)
        } else {
            restoreTarget = null
            fitWidth()
        }
        settleTimer.restart()
        // The search of the tab (highlights and current match) comes back with the document, without scrolling.
        restoreSearch(pendingSearch)
        pendingSearch = null
        readyTimer.ticks = 0
        readyTimer.restart()
    }

    // Page at the top of the viewport and how far (0..1) it is scrolled; this is what gets saved.
    function topPosition() {
        const flick = flickable()
        if (!flick || !hasDocument)
            return { page: currentPage, offset: 0 }
        let row = flick.cellAtPos(view.width / 2, 1).y
        if (row < 0)
            row = flick.cellAtPos(view.width / 2, 8).y
        if (row < 0)
            row = currentPage
        const item = flick.itemAtCell(0, row)
        if (!item || item.height <= 0)
            return { page: row, offset: 0 }
        return { page: row, offset: Math.max(0, Math.min(1, (flick.contentY - item.y) / item.height)) }
    }

    function goToPage(page) {
        // Not view.goToPage(): it ignores requests for the page its navigator last saw, which can be stale.
        if (hasDocument && view)
            view.goToLocation(Math.max(0, Math.min(page, pageCount - 1)), Qt.point(-1, -1), 0)
    }

    function setZoom(value) {
        if (!view)
            return
        view.renderScale = Math.max(minZoom, Math.min(maxZoom, value))
    }

    function zoomIn() {
        setZoom(zoom * zoomStep)
    }

    // Zooms for a wheel turn of `angleDelta` (120 per notch; high-resolution wheels and touchpads send fractions of it,
    // so the zoom follows the amount turned).
    function zoomByWheel(angleDelta) {
        setZoom(zoom * Math.pow(zoomStep, angleDelta / 120 / wheelZoomDivisor))
    }

    function zoomOut() {
        setZoom(zoom / zoomStep)
    }

    function fitWidth() {
        if (hasDocument && width > 0)
            // Leave room for the vertical scroll bar.
            setZoom((width - 16) / document.pagePointSize(0).width)
    }

    // Keeps the table's width in step with the widest page (see readyTimer); also for later changes.
    function relayout() {
        const flick = flickable()
        if (flick)
            flick.forceLayout()
    }

    PdfDocument {
        id: document
        onMaxPageWidthChanged: root.relayout()
        onStatusChanged: function (status) {
            // Nothing to wait for if the file cannot be opened; let Viewer show the error.
            if (status === PdfDocument.Error)
                root.ready = true
        }
    }

    // PdfPageImage clamps currentFrame to 0 if it is set before its document is ready, and never re-applies it, so a
    // page created for a jump target can show page 1 while its delegate index is the target page. Re-binding
    // currentFrame to the delegate index makes the image load the right page (and keeps the binding intact).
    // Returns the page images of the visible rows.
    function fixPageImages() {
        const flick = flickable()
        const images = []
        if (!flick || !hasDocument)
            return images
        const first = Math.max(0, flick.cellAtPos(view.width / 2, 1).y)
        const last = Math.min(pageCount - 1, Math.max(first, flick.cellAtPos(view.width / 2, view.height - 1).y))
        for (let row = first; row <= last; ++row) {
            const holder = flick.itemAtCell(0, row)
            if (!holder)
                continue
            for (const paper of holder.children) {
                for (const image of paper.children) {
                    if (image.currentFrame === undefined)
                        continue
                    if (image.currentFrame !== holder.index)
                        image.currentFrame = Qt.binding(() => holder.index)
                    images.push(image)
                }
            }
        }
        return images
    }

    function onRowsAvailable() {
        const flick = flickable()
        if (flick.rows > 0) {
            flick.rowsChanged.disconnect(root.onRowsAvailable)
            applyPending()
        }
    }

    function onScrolled() {
        pageTimer.restart()
        if (settled)
            moveTimer.restart()
    }

    onZoomChanged: if (settled) moveTimer.restart()

    // Updates the current page once layout has caught up; owned by the pane, so it dies with it.
    Timer {
        id: pageTimer
        interval: 0
        onTriggered: root.updateCurrentPage()
    }

    // Announces a new position only after the view has been still for a moment. Right after a jump the table has not
    // laid out the target yet, and topPosition() would report garbage (and that garbage would get saved).
    Timer {
        id: moveTimer
        interval: 150
        onTriggered: root.viewMoved()
    }

    // After the jump, waits (briefly, bounded) until the visible pages are rendered, then marks the pane ready.
    Timer {
        id: readyTimer
        interval: 30
        repeat: true
        property int ticks: 0
        onTriggered: {
            const images = root.fixPageImages()
            // "Ready" alone is not enough: for a heavy page the status flips before any picture exists (painted size 0).
            const rendered = images.length > 0
                             && images.every(image => image.status === Image.Ready && image.paintedWidth > 0)
            // The table is as wide as the widest page, which the document reports a moment after the table was laid
            // out, and the table does not notice. Re-lay it out until it agrees, or the pages jump sideways later.
            const flick = root.flickable()
            const layoutSettled = Math.abs(flick.contentWidth - flick.columnWidthProvider(0)) < 0.5
            if (!layoutSettled)
                flick.forceLayout()
            if ((rendered && layoutSettled && root.restoreReached()) || ++ticks > 100) {
                stop()
                root.ready = true
            }
        }
    }

    Timer {
        id: settleTimer
        interval: 250
        onTriggered: root.settled = true
    }

    // The view is built only after the document is ready and told where to go before it lays out anything. If it
    // first rendered pages 1-2 and then jumped, the page images at the target would stay stale (showing page 2
    // while the view reports the right page) until scrolled away and back.
    Loader {
        id: viewLoader
        anchors.fill: parent
        // Also wait for a real size: a view created at 0x0 (app start, before the window is laid out) loses the jump.
        active: root.hasDocument && root.width > 0 && root.height > 0
        sourceComponent: SheafPdfView {
            document: root.document
            search: root.search
            onLinkTapped: (page, location, zoom) => root.followLink(page, location, zoom)
            onExternalLinkTapped: url => root.externalLinkRequested(url)
        }
        onLoaded: {
            // Connect explicitly: a Connections element would have to re-resolve the table inside the view.
            const flick = root.flickable()
            flick.contentYChanged.connect(root.onScrolled)
            // Jump only once the table has its rows. Called earlier, the view queues the jump and runs it later by
            // itself, after we may already have decided the pane is ready: the page would then move after it was shown.
            if (flick.rows > 0)
                root.applyPending()
            else
                flick.rowsChanged.connect(root.onRowsAvailable)
        }
    }

    // The scrollable item inside PdfMultiPageView (a TableView), which the view does not expose.
    function flickable() {
        if (!view)
            return null
        for (const child of view.children) {
            if (child.contentY !== undefined)
                return child
        }
        return null
    }

    function updateCurrentPage() {
        const flick = flickable()
        if (!flick || !hasDocument)
            return
        const row = flick.cellAtPos(view.width / 2, view.height / 2).y
        if (row >= 0)
            currentPage = row
    }

    function scrollBy(dy) {
        const flick = flickable()
        if (!flick)
            return
        const top = flick.originY
        const bottom = Math.max(top, top + flick.contentHeight - flick.height)
        flick.cancelFlick()
        flick.contentY = Math.max(top, Math.min(bottom, flick.contentY + dy))
    }

    // Ctrl+wheel zooms and plain wheel scrolls. This item sits above the view because the view's flickable
    // would otherwise consume every wheel event.
    Item {
        anchors.fill: parent
        z: 1
        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            acceptedDevices: PointerDevice.AllDevices
            onWheel: event => root.zoomByWheel(event.angleDelta.y)
        }
        WheelHandler {
            acceptedModifiers: Qt.NoModifier
            acceptedDevices: PointerDevice.AllDevices
            onWheel: event => {
                // Touchpads report exact pixels. Mice may report pixelDelta too (e.g. on Wayland), but it is
                // tiny, so use the notch count from angleDelta (120 units per notch) for everything else.
                const touchpad = event.device.type === PointerDevice.TouchPad && event.pixelDelta.y !== 0
                const dy = touchpad ? event.pixelDelta.y : event.angleDelta.y / 120 * root.wheelScrollPixels
                root.scrollBy(-dy)
            }
        }
    }
}
