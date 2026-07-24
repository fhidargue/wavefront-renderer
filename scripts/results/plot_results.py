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
        if height > 0:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                height + 100,
                f"{height:,.0f}ms",
                ha="center",
                va="bottom",
                fontsize=7,
                rotation=90,
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
    fig, axes = plt.subplots(1, len(scenes), figsize=(7 * len(scenes), 5), sharey=True)

    if len(scenes) == 1:
        axes = [axes]

    for ax, scene in zip(axes, scenes):
        scene_df = df_melt[df_melt["scene"] == scene]
        pivot = scene_df.pivot(index="policy", columns="stage", values="ms")
        pivot = pivot.reindex(ORDERED_POLICY_LABELS)
        pivot[["Sort", "Intersect", "Shade"]].plot(
            kind="bar", stacked=True, ax=ax, color=["#4878d0", "#ee854a", "#6acc65"]
        )

        # Annotate total on top of each stacked bar
        totals = pivot[["Sort", "Intersect", "Shade"]].sum(axis=1)
        for i, (policy, total) in enumerate(totals.items()):
            ax.text(
                i,
                total + 200,
                f"{total:,.0f}",
                ha="center",
                va="bottom",
                fontsize=7,
            )

        ax.set_title(f"{scene}")
        ax.set_ylabel("Time (ms)" if ax == axes[0] else "")
        ax.set_xlabel("")
        ax.tick_params(axis="x", rotation=25)
        ax.legend(title="Stage")

    fig.suptitle("Pipeline Time Breakdown by Policy and Scene", fontsize=13)
    plt.tight_layout()

    save(fig, "pipeline_breakdown")


def plot_coherence(df: pd.DataFrame):
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))

    def annotate(ax):
        for bar in ax.patches:
            height = bar.get_height()
            if height > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    height + 0.005,
                    f"{height:.3f}",
                    ha="center",
                    va="bottom",
                    fontsize=6,
                    rotation=90,
                )

    def annotate_run(ax):
        for bar in ax.patches:
            height = bar.get_height()
            if height > 0:
                ax.text(
                    bar.get_x() + bar.get_width() / 2,
                    height + 1,
                    f"{height:.1f}",
                    ha="center",
                    va="bottom",
                    fontsize=6,
                    rotation=90,
                )

    sns.barplot(
        data=df,
        x="scene",
        y="mat_run_length",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[0][0],
    )
    axes[0][0].set_title("Material Run Length")
    axes[0][0].set_ylabel("Avg Run Length")
    axes[0][0].set_xlabel("")
    axes[0][0].legend(title="Policy")
    annotate_run(axes[0][0])

    sns.barplot(
        data=df,
        x="scene",
        y="tex_run_length",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[0][1],
    )
    axes[0][1].set_title("Texture Run Length")
    axes[0][1].set_ylabel("Avg Run Length")
    axes[0][1].set_xlabel("")
    axes[0][1].legend(title="Policy")
    annotate_run(axes[0][1])

    sns.barplot(
        data=df,
        x="scene",
        y="mat_homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[1][0],
    )
    axes[1][0].set_title("Material ID Cache Line Homogeneity")
    axes[1][0].set_ylabel("Homogeneity (0–1)")
    axes[1][0].set_xlabel("Scene")
    axes[1][0].legend(title="Policy")
    annotate(axes[1][0])

    sns.barplot(
        data=df,
        x="scene",
        y="tex_homogeneity",
        hue="policy",
        hue_order=ORDERED_POLICY_LABELS,
        ax=axes[1][1],
    )
    axes[1][1].set_title("Texture ID Cache Line Homogeneity")
    axes[1][1].set_ylabel("Homogeneity (0–1)")
    axes[1][1].set_xlabel("Scene")
    axes[1][1].legend(title="Policy")
    annotate(axes[1][1])

    fig.suptitle("Memory Coherence Metrics by Policy and Scene", fontsize=13)
    plt.tight_layout()

    save(fig, "coherence_metrics")


def main():
    df = load_data()

    print(f"Loaded {len(df)} rows from {RESULTS_CSV}")
    print(df[["scene", "policy", "shade_ms", "mat_homogeneity"]].to_string(index=False))
    print()

    plot_shade_time(df)
    plot_pipeline_breakdown(df)
    plot_coherence(df)

    print(f"\nAll figures saved to {OUTPUT_DIR}/")


if __name__ == "__main__":
    main()
