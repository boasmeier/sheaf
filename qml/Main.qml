// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window

    // `tabs` (TabController) is provided by the C++ side.
    readonly property alias viewer: viewer
    readonly property alias sidebar: sidebar
    readonly property alias deleteGroupDialog: deleteGroupDialog
    readonly property alias missingPanel: missingPanel
    readonly property alias mismatchDialog: mismatchDialog
    readonly property alias infoDialog: infoDialog

    visible: true
    width: 1100
    height: 750
    title: tabs.activeTabId >= 0 ? qsTr("%1 – sheaf").arg(tabs.activeTitle) : "sheaf"

    // Shows whichever tab is active; only that tab's document is loaded. A tab whose file is gone shows the "File not
    // found" panel instead.
    function showActiveTab() {
        if (tabs.activeTabId < 0 || tabs.activeTabMissing)
            viewer.close()
        else
            viewer.open(tabs.activeUrl, tabs.activePosition, tabs.activeSearch)
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

    // --- Missing files ---

    // The file the user picked for the active (missing) tab, waiting for the answer to "different file, use anyway?".
    property url pendingLocateUrl

    function locateActiveTab(url) {
        const result = tabs.locate(tabs.activeTabId, url, false)
        if (result === 2) {
            pendingLocateUrl = url
            mismatchDialog.open()
        } else if (result === 1) {
            infoDialog.text = qsTr("That file cannot be read.")
            infoDialog.open()
        }
    }

    FileDialog {
        id: locateDialog
        title: qsTr("Locate the PDF")
        nameFilters: [qsTr("PDF files (*.pdf)"), qsTr("All files (*)")]
        onAccepted: window.locateActiveTab(selectedFile)
    }

    Dialog {
        id: mismatchDialog
        title: qsTr("Different file?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: tabs.locate(tabs.activeTabId, window.pendingLocateUrl, true)
        Label {
            text: qsTr("This does not look like the file that was open here. Use it anyway?")
            wrapMode: Text.Wrap
            width: 320
        }
    }

    Dialog {
        id: infoDialog
        property alias text: infoLabel.text
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        Label { id: infoLabel; wrapMode: Text.Wrap; width: 320 }
    }

    // --- Window size and position, remembered between runs ---

    property bool windowStateReady: false

    function restoreWindowState() {
        const w = Number(tabs.value("window_width", 0))
        const h = Number(tabs.value("window_height", 0))
        if (w >= 480 && h >= 320) {
            width = Math.min(w, Screen.desktopAvailableWidth)
            height = Math.min(h, Screen.desktopAvailableHeight)
        }
        // Position: honoured on X11 and Windows; a Wayland compositor decides by itself.
        const savedX = tabs.value("window_x", "")
        const savedY = tabs.value("window_y", "")
        if (savedX !== "" && savedY !== "") {
            const nx = Number(savedX), ny = Number(savedY)
            if (nx >= 0 && ny >= 0 && nx < Screen.desktopAvailableWidth - 100 && ny < Screen.desktopAvailableHeight - 100) {
                x = nx
                y = ny
            }
        }
        if (tabs.value("window_maximized", "false") === "true")
            visibility = Window.Maximized
    }

    function saveWindowState() {
        if (!windowStateReady)
            return
        if (visibility === Window.Windowed) {
            tabs.setValue("window_width", width)
            tabs.setValue("window_height", height)
            tabs.setValue("window_x", x)
            tabs.setValue("window_y", y)
        }
        tabs.setValue("window_maximized", visibility === Window.Maximized)
        tabs.setValue("sidebar_width", sidebar.width)
    }

    Timer {
        id: windowStateTimer
        interval: 500
        onTriggered: window.saveWindowState()
    }

    onWidthChanged: windowStateTimer.restart()
    onHeightChanged: windowStateTimer.restart()
    onXChanged: windowStateTimer.restart()
    onYChanged: windowStateTimer.restart()
    onVisibilityChanged: windowStateTimer.restart()

    // Every way of quitting (window close, Ctrl+Q, SIGTERM at logout) ends in aboutToQuit.
    Connections {
        target: Qt.application
        function onAboutToQuit() { window.saveWindowState() }
    }

    Component.onCompleted: {
        restoreWindowState()
        windowStateReady = true
        showActiveTab()
    }

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
    Shortcut { sequences: ["Ctrl+F"]; onActivated: viewer.openSearch() }
    Shortcut { sequences: ["F3"]; onActivated: viewer.searchStep(true) }
    Shortcut { sequences: ["Shift+F3"]; onActivated: viewer.searchStep(false) }
    Shortcut { sequences: [StandardKey.Back]; onActivated: window.navigateBack() }
    Shortcut { sequences: [StandardKey.Forward]; onActivated: window.navigateForward() }
    Shortcut { sequences: [StandardKey.Close]; onActivated: tabs.closeTab(tabs.activeTabId) }
    Shortcut { sequences: ["Ctrl+Tab"]; onActivated: tabs.cycle(1) }
    Shortcut { sequences: ["Ctrl+Shift+Tab"]; onActivated: tabs.cycle(-1) }
    Shortcut { sequences: ["Ctrl+Shift+N"]; onActivated: sidebar.newGroup() }
    Shortcut { sequences: ["F2"]; onActivated: sidebar.renameActiveGroup() }
    Shortcut { sequences: ["Ctrl+Shift+Up"]; onActivated: tabs.moveActiveTab(-1) }
    Shortcut { sequences: ["Ctrl+Shift+Down"]; onActivated: tabs.moveActiveTab(1) }
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

    // Deleting a group closes its tabs, so ask unless it is empty.
    property int groupToDelete: -1

    function deleteGroup(groupId) {
        if (tabs.model.groupIndex(groupId) < 0)
            return
        if (tabs.model.groupTabCount(groupId) === 0) {
            tabs.deleteGroup(groupId)
            return
        }
        groupToDelete = groupId
        deleteGroupDialog.open()
    }

    Dialog {
        id: deleteGroupDialog
        title: qsTr("Delete group?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: tabs.deleteGroup(window.groupToDelete)
        Label {
            text: qsTr("This closes the tabs in the group. Positions in the documents are remembered.")
            wrapMode: Text.Wrap
            width: 320
        }
    }

    SplitView {
        anchors.fill: parent

        Sidebar {
            id: sidebar
            // The width of last time, read when the window is created; dragging the handle replaces this binding.
            SplitView.preferredWidth: {
                const saved = Number(tabs.value("sidebar_width", 240))
                return saved >= 140 && saved < 2000 ? saved : 240
            }
            SplitView.minimumWidth: 140
            SplitView.fillHeight: true
            controller: tabs
            onDeleteGroupRequested: groupId => window.deleteGroup(groupId)
        }

        Item {
            SplitView.fillWidth: true
            SplitView.fillHeight: true

            Viewer {
                id: viewer
                anchors.fill: parent
                focus: true

                onLinkFollowed: from => tabs.recordJump(from.page, from.offsetY, from.zoom)
                onExternalLinkRequested: url => window.askToOpenExternally(url)
                onBackRequested: window.navigateBack()
                onForwardRequested: window.navigateForward()

                onSearchStateChanged: {
                const state = searchState()
                tabs.setSearchState(state.open, state.query, state.page, state.start)
            }

            onViewMoved: {
                    const top = topPosition()
                    tabs.reportPosition(top.page, top.offset, zoom)
                }
            }

            // Shown instead of the document when the active tab's file is gone.
            Rectangle {
                id: missingPanel
                anchors.fill: parent
                visible: tabs.activeTabId >= 0 && tabs.activeTabMissing
                color: palette.window

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 480)
                    spacing: 12

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "⚠"
                        font.pixelSize: 40
                        color: "#c07000"
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("File not found")
                        font.pixelSize: 22
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WrapAnywhere
                        textFormat: Text.PlainText
                        opacity: 0.7
                        text: tabs.activePath
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        opacity: 0.7
                        text: qsTr("It was moved, renamed or deleted. Your place in it is remembered.")
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        Button { text: qsTr("Locate…"); onClicked: locateDialog.open() }
                        Button { text: qsTr("Remove tab"); onClicked: tabs.closeTab(tabs.activeTabId) }
                    }
                }
            }
        }
    }
}
