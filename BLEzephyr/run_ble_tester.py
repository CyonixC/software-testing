#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# Imports
# -----------------------------------------------------------------------------
import logging
import asyncio
import sys
import os
from binascii import hexlify

from bumble.device import Device, Peer
from bumble.host import Host
from bumble.gatt import show_services
from bumble.core import ProtocolError
from bumble.controller import Controller
from bumble.link import LocalLink
from bumble.transport import open_transport_or_link
from bumble.utils import AsyncRunner
from bumble.colors import color

cpp_fifo_name = "./pipe/cpp.fifo"
python_fifo_name = "./pipe/python.fifo"

# Open the FIFO pipe for reading
rfd = 0
wfd = 0
hci_source = None

def fifo_read(rfd):
    msg_len = int.from_bytes(os.read(rfd, 2), byteorder='little')    
    msg = os.read(rfd, msg_len)
    return msg, msg_len

def fifo_write(wfd, message):
    len_bytes = int.to_bytes(len(message), length=2, byteorder='little')
    os.write(wfd, len_bytes)
    os.write(wfd, str.encode(message))

async def write_target(target, attribute, bytes):
    # Write
    try:
        bytes_to_write = bytearray(bytes)
        await asyncio.wait_for(target.write_value(attribute, bytes_to_write, True), 1)
        # print(color(f'[OK] WRITE Handle 0x{attribute.handle:04X} --> Bytes={len(bytes_to_write):02d}, Val={hexlify(bytes_to_write).decode()}', 'green'))
        return True
    except ProtocolError as error:
        return True
        # print(color(f'[!]  Cannot write attribute 0x{attribute.handle:04X}:', 'yellow'), error)
    except asyncio.TimeoutError:
        pass
        # print(color('[X] Write Timeout', 'red'))
    return False


async def read_target(target, attribute):
    # Read
    try: 
        read = await asyncio.wait_for(target.read_value(attribute), 1)
        value = read.decode('latin-1')
        # print(color(f'[OK] READ  Handle 0x{attribute.handle:04X} <-- Bytes={len(read):02d}, Val={read.hex()}', 'cyan'))
        return True
    except ProtocolError as error:
        return True
        # print(color(f'[!]  Cannot read attribute 0x{attribute.handle:04X}:', 'yellow'), error)
        return True
    except asyncio.TimeoutError:
        pass
        # print(color('[!] Read Timeout'))
    
    return False

# -----------------------------------------------------------------------------
class TargetEventsListener(Device.Listener):

    got_advertisement = False
    advertisement = None
    connection = None

    def on_advertisement(self, advertisement):

        # print(f'{color("Advertisement", "cyan")} <-- '
        #       f'{color(advertisement.address, "yellow")}')
        
        # Indicate that an from target advertisement has been received
        self.advertisement = advertisement
        self.got_advertisement = True


    @AsyncRunner.run_in_task()
    # pylint: disable=invalid-overridden-method
    async def on_connection(self, connection):
        # print(color(f'[OK] Connected!', 'green'))
        self.connection = connection

        # Discover all attributes (services, characteristitcs, descriptors, etc)
        # print('=== Discovering services')
        target = Peer(connection)
        attributes = []
        await target.discover_services()
        for service in target.services:
            attributes.append(service)
            await service.discover_characteristics()
            for characteristic in service.characteristics:
                attributes.append(characteristic)
                await characteristic.discover_descriptors()
                for descriptor in characteristic.descriptors:
                    attributes.append(descriptor)

        # print(color('[OK] Services discovered', 'green'))
        show_services(target.services)
        
        # -------- Main interaction with the target here --------
        fifo_write(wfd, "Num messages (including attribute number msg)?")
        msg, _ = fifo_read(rfd)
        msg_count = int.from_bytes(msg, byteorder='little')
        fifo_write(wfd, "Attribute?")
        msg, _ = fifo_read(rfd)
        attribute_num = int.from_bytes(msg, byteorder='little')
        # print(f"Python said: Sending {msg_count - 1} messages to attribute no. {attribute_num}")
        
        # print('=== Read/Write Attributes (Handles)')
        
        correct_attribute = None
        for attribute in attributes:
            if(attribute_num == attribute.handle):
                correct_attribute = attribute
            # print("Python said: Found attribute! " + str(attribute.handle))
        
        is_successful = True
        
        for i in range(msg_count - 1):
            fifo_write(wfd, f"#{i+1} pls?")
            msg, len = fifo_read(rfd)
            
            if(len == 0):
                msg = []
                
            # print(f"Python said: Thx for the {i+1}th message!\n Sending: [", end="")
            # for byte in msg:
            #     print(f"0x{byte:02x}", end=", ")
            # print("]")

            if(not(await write_target(target, correct_attribute, msg)) or not (await read_target(target, correct_attribute))):
                # print("Zephyr Server timed out. Ending early")
                fifo_write(wfd, f"end")
                is_successful = False
                break
        
        if(is_successful):
            fifo_write(wfd, "ok")
        else:
            fifo_write(wfd, "end")
        
        # print('---------------------------------------------------------------')
        # print(color('[OK] Communication Finished', 'green'))
        # print('---------------------------------------------------------------')
        # print(f"Python said: Received all messages.... closing pipes...")
        os.close(wfd)
        os.close(rfd)
        # print(f"Python said: Pipe closed. Exiting...")
        
        hci_source.terminated.set_result("Done")
        # ---------------------------------------------------
        
        

# -----------------------------------------------------------------------------
async def main():
    if len(sys.argv) != 2:
        # print('Usage: run_controller.py <transport-address>')
        # print('example: ./run_ble_tester.py tcp-server:0.0.0.0:9000')
        return

    # print('>>> Waiting connection to HCI...')
    
    global hci_source
    async with await open_transport_or_link(sys.argv[1]) as (hci_source, hci_sink):
        # print('>>> Connected')

        # Create a local communication channel between multiple controllers
        link = LocalLink()

        # Create a first controller for connection with host interface (Zephyr)
        zephyr_controller = Controller('Zephyr', host_source=hci_source,
                                 host_sink=hci_sink,
                                 link=link)


        # Create our own device (tester central) to manage the host BLE stack
        device = Device.from_config_file('tester_config.json')
        # Create a host for the second controller
        device.host = Host() 
        # Create a second controller for connection with this test driver (Bumble)
        device.host.controller = Controller('Fuzzer', link=link)
        # Connect class to receive events during communication with target
        device.listener = TargetEventsListener()
        
        # Start BLE scanning here
        await device.power_on()
        await device.start_scanning() # this calls "on_advertisement"

        # print('Waiting Advertisment from BLE Target')
        
        global rfd
        global wfd
        rfd = os.open(python_fifo_name, os.O_RDONLY)
        wfd = os.open(cpp_fifo_name, os.O_WRONLY)
        
        while device.listener.got_advertisement is False:
            await asyncio.sleep(0.5)
        await device.stop_scanning() # Stop scanning for targets

        # print(color('\n[OK] Got Advertisment from BLE Target!', 'green'))
        target_address = device.listener.advertisement.address

        # Start BLE connection here
        # print(f'=== Connecting to {target_address}...')
        await device.connect(target_address) # this calls "on_connection"
        
        # Wait in an infinite loop
        await hci_source.wait_for_termination()


# -----------------------------------------------------------------------------
logging.basicConfig(level=os.environ.get('BUMBLE_LOGLEVEL', 'CRITICAL').upper())
asyncio.run(main())
