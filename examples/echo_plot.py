import sys
import numpy as np
import matplotlib.pyplot as plt
import csv

def read_latency(path):
    f = open(path, "r")
    reader = csv.reader(f)
    res = [float(row[0]) for row in reader]
    f.close()
    return res

def plot_latencies(out_name, files, titles, plot_title, x_label, scale = "linear"):
    plt.figure(figsize=(10,6))
    plt.title(plot_title)
    plt.xlabel(x_label)
    plt.ylabel("Latency (s)")
    plt.yscale(scale)

    for file, title in zip(files, titles):
        pts = read_latency(file)
        plt.plot(pts, label=title)

    plt.legend()
    plt.savefig(out_name, dpi=300)

def read_server_stats(file_name):
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

def read_watch(file_name):
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

plot_latencies("echo-out/join_lat.png", ["echo-out/echo-join-lat-async.csv", "echo-out/echo-join-lat-epoll.csv"], ["async", "epoll"], "Joining Latency", "No. of Clients")
plot_latencies("echo-out/msg_lat.png", ["echo-out/echo-msg-lat-async.csv", "echo-out/echo-msg-lat-epoll.csv"], ["async", "epoll"], "Message Latency", "Test Duration", scale="log")
plot_server_stats("echo-out/server.png", ["echo-out/async-server.csv", "echo-out/epoll-server.csv"], ["async", "epoll"])
plot_watch("echo-out/watch.png", ["echo-out/async-watch.csv", "echo-out/epoll-watch.csv"], ["async", "epoll"])
