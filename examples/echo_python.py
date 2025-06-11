#! /usr/bin/python
import asyncio
import sys

current_users = 0
bytes_processed = 0

async def echo_round(r, w):
    msg_len_bytes = await r.readexactly(4)
    msg_len = int.from_bytes(msg_len_bytes, "big")
    msg_bytes = await r.readexactly(msg_len)
    msg = msg_bytes.decode("utf-8")
    print(msg, file=sys.stderr)

    w.write(msg_len_bytes)
    await w.drain()
    w.write(msg_bytes)
    await w.drain()
    return msg_len + 4

async def run_client(r, w):
    global current_users
    global bytes_processed 

    current_users += 1
    try:
        while True:
            bytes_processed += await echo_round(r, w)
    except:
        pass

    current_users -= 1

async def log_stats():
    global current_users
    global bytes_processed 
    i = 0
    while True:
        await asyncio.sleep(1)
        print(f"{i}, {current_users}, {bytes_processed}")
        bytes_processed = 0

async def run_server(port):
    asyncio.create_task(log_stats())
    serv = await asyncio.start_server(run_client, port = port)
    try:
        await serv.start_serving()
        await serv.serve_forever()
    except:
        pass

asyncio.run(run_server(sys.argv[1]))
