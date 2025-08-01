import ast
from collections import defaultdict
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import numpy as np

def process_file(filename):
    result = {
        "0.500000": defaultdict(list),
        "0.900000": defaultdict(list),
        "0.990625": defaultdict(list)
    }
    with open(filename) as f:
        for line in f:
            if line.startswith("RPS:"):
                parts = line.strip().split(", tail_lat: ")
                rps_part = parts[0]
                lat_part = parts[1]
                rps = int(rps_part.split(":")[1].strip())
                lat_dict = ast.literal_eval(lat_part)
                for key in ["0.500000", "0.900000", "0.990625"]:
                    result[key][rps].append(float(lat_dict[key]))
    # Now process each list: sort, remove largest 2 and smallest 2, then average
    final_result = {}
    for key in result:
        final_result[key] = {}
        for rps, latencies in result[key].items():
            sorted_lats = sorted(latencies)
            if len(sorted_lats) > 4:
                trimmed = sorted_lats[2:-2]
            else:
                trimmed = sorted_lats
            avg = sum(trimmed) / len(trimmed) if trimmed else None
            final_result[key][rps] = avg
    return final_result

# ser1de_improved = "tail_ser1de_improved"

# List of dataset names
names = ["0B"]

labels = {
    "0.500000": "p50",
    "0.900000": "p90",
    "0.990625": "p99"
}

ser1de = "tail_ser1de"
proto = "tail_protobuf"

# Assign colors: blue for protobuf, orange for SERenaDE
proto_color = "#1f77b4"  # Default matplotlib blue
ser1de_color = "#ff7f0e"  # Default matplotlib orange

# Only plot p99 as in your original code, but you can loop over all if needed
for key in ["0.990625"]:
    plt.figure(figsize=(12, 5))
    for idx, name in enumerate(names):
        # Process both files for each dataset
        res1 = process_file(f"{ser1de}_{name}.txt")
        res2 = process_file(f"{proto}_{name}.txt")
        # Sort RPS for consistent plotting
        rps1 = sorted(res1[key].keys())
        rps2 = sorted(res2[key].keys())
        plt.plot(rps2, [res2[key][rps] for rps in rps2], marker='o', color=proto_color, label="Protobuf")
        plt.plot(rps1, [res1[key][rps] for rps in rps1], marker='o', color=ser1de_color, label="SERenaDE")
    plt.xlabel("RPS", fontsize=22)
    plt.ylabel(f"Latency ({labels[key]})", fontsize=22)
    plt.title(f"Latency vs RPS @ {labels[key]}", fontsize=22)
    plt.xticks(fontsize=22)
    plt.yticks(fontsize=22)
    plt.legend(loc='upper left', fontsize=22)
    plt.grid(True)
    plt.tight_layout()
    plt.ylim(bottom=0, top=500)
    plt.savefig(f"latency_{labels[key]}_all.pdf")  # Save the combined plot as a PNG file
    # plt.show()  # Uncomment to display interactively

print("Plot saved as latency_p99_all.png")