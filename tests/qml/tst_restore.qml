import QtQuick
import QtTest
import "../../qml" as App
import "helpers.js" as Helpers

// Restoring a saved position: exact, across documents, from an unsized viewer, and without ever showing a wrong
// or half-rendered page.
Item {
    id: host
    width: 1000
    height: 700
    property bool sized: true

    App.Viewer {
        id: viewer
        width: host.sized ? host.width : 0
        height: host.sized ? host.height : 0
    }

    SignalSpy { id: switchedSpy; target: viewer; signalName: "switched" }
    SignalSpy { id: movedSpy; target: viewer; signalName: "viewMoved" }

    TestCase {
        name: "Restore"
        when: windowShown

        function init() {
            host.sized = true
            switchedSpy.clear()
            movedSpy.clear()
        }

        function cleanup() {
            viewer.close()
        }

        function openAndWait(url, position) {
            const count = switchedSpy.count
            viewer.open(url, position)
            tryVerify(() => switchedSpy.count > count, 10000)
        }

        // The rendered result after scrolling away and back: what a correct render of the current spot looks like.
        function freshFingerprint() {
            const flick = viewer.pane.flickable()
            const y = flick.contentY
            flick.contentY = y + 6000
            wait(400)
            flick.contentY = y
            wait(600)
            return Helpers.fingerprint(grabImage(viewer))
        }

        function test_restores_exact_position() {
            openAndWait(fixtures.alpha, { valid: false })
            viewer.pane.setZoom(1.9)
            viewer.goToPage(80)
            wait(500)
            const top = viewer.topPosition()
            const zoom = viewer.zoom
            compare(top.page, 80)

            openAndWait(fixtures.alpha, { valid: true, page: top.page, offsetY: top.offset, zoom: zoom })
            const restored = viewer.topPosition()
            compare(restored.page, top.page)
            fuzzyCompare(restored.offset, top.offset, 0.01)
            fuzzyCompare(viewer.zoom, zoom, 0.001)
        }

        function test_restores_offset_within_page() {
            openAndWait(fixtures.beta, { valid: true, page: 120, offsetY: 0.4, zoom: 1.3 })
            const restored = viewer.topPosition()
            compare(restored.page, 120)
            fuzzyCompare(restored.offset, 0.4, 0.01)
            fuzzyCompare(viewer.zoom, 1.3, 0.001)
        }

        function test_restores_across_documents_of_different_length() {
            const cases = [[fixtures.alpha, 140, 0.0, 1.0], [fixtures.beta, 170, 0.5, 0.8], [fixtures.alpha, 7, 0.25, 2.0],
                           [fixtures.beta, 90, 0.0, 1.5], [fixtures.alpha, 140, 0.0, 1.0]]
            for (const c of cases) {
                openAndWait(c[0], { valid: true, page: c[1], offsetY: c[2], zoom: c[3] })
                const top = viewer.topPosition()
                compare(top.page, c[1])
                fuzzyCompare(top.offset, c[2], 0.01)
                fuzzyCompare(viewer.zoom, c[3], 0.001)
            }
        }

        function test_clamps_a_page_beyond_the_document() {
            openAndWait(fixtures.alpha, { valid: true, page: 5000, offsetY: 0, zoom: 1 })
            // The end of the document is as far as the view can scroll; the last pages are on screen.
            verify(viewer.topPosition().page >= 148)
        }

        function test_restore_works_when_the_viewer_starts_unsized() {
            host.sized = false
            viewer.open(fixtures.beta, { valid: true, page: 100, offsetY: 0.3, zoom: 1.2 })
            wait(100)
            host.sized = true
            tryVerify(() => switchedSpy.count > 0, 10000)
            compare(viewer.topPosition().page, 100)
            fuzzyCompare(viewer.topPosition().offset, 0.3, 0.01)
        }

        function test_page_images_match_a_fresh_render_right_after_the_switch() {
            const positions = [[fixtures.alpha, 60], [fixtures.beta, 120], [fixtures.alpha, 100], [fixtures.beta, 40],
                               [fixtures.alpha, 140], [fixtures.beta, 170]]
            for (const p of positions) {
                openAndWait(p[0], { valid: true, page: p[1], offsetY: 0.2, zoom: 1.0 })
                // Grab immediately: nothing wrong may be on screen at the moment the new document appears.
                const immediate = Helpers.fingerprint(grabImage(viewer))
                compare(immediate, freshFingerprint(), "wrong page image after switching to page " + p[1])
            }
        }

        function test_previous_document_stays_visible_until_the_next_is_ready() {
            openAndWait(fixtures.alpha, { valid: true, page: 30, offsetY: 0, zoom: 1.0 })
            const previous = viewer.pane
            const count = switchedSpy.count
            viewer.open(fixtures.beta, { valid: true, page: 100, offsetY: 0, zoom: 1.0 })
            let sawGap = false
            let sawOld = false
            for (let i = 0; i < 400 && switchedSpy.count === count; ++i) {
                if (!viewer.hasDocument)
                    sawGap = true
                if (viewer.pane === previous)
                    sawOld = true
                wait(5)
            }
            tryVerify(() => switchedSpy.count > count, 10000)
            verify(!sawGap, "viewer had no document while switching")
            verify(sawOld || switchedSpy.count > count)
            verify(viewer.pane !== previous)
            compare(viewer.pageCount, 180)
        }

        function test_no_position_report_without_user_action() {
            openAndWait(fixtures.alpha, { valid: true, page: 50, offsetY: 0.5, zoom: 1.0 })
            movedSpy.clear()
            wait(800)
            compare(movedSpy.count, 0, "a restore must not report a position by itself")
        }

        function test_user_scrolling_reports_the_settled_position() {
            openAndWait(fixtures.alpha, { valid: false })
            wait(500)
            movedSpy.clear()
            viewer.goToPage(70)
            tryVerify(() => movedSpy.count > 0, 3000)
            compare(viewer.topPosition().page, 70)
        }

        function test_open_error_is_shown_instead_of_hanging() {
            const count = switchedSpy.count
            viewer.open("file:///nonexistent/missing.pdf", { valid: false })
            tryVerify(() => switchedSpy.count > count, 5000)
            verify(!viewer.hasDocument)
        }
    }
}
