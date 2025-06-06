import socket
import asyncio
import random
import sys
import time
import argparse

SYMBOLS = [chr(c) for c in range(48, 58)] + [chr(c) for c in range(64, 91)] + [chr(c) for c in range(97, 123)]

msg_latencies = []

class Echo:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        self.reader = None
        self.writer = None
        self.bytes_sent = 0
        self.connected = False

    async def echo_round(self, msg):
        msg_len = len(msg)
        len_bytes = msg_len.to_bytes(4, byteorder="big")
        msg_bytes = msg.encode("utf-8")

        full_msg = len_bytes + msg_bytes
        self.writer.write(full_msg)
        await self.writer.drain()
        self.bytes_sent += len(full_msg)

        msg_wait_start = time.time()
        recv = await self.reader.readexactly(len(full_msg))
        msg_wait_end = time.time()
        msg_latencies.append(msg_wait_end - msg_wait_start)

        recv_len = int.from_bytes(recv[:4], byteorder="big")
        recv_msg = recv[4:].decode("utf-8")
        if recv_len != msg_len or recv_msg != msg:
            print("Expected length", msg_len, "got length", recv_len, file=sys.stderr)
            print("Expected `", msg, "' got `", recv_msg, "'", file=sys.stderr)
            assert False

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(self.ip, self.port)
        await self.echo_round("foo")
        self.connected = True
        return self

    async def do_echo(self, msg_len, msg_cnt):
        global msg_latencies

        self.bytes_sent = 0
        for i in range(0, msg_cnt):
            try:
                msg = "".join(random.choices(SYMBOLS, k=msg_len))
                await self.echo_round(msg)

            except Exception as e:
                print(e)
                break
        return self

    async def close(self):
        self.writer.close()
        await self.writer.wait_closed()
        return self


async def main(ip, port, client_cnt, name):
    global msg_latencies

    msg_cnt = 10
    clients_in_batch = 1000

    print(f"[{name}]: joining the server", file=sys.stderr)
    clients = []
    for i in range(0, client_cnt // clients_in_batch):
        batch = [asyncio.create_task(Echo(ip, port).connect()) for _ in range(clients_in_batch)]
        await asyncio.wait(batch, return_when=asyncio.ALL_COMPLETED)
        for c in batch:
            client = await c
            assert type(client) == Echo
            assert client.connected
            clients.append(client)
    print(f"[{name}]: all clients joined", file=sys.stderr)

    for msg_len in [100, 500, 1000, 5000, 10000, 50000, 100000]:
        msg_latencies = []
        print(f"[{name}]: starting test with msg_len={msg_len}", file=sys.stderr)
        start = time.time()
        for i in range(len(clients)):
            assert type(clients[i]) == Echo
            clients[i] = asyncio.create_task(clients[i].do_echo(msg_len, msg_cnt))
        bytes_sent = 0
        await asyncio.wait(clients, return_when=asyncio.ALL_COMPLETED)
        end = time.time()
        print(f"[{name}]: done", file=sys.stderr)

        for i in range(client_cnt):
            clients[i] = await clients[i]
            assert type(clients[i]) == Echo
            bytes_sent += client.bytes_sent

        print(f"[{name}]: sent {bytes_sent} bytes over {end-start}s", file=sys.stderr)
        print(msg_len, ",", bytes_sent, ",", end - start)
        f = open(f"echo-out/{name}-{msg_len}-msg-lat.csv", "w")
        for l in msg_latencies:
            print(l, file=f)
        f.close()
        await asyncio.sleep(1)

    for i in range(0, client_cnt // clients_in_batch):
        for j in range(0, clients_in_batch):
            idx = i * clients_in_batch + j
            assert type(clients[idx]) == Echo
            clients[idx] = asyncio.create_task(clients[idx].close())
        await asyncio.sleep(1)

    await asyncio.wait(clients, return_when=asyncio.ALL_COMPLETED)
    for i in range(client_cnt):
        clients[i] = await clients[i]
        assert type(clients[i]) == Echo

parser = argparse.ArgumentParser(
    prog="echo_client_stress",
)
parser.add_argument(
    "--ip",
    required=False,
    default="127.0.0.1",
)
parser.add_argument(
    "--port",
    required=False,
    default="8080",
)
parser.add_argument(
    "--client_cnt",
    type=int,
    required=False,
    default=5000,
)
parser.add_argument(
    "--name",
    required=True,
)

args = parser.parse_args(sys.argv[1:])
asyncio.run(main(args.ip, args.port, args.client_cnt, args.name))

