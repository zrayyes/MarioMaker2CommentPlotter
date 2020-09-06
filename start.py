import sys
import re
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


class MyImage(object):
    def __init__(self):
        self.image = None
        self.palette = gen_palette_image(create_palette(COLORS))

    def open(self, image_file_location):
        og = Image.open(image_file_location).convert("RGBA")
        background = Image.new("RGB", (320, 180), "WHITE")
        background.paste(og, mask=og.split()[3])
        self.image = background

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

    def save_to_c(self):
        if len(COLORS) > 16:
            raise BaseException("Too many colors selected")

        pixel_arr = self.get_pixel_array()

        # header
        arr_length = 1 + len(COLORS) + 3

        # Each pixel uses half a byte
        arr_length += len(pixel_arr) // 2

        str_out = "#include <stdint.h>\n#include <avr/pgmspace.h>\n\n"
        str_out += "const uint8_t image_data[%s] PROGMEM = {" % hex(arr_length)

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
        for count, pixel in enumerate(pixel_arr, 0):
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

        # Replace Array size in Joystick.c
        with open('Joystick.c', 'r') as file:
            filedata = file.read()

        filedata = re.sub(
            r'(?<=image_data\[)(.*)(?=\] PROGMEM)', hex(arr_length), filedata)

        with open('Joystick.c', 'w') as file:
            file.write(filedata)


if __name__ == "__main__":
    my_img = MyImage()
    my_img.open(sys.argv[1])
    my_img.quantize(len(sys.argv) > 2)
    my_img.save_to_c()
    my_img.save("preview.png")
