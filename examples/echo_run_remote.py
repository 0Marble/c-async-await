import os
import sys
import time
import asyncio

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

async def run_instance(server, port, name):
    server_proc = await asyncio.subprocess.create_subprocess_exec(
        server, 
        port,
        stdout=asyncio.subprocess.PIPE,
    )
    print(name, "pid:", server_proc.pid)
    await asyncio.sleep(1)
    watch_task = asyncio.create_task(run_watch(server_proc.pid))
    server_task = asyncio.create_task(server_proc.wait())
    await asyncio.wait([server_task, watch_task], return_when=asyncio.FIRST_COMPLETED)

    watch_task.cancel()
    watch_stdout = await watch_task
    server_proc.kill()
    server_stdout = (await server_proc.stdout.read()).decode("utf-8")

    f = open(f"echo-out/{name}-server-remote.csv", "w")
    print(server_stdout, file=f)
    f.close()
    f = open(f"echo-out/{name}-watch-remote.csv", "w")
    print(watch_stdout, file=f)
    f.close()

async def main():
    output_dir = "echo-out"
    if output_dir not in os.listdir():
        os.mkdir(output_dir)

    t1 = asyncio.create_task(run_instance("build/examples/echo_async", "7000", "async"))
    t2 = asyncio.create_task(run_instance("build/examples/echo_epoll", "7777", "epoll"))
    await asyncio.wait([t1, t2])
    await t1
    await t2

asyncio.run(main())
