import ast
from collections import defaultdict
import matplotlib.pyplot as plt


def process_file(filename):
    result = {
        "0.500000": defaultdict(list),
        "0.900000": defaultdict(list),
        "0.990625": defaultdict(list),
    }
    with open(filename) as f:
        for line in f:
            line = line.strip()
            if not line or not line.startswith("RPS:"):
                continue
            try:
                parts = line.split(", tail_lat: ")
                rps_part = parts[0]
                lat_part = parts[1]
                rps = int(rps_part.split(":")[1].strip())
                lat_dict = ast.literal_eval(lat_part)
                for key in ["0.500000", "0.900000", "0.990625"]:
                    result[key][rps].append(float(lat_dict[key]))
            except Exception:
                continue
    # Average (optionally trim 2 extremes like get_avg_latency.py)
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


files = {
    "No Offloading": "tail_c0s0.txt",
    "Only SERenaDE": "tail_c0s1.txt",
    "Only Compression Offloading": "tail_c1s0.txt",
    "Offloading Both": "tail_c1s1.txt",
}

colors = {
    "No Offloading": "#1f77b4",
    "Only SERenaDE": "#ff7f0e",
    "Only Compression Offloading": "#2ca02c",
    "Offloading Both": "#d62728",
}

key = "0.990625"  # p99
plt.figure(figsize=(12, 5))

for label, path in files.items():
    res = process_file(path)
    rps_sorted = sorted(res[key].keys())
    plt.plot(rps_sorted, [res[key][rps] for rps in rps_sorted], marker='o', label=label, color=colors.get(label, None))

plt.xlabel("RPS", fontsize=22)
plt.ylabel("Latency (p99)", fontsize=22)
plt.title("Latency vs RPS @ p99", fontsize=22)
plt.xticks(fontsize=22)
plt.yticks(fontsize=22)
plt.legend(loc='upper left', fontsize=22)
plt.grid(True)
plt.tight_layout()
plt.ylim(bottom=0)
plt.savefig("latency_p99_four.pdf")
print("Plot saved as latency_p99_four.pdf")
