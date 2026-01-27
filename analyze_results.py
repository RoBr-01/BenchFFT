#!/usr/bin/env python3
"""
FFT Benchmark Results Analyzer
Visualizes and analyzes FFT benchmark results from CSV output
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import argparse
from pathlib import Path

def load_results(csv_file):
    """Load benchmark results from CSV file"""
    df = pd.read_csv(csv_file)
    return df

def plot_throughput_by_size(df, transform_type=None, output_dir=None):
    """Plot throughput vs FFT size for all libraries"""
    if transform_type:
        df = df[df['Transform'] == transform_type]
        title_suffix = f' - {transform_type}'
    else:
        title_suffix = ' - All Transforms'
    
    plt.figure(figsize=(12, 6))
    
    for lib in df['Library'].unique():
        lib_data = df[df['Library'] == lib]
        for transform in lib_data['Transform'].unique():
            transform_data = lib_data[lib_data['Transform'] == transform]
            label = f"{lib} ({transform})" if not transform_type else lib
            plt.plot(transform_data['Size'], 
                    transform_data['Throughput(Msamp/s)'],
                    marker='o', label=label, linewidth=2)
    
    plt.xlabel('FFT Size', fontsize=12)
    plt.ylabel('Throughput (Msamples/s)', fontsize=12)
    plt.title(f'FFT Library Performance{title_suffix}', fontsize=14, fontweight='bold')
    plt.xscale('log', base=2)
    plt.grid(True, alpha=0.3)
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.tight_layout()
    
    if output_dir:
        filename = f'throughput_by_size{"_" + transform_type.replace(" ", "_") if transform_type else ""}.png'
        plt.savefig(Path(output_dir) / filename, dpi=300, bbox_inches='tight')
        print(f"Saved: {filename}")
    else:
        plt.show()
    
    plt.close()

def plot_latency_by_size(df, transform_type=None, output_dir=None):
    """Plot latency vs FFT size for all libraries"""
    if transform_type:
        df = df[df['Transform'] == transform_type]
        title_suffix = f' - {transform_type}'
    else:
        title_suffix = ' - All Transforms'
    
    plt.figure(figsize=(12, 6))
    
    for lib in df['Library'].unique():
        lib_data = df[df['Library'] == lib]
        for transform in lib_data['Transform'].unique():
            transform_data = lib_data[lib_data['Transform'] == transform]
            label = f"{lib} ({transform})" if not transform_type else lib
            plt.plot(transform_data['Size'], 
                    transform_data['Mean(us)'],
                    marker='o', label=label, linewidth=2)
    
    plt.xlabel('FFT Size', fontsize=12)
    plt.ylabel('Latency (μs)', fontsize=12)
    plt.title(f'FFT Library Latency{title_suffix}', fontsize=14, fontweight='bold')
    plt.xscale('log', base=2)
    plt.yscale('log')
    plt.grid(True, alpha=0.3)
    plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.tight_layout()
    
    if output_dir:
        filename = f'latency_by_size{"_" + transform_type.replace(" ", "_") if transform_type else ""}.png'
        plt.savefig(Path(output_dir) / filename, dpi=300, bbox_inches='tight')
        print(f"Saved: {filename}")
    else:
        plt.show()
    
    plt.close()

def plot_comparison_heatmap(df, metric='Throughput(Msamp/s)', output_dir=None):
    """Create heatmap comparing libraries across sizes"""
    pivot = df.pivot_table(values=metric, 
                           index='Library', 
                           columns='Size', 
                           aggfunc='mean')
    
    plt.figure(figsize=(14, 6))
    sns.heatmap(pivot, annot=True, fmt='.1f', cmap='RdYlGn', 
                cbar_kws={'label': metric})
    plt.title(f'FFT Library Comparison - {metric}', fontsize=14, fontweight='bold')
    plt.xlabel('FFT Size', fontsize=12)
    plt.ylabel('Library', fontsize=12)
    plt.tight_layout()
    
    if output_dir:
        filename = f'heatmap_{metric.replace("(", "").replace(")", "").replace("/", "_")}.png'
        plt.savefig(Path(output_dir) / filename, dpi=300, bbox_inches='tight')
        print(f"Saved: {filename}")
    else:
        plt.show()
    
    plt.close()

def plot_speedup_comparison(df, baseline='FFTW', transform_type=None, output_dir=None):
    """Plot speedup relative to baseline library"""
    if transform_type:
        df = df[df['Transform'] == transform_type]
        title_suffix = f' - {transform_type}'
    else:
        title_suffix = ''
    
    baseline_data = df[df['Library'] == baseline].set_index(['Size', 'Transform'])
    
    plt.figure(figsize=(12, 6))
    
    for lib in df['Library'].unique():
        if lib == baseline:
            continue
        
        lib_data = df[df['Library'] == lib]
        speedups = []
        sizes = []
        
        for _, row in lib_data.iterrows():
            try:
                baseline_throughput = baseline_data.loc[
                    (row['Size'], row['Transform']), 'Throughput(Msamp/s)']
                speedup = row['Throughput(Msamp/s)'] / baseline_throughput
                speedups.append(speedup)
                sizes.append(row['Size'])
            except KeyError:
                continue
        
        if speedups:
            plt.plot(sizes, speedups, marker='o', label=lib, linewidth=2)
    
    plt.axhline(y=1.0, color='black', linestyle='--', alpha=0.5, label=f'{baseline} (baseline)')
    plt.xlabel('FFT Size', fontsize=12)
    plt.ylabel(f'Speedup vs {baseline}', fontsize=12)
    plt.title(f'Relative Performance{title_suffix}', fontsize=14, fontweight='bold')
    plt.xscale('log', base=2)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    
    if output_dir:
        filename = f'speedup_vs_{baseline}{"_" + transform_type.replace(" ", "_") if transform_type else ""}.png'
        plt.savefig(Path(output_dir) / filename, dpi=300, bbox_inches='tight')
        print(f"Saved: {filename}")
    else:
        plt.show()
    
    plt.close()

def print_summary_statistics(df):
    """Print summary statistics"""
    print("\n" + "="*80)
    print("BENCHMARK SUMMARY STATISTICS")
    print("="*80)
    
    for transform in df['Transform'].unique():
        print(f"\n{transform}:")
        print("-" * 80)
        transform_data = df[df['Transform'] == transform]
        
        print("\nFastest library by size:")
        for size in sorted(transform_data['Size'].unique()):
            size_data = transform_data[transform_data['Size'] == size]
            fastest = size_data.loc[size_data['Throughput(Msamp/s)'].idxmax()]
            print(f"  Size {size:5d}: {fastest['Library']:15s} "
                  f"({fastest['Throughput(Msamp/s)']:.2f} Msamp/s)")
        
        print("\nOverall statistics by library:")
        lib_stats = transform_data.groupby('Library').agg({
            'Throughput(Msamp/s)': ['mean', 'std', 'min', 'max'],
            'Mean(us)': ['mean', 'min', 'max']
        }).round(2)
        print(lib_stats)

def generate_report(csv_file, output_dir=None):
    """Generate complete analysis report"""
    df = load_results(csv_file)
    
    print(f"\nLoaded {len(df)} benchmark results")
    print(f"Libraries tested: {', '.join(df['Library'].unique())}")
    print(f"Transform types: {', '.join(df['Transform'].unique())}")
    print(f"Sizes tested: {sorted(df['Size'].unique())}")
    
    if output_dir:
        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)
        print(f"\nGenerating plots in: {output_path}")
    
    # Generate all plots
    plot_throughput_by_size(df, output_dir=output_dir)
    plot_latency_by_size(df, output_dir=output_dir)
    
    for transform in df['Transform'].unique():
        plot_throughput_by_size(df, transform_type=transform, output_dir=output_dir)
        plot_latency_by_size(df, transform_type=transform, output_dir=output_dir)
    
    plot_comparison_heatmap(df, metric='Throughput(Msamp/s)', output_dir=output_dir)
    plot_comparison_heatmap(df, metric='Mean(us)', output_dir=output_dir)
    
    if 'FFTW' in df['Library'].unique():
        plot_speedup_comparison(df, baseline='FFTW', output_dir=output_dir)
    
    print_summary_statistics(df)
    
    print("\n" + "="*80)
    print("Analysis complete!")
    print("="*80)

def main():
    parser = argparse.ArgumentParser(
        description='Analyze FFT benchmark results',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s fft_benchmark_results.csv
  %(prog)s results.csv -o plots/
  %(prog)s results.csv --no-plots
        """
    )
    
    parser.add_argument('csv_file', 
                       help='Path to CSV file with benchmark results')
    parser.add_argument('-o', '--output-dir', 
                       help='Directory to save plots (shows interactively if not specified)')
    parser.add_argument('--no-plots', action='store_true',
                       help='Only print statistics, skip plots')
    
    args = parser.parse_args()
    
    if not Path(args.csv_file).exists():
        print(f"Error: File not found: {args.csv_file}")
        sys.exit(1)
    
    if args.no_plots:
        df = load_results(args.csv_file)
        print_summary_statistics(df)
    else:
        generate_report(args.csv_file, args.output_dir)

if __name__ == '__main__':
    main()