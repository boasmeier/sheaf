import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // Set from the command line.
    property url initialFile

    visible: true
    width: 1100
    height: 750
    title: viewer.hasDocument ? qsTr("%1 – sheaf").arg(fileName(viewer.source)) : "sheaf"

    function fileName(url) {
        const path = url.toString()
        return decodeURIComponent(path.substring(path.lastIndexOf("/") + 1))
    }

    Component.onCompleted: if (initialFile.toString() !== "") viewer.source = initialFile

    FileDialog {
        id: openDialog
        title: qsTr("Open PDF")
        nameFilters: [qsTr("PDF files (*.pdf)"), qsTr("All files (*)")]
        onAccepted: viewer.source = selectedFile
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: openDialog.open() }
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
            Label { text: qsTr("/ %1").arg(viewer.pageCount) ; enabled: viewer.hasDocument }

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

    Viewer {
        id: viewer
        anchors.fill: parent
        focus: true
    }
}
