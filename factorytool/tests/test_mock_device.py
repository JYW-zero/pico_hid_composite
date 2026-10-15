import unittest
from adapters.mock_device import MockDevice
from adapters.base import (
    DeviceError, CAP_MFG_FACTORY_TEST, CAP_MFG_PROVISION,
    DEVICE_FLAG_MFG_SHIPPED_LOCKED,
)
from core import errors

MODEL = "PICO2-HID-COMPOSITE"


class TestMockDevice(unittest.TestCase):
    def test_connect_and_identify_unlocked(self):
        d = MockDevice(model=MODEL)
        d.connect()
        info = d.identify()
        self.assertEqual(info.model, MODEL)
        self.assertTrue(info.connected)
        self.assertTrue(info.capabilities & (1 << CAP_MFG_FACTORY_TEST))
        self.assertTrue(info.capabilities & (1 << CAP_MFG_PROVISION))
        self.assertEqual(info.device_flags, 0)
        self.assertTrue(d.is_mfg_unlocked())
        d.disconnect()

    def test_connect_failure(self):
        d = MockDevice(model=MODEL, faults=["connect"])
        with self.assertRaises(DeviceError) as ctx:
            d.connect()
        self.assertEqual(ctx.exception.code, errors.TIMEOUT)

    def test_identify_failure(self):
        d = MockDevice(model=MODEL, faults=["identify"])
        d.connect()
        with self.assertRaises(DeviceError) as ctx:
            d.identify()
        self.assertEqual(ctx.exception.code, errors.TIMEOUT)

    def test_op_requires_connection(self):
        d = MockDevice(model=MODEL)
        with self.assertRaises(DeviceError) as ctx:
            d.write_serial("P20000000001")
        self.assertEqual(ctx.exception.code, errors.DEVICE_NOT_CONNECTED)

    def test_write_serial_ok_recorded(self):
        d = MockDevice(model=MODEL)
        d.connect()
        resp = d.write_serial("P20000000001")
        self.assertTrue(resp.ok)
        self.assertEqual(d.identify().serial, "P20000000001")

    def test_write_sn_flash_failure(self):
        d = MockDevice(model=MODEL, faults=["write_sn"])
        d.connect()
        resp = d.write_serial("P20000000001")
        self.assertFalse(resp.ok)
        self.assertEqual(resp.code, errors.FLASH_WRITE)

    def test_locked_blocks_mfg(self):
        d = MockDevice(model=MODEL, mfg_locked=True)
        d.connect()
        self.assertFalse(d.is_mfg_unlocked())
        info = d.identify()
        self.assertFalse(info.capabilities & (1 << CAP_MFG_FACTORY_TEST))
        self.assertTrue(info.device_flags & DEVICE_FLAG_MFG_SHIPPED_LOCKED)
        resp = d.write_serial("P20000000001")
        self.assertEqual(resp.code, errors.MFG_LOCKED)

    def test_locked_via_fault_token(self):
        d = MockDevice(model=MODEL, faults=["mfg_locked"])
        d.connect()
        self.assertFalse(d.is_mfg_unlocked())
        self.assertEqual(d.write_serial("P20000000001").code, errors.MFG_LOCKED)

    def test_self_test_pass_measurements(self):
        d = MockDevice(model=MODEL)
        d.connect()
        resp = d.run_self_test("keypad")
        self.assertTrue(resp.ok)
        self.assertEqual(resp.details["measurements"]["keys_total"], 64)

    def test_self_test_specific_failure(self):
        d = MockDevice(model=MODEL, faults=["selftest:keypad"])
        d.connect()
        resp = d.run_self_test("keypad")
        self.assertEqual(resp.code, errors.SELF_TEST_FAIL)
        self.assertEqual(resp.details["measurements"]["keys_ok"], 63)
        # 未注入故障的其他自检仍通过
        self.assertTrue(d.run_self_test("sensor").ok)

    def test_wrong_model(self):
        d = MockDevice(model=MODEL, faults=["wrong_model"])
        d.connect()
        self.assertEqual(d.identify().model, "UNKNOWN-DEVICE")

    def test_ship_lock(self):
        d = MockDevice(model=MODEL)
        d.connect()
        self.assertTrue(d.ship_lock().ok)
        self.assertFalse(d.is_mfg_unlocked())


if __name__ == "__main__":
    unittest.main()
