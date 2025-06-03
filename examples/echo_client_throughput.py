import socket
import asyncio
import random
import sys
import time

SYMBOLS = [chr(c) for c in range(48, 58)] + [chr(c) for c in range(64, 91)] + [chr(c) for c in range(97, 123)]

msg_latencies = []
in_messages = False

class Echo:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        self.reader = None
        self.writer = None
        self.bytes_sent = 0
        self.connected = False

    async def connect(self):
        self.reader, self.writer = await asyncio.open_connection(self.ip, self.port)
        self.connected = True
        return self

    async def do_echo(self, msg_len):
        global msg_latencies
        global in_messages

        self.bytes_sent = 0
        while in_messages:
            try:
                msg = "".join(random.choices(SYMBOLS, k=msg_len))
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
    global in_messages

    run_time = 30
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
        in_messages = True
        for i in range(len(clients)):
            assert type(clients[i]) == Echo
            clients[i] = asyncio.create_task(clients[i].do_echo(msg_len))
        print(f"[{name}]: running for {run_time}s...", file=sys.stderr)
        await asyncio.sleep(run_time)
        print(f"[{name}]: done", file=sys.stderr)
        in_messages = False

        bytes_sent = 0
        await asyncio.wait(clients, return_when=asyncio.ALL_COMPLETED)

        for i in range(client_cnt):
            clients[i] = await clients[i]
            assert type(clients[i]) == Echo
            bytes_sent += client.bytes_sent

        print(f"[{name}]: sent {bytes_sent} bytes", file=sys.stderr)
        print(msg_len, ", ", bytes_sent)
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

# ip = "134.209.100.202"
ip = "127.0.0.1"
asyncio.run(main(ip, sys.argv[1], int(sys.argv[2]), sys.argv[3]))

