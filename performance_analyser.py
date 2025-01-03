#!/usr/bin/env python3
import os
import re
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import numpy as np

class TestAnalyzer:
    def __init__(self, results_dir):
        self.results_dir = results_dir
        self.test_results = {}
        self.process_all_tests()

    def process_all_tests(self):
        for test_dir in os.listdir(self.results_dir):
            test_path = os.path.join(self.results_dir, test_dir)
            if os.path.isdir(test_path):
                self.test_results[test_dir] = self.analyze_test(test_path)

    def analyze_test(self, test_path):
        server_log = os.path.join(test_path, "server_log.txt")
        server_output = os.path.join(test_path, f"server_output_{os.path.basename(test_path)}.log")
        
        metrics = {
            'timestamp': datetime.now(),
            'total_processing_time': 0,
            'requests_processed': 0,
            'avg_response_time': 0,
            'throughput': 0,
            'errors': 0
        }

        if os.path.exists(server_log):
            metrics.update(self.analyze_server_log(server_log))
        
        if os.path.exists(server_output):
            metrics.update(self.analyze_server_output(server_output))

        return metrics

    def analyze_server_log(self, log_file):
        with open(log_file, 'r') as f:
            content = f.readlines()

        metrics = {
            'data_receipts': len([l for l in content if 'Added' in l]),
            'rankings_sent': len([l for l in content if 'Sent ranking' in l]),
            'final_results_sent': len([l for l in content if 'Sent final results' in l]),
            'errors': len([l for l in content if 'Error' in l])
        }

        return metrics

    def analyze_server_output(self, output_file):
        with open(output_file, 'r') as f:
            content = f.readlines()

        start_time = None
        end_time = None
        for line in content:
            if 'Starting server' in line:
                start_time = self.extract_timestamp(line)
            elif 'Shutting down' in line:
                end_time = self.extract_timestamp(line)

        metrics = {
            'total_runtime': (end_time - start_time).total_seconds() if start_time and end_time else 0
        }

        return metrics

    def generate_reports(self):
        df = pd.DataFrame.from_dict(self.test_results, orient='index')
        
        self.plot_metrics(df)
        self.save_csv_report(df)
        self.generate_summary_report(df)

    def plot_metrics(self, df):
        metrics_to_plot = ['total_processing_time', 'requests_processed', 'avg_response_time', 'throughput']
        
        plt.figure(figsize=(15, 10))
        for i, metric in enumerate(metrics_to_plot, 1):
            plt.subplot(2, 2, i)
            sns.barplot(data=df, x=df.index, y=metric)
            plt.title(f'{metric.replace("_", " ").title()}')
            plt.xticks(rotation=45)
        plt.tight_layout()
        plt.savefig('performance_comparison.png')
        plt.close()

    def save_csv_report(self, df):
        df.to_csv('performance_metrics.csv')

    def generate_summary_report(self, df):
        with open('performance_summary.txt', 'w') as f:
            f.write("Performance Test Summary\n")
            f.write("=" * 50 + "\n\n")
            
            f.write("Best Configurations:\n")
            for metric in df.columns:
                if metric != 'timestamp':
                    best_test = df[metric].idxmax()
                    f.write(f"{metric}: Test {best_test} ({df[metric].max():.2f})\n")
            
            f.write("\nAverage Metrics:\n")
            for metric in df.columns:
                if metric != 'timestamp':
                    f.write(f"{metric}: {df[metric].mean():.2f}\n")

if __name__ == "__main__":
    import sys
    results_dir = sys.argv[1] if len(sys.argv) > 1 else "results"
    analyzer = TestAnalyzer(results_dir)
    analyzer.generate_reports()