import numpy as np


def main():
    colors = [
        0xF44336, 0xE91E63, 0x9C27B0, 0x673AB7, 0x3F51B5, 0x2196F3, 0x03A9F4,
        0x00BCD4, 0x57C7B8, 0x4CAF50, 0x8BC34A, 0xCDDC39, 0xFFEB3B, 0xFFC107,
        0xFF9800, 0xFF5722
    ]
    colors = [
        np.asarray([(x >> 16) & 0xff, (x >> 8) & 0xff, (x) & 0xff])
        for x in colors
    ]
    output = []
    tstep = 255 / len(colors)
    for r in np.linspace(0.0, 0.999999, 255):
        id = int(np.floor(r * (len(colors) - 1)))
        a = colors[id]
        b = colors[id + 1]
        dc = b - a
        dt = tstep * (r - (id / (len(colors) - 1)))
        print(r, id, dt)
        output.append(a + (dt * dc))
    output = [(int(x[0]) << 16) | (int(x[1]) << 8) | int(x[2]) for x in output]
    for x in output:
        print("0x{:06x}, ".format(x), end='')


if __name__ == "__main__":
    main()
