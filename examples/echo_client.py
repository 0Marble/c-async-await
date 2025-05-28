import socket
import asyncio
import random
import sys

MSG_LEN_MAX = 1000
ROUND_CNT_MAX = 1000
CLIENT_CNT = 5000
SYMBOLS = [chr(c) for c in range(48, 58)] + [chr(c) for c in range(64, 91)] + [chr(c) for c in range(97, 123)]

def random_msg():
    msg_len = random.randint(4, MSG_LEN_MAX)
    msg = "".join(random.choices(SYMBOLS, k=msg_len))
    return msg


async def do_echo(ip, port):
    total_bytes_sent = 0
    reader, writer = await asyncio.open_connection(ip, port)
    round_cnt = random.randint(1, ROUND_CNT_MAX)
    for _ in range(round_cnt):
        msg = random_msg()
        msg_len = len(msg)
        len_bytes = msg_len.to_bytes(4, byteorder="big")
        msg_bytes = msg.encode("utf-8")

        full_msg = len_bytes + msg_bytes
        total_bytes_sent += len(full_msg)
        writer.write(full_msg)
        await writer.drain()

        recv = await reader.readexactly(len(full_msg))
        recv_len = int.from_bytes(recv[:4], byteorder="big")
        recv_msg = recv[4:].decode("utf-8")
        if recv_len != msg_len or recv_msg != msg:
            print("Expected length", msg_len, "got length", recv_len)
            print("Expected `", msg, "' got `", recv_msg, "'")
            assert False

    writer.close()
    await writer.wait_closed()
    return total_bytes_sent


async def main(ip, port):
    clients = [asyncio.create_task(do_echo(ip, port)) for _ in range(CLIENT_CNT)]
    await asyncio.wait(clients)
    total_bytes = 0
    for client in clients:
        total_bytes += await client

    print("Total bytes sent:", total_bytes)

asyncio.run(main("127.0.0.1", sys.argv[1]))

