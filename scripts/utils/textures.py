import random
import numpy as np
from PIL import Image

from constants import EXISTING_TEXTURES, GENERATED_TEXTURES_DIR, SCENES_DIR

GENERATED_TEXTURE_SIZES = [128, 256, 512, 1024]
UNIQUE_TEXTURE_SIZE_RANGE = (128, 2688)
EXISTING_TEXTURE_REUSE_CHANCE = 0.2
BYTES_PER_TEXEL = 12

# Fractal noise
FRACTAL_NOISE_OCTAVES = 5
FRACTAL_NOISE_PERSISTENCE = 0.55
FRACTAL_NOISE_BASE_GRID_SIZE = 4


def _generate_fractal_noise_channel(size: int, rng: random.Random) -> np.ndarray:
    """
    Generates a single-channel fractal (multi-octave) noise array by summing bicubic-upsampled
    random lattices at decreasing amplitude.

    Args:
        size: Width and height, in pixels, of the square output array.
        rng: Seeded random number generator used to fill each octave's lattice values.
    """
    accumulated = np.zeros((size, size), dtype=np.float64)
    amplitude = 1.0
    total_amplitude = 0.0
    grid_size = FRACTAL_NOISE_BASE_GRID_SIZE

    for _ in range(FRACTAL_NOISE_OCTAVES):
        lattice_size = min(grid_size, size)
        lattice_values = [
            rng.uniform(0.0, 1.0) for _ in range(lattice_size * lattice_size)
        ]
        lattice_image = Image.new("F", (lattice_size, lattice_size))
        lattice_image.putdata(lattice_values)
        upsampled = lattice_image.resize((size, size), Image.BICUBIC)

        accumulated += np.array(upsampled, dtype=np.float64) * amplitude
        total_amplitude += amplitude
        amplitude *= FRACTAL_NOISE_PERSISTENCE
        grid_size *= 2

    normalized = np.clip(accumulated / total_amplitude, 0.0, 1.0) * 255.0

    return normalized.astype(np.uint8)


def generate_noise_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image of fully independent random pixel colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to draw each pixel's channel values.
    """
    image = Image.new("RGB", (size, size))
    pixels = image.load()
    for y in range(size):
        for x in range(size):
            pixels[x, y] = (rng.randrange(256), rng.randrange(256), rng.randrange(256))

    return image


def generate_gradient_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image that diagonally blends between two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the start and end colors.
    """
    start_color = tuple(rng.randrange(256) for _ in range(3))
    end_color = tuple(rng.randrange(256) for _ in range(3))
    image = Image.new("RGB", (size, size))
    pixels = image.load()

    for y in range(size):
        for x in range(size):
            t = ((x + y) / (2 * (size - 1))) if size > 1 else 0.0
            pixels[x, y] = tuple(
                int(start_color[c] + (end_color[c] - start_color[c]) * t)
                for c in range(3)
            )

    return image


def generate_stripe_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image of alternating vertical stripes in two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the stripe count and the two colors.
    """
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
    """
    Generates a square RGB image of a checkerboard pattern in two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the cell count and the two colors.
    """
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


def generate_fractal_noise_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image by independently generating fractal noise for each
    color channel and merging them.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to generate each channel's noise.
    """
    channels = [
        Image.fromarray(_generate_fractal_noise_channel(size, rng), mode="L")
        for _ in range(3)
    ]

    return Image.merge("RGB", channels)


TEXTURE_GENERATORS = [
    generate_noise_texture,
    generate_gradient_texture,
    generate_stripe_texture,
    generate_checker_texture,
    generate_fractal_noise_texture,
]


def generate_unique_material_texture(material_name: str, rng: random.Random) -> str:
    """
    Produces a texture for the given material, either by reusing an existing texture at random or by
    generating a new one with a randomly chosen generator and size, and returns its relative path.

    Args:
        material_name: Name of the material the texture is being generated for, used in the output filename.
        rng: Seeded random number generator used for the reuse decision, size, generator choice, and pixel content.
    """
    if rng.random() < EXISTING_TEXTURE_REUSE_CHANCE:
        return f"textures/{rng.choice(EXISTING_TEXTURES)}"

    size = rng.randrange(*UNIQUE_TEXTURE_SIZE_RANGE, 64)
    generator = rng.choice(TEXTURE_GENERATORS)
    image = generator(size, rng)

    kind = generator.__name__.replace("generate_", "").replace("_texture", "")
    filename = f"{material_name}_{kind}_{size}.png"
    GENERATED_TEXTURES_DIR.mkdir(parents=True, exist_ok=True)
    image.save(GENERATED_TEXTURES_DIR / filename)

    return f"textures/generated/{filename}"


def estimate_resident_texture_bytes(materials: list) -> int:
    """
    Estimate total texture memory the renderer will hold resident at once

    Args:
        materials: Material recipes to sum over
    """
    total_bytes = 0

    for material in materials:
        if not material.texture_path:
            continue

        full_path = SCENES_DIR / material.texture_path

        with Image.open(full_path) as image:
            width, height = image.size
        total_bytes += width * height * BYTES_PER_TEXEL

    return total_bytes
