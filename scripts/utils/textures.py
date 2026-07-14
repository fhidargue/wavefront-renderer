import random
from PIL import Image

from constants import EXISTING_TEXTURES, GENERATED_TEXTURES_DIR

GENERATED_TEXTURE_SIZES = [128, 256, 512, 1024]


def generate_noise_texture(size: int, rng: random.Random) -> Image.Image:
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            pixels[x, y] = (rng.randrange(256), rng.randrange(256), rng.randrange(256))
    return image


def generate_gradient_texture(size: int, rng: random.Random) -> Image.Image:
    start_color = tuple(rng.randrange(256) for _ in range(3))
    end_color = tuple(rng.randrange(256) for _ in range(3))
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            t = ((x + y) / (2 * (size - 1))) if size > 1 else 0.0
            pixels[x, y] = tuple(
                int(start_color[c] + (end_color[c] - start_color[c]) * t) for c in range(3)
            )
    return image


def generate_stripe_texture(size: int, rng: random.Random) -> Image.Image:
    stripe_count = rng.choice([4, 8, 16])
    stripe_width = max(1, size // stripe_count)
    color_a = tuple(rng.randrange(256) for _ in range(3))
    color_b = tuple(rng.randrange(256) for _ in range(3))
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            pixels[x, y] = color_a if (x // stripe_width) % 2 == 0 else color_b
    return image


def generate_checker_texture(size: int, rng: random.Random) -> Image.Image:
    cell_count = rng.choice([4, 8])
    cell_size = max(1, size // cell_count)
    color_a = tuple(rng.randrange(256) for _ in range(3))
    color_b = tuple(rng.randrange(256) for _ in range(3))
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            cell = (x // cell_size + y // cell_size) % 2
            pixels[x, y] = color_a if cell == 0 else color_b
    return image


TEXTURE_GENERATORS = [
    generate_noise_texture,
    generate_gradient_texture,
    generate_stripe_texture,
    generate_checker_texture,
]


def generate_texture_pool(rng: random.Random) -> list[str]:
    """Generates procedural PNGs"""
    GENERATED_TEXTURES_DIR.mkdir(parents=True, exist_ok=True)
    relative_paths = [f"textures/{name}" for name in EXISTING_TEXTURES]

    for size in GENERATED_TEXTURE_SIZES:
        for generator in TEXTURE_GENERATORS:
            image = generator(size, rng)
            filename = f"{generator.__name__.replace('generate_', '').replace('_texture', '')}_{size}.png"
            image.save(GENERATED_TEXTURES_DIR / filename)
            relative_paths.append(f"textures/generated/{filename}")

    return relative_paths