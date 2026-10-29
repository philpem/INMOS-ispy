"""Check both native sprite formats, dimensions, masks and exported names."""
import struct
import unittest

from sprites import build


def read_sprites(data):
    count, first, used = struct.unpack_from('<3I', data)
    if first != 16 or used != len(data)+4:
        raise AssertionError('Sprite area offsets are inconsistent')
    position = first-4
    output = {}
    for _ in range(count):
        size, name, words, rows, left, right, image, mask, mode = (
            struct.unpack_from('<I12s7I', data, position))
        stride = (words+1)*4
        width = ((words+1)*32-left-(31-right))//4
        height = rows+1
        assert image == 44 and mask-image == stride*height and size-mask == stride*height
        alpha = data[position+mask:position+size]
        assert alpha[0] == 0 and 255 in alpha
        assert all((b & 15) in (0, 15) and (b >> 4) in (0, 15) for b in alpha)
        output[name.rstrip(b'\0').decode()] = (width, height, mode)
        position += size
    assert position == len(data)
    return output


class Sprites(unittest.TestCase):
    def test_public_tv_and_vga_icons(self):
        for vga in (False, True):
            with self.subTest(vga=vga):
                sprites = read_sprites(build(vga))
                self.assertEqual(set(sprites), {'!ispy', 'sm!ispy', 'ic_ispy'})
                self.assertEqual(sprites['!ispy'], (34, 34 if vga else 17, 27 if vga else 12))
                self.assertEqual(sprites['ic_ispy'], sprites['!ispy'])
                self.assertEqual(sprites['sm!ispy'], (17, 17 if vga else 9, 27 if vga else 12))

    def test_private_nodes_do_not_pollute_public_sprite_pool(self):
        for vga in (False, True):
            public = read_sprites(build(vga))
            private = read_sprites(build(vga, private=True))
            self.assertEqual(set(private), {'host', 'tpnode'})
            self.assertFalse(set(public) & set(private))
            self.assertEqual(private['tpnode'], public['!ispy'])
            self.assertEqual(private['host'], private['tpnode'])


if __name__ == '__main__':
    unittest.main()
