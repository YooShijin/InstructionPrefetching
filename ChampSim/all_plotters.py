#!/usr/bin/env python3
"""
Comprehensive plotting script for all performance metrics
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def load_data(csv_file):
    """Load results from CSV file"""
    try:
        df = pd.read_csv(csv_file)
        # Convert numeric columns
        for col in ['IPC', 'Branch_MPKI', 'L1I_MPKI']:
            df[col] = pd.to_numeric(df[col], errors='coerce')
        return df.dropna()
    except Exception as e:
        print(f"Error loading CSV: {e}")
        sys.exit(1)

def create_comprehensive_plots(df, output_dir):
    """Create comprehensive performance comparison plots"""
    
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    predictors = sorted(df['Predictor'].unique())
    traces = sorted(df['Trace'].unique())
    colors = ['#2E86AB', '#A23B72', '#F18F01', '#C73E1D', '#6A994E']
    
    # Plot 1: IPC Comparison
    fig, ax = plt.subplots(figsize=(14, 6))
    x = np.arange(len(traces))
    width = 0.8 / len(predictors)
    
    for i, predictor in enumerate(predictors):
        predictor_data = df[df['Predictor'] == predictor]
        ipc_values = [
            predictor_data[predictor_data['Trace'] == trace]['IPC'].values[0] 
            if len(predictor_data[predictor_data['Trace'] == trace]) > 0 
            else 0 
            for trace in traces
        ]
        
        offset = width * (i - len(predictors)/2 + 0.5)
        ax.bar(x + offset, ipc_values, width, 
               label=predictor.upper(), color=colors[i % len(colors)],
               alpha=0.8, edgecolor='black', linewidth=0.5)
    
    ax.set_xlabel('Trace', fontsize=12, fontweight='bold')
    ax.set_ylabel('IPC (Instructions Per Cycle)', fontsize=12, fontweight='bold')
    ax.set_title('IPC Comparison', fontsize=14, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right')
    ax.legend(loc='upper left')
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path / 'ipc_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 2: Branch MPKI Comparison
    fig, ax = plt.subplots(figsize=(14, 6))
    
    for i, predictor in enumerate(predictors):
        predictor_data = df[df['Predictor'] == predictor]
        mpki_values = [
            predictor_data[predictor_data['Trace'] == trace]['Branch_MPKI'].values[0] 
            if len(predictor_data[predictor_data['Trace'] == trace]) > 0 
            else 0 
            for trace in traces
        ]
        
        offset = width * (i - len(predictors)/2 + 0.5)
        ax.bar(x + offset, mpki_values, width, 
               label=predictor.upper(), color=colors[i % len(colors)],
               alpha=0.8, edgecolor='black', linewidth=0.5)
    
    ax.set_xlabel('Trace', fontsize=12, fontweight='bold')
    ax.set_ylabel('Branch MPKI', fontsize=12, fontweight='bold')
    ax.set_title('Branch Misprediction Rate Comparison', fontsize=14, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(traces, rotation=45, ha='right')
    ax.legend(loc='upper left')
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path / 'branch_mpki_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 3: Multi-metric dashboard
    fig = plt.figure(figsize=(16, 10))
    gs = fig.add_gridspec(2, 2, hspace=0.3, wspace=0.3)
    
    # IPC Average
    ax1 = fig.add_subplot(gs[0, 0])
    avg_ipc = df.groupby('Predictor')['IPC'].mean().sort_values(ascending=False)
    bars = ax1.bar(range(len(avg_ipc)), avg_ipc.values, 
                   color=[colors[i % len(colors)] for i in range(len(avg_ipc))],
                   alpha=0.8, edgecolor='black')
    ax1.set_xticks(range(len(avg_ipc)))
    ax1.set_xticklabels([p.upper() for p in avg_ipc.index], fontweight='bold')
    ax1.set_ylabel('Average IPC', fontweight='bold')
    ax1.set_title('Average IPC (Higher is Better)', fontweight='bold')
    ax1.grid(axis='y', alpha=0.3)
    for bar, val in zip(bars, avg_ipc.values):
        ax1.text(bar.get_x() + bar.get_width()/2, val, f'{val:.3f}',
                ha='center', va='bottom', fontweight='bold')
    
    # Branch MPKI Average
    ax2 = fig.add_subplot(gs[0, 1])
    avg_branch_mpki = df.groupby('Predictor')['Branch_MPKI'].mean().sort_values()
    bars = ax2.bar(range(len(avg_branch_mpki)), avg_branch_mpki.values, 
                   color=[colors[i % len(colors)] for i in range(len(avg_branch_mpki))],
                   alpha=0.8, edgecolor='black')
    ax2.set_xticks(range(len(avg_branch_mpki)))
    ax2.set_xticklabels([p.upper() for p in avg_branch_mpki.index], fontweight='bold')
    ax2.set_ylabel('Average Branch MPKI', fontweight='bold')
    ax2.set_title('Average Branch MPKI (Lower is Better)', fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)
    for bar, val in zip(bars, avg_branch_mpki.values):
        ax2.text(bar.get_x() + bar.get_width()/2, val, f'{val:.2f}',
                ha='center', va='bottom', fontweight='bold')
    
    # L1I MPKI Average
    ax3 = fig.add_subplot(gs[1, 0])
    avg_l1i_mpki = df.groupby('Predictor')['L1I_MPKI'].mean().sort_values()
    bars = ax3.bar(range(len(avg_l1i_mpki)), avg_l1i_mpki.values, 
                   color=[colors[i % len(colors)] for i in range(len(avg_l1i_mpki))],
                   alpha=0.8, edgecolor='black')
    ax3.set_xticks(range(len(avg_l1i_mpki)))
    ax3.set_xticklabels([p.upper() for p in avg_l1i_mpki.index], fontweight='bold')
    ax3.set_ylabel('Average L1I MPKI', fontweight='bold')
    ax3.set_title('Average L1I MPKI (Lower is Better)', fontweight='bold')
    ax3.grid(axis='y', alpha=0.3)
    for bar, val in zip(bars, avg_l1i_mpki.values):
        ax3.text(bar.get_x() + bar.get_width()/2, val, f'{val:.2f}',
                ha='center', va='bottom', fontweight='bold')
    
    # Normalized performance (IPC normalized to baseline)
    ax4 = fig.add_subplot(gs[1, 1])
    baseline = predictors[0]  # Use first predictor as baseline
    normalized_ipc = {}
    for predictor in predictors:
        pred_data = df[df['Predictor'] == predictor]['IPC'].values
        base_data = df[df['Predictor'] == baseline]['IPC'].values
        if len(base_data) > 0 and len(pred_data) > 0:
            norm = np.mean(pred_data) / np.mean(base_data) * 100
            normalized_ipc[predictor] = norm
    
    if normalized_ipc:
        bars = ax4.bar(range(len(normalized_ipc)), list(normalized_ipc.values()),
                      color=[colors[i % len(colors)] for i in range(len(normalized_ipc))],
                      alpha=0.8, edgecolor='black')
        ax4.axhline(y=100, color='red', linestyle='--', linewidth=2, label=f'Baseline ({baseline.upper()})')
        ax4.set_xticks(range(len(normalized_ipc)))
        ax4.set_xticklabels([p.upper() for p in normalized_ipc.keys()], fontweight='bold')
        ax4.set_ylabel('Normalized IPC (%)', fontweight='bold')
        ax4.set_title(f'Normalized IPC (Baseline: {baseline.upper()})', fontweight='bold')
        ax4.grid(axis='y', alpha=0.3)
        ax4.legend()
        for bar, val in zip(bars, normalized_ipc.values()):
            ax4.text(bar.get_x() + bar.get_width()/2, val, f'{val:.1f}%',
                    ha='center', va='bottom', fontweight='bold')
    
    plt.suptitle('Performance Metrics Dashboard', fontsize=16, fontweight='bold', y=0.995)
    plt.savefig(output_path / 'performance_dashboard.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"✓ Saved: {output_path / 'ipc_comparison.png'}")
    print(f"✓ Saved: {output_path / 'branch_mpki_comparison.png'}")
    print(f"✓ Saved: {output_path / 'performance_dashboard.png'}")

def generate_summary_report(df, output_dir):
    """Generate comprehensive text summary"""
    output_path = Path(output_dir)
    
    report = []
    report.append("="*80)
    report.append("COMPREHENSIVE PERFORMANCE ANALYSIS REPORT")
    report.append("="*80)
    report.append("")
    
    # Overall statistics
    report.append("OVERALL STATISTICS")
    report.append("-"*80)
    for metric in ['IPC', 'Branch_MPKI', 'L1I_MPKI']:
        report.append(f"\n{metric}:")
        stats = df.groupby('Predictor')[metric].agg(['mean', 'min', 'max', 'std'])
        report.append(stats.to_string())
    
    report.append("\n" + "="*80)
    report.append("PREDICTOR RANKINGS")
    report.append("-"*80)
    
    # IPC Ranking (higher is better)
    report.append("\nIPC Ranking (Higher is Better):")
    ipc_rank = df.groupby('Predictor')['IPC'].mean().sort_values(ascending=False)
    for i, (pred, val) in enumerate(ipc_rank.items(), 1):
        report.append(f"  {i}. {pred.upper():10s} - {val:.4f}")
    
    # Branch MPKI Ranking (lower is better)
    report.append("\nBranch MPKI Ranking (Lower is Better):")
    branch_rank = df.groupby('Predictor')['Branch_MPKI'].mean().sort_values()
    for i, (pred, val) in enumerate(branch_rank.items(), 1):
        report.append(f"  {i}. {pred.upper():10s} - {val:.4f}")
    
    # L1I MPKI Ranking (lower is better)
    report.append("\nL1I MPKI Ranking (Lower is Better):")
    l1i_rank = df.groupby('Predictor')['L1I_MPKI'].mean().sort_values()
    for i, (pred, val) in enumerate(l1i_rank.items(), 1):
        report.append(f"  {i}. {pred.upper():10s} - {val:.4f}")
    
    report.append("\n" + "="*80)
    report.append("DETAILED RESULTS BY TRACE")
    report.append("-"*80)
    report.append("")
    
    for trace in sorted(df['Trace'].unique()):
        report.append(f"\nTrace: {trace}")
        report.append("-"*40)
        trace_data = df[df['Trace'] == trace][['Predictor', 'IPC', 'Branch_MPKI', 'L1I_MPKI']]
        report.append(trace_data.to_string(index=False))
    
    report.append("\n" + "="*80)
    
    # Save report
    report_file = output_path / 'comprehensive_report.txt'
    with open(report_file, 'w') as f:
        f.write('\n'.join(report))
    
    print(f"✓ Saved: {report_file}")
    
    # Print to console
    print("\n" + '\n'.join(report))

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_all_metrics.py <summary.csv> [output_dir]")
        sys.exit(1)
    
    csv_file = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else str(Path(csv_file).parent / "plots")
    
    print("="*80)
    print("COMPREHENSIVE METRICS PLOTTING TOOL")
    print("="*80)
    
    df = load_data(csv_file)
    create_comprehensive_plots(df, output_dir)
    generate_summary_report(df, output_dir)
    
    print("\n" + "="*80)
    print("✓ All analysis complete!")
    print("="*80)

if __name__ == "__main__":
    main()