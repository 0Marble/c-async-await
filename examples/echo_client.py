import socket
import asyncio
import random
import sys
import time

SYMBOLS = [chr(c) for c in range(48, 58)] + [chr(c) for c in range(64, 91)] + [chr(c) for c in range(97, 123)]

join_latencies = []
msg_latencies = []

async def do_echo(ip, port, msg_len):
    global join_latencies
    global msg_latencies

    connection_ok = False
    try:
        conn_start = time.time()
        reader, writer = await asyncio.open_connection(ip, port)
        conn_end = time.time()
        join_latencies.append(conn_end - conn_start)

        connection_ok = True
        while True:
            msg = "".join(random.choices(SYMBOLS, k=msg_len))
            len_bytes = msg_len.to_bytes(4, byteorder="big")
            msg_bytes = msg.encode("utf-8")

            full_msg = len_bytes + msg_bytes
            writer.write(full_msg)
            await writer.drain()

            msg_wait_start = time.time()
            recv = await reader.readexactly(len(full_msg))
            msg_wait_end = time.time()
            msg_latencies.append(msg_wait_end - msg_wait_start)

            recv_len = int.from_bytes(recv[:4], byteorder="big")
            recv_msg = recv[4:].decode("utf-8")
            if recv_len != msg_len or recv_msg != msg:
                print("Expected length", msg_len, "got length", recv_len)
                print("Expected `", msg, "' got `", recv_msg, "'")
                assert False

    except Exception as e:
        print(e)

    if connection_ok:
        writer.close()
        await writer.wait_closed()


def on_failure(clients, t, cnt, name):
    global msg_latencies
    global join_latencies
    f = open(f"echo-out/echo-join-lat-{name}.csv", "w")
    for l in join_latencies:
        print(l, file=f)
    f.close()
    f = open(f"echo-out/echo-msg-lat-{name}.csv", "w")
    for l in msg_latencies:
        print(l, file=f)
    f.close()

    clients.discard(t)
    print("Failed at cnt=", cnt)
    sys.exit(0)

async def main(ip, port, msg_len, name):
    clients = set()

    cur_clients = 0
    while True:
        cur_clients += 1 
        for _ in range(1000):
            task = asyncio.create_task(do_echo(ip, port, msg_len))
            clients.add(task)
            task.add_done_callback(lambda t: on_failure(clients, t, cur_clients * 1000, name))
        await asyncio.sleep(10)

# ip = "134.209.100.202"
ip = "127.0.0.1"
asyncio.run(main(ip, sys.argv[1], int(sys.argv[2]), sys.argv[3]))

