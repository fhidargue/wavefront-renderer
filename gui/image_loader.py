import numpy as np
from pathlib import Path


def load_exr(filepath: str) -> np.ndarray | None:
    path = Path(filepath)

    if not path.exists():
        return None

    try:
        import OpenImageIO as oiio

        image = oiio.ImageInput.open(str(path))

        if not image:
            return None

        specification = image.spec()
        pixels = image.read_image(oiio.FLOAT)
        image.close()

        if pixels is None:
            return None

        pixels = pixels.reshape(
            specification.height, specification.width, specification.nchannels
        )

        # Only keep RGB channels from RGBA
        if specification.nchannels > 3:
            pixels = pixels[:, :, :3]

        # Flip image vertically for OpenGL
        pixels = np.flipud(pixels)

        return pixels
    except (OSError, RuntimeError) as error:
        print(f"Failed to load {filepath}: {error}")
        return None


def linear_to_srgb(pixels: np.ndarray) -> np.ndarray:
    pixels = np.clip(pixels, 0.0, None)

    return np.where(
        pixels <= 0.0031308,
        12.92 * pixels,
        1.055 * np.power(pixels, 1.0 / 2.4) - 0.055,
    ).astype(np.float32)
