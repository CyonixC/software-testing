from __future__ import print_function
import sys
import threading
import sqlite3
from coapthon.client.helperclient import HelperClient

def send_coap_request(client, method, path, payload=None):
    try:
        if method.upper() == "GET":
            response = client.get(path)
        elif method.upper() == "POST":
            response = client.post(path, payload=payload)
        elif method.upper() == "DELETE":
            response = client.delete(path)
        elif method.upper() == "PUT":
            response = client.put(path, payload=payload)
        else:
            print("Unsupported method: %s" % method, file=sys.stderr)
            return False

        if response is not None:
            print(response)
            return True
    except Exception as e:
        print("Exception during request: %s" % str(e), file=sys.stderr)
        return False
    return False

def thread_coap_request(client, method, path, payload=None):
    # Wrapper function for threading
    result = send_coap_request(client, method, path, payload)
    return result

def fuzz_coap(input_data):
    lines = input_data.splitlines()
    if len(lines) < 2:
        print("Input file must at least include method and path.", file=sys.stderr)
        sys.exit(1)
    
    method = lines[0].strip()
    path = lines[1].strip()
    print(path)
    payload = "\n".join(lines[2:]) if len(lines) > 2 else None
    print(payload)

    host = "127.0.0.1"
    port = 5683
    client = HelperClient(server=(host, port))

    # Create a thread for the CoAP request
    coap_thread = threading.Thread(target=thread_coap_request, args=(client, method, path, payload))
    coap_thread.start()
    coap_thread.join(timeout=10)  # Wait for the thread to complete or timeout after 10 seconds

    if coap_thread.is_alive():
        print("The CoAP request timed out.", file=sys.stderr)
        # Attempt to stop the thread and client here
        # However, Python does not provide a direct way to kill threads.
        # This is a limitation and properly handling such situations might require a different approach.
    else:
        print("The CoAP request completed within the timeout.")

    client.stop()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python coap_testdriver.py <input_file>", file=sys.stderr)
        sys.exit(1)
    con = sqlite3.connect(".coverage")
    cur = con.cursor()
    for row in cur.execute('SELECT * FROM arc;'):
        print(row)
    input_file = sys.argv[1]
    with open(input_file, 'rb') as f:
        input_data = f.read().decode('utf-8')
    fuzz_coap(input_data)
