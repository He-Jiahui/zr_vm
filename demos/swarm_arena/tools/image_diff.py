#!/usr/bin/env python3
"""Compare P6 RGB snapshots: all channels within 2 at >=99.5% of pixels."""
import argparse
import json
from pathlib import Path


def read_ppm(path):
    data = Path(path).read_bytes()
    position = 0
    tokens = []
    while len(tokens) < 4:
        while position < len(data) and data[position] in b" \t\r\n":
            position += 1
        if position < len(data) and data[position] == ord("#"):
            position = data.index(b"\n", position) + 1
            continue
        end = position
        while end < len(data) and data[end] not in b" \t\r\n":
            end += 1
        tokens.append(data[position:end])
        position = end
    if tokens[0] != b"P6" or tokens[3] != b"255":
        raise ValueError("expected P6 RGB8 image: " + str(path))
    width, height = map(int, tokens[1:3])
    # Consume only the header delimiter; the first pixel may be whitespace.
    position += 2 if data[position:position + 2] == b"\r\n" else 1
    pixels = data[position:]
    if width <= 0 or height <= 0 or len(pixels) != width * height * 3:
        raise ValueError("incorrect PPM dimensions/payload: " + str(path))
    return width, height, pixels


def compare_pixels(reference, actual):
    if len(reference) != len(actual) or not reference or len(reference) % 3:
        raise ValueError("RGB image sizes differ or are empty")
    matched = 0
    maximum = 0
    for i in range(0, len(reference), 3):
        delta = max(abs(reference[i + c] - actual[i + c]) for c in range(3))
        maximum = max(maximum, delta)
        matched += delta <= 2
    count = len(reference) // 3
    return dict(pixels=count, matched=matched, match_fraction=matched / count,
                max_channel_delta=maximum, channel_tolerance=2, required_fraction=0.995,
                passed=matched * 1000 >= count * 995)


def compare(reference, actual):
    rw, rh, rp = read_ppm(reference)
    aw, ah, ap = read_ppm(actual)
    if (rw, rh) != (aw, ah):
        raise ValueError("snapshot dimensions differ")
    return dict(width=rw, height=rh, **compare_pixels(rp, ap))


def coverage(reference, actual, x0=0, y0=0):
    """Compare non-background masks; geometry coverage is separate from color."""
    rw, rh, rp = read_ppm(reference)
    aw, ah, ap = read_ppm(actual)
    if (rw, rh) != (aw, ah) or not (0 <= x0 < rw and 0 <= y0 < rh):
        raise ValueError("incorrect coverage image size/region")
    background = bytes((16, 20, 28))
    reference_pixels = actual_pixels = intersection = union = 0
    for y in range(y0, rh):
        for x in range(x0, rw):
            i = (y * rw + x) * 3
            left, right = rp[i:i + 3] != background, ap[i:i + 3] != background
            reference_pixels += left
            actual_pixels += right
            intersection += left and right
            union += left or right
    return dict(reference_pixels=reference_pixels, actual_pixels=actual_pixels,
                intersection=intersection, union=union, iou=intersection / union if union else 1.0,
                reference_only=reference_pixels - intersection, actual_only=actual_pixels - intersection,
                region=[x0, y0, rw, rh], background_rgb=list(background))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference")
    parser.add_argument("actual")
    args = parser.parse_args()
    result = compare(args.reference, args.actual)
    print(json.dumps(result, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
