import socket
import asyncio
import random
import sys
import time

SYMBOLS = [chr(c) for c in range(48, 58)] + [chr(c) for c in range(64, 91)] + [chr(c) for c in range(97, 123)]

join_latencies = []
msg_latencies = []
send_messages = False

class Echo:
    def __init__(self, ip, port):
        self.ip = ip
        self.port = port
        self.reader = None
        self.writer = None
        self.bytes_sent = 0
        self.connected = False

    async def connect(self):
        global msg_latencies
        try:
            start = time.time()
            self.reader, self.writer = await asyncio.open_connection(self.ip, self.port)
            end = time.time()
            join_latencies.append(end - start)
            self.connected = True
        except:
            self.connected = False

        return self

    async def do_echo(self, msg_len):
        assert self.connected
        global msg_latencies
        global send_messages

        self.bytes_sent = 0
        while send_messages:
            try:
                msg = "".join(random.choices(SYMBOLS, k=msg_len))
                len_bytes = msg_len.to_bytes(4, byteorder="big")
                msg_bytes = msg.encode("utf-8")

                full_msg = len_bytes + msg_bytes
                self.writer.write(full_msg)
                await self.writer.drain()
                self.bytes_sent += len(full_msg)

                recv = await self.reader.readexactly(len(full_msg))

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
        if self.connected:
            self.writer.close()
            await self.writer.wait_closed()
            self.connected = False
        return self

def do_exit(name, cnt):
    global send_messages
    global msg_latencies
    had_error = True 
    send_messages = False
    print(f"[{name}]: failed at {cnt}", file = sys.stderr)
    f = open(f"echo-out/echo-join-lat-{name}.csv", "w")
    for l in join_latencies:
        print(l, file=f)
    f.close()
    sys.exit(0)

async def main(ip, port, msg_len, name):
    print(f"[{name}]: started", file = sys.stderr)
    batch_size = 1000
    clients = []
    global send_messages
    global msg_latencies
    send_messages = True

    while send_messages:
        batch = [asyncio.create_task(Echo(ip, port).connect()) for _ in range(batch_size)]
        await asyncio.wait(batch, return_when=asyncio.ALL_COMPLETED)
        had_error = False
        for c in batch:
            client = await c
            assert type(client) == Echo
            if not client.connected:
                do_exit(name, len(clients))
            else:
                task = asyncio.create_task(client.do_echo(msg_len))
                task.add_done_callback(lambda x: do_exit(name, len(clients)))
                clients.append(task)
        if had_error:
            await asyncio.wait(clients)  
            for i in range(len(clients)):
                clients[i] = await clients[i]
                assert type(clients[i]) == Echo
                clients[i] = asyncio.create_task(clients[i].close())
            break
        print(f"[{name}]: client count = {len(clients)}", file = sys.stderr)

    await asyncio.wait(clients)  
    for i in range(len(clients)):
        clients[i] = await clients[i]
        assert type(clients[i]) == Echo


# ip = "134.209.100.202"
ip = "127.0.0.1"
asyncio.run(main(ip, sys.argv[1], int(sys.argv[2]), sys.argv[3]))

