// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The search bar over the top right corner of the document: a field, a "3 of 17" counter, previous / next and close.
// It only shows and reports; the Viewer runs the search.
Rectangle {
    id: root

    // The PdfSearchSession of the document shown (null if there is none).
    property var session: null
    // The text in the field. Set from outside when another tab is shown; typing reports queryEdited().
    property string query: ""

    signal queryEdited(string text)
    signal nextRequested
    signal previousRequested
    signal closeRequested

    readonly property alias field: field

    // What the counter says.
    readonly property string status: {
        if (!session || query.trim() === "")
            return ""
        if (session.count === 0) {
            if (session.searching)
                return qsTr("Searching…")
            if (session.complete)
                return session.hasText ? qsTr("No matches") : qsTr("No text in this document")
            return ""
        }
        if (session.complete)
            return qsTr("%1 of %2").arg(session.currentNumber).arg(session.count)
        // Still reading the document: the total can still grow.
        return qsTr("%1 of %2+").arg(session.currentNumber > 0 ? session.currentNumber : "?").arg(session.count)
    }
    readonly property bool nothingFound: session !== null && session.complete && session.count === 0 && query.trim() !== ""

    function focusField() {
        field.forceActiveFocus()
        field.selectAll()
    }

    onQueryChanged: if (field.text !== query) field.text = query

    implicitWidth: 400
    implicitHeight: 44
    radius: 6
    color: palette.window
    border.color: palette.mid
    z: 50

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 4
        spacing: 4

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Find in document")
            selectByMouse: true
            onTextEdited: root.queryEdited(text)
            Keys.onReturnPressed: event => {
                if (event.modifiers & Qt.ShiftModifier)
                    root.previousRequested()
                else
                    root.nextRequested()
            }
            Keys.onEnterPressed: event => {
                if (event.modifiers & Qt.ShiftModifier)
                    root.previousRequested()
                else
                    root.nextRequested()
            }
            Keys.onEscapePressed: root.closeRequested()
        }

        Label {
            id: counter
            Layout.preferredWidth: 130
            text: root.status
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignRight
            color: root.nothingFound ? "#c0392b" : palette.windowText
            opacity: 0.85
        }

        ToolButton {
            text: "▲"
            enabled: root.session !== null && root.session.count > 0
            onClicked: root.previousRequested()
            ToolTip.text: qsTr("Previous match (Shift+Enter)")
            ToolTip.visible: hovered
        }
        ToolButton {
            text: "▼"
            enabled: root.session !== null && root.session.count > 0
            onClicked: root.nextRequested()
            ToolTip.text: qsTr("Next match (Enter)")
            ToolTip.visible: hovered
        }
        ToolButton {
            text: "×"
            onClicked: root.closeRequested()
            ToolTip.text: qsTr("Close (Esc)")
            ToolTip.visible: hovered
        }
    }
}
