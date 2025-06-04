import sys
import numpy as np
import matplotlib.pyplot as plt
import csv

def read_latency(path):
    print(path)
    f = open(path, "r")
    reader = csv.reader(f)
    res = [float(row[0]) for row in reader]
    f.close()
    return res

def plot_join_latency(out_name, files, titles):
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

def read_server_stats(file_name):
    print(file_name)
    f = open(file_name)
    reader = csv.reader(f)
    data = [[float(x) for x in row] for row in reader]
    f.close()
    ts = [row[0] for row in data[:-1]]
    clients = [row[1] for row in data[:-1]]
    bytes_processed = [row[2] for row in data[:-1]]
    return ts, clients, bytes_processed

def plot_server_stats(out_name, data_files, titles):
    plt.figure(figsize=(10, 12))
    clients_num_axis = plt.subplot(2, 1, 1)
    bytes_processed_axis = plt.subplot(2, 1, 2)
    clients_num_axis.set_xlabel("Time (s)")
    bytes_processed_axis.set_xlabel("Time (s)")
    clients_num_axis.set_ylabel("No. of Client")
    bytes_processed_axis.set_ylabel("bps")
    clients_num_axis.set_title("No. of clients vs time")
    bytes_processed_axis.set_title("Bytes processed per second vs time")

    max_time = 0
    for file, title in zip(data_files, titles):
        t, c, b = read_server_stats(file)
        max_time = max(max_time, max(t))
        clients_num_axis.plot(t, c, label=title)
        bytes_processed_axis.plot(t, b, label=title)

    ideal_ts = [i for i in range(0, int(max_time) + 1)]
    ideal_clis = [(i // 10) * 1000 for i in range(10, int(max_time) + 11)]
    clients_num_axis.plot(ideal_ts, ideal_clis, label="client connect()")

    clients_num_axis.legend()
    bytes_processed_axis.legend()
    plt.savefig(out_name, dpi=300)
    plt.close()

def read_watch(file_name):
    print(file_name)
    f = open(file_name)
    reader = csv.reader(f, delimiter=' ')
    data = [[float(x) for x in row] for row in reader]
    f.close()
    cpu = [row[0] for row in data[:-2]]
    mem = [row[1] for row in data[:-2]]
    return cpu, mem

def plot_watch(out_name, data_files, titles):
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

def plot_msg_latency(out_name, data_files, titles):
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
    pass

plot_join_latency("echo-out/join_lat.png", ["echo-out/async-stress-join-lat.csv", "echo-out/epoll-stress-join-lat.csv"], ["async", "epoll"])
plot_server_stats("echo-out/server-stress.png", ["echo-out/async-stress-server.csv", "echo-out/epoll-stress-server.csv"], ["async", "epoll"])
plot_watch("echo-out/watch-stress.png", ["echo-out/async-stress-watch.csv", "echo-out/epoll-stress-watch.csv"], ["async", "epoll"])
plot_server_stats("echo-out/server-throughput.png", ["echo-out/async-throughput-server.csv", "echo-out/epoll-throughput-server.csv"], ["async", "epoll"])
plot_watch("echo-out/watch-throughput.png", ["echo-out/async-throughput-watch.csv", "echo-out/epoll-throughput-watch.csv"], ["async", "epoll"])
plot_msg_latency("echo-out/msg-latency.png", [
    "echo-out/async-throughput-100-msg-lat.csv", 
    "echo-out/epoll-throughput-100-msg-lat.csv",
    "echo-out/async-throughput-1000-msg-lat.csv", 
    "echo-out/epoll-throughput-1000-msg-lat.csv",
    "echo-out/async-throughput-10000-msg-lat.csv", 
    "echo-out/epoll-throughput-10000-msg-lat.csv",
    "echo-out/async-throughput-100000-msg-lat.csv", 
    "echo-out/epoll-throughput-100000-msg-lat.csv",
], [
    "async-100", 
    "epoll-100",
    "async-1000", 
    "epoll-1000",
    "async-10000", 
    "epoll-10000",
    "async-100000", 
    "epoll-100000",
])
