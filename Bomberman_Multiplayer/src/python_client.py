import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
s.sendto(b'JOEEEE BIDEN', ('62.98.90.61', 9899))
#s.sendto(b'quit', ('62.98.90.61', 9898))
