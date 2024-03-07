from coapthon.client.helperclient import HelperClient
import random
import string

class CoAPFuzzer:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.client = HelperClient(server=(self.host, self.port))
        self.original_payload = "Hello, CoAP!"

    def fuzz_payload(self, payload, num_bytes):
        # Generate random bytes to replace part of the payload
        fuzz_bytes = ''.join(random.choice(string.ascii_letters + string.digits) for _ in range(num_bytes))
        return payload[:3] + fuzz_bytes + payload[3 + num_bytes:]

    def fuzz_and_send_requests(self, num_requests, num_bytes):
        for _ in range(num_requests):
            fuzzed_payload = self.fuzz_payload(self.original_payload, num_bytes)
            print("Fuzzing payload:", fuzzed_payload)

            # Send fuzzed GET request with the fuzzed payload and path "/basic/"
            response = self.client.get("/basic", payload=fuzzed_payload)
            print(response.pretty_print())

    def close_connection(self):
        self.client.stop()

def main():
    host = "127.0.0.1"
    port = 5683

    fuzzer = CoAPFuzzer(host, port)
    #while(1):
    #    try:
    fuzzer.fuzz_and_send_requests(num_requests=3, num_bytes=5)
    #    except:
    fuzzer.close_connection()

if __name__ == "__main__":
    main()
