import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

// Shows one PDF at a time. Every open() builds a brand-new PdfPane (and so a new PdfDocument and view) and drops the
// previous one, which also releases that document's memory.
Item {
    id: root

    readonly property PdfPane pane: loader.item as PdfPane
    readonly property bool hasDocument: pane !== null && pane.hasDocument
    readonly property int pageCount: pane ? pane.pageCount : 0
    readonly property int currentPage: pane ? pane.currentPage : 0
    readonly property real zoom: pane ? pane.zoom : 1

    // Emitted when scroll position or zoom changed after the document was shown and restored; use topPosition().
    signal viewMoved

    property url pendingUrl
    property var pendingPosition: null

    // Shows `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    function open(url, position) {
        pendingUrl = url
        pendingPosition = position
        loader.sourceComponent = null
        loader.sourceComponent = paneComponent
    }

    function close() {
        loader.sourceComponent = null
    }

    function topPosition() {
        return pane ? pane.topPosition() : { page: 0, offset: 0 }
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

    Component {
        id: paneComponent
        PdfPane {
            onViewMoved: root.viewMoved()
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
        onLoaded: item.open(root.pendingUrl, root.pendingPosition)
    }

    Label {
        anchors.centerIn: parent
        visible: !root.hasDocument
        color: palette.placeholderText
        text: root.pane && root.pane.status === PdfDocument.Error ? qsTr("Cannot open this PDF: %1").arg(root.pane.error)
            : root.pane && root.pane.status === PdfDocument.Loading ? qsTr("Loading…")
            : qsTr("Open a PDF with Ctrl+O")
    }
}
