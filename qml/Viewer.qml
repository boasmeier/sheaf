import QtQuick
import QtQuick.Controls
import QtQuick.Pdf

// Shows one PDF at a time. Every open() builds a brand-new PdfPane (and so a new PdfDocument and view) in the
// background, restores the position, waits until the visible pages are rendered and only then swaps it in. Until
// then the previous pane stays on screen, so there is never a wrong or half-rendered page. The old pane is destroyed
// afterwards, which also releases its document's memory.
Item {
    id: root

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

    // Shows `url`, restoring `position` ({valid, page, offsetY, zoom}) or fitting the width if it is not valid.
    function open(url, position) {
        // The outgoing pane must not report positions that would be saved for the incoming document's tab.
        if (pane)
            pane.freeze()
        pendingUrl = url
        pendingPosition = position
        loadingLoader.sourceComponent = null
        loadingLoader.sourceComponent = paneComponent
    }

    function close() {
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
        onLoaded: item.open(root.pendingUrl, root.pendingPosition)
    }

    Loader {
        id: slotB
        anchors.fill: parent
        onLoaded: item.open(root.pendingUrl, root.pendingPosition)
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
