import random
from dataclasses import dataclass

# Only Diffuse/Metal/Plastic
MATERIAL_KIND_WEIGHTS = {
    "diffuse": 15,
    "diffuse_tex": 20,
    "metal": 10,
    "metal_tex": 15,
    "plastic": 10,
    "plastic_tex": 15,
    "glass": 8,
    "checker": 3,
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
    lo, hi = COLOR_CHANNEL_RANGE
    return tuple(round(rng.uniform(lo, hi), 3) for _ in range(3))


def build_material_kind_sequence(count: int, rng: random.Random) -> list[str]:
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


def build_material_pool(count: int, texture_pool: list[str], rng: random.Random) -> list[MaterialRecipe]:
    kinds = build_material_kind_sequence(count, rng)
    materials = []

    for index, kind in enumerate(kinds):
        name = f"Material_{index:03d}_{kind}"

        if kind in ("diffuse", "metal", "plastic"):
            materials.append(MaterialRecipe(
                name=name, kind=kind,
                color=random_color(rng),
                roughness=round(rng.uniform(*ROUGHNESS_RANGE), 3),
            ))
        elif kind in ("diffuse_tex", "metal_tex", "plastic_tex"):
            materials.append(MaterialRecipe(
                name=name, kind=kind,
                color=random_color(rng),
                roughness=round(rng.uniform(*ROUGHNESS_RANGE), 3),
                texture_path=rng.choice(texture_pool),
            ))
        elif kind == "glass":
            materials.append(MaterialRecipe(
                name=name, kind=kind,
                ior=round(rng.uniform(*GLASS_IOR_RANGE), 3),
            ))
        elif kind == "checker":
            materials.append(MaterialRecipe(
                name=name, kind=kind,
                color=random_color(rng),
                checker_cell_size=round(rng.uniform(*CHECKER_CELL_SIZE_RANGE), 3),
            ))
        else:
            raise ValueError(f"Unhandled material kind: {kind}")

    return materials