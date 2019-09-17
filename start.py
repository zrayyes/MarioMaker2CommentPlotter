import sys
import copy
from PIL import Image

COLORS = [
    (1, "fe1600"),  # Light red
    (2, "bb1000"),  # Dark red
    (3, "fff3d0"),  # Light brown
    (4, "ad7f46"),  # Dark brown
    (5, "fff001"),  # Yellow
    (6, "fec100"),  # Orange
    (7, "03d901"),  # Light green
    (8, "00bb00"),  # Dark green
    (9, "01e9ff"),  # Light blue
    (10, "000eff"),  # Dark blue
    (11, "ba62fe"),  # Light purple
    (12, "8617ba"),  # Dark purple
    (13, "fec0fd"),  # Light pink
    (14, "b81889"),  # Dark pink
    # (15, "bbbabb"),  # Gray
    (16, "000000"),  # Black
    (17, "ffffff"),  # White
]


def get_color_by_value(i):
    return COLORS[i][0]


def hex_to_rgb(hex):
    hex.lstrip('#')
    return tuple(int(hex[i:i+2], 16) for i in (0, 2, 4))


def get_color_from_list(n):
    return hex_to_rgb(COLORS[n][1])


def create_palette(colors):
    palette = []
    for _, c in colors:
        c.lstrip("#")
        for i in (0, 2, 4):
            palette.append(int(c[i: i + 2], 16))
    palette += [0] * (255 - len(colors)) * 3
    return palette


def gen_palette_image(palette):
    pal_image = Image.new("P", (1, 1))
    pal_image.putpalette(create_palette(COLORS))
    return pal_image


def split_len(seq, length):
    return [seq[i:i+length] for i in range(0, len(seq), length)]


class MyImage(object):
    def __init__(self, image_file):
        og = Image.open(image_file).convert("RGBA")
        bg = Image.new("RGB", (320, 180), "WHITE")
        bg.paste(og, mask=og.split()[3])
        self.image = bg
        self.palette = gen_palette_image(create_palette(COLORS))

    def quantize(self, dither=False):
        self.image.load()
        self.palette.load()

        if self.palette.mode != "P":
            raise ValueError("bad mode for palette image")
        if self.image.mode != "RGB" and self.image.mode != "L":
            raise ValueError("only RGB or L mode images can be quantized \
                to a palette")
        im = self.image.im.convert("P", 1 if dither else 0, self.palette.im)
        self.image = self.image._new(im)

    def save(self, name):
        self.image.save(name)

    def get_pixel_array(self):
        return([self.image.getpixel((x, y))
                for x in range(self.image.width)
                for y in range(self.image.height)])


def compress_n(pixel_arr, n=1):
    temp = []
    repetition = 1
    held_pixels = None
    for d in range(0, len(pixel_arr), n):
        pixels = pixel_arr[d:d+n]
        if not temp:
            temp.extend(pixels)
            held_pixels = pixels
        elif (held_pixels != pixels) or (repetition == 15):
            temp.append(repetition)
            temp.extend(pixels)
            held_pixels = pixels
            repetition = 1
        else:
            repetition += 1
    # temp.extend(held_pixels)
    temp.append(repetition)
    return temp


def to_c(pixel_arr, half=1):
    if len(COLORS) > 16:
        raise BaseException("Too many colors selected")

    half_len = len(pixel_arr) // 2
    pixel_arr = pixel_arr[:half_len] if half == 1 else pixel_arr[half_len:]

    # header
    array_length = 1 + len(COLORS) + 3

    # Each pixel uses half a byte
    array_length += half_len // 2

    str_out = "#include <stdint.h>\n#include <avr/pgmspace.h>\n\nconst uint8_t image_data[%s] PROGMEM = {" % hex(
        array_length)

    str_out += "0x0"
    str_out += ","
    str_out += "0xFF,"
    for code, color in COLORS:
        str_out += str(hex(code))
        str_out += ","
    str_out += "0xFF,"

    # Half bytes
    left_half = None
    right_half = None
    for pixel in pixel_arr:
        if left_half is None:
            left_half = pixel
        elif right_half is None:
            right_half = pixel
            temp = (left_half << 4)
            temp |= right_half
            str_out += str(hex(temp)) + ","
            left_half, right_half = None, None

    # Odd number remains
    if left_half:
        str_out += str(hex(left_half)) + ","

    str_out += "0xFF};\n"

    with open("image.c", "w") as f:
        f.write(str_out)


def uncompress(pixel_arr, img, size=1):
    w = img.width
    h = img.height
    new_img = copy.copy(img)

    index = 0
    current_value = None
    current_rep = 1

    for col in range(0, h, size):
        for row in range(0, w, size):
            if (current_rep == 1) and (index != len(pixel_arr)):
                current_value = pixel_arr[index]
                current_rep = pixel_arr[index+1]
                index += 2
            else:
                current_rep -= 1

            for y in range(size):
                for x in range(size):
                    new_img.putpixel(
                        (row+x, col+y), get_color_from_list(current_value))

    return new_img


def create_img_from_array(pixel_arr, img):
    w = img.width
    h = img.height
    new_img = copy.copy(img)

    for row in range(w):
        for col in range(h):
            new_img.putpixel(
                (row, col), get_color_from_list(pixel_arr[(row*180) + col]))
    return new_img


if __name__ == "__main__":
    # TODO: Add two-color option option
    my_img = MyImage(sys.argv[1])
    my_img.quantize(len(sys.argv) > 2)

    raw = my_img.get_pixel_array()
    raw_size = (320*180)//2

    to_c(raw)

    my_img.save("preview.png")
