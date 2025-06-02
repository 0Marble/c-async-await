import sys
import matplotlib.pyplot as plt
import numpy as np

def read_latencies(file_path):
    """Read latencies from a file (one float per line)."""
    with open(file_path, 'r') as f:
        return [float(line.strip()) for line in f if line.strip()]

def running_average(data):
    """Compute running average of a list."""
    return np.cumsum(data) / (np.arange(len(data)) + 1)

def plot_comparison(file1, file2, label1="File 1", label2="File 2"):
    """Plot running averages of two latency files."""
    latencies1 = read_latencies(file1)
    latencies2 = read_latencies(file2)

    # Compute running averages
    avg1 = running_average(latencies1)
    avg2 = running_average(latencies2)

    # Plot
    plt.figure(figsize=(10, 6))
    plt.plot(avg1, label=f"{label1} (n={len(latencies1)})", alpha=0.8, linewidth=2)
    plt.plot(avg2, label=f"{label2} (n={len(latencies2)})", alpha=0.8, linewidth=2)
    
    plt.title("Average Connection Latency")
    plt.xlabel("Number of Clients")
    plt.ylabel("Latency (s)")
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python latency_plot.py <file1.txt> <file2.txt>")
        sys.exit(1)

    file1, file2 = sys.argv[1], sys.argv[2]
    plot_comparison(file1, file2, label1="Async/Await", label2="Epoll")
