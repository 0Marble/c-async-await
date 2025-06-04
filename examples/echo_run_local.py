import os
import sys
import time
import asyncio
import random

async def run_watch(server_pid):
    s = ""
    try:
        while True:
            await asyncio.sleep(1)
            ps_proc = await asyncio.subprocess.create_subprocess_shell(f"top -b -n 2 -d 0.2 -p {server_pid} | tail -1 |" + " awk '{print $9,$10}'", stdout=asyncio.subprocess.PIPE) 
            ps_res = await ps_proc.wait()
            s = s + (await ps_proc.stdout.read()).decode("utf-8") 
    finally:
        return s


async def run_instance(server, client, port, count, name):
    server_proc = await asyncio.subprocess.create_subprocess_exec(
        server, 
        port,
        stdout=asyncio.subprocess.PIPE,
    )
    await asyncio.sleep(1)
    client_proc = await asyncio.subprocess.create_subprocess_exec(
        "python",
        client,
        port,
        count,
        name,
        stdout=asyncio.subprocess.PIPE,
    )
    watch_task = asyncio.create_task(run_watch(server_proc.pid))
    server_task = asyncio.create_task(server_proc.wait())
    client_task = asyncio.create_task(client_proc.wait())
    await asyncio.wait([ server_task, client_task, watch_task], return_when=asyncio.FIRST_COMPLETED)

    server_proc.kill()
    watch_task.cancel()
    watch_stdout = await watch_task
    await client_task
    server_stdout = (await server_proc.stdout.read()).decode("utf-8")
    client_stdout = (await client_proc.stdout.read()).decode("utf-8")

    f = open(f"echo-out/{name}-server.csv", "w")
    print(server_stdout, file=f)
    f.close()
    f = open(f"echo-out/{name}-client.csv", "w")
    print(client_stdout, file=f)
    f.close()
    f = open(f"echo-out/{name}-watch.csv", "w")
    print(watch_stdout, file=f)
    f.close()

async def free_ports(ports):
    for p in ports:
        proc = await asyncio.subprocess.create_subprocess_shell(f"fuser -k {p}/tcp")
        await asyncio.create_task(proc.wait())

async def main():
    output_dir = "echo-out"
    if output_dir not in os.listdir():
        os.mkdir(output_dir)

    p1 = random.randint(8000, 9000)
    p2 = random.randint(8000, 9000)
    while p1 == p2:
        p2 = random.randint(8000, 9000)
    await free_ports([str(p1), str(p2)])

    t1 = asyncio.create_task(run_instance(
        "build/examples/echo_async", 
        "examples/echo_client_stress.py", 
        str(p1), 
        "1000", 
        "async-stress"))
    t2 = asyncio.create_task(run_instance(
        "build/examples/echo_epoll", 
        "examples/echo_client_stress.py", 
        str(p2), 
        "1000", 
        "epoll-stress"))
    await asyncio.wait([t1, t2])
    await t1
    await t2

    p1 = random.randint(8000, 9000)
    p2 = random.randint(8000, 9000)
    while p1 == p2:
        p2 = random.randint(8000, 9000)
    await free_ports([str(p1), str(p2)])

    t1 = asyncio.create_task(run_instance(
        "build/examples/echo_async", 
        "examples/echo_client_throughput.py", 
        str(p1), 
        "10000", 
        "async-throughput"))
    t2 = asyncio.create_task(run_instance(
        "build/examples/echo_epoll", 
        "examples/echo_client_throughput.py", 
        str(p2), 
        "10000", 
        "epoll-throughput"))

    await asyncio.wait([t1, t2])
    await t1
    await t2

asyncio.run(main())
