import socket
import sys

def send_length_prefixed_message(sock, message):
    """Send a message prefixed with its length (as 4-byte big-endian)."""
    message_bytes = message.encode('utf-8')
    length_prefix = len(message_bytes).to_bytes(4, byteorder='big')
    sock.sendall(length_prefix + message_bytes)

def receive_length_prefixed_message(sock):
    """Receive a length-prefixed message."""
    # Read the 4-byte length prefix
    length_bytes = sock.recv(4)
    if not length_bytes:
        return None  # Connection closed
    length = int.from_bytes(length_bytes, byteorder='big')
    
    # Read the message itself
    received_bytes = bytearray()
    while len(received_bytes) < length:
        chunk = sock.recv(length - len(received_bytes))
        if not chunk:
            raise ConnectionError("Connection closed prematurely")
        received_bytes.extend(chunk)
    
    return received_bytes.decode('utf-8')

def echo_client(host='127.0.0.1', port=8080):
    """Connect to an echo server and send length-prefixed messages."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((host, port))
        print(f"Connected to {host}:{port}")
        
        while True:
            try:
                message = input("Enter message (or 'quit' to exit): ")
                if message.lower() == 'quit':
                    break
                
                # Send the message with length prefix
                send_length_prefixed_message(sock, message)
                print(f"SENT: {message}")
                
                # Receive the echoed response
                echoed = receive_length_prefixed_message(sock)
                print(f"RECV: {echoed}")
            
            except (ConnectionError, KeyboardInterrupt) as e:
                print(f"Error: {e}")
                break

if __name__ == "__main__":
    echo_client(port=int(sys.argv[1]))
