#!/usr/bin/env python3

from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

RESULTS_CSV = Path("results/benchmark_results.csv")
OUTPUT_DIR = Path("results/figures")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

POLICY_ORDER = ["none", "material", "texture", "costBenefit"]

POLICY_LABELS = {
    "none": "None",
    "material": "MaterialAware",
    "texture": "TextureAware",
    "costBenefit": "CostBenefitAware",
}

SCENE_LABELS = {
    "stressTestDragons": "Dragons",
    "stressTestMixed": "Mixed",
}

ORDERED_POLICY_LABELS = [POLICY_LABELS[p] for p in POLICY_ORDER]


def load_data() -> pd.DataFrame:
    """
    Loads the benchmark CSV, converts metric columns to numeric,
    and maps raw keys to human readable labels.
    """
    df = pd.read_csv(RESULTS_CSV)

    numeric_columns = [
        "shade_ms",
        "intersect_ms",
        "sort_ms",
        "mat_run_length",
        "tex_run_length",
        "mat_homogeneity",
        "tex_homogeneity",
        "total_shaded_hits",
    ]
    for col in numeric_columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    df["policy"] = df["policy"].map(POLICY_LABELS).fillna(df["policy"])
    df["scene"] = df["scene"].map(SCENE_LABELS).fillna(df["scene"])

    # Average multiple runs of the same scene/policy combination
    df = df.groupby(["scene", "policy"], as_index=False)[numeric_columns].mean()

    return df


def save(fig: plt.Figure, name: str):
    """
    Saves a figure as both PDF (for the thesis) and PNG (for quick preview).

    Args:
        fig: The matplotlib figure to save.
        name: Output filename without extension.
    """
    png_path = OUTPUT_DIR / f"{name}.png"
    fig.savefig(png_path, dpi=150, bbox_inches="tight")
    plt.close(fig)

    print(f"Saved {png_path}")


def plot_shade_time(df: pd.DataFrame):
    baseline = df[df["policy"] == "None"][["scene", "shade_ms"]].rename(
        columns={"shade_ms": "baseline_ms"}
    )
    merged = df.merge(baseline, on="scene")
    merged["improvement_pct"] = (
        (merged["baseline_ms"] - merged["shade_ms"]) / merged["baseline_ms"] * 100
    )

    fig, ax = plt.subplots(figsize=(11, 5))
    sns.barplot(
        data=merged,
        x="scene",
        y="shade_ms",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=ax,
    )

    for bar in ax.patches:
        height = bar.get_height()
        if height > 500:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                height * 0.5,
                f"{height:,.0f}ms",
                ha="center",
                va="center",
                fontsize=7,
                color="white",
                fontweight="bold",
            )

    ax.set_title("Shading Time by Policy and Scene")
    ax.set_ylabel("Shade Time (ms)")
    ax.set_xlabel("Scene")
    ax.legend(title="Policy")

    save(fig, "shade_time")


def plot_pipeline_breakdown(df: pd.DataFrame):
    df_melt = df.melt(
        id_vars=["scene", "policy"],
        value_vars=["sort_ms", "intersect_ms", "shade_ms"],
        var_name="stage",
        value_name="ms",
    )

    stage_labels = {"sort_ms": "Sort", "intersect_ms": "Intersect", "shade_ms": "Shade"}
    df_melt["stage"] = df_melt["stage"].map(stage_labels)

    scenes = df["scene"].unique()
    fig, axes = plt.subplots(1, len(scenes), figsize=(8 * len(scenes), 6), sharey=True)

    if len(scenes) == 1:
        axes = [axes]

    for ax, scene in zip(axes, scenes):
        scene_df = df_melt[df_melt["scene"] == scene]
        pivot = scene_df.pivot(index="policy", columns="stage", values="ms")
        pivot = pivot.reindex(ORDERED_POLICY_LABELS)
        ordered = pivot[["Sort", "Intersect", "Shade"]]
        ordered.plot(kind="bar", stacked=True, ax=ax, color=["#4878d0", "#ee854a", "#6acc65"])

        # Annotate each segment with its value
        for bar_group_idx, policy in enumerate(ORDERED_POLICY_LABELS):
            if policy not in pivot.index:
                continue
            bottom = 0
            for stage in ["Sort", "Intersect", "Shade"]:
                val = pivot.loc[policy, stage] if stage in pivot.columns else 0
                if val > 500:
                    ax.text(
                        bar_group_idx,
                        bottom + val / 2,
                        f"{val:,.0f}",
                        ha="center",
                        va="center",
                        fontsize=7,
                        color="white",
                        fontweight="bold",
                    )
                bottom += val

        ax.set_title(f"{scene}", fontsize=12)
        ax.set_ylabel("Time (ms)" if ax == axes[0] else "")
        ax.set_xlabel("")
        ax.tick_params(axis="x", rotation=25)

        # Move legend outside the plot area
        ax.legend(title="Stage", bbox_to_anchor=(1.01, 1), loc="upper left", borderaxespad=0)

    fig.suptitle("Pipeline Time Breakdown by Policy and Scene", fontsize=13)
    plt.tight_layout()

    save(fig, "pipeline_breakdown")


def plot_run_length(df: pd.DataFrame):
    """
    Horizontal bar chart comparing material and texture run length per policy.
    Split into two subplots side by side for direct comparison.
    """
    fig, axes = plt.subplots(1, 2, figsize=(16, 5))

    for ax, metric, title in zip(
        axes,
        ["mat_run_length", "tex_run_length"],
        ["Material Run Length", "Texture Run Length"],
    ):
        sns.barplot(
            data=df,
            y="policy",
            x=metric,
            hue="scene",
            orient="h",
            ax=ax,
            order=ORDERED_POLICY_LABELS,
        )
        for bar in ax.patches:
            width = bar.get_width()
            if width > 0:
                ax.text(
                    width + 5,
                    bar.get_y() + bar.get_height() / 2,
                    f"{width:.1f}",
                    va="center",
                    ha="left",
                    fontsize=9,
                    fontweight="bold",
                )
        ax.set_title(f"{title} — higher is better", fontsize=11)
        ax.set_xlabel("Average Run Length")
        ax.set_ylabel("Policy")
        ax.legend(title="Scene")
        ax.set_xlim(0, df[metric].max() * 1.18)

    fig.suptitle("Run Length by Policy and Scene", fontsize=13)
    plt.tight_layout()
    save(fig, "run_length")


def plot_homogeneity(df: pd.DataFrame):
    """
    Side by side bar charts of material and texture cache line homogeneity.
    Values annotated inside each bar.
    """
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    def annotate(ax):
        for bar in ax.patches:
            h = bar.get_height()
            if h > 0.05:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    h * 0.5,
                    f"{h:.3f}",
                    ha="center",
                    va="center",
                    fontsize=8,
                    color="white",
                    fontweight="bold",
                )

    sns.barplot(
        data=df,
        x="scene",
        y="mat_homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[0],
    )
    axes[0].set_title("Material ID Cache Line Homogeneity")
    axes[0].set_ylabel("Homogeneity (0–1)")
    axes[0].set_xlabel("Scene")
    axes[0].legend(title="Policy", fontsize=7)
    axes[0].set_ylim(0, 1.1)
    annotate(axes[0])

    sns.barplot(
        data=df,
        x="scene",
        y="tex_homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[1],
    )
    axes[1].set_title("Texture ID Cache Line Homogeneity")
    axes[1].set_ylabel("Homogeneity (0–1)")
    axes[1].set_xlabel("Scene")
    axes[1].legend(title="Policy", fontsize=7)
    axes[1].set_ylim(0, 1.1)
    annotate(axes[1])

    fig.suptitle("Cache Line Homogeneity by Policy and Scene", fontsize=13)
    plt.tight_layout()

    save(fig, "cache_homogeneity")


def main():
    df = load_data()

    print(f"Loaded {len(df)} rows from {RESULTS_CSV}")
    print(df[["scene", "policy", "shade_ms", "mat_homogeneity"]].to_string(index=False))
    print()

    plot_shade_time(df)
    plot_pipeline_breakdown(df)
    plot_run_length(df)
    plot_homogeneity(df)

    print(f"\nAll figures saved to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
