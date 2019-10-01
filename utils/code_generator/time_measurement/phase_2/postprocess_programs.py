"""
Fuse the files generated by the execution jobs to a single file.

The final file will be a list, where each entry has the following format :

(function_id, schedule_id, list of execution times, the median, the speedup)
"""

import pickle
import numpy as np
from pathlib import Path

# Path to the files generated by the execution jobs
src_path = Path("results/parts/")

# Path to the file that will contain all the execution files
dst_path = "results/final_exec_times.pickle"

final_exec_times = []

# Fuse all execution times to a single list
for file_path in src_path.iterdir():
    if file_path.name.startswith("final_exec_times"):
        with open(file_path, "rb") as f:
            final_exec_times.extend(pickle.load(f))
            
# Compute the medians
final_exec_times_median = []

for x in final_exec_times:
    func_id, sched_id, e = x
    final_exec_times_median.append((func_id, sched_id, e, np.median(e)))
    
# Compute the speedups
ref_progs = dict()

for x in final_exec_times_median:
    func_id, sched_id, _, median = x
    if sched_id.endswith("no_schedule"):
        ref_progs[func_id] = median
        
final_exec_times_median_speedup = []
for x in final_exec_times_median:
    func_id, sched_id, e, median = x
    speedup = float(ref_progs[func_id] / median)
    
    final_exec_times_median_speedup.append((func_id, sched_id, e, median, speedup))

# Save results
with open(dst_path, "wb") as f:
    pickle.dump(final_exec_times_median_speedup, f)
