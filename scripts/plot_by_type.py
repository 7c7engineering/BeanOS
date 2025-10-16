#!/usr/bin/env python3
import argparse
import os
import sys

# Use a non-interactive backend for headless environments
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import pandas as pd

TYPE_NAMES = {                     
    0: "Temperature",              
    1: "Pressure",                 
    2: "Altitude",                 
    3: "Acceleration X",           
    4: "Acceleration Y",           
    5: "Acceleration Z",           
    6: "Gyroscope",                
    7: "Battery Voltage",          
    8: "Stage Transition"          
}                                  

def parse_args():
    p = argparse.ArgumentParser(
        description=(
            "Plot time-series per data type from a 3-column CSV: "
            "[time_ms],[type],[value]. Saves one image per type."
        )
    )
    p.add_argument("csv_file", help="Path to input CSV file (comma-separated, no header).")
    p.add_argument(
        "-o", "--outdir",
        default="plots",
        help='Output directory to save plots (default: "plots").'
    )
    p.add_argument(
        "--ext",
        default="png",
        choices=["png", "jpg", "jpeg", "pdf", "svg"],
        help="Image format/extension for saved plots (default: png)."
    )
    p.add_argument(
        "--dpi",
        type=int,
        default=150,
        help="DPI for raster outputs like PNG/JPG (default: 150)."
    )
    p.add_argument(
        "--start-at-zero",
        action="store_true",
        help="Normalize time axis so it starts at 0 ms (relative to first timestamp)."
    )
    p.add_argument(
        "--title-prefix",
        default="Type",
        help='Title prefix for plots (default: "Type").'
    )
    p.add_argument(
        "--separator",
        default=",",
        help="CSV separator (default: ',')."
    )
    p.add_argument(
        "--no-grid",
        action="store_true",
        help="Disable grid on plots."
    )
    return p.parse_args()


def main():
    args = parse_args()

    # Ensure output directory exists
    os.makedirs(args.outdir, exist_ok=True)

    # Read CSV: no header, 3 columns
    try:
        df = pd.read_csv(
            args.csv_file,
            sep=args.separator,
            header=None,
            names=["time_ms", "type", "value"],
            dtype={"time_ms": "int64", "type": "int64", "value": "float64"},
            skiprows=1,
            engine="c"
        )
    except Exception as e:
        print(f"Error reading CSV: {e}", file=sys.stderr)
        sys.exit(1)

    if df.empty:
        print("Input CSV is empty.", file=sys.stderr)
        sys.exit(2)

    # Optionally normalize time to start at zero
    if args.start_at_zero:
        t0 = df["time_ms"].min()
        df["time_ms"] = df["time_ms"] - t0

    # Sort by time for nice lines
    df = df.sort_values(["type", "time_ms"])

    # Group by type and plot each
    unique_types = df["type"].unique()
    saved = []
    for t in unique_types:
        sub = df[df["type"] == t]
        
        # For type 2 (Altitude), ignore 0 values until first non-zero value
        if t == 2:
            # Find the index of the first non-zero value
            non_zero_mask = sub["value"] != 0
            if non_zero_mask.any():
                first_non_zero_idx = sub[non_zero_mask].index[0]
                # Keep all data from the first non-zero value onwards
                sub = sub.loc[first_non_zero_idx:]
            else:
                # If all values are zero, keep the original data
                pass

        # Prepare figure
        plt.figure(figsize=(10, 4.5))
        plt.plot(sub["time_ms"].values, sub["value"].values, linewidth=1.5)
        plt.xlabel("Time (ms)")
        plt.ylabel("Value")
        type_name = TYPE_NAMES.get(t, f"Unknown Type {t}")
        plt.title(f"{type_name}")
        if not args.no_grid:
            plt.grid(True, linestyle="--", alpha=0.4)
        plt.tight_layout()

        # Save
        outfile = os.path.join(args.outdir, f"{type_name.lower().replace(' ', '_')}.{args.ext}")
        try:
            plt.savefig(outfile, dpi=args.dpi if args.ext.lower() in ("png", "jpg", "jpeg") else None)
            saved.append(outfile)
        except Exception as e:
            print(f"Failed to save plot for type {t}: {e}", file=sys.stderr)
        finally:
            plt.close()

    if not saved:
        print("No plots were saved.", file=sys.stderr)
        sys.exit(3)

    print("Saved files:")
    for p in saved:
        print(p)


if __name__ == "__main__":
    main()
