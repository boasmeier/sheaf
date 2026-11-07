// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The tab tree: groups with their tabs, as a flat list of rows (see SessionModel).
//
// Mouse: click a tab to show it, click a group to make it the target for new files, click the arrow to collapse or
// expand, double-click a group to rename it, right-click for a menu, drag rows to move them.
// Keyboard (list focused): Up/Down move the cursor, Return/Space show a tab or fold a group, Left/Right fold and
// unfold, F2 renames a group, Delete closes a tab or deletes a group.
Item {
    id: root

    // The TabController.
    required property var controller

    readonly property alias list: list
    readonly property int rowHeight: 30

    // A group is about to be deleted; Main asks for confirmation if it has tabs.
    signal deleteGroupRequested(int groupId)

    // Id of the group whose name is being edited, or -1.
    property int renamingGroupId: -1

    // --- Drag and drop state ---
    property bool dragging: false
    property bool dragIsGroup: false
    property int dragRowId: -1
    property string dragTitle
    property real dragPointerY: 0     // in list viewport coordinates
    property int dropRow: -1
    property bool dropAfter: false
    // True if the drop would put a tab into a group (dropped on its header) rather than between rows.
    property bool dropIntoGroup: false

    SystemPalette { id: colors }

    function beginRename(groupId) {
        const row = controller.model.groupRow(groupId)
        if (row < 0)
            return
        list.positionViewAtIndex(row, ListView.Contain)
        renamingGroupId = groupId
    }

    function newGroup() {
        beginRename(controller.newGroup())
    }

    function renameActiveGroup() {
        if (controller.activeGroupId >= 0)
            beginRename(controller.activeGroupId)
    }

    // --- Drag helpers ---

    function updateDrop() {
        const count = list.count
        if (count === 0) {
            dropRow = -1
            return
        }
        const contentPoint = dragPointerY + list.contentY
        let index = list.indexAt(10, contentPoint)
        let after = false
        if (index < 0) {
            // Above the first or below the last row.
            index = contentPoint < 0 ? 0 : count - 1
            after = contentPoint >= 0
        } else {
            const item = list.itemAtIndex(index)
            after = item ? (contentPoint - item.y) > item.height / 2 : false
        }
        dropRow = index
        dropAfter = after
        const target = list.itemAtIndex(index)
        dropIntoGroup = !dragIsGroup && target !== null && target.isGroup
    }

    function endDrag() {
        if (dragging && dropRow >= 0)
            controller.drop(dragIsGroup, dragRowId, dropRow, dropAfter)
        dragging = false
        dropRow = -1
    }

    Timer {
        // Scrolls while a row is dragged near the top or bottom edge of the list.
        interval: 30
        repeat: true
        running: root.dragging
        onTriggered: {
            const edge = 28
            const maxY = Math.max(0, list.contentHeight - list.height)
            if (root.dragPointerY < edge)
                list.contentY = Math.max(0, list.contentY - 14)
            else if (root.dragPointerY > list.height - edge)
                list.contentY = Math.min(maxY, list.contentY + 14)
            root.updateDrop()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolButton {
            Layout.fillWidth: true
            text: qsTr("＋ New group")
            onClicked: root.newGroup()
            ToolTip.text: qsTr("New group (Ctrl+Shift+N)")
            ToolTip.visible: hovered
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.controller.model
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            keyNavigationEnabled: true
            currentIndex: -1
            ScrollBar.vertical: ScrollBar {}

            Connections {
                target: root.controller
                function onActiveTabChanged() {
                    // The cursor follows the active tab.
                    list.currentIndex = root.controller.model.tabRow(root.controller.activeTabId)
                }
            }

            Keys.onPressed: event => {
                const item = list.currentItem
                if (!item)
                    return
                switch (event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                case Qt.Key_Space:
                    if (item.isGroup) {
                        root.controller.selectGroup(item.rowId)
                        root.controller.toggleCollapsed(item.rowId)
                    } else {
                        root.controller.activate(item.rowId)
                    }
                    break
                case Qt.Key_Left:
                    if (item.isGroup)
                        root.controller.setCollapsed(item.rowId, true)
                    else
                        list.currentIndex = root.controller.model.groupRow(item.groupId)
                    break
                case Qt.Key_Right:
                    if (item.isGroup)
                        root.controller.setCollapsed(item.rowId, false)
                    break
                case Qt.Key_F2:
                    if (item.isGroup)
                        root.beginRename(item.rowId)
                    break
                case Qt.Key_Delete:
                    if (item.isGroup)
                        root.deleteGroupRequested(item.rowId)
                    else
                        root.controller.closeTab(item.rowId)
                    break
                default:
                    return
                }
                event.accepted = true
            }

            delegate: Item {
                id: row

                required property int index
                required property string kind
                required property string title
                required property int rowId
                required property int groupId
                required property bool collapsed
                required property int tabCount
                required property bool active
                required property bool missing
                required property string path

                readonly property bool isGroup: kind === "group"
                readonly property bool editing: isGroup && root.renamingGroupId === rowId
                readonly property bool beingDragged: root.dragging && root.dragRowId === rowId
                                                     && root.dragIsGroup === isGroup

                width: ListView.view.width
                height: root.rowHeight
                opacity: beingDragged ? 0.4 : 1

                Rectangle {
                    anchors.fill: parent
                    color: row.active ? colors.highlight
                         : row.isGroup ? Qt.darker(colors.window, 1.06)
                         : hover.hovered ? Qt.darker(colors.window, 1.04) : "transparent"
                }
                // The keyboard cursor.
                Rectangle {
                    anchors.fill: parent
                    visible: row.ListView.isCurrentItem && list.activeFocus
                    color: "transparent"
                    border.color: colors.highlight
                    border.width: 2
                }
                HoverHandler { id: hover }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: row.isGroup ? 4 : 22
                    anchors.rightMargin: 4
                    spacing: 4

                    Label {
                        visible: row.isGroup
                        text: row.collapsed ? "▸" : "▾"
                        Layout.preferredWidth: 18
                        horizontalAlignment: Text.AlignHCenter
                        color: colors.text
                        TapHandler {
                            onTapped: root.controller.toggleCollapsed(row.rowId)
                        }
                    }

                    Label {
                        visible: row.missing
                        text: "⚠"
                        color: row.active ? colors.highlightedText : "#c07000"
                        ToolTip.text: qsTr("File not found:\n%1").arg(row.path)
                        ToolTip.visible: missingHover.hovered
                        HoverHandler { id: missingHover }
                    }

                    Label {
                        visible: !row.editing
                        Layout.fillWidth: true
                        text: row.title
                        elide: Text.ElideRight
                        font.bold: row.isGroup && root.controller.activeGroupId === row.rowId
                        font.italic: row.missing
                        opacity: row.missing ? 0.6 : 1
                        color: row.active ? colors.highlightedText : colors.text
                    }

                    TextField {
                        id: nameField
                        visible: row.editing
                        Layout.fillWidth: true
                        text: row.title
                        selectByMouse: true
                        onVisibleChanged: if (visible) {
                            text = row.title
                            forceActiveFocus()
                            selectAll()
                        }
                        // Handle Return ourselves: TextField's accepted() lets the key travel on to the list, which
                        // would treat it as "fold this group".
                        function commit() {
                            root.controller.renameGroup(row.rowId, text)
                            root.renamingGroupId = -1
                            list.forceActiveFocus()
                        }
                        Keys.onReturnPressed: commit()
                        Keys.onEnterPressed: commit()
                        Keys.onEscapePressed: {
                            root.renamingGroupId = -1
                            list.forceActiveFocus()
                        }
                        onActiveFocusChanged: if (!activeFocus && row.editing) {
                            root.controller.renameGroup(row.rowId, text)
                            root.renamingGroupId = -1
                        }
                    }

                    Label {
                        visible: row.isGroup && !row.editing
                        text: row.tabCount
                        opacity: 0.6
                    }

                    ToolButton {
                        visible: !row.isGroup && (hover.hovered || row.active)
                        text: "×"
                        implicitWidth: 24
                        implicitHeight: 24
                        onClicked: root.controller.closeTab(row.rowId)
                    }
                }

                // Two quick taps on a group name rename it. Timed here rather than with TapHandler's tapCount, which
                // is hard to rely on for the second tap.
                property real lastTapTime: 0

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        const now = Date.now()
                        const doubleTap = now - row.lastTapTime < Qt.styleHints.mouseDoubleClickInterval
                        row.lastTapTime = now
                        if (doubleTap) {
                            // Must not take focus from the rename field it opens.
                            if (row.isGroup)
                                root.beginRename(row.rowId)
                            return
                        }
                        list.currentIndex = row.index
                        list.forceActiveFocus()
                        if (row.isGroup)
                            root.controller.selectGroup(row.rowId)
                        else
                            root.controller.activate(row.rowId)
                    }
                }

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        list.currentIndex = row.index
                        if (row.isGroup) {
                            groupMenu.groupId = row.rowId
                            groupMenu.popup()
                        } else {
                            tabMenu.tabId = row.rowId
                            moveMenu.groups = root.controller.groups()
                            tabMenu.popup()
                        }
                    }
                }

                DragHandler {
                    id: dragHandler
                    target: null  // the row stays put; a ghost follows the pointer
                    acceptedButtons: Qt.LeftButton
                    grabPermissions: PointerHandler.CanTakeOverFromAnything
                    onActiveChanged: {
                        if (active) {
                            root.dragIsGroup = row.isGroup
                            root.dragRowId = row.rowId
                            root.dragTitle = row.title
                            root.dragging = true
                        } else {
                            root.endDrag()
                        }
                    }
                    onCentroidChanged: if (active) {
                        root.dragPointerY = list.mapFromItem(row, centroid.position.x, centroid.position.y).y
                        root.updateDrop()
                    }
                }
            }
        }
    }

    // Where the dragged row would land: a line between rows, or an outline around the group it would join.
    Rectangle {
        visible: root.dragging && root.dropRow >= 0 && !root.dropIntoGroup
        x: list.x
        width: list.width
        height: 2
        color: colors.highlight
        y: {
            const item = list.itemAtIndex(root.dropRow)
            if (!item)
                return -10
            return list.y + item.y - list.contentY + (root.dropAfter ? item.height : 0) - 1
        }
        z: 20
    }

    Rectangle {
        visible: root.dragging && root.dropRow >= 0 && root.dropIntoGroup
        x: list.x
        width: list.width
        height: root.rowHeight
        color: "transparent"
        border.color: colors.highlight
        border.width: 2
        y: {
            const item = list.itemAtIndex(root.dropRow)
            return item ? list.y + item.y - list.contentY : -100
        }
        z: 20
    }

    // The dragged row following the pointer.
    Rectangle {
        visible: root.dragging
        x: list.x + 8
        width: list.width - 16
        height: root.rowHeight - 4
        y: list.y + root.dragPointerY - height / 2
        radius: 3
        color: colors.highlight
        opacity: 0.85
        z: 30
        Label {
            anchors.fill: parent
            anchors.leftMargin: 8
            verticalAlignment: Text.AlignVCenter
            text: root.dragTitle
            elide: Text.ElideRight
            color: colors.highlightedText
        }
    }

    Menu {
        id: groupMenu
        property int groupId: -1
        MenuItem { text: qsTr("Rename"); onTriggered: root.beginRename(groupMenu.groupId) }
        MenuItem {
            text: qsTr("Move up")
            enabled: root.controller.model.groupIndex(groupMenu.groupId) > 0
            onTriggered: root.controller.moveGroup(groupMenu.groupId, root.controller.model.groupIndex(groupMenu.groupId) - 1)
        }
        MenuItem {
            text: qsTr("Move down")
            onTriggered: root.controller.moveGroup(groupMenu.groupId, root.controller.model.groupIndex(groupMenu.groupId) + 1)
        }
        MenuSeparator {}
        MenuItem { text: qsTr("New group"); onTriggered: root.newGroup() }
        MenuItem { text: qsTr("Delete group…"); onTriggered: root.deleteGroupRequested(groupMenu.groupId) }
    }

    Menu {
        id: tabMenu
        property int tabId: -1
        MenuItem { text: qsTr("Close tab"); onTriggered: root.controller.closeTab(tabMenu.tabId) }
        Menu {
            id: moveMenu
            title: qsTr("Move to group")
            property var groups: []
            Instantiator {
                model: moveMenu.groups
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: root.controller.moveTabToGroup(tabMenu.tabId, modelData.id)
                }
                onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => moveMenu.removeItem(object)
            }
        }
    }
}
