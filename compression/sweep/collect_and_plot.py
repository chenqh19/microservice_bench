#!/usr/bin/env python3
"""
Collect [x1, latency_us] tuples from lat_<size>_<rps> files in this directory.
Compute per-file: avg(first element), avg(second element).
Plot two tables: rows=size, cols=rps; cells = avg first element, avg second element.
"""

import re
import os
from pathlib import Path
from collections import defaultdict
import numpy as np
import matplotlib.pyplot as plt

SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR / "data"  # lat_* files live in sweep/data/
PAT = re.compile(r"\[(\d+),\s*(\d+)\]")


def parse_file(path: Path) -> tuple[list[tuple[int, int]], int, int]:
    """Read file, extract all [a, b] tuples. Return (list of (a,b), size, rps)."""
    name = path.stem  # lat_2097152_800
    if not name.startswith("lat_"):
        return [], 0, 0
    parts = name[4:].split("_")  # ['2097152', '800']
    if len(parts) != 2:
        return [], 0, 0
    try:
        size = int(parts[0])
        rps = int(parts[1])
    except ValueError:
        return [], 0, 0
    text = path.read_text()
    tuples = []
    for m in PAT.finditer(text):
        a, b = int(m.group(1)), int(m.group(2))
        tuples.append((a, b))
    return tuples, size, rps


def main():
    data = {}  # (size, rps) -> (avg_first, avg_second)
    for f in DATA_DIR.glob("lat_*"):
        if not f.is_file():
            continue
        tuples, size, rps = parse_file(f)
        if not tuples:
            continue
        first_vals = [t[0] for t in tuples]
        second_vals = [t[1] for t in tuples]
        avg_first = sum(first_vals) / len(first_vals)
        avg_second = sum(second_vals) / len(second_vals)
        data[(size, rps)] = (avg_first, avg_second)

    if not data:
        print("No lat_* files with valid tuples found.")
        return

    sizes = sorted({s for s, r in data})
    rps_list = sorted({r for s, r in data})
    n_rows, n_cols = len(sizes), len(rps_list)
    table_first = np.full((n_rows, n_cols), np.nan)
    table_second = np.full((n_rows, n_cols), np.nan)
    size_to_row = {s: i for i, s in enumerate(sizes)}
    rps_to_col = {r: j for j, r in enumerate(rps_list)}

    for (size, rps), (avg_first, avg_second) in data.items():
        i, j = size_to_row[size], rps_to_col[rps]
        table_first[i, j] = avg_first
        table_second[i, j] = avg_second

    # Table 1: average of first element (x1 / inflight)
    fig1, ax1 = plt.subplots(figsize=(max(8, n_cols * 0.5), max(5, n_rows * 0.4)))
    im1 = ax1.imshow(table_first, aspect="auto", cmap="viridis")
    ax1.set_xticks(range(n_cols))
    ax1.set_xticklabels(rps_list, rotation=45, ha="right")
    ax1.set_yticks(range(n_rows))
    ax1.set_yticklabels(sizes)
    ax1.set_xlabel("RPS")
    ax1.set_ylabel("Size (bytes)")
    ax1.set_title("Average first element (x1 / inflight at submit)")
    for i in range(n_rows):
        for j in range(n_cols):
            if not np.isnan(table_first[i, j]):
                ax1.text(j, i, f"{table_first[i, j]:.1f}", ha="center", va="center", fontsize=7, color="white")
    plt.colorbar(im1, ax=ax1, label="avg first")
    plt.tight_layout()
    out1 = SCRIPT_DIR / "table_avg_first_element.pdf"
    fig1.savefig(out1, bbox_inches="tight")
    plt.close(fig1)
    print(f"Saved {out1}")

    # Table 2: average of second element (latency_us)
    fig2, ax2 = plt.subplots(figsize=(max(8, n_cols * 0.5), max(5, n_rows * 0.4)))
    im2 = ax2.imshow(table_second, aspect="auto", cmap="plasma")
    ax2.set_xticks(range(n_cols))
    ax2.set_xticklabels(rps_list, rotation=45, ha="right")
    ax2.set_yticks(range(n_rows))
    ax2.set_yticklabels(sizes)
    ax2.set_xlabel("RPS")
    ax2.set_ylabel("Size (bytes)")
    ax2.set_title("Average second element (latency_us)")
    for i in range(n_rows):
        for j in range(n_cols):
            if not np.isnan(table_second[i, j]):
                ax2.text(j, i, f"{table_second[i, j]:.0f}", ha="center", va="center", fontsize=7, color="white")
    plt.colorbar(im2, ax=ax2, label="avg latency (us)")
    plt.tight_layout()
    out2 = SCRIPT_DIR / "table_avg_second_element.pdf"
    fig2.savefig(out2, bbox_inches="tight")
    plt.close(fig2)
    print(f"Saved {out2}")


if __name__ == "__main__":
    main()
