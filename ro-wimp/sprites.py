"""Build TV (mode 12) and VGA (mode 27) sprite resources without image tools.

Large sprites are 68 OS units square: 34x17 TV or 34x34 VGA pixels.
The old-style mask has four bits per pixel; sprite files omit the area size.
"""
import struct


def artwork(host=False, tv=False):
    pixels = [[0] * 32 for _ in range(32)]
    mask = [[0] * 32 for _ in range(32)]

    def rect(x0, y0, x1, y1, colour):
        for y in range(y0, y1):
            for x in range(x0, x1):
                pixels[y][x] = colour
                mask[y][x] = 15

    if host:
        rect(2, 3, 30, 25, 7)
        rect(3, 4, 29, 24, 2)
        rect(5, 6, 27, 21, 7)
        rect(6, 7, 26, 20, 1)
        rect(8, 9, 24, 10, 0)
        rect(14, 25, 18, 28, 4)
        rect(8, 28, 24, 30, 7)
    else:
        for p in range(6, 27, 4):
            rect(p, 1, p+2, 31, 4)
            # The large canvas adds one row above this artwork. Align TV
            # side pins to a scanline pair, so combining rows does not turn
            # each pin into two adjacent TV pixels and close every gap.
            side_y = p-1 if tv else p
            rect(1, side_y, 31, side_y+2, 4)
        rect(4, 4, 28, 28, 7)
        rect(5, 5, 27, 27, 2)
        rect(7, 7, 25, 25, 7)
        rect(8, 8, 24, 24, 4)
        rect(9, 9, 23, 10, 1)
        rect(9, 10, 10, 23, 1)
        rect(11, 13, 21, 15, 0)
        rect(15, 15, 17, 21, 0)
        rect(6, 6, 8, 8, 0)
    return pixels, mask


def canvas(host=False, small=False, tv=False):
    if small:
        # A separate small chip design, with legible pins and an unlettered die.
        pixels = [[0]*17 for _ in range(17)]
        mask = [[0]*17 for _ in range(17)]
        side_pins = (4, 8, 12) if tv else (4, 7, 10, 13)
        for y in range(1, 16):
            for x in range(1, 16):
                body = 3 <= x < 14 and 3 <= y < 14
                pin = ((x in (1, 2, 14, 15) and y in side_pins) or
                       (y in (1, 2, 14, 15) and x in (4, 7, 10, 13)))
                if body or pin:
                    mask[y][x] = 15
                    pixels[y][x] = 7 if body and (x in (3, 13) or y in (3, 13)) else 4
        return pixels, mask
    image, alpha = artwork(host, tv)
    pixels = [[0]*34 for _ in range(34)]
    mask = [[0]*34 for _ in range(34)]
    for y in range(32):
        pixels[y+1][1:33] = image[y]
        mask[y+1][1:33] = alpha[y]
    return pixels, mask


def sprite(name, vga=False, host=False, small=False):
    pixels, alpha = canvas(host, small, tv=not vga)
    width = len(pixels[0])
    if not vga:
        # Preserve opaque detail when combining two square-pixel scanlines.
        combined, masks = [], []
        for y in range(0, len(pixels), 2):
            other = min(y+1, len(pixels)-1)
            row, mask = [], []
            for x in range(width):
                candidates = [pixels[k][x] for k in (y, other) if alpha[k][x]]
                row.append(max(candidates) if candidates else 0)
                mask.append(15 if candidates else 0)
            combined.append(row)
            masks.append(mask)
        pixels, alpha = combined, masks
    stride = ((width*4+31)//32)*4

    def packed(rows):
        data = bytearray()
        for row in rows:
            output = bytearray(stride)
            for x, colour in enumerate(row):
                output[x//2] |= colour << ((x % 2)*4)
            data.extend(output)
        return bytes(data)

    image, mask = packed(pixels), packed(alpha)
    size = 44+len(image)+len(mask)
    return (struct.pack('<I12s7I', size, name.encode('ascii'), stride//4-1,
                        len(pixels)-1, 0, (width*4-1) % 32,
                        44, 44+len(image), 27 if vga else 12) + image + mask)


def build(vga=False, private=False):
    if private:
        data = sprite('tpnode', vga) + sprite('host', vga, host=True)
        count = 2
    else:
        data = (sprite('!ispy', vga) + sprite('sm!ispy', vga, small=True) +
                sprite('ic_ispy', vga))
        count = 3
    return struct.pack('<3I', count, 16, 16+len(data)) + data
