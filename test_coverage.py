import coverage
import atexit
import random
import socket

def fn(x: int, y:int):
    """Just a random function for demo purposes"""
    if x > y:
        return x
    else:
        if x == 10:
            return 13
        if y == 10:
            return 14
        if x == 5:
            return 15
        if y == 5:
            return 16
        if x == y:
            return 3
        else:
            return y
    
def open_tcp(port_no):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(('localhost', port_no))
    s.listen(1)
    return s

def main():

    def atexit_handler():
        """Coverage saving must be registered as atexit in case of crashing"""
        cov.stop()
        cov.save()
        s.close()

    cov = coverage.Coverage(branch=True, config_file=".coveragearc")
    s = open_tcp(4345)
    atexit.register(atexit_handler)

    while True:
        conn, addr = s.accept()
        x = int.from_bytes(conn.recv(1), "little")
        y = int.from_bytes(conn.recv(1), "little")
        cov.start()
        fn(int(x), int(y))
        cov.stop()
        cov.save()
        conn.sendall(b'OK')

if __name__ == "__main__":
    main()
