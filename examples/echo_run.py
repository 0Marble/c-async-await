import os
import sys
import time
import asyncio
import random
import argparse

async def run_watch(name, server_pid):
    s = ""
    try:
        script = f"top -b -n 2 -d 0.2 -p {server_pid} | tail -1 |" + " awk '{print $9,$10}'"
        while True:
            await asyncio.sleep(1)
            ps_proc = await asyncio.subprocess.create_subprocess_shell(
                script, 
                stdout=asyncio.subprocess.PIPE
            ) 
            ps_res = await ps_proc.wait()
            new_stat = (await ps_proc.stdout.read()).decode("utf-8")
            print(f"[{name}]: watcher {new_stat}", file=sys.stderr)
            s = s + new_stat
    finally:
        print(f"[{name}]: watcher closed", file=sys.stderr)
        return s

async def run_client(client, ip, port, out, name):
    print(f"[{name}]: connecting to listener {ip}:{port}", file=sys.stderr)
    reader, writer = await asyncio.open_connection(ip, port)
    print(f"[{name}]: connected to listener", file=sys.stderr)

    writer.write("START".encode("utf-8"))
    await writer.drain()
    port_bytes = await reader.readexactly(2)
    server_port = str(int.from_bytes(port_bytes, "big"))
    print(f"[{name}]: server port {server_port}", file=sys.stderr)

    client_proc = await asyncio.subprocess.create_subprocess_exec(
        "python",
        client,
        "--ip", ip,
        "--port", server_port,
        "--name", name,
        stdout=asyncio.subprocess.PIPE,
    )
    client_task = asyncio.create_task(client_proc.wait())
    await client_task
    client_stdout = (await client_proc.stdout.read()).decode("utf-8")

    f = open(f"{out}/{name}-client.csv", "w")
    print(client_stdout, file=f)
    f.close()

    print(f"[{name}]: client finished", file=sys.stderr)
    writer.write("END".encode("utf-8"))
    await writer.drain()
    writer.close()
    await writer.wait_closed()
    await asyncio.sleep(10)

async def run_server_listener(reader, writer, server, port, out, name, serv):
    print(f"[{name}]: starting server, port {port}")
    msg = (await reader.readexactly(len("START"))).decode("utf-8")
    print(f"[{name}]: got a `{msg}' message from client", file=sys.stderr)
    assert msg == "START"
    writer.write(int.to_bytes(port, 2, "big"))
    await writer.drain()

    server_proc = await asyncio.subprocess.create_subprocess_exec(
        server, str(port), stdout=asyncio.subprocess.PIPE,
    )
    watch_task = asyncio.create_task(run_watch(name, server_proc.pid))
    server_task = asyncio.create_task(server_proc.wait())
    msg = (await reader.readexactly(len("END"))).decode("utf-8") 
    print(f"[{name}]: got `{msg}' message from client", file=sys.stderr)
    assert msg == "END"
    await asyncio.sleep(1)

    server_proc.kill()
    watch_task.cancel()
    watch_stdout = await watch_task
    server_stdout = (await server_proc.stdout.read()).decode("utf-8")

    f = open(f"{out}/{name}-server.csv", "w")
    print(server_stdout, file=f)
    f.close()
    f = open(f"{out}/{name}-watch.csv", "w")
    print(watch_stdout, file=f)
    f.close()

    serv.close()
    serv.close_clients()
    print("[RUNNER]: listener closed")

async def run_server(server, listener_port, out, name):
    print(f"[{name}]: starting listener, port {listener_port}")
    serv = await asyncio.start_server(
        lambda r, w: run_server_listener(r, w, server, random.randint(8000, 9000), out, name, serv),
        port=listener_port,
    )
    try:
        await serv.start_serving()
        await serv.serve_forever()
    except asyncio.CancelledError:
        pass
    serv.close_clients()
    serv.close()
    print("[RUNNER]: listener closed")

async def run_instance(server, client, name, args):
    if args.kind == "local":
        server_task = asyncio.create_task(run_server(server, args.port, args.out, name))
        client_task = asyncio.create_task(run_client(client, args.ip, args.port, args.out, name))
        await asyncio.wait([client_task, server_task], return_when=asyncio.FIRST_COMPLETED)
        await client_task
        await server_task

    elif args.kind == "client":
        client_task = asyncio.create_task(run_client(client, args.ip, args.port, args.out, name))
        await client_task
    elif args.kind == "server":
        server_task = asyncio.create_task(run_server(server, args.port, args.out, name))
        await server_task

async def main(args):
    try:
        os.mkdir(args.out)
    except FileExistsError:
        pass

    if "all" in args.tests or "throughput" in args.tests:
        await run_instance(
                "./build/examples/echo_async", 
                "./examples/echo_client_throughput.py", 
                 f"{args.prefix}-async-throughput", args,
        )
        await run_instance(
                "./build/examples/echo_epoll", 
                "./examples/echo_client_throughput.py", 
                 f"{args.prefix}-epoll-throughput", args,
        )
        await run_instance(
                "./examples/echo_python.py", 
                "./examples/echo_client_throughput.py", 
                 f"{args.prefix}-python-throughput", args,
        )
    if "all" in args.tests or "stress" in args.tests:
        await run_instance(
                "./build/examples/echo_async", 
                "./examples/echo_client_stress.py", 
                 f"{args.prefix}-async-stress", args,
        )
        await run_instance(
                "./build/examples/echo_epoll", 
                "./examples/echo_client_stress.py", 
                 f"{args.prefix}-epoll-stress", args,
        )
        await run_instance(
                "./examples/echo_python.py", 
                "./examples/echo_client_stress.py", 
                 f"{args.prefix}-python-stress", args,
        )

parser = argparse.ArgumentParser("runner")
parser.add_argument("--kind", choices=["local", "client", "server"], default="local")
parser.add_argument("--ip", default="localhost")
parser.add_argument("--port", default="6969")
parser.add_argument("--tests", nargs="+", default="all", choices=["all", "stress", "throughput"])
parser.add_argument("--prefix", default="local")
parser.add_argument("--out", default="echo-out")
args = parser.parse_args(sys.argv[1:])

asyncio.run(main(args))
