import os

cpp_fifo_name = "./pipe/cpp.fifo"
python_fifo_name = "./pipe/python.fifo"

def fifo_read(rfd):
    msg_len = int.from_bytes(os.read(rfd, 2), byteorder='little')    
    msg = os.read(rfd, msg_len).decode()
    return msg, msg_len

def fifo_write(wfd, message):
    len_bytes = int.to_bytes(len(message), length=2, byteorder='little')
    os.write(wfd, len_bytes)
    os.write(wfd, str.encode(message))

def main():
    data = b"Hello World"
    
    print(data)
    for b in data:
        print(f"0x{b:02x}", end=", ")
    
    return
    with open('python_logs.txt', 'w') as log:
        log.write("Python is starting\n")
        
        # Open the FIFO pipe for reading
        rfd = os.open(python_fifo_name, os.O_RDONLY)
        wfd = os.open(cpp_fifo_name, os.O_WRONLY)
        
        for i in range(10):
            if(i%2 == 0):
                msg, _ = fifo_read(rfd)
                log.write(f"Python received: {msg}\n")
            else:
                fifo_write(wfd, str(i))
                log.write(f"Python sending: {i}\n")
                
    exit(0)

if __name__ == "__main__":
    main()
