#!/usr/bin/env python3

import csv
import re
import sys
from datetime import datetime
from pathlib import Path

RESULTS_DIR = Path(__file__).resolve().parents[2] / "results"

BENCHMARK_SCENES = {"stressTestDragons", "stressTestMixed"}

# CSV columns
FIELDS = [
    "timestamp",
    "scene",
    "policy",
    "samples",
    "shade_ms",
    "intersect_ms",
    "sort_ms",
    "mat_run_length",
    "tex_run_length",
    "mat_homogeneity",
    "tex_homogeneity",
    "total_shaded_hits",
]

# Each pattern extracts one number from the renderer stdout block
# using the exact text the renderer already prints
PATTERNS = {
    "shade_ms": r"Total shade time \(ms\)\s+:\s+([\d.]+)",
    "intersect_ms": r"Total intersect time \(ms\)\s+:\s+([\d.]+)",
    "sort_ms": r"Total ray sort time \(ms\)\s+:\s+([\d.]+)",
    "mat_run_length": r"Average material run length\s+:\s+([\d.]+)",
    "tex_run_length": r"Average texture run length\s+:\s+([\d.]+)",
    "mat_homogeneity": r"Material ID cache line homogeneity\s+:\s+([\d.]+)",
    "tex_homogeneity": r"Texture  ID cache line homogeneity\s+:\s+([\d.]+)",
    "total_shaded_hits": r"Total shaded hits\s+:\s+(\d+)",
    "samples": r"Rendered:.*\|\s+(\d+) samples",
}


def parse(stdout: str, scene: str, policy: str) -> dict:
    """
    Extracts benchmark metrics from renderer stdout and returns them as a dict
    ready to be written as a CSV row.

    Args:
        stdout: Full text output captured from the renderer process.
        scene: Scene name label to store in the CSV (e.g. stressTestMixed).
        policy: Scheduling policy label to store in the CSV (e.g. material).
    """
    row = {
        "timestamp": datetime.now().isoformat(),
        "scene": scene,
        "policy": policy,
    }

    for field, pattern in PATTERNS.items():
        match = re.search(pattern, stdout)
        row[field] = match.group(1) if match else ""

    return row


def results_csv_for_samples(sample_count: int) -> Path:
    """
    Returns the CSV path for a specific sample count bucket.

    Args:
        sample_count: Number of samples the benchmark was run at.
    """
    return RESULTS_DIR / f"benchmark_results_{sample_count}.csv"


def append_row(row: dict):
    """
    Appends one result row to the per sample bucket CSV.
    Skips non-benchmark scenes silently.

    Args:
        row: Dict with keys matching FIELDS, produced by parse().
    """
    if row.get("scene") not in BENCHMARK_SCENES:
        return

    sample_count = row.get("samples", "")

    if not sample_count:
        return

    output_path = results_csv_for_samples(int(sample_count))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    write_header = not output_path.exists()

    with open(output_path, "a", newline="\n") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)

        if write_header:
            writer.writeheader()

        writer.writerow(row)
        f.flush()


def list_buckets():
    """
    Lists all existing per sample bucket CSV files and their row counts.
    """
    bucket_files = sorted(RESULTS_DIR.glob("benchmark_results_*.csv"))

    if not bucket_files:
        print("No bucket files found.")
        return

    for path in bucket_files:
        with open(path, newline="") as f:
            row_count = sum(1 for _ in csv.DictReader(f))
        print(f"{path.name}  ({row_count} rows)")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--list-buckets":
        list_buckets()
    elif len(sys.argv) < 4:
        print("Usage: parse_results.py <log_file> <scene_name> <policy>")
        sys.exit(1)
    else:
        log_text = Path(sys.argv[1]).read_text()
        row = parse(log_text, scene=sys.argv[2], policy=sys.argv[3])
        append_row(row)
        print(f"Recorded: scene={row['scene']} policy={row['policy']} shade_ms={row['shade_ms']}")
