import random
from dataclasses import dataclass

from .textures import generate_unique_material_texture

MATERIAL_KIND_WEIGHTS = {
    "diffuse_tex": 35,
    "metal_tex": 30,
    "plastic_tex": 30,
    "glass": 3,
    "checker": 5,
}

COLOR_CHANNEL_RANGE = (0.2, 0.9)
ROUGHNESS_RANGE = (0.05, 0.9)
GLASS_IOR_RANGE = (1.3, 1.8)
CHECKER_CELL_SIZE_RANGE = (0.5, 2.0)


@dataclass
class MaterialRecipe:
    name: str
    kind: str
    color: tuple = (0.8, 0.8, 0.8)
    roughness: float = 0.5
    ior: float = 1.5
    texture_path: str | None = None
    checker_cell_size: float = 1.0


def random_color(rng: random.Random) -> tuple:
    """
    Generates a random RGB color tuple with each channel drawn uniformly
    from COLOR_CHANNEL_RANGE.

    Args:
        rng: Seeded random number generator to draw channel values from.
    """
    lo, hi = COLOR_CHANNEL_RANGE

    return tuple(round(rng.uniform(lo, hi), 3) for _ in range(3))


def build_material_kind_sequence(count: int, rng: random.Random) -> list[str]:
    """
    Builds a shuffled list of `count` material kind labels proportioned according
    to MATERIAL_KIND_WEIGHTS.

    Args:
        count: Total number of material kind entries to produce.
        rng: Seeded random number generator used to shuffle the sequence.
    """
    total_weight = sum(MATERIAL_KIND_WEIGHTS.values())
    sequence = []

    for kind, weight in MATERIAL_KIND_WEIGHTS.items():
        sequence.extend([kind] * round(count * weight / total_weight))

    most_common = max(MATERIAL_KIND_WEIGHTS, key=MATERIAL_KIND_WEIGHTS.get)

    while len(sequence) < count:
        sequence.append(most_common)

    while len(sequence) > count:
        sequence.pop()

    rng.shuffle(sequence)

    return sequence


def build_material_pool(count: int, rng: random.Random) -> list[MaterialRecipe]:
    """
    Builds a pool of `count` MaterialRecipe instances spanning diffuse, metal, plastic,
    glass, and checker kinds, generating textures for textured variants and reporting progress as it goes.

    Args:
        count: Total number of materials to generate.
        rng: Seeded random number generator used for kind selection, colors, roughness, IOR, and texture generation.
    """
    kinds = build_material_kind_sequence(count, rng)
    materials = []

    total_textured = sum(1 for kind in kinds if kind.endswith("_tex"))
    textured_progress = 0

    for index, kind in enumerate(kinds):
        name = f"Material_{index:03d}_{kind}"

        if kind in ("diffuse_tex", "metal_tex", "plastic_tex"):
            textured_progress += 1
            print(
                f"Generating textures: {textured_progress}/{total_textured} "
                f"({100 * textured_progress / total_textured:.0f}%) - {name}",
                end="\r",
                flush=True,
            )

            materials.append(
                MaterialRecipe(
                    name=name,
                    kind=kind,
                    color=random_color(rng),
                    roughness=round(rng.uniform(*ROUGHNESS_RANGE), 3),
                    texture_path=generate_unique_material_texture(name, rng),
                )
            )
        elif kind == "glass":
            materials.append(
                MaterialRecipe(
                    name=name,
                    kind=kind,
                    ior=round(rng.uniform(*GLASS_IOR_RANGE), 3),
                )
            )
        elif kind == "checker":
            materials.append(
                MaterialRecipe(
                    name=name,
                    kind=kind,
                    color=random_color(rng),
                    checker_cell_size=round(rng.uniform(*CHECKER_CELL_SIZE_RANGE), 3),
                )
            )
        else:
            raise ValueError(f"Unhandled material kind: {kind}")

    if total_textured > 0:
        print()

    return materials
