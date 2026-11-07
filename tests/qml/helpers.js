.pragma library

// A cheap fingerprint of an image: sampled pixels hashed together. Equal renders give equal fingerprints.
// (Image.pixel() returns a colour string, so the channels are read one by one.)
function fingerprintRegion(image, x0, y0, x1, y1, step) {
    var hash = 0
    var stride = step || 2
    for (var y = y0; y < Math.min(y1, image.height); y += stride) {
        for (var x = x0; x < Math.min(x1, image.width); x += stride)
            hash = (hash * 31 + image.red(x, y) * 65536 + image.green(x, y) * 256 + image.blue(x, y)) | 0
    }
    return hash
}

function fingerprint(image) {
    return fingerprintRegion(image, 0, 0, image.width, image.height, 4)
}

// The fraction (0..1) of sampled pixels that differ clearly (by more than 48 in some colour channel) between two images
// of the same size. Identical renders give 0, a different page gives a large value. Sub-pixel layout differences
// (a border moved by one pixel) only touch thin edges, so they stay in the low single-digit percent.
function differingFraction(a, b, step) {
    var stride = step || 4
    var differing = 0
    var total = 0
    for (var y = 0; y < Math.min(a.height, b.height); y += stride) {
        for (var x = 0; x < Math.min(a.width, b.width); x += stride) {
            ++total
            if (Math.abs(a.red(x, y) - b.red(x, y)) > 48 || Math.abs(a.green(x, y) - b.green(x, y)) > 48
                    || Math.abs(a.blue(x, y) - b.blue(x, y)) > 48)
                ++differing
        }
    }
    return total === 0 ? 0 : differing / total
}

// Largest differingFraction() that still counts as "the same picture".
var sameThreshold = 0.04

// The background grey of page `page` (0-based) in the generated fixtures (see Fixtures.h).
function fixtureShade(page) {
    return 40 + ((page * 61) % 170)
}

// Checks that every visible page of a fixture document shows its own background shade, i.e. that no page displays
// another page's picture. `flick` is the pane's table, `rootItem` the item `image` was grabbed from. Returns
// {checked: number of pages checked, problems: [descriptions]}.
function checkFixturePages(flick, rootItem, image) {
    var problems = []
    var checked = 0
    var first = Math.max(0, flick.cellAtPos(flick.width / 2, 1).y)
    var last = Math.max(first, flick.cellAtPos(flick.width / 2, flick.height - 1).y)
    for (var row = first; row <= last; ++row) {
        var holder = flick.itemAtCell(0, row)
        if (!holder)
            continue
        var paper = null
        for (var i = 0; i < holder.children.length; ++i) {
            if (holder.children[i].pagePointSize !== undefined)
                paper = holder.children[i]
        }
        if (!paper)
            continue
        // A point on the plain background of the page: centred, a fifth down (away from the text, bar and border).
        var point = paper.mapToItem(rootItem, paper.width * 0.5, paper.height * 0.2)
        var x = Math.floor(point.x)
        var y = Math.floor(point.y)
        if (x < 0 || y < 0 || x >= image.width || y >= image.height)
            continue
        var found = image.red(x, y)
        var expected = fixtureShade(row)
        ++checked
        if (Math.abs(found - expected) > 6)
            problems.push("page " + row + " shows shade " + found + ", expected " + expected)
    }
    return { checked: checked, problems: problems }
}
