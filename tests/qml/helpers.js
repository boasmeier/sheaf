.pragma library

// A cheap fingerprint of an image: sampled pixels hashed together. Equal renders give equal fingerprints.
function fingerprint(image) {
    var hash = 0
    for (var y = 0; y < image.height; y += 4) {
        for (var x = 0; x < image.width; x += 4)
            hash = (hash * 31 + (image.pixel(x, y) & 0xffffff)) | 0
    }
    return hash
}
