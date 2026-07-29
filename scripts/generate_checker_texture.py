import numpy as np
from PIL import Image

size = 512
tile = 64
img = np.zeros((size, size, 3), dtype=np.uint8)

for row in range(size // tile):
    for col in range(size // tile):
        color = 230 if (row + col) % 2 == 0 else 5
        img[row * tile : (row + 1) * tile, col * tile : (col + 1) * tile] = color

Image.fromarray(img).save("scenes/textures/checker_floor.png")
