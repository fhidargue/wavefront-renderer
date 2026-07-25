from pathlib import Path

import seaborn as sns

RESULTS_CSV = Path("results/benchmark_results.csv")
FIGURES_OUTPUT_DIR = Path("results/figures")

POLICY_KEYS = ["none", "material", "texture", "costBenefit"]

POLICY_DISPLAY_NAMES = {
    "none": "None",
    "material": "MaterialAware",
    "texture": "TextureAware",
    "costBenefit": "CostBenefitAware",
}

SCENE_DISPLAY_NAMES = {
    "stressTestDragons": "Dragons",
    "stressTestMixed": "Mixed",
}

ORDERED_POLICY_LABELS = [POLICY_DISPLAY_NAMES[key] for key in POLICY_KEYS]

TAB10_PALETTE = sns.color_palette("tab10").as_hex()

POLICY_COLORS = {
    "None": TAB10_PALETTE[0],
    "MaterialAware": TAB10_PALETTE[1],
    "TextureAware": TAB10_PALETTE[2],
    "CostBenefitAware": TAB10_PALETTE[3],
}

STAGE_COLORS = {
    "Sort": TAB10_PALETTE[0],
    "Intersect": TAB10_PALETTE[1],
    "Shade": TAB10_PALETTE[2],
}

SHADE_TIME_FIGURE_SIZE = (11, 5)
COMBINED_FIGURE_SIZE = (14, 12)
RUN_LENGTH_FIGURE_SIZE = (10, 5)

COMBINED_GRIDSPEC_HSPACE = 0.45
COMBINED_GRIDSPEC_WSPACE = 0.35

MIN_VALUE_MS_FOR_LABEL = 500
MIN_HOMOGENEITY_FOR_LABEL = 0.05

HOMOGENEITY_Y_AXIS_MAX = 1.1
RUN_LENGTH_X_AXIS_MARGIN = 1.18

BAR_LABEL_FONT_SIZE = 7
SEGMENT_LABEL_FONT_SIZE = 7
RUN_LENGTH_LABEL_FONT_SIZE = 9
LEGEND_FONT_SIZE = 7
SUBPLOT_TITLE_FONT_SIZE = 11
SUPTITLE_FONT_SIZE = 13

FIGURE_OUTPUT_DPI = 150
