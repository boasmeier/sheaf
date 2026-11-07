import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

// Continuous-scroll PDF view with zoom. Wraps Qt PDF's PdfMultiPageView.
Item {
    id: root

    property alias source: document.source
    readonly property alias document: document
    // Set by close(); the last document stays in memory until the next open() but is hidden.
    property bool closed: true
    readonly property bool hasDocument: !closed && document.status === PdfDocument.Ready
    readonly property int pageCount: document.pageCount
    // Page at the centre of the viewport. PdfMultiPageView's own currentPage only updates when its scroll bar
    // is used, not while scrolling with the wheel, so track the visible page ourselves.
    property int currentPage: 0
    readonly property real zoom: view.renderScale

    readonly property real minZoom: 0.1
    readonly property real maxZoom: 10
    readonly property real zoomStep: 1.25
    // Pixels scrolled per mouse wheel notch. Qt's default for flickables is far too small for page-sized content.
    property real wheelScrollPixels: 180

    // Emitted when scroll position or zoom changed after the document was shown and restored; use topPosition().
    signal viewChanged

    // True once the requested position has been restored; changes before that are not the user's.
    property bool settled: false
    property var pendingRestore: null

    // Shows `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    function open(url, position) {
        closed = false
        settled = false
        pendingRestore = position
        if (document.source == url)
            Qt.callLater(applyPending)
        else
            document.source = url
    }

    function close() {
        closed = true
        settled = false
        pendingRestore = null
    }

    function applyPending() {
        if (!hasDocument)
            return
        const position = pendingRestore
        pendingRestore = null
        if (position && position.valid) {
            const page = Math.max(0, Math.min(position.page, pageCount - 1))
            const scale = Math.max(minZoom, Math.min(maxZoom, position.zoom))
            const pageHeight = document.pagePointSize(page).height
            // The table still holds the previous document's row heights; rebuild them for this document and
            // zoom first, or the jump lands in the wrong place and the view ends up at the top.
            view.renderScale = scale
            flickable().forceLayout()
            // PdfMultiPageView keeps a 10 px margin above the requested location.
            view.goToLocation(page, Qt.point(0, position.offsetY * pageHeight + 10 / scale), scale)
        } else {
            fitWidth()
        }
        settleTimer.restart()
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
        if (hasDocument)
            view.goToLocation(Math.max(0, Math.min(page, pageCount - 1)), Qt.point(-1, -1), 0)
    }

    function setZoom(value) {
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
            if (status === PdfDocument.Ready)
                // PdfMultiPageView resets its scale when the source changes, so apply ours afterwards.
                Qt.callLater(root.applyPending)
        }
    }

    Connections {
        target: root.flickable()
        function onContentYChanged() {
            Qt.callLater(root.updateCurrentPage)
            if (root.settled)
                root.viewChanged()
        }
    }

    onZoomChanged: if (settled) viewChanged()

    Timer {
        id: settleTimer
        interval: 250
        onTriggered: root.settled = true
    }

    PdfMultiPageView {
        id: view
        anchors.fill: parent
        document: document
        visible: root.hasDocument
    }

    // The scrollable item inside PdfMultiPageView (a TableView), which the view does not expose.
    function flickable() {
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

    Label {
        anchors.centerIn: parent
        visible: !root.hasDocument
        color: palette.placeholderText
        text: document.status === PdfDocument.Error ? qsTr("Cannot open this PDF: %1").arg(document.error)
            : document.status === PdfDocument.Loading ? qsTr("Loading…")
            : qsTr("Open a PDF with Ctrl+O")
    }
}
