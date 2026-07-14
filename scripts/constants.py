from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SCENES_DIR = SCRIPT_DIR.parent / "scenes"
GENERATED_TEXTURES_DIR = SCENES_DIR / "textures" / "generated"
OBJECTS_DIR = SCENES_DIR / "objects"

TEAPOT_ASSET = OBJECTS_DIR / "teapot.usda"
DRAGON_ASSET = OBJECTS_DIR / "dragon_uv.usda"
CORNELL_BOX_ASSET = SCENES_DIR / "cornellBox.usda"

EXISTING_TEXTURES = ["fabric.png", "wood.png", "metal.png"]

# Grid constants
GRID_ROWS = 8
GRID_COLS = 8
GRID_LAYERS = 8
OBJECT_COUNT = GRID_ROWS * GRID_COLS * GRID_LAYERS

SCENE_RANDOM_SEED = 1337
DRAGON_FRACTION_IN_MIXED_SCENE = 0.35

# Cornell box interior bounds
ROOM_MIN_X = -5.0
ROOM_MAX_X = 5.0
ROOM_MIN_Z = -10.0
ROOM_MAX_Z = 5.0
ROOM_MIN_Y = 0.0
ROOM_MAX_Y = 10.0

GRID_WALL_MARGIN = 0.5
OBJECT_FILL_FACTOR = 0.7