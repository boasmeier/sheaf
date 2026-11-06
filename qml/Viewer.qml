import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

// Continuous-scroll PDF view with zoom. Wraps Qt PDF's PdfMultiPageView.
Item {
    id: root

    property alias source: document.source
    readonly property alias document: document
    readonly property bool hasDocument: document.status === PdfDocument.Ready
    readonly property int pageCount: document.pageCount
    readonly property int currentPage: view.currentPage
    readonly property real zoom: view.renderScale

    readonly property real minZoom: 0.1
    readonly property real maxZoom: 10
    readonly property real zoomStep: 1.25

    function goToPage(page) {
        if (hasDocument)
            view.goToPage(Math.max(0, Math.min(page, pageCount - 1)))
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
                // PdfMultiPageView resets its scale when the source changes, so fit afterwards.
                Qt.callLater(root.fitWidth)
        }
    }

    PdfMultiPageView {
        id: view
        anchors.fill: parent
        document: document
        visible: root.hasDocument
    }

    // Ctrl+wheel zooms; plain wheel falls through to the view for scrolling. This item sits above the view
    // because the view's flickable would otherwise consume every wheel event.
    Item {
        anchors.fill: parent
        z: 1
        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: event => {
                if (event.angleDelta.y > 0)
                    root.zoomIn()
                else if (event.angleDelta.y < 0)
                    root.zoomOut()
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
