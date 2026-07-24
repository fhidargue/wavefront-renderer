import random

import numpy as np
from constants import GENERATED_TEXTURES_DIR, SCENES_DIR
from PIL import Image

TEXTURE_SIZE_TIERS = [
    (256, 15),  # cheap
    (1024, 35),  # medium
    (2048, 35),  # large
    (4096, 15),  # heavy
]
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
        lattice_values = [rng.uniform(0.0, 1.0) for _ in range(lattice_size * lattice_size)]
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
    seed = rng.randrange(2**31)
    np_rng = np.random.default_rng(seed)
    pixel_data = np_rng.integers(0, 256, size=(size, size, 3), dtype=np.uint8)

    return Image.fromarray(pixel_data, mode="RGB")


def generate_gradient_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image that diagonally blends between two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the start and end colors.
    """
    start_color = np.array([rng.randrange(256) for _ in range(3)], dtype=np.float32)
    end_color = np.array([rng.randrange(256) for _ in range(3)], dtype=np.float32)

    xs = np.arange(size, dtype=np.float32)
    ys = np.arange(size, dtype=np.float32)
    t = np.clip((xs[np.newaxis, :] + ys[:, np.newaxis]) / (2.0 * max(size - 1, 1)), 0.0, 1.0)

    pixel_data = (start_color + t[:, :, np.newaxis] * (end_color - start_color)).astype(np.uint8)

    return Image.fromarray(pixel_data, mode="RGB")


def generate_stripe_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image of alternating vertical stripes in two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the stripe count and the two colors.
    """
    stripe_count = rng.choice([4, 8, 16])
    stripe_width = max(1, size // stripe_count)
    color_a = np.array([rng.randrange(256) for _ in range(3)], dtype=np.uint8)
    color_b = np.array([rng.randrange(256) for _ in range(3)], dtype=np.uint8)

    col_indices = np.arange(size)
    stripe_mask = (col_indices // stripe_width) % 2
    pixel_row = np.where(stripe_mask[:, np.newaxis], color_b, color_a)
    pixel_data = np.broadcast_to(pixel_row[np.newaxis, :, :], (size, size, 3)).copy()

    return Image.fromarray(pixel_data, mode="RGB")


def generate_checker_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image of a checkerboard pattern in two randomly chosen colors.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to pick the cell count and the two colors.
    """
    cell_count = rng.choice([4, 8])
    cell_size = max(1, size // cell_count)
    color_a = np.array([rng.randrange(256) for _ in range(3)], dtype=np.uint8)
    color_b = np.array([rng.randrange(256) for _ in range(3)], dtype=np.uint8)

    xs = np.arange(size) // cell_size
    ys = np.arange(size) // cell_size
    checker = (xs[np.newaxis, :] + ys[:, np.newaxis]) % 2
    pixel_data = np.where(checker[:, :, np.newaxis], color_b, color_a).astype(np.uint8)

    return Image.fromarray(pixel_data, mode="RGB")


def generate_fractal_noise_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image by independently generating fractal noise for each
    color channel and merging them.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used to generate each channel's noise.
    """
    channels = [
        Image.fromarray(_generate_fractal_noise_channel(size, rng), mode="L") for _ in range(3)
    ]

    return Image.merge("RGB", channels)


def generate_layered_composite_texture(size: int, rng: random.Random) -> Image.Image:
    """
    Generates a square RGB image by compositing three independent fractal noise
    layers with randomised blend weights, simulating a multi-texture lookup.

    Args:
        size: Width and height, in pixels, of the square output image.
        rng: Seeded random number generator used for each layer and blend weights.
    """
    layer_count = 3
    weights = [rng.uniform(0.2, 1.0) for _ in range(layer_count)]
    total_weight = sum(weights)

    accumulated = np.zeros((size, size, 3), dtype=np.float64)

    for weight in weights:
        channels = [_generate_fractal_noise_channel(size, rng) for _ in range(3)]
        layer = np.stack(channels, axis=-1).astype(np.float64)
        accumulated += layer * (weight / total_weight)

    return Image.fromarray(np.clip(accumulated, 0, 255).astype(np.uint8), mode="RGB")


TEXTURE_GENERATORS = [
    generate_noise_texture,
    generate_gradient_texture,
    generate_stripe_texture,
    generate_checker_texture,
    generate_fractal_noise_texture,
    generate_layered_composite_texture,
]


def generate_unique_material_texture(material_name: str, rng: random.Random) -> str:
    """
    Produces a texture for the given material by picking a size from weighted tiers
    and a randomly chosen generator, then saves it and returns its relative path.

    Args:
        material_name: Name of the material the texture is being generated for.
        rng: Seeded random number generator used for size, generator choice, and pixel content.
    """
    sizes, weights = zip(*TEXTURE_SIZE_TIERS)
    size = rng.choices(sizes, weights=weights, k=1)[0]

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


def generate_heavy_texture(material_name: str, rng: random.Random) -> str:
    """
    Produces a guaranteed 4096x4096 layered composite texture for heavy materials.

    Args:
        material_name: Name of the material, used in the output filename.
        rng: Seeded random number generator.
    """
    size = 4096
    image = generate_layered_composite_texture(size, rng)
    filename = f"{material_name}_layered_{size}.png"
    GENERATED_TEXTURES_DIR.mkdir(parents=True, exist_ok=True)
    image.save(GENERATED_TEXTURES_DIR / filename)

    return f"textures/generated/{filename}"
