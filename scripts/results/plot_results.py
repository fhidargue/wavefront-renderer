#!/usr/bin/env python3

from pathlib import Path

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

BENCHMARK_SCENES = {"stressTestDragons", "stressTestMixed"}

# Maps stage display name to its raw CSV column name.
STAGE_COLUMN_MAP = {
    "Sort": "sort_ms",
    "Intersect": "intersect_ms",
    "Shade": "shade_ms",
}

NONE_POLICY_LABEL = "None"
PIPELINE_PCT_FONT_SIZE = 6
ORDERED_SCENES = ["Mixed", "Dragons"]


def load_data(csv_path: Path) -> pd.DataFrame:
    """
    Loads a per-sample benchmark CSV, filters to stress scenes only, converts
    metric columns to numeric and maps raw keys to display labels.

    Args:
        csv_path: Path to the specific per-sample CSV to load.
    """
    df = pd.read_csv(csv_path)
    df = df[df["scene"].isin(BENCHMARK_SCENES)]
    df["samples"] = pd.to_numeric(df["samples"], errors="coerce")

    for column in NUMERIC_COLUMNS:
        df[column] = pd.to_numeric(df[column], errors="coerce")

    df["policy"] = df["policy"].map(POLICY_DISPLAY_NAMES).fillna(df["policy"])
    df["scene"] = df["scene"].map(SCENE_DISPLAY_NAMES).fillna(df["scene"])

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
        value_format: Python format string for the label.
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


def _compute_baseline_lookup(df_mean: pd.DataFrame) -> dict:
    """
    Builds a {(scene, stage_name): ms_value} dict for the None policy baseline.
    Used by the pipeline chart to compute per-segment % change annotations.
    """
    baseline_lookup = {}

    for scene in df_mean["scene"].unique():
        none_row = df_mean[(df_mean["policy"] == NONE_POLICY_LABEL) & (df_mean["scene"] == scene)]
        if none_row.empty:
            continue
        for stage_name, stage_column in STAGE_COLUMN_MAP.items():
            baseline_lookup[(scene, stage_name)] = none_row[stage_column].values[0]

    return baseline_lookup


def create_figures(df: pd.DataFrame, save_to_disk: bool = False) -> dict:
    """
    Builds all benchmark figures. When save_to_disk is True, saves each figure
    as a PNG to FIGURES_OUTPUT_DIR. Always returns the dict of figures for GUI use.

    Args:
        df: Raw benchmark results dataframe produced by load_data().
        save_to_disk: If True, saves figures to disk in addition to returning them.
    """
    df_mean = df.groupby(["scene", "policy"], as_index=False)[NUMERIC_COLUMNS].mean()
    figures = {}

    # Shade time

    baseline = df_mean[df_mean["policy"] == NONE_POLICY_LABEL][["scene", "shade_ms"]].rename(
        columns={"shade_ms": "baseline_ms"}
    )
    merged_mean = df_mean.merge(baseline, on="scene")
    merged_mean["improvement_pct"] = (
        (merged_mean["shade_ms"] - merged_mean["baseline_ms"]) / merged_mean["baseline_ms"] * 100
    )
    improvement_lookup = {
        (row["scene"], row["policy"]): row["improvement_pct"] for _, row in merged_mean.iterrows()
    }

    fig, ax = plt.subplots(figsize=SHADE_TIME_FIGURE_SIZE)
    scenes = ORDERED_SCENES

    sns.barplot(
        data=df,
        x="scene",
        y="shade_ms",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        order=scenes,
        palette=POLICY_COLORS,
        errorbar=None,
        ax=ax,
    )

    num_scenes = len(scenes)

    for bar_index, bar in enumerate(ax.patches):
        bar_height_ms = bar.get_height()

        if bar_height_ms < MIN_VALUE_MS_FOR_LABEL:
            continue

        policy = ORDERED_POLICY_LABELS[bar_index // num_scenes]
        scene = scenes[bar_index % num_scenes]

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

        if policy != NONE_POLICY_LABEL:
            pct = improvement_lookup.get((scene, policy), 0)
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar_height_ms + 100,
                f"{pct:+.1f}%",
                ha="center",
                va="bottom",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="black",
                fontweight="bold",
            )

    ax.set_title("Shading Time by Policy and Scene")
    ax.set_ylabel("Shade Time (ms)")
    ax.set_xlabel("Scene")
    ax.legend(title="Policy")
    figures["shade_time"] = fig

    if save_to_disk:
        save(fig, "shade_time")

    # Pipeline breakdown
    baseline_lookup = _compute_baseline_lookup(df_mean)

    rows = []
    for scene in ORDERED_SCENES:
        for policy in ORDERED_POLICY_LABELS:
            row = df_mean[(df_mean["policy"] == policy) & (df_mean["scene"] == scene)]

            if not row.empty:
                rows.append(
                    {
                        "label": f"{scene}\n{policy}",
                        "scene": scene,
                        "policy": policy,
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
        bars = ax.bar(plot_df["label"], values, bottom=bottom, color=stage_color, label=stage_name)

        for bar_index, (bar, val, bot) in enumerate(zip(bars, values, bottom)):
            if val <= MIN_VALUE_MS_FOR_LABEL:
                continue

            scene = plot_df.iloc[bar_index]["scene"]
            policy = plot_df.iloc[bar_index]["policy"]

            segment_center_y = bot + val / 2

            if policy == NONE_POLICY_LABEL:
                # Baseline bars show only the raw ms value
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    segment_center_y,
                    f"{val:,.0f}",
                    ha="center",
                    va="center",
                    fontsize=SEGMENT_LABEL_FONT_SIZE,
                    color="white",
                    fontweight="bold",
                )
            else:
                # Non baseline bars show ms value above center and % change below center
                baseline_val = baseline_lookup.get((scene, stage_name), 0)
                pct_change = (val - baseline_val) / baseline_val * 100 if baseline_val > 0 else 0.0

                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    segment_center_y + val * 0.12,
                    f"{val:,.0f}",
                    ha="center",
                    va="center",
                    fontsize=SEGMENT_LABEL_FONT_SIZE,
                    color="white",
                    fontweight="bold",
                )
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    segment_center_y - val * 0.12,
                    f"{pct_change:+.1f}%",
                    ha="center",
                    va="center",
                    fontsize=PIPELINE_PCT_FONT_SIZE,
                    color="white",
                    fontweight="bold",
                    alpha=0.9,
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
    ax.tick_params(axis="x", rotation=50)
    ax.legend(title="Stage", loc="lower left")
    fig.suptitle("Pipeline Time Breakdown by Policy and Scene", fontsize=SUPTITLE_FONT_SIZE)
    plt.tight_layout()
    figures["pipeline"] = fig

    if save_to_disk:
        save(fig, "pipeline_breakdown")

    # Run Length
    fig, ax = plt.subplots(figsize=(18, 7))

    sns.barplot(
        data=df,
        y="policy",
        x="mat_run_length",
        hue="scene",
        orient="h",
        ax=ax,
        order=ORDERED_POLICY_LABELS,
        errorbar=None,
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
    figures["run_length"] = fig

    if save_to_disk:
        save(fig, "run_length")

    # Cache homogeneity
    df_melt = df.melt(
        id_vars=["scene", "policy"],
        value_vars=["mat_homogeneity", "tex_homogeneity"],
        var_name="metric",
        value_name="homogeneity",
    )
    df_melt["metric"] = df_melt["metric"].map(
        {"mat_homogeneity": "Material ID", "tex_homogeneity": "Texture ID"}
    )
    df_melt["group"] = df_melt["scene"] + " " + df_melt["metric"]
    available_scenes = df["scene"].unique()
    group_order = [
        f"{scene} {metric}"
        for scene in available_scenes
        for metric in ["Material ID", "Texture ID"]
        if not df_melt[df_melt["group"] == f"{scene} {metric}"]["homogeneity"].isna().all()
    ]

    # Use mean values for annotation, raw for error bars
    df_melt_mean = df_mean.melt(
        id_vars=["scene", "policy"],
        value_vars=["mat_homogeneity", "tex_homogeneity"],
        var_name="metric",
        value_name="homogeneity",
    )
    df_melt_mean["metric"] = df_melt_mean["metric"].map(
        {"mat_homogeneity": "Material ID", "tex_homogeneity": "Texture ID"}
    )
    df_melt_mean["group"] = df_melt_mean["scene"] + " " + df_melt_mean["metric"]

    fig, ax = plt.subplots(figsize=SHADE_TIME_FIGURE_SIZE)

    sns.barplot(
        data=df_melt,
        x="group",
        y="homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        order=group_order,
        palette=POLICY_COLORS,
        errorbar=None,
        ax=ax,
    )

    for bar in ax.patches:
        h = bar.get_height()

        if h > MIN_HOMOGENEITY_FOR_LABEL:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                h * 0.5,
                f"{h:.3f}",
                ha="center",
                va="center",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="white",
                fontweight="bold",
            )

    ax.set_ylim(0, HOMOGENEITY_Y_AXIS_MAX)
    ax.set_ylabel("Homogeneity (0–1)")
    ax.set_xlabel("")
    ax.legend(title="Policy", fontsize=LEGEND_FONT_SIZE)
    ax.set_title("Cache Line Homogeneity by Policy and Scene")
    plt.tight_layout()
    figures["homogeneity"] = fig

    if save_to_disk:
        save(fig, "cache_homogeneity")

    # Total shaded hits
    fig, ax = plt.subplots(figsize=SHADE_TIME_FIGURE_SIZE)
    sns.barplot(
        data=df,
        x="scene",
        y="total_shaded_hits",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        order=ORDERED_SCENES,
        palette=POLICY_COLORS,
        errorbar=None,
        ax=ax,
    )

    baseline_hits = df_mean[df_mean["policy"] == NONE_POLICY_LABEL][
        ["scene", "total_shaded_hits"]
    ].rename(columns={"total_shaded_hits": "baseline_hits"})
    merged_hits = df_mean.merge(baseline_hits, on="scene")
    merged_hits["hits_pct"] = (
        (merged_hits["total_shaded_hits"] - merged_hits["baseline_hits"])
        / merged_hits["baseline_hits"]
        * 100
    )
    hits_improvement_lookup = {
        (row["scene"], row["policy"]): row["hits_pct"] for _, row in merged_hits.iterrows()
    }

    scenes_hits = ORDERED_SCENES

    for bar_index, bar in enumerate(ax.patches):
        bar_height = bar.get_height()

        if bar_height < MIN_VALUE_MS_FOR_LABEL:
            continue

        policy = ORDERED_POLICY_LABELS[bar_index // len(scenes_hits)]
        scene = scenes_hits[bar_index % len(scenes_hits)]
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar_height * 0.5,
            f"{bar_height / 1_000_000_000:.3f}B",
            ha="center",
            va="center",
            fontsize=BAR_LABEL_FONT_SIZE,
            color="white",
            fontweight="bold",
        )
        if policy != NONE_POLICY_LABEL:
            pct = hits_improvement_lookup.get((scene, policy), 0)
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                bar_height + 1_000_000,
                f"{pct:+.1f}%",
                ha="center",
                va="bottom",
                fontsize=BAR_LABEL_FONT_SIZE,
                color="black",
                fontweight="bold",
            )

    ax.set_title("Total Shaded Hits by Policy and Scene")
    ax.set_ylabel("Total Shaded Hits")
    ax.set_xlabel("Scene")
    ax.legend(title="Policy")
    figures["shaded_hits"] = fig

    if save_to_disk:
        save(fig, "shaded_hits")

    return figures


def build_figures(df: pd.DataFrame) -> dict:
    """
    GUI entry point. Builds figures without saving to disk.
    """
    return create_figures(df, save_to_disk=False)


def main():
    sns.set_theme(style="whitegrid", palette="tab10")

    results_dir = Path(__file__).resolve().parents[2] / "results"
    bucket_files = sorted(results_dir.glob("benchmark_results_*.csv"))

    if not bucket_files:
        print("No benchmark CSV files found in results/")
        return

    for csv_path in bucket_files:
        sample_label = csv_path.stem.replace("benchmark_results_", "")
        print(f"\nProcessing {csv_path.name}")

        df = load_data(csv_path)

        if df.empty:
            print("No valid data. Skipping")
            continue

        create_figures(df, save_to_disk=True)
        print(f"Figures saved for {sample_label} samples")

    print(f"\nAll figures saved to {FIGURES_OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
