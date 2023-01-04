import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
s.bind(('127.0.0.1', 9999))
while True:
    message, sender = s.recvfrom(4096)
    print(message, sender)

    if message == b'quit':
        print("QUIT")
        break
print(message, sender)