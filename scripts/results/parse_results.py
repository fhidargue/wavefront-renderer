#!/usr/bin/env python3

import csv
import re
import sys
from datetime import datetime
from pathlib import Path

RESULTS_CSV = Path(__file__).resolve().parents[2] / "results" / "benchmark_results.csv"

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


def append_row(row: dict):
    """
    Appends one result row to the CSV, writing the header first if the file
    does not exist yet.

    Args:
        row: Dict with keys matching FIELDS, produced by parse().
    """
    RESULTS_CSV.parent.mkdir(parents=True, exist_ok=True)
    write_header = not RESULTS_CSV.exists()

    with open(RESULTS_CSV, "a", newline="\n") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)

        if write_header:
            writer.writeheader()

        writer.writerow(row)
        f.flush()


if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: parse_results.py <log_file> <scene_name> <policy>")
        sys.exit(1)

    log_text = Path(sys.argv[1]).read_text()

    scene_name = sys.argv[2]
    policy_name = sys.argv[3]

    row = parse(log_text, scene=scene_name, policy=policy_name)
    append_row(row)

    print(f"Recorded: scene={scene_name} policy={policy_name} shade_ms={row['shade_ms']}")
