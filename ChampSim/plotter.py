import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os

# Load CSV
df = pd.read_csv("temp.csv")

# Replace 'N/A' with NaN and convert columns to numeric
df['L1I_MPKI'] = pd.to_numeric(df['L1I_MPKI'], errors='coerce')
df['Branch_MPKI'] = pd.to_numeric(df['Branch_MPKI'], errors='coerce')

# Get unique traces and prefetchers
traces = df['Trace'].unique()
prefetchers = df['Prefetcher'].unique()

# Create output directory for plots
os.makedirs("plots", exist_ok=True)

# Function to plot and save a metric
def plot_metric(metric, ylabel):
    x = np.arange(len(traces))  # label locations
    width = 0.25  # width of bars

    fig, ax = plt.subplots(figsize=(12, 6))

    for i, prefetcher in enumerate(prefetchers):
        values = []
        for trace in traces:
            val = df[(df['Trace'] == trace) & (df['Prefetcher'] == prefetcher)][metric].values
            values.append(val[0] if len(val) > 0 else np.nan)
        ax.bar(x + i * width, values, width, label=prefetcher)

    ax.set_xticks(x + width * (len(prefetchers) - 1) / 2)
    ax.set_xticklabels(traces, rotation=45, ha='right')
    ax.set_ylabel(ylabel)
    ax.set_title(f'{ylabel} for Different Traces and Prefetchers')
    ax.legend()
    plt.tight_layout()

    # Save plot
    output_path = os.path.join("plots", f"{metric}.png")
    plt.savefig(output_path)
    print(f"✅ Saved: {output_path}")
    plt.close()

# Plot and save Branch_MPKI
plot_metric('Branch_MPKI', 'Branch MPKI')

# Plot and save L1I_MPKI
plot_metric('L1I_MPKI', 'L1I MPKI')
