import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

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
    // Pixels scrolled per mouse wheel notch. Qt's default for flickables is far too small for page-sized content.
    property real wheelScrollPixels: 180

    // Emitted when scroll position or zoom changed after the document was shown and restored; use topPosition().
    signal viewMoved

    // True once the requested position has been restored; changes before that are not the user's.
    property bool settled: false
    // True once the restored position is on screen with its page images rendered. Viewer keeps this pane hidden (and
    // the previous one visible) until then, so no half-rendered or wrong page is ever shown.
    property bool ready: false
    // Set by Viewer when this pane is the one on screen.
    property bool shown: false
    opacity: shown ? 1 : 0
    z: shown ? 1 : 0
    property var pendingRestore: null

    // Loads `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    function open(url, position) {
        pendingRestore = position
        document.source = url
    }

    // Stops reporting position changes; used when another document is about to replace this one.
    function freeze() {
        settled = false
        moveTimer.stop()
    }

    function applyPending() {
        if (!hasDocument || !view)
            return
        const position = pendingRestore
        pendingRestore = null
        if (position && position.valid) {
            const page = Math.max(0, Math.min(position.page, pageCount - 1))
            const scale = Math.max(minZoom, Math.min(maxZoom, position.zoom))
            const pageHeight = document.pagePointSize(page).height
            // Apply the zoom and rebuild the row heights first, or the jump lands in the wrong place.
            view.renderScale = scale
            flickable().forceLayout()
            // PdfMultiPageView keeps a 10 px margin above the requested location.
            view.goToLocation(page, Qt.point(0, position.offsetY * pageHeight + 10 / scale), scale)
        } else {
            fitWidth()
        }
        settleTimer.restart()
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

    function zoomOut() {
        setZoom(zoom / zoomStep)
    }

    function fitWidth() {
        if (hasDocument && width > 0)
            // Leave room for the vertical scroll bar.
            setZoom((width - 16) / document.pagePointSize(0).width)
    }

    PdfDocument {
        id: document
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
            const rendered = images.length > 0 && images.every(image => image.status === Image.Ready)
            if (rendered || ++ticks > 100) {
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
        sourceComponent: PdfMultiPageView {
            document: root.document
        }
        onLoaded: {
            // Connect explicitly: a Connections element would have to re-resolve the table inside the view.
            root.flickable().contentYChanged.connect(root.onScrolled)
            root.applyPending()
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
            onWheel: event => {
                if (event.angleDelta.y > 0)
                    root.zoomIn()
                else if (event.angleDelta.y < 0)
                    root.zoomOut()
            }
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
