import sys
import numpy as np
import matplotlib.pyplot as plt
import csv

def verify_files_exist(files):
    for path in files:
        try:
            f = open(path, "r")
            f.close()
        except:
            print("[ERROR] File not accessible", path)


def read_latency(path):
    print(path)
    f = open(path, "r")
    reader = csv.reader(f)
    res = [float(row[0]) for row in reader]
    if len(res) == 0: print("[WARN] empty file:", path)
    f.close()
    return res

def plot_join_latency(out_name, files, titles):
    verify_files_exist(files)
    try:
        plt.figure(figsize=(10,6))
        plt.title("Joining Latency")
        plt.xlabel("No. of Clients")
        plt.ylabel("Latency (s)")

        for file, title in zip(files, titles):
            pts = read_latency(file)
            plt.plot(pts, label=title)

        plt.legend()
        plt.savefig(out_name, dpi=300)
        plt.close()
        print("[OK]", out_name, file=sys.stderr)
    except:
        print("[ERROR] failed to plot", out_name, file=sys.stderr)

def read_server_stats(file_name):
    print(file_name)
    f = open(file_name)
    reader = csv.reader(f)
    data = [[float(x) for x in row] for row in reader]
    f.close()
    ts = [row[0] for row in data[:-1]]
    clients = [row[1] for row in data[:-1]]
    bytes_processed = [row[2] for row in data[:-1]]
    if len(ts) == 0 or len(clients) == 0 or len(bytes_processed) == 0: 
        print("[WARN] empty file:", file_name)
    return ts, clients, bytes_processed

def plot_server_stats(out_name, data_files, titles):
    verify_files_exist(data_files)
    try:
        plt.figure(figsize=(10, 6))
        plt.xlabel("Time (s)")
        plt.ylabel("No. of Client")
        plt.title("No. of clients vs time")

        max_time = 0
        for file, title in zip(data_files, titles):
            t, c, b = read_server_stats(file)
            max_time = max(max_time, max(t))
            plt.plot(t, c, label=title)

        plt.legend()
        plt.savefig(out_name, dpi=300)
        plt.close()
        print("[OK]", out_name, file=sys.stderr)
    except:
        print("[ERROR] failed to plot", out_name, file=sys.stderr)

def read_watch(file_name):
    print(file_name)
    f = open(file_name)
    reader = csv.reader(f, delimiter=' ')
    data = [[float(x) for x in row] for row in reader]
    f.close()
    cpu = [row[0] for row in data[:-2]]
    mem = [row[1] for row in data[:-2]]
    if len(mem) == 0 or len(cpu) == 0: 
        print("[WARN] empty file:", file_name)
    return cpu, mem

def plot_watch(out_name, data_files, titles):
    verify_files_exist(data_files)
    try:
        plt.figure(figsize=(10, 12))
        cpu_axis = plt.subplot(2, 1, 1)
        mem_axis = plt.subplot(2, 1, 2)
        cpu_axis.set_xlabel("Time (s)")
        mem_axis.set_xlabel("Time (s)")
        cpu_axis.set_ylabel("CPU%")
        mem_axis.set_ylabel("MEM%")
        cpu_axis.set_title("CPU usage, single-core %")
        mem_axis.set_title("Memory usage, % of 64GB")

        for file, title in zip(data_files, titles):
            cpu, mem = read_watch(file)
            cpu_axis.plot(cpu, label=title)
            mem_axis.plot(mem, label=title)

        cpu_axis.legend()
        mem_axis.legend()
        plt.savefig(out_name, dpi=300)
        plt.close()
        print("[OK]", out_name, file=sys.stderr)
    except:
        print("[ERROR] failed to plot", out_name, file=sys.stderr)

def plot_msg_latency(out_name, data_files, titles):
    verify_files_exist(data_files)
    try:
        plt.figure(figsize=(10,6))
        plt.title("Message Latency CDF")
        plt.xlabel("Latency (s)")
        plt.xscale("log")

        for file, title in zip(data_files, titles):
            data = read_latency(file)
            plt.ecdf(data, label=title)

        plt.legend()
        plt.savefig(out_name, dpi=300)
        plt.close()
        print("[OK]", out_name, file=sys.stderr)
    except:
        print("[ERROR] failed to plot", out_name, file=sys.stderr)

def read_throughput(file_name):
    print(file_name)
    f = open(file_name)
    reader = csv.reader(f)
    data = [[float(x) for x in row] for row in reader]
    f.close()
    size = [row[0] for row in data[:-1]]
    byts = [row[1] for row in data[:-1]]
    time = [row[2] for row in data[:-1]]
    if len(size) == 0 or len(byts) == 0 or len(time) == 0: 
        print("[WARN] empty file:", file_name)
    return size, byts, time

def plot_throughput(out_name, data_files, titles):
    verify_files_exist(data_files)
    try:
        plt.figure(figsize=(10,6))
        plt.title("Message Throughput")
        plt.xlabel("Message Size")
        plt.xscale("log")
        plt.ylabel("Bytes Per Second")

        for file, title in zip(data_files, titles):
            size, byts, time = read_throughput(file)
            plt.plot(size, np.array(byts) / np.array(time), label=title)

        plt.legend()
        plt.savefig(out_name, dpi=300)
        plt.close()
        print("[OK]", out_name, file=sys.stderr)
    except:
        print("[ERROR] failed to plot", out_name, file=sys.stderr)

plot_join_latency("echo-out/local-join_lat.png", [
    "echo-out/local-async-stress-join-lat.csv", 
    "echo-out/local-epoll-stress-join-lat.csv", 
    "echo-out/local-python-stress-join-lat.csv"
], ["async", "epoll", "python"])

plot_server_stats("echo-out/local-server-stress.png", [
    "echo-out/local-async-stress-server.csv", 
    "echo-out/local-epoll-stress-server.csv", 
    "echo-out/local-python-stress-server.csv"
], ["async", "epoll", "python"])

plot_watch("echo-out/local-watch-stress.png", [
    "echo-out/local-async-stress-watch.csv",
    "echo-out/local-epoll-stress-watch.csv",
    "echo-out/local-python-stress-watch.csv"
], ["async", "epoll", "python"])

plot_server_stats("echo-out/local-server-throughput.png", [
    "echo-out/local-async-throughput-server.csv",
    "echo-out/local-epoll-throughput-server.csv",
    "echo-out/local-python-throughput-server.csv"
], ["async", "epoll", "python"])

plot_watch("echo-out/local-watch-throughput.png", [
    "echo-out/local-async-throughput-watch.csv",
    "echo-out/local-epoll-throughput-watch.csv",
    "echo-out/local-python-throughput-watch.csv"
], ["async", "epoll", "python"])

plot_msg_latency("echo-out/local-msg-latency.png", [
    "echo-out/local-async-throughput-100-msg-lat.csv", 
    "echo-out/local-async-throughput-1000-msg-lat.csv", 
    "echo-out/local-async-throughput-10000-msg-lat.csv", 
    "echo-out/local-async-throughput-100000-msg-lat.csv", 
    "echo-out/local-epoll-throughput-100-msg-lat.csv",
    "echo-out/local-epoll-throughput-1000-msg-lat.csv",
    "echo-out/local-epoll-throughput-10000-msg-lat.csv",
    "echo-out/local-epoll-throughput-100000-msg-lat.csv",
    "echo-out/local-python-throughput-100-msg-lat.csv",
    "echo-out/local-python-throughput-1000-msg-lat.csv",
    "echo-out/local-python-throughput-10000-msg-lat.csv",
    "echo-out/local-python-throughput-100000-msg-lat.csv",
], [
    "async-100", 
    "async-1000", 
    "async-10000", 
    "async-100000", 
    "epoll-100",
    "epoll-1000",
    "epoll-10000",
    "epoll-100000",
    "python-100",
    "python-1000",
    "python-10000",
    "python-100000",
])

plot_join_latency("echo-out/remote-join_lat.png", [
    "echo-out/remote-async-stress-join-lat.csv",
    "echo-out/remote-epoll-stress-join-lat.csv",
    "echo-out/remote-python-stress-join-lat.csv"
], ["async", "epoll", "python"])

plot_server_stats("echo-out/remote-server-stress.png", [
    "echo-out/remote-async-stress-server.csv",
    "echo-out/remote-epoll-stress-server.csv",
    "echo-out/remote-python-stress-server.csv"
], ["async", "epoll", "python"])

plot_watch("echo-out/remote-watch-stress.png", [
    "echo-out/remote-async-stress-watch.csv",
    "echo-out/remote-epoll-stress-watch.csv",
    "echo-out/remote-python-stress-watch.csv"
], ["async", "epoll", "python"])

plot_server_stats("echo-out/remote-server-throughput.png", [
    "echo-out/remote-async-throughput-server.csv",
    "echo-out/remote-epoll-throughput-server.csv",
    "echo-out/remote-async-throughput-server.csv"
], ["async", "epoll", "python"])

plot_watch("echo-out/remote-watch-throughput.png", [
    "echo-out/remote-async-throughput-watch.csv",
    "echo-out/remote-epoll-throughput-watch.csv",
    "echo-out/remote-python-throughput-watch.csv"
], ["async", "epoll", "python"])

plot_throughput("echo-out/local-throughput.png", [
    "./echo-out/local-async-throughput-client.csv",
    "./echo-out/local-epoll-throughput-client.csv",
    "./echo-out/local-python-throughput-client.csv",
], ["async", "epoll", "python"])

plot_throughput("echo-out/remote-throughput.png", [
    "./echo-out/remote-async-throughput-client.csv",
    "./echo-out/remote-epoll-throughput-client.csv",
    "./echo-out/remote-python-throughput-client.csv",
], ["async", "epoll", "python"])

plot_msg_latency("echo-out/remote-msg-latency.png", [
    "echo-out/remote-async-throughput-100-msg-lat.csv", 
    "echo-out/remote-async-throughput-1000-msg-lat.csv", 
    "echo-out/remote-async-throughput-10000-msg-lat.csv", 
    "echo-out/remote-async-throughput-100000-msg-lat.csv", 
    "echo-out/remote-epoll-throughput-100-msg-lat.csv",
    "echo-out/remote-epoll-throughput-1000-msg-lat.csv",
    "echo-out/remote-epoll-throughput-10000-msg-lat.csv",
    "echo-out/remote-epoll-throughput-100000-msg-lat.csv",
    "echo-out/remote-python-throughput-100-msg-lat.csv",
    "echo-out/remote-python-throughput-1000-msg-lat.csv",
    "echo-out/remote-python-throughput-10000-msg-lat.csv",
    "echo-out/remote-python-throughput-100000-msg-lat.csv",
], [
    "async-100", 
    "async-1000", 
    "async-10000", 
    "async-100000", 
    "epoll-100",
    "epoll-1000",
    "epoll-10000",
    "epoll-100000",
    "python-100",
    "python-1000",
    "python-10000",
    "python-100000",
])
