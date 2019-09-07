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
    if n == 255:
        return (255, 255, 255)
    if n == 0:
        return (0, 0, 0)

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


def compress(pixel_arr):
    temp = []
    counter = 1
    current = None
    for i in pixel_arr:
        if not temp:
            temp.append(i)
        elif (current is not i) or (counter == 15):
            temp.append(counter)
            temp.append(i)
            current = i
            counter = 1
        else:
            counter += 1
    temp.append(current)
    return temp


def to_c(pixel_arr, compressed=False):
    if len(COLORS) > 16:
        raise BaseException("Too many colors selected")

    pixel_arr = pixel_arr[:(len(pixel_arr) // 10) * 5]
    # header
    array_length = 1
    # Number of colors to represent
    array_length += len(COLORS)
    # 3 blank lines 0xFF
    array_length += 3
    # Number of pixels to represent > 320*180
    array_length += len(pixel_arr)//2

    str_out = "#include <stdint.h>\n#include <avr/pgmspace.h>\n\nconst uint8_t image_data[%s] PROGMEM = {" % hex(
        array_length)

    str_out += "0x1" if compressed else "0x0"
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


if __name__ == "__main__":
    # TODO: Add two-color option option
    my_img = MyImage(sys.argv[1])
    my_img.quantize(len(sys.argv) > 2)

    rle = False

    uncompressed = my_img.get_pixel_array()
    uncompressed_size = (320*180)/2

    # compressed = compress(uncompressed)
    # compressed_size = len(compressed)/2

    # rle = True
    # my_img.image = uncompress(compressed, my_img.image)

    to_c(uncompressed)

    my_img.save("preview.png")