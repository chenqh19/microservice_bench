import ast
from collections import defaultdict
import matplotlib.pyplot as plt

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

ser1de = "tail_ser1de"
proto = "tail_protobuf"
ser1de_improved = "tail_ser1de_improved"

# Process all three files
res1 = process_file(ser1de+'.txt')
res2 = process_file(proto+'.txt')
res3 = process_file(ser1de_improved+'.txt')

labels = {
    "0.500000": "p50",
    "0.900000": "p90",
    "0.990625": "p99"
}

for key in ["0.500000", "0.900000", "0.990625"]:
    plt.figure()
    # Sort RPS for consistent plotting
    rps1 = sorted(res1[key].keys())
    rps2 = sorted(res2[key].keys())
    rps3 = sorted(res3[key].keys())
    plt.plot(rps1, [res1[key][rps] for rps in rps1], marker='o', label=ser1de)
    plt.plot(rps2, [res2[key][rps] for rps in rps2], marker='o', label=proto)
    plt.plot(rps3, [res3[key][rps] for rps in rps3], marker='o', label=ser1de_improved)
    plt.xlabel("RPS")
    plt.ylabel(f"Latency ({labels[key]})")
    plt.title(f"Latency vs RPS @ {labels[key]}")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"latency_{labels[key]}.png")  # Save each plot as a PNG file
    # plt.show()  # Uncomment to display interactively

print("Plots saved as latency_p50.png, latency_p90.png, latency_p99.png")