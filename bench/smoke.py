import socket, time

def enc(*args):
    parts=[f"*{len(args)}\r\n".encode()]
    for a in args:
        a=str(a).encode()
        parts += [f"${len(a)}\r\n".encode(), a, b"\r\n"]
    return b"".join(parts)

s = socket.create_connection(("127.0.0.1", 6380))
s.sendall(enc("PING"));      print(s.recv(128))  # +PONG
s.sendall(enc("SET","a","1"))
s.sendall(enc("GET","a"));   print(s.recv(128))  # $1\r\n1\r\n
s.sendall(enc("EXISTS","a","b")); print(s.recv(128))  # :1
s.sendall(enc("DEL","a"));   print(s.recv(128))  # :1
