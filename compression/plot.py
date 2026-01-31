import re
import matplotlib.pyplot as plt

with open("latency.txt") as f:
    text = f.read()

# Format: [x1=inflight_at_submit, x2=inflight_at_return, task_size_bytes->y]
pts = re.findall(r"\[(\d+),(\d+),(\d+)->(\d+)\]", text)
x1 = [int(p[0]) for p in pts]
y = [int(p[3]) for p in pts]

# just plot x1 -> y
plt.figure(figsize=(5, 4))
plt.scatter(x1, y, s=10, alpha=0.5)
plt.xlabel("inflight_at_submit (x1)")
plt.ylabel("latency_us (y)")
plt.title(f"x1 → y (n={len(x1)})")
plt.tight_layout()
plt.savefig("latency.pdf")
plt.close()

