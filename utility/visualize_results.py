#!/usr/bin/env python3
"""
visualize_results.py
--------------------
Reads the "extended_results.txt" file produced by run_extended_experiments.sh,
extracts the detailed misprediction-rate data, and generates multiple plots:

  1) GShare sweep: line chart of misprediction rate vs. ghistoryBits=8..16
     with lines for each trace + an extra line for the average.

  2) Tournament sweep: 3D parameter combos (gh, lh, pc) for gh=9..14, lh=9..12, pc=9..12.
     We'll produce a 2D heatmap for each pc, with GH on x-axis, LH on y-axis,
     color-coded by average misprediction rate across the six traces.
     We put four subplots (pc=9,10,11,12) into a single figure.

Plots are saved into a ./visualizations/ folder.
"""

import sys
import os
import re
import numpy as np
import matplotlib
matplotlib.use("Agg")  # Force matplotlib to NOT pop up a window; we only save figures
import matplotlib.pyplot as plt


def parse_extended_results(filename: str):
    """
    Parses extended_results.txt and returns a nested dictionary structure:
      data["GSHARE"][ghistoryBits][trace_name]      = misprediction_rate (float)
      data["TOURNAMENT"][(gh, lh, pc)][trace_name]  = misprediction_rate (float)
      data["CUSTOM"][trace_name]                    = misprediction_rate (float)

    Where each `trace_name` is one of: {int_1, int_2, fp_1, fp_2, mm_1, mm_2}.

    Even though we store CUSTOM data, we will not plot it in this version.
    """

    data = {
        "GSHARE": {},
        "TOURNAMENT": {},
        "CUSTOM": {}
    }

    current_mode = None    # "GSHARE" or "TOURNAMENT" or "CUSTOM"
    current_config = None
    current_trace = None

    with open(filename, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()

            # 1) Detect GSHARE lines: "GSHARE - ghistoryBits=8"
            if line.startswith("GSHARE - ghistoryBits="):
                match = re.search(r'ghistoryBits=(\d+)', line)
                if match:
                    ghist_bits = int(match.group(1))
                    current_mode = "GSHARE"
                    current_config = ghist_bits
                    # Ensure dictionary slot is ready
                    if ghist_bits not in data["GSHARE"]:
                        data["GSHARE"][ghist_bits] = {}
                continue

            # 2) Detect TOURNAMENT lines: "TOURNAMENT - gh=9, lh=9, pc=9"
            elif line.startswith("TOURNAMENT - gh="):
                match = re.search(r'gh=(\d+), lh=(\d+), pc=(\d+)', line)
                if match:
                    gh = int(match.group(1))
                    lh = int(match.group(2))
                    pc = int(match.group(3))
                    current_mode = "TOURNAMENT"
                    current_config = (gh, lh, pc)
                    if current_config not in data["TOURNAMENT"]:
                        data["TOURNAMENT"][current_config] = {}
                continue

            # 3) Detect CUSTOM lines: "CUSTOM TAGE tests..."
            elif "CUSTOM TAGE tests" in line:
                current_mode = "CUSTOM"
                current_config = None
                continue

            # 4) Detect lines like "Trace: int_1"
            elif line.startswith("Trace:"):
                parts = line.split(":", 1)
                if len(parts) == 2:
                    current_trace = parts[1].strip()
                else:
                    current_trace = None
                continue

            # 5) Detect lines with "Misprediction Rate: ###"
            elif "Misprediction Rate:" in line:
                match = re.search(r'Misprediction Rate:\s+([\d\.]+)', line)
                if match:
                    rate = float(match.group(1))
                    if current_mode == "GSHARE" and current_trace is not None:
                        data["GSHARE"][current_config][current_trace] = rate
                    elif current_mode == "TOURNAMENT" and current_trace is not None:
                        data["TOURNAMENT"][current_config][current_trace] = rate
                    elif current_mode == "CUSTOM" and current_trace is not None:
                        data["CUSTOM"][current_trace] = rate

                continue

            # Ignore other lines

    return data


def make_visualizations(data, out_dir="../visualizations"):
    """
    Generates and saves plots into `out_dir` for GSHARE and TOURNAMENT data only.
    """

    os.makedirs(out_dir, exist_ok=True)

    # 1) GSHARE Sweep
    plot_gshare_sweep(data["GSHARE"], out_dir)

    # 2) Tournament Heatmap
    plot_tournament_heatmap(data["TOURNAMENT"], out_dir)


def plot_gshare_sweep(gshare_data, out_dir):
    """
    gshare_data is of the form:
      {
         8: { "int_1": rate, "int_2": rate, ..., "mm_2": rate },
         9: {...},
         ...
         16:{...}
      }

    Produces a line plot with x = ghistoryBits, y = misprediction rate for each trace + average.
    """
    if not gshare_data:
        print("No GSHARE data found to plot.")
        return

    trace_names = ["int_1", "int_2", "fp_1", "fp_2", "mm_1", "mm_2"]
    sorted_bits = sorted(gshare_data.keys())

    rates_by_trace = {t: [] for t in trace_names}
    avg_rates = []

    for gb in sorted_bits:
        rates_for_this_bit = gshare_data[gb]
        sum_ = 0.0
        count_ = 0
        for t in trace_names:
            val = rates_for_this_bit.get(t, np.nan)
            rates_by_trace[t].append(val)
            if not np.isnan(val):
                sum_ += val
                count_ += 1
        avg_rates.append(sum_ / count_ if count_ else np.nan)

    # Plot
    plt.figure(figsize=(8, 5))
    xvals = sorted_bits

    for t in trace_names:
        plt.plot(xvals, rates_by_trace[t], marker='o', label=t)
    plt.plot(xvals, avg_rates, marker='o', color='k', linestyle='--', label='Average')

    plt.title('GShare Misprediction Rate vs. History Bits')
    plt.xlabel('GShare History Bits')
    plt.ylabel('Misprediction Rate (%)')
    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    outpath = os.path.join(out_dir, "gshare_sweep.png")
    plt.savefig(outpath, dpi=150)
    plt.close()
    print(f"GShare sweep figure saved to {outpath}")


def plot_tournament_heatmap(tournament_data, out_dir):
    """
    tournament_data is a dict:
      {
         (gh, lh, pc): { "int_1": rate, "int_2": rate, ..., "mm_2": rate },
         ...
      }

    Builds a heatmap subplot for each distinct pc, with GH on x-axis, LH on y-axis,
    color-coded by the average misprediction rate across the standard 6 traces.
    """
    if not tournament_data:
        print("No TOURNAMENT data found to plot.")
        return

    param_keys = list(tournament_data.keys())
    gh_values = sorted(set(k[0] for k in param_keys))
    lh_values = sorted(set(k[1] for k in param_keys))
    pc_values = sorted(set(k[2] for k in param_keys))

    gh_index = {gh: i for i, gh in enumerate(gh_values)}
    lh_index = {lh: j for j, lh in enumerate(lh_values)}

    import math
    num_pcs = len(pc_values)
    ncols = 2 if num_pcs > 1 else 1
    nrows = math.ceil(num_pcs / ncols)

    fig, axes = plt.subplots(nrows=nrows, ncols=ncols,
                             figsize=(6*ncols, 5*nrows),
                             constrained_layout=True)
    axes = np.array(axes).reshape(nrows, ncols)

    config_avg = {}
    all_averages = []
    for (gh, lh, pc) in tournament_data:
        rates = list(tournament_data[(gh, lh, pc)].values())
        avg_rate = np.mean(rates)
        config_avg[(gh, lh, pc)] = avg_rate
        all_averages.append(avg_rate)

    vmin = min(all_averages) if all_averages else 0
    vmax = max(all_averages) if all_averages else 1

    row_idx, col_idx = 0, 0
    for p in pc_values:
        ax = axes[row_idx, col_idx]
        matrix = np.full((len(lh_values), len(gh_values)), np.nan)
        for gh in gh_values:
            for lh in lh_values:
                if (gh, lh, p) in config_avg:
                    matrix[lh_index[lh], gh_index[gh]] = config_avg[(gh, lh, p)]

        # Show heatmap
        im = ax.imshow(matrix, origin="upper", cmap="viridis",
                       vmin=vmin, vmax=vmax, aspect="auto",
                       extent=(min(gh_values)-0.5, max(gh_values)+0.5,
                               max(lh_values)+0.5, min(lh_values)-0.5))
        ax.set_title(f"pc = {p}")
        ax.set_xlabel("GH bits")
        ax.set_ylabel("LH bits")
        ax.set_xticks(gh_values)
        ax.set_yticks(lh_values)

        col_idx += 1
        if col_idx >= ncols:
            row_idx += 1
            col_idx = 0

    cbar = fig.colorbar(im, ax=axes.ravel().tolist(), shrink=0.8)
    cbar.set_label("Avg. Misprediction Rate (%)")

    plt.suptitle("Tournament Predictor: GH vs. LH for different PC bits\n(Average Misprediction Rate)")
    outpath = os.path.join(out_dir, "tournament_heatmap.png")
    plt.savefig(outpath, dpi=150)
    plt.close()
    print(f"Tournament heatmap figure saved to {outpath}")


def main():
    filename = "../src/extended_results.txt"
    if len(sys.argv) > 1:
        filename = sys.argv[1]

    data = parse_extended_results(filename)
    make_visualizations(data, out_dir="../visualizations")


if __name__ == "__main__":
    main()
