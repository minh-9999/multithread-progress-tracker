import json
import os
import pandas as pd

# Directory containing job_summary_{threads}.json files, for example:
# job_summary_1.json, job_summary_2.json, job_summary_4.json, ...
input_dir = "result"

records = []

for filename in os.listdir(input_dir): 
    if filename.startswith("job_summary_") and filename.endswith(".json"): 
        threads = int(filename.split("_")[2].split(".")[0]) # Extract thread number from file name 
        with open(os.path.join(input_dir, filename)) as f: 
            data = json.load(f) 
            avg_latency = data.get("average_latency_ms", -1) 
            records.append({ 
            "threads": threads, 
            "avg_latency_ms": avg_latency 
            })

# Create DataFrame and export CSV
df = pd.DataFrame(records)
df.sort_values("threads", inplace=True)
df.to_csv("avg_latency_result.csv", index=False)

print("✅ Exported avg_latency_result.csv")