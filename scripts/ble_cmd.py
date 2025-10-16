import asyncio
from bleak import BleakScanner, BleakClient

DEV_NAME = "BeanOS"

SERVICE = "2b771fcc-c87d-42bd-9216-000000000000"
CTRL_RX = "2b771fcc-c87d-42bd-9216-000000000020"
CTRL_TX = "2b771fcc-c87d-42bd-9216-000000000010"
# DATA_RX = "12345678-0003-1000-8000-00805f9b34fb"
# DATA_TX = "12345678-0004-1000-8000-00805f9b34fb"

async def find_device_by_name(name: str):
    devices = await BleakScanner.discover(timeout=5.0)
    for d in devices:
        if d.name == name:
            return d
    return None

async def main():
    dev = await find_device_by_name(DEV_NAME)
    if not dev:
        raise SystemExit(f"Device named {DEV_NAME!r} not found. Make sure it's advertising.")

    async with BleakClient(dev) as client:
        # subscribe first (enables CCCD)
        await client.start_notify(CTRL_TX, lambda _, data: print("CTRL_TX:", data))
        # await client.start_notify(DATA_TX, lambda _, data: print("DATA_TX:", data))

        # simple writes
        await client.write_gatt_char(CTRL_RX, b"!!! CUSTOM\n", response=False)

        await asyncio.sleep(2.0)  # allow notifies to print

asyncio.run(main())
