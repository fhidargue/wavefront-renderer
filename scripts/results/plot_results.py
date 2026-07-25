#!/usr/bin/env python3

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from constants import (
    BAR_LABEL_FONT_SIZE,
    FIGURE_OUTPUT_DPI,
    FIGURES_OUTPUT_DIR,
    HOMOGENEITY_Y_AXIS_MAX,
    LEGEND_FONT_SIZE,
    MIN_HOMOGENEITY_FOR_LABEL,
    MIN_VALUE_MS_FOR_LABEL,
    ORDERED_POLICY_LABELS,
    POLICY_COLORS,
    POLICY_DISPLAY_NAMES,
    RESULTS_CSV,
    RUN_LENGTH_FIGURE_SIZE,
    RUN_LENGTH_LABEL_FONT_SIZE,
    RUN_LENGTH_X_AXIS_MARGIN,
    SCENE_DISPLAY_NAMES,
    SEGMENT_LABEL_FONT_SIZE,
    SHADE_TIME_FIGURE_SIZE,
    STAGE_COLORS,
    SUBPLOT_TITLE_FONT_SIZE,
    SUPTITLE_FONT_SIZE,
)

FIGURES_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

NUMERIC_COLUMNS = [
    "shade_ms",
    "intersect_ms",
    "sort_ms",
    "mat_run_length",
    "tex_run_length",
    "mat_homogeneity",
    "tex_homogeneity",
    "total_shaded_hits",
]


def load_data() -> pd.DataFrame:
    """
    Loads the benchmark CSV, converts metric columns to numeric, maps raw keys
    to display labels, and averages multiple runs of the same scene/policy pair.
    """
    df = pd.read_csv(RESULTS_CSV)

    for column in NUMERIC_COLUMNS:
        df[column] = pd.to_numeric(df[column], errors="coerce")

    df["policy"] = df["policy"].map(POLICY_DISPLAY_NAMES).fillna(df["policy"])
    df["scene"] = df["scene"].map(SCENE_DISPLAY_NAMES).fillna(df["scene"])
    df = df.groupby(["scene", "policy"], as_index=False)[NUMERIC_COLUMNS].mean()

    return df


def save(fig: plt.Figure, filename: str):
    """
    Saves a figure as a PNG to the figures output directory.

    Args:
        fig: The matplotlib figure to save.
        filename: Output filename without extension.
    """
    output_path = FIGURES_OUTPUT_DIR / f"{filename}.png"
    fig.savefig(output_path, dpi=FIGURE_OUTPUT_DPI, bbox_inches="tight")
    plt.close(fig)

    print(f"Saved {output_path}")


def annotate_bars_inside(ax: plt.Axes, value_format: str, minimum_height: float):
    """
    Annotates vertical bars with their value centered inside each bar.

    Args:
        ax: The axes containing the bars to annotate.
        value_format: Python format string for the label (e.g. '{:.3f}').
        minimum_height: Bars shorter than this value are not annotated.
    """
    for bar in ax.patches:
        height = bar.get_height()
        if height > minimum_height:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                height * 0.5,
                value_format.format(height),
                ha="center",
                va="center",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="white",
                fontweight="bold",
            )


def plot_shade_time(df: pd.DataFrame):
    """
    Grouped bar chart of shading time per policy per scene.
    Values are annotated inside each bar for direct readability.

    Args:
        df: Benchmark results dataframe with scene, policy, and shade_ms columns.
    """
    fig, ax = plt.subplots(figsize=SHADE_TIME_FIGURE_SIZE)

    sns.barplot(
        data=df,
        x="scene",
        y="shade_ms",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        palette=POLICY_COLORS,
        ax=ax,
    )

    for bar in ax.patches:
        bar_height_ms = bar.get_height()
        if bar_height_ms > MIN_VALUE_MS_FOR_LABEL:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar_height_ms * 0.5,
                f"{bar_height_ms:,.0f}ms",
                ha="center",
                va="center",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="white",
                fontweight="bold",
            )

    ax.set_title("Shading Time by Policy and Scene")
    ax.set_ylabel("Shade Time (ms)")
    ax.set_xlabel("Scene")
    ax.legend(title="Policy")

    save(fig, "shade_time")


def plot_pipeline_breakdown(df: pd.DataFrame):
    """
    Single stacked bar chart combining all scenes and policies.
    X-axis groups by scene then policy so all data is visible in one graph.

    Args:
        df: Benchmark results dataframe.
    """
    rows = []
    for scene in df["scene"].unique():
        for policy in ORDERED_POLICY_LABELS:
            row = df[(df["policy"] == policy) & (df["scene"] == scene)]
            if not row.empty:
                rows.append(
                    {
                        "label": f"{scene}\n{policy}",
                        "Sort": row["sort_ms"].values[0],
                        "Intersect": row["intersect_ms"].values[0],
                        "Shade": row["shade_ms"].values[0],
                    }
                )

    plot_df = pd.DataFrame(rows)

    fig, ax = plt.subplots(figsize=(16, 6))

    bottom = [0] * len(plot_df)
    for stage_name, stage_color in STAGE_COLORS.items():
        values = plot_df[stage_name].tolist()
        bars = ax.bar(
            plot_df["label"],
            values,
            bottom=bottom,
            color=stage_color,
            label=stage_name,
        )

        for bar, val, bot in zip(bars, values, bottom):
            if val > MIN_VALUE_MS_FOR_LABEL:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    bot + val / 2,
                    f"{val:,.0f}",
                    ha="center",
                    va="center",
                    fontsize=SEGMENT_LABEL_FONT_SIZE,
                    color="white",
                    fontweight="bold",
                )

        bottom = [b + v for b, v in zip(bottom, values)]

    for bar_index, (_, row) in enumerate(plot_df.iterrows()):
        total = row["Sort"] + row["Intersect"] + row["Shade"]
        ax.text(
            bar_index,
            total + 300,
            f"{total:,.0f}ms",
            ha="center",
            va="bottom",
            fontsize=SEGMENT_LABEL_FONT_SIZE,
            fontweight="bold",
        )

    ax.set_ylabel("Time (ms)")
    ax.set_xlabel("")
    ax.tick_params(axis="x", rotation=25)
    ax.legend(title="Stage", bbox_to_anchor=(1.01, 1), loc="upper left")

    fig.suptitle("Pipeline Time Breakdown by Policy and Scene", fontsize=SUPTITLE_FONT_SIZE)
    plt.tight_layout()

    save(fig, "pipeline_breakdown")


def plot_cache_homogeneity(df: pd.DataFrame):
    """
    Single bar chart grouping by metric and scene combination.
    Groups are built dynamically from available data — no empty bars.

    Args:
        df: Benchmark results dataframe.
    """
    df_melt = df.melt(
        id_vars=["scene", "policy"],
        value_vars=["mat_homogeneity", "tex_homogeneity"],
        var_name="metric",
        value_name="homogeneity",
    )

    metric_labels = {
        "mat_homogeneity": "Material",
        "tex_homogeneity": "Texture",
    }
    df_melt["metric"] = df_melt["metric"].map(metric_labels)
    df_melt["group"] = df_melt["scene"] + " " + df_melt["metric"]

    available_scenes = df["scene"].unique()
    group_order = [
        f"{scene} {metric}"
        for metric in ["Material", "Texture"]
        for scene in available_scenes
        if not df_melt[df_melt["group"] == f"{scene} {metric}"]["homogeneity"].isna().all()
    ]

    fig, ax = plt.subplots(figsize=SHADE_TIME_FIGURE_SIZE)

    sns.barplot(
        data=df_melt,
        x="group",
        y="homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        order=group_order,
        palette=POLICY_COLORS,
        ax=ax,
    )

    for bar in ax.patches:
        bar_height = bar.get_height()
        if bar_height > MIN_HOMOGENEITY_FOR_LABEL:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar_height * 0.5,
                f"{bar_height:.3f}",
                ha="center",
                va="center",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="white",
                fontweight="bold",
            )

    ax.set_title(
        "Cache Line Homogeneity by Policy, Metric and Scene",
        fontsize=SUBPLOT_TITLE_FONT_SIZE,
    )
    ax.set_ylabel("Homogeneity (0–1)")
    ax.set_xlabel("")
    ax.set_ylim(0, HOMOGENEITY_Y_AXIS_MAX)
    ax.legend(title="Policy", fontsize=LEGEND_FONT_SIZE)

    plt.tight_layout()

    save(fig, "cache_homogeneity")


def plot_run_length(df: pd.DataFrame):
    """
    Horizontal grouped bar chart of average material run length per policy and scene.
    Higher run length means more consecutive rays hit the same material — better coherence.

    Args:
        df: Benchmark results dataframe with mat_run_length column.
    """
    fig, ax = plt.subplots(figsize=RUN_LENGTH_FIGURE_SIZE)

    sns.barplot(
        data=df,
        y="policy",
        x="mat_run_length",
        hue="scene",
        orient="h",
        ax=ax,
        order=ORDERED_POLICY_LABELS,
    )

    for bar in ax.patches:
        bar_width = bar.get_width()
        if bar_width > 0:
            ax.text(
                bar_width + 5,
                bar.get_y() + bar.get_height() / 2,
                f"{bar_width:.1f}",
                va="center",
                ha="left",
                fontsize=RUN_LENGTH_LABEL_FONT_SIZE,
                fontweight="bold",
            )

    ax.set_title(
        "Average Run Length by Policy — higher is better", fontsize=SUBPLOT_TITLE_FONT_SIZE
    )
    ax.set_xlabel("Average Run Length (consecutive rays hitting same material)")
    ax.set_ylabel("Policy")
    ax.legend(title="Scene")
    ax.set_xlim(0, df["mat_run_length"].max() * RUN_LENGTH_X_AXIS_MARGIN)

    fig.suptitle("Run Length by Policy and Scene", fontsize=SUPTITLE_FONT_SIZE)
    plt.tight_layout()

    save(fig, "run_length")


def main():
    sns.set_theme(style="whitegrid", palette="tab10")

    df = load_data()
    print(f"Loaded {len(df)} rows from {RESULTS_CSV}")

    plot_shade_time(df)
    plot_pipeline_breakdown(df)
    plot_cache_homogeneity(df)
    plot_run_length(df)

    print("\nAll figures created")


if __name__ == "__main__":
    main()
