// Copyright (C) 2026 Boas Meier
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtTest
import "helpers.js" as Helpers

// The image helpers the other tests rely on must actually tell images apart; a fingerprint that is always the same
// would make every "no wrong page image" check pass vacuously.
Item {
    width: 200
    height: 100

    Rectangle {
        id: swatch
        anchors.fill: parent
        color: "red"
        Rectangle { id: mark; width: 40; height: 20; x: 10; y: 10; color: "white" }
    }

    TestCase {
        name: "ImageHelpers"
        when: windowShown

        function test_fingerprints_tell_images_apart() {
            const red = Helpers.fingerprint(grabImage(swatch))
            compare(Helpers.fingerprint(grabImage(swatch)), red)  // stable for the same picture
            verify(red !== 0, "a red image must not fingerprint as 0")

            mark.x = 120
            wait(50)
            verify(Helpers.fingerprint(grabImage(swatch)) !== red, "a moved mark must change the fingerprint")
            mark.x = 10
            wait(50)
            compare(Helpers.fingerprint(grabImage(swatch)), red)

            swatch.color = "#fe0000"  // one colour step off
            wait(50)
            verify(Helpers.fingerprint(grabImage(swatch)) !== red, "a slightly different colour must change it")
            swatch.color = "red"
        }

        function test_differing_fraction_separates_noise_from_a_different_page() {
            const reference = grabImage(swatch)
            compare(Helpers.differingFraction(reference, grabImage(swatch)), 0)

            mark.x = 11  // a one-pixel shift of a small mark: a thin edge only
            wait(50)
            const shifted = Helpers.differingFraction(reference, grabImage(swatch))
            verify(shifted < Helpers.sameThreshold, "1px shift gave " + shifted)
            mark.x = 10

            swatch.color = "#404040"  // a completely different page
            wait(50)
            const different = Helpers.differingFraction(reference, grabImage(swatch))
            verify(different > 0.5, "different picture gave only " + different)
            swatch.color = "red"
        }

        function test_regions_only_look_at_their_own_area() {
            mark.x = 120  // in the right half
            wait(50)
            const image = grabImage(swatch)
            const left = Helpers.fingerprintRegion(image, 0, 0, 100, 100)
            const right = Helpers.fingerprintRegion(image, 100, 0, 200, 100)
            mark.x = 150  // still the right half: only that region changes
            wait(50)
            const changed = grabImage(swatch)
            compare(Helpers.fingerprintRegion(changed, 0, 0, 100, 100), left)
            verify(Helpers.fingerprintRegion(changed, 100, 0, 200, 100) !== right)
            mark.x = 10
        }
    }
}
