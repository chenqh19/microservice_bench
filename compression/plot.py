import re
import matplotlib.pyplot as plt

with open("latency.txt") as f:
    text = f.read()

# Format: [ x1 , latency_us ] (x1=inflight_at_submit, second=latency_us)
pts = re.findall(r"\[\s*(\d+)\s*,\s*(\d+)\s*\]", text)
x1 = [int(p[0]) for p in pts]
y = [int(p[1]) for p in pts]

if not x1:
    print("No data points found in latency.txt")
    exit(1)

# Scatter: x1 vs y
plt.figure(figsize=(5, 4))
plt.scatter(x1, y, s=10, alpha=0.5)
plt.xlabel("inflight_at_submit (x1)")
plt.ylabel("latency_us (y)")
plt.title(f"x1 → y (n={len(x1)})")
plt.tight_layout()
plt.savefig("latency.pdf")
plt.close()
print(f"Saved latency.pdf with {len(x1)} points")

# Line plot: id on x-axis, y1 (first element) on left y-axis, y2 (second element) on right y-axis
ids = list(range(len(x1)))
fig, ax1 = plt.subplots(figsize=(8, 4))
ax1.plot(ids, x1, color="C0", alpha=0.8, label="x1 (inflight_at_submit)")
ax1.set_xlabel("id")
ax1.set_ylabel("x1 (inflight_at_submit)", color="C0")
ax1.tick_params(axis="y", labelcolor="C0")
ax1.set_ylim(bottom=0)

ax2 = ax1.twinx()
ax2.plot(ids, y, color="C1", alpha=0.8, label="latency_us")
ax2.set_ylabel("latency_us (y)", color="C1")
ax2.tick_params(axis="y", labelcolor="C1")
ax2.set_ylim(bottom=0)

plt.title(f"id → x1, latency_us (n={len(x1)})")
fig.tight_layout()
plt.savefig("latency_lines.pdf")
plt.close()
print(f"Saved latency_lines.pdf with {len(x1)} points")
