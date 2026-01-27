#!/usr/bin/env python3
"""
FFT Benchmark Results Analyzer - Enhanced Version
Comprehensive visualization and statistical analysis of FFT benchmark results
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import sys
import argparse
from pathlib import Path
from scipy import stats
import warnings
warnings.filterwarnings('ignore')

# Set publication-quality plotting style
plt.style.use('seaborn-v0_8-darkgrid')
sns.set_palette("husl")

class FFTBenchmarkAnalyzer:
    def __init__(self, csv_file):
        self.csv_file = Path(csv_file)
        self.df = None
        self.metadata = {}
        self.load_results()
        
    def load_results(self):
        """Load benchmark results and extract metadata from CSV comments"""
        metadata_lines = []
        
        with open(self.csv_file, 'r') as f:
            for line in f:
                if line.startswith('#'):
                    metadata_lines.append(line[1:].strip())
                else:
                    break
        
        # Parse metadata
        for line in metadata_lines:
            if ':' in line:
                key, value = line.split(':', 1)
                self.metadata[key.strip()] = value.strip()
        
        # Load data
        self.df = pd.read_csv(self.csv_file, comment='#')
        
        # Calculate additional metrics
        self.df['Latency_ns'] = self.df['Mean(us)'] * 1000
        self.df['CV'] = (self.df['StdDev(us)'] / self.df['Mean(us)']) * 100  # Coefficient of variation
        
        print(f"\n{'='*80}")
        print("BENCHMARK METADATA")
        print(f"{'='*80}")
        for key, value in self.metadata.items():
            print(f"  {key:20s}: {value}")
        print(f"{'='*80}\n")
        
    def get_winner_by_category(self):
        """Determine the best library for each size/transform combination"""
        winners = []
        
        for size in sorted(self.df['Size'].unique()):
            for transform in self.df['Transform'].unique():
                subset = self.df[(self.df['Size'] == size) & 
                                (self.df['Transform'] == transform)]
                if len(subset) > 0:
                    best = subset.loc[subset['Throughput(Msamp/s)'].idxmax()]
                    winners.append({
                        'Size': size,
                        'Transform': transform,
                        'Library': best['Library'],
                        'Throughput': best['Throughput(Msamp/s)'],
                        'Latency': best['Mean(us)']
                    })
        
        return pd.DataFrame(winners)
    
    def plot_performance_overview(self, output_dir=None):
        """Create a comprehensive 2x2 subplot overview"""
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('FFT Benchmark Performance Overview', fontsize=16, fontweight='bold', y=0.995)
        
        # 1. Throughput by size (log-log)
        ax = axes[0, 0]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib].groupby('Size')['Throughput(Msamp/s)'].mean()
            ax.plot(lib_data.index, lib_data.values, marker='o', label=lib, linewidth=2, markersize=8)
        ax.set_xlabel('FFT Size', fontsize=11)
        ax.set_ylabel('Throughput (Msamp/s)', fontsize=11)
        ax.set_title('Average Throughput vs Size', fontweight='bold')
        ax.set_xscale('log', base=2)
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        
        # 2. Latency by size (log-log)
        ax = axes[0, 1]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib].groupby('Size')['Mean(us)'].mean()
            ax.plot(lib_data.index, lib_data.values, marker='s', label=lib, linewidth=2, markersize=8)
        ax.set_xlabel('FFT Size', fontsize=11)
        ax.set_ylabel('Latency (μs)', fontsize=11)
        ax.set_title('Average Latency vs Size', fontweight='bold')
        ax.set_xscale('log', base=2)
        ax.set_yscale('log')
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        
        # 3. Variance analysis (coefficient of variation)
        ax = axes[1, 0]
        cv_data = self.df.groupby('Library')['CV'].mean().sort_values()
        colors = sns.color_palette("RdYlGn_r", len(cv_data))
        bars = ax.barh(range(len(cv_data)), cv_data.values, color=colors)
        ax.set_yticks(range(len(cv_data)))
        ax.set_yticklabels(cv_data.index)
        ax.set_xlabel('Coefficient of Variation (%)', fontsize=11)
        ax.set_title('Timing Consistency (lower is better)', fontweight='bold')
        ax.grid(True, alpha=0.3, axis='x')
        for i, (idx, val) in enumerate(cv_data.items()):
            ax.text(val + 0.1, i, f'{val:.2f}%', va='center')
        
        # 4. Winner distribution pie chart
        ax = axes[1, 1]
        winners_df = self.get_winner_by_category()
        winner_counts = winners_df['Library'].value_counts()
        colors_pie = sns.color_palette("pastel", len(winner_counts))
        wedges, texts, autotexts = ax.pie(winner_counts.values, labels=winner_counts.index, 
                                           autopct='%1.1f%%', colors=colors_pie,
                                           startangle=90, textprops={'fontsize': 10})
        ax.set_title('Overall Winner Distribution\n(across all sizes & transforms)', fontweight='bold')
        
        plt.tight_layout()
        
        if output_dir:
            plt.savefig(Path(output_dir) / 'overview.png', dpi=300, bbox_inches='tight')
            print(f"✓ Saved: overview.png")
        else:
            plt.show()
        plt.close()
    
    def plot_transform_comparison(self, output_dir=None):
        """Compare performance across different transform types"""
        transforms = self.df['Transform'].unique()
        n_transforms = len(transforms)
        
        fig, axes = plt.subplots(1, n_transforms, figsize=(6*n_transforms, 5))
        if n_transforms == 1:
            axes = [axes]
        
        fig.suptitle('Performance by Transform Type', fontsize=14, fontweight='bold')
        
        for idx, transform in enumerate(transforms):
            ax = axes[idx]
            transform_data = self.df[self.df['Transform'] == transform]
            
            for lib in transform_data['Library'].unique():
                lib_data = transform_data[transform_data['Library'] == lib]
                ax.plot(lib_data['Size'], lib_data['Throughput(Msamp/s)'], 
                       marker='o', label=lib, linewidth=2)
            
            ax.set_xlabel('FFT Size', fontsize=10)
            ax.set_ylabel('Throughput (Msamp/s)', fontsize=10)
            ax.set_title(transform, fontweight='bold')
            ax.set_xscale('log', base=2)
            ax.grid(True, alpha=0.3)
            ax.legend(loc='best', fontsize=8)
        
        plt.tight_layout()
        
        if output_dir:
            plt.savefig(Path(output_dir) / 'transform_comparison.png', dpi=300, bbox_inches='tight')
            print(f"✓ Saved: transform_comparison.png")
        else:
            plt.show()
        plt.close()
    
    def plot_scaling_analysis(self, output_dir=None):
        """Analyze how performance scales with FFT size"""
        fig, axes = plt.subplots(1, 2, figsize=(14, 5))
        fig.suptitle('Scaling Analysis', fontsize=14, fontweight='bold')
        
        # Theoretical complexity: O(n log n)
        sizes = sorted(self.df['Size'].unique())
        theoretical = [s * np.log2(s) for s in sizes]
        theoretical_normalized = [t / theoretical[0] for t in theoretical]
        
        # Left plot: Actual vs theoretical scaling
        ax = axes[0]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib].groupby('Size')['Mean(us)'].mean()
            # Normalize to first size
            normalized = lib_data.values / lib_data.values[0]
            ax.plot(lib_data.index, normalized, marker='o', label=lib, linewidth=2)
        
        ax.plot(sizes, theoretical_normalized, 'k--', linewidth=2, 
               label='Theoretical O(n log n)', alpha=0.7)
        ax.set_xlabel('FFT Size', fontsize=11)
        ax.set_ylabel('Relative Time (normalized to size 128)', fontsize=11)
        ax.set_title('Scaling Behavior', fontweight='bold')
        ax.set_xscale('log', base=2)
        ax.set_yscale('log')
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        
        # Right plot: Efficiency (actual throughput vs theoretical peak)
        ax = axes[1]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib].groupby('Size').agg({
                'Throughput(Msamp/s)': 'mean'
            })
            ax.plot(lib_data.index, lib_data['Throughput(Msamp/s)'], 
                   marker='s', label=lib, linewidth=2)
        
        ax.set_xlabel('FFT Size', fontsize=11)
        ax.set_ylabel('Throughput (Msamp/s)', fontsize=11)
        ax.set_title('Throughput Scaling (Higher = Better)', fontweight='bold')
        ax.set_xscale('log', base=2)
        ax.grid(True, alpha=0.3)
        ax.legend(loc='best')
        
        plt.tight_layout()
        
        if output_dir:
            plt.savefig(Path(output_dir) / 'scaling_analysis.png', dpi=300, bbox_inches='tight')
            print(f"✓ Saved: scaling_analysis.png")
        else:
            plt.show()
        plt.close()
    
    def plot_statistical_distributions(self, output_dir=None):
        """Show distribution of timings and variance analysis"""
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Statistical Analysis', fontsize=14, fontweight='bold')
        
        # 1. Median vs Mean comparison
        ax = axes[0, 0]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib]
            ax.scatter(lib_data['Mean(us)'], lib_data['Median(us)'], 
                      label=lib, alpha=0.6, s=50)
        
        # Add diagonal line (mean = median)
        max_val = max(self.df['Mean(us)'].max(), self.df['Median(us)'].max())
        ax.plot([0, max_val], [0, max_val], 'k--', alpha=0.5, label='Mean = Median')
        ax.set_xlabel('Mean Latency (μs)', fontsize=10)
        ax.set_ylabel('Median Latency (μs)', fontsize=10)
        ax.set_title('Mean vs Median (symmetry check)', fontweight='bold')
        ax.legend(loc='best')
        ax.grid(True, alpha=0.3)
        
        # 2. Standard deviation analysis
        ax = axes[0, 1]
        box_data = [self.df[self.df['Library'] == lib]['StdDev(us)'].values 
                    for lib in self.df['Library'].unique()]
        bp = ax.boxplot(box_data, labels=self.df['Library'].unique(), patch_artist=True)
        for patch, color in zip(bp['boxes'], sns.color_palette("husl", len(box_data))):
            patch.set_facecolor(color)
        ax.set_ylabel('Standard Deviation (μs)', fontsize=10)
        ax.set_title('Timing Variability Distribution', fontweight='bold')
        ax.grid(True, alpha=0.3, axis='y')
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')
        
        # 3. Min/Max range analysis
        ax = axes[1, 0]
        for lib in self.df['Library'].unique():
            lib_data = self.df[self.df['Library'] == lib]
            ranges = lib_data['Max(us)'] - lib_data['Min(us)']
            ax.scatter(lib_data['Mean(us)'], ranges, label=lib, alpha=0.6, s=50)
        ax.set_xlabel('Mean Latency (μs)', fontsize=10)
        ax.set_ylabel('Range (Max - Min) (μs)', fontsize=10)
        ax.set_title('Timing Stability', fontweight='bold')
        ax.legend(loc='best')
        ax.grid(True, alpha=0.3)
        
        # 4. Performance consistency heatmap
        ax = axes[1, 1]
        pivot = self.df.pivot_table(values='CV', index='Library', columns='Size', aggfunc='mean')
        sns.heatmap(pivot, annot=True, fmt='.1f', cmap='RdYlGn_r', ax=ax,
                   cbar_kws={'label': 'CV (%)'}, vmin=0, vmax=10)
        ax.set_title('Coefficient of Variation by Size (%)', fontweight='bold')
        ax.set_xlabel('FFT Size', fontsize=10)
        ax.set_ylabel('Library', fontsize=10)
        
        plt.tight_layout()
        
        if output_dir:
            plt.savefig(Path(output_dir) / 'statistical_analysis.png', dpi=300, bbox_inches='tight')
            print(f"✓ Saved: statistical_analysis.png")
        else:
            plt.show()
        plt.close()
    
    def plot_speedup_matrix(self, baseline='FFTW', output_dir=None):
        """Create a comprehensive speedup comparison matrix"""
        if baseline not in self.df['Library'].unique():
            baseline = self.df['Library'].unique()[0]
            print(f"Warning: Baseline '{baseline}' not found, using '{baseline}'")
        
        # Calculate speedups
        speedup_data = []
        for lib in self.df['Library'].unique():
            if lib == baseline:
                continue
            
            for size in self.df['Size'].unique():
                for transform in self.df['Transform'].unique():
                    baseline_perf = self.df[
                        (self.df['Library'] == baseline) & 
                        (self.df['Size'] == size) & 
                        (self.df['Transform'] == transform)
                    ]['Throughput(Msamp/s)'].values
                    
                    lib_perf = self.df[
                        (self.df['Library'] == lib) & 
                        (self.df['Size'] == size) & 
                        (self.df['Transform'] == transform)
                    ]['Throughput(Msamp/s)'].values
                    
                    if len(baseline_perf) > 0 and len(lib_perf) > 0:
                        speedup = lib_perf[0] / baseline_perf[0]
                        speedup_data.append({
                            'Library': lib,
                            'Size': size,
                            'Transform': transform,
                            'Speedup': speedup
                        })
        
        speedup_df = pd.DataFrame(speedup_data)
        
        # Create subplots for each transform type
        transforms = speedup_df['Transform'].unique()
        fig, axes = plt.subplots(len(transforms), 1, figsize=(12, 5*len(transforms)))
        if len(transforms) == 1:
            axes = [axes]
        
        fig.suptitle(f'Speedup Relative to {baseline}', fontsize=14, fontweight='bold')
        
        for idx, transform in enumerate(transforms):
            ax = axes[idx]
            transform_data = speedup_df[speedup_df['Transform'] == transform]
            pivot = transform_data.pivot_table(values='Speedup', 
                                               index='Library', 
                                               columns='Size')
            
            sns.heatmap(pivot, annot=True, fmt='.2f', cmap='RdYlGn', 
                       center=1.0, vmin=0.5, vmax=2.0, ax=ax,
                       cbar_kws={'label': 'Speedup'})
            ax.set_title(f'{transform} (>1.0 = faster than {baseline})', fontweight='bold')
            ax.set_xlabel('FFT Size', fontsize=10)
            ax.set_ylabel('Library', fontsize=10)
        
        plt.tight_layout()
        
        if output_dir:
            plt.savefig(Path(output_dir) / f'speedup_matrix_vs_{baseline}.png', 
                       dpi=300, bbox_inches='tight')
            print(f"✓ Saved: speedup_matrix_vs_{baseline}.png")
        else:
            plt.show()
        plt.close()
    
    def generate_summary_table(self):
        """Generate comprehensive summary statistics"""
        print(f"\n{'='*100}")
        print("PERFORMANCE SUMMARY")
        print(f"{'='*100}\n")
        
        # Overall winner analysis
        winners_df = self.get_winner_by_category()
        winner_counts = winners_df['Library'].value_counts()
        
        print("Overall Winners (count of best performance across all categories):")
        print("-" * 100)
        for lib, count in winner_counts.items():
            total = len(winners_df)
            percentage = (count / total) * 100
            print(f"  {lib:20s}: {count:3d} / {total} ({percentage:5.1f}%)")
        
        # Best by transform type
        print(f"\n{'='*100}")
        print("BEST LIBRARY BY TRANSFORM TYPE")
        print(f"{'='*100}")
        for transform in sorted(self.df['Transform'].unique()):
            print(f"\n{transform}:")
            print("-" * 100)
            transform_winners = winners_df[winners_df['Transform'] == transform]
            transform_counts = transform_winners['Library'].value_counts()
            for lib, count in transform_counts.items():
                total = len(transform_winners)
                percentage = (count / total) * 100
                avg_throughput = transform_winners[transform_winners['Library'] == lib]['Throughput'].mean()
                print(f"  {lib:20s}: {count:2d} / {total} ({percentage:5.1f}%) - "
                      f"Avg: {avg_throughput:7.2f} Msamp/s")
        
        # Statistical summary by library
        print(f"\n{'='*100}")
        print("STATISTICAL SUMMARY BY LIBRARY")
        print(f"{'='*100}\n")
        
        summary_stats = self.df.groupby('Library').agg({
            'Throughput(Msamp/s)': ['mean', 'std', 'min', 'max'],
            'Mean(us)': ['mean', 'min', 'max'],
            'CV': 'mean'
        }).round(2)
        
        print(summary_stats.to_string())
        
        # Size-specific recommendations
        print(f"\n{'='*100}")
        print("RECOMMENDED LIBRARY BY SIZE (highest average throughput)")
        print(f"{'='*100}\n")
        
        for size in sorted(self.df['Size'].unique()):
            size_data = self.df[self.df['Size'] == size].groupby('Library')['Throughput(Msamp/s)'].mean()
            best_lib = size_data.idxmax()
            best_throughput = size_data.max()
            print(f"  Size {size:5d}: {best_lib:15s} ({best_throughput:7.2f} Msamp/s)")
    
    def generate_report(self, output_dir=None):
        """Generate complete analysis report with all visualizations"""
        print(f"\n{'='*100}")
        print(f"FFT BENCHMARK ANALYSIS REPORT")
        print(f"{'='*100}")
        print(f"Input file: {self.csv_file}")
        print(f"Total benchmarks: {len(self.df)}")
        print(f"Libraries: {', '.join(sorted(self.df['Library'].unique()))}")
        print(f"Transforms: {', '.join(sorted(self.df['Transform'].unique()))}")
        print(f"Sizes: {', '.join(map(str, sorted(self.df['Size'].unique())))}")
        
        if output_dir:
            output_path = Path(output_dir)
            output_path.mkdir(exist_ok=True, parents=True)
            print(f"\nGenerating visualizations in: {output_path.absolute()}")
            print("-" * 100)
        
        # Generate all visualizations
        self.plot_performance_overview(output_dir)
        self.plot_transform_comparison(output_dir)
        self.plot_scaling_analysis(output_dir)
        self.plot_statistical_distributions(output_dir)
        
        if 'FFTW' in self.df['Library'].unique():
            self.plot_speedup_matrix(baseline='FFTW', output_dir=output_dir)
        else:
            self.plot_speedup_matrix(baseline=self.df['Library'].unique()[0], output_dir=output_dir)
        
        # Generate summary tables
        self.generate_summary_table()
        
        print(f"\n{'='*100}")
        print("ANALYSIS COMPLETE")
        print(f"{'='*100}\n")

def main():
    parser = argparse.ArgumentParser(
        description='Enhanced FFT Benchmark Analysis Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Interactive mode (shows plots)
  %(prog)s fft_benchmark_results.csv
  
  # Save all plots to directory
  %(prog)s results.csv -o analysis_output/
  
  # Just print statistics
  %(prog)s results.csv --stats-only
  
  # Specify baseline for speedup comparison
  %(prog)s results.csv -o plots/ --baseline PFFFT
        """
    )
    
    parser.add_argument('csv_file', 
                       help='Path to CSV file with benchmark results')
    parser.add_argument('-o', '--output-dir', 
                       help='Directory to save plots (interactive if not specified)')
    parser.add_argument('--stats-only', action='store_true',
                       help='Only print statistics, skip visualizations')
    parser.add_argument('--baseline',
                       help='Baseline library for speedup comparison (default: FFTW)')
    
    args = parser.parse_args()
    
    if not Path(args.csv_file).exists():
        print(f"Error: File not found: {args.csv_file}")
        sys.exit(1)
    
    analyzer = FFTBenchmarkAnalyzer(args.csv_file)
    
    if args.stats_only:
        analyzer.generate_summary_table()
    else:
        analyzer.generate_report(args.output_dir)

if __name__ == '__main__':
    main()