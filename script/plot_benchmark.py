import matplotlib.pyplot as plt
import pandas as pd

# Read data from CSV
df = pd.read_csv("benchmark_result.csv")

# Get data from DataFrame
thread_counts = df["threads"].tolist()
durations_ms = df["duration_ms"].tolist()

# Draw a chart
plt.figure(figsize=(10, 6))
plt.plot(thread_counts, durations_ms, marker='o', linestyle='-', color='dodgerblue')

plt.title("JobDispatcher Benchmark: Duration vs Threads")
plt.xlabel("Number of Threads")
plt.ylabel("Total Duration (ms)")
plt.grid(True)
plt.xticks(thread_counts)
plt.gca().invert_yaxis() # The more threads, the shorter the time => y decreases

# Display data on the chart
for x, y in zip(thread_counts, durations_ms):
    plt.text(x, y + 20, f"{y}ms", ha='center')

plt.tight_layout()
plt.savefig("benchmark_plot.png")
plt.show()