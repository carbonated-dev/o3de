"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

# This script creates vulkan_timeline.png file with the visual representation of how GPU and CPU threads work
# Data for profiling is put into output log when "r_logGPUStats N" console command is performed

import re
import matplotlib.pyplot as plt
from collections import defaultdict
import sys

def parse_log_file(filepath):
    frame_data_gpu = defaultdict(list)
    frame_data_cpu = defaultdict(dict)
    frame_start_times = {}

    begin_frame_pat = re.compile(r"\(GPU/CPU\) - Begin Frame at ([+-]?\d*\.?\d+)\. Frame num=(\d+)")
    gpu_pat = re.compile(r"\(GPU/CPU\) - frame (\d+), commit ([+-]?\d*\.?\d+), begin: ([+-]?\d*\.?\d+), end: ([+-]?\d*\.?\d+)")
    cpu_pat = re.compile(r"\(GPU/CPU\) - frame=(\d+), threadName=([^,]+), lastFrameTime=([+-]?\d*\.?\d+)")

    with open(filepath, 'r') as f:
        for line in f:
            m = begin_frame_pat.search(line)
            if m:
                t = float(m.group(1))
                fnum = int(m.group(2))
                frame_start_times[fnum] = t
                continue

            m = gpu_pat.search(line)
            if m:
                fnum = int(m.group(1))
                commit = float(m.group(2))
                begin = float(m.group(3))
                end = float(m.group(4))
                # Skip too long submissions
                if end - begin < 0.1:
                    frame_data_gpu[fnum].append({'commit': commit, 'begin': begin, 'end': end})
                continue

            m = cpu_pat.search(line)
            if m:
                fnum = int(m.group(1))
                name = m.group(2).strip()
                dur = float(m.group(3))
                frame_data_cpu[fnum][name] = dur

    return frame_data_gpu, frame_data_cpu, frame_start_times

def get_cpu_color(name):
    if name == "Main":
        return 'tab:blue'
    elif name in ("GPU", "*GPUendMax", "*GPUwaitAvg"):
        return 'lightgreen'
    else:
        return 'gold'

def plot_timeline(frame_data_gpu, frame_data_cpu, frame_start_times, output_path='vulkan_timeline.png'):
    if not frame_data_gpu and not frame_data_cpu:
        print("No GPU or CPU data found.")
        return

    y_step = 0.15

    # === GPU ===
    all_gpu = []
    max_gpu_per_frame = 0
    for frame, subs in frame_data_gpu.items():
        sorted_subs = sorted(subs, key=lambda x: x['commit'])
        max_gpu_per_frame = max(max_gpu_per_frame, len(sorted_subs))
        for idx, sub in enumerate(sorted_subs):
            all_gpu.append({
                'frame': frame,
                'commit': sub['commit'],
                'begin': sub['begin'],
                'end': sub['end'],
                'y': idx * y_step
            })

    # === CPU ===
    all_cpu_threads = set()
    for d in frame_data_cpu.values():
        all_cpu_threads.update(d.keys())
    all_cpu_threads = sorted(all_cpu_threads)

    cpu_y_offset = - (len(all_cpu_threads) * y_step + y_step)
    gpu_y_offset = 0.0

    all_cpu = []
    for frame, start_time in frame_start_times.items():
        if frame in frame_data_cpu:
            for name, duration in frame_data_cpu[frame].items():
                if name in all_cpu_threads:
                    idx = all_cpu_threads.index(name)
                    y = cpu_y_offset + idx * y_step
                    all_cpu.append({
                        'name': name,
                        'x0': start_time,
                        'x1': start_time + duration,
                        'y': y
                    })

    # === Compute frame durations ===
    sorted_frames = sorted(frame_start_times.keys())
    frame_durations = {}
    for i, frame in enumerate(sorted_frames):
        if i + 1 < len(sorted_frames):
            frame_durations[frame] = frame_start_times[sorted_frames[i+1]] - frame_start_times[frame]
        else:
            # Last frame: use average or 0
            if len(sorted_frames) > 1:
                avg = sum(frame_durations[f] for f in sorted_frames[:-1]) / (len(sorted_frames) - 1)
                frame_durations[frame] = avg
            else:
                frame_durations[frame] = 0.0

    # === Plot ===
    total_height = max(4.0, (max_gpu_per_frame + len(all_cpu_threads) + 2) * y_step * 2)
    fig, ax = plt.subplots(figsize=(16, total_height))

    # --- GPU ---
    for item in all_gpu:
        ax.hlines(item['y'], item['begin'], item['end'], color='darkgreen', linewidth=2, alpha=0.9)
        ax.plot(item['commit'], item['y'], 'ko', markersize=2)

    # --- CPU ---
    for item in all_cpu:
        color = get_cpu_color(item['name'])
        ax.hlines(item['y'], item['x0'], item['x1'], color=color, linewidth=2, alpha=0.9)

    # --- Begin Frame lines and annotations ---
    for frame in sorted_frames:
        start_time = frame_start_times[frame]
        duration = frame_durations[frame]

        # Vertical line at frame start
        ax.axvline(x=start_time, color='gray', linestyle='--', alpha=0.7, linewidth=0.9)

        # Top: Frame N
        y_top = (max_gpu_per_frame * y_step) + y_step * 0.8
        ax.text(start_time, y_top, f"Frame {frame}",
                rotation=90, va='bottom', ha='center', fontsize=8, color='black')

        # Middle: duration in ms
        y_mid = (max_gpu_per_frame * y_step + cpu_y_offset) / 2
        dur_ms = duration * 1000
        ax.text(start_time + duration / 2, y_mid, f"{dur_ms:.1f} ms",
                ha='center', va='center', fontsize=7, color='darkred', alpha=0.8)

        # Bottom: start time
        y_bottom = cpu_y_offset - y_step * 1.2
        ax.text(start_time, y_bottom, f"{start_time:.3f}",
                ha='center', va='top', fontsize=7, color='gray')

    # --- Y axis ---
    y_ticks = []
    y_labels = []

    for name in all_cpu_threads:
        idx = all_cpu_threads.index(name)
        y = cpu_y_offset + idx * y_step
        y_ticks.append(y)
        y_labels.append(name)

    for i in range(max_gpu_per_frame):
        y = gpu_y_offset + i * y_step
        y_ticks.append(y)
        y_labels.append(f"GPU #{i}")

    ax.set_yticks(y_ticks)
    ax.set_yticklabels(y_labels, fontsize=8)

    ax.set_xticks([])
    ax.set_xlabel("")

    ax.set_title("GPU Submissions (dark green) and CPU Threads (colored) - Per-Frame View", fontsize=10)
    ax.grid(False, axis='x')
    ax.grid(True, axis='y', linestyle=':', alpha=0.4)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved clean timeline to: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python vulkan_timeline_plot.py <log_file.txt>")
        sys.exit(1)

    log_file = sys.argv[1]
    frame_data_gpu, frame_data_cpu, frame_start_times = parse_log_file(log_file)
    plot_timeline(frame_data_gpu, frame_data_cpu, frame_start_times, output_path='vulkan_timeline.png')