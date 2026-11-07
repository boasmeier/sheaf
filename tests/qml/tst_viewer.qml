import QtQuick
import QtTest
import "../../qml" as App

// Basic viewer behaviour: scrolling, zoom, go-to-page and page tracking.
Item {
    width: 1000
    height: 700

    App.Viewer { id: viewer; anchors.fill: parent }

    TestCase {
        name: "Viewer"
        when: windowShown

        function init() {
            viewer.open(fixtures.alpha, { valid: false })
            tryVerify(() => viewer.hasDocument, 5000)
            wait(300)
        }

        function cleanup() {
            viewer.close()
        }

        function test_opens_fit_to_width() {
            verify(viewer.pageCount === 150)
            // Fit width leaves 16 px for the scroll bar; A5 is 420 points wide at 72 dpi.
            const pageWidth = viewer.pane.document.pagePointSize(0).width
            fuzzyCompare(viewer.zoom, (viewer.width - 16) / pageWidth, 0.01)
            compare(viewer.currentPage, 0)
        }

        function test_wheel_scrolls_a_fixed_distance_per_notch() {
            const flick = viewer.pane.flickable()
            const start = flick.contentY
            mouseWheel(viewer, 300, 300, 0, -120)
            tryCompare(flick, "contentY", start + viewer.pane.wheelScrollPixels)
            mouseWheel(viewer, 300, 300, 0, 120)
            tryCompare(flick, "contentY", start)
            // Scrolling up at the top stays at the top.
            mouseWheel(viewer, 300, 300, 0, 120)
            wait(50)
            compare(flick.contentY, flick.originY)
        }

        function test_current_page_follows_wheel_scrolling() {
            for (let i = 0; i < 40; ++i)
                mouseWheel(viewer, 300, 300, 0, -120)
            tryVerify(() => viewer.currentPage > 0, 2000)
            const page = viewer.currentPage
            const top = viewer.topPosition()
            verify(Math.abs(top.page - page) <= 1)
        }

        function test_ctrl_wheel_zooms_and_plain_wheel_does_not() {
            const before = viewer.zoom
            mouseWheel(viewer, 300, 300, 0, -120)
            compare(viewer.zoom, before)
            mouseWheel(viewer, 300, 300, 0, 120, Qt.NoButton, Qt.ControlModifier)
            verify(viewer.zoom > before)
            mouseWheel(viewer, 300, 300, 0, -120, Qt.NoButton, Qt.ControlModifier)
            fuzzyCompare(viewer.zoom, before, 0.001)
        }

        function test_zoom_buttons_and_limits() {
            const before = viewer.zoom
            viewer.zoomIn()
            fuzzyCompare(viewer.zoom, before * 1.25, 0.001)
            viewer.zoomOut()
            fuzzyCompare(viewer.zoom, before, 0.001)
            for (let i = 0; i < 40; ++i)
                viewer.zoomIn()
            compare(viewer.zoom, viewer.pane.maxZoom)
            viewer.fitWidth()
            fuzzyCompare(viewer.zoom, before, 0.001)
        }

        function test_go_to_page_from_any_position() {
            viewer.goToPage(100)
            tryCompare(viewer, "currentPage", 100)
            viewer.goToPage(4)
            tryCompare(viewer, "currentPage", 4)
            // The page the navigator last saw can be stale; going back to page 0 must still work.
            viewer.goToPage(0)
            tryCompare(viewer, "currentPage", 0)
            viewer.goToPage(100000)
            tryCompare(viewer, "currentPage", 149)
        }

        function test_close_releases_the_document() {
            viewer.close()
            verify(!viewer.hasDocument)
            compare(viewer.pageCount, 0)
        }
    }
}
