# Async/Await in C

Repository for the 2025 PKU Advanced Networking class project, where I implement the async/await concurrent programming model as a library in C.

## Contents

- `src`: library code
- `tests`: tests
- `examples`: code for the echo server example

## Running

Build everything:
```bash
make build
```

To run the C examples, do:

```bash
./build/examples/<example name> port
```

Manual client (i.e. send echo messages by hand): `examples/echo_client.py`
Run echo server evaluation locally
```bash
python examples/echo_run.py --prefix local --out echo-out
```

Run echo server evaluation remote (client):
```bash
python examples/echo_run.py --prefix remote --out echo-out --kind client --ip <ip of server>
```

Run echo server evaluation remote (server):
```bash
python examples/echo_run.py --prefix remote --out echo-out --kind server 
```

After running, draw all plots:
```bash
python examples/echo_plot.py
```
