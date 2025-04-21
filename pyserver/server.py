import socket

HOST = '127.0.0.1'  # Localhost
PORT = 5000        # Port to listen on

def process_string(input_string):
    chunks = input_string.split()  # Split the string into chunks by whitespace
    for chunk in chunks:
        if chunk == "ERROR":
            return "RESTART"
    return chunks  # Return the chunks if "ERROR" is not found



def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.bind((HOST, PORT))
        server_socket.listen(1)  # Listen for incoming connections
        print(f"Server is listening on {HOST}:{PORT}")

        while True:
            # Accept a client connection
            client_socket, addr = server_socket.accept()
            print(f"Connected by {addr}")
            with client_socket:
                # Receive data from the client
                data = client_socket.recv(1024).decode('utf-8')  # Assuming UTF-8 encoding
                if not data:
                    break

                print(f"Received: {data}")
                # "ADS a002a5e39072 ERROR 18"
                result = process_string(data)

                # Check if the received data is "ADS"
                if data == "ADS":
                    # Send acknowledgment to the client
                    response = "ACK\r\n"
                    print("Sent acknowledgment: ACK")
                else:
                    response = "UNK\r\n"
                    print("Sent acknowledgment: ACK")
                    print("Received data is not 'ADS'")
                client_socket.sendall(response.encode('utf-8'))


if __name__ == "__main__":
    while True:
        try:
            start_server()
        except Exception as e:
            print(f"Server error: {e}")

