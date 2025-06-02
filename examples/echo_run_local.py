import os
import sys
import time
import asyncio

async def run_watch(server_pid):
    s = ""
    try:
        while True:
            print("FOO")
            await asyncio.sleep(1)
            ps_proc = await asyncio.subprocess.create_subprocess_shell(f"top -b -n 2 -d 0.2 -p {server_pid} | tail -1 |" + " awk '{print $9,$10}'", stdout=asyncio.subprocess.PIPE) 
            ps_res = await ps_proc.wait()
            s = s + (await ps_proc.stdout.read()).decode("utf-8") 
    finally:
        return s


async def run_instance(server, client, port, name):
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
        "1000",
        name,
        stdout=asyncio.subprocess.PIPE,
    )
    watch_task = asyncio.create_task(run_watch(server_proc.pid))
    server_task = asyncio.create_task(server_proc.wait())
    client_task = asyncio.create_task(client_proc.wait())
    await asyncio.wait([ server_task, client_task, watch_task], return_when=asyncio.FIRST_COMPLETED)

    watch_task.cancel()
    watch_stdout = await watch_task
    server_proc.kill()
    await client_task
    server_stdout = (await server_proc.stdout.read()).decode("utf-8")
    print((await client_proc.stdout.read()).decode("utf-8"))

    f = open(f"echo-out/{name}-server.csv", "w")
    print(server_stdout, file=f)
    f.close()
    f = open(f"echo-out/{name}-watch.csv", "w")
    print(watch_stdout, file=f)
    f.close()

async def main():
    output_dir = "echo-out"
    if output_dir not in os.listdir():
        os.mkdir(output_dir)

    t1 = asyncio.create_task(run_instance("build/examples/echo_async", "examples/echo_client.py", "8080", "async"))
    t2 = asyncio.create_task(run_instance("build/examples/echo_epoll", "examples/echo_client.py", "8888", "epoll"))
    await asyncio.wait([t1, t2])
    await t1
    await t2

asyncio.run(main())
