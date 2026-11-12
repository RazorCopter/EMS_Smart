#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Automated headless test suite for the Afore AF5K-SL Local GUI Simulator.
Verifies register calculation, Modbus frame responses, API state transitions,
and fault injection scenarios. Pure Python standard library (no external dependencies).
"""

import struct
import unittest
from server import (
    calculate_modbus_crc,
    get_registers_for_range,
    handle_modbus_rtu_frame,
    state,
    state_lock,
)

class TestAforeSimulator(unittest.TestCase):
    def setUp(self):
        # Reset state to default valid baseline before each test
        with state_lock:
            state["grid_power_w"] = 1250
            state["grid_voltage_v"] = 230.2
            state["inverter_temp_c"] = 38.5
            state["pv1_power_w"] = 1800
            state["pv1_voltage_v"] = 320.0
            state["pv2_power_w"] = 1200
            state["pv2_voltage_v"] = 315.0
            state["battery_soc"] = 78
            state["battery_power_w"] = -650
            state["fault_timeout"] = False
            state["fault_crc"] = False
            state["fault_partial_frame"] = False
            state["fault_out_of_range"] = False

    def test_crc16_modbus(self):
        # Standard Modbus CRC check: 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A -> 0xC5CD (little endian)
        frame = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x0A])
        crc = calculate_modbus_crc(frame)
        self.assertEqual(crc, 0xCDC5)

    def test_power_block_registers(self):
        # Power/Grid block starts at 512 (0x0200)
        regs = get_registers_for_range(512, 19)
        self.assertEqual(len(regs), 19)
        # 512: Running status
        self.assertEqual(regs[0], 1)
        # 515 (offset 3): Grid voltage 230.2V -> 2302
        self.assertEqual(regs[3], 2302)
        # 516 (offset 4): Inverter temp 38.5C -> 385
        self.assertEqual(regs[4], 385)
        # 520 (offset 8): Grid power 1250W
        self.assertEqual(regs[8], 1250)

    def test_battery_block_registers(self):
        # Battery block starts at 2000 (0x07D0)
        regs = get_registers_for_range(2000, 10)
        self.assertEqual(len(regs), 10)
        # 2000 (offset 0): Battery SoC 78%
        self.assertEqual(regs[0], 78)
        # 2001 (offset 1): Battery power -650W -> 65536 - 650 = 64886
        self.assertEqual(regs[1], 64886)
        # Signed conversion check
        signed_power = struct.unpack(">h", struct.pack(">H", regs[1]))[0]
        self.assertEqual(signed_power, -650)

    def test_modbus_rtu_frame_handling(self):
        # Request: Slave 1, FC 03, Start 512, Count 19
        req = struct.pack(">BBHH", 1, 3, 512, 19)
        req_crc = calculate_modbus_crc(req)
        req_frame = req + struct.pack("<H", req_crc)

        resp = handle_modbus_rtu_frame(req_frame)
        self.assertGreater(len(resp), 5)
        self.assertEqual(resp[0], 1) # Slave ID
        self.assertEqual(resp[1], 3) # FC
        self.assertEqual(resp[2], 38) # Byte count = 19 * 2

        # Check response CRC validity
        crc_received = struct.unpack("<H", resp[-2:])[0]
        crc_computed = calculate_modbus_crc(resp[:-2])
        self.assertEqual(crc_received, crc_computed)

    def test_fault_crc_corruption(self):
        with state_lock:
            state["fault_crc"] = True

        req = struct.pack(">BBHH", 1, 3, 512, 19)
        req_frame = req + struct.pack("<H", calculate_modbus_crc(req))
        resp = handle_modbus_rtu_frame(req_frame)

        crc_received = struct.unpack("<H", resp[-2:])[0]
        crc_computed = calculate_modbus_crc(resp[:-2])
        # Corrupted CRC must NOT match computed CRC
        self.assertNotEqual(crc_received, crc_computed)

    def test_fault_partial_frame(self):
        with state_lock:
            state["fault_partial_frame"] = True

        req = struct.pack(">BBHH", 1, 3, 512, 19)
        req_frame = req + struct.pack("<H", calculate_modbus_crc(req))
        resp = handle_modbus_rtu_frame(req_frame)

        # Truncated frame has length 4
        self.assertEqual(len(resp), 4)

    def test_fault_out_of_range(self):
        with state_lock:
            state["fault_out_of_range"] = True

        regs = get_registers_for_range(512, 19)
        self.assertEqual(regs, [65535] * 19)

if __name__ == "__main__":
    unittest.main()
