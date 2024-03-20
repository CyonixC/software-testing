# Copyright 2021-2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# -----------------------------------------------------------------------------
# Imports
# -----------------------------------------------------------------------------
import asyncio
import os
import logging
import time

import click

from bumble.company_ids import COMPANY_IDENTIFIERS
from bumble.colors import color
from bumble.core import name_or_number
from bumble.hci import (
    map_null_terminated_utf8_string,
    LeFeatureMask,
    HCI_SUCCESS,
    HCI_VERSION_NAMES,
    LMP_VERSION_NAMES,
    HCI_Command,
    HCI_Command_Complete_Event,
    HCI_Command_Status_Event,
    HCI_READ_BUFFER_SIZE_COMMAND,
    HCI_Read_Buffer_Size_Command,
    HCI_READ_BD_ADDR_COMMAND,
    HCI_Read_BD_ADDR_Command,
    HCI_READ_LOCAL_NAME_COMMAND,
    HCI_Read_Local_Name_Command,
    HCI_LE_READ_BUFFER_SIZE_COMMAND,
    HCI_LE_Read_Buffer_Size_Command,
    HCI_LE_READ_MAXIMUM_DATA_LENGTH_COMMAND,
    HCI_LE_Read_Maximum_Data_Length_Command,
    HCI_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS_COMMAND,
    HCI_LE_Read_Number_Of_Supported_Advertising_Sets_Command,
    HCI_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH_COMMAND,
    HCI_LE_Read_Maximum_Advertising_Data_Length_Command,
    HCI_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH_COMMAND,
    HCI_LE_Read_Suggested_Default_Data_Length_Command,
    HCI_Read_Local_Version_Information_Command,
)
from bumble.host import Host
from bumble.transport import open_transport_or_link


# -----------------------------------------------------------------------------
def command_succeeded(response):
    if isinstance(response, HCI_Command_Status_Event):
        return response.status == HCI_SUCCESS
    if isinstance(response, HCI_Command_Complete_Event):
        return response.return_parameters.status == HCI_SUCCESS
    return False


# -----------------------------------------------------------------------------
async def get_classic_info(host: Host) -> None:
    if host.supports_command(HCI_READ_BD_ADDR_COMMAND):
        response = await host.send_command(HCI_Read_BD_ADDR_Command())
        if command_succeeded(response):
            print()
            print(
                color('Classic Address:', 'yellow'),
                response.return_parameters.bd_addr.to_string(False),
            )

    if host.supports_command(HCI_READ_LOCAL_NAME_COMMAND):
        response = await host.send_command(HCI_Read_Local_Name_Command())
        if command_succeeded(response):
            print()
            print(
                color('Local Name:', 'yellow'),
                map_null_terminated_utf8_string(response.return_parameters.local_name),
            )


# -----------------------------------------------------------------------------
async def get_le_info(host: Host) -> None:
    print()

    if host.supports_command(HCI_LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS_COMMAND):
        response = await host.send_command(
            HCI_LE_Read_Number_Of_Supported_Advertising_Sets_Command()
        )
        if command_succeeded(response):
            print(
                color('LE Number Of Supported Advertising Sets:', 'yellow'),
                response.return_parameters.num_supported_advertising_sets,
                '\n',
            )

    if host.supports_command(HCI_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH_COMMAND):
        response = await host.send_command(
            HCI_LE_Read_Maximum_Advertising_Data_Length_Command()
        )
        if command_succeeded(response):
            print(
                color('LE Maximum Advertising Data Length:', 'yellow'),
                response.return_parameters.max_advertising_data_length,
                '\n',
            )

    if host.supports_command(HCI_LE_READ_MAXIMUM_DATA_LENGTH_COMMAND):
        response = await host.send_command(HCI_LE_Read_Maximum_Data_Length_Command())
        if command_succeeded(response):
            print(
                color('Maximum Data Length:', 'yellow'),
                (
                    f'tx:{response.return_parameters.supported_max_tx_octets}/'
                    f'{response.return_parameters.supported_max_tx_time}, '
                    f'rx:{response.return_parameters.supported_max_rx_octets}/'
                    f'{response.return_parameters.supported_max_rx_time}'
                ),
                '\n',
            )

    if host.supports_command(HCI_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH_COMMAND):
        response = await host.send_command(
            HCI_LE_Read_Suggested_Default_Data_Length_Command()
        )
        if command_succeeded(response):
            print(
                color('Suggested Default Data Length:', 'yellow'),
                f'{response.return_parameters.suggested_max_tx_octets}/'
                f'{response.return_parameters.suggested_max_tx_time}',
                '\n',
            )

    print(color('LE Features:', 'yellow'))
    for feature in host.supported_le_features:
        print(LeFeatureMask(feature).name)


# -----------------------------------------------------------------------------
async def get_acl_flow_control_info(host: Host) -> None:
    print()

    if host.supports_command(HCI_READ_BUFFER_SIZE_COMMAND):
        response = await host.send_command(
            HCI_Read_Buffer_Size_Command(), check_result=True
        )
        print(
            color('ACL Flow Control:', 'yellow'),
            f'{response.return_parameters.hc_total_num_acl_data_packets} '
            f'packets of size {response.return_parameters.hc_acl_data_packet_length}',
        )

    if host.supports_command(HCI_LE_READ_BUFFER_SIZE_COMMAND):
        response = await host.send_command(
            HCI_LE_Read_Buffer_Size_Command(), check_result=True
        )
        print(
            color('LE ACL Flow Control:', 'yellow'),
            f'{response.return_parameters.hc_total_num_le_acl_data_packets} '
            f'packets of size {response.return_parameters.hc_le_acl_data_packet_length}',
        )


# -----------------------------------------------------------------------------
async def async_main(latency_probes, transport):
    print('<<< connecting to HCI...')
    async with await open_transport_or_link(transport) as (hci_source, hci_sink):
        print('<<< connected')

        host = Host(hci_source, hci_sink)
        await host.reset()

        # Measure the latency if requested
        latencies = []
        if latency_probes:
            for _ in range(latency_probes):
                start = time.time()
                await host.send_command(HCI_Read_Local_Version_Information_Command())
                latencies.append(1000 * (time.time() - start))
            print(
                color('HCI Command Latency:', 'yellow'),
                (
                    f'min={min(latencies):.2f}, '
                    f'max={max(latencies):.2f}, '
                    f'average={sum(latencies)/len(latencies):.2f}'
                ),
                '\n',
            )

        # Print version
        print(color('Version:', 'yellow'))
        print(
            color('  Manufacturer:  ', 'green'),
            name_or_number(COMPANY_IDENTIFIERS, host.local_version.company_identifier),
        )
        print(
            color('  HCI Version:   ', 'green'),
            name_or_number(HCI_VERSION_NAMES, host.local_version.hci_version),
        )
        print(color('  HCI Subversion:', 'green'), host.local_version.hci_subversion)
        print(
            color('  LMP Version:   ', 'green'),
            name_or_number(LMP_VERSION_NAMES, host.local_version.lmp_version),
        )
        print(color('  LMP Subversion:', 'green'), host.local_version.lmp_subversion)

        # Get the Classic info
        await get_classic_info(host)

        # Get the LE info
        await get_le_info(host)

        # Print the ACL flow control info
        await get_acl_flow_control_info(host)

        # Print the list of commands supported by the controller
        print()
        print(color('Supported Commands:', 'yellow'))
        for command in host.supported_commands:
            print('  ', HCI_Command.command_name(command))


# -----------------------------------------------------------------------------
@click.command()
@click.option(
    '--latency-probes',
    metavar='N',
    type=int,
    help='Send N commands to measure HCI transport latency statistics',
)
@click.argument('transport')
def main(latency_probes, transport):
    logging.basicConfig(level=os.environ.get('BUMBLE_LOGLEVEL', 'WARNING').upper())
    asyncio.run(async_main(latency_probes, transport))


# -----------------------------------------------------------------------------
if __name__ == '__main__':
    main()
