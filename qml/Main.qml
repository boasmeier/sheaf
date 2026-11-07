import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // `tabs` (TabController) is provided by the C++ side.
    readonly property alias viewer: viewer

    visible: true
    width: 1100
    height: 750
    title: tabs.activeIndex >= 0 ? qsTr("%1 – sheaf").arg(tabs.activeTitle) : "sheaf"

    // Shows whichever tab is active; only that tab's document is loaded.
    function showActiveTab() {
        if (tabs.activeIndex < 0)
            viewer.close()
        else
            viewer.open(tabs.activeUrl, tabs.activePosition)
    }

    // Back / forward through the links followed in the active tab; the history lives in the TabController per tab.
    function navigate(forward) {
        if (!viewer.hasDocument)
            return
        const here = viewer.topPosition()
        const target = forward ? tabs.goForward(here.page, here.offset, viewer.zoom)
                               : tabs.goBack(here.page, here.offset, viewer.zoom)
        if (target.valid)
            viewer.jumpTo(target)
    }

    function navigateBack() { navigate(false) }
    function navigateForward() { navigate(true) }

    // The URL of an external link waiting for the user's decision (empty when none).
    property url pendingExternalUrl
    readonly property alias externalLinkDialog: externalDialog

    function askToOpenExternally(url) {
        // Only web and mail links; anything else (file:, custom schemes) could do more than the user expects.
        const scheme = url.toString().split(":")[0].toLowerCase()
        if (scheme !== "http" && scheme !== "https" && scheme !== "mailto")
            return
        pendingExternalUrl = url
        externalDialog.open()
    }

    Component.onCompleted: showActiveTab()

    Dialog {
        id: externalDialog
        title: qsTr("Open link?")
        modal: true
        anchors.centerIn: parent
        width: Math.min(window.width - 40, 520)
        standardButtons: Dialog.Open | Dialog.Cancel
        onAccepted: Qt.openUrlExternally(window.pendingExternalUrl)
        Label {
            width: parent.width
            wrapMode: Text.WrapAnywhere
            textFormat: Text.PlainText
            text: qsTr("This PDF links to:\n\n%1\n\nOpen it in your browser?").arg(window.pendingExternalUrl)
        }
    }

    Connections {
        target: tabs
        function onActiveTabChanged() {
            window.showActiveTab()
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open PDF")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("PDF files (*.pdf)"), qsTr("All files (*)")]
        onAccepted: {
            for (const file of selectedFiles)
                tabs.openFile(file)
        }
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: openDialog.open() }
    Shortcut { sequences: [StandardKey.Back]; onActivated: window.navigateBack() }
    Shortcut { sequences: [StandardKey.Forward]; onActivated: window.navigateForward() }
    Shortcut { sequences: [StandardKey.Close]; onActivated: tabs.closeTab(tabs.activeIndex) }
    Shortcut { sequences: ["Ctrl+Tab"]; onActivated: tabs.cycle(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab"]; onActivated: tabs.cycle(-1) }
    Shortcut { sequences: [StandardKey.ZoomIn, "Ctrl+="]; onActivated: viewer.zoomIn() }
    Shortcut { sequences: [StandardKey.ZoomOut]; onActivated: viewer.zoomOut() }
    Shortcut { sequences: ["Ctrl+0"]; onActivated: viewer.fitWidth() }
    Shortcut { sequences: ["Ctrl+G"]; onActivated: pageBox.forceActiveFocus() }
    Shortcut { sequences: [StandardKey.Quit]; onActivated: Qt.quit() }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton { text: qsTr("Open"); onClicked: openDialog.open() }
            ToolButton { text: "←"; enabled: tabs.canGoBack; onClicked: window.navigateBack(); ToolTip.text: qsTr("Back (Alt+Left)"); ToolTip.visible: hovered }
            ToolButton { text: "→"; enabled: tabs.canGoForward; onClicked: window.navigateForward(); ToolTip.text: qsTr("Forward (Alt+Right)"); ToolTip.visible: hovered }

            Item { Layout.fillWidth: true }

            // "Go to page": type a number and press Enter.
            SpinBox {
                id: pageBox
                enabled: viewer.hasDocument
                editable: true
                from: 1
                to: Math.max(1, viewer.pageCount)
                value: viewer.currentPage + 1
                onValueModified: {
                    viewer.goToPage(value - 1)
                    viewer.forceActiveFocus()
                }
            }
            Label { text: qsTr("/ %1").arg(viewer.pageCount); enabled: viewer.hasDocument }

            ToolButton { text: "−"; enabled: viewer.hasDocument; onClicked: viewer.zoomOut() }
            Label {
                text: Math.round(viewer.zoom * 100) + " %"
                Layout.preferredWidth: 48
                horizontalAlignment: Text.AlignHCenter
            }
            ToolButton { text: "+"; enabled: viewer.hasDocument; onClicked: viewer.zoomIn() }
            ToolButton { text: qsTr("Fit width"); enabled: viewer.hasDocument; onClicked: viewer.fitWidth() }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Flat tab list for now; M4 turns this into the group tree.
        ListView {
            id: tabList
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            clip: true
            model: tabs.model
            currentIndex: tabs.activeIndex
            boundsBehavior: Flickable.StopAtBounds

            delegate: ItemDelegate {
                id: tabDelegate
                required property int index
                required property string title
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                onClicked: tabs.activate(index)

                contentItem: RowLayout {
                    Label {
                        Layout.fillWidth: true
                        text: tabDelegate.title
                        elide: Text.ElideRight
                    }
                    ToolButton {
                        text: "×"
                        visible: tabDelegate.hovered || tabDelegate.highlighted
                        onClicked: tabs.closeTab(tabDelegate.index)
                    }
                }
            }
        }

        ToolSeparator { Layout.fillHeight: true }

        Viewer {
            id: viewer
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true

            onLinkFollowed: from => tabs.recordJump(from.page, from.offsetY, from.zoom)
            onExternalLinkRequested: url => window.askToOpenExternally(url)
            onBackRequested: window.navigateBack()
            onForwardRequested: window.navigateForward()

            onViewMoved: {
                const top = topPosition()
                tabs.reportPosition(top.page, top.offset, zoom)
            }
        }
    }
}
