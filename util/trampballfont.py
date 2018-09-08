#!/usr/bin/env python3

import argparse
import struct
import os.path
import freetype
import numpy as np

asciichars = ''.join(chr(i) for i in range(ord(' '), ord('~')+1))

def mkbuf(face):
    tops = [(face.load_char(c), face.glyph.bitmap_top)[1] for c in asciichars]
    heights = [(face.load_char(c), face.glyph.bitmap.rows)[1] for c in asciichars]
    widths = [(face.load_char(c), face.glyph.bitmap.width)[1] for c in asciichars]
    lefts = [(face.load_char(c), face.glyph.bitmap_left)[1] for c in asciichars]
    bottoms = [t-h for t, h in zip(tops, heights)]

    highest_height = max(tops)
    chr_w = max(widths)
    chr_h = max(tops) - min(bottoms)
    buf = np.zeros([len(asciichars), chr_h, chr_w], dtype='uint8')

    for i, c in enumerate(asciichars):
        face.load_char(c)
        y = highest_height - face.glyph.bitmap_top
        x = face.glyph.bitmap_left
        h = face.glyph.bitmap.rows
        w = face.glyph.bitmap.width
        buf[i, y:y+h, x:x+w].flat = face.glyph.bitmap.buffer

    return buf, chr_w, chr_h


def demo_buf(buf, chr_w, chr_h):
    img = np.zeros([10, 10, chr_h, chr_w], dtype='uint8')
    img.flat[:buf.size] = buf.flat
    return np.hstack(np.hstack(img))


def demo_xpm(fn, buf, chr_w, chr_h):
    img = demo_buf(buf, chr_w, chr_h)
    colours = np.unique(img)
    abc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    abc += abc.lower()
    abc += '0123456789'
    abc = ' .,:~/?!#' + abc
    abc1 = abc
    while len(colours) > len(abc):
        abc = [a + b for a in abc for b in abc1]
    cmap = dict(zip(colours, abc))
    with open(fn, 'w') as fp:
        print('/* XPM */', file=fp)
        print('static char * font_demo_xpm[] = {', file=fp)
        print('"%d %d %d %d"' % (img.shape[1], img.shape[0], len(cmap), len(abc[0])), file=fp)
        for colourval, code in cmap.items():
            print('"%s\tc #%s",' % (code, 3*('%02x'%colourval)), file=fp)
        for line in img[:-1]:
            print('"' + ''.join(map(cmap.__getitem__, line)) + '",', file=fp)
        print('"' + ''.join(map(cmap.__getitem__, img[-1])) + '"};', file=fp)

def demo_bmp(fn, buf, chr_w, chr_h):
    img = demo_buf(buf, chr_w, chr_h)
    colours = np.unique(img)
    if len(colours) <= 2:
        bits = 1
        px_per_byte = 8
    elif len(colours) <= 0x10:
        bits = 4
        px_per_byte = 2
    else:
        bits = 8
        px_per_byte = 1
    colourtable = dict(zip(colours, range(len(colours))))

    print(bits, len(colours))

    # construct the file in memory first
    h = img.shape[0]
    w = img.shape[1]

    row_len_dwords = int(np.ceil(w / px_per_byte / 4))
    img_buf = np.zeros([h, row_len_dwords*4], dtype='u1')
    padded_original_row_len = row_len_dwords * 4 * px_per_byte
    img_padded = np.zeros([h, padded_original_row_len], dtype='u1')
    img_padded[:,:w] = img
    img_padded = img_padded.reshape([h, row_len_dwords*4, px_per_byte])

    for i in range(img_buf.shape[0]):
        for j in range(img_buf.shape[1]):
            for k in range(px_per_byte):
                clr = colourtable[img_padded[i,j,k]]
                img_buf[h-i-1,j] |= clr << ((px_per_byte-1)-k)*bits

    bitmapinfoheader = struct.pack('<LLLHHLLLLLL',
                                   40,           # header size
                                   w,            # width
                                   h,            # height
                                   1,            # number of colour planes
                                   bits,         # bits per pixel
                                   0,            # uncompressed BI_RGB
                                   img_buf.size, # image size in bytes
                                   1417,         # horiz res
                                   1417,         # vert res
                                   len(colours), # lenth of palette
                                   0)

    bmp_colourtable = b''.join(struct.pack('BBBB', c, c, c, 0) for c in colours)

    dataoffset = 14 + len(bitmapinfoheader) + len(bmp_colourtable)
    filesize = dataoffset + img_buf.size
    fileheader = b'BM' + struct.pack('<LHHL', filesize, 0, 0, dataoffset)

    with open(fn, 'wb') as fp:
        fp.write(fileheader)
        fp.write(bitmapinfoheader)
        fp.write(bmp_colourtable)
        fp.write(img_buf.ravel('C'))


def write_font_bin(fn, buf, size, w, h):
    bitmap = np.all((buf == 0) | (buf == 255))
    with open(fn, 'wb') as fp:
        fp.write(b'TRAMPBALLFONT 1')
        if bitmap:
            fp.write(b'b')
        else:
            fp.write(b'\x00')
        fp.write(struct.pack('!LLLL', size, w, h, buf.size//(w*h)))
        if bitmap:
            octets = int(np.ceil(buf.size/8))
            buf2 = np.zeros([octets, 8], dtype='u1')
            buf2.flat[:buf.size] = buf.flat
            for bits in buf2:
                b = (((bits[0] & 1) << 7) |
                     ((bits[1] & 1) << 6) |
                     ((bits[2] & 1) << 5) |
                     ((bits[3] & 1) << 4) |
                     ((bits[4] & 1) << 3) |
                     ((bits[5] & 1) << 2) |
                     ((bits[6] & 1) << 1) |
                     ((bits[7] & 1) << 0))
                fp.write(bytes([b]))
        else:
            fp.write(buf.ravel('C'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert TTF to trampball bitmap font format')
    parser.add_argument('ttf_file', metavar='ttf-file', help='input file name')
    parser.add_argument('-s', '--size', default=16, type=int,
                        help='nominal size in pixels')
    parser.add_argument('-o', '--output', help='output file name')
    parser.add_argument('-x', '--create-xpm', action='store_true',
                        help='create an XPM image file showing the font')
    parser.add_argument('-b', '--create-bmp', action='store_true',
                        help='create a BMP image file showing the font')

    args = parser.parse_args()

    face = freetype.Face(args.ttf_file)
    size = args.size
    face.set_pixel_sizes(size, size)
    buf, chr_w, chr_h = mkbuf(face)

    out_fn = args.output
    if not out_fn:
        if args.ttf_file.lower().endswith('.ttf'):
            out_fn = args.ttf_file[:-4] + '.tbf'
        else:
            out_fn = args.ttf_file + '.tbf'

    if args.create_xpm:
        xpm_fn = out_fn + '.xpm'
        demo_xpm(xpm_fn, buf, chr_w, chr_h)

    if args.create_bmp:
        xpm_fn = out_fn + '.bmp'
        demo_bmp(xpm_fn, buf, chr_w, chr_h)

    write_font_bin(out_fn, buf, size, chr_w, chr_h)
