import logging
import signal
from pathlib import Path

from pyhap.accessory import Accessory
from pyhap.accessory_driver import AccessoryDriver
from pyhap.const import CATEGORY_SENSOR

logging.basicConfig(level=logging.INFO, format="[%(module)s] %(message)s")

def find_mcp9808_hwmon():
    """Return Path('/sys/class/hwmon/hwmonX') whose name == mcp9808_personal."""
    for p in Path("/sys/class/hwmon").glob("hwmon*"):
        try:
            if (p / "name").read_text().strip() == "mcp9808_personal":
                return p
        except FileNotFoundError:
            pass
    raise RuntimeError("mcp9808_personal hwmon device not found")

def read_temp_c(hwmon_path: Path) -> float:
    raw = int((hwmon_path / "temp1_input").read_text().strip())
    return raw / 1000.0  # convert milli-celsius into celsius

class TemperatureSensor(Accessory):
    """Temperature sensor that reads from hwmon mcp9808_personal driver."""

    category = CATEGORY_SENSOR

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        serv_temp = self.add_preload_service('TemperatureSensor')
        self.char_temp = serv_temp.configure_char('CurrentTemperature')

        # resolve hwmon path for driver
        self.hwmon_path = find_mcp9808_hwmon()

    @Accessory.run_at_interval(3)
    def run(self):
        try:
            temp_c = read_temp_c(self.hwmon_path)
            self.char_temp.set_value(temp_c)
        except Exception as e:
            logging.warning(f"Temp read failed: {e}")


def main():
    driver = AccessoryDriver(port=51826)
    acc = TemperatureSensor(driver, "MCP9808_Temperature")
    driver.add_accessory(acc)

    # handle terminate here to allow for graceful accessory stop
    signal.signal(signal.SIGTERM, driver.signal_handler)

    logging.info("Starting HAP bridge…")
    driver.start()

if __name__ == "__main__":
    main()

