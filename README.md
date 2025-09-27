# MCP9808 Linux Kernel Driver w/ HomeKit Layer
* A C-based Linux kernel driver, instantiated via devicetree, for the MCP9808 temperature sensor.
* Temperature readings are read over I2C and written to hwmon as device temperature metrics.
* A HAP-python layer reads these temperature readings from hwmon and broadcasts sensor metrics into Apple HomeKit, turning the Pi Zero 2 W into a Linux-based smart temperature sensor.

## HomeKit screenshots
<img width="215" height="466" alt="IMG_7659" src="https://github.com/user-attachments/assets/654f1e5d-9c6c-498d-9c3d-01b82fb21334" /> ![IMG_7658 (1)](https://github.com/user-attachments/assets/1bcd6ed3-943a-416d-92ae-635d6bb28b3f)

## Installation and setup

* Note: The driver and HomeKit layer should be compatible with many Linux-based boards like the Pi Zero 2 W, but I chose the Pi Zero 2 W here because it was relatively inexpensive and had an I2C bus. Steps here assume you're using a Pi Zero 2 W.

### Hardware prerequisites
- Raspberry Pi Zero 2 W (with header)
- Adafruit MCP9808 I2C temperature sensor
- 16GB or higher microSD card
- 5V/2A or higher power supply with micro-USB cable
- Wi-Fi and internet access

For the purposes of this project, I used a Qwiic pHat as well as the Qwiic-compatible MCP9808 for quick setup without soldering.

### Software prerequisites
#### Driver
* microSD card should have Raspberry Pi OS Lite flashed (tested with kernel 6.12.25)
* `build-essential`
* `git` 
* `i2c-tools` 
* `raspberrypi-kernel-headers`

#### HomeKit layer
* Recent version of `python3-dev` and `python3-venv` (tested on Python 3.11.2)
* [HAP-python](https://github.com/ikalchev/HAP-python) including dependency `libavahi-compat-libdnssd-dev`

It is expected that the user has installed all dependencies written above, logged into the Raspberry Pi, and connected it to internet.

### Step one: Configuring the kernel
Modify `/boot/firmware/config.txt` (use any text editor like `nano` with superuser permissions):

A. Uncomment `dtparam=i2c_arm=on` by removing `#` to enable I2C support, if not already enabled.

B. Write `dtoverlay=mcp9808-personal` under the `[all]` line to allow the driver to be instantiated at boot.

* Don't reboot for now, since the overlay hasn't been installed yet.

### Step two: Installing the driver
A. Clone the repository to your device and build into kernel modules: 
```
git clone https://github.com/lolrepeatlol/mcp9808_personal
cd mcp9808_personal
make -C /lib/modules/$(uname -r)/build M=$PWD modules
```

B. Install the driver into the modules tree and load the driver
```
sudo install -D -m 0644 mcp9808-personal.ko /lib/modules/$(uname -r)/extra/mcp9808-personal.ko
sudo depmod -a
sudo modprobe mcp9808-personal
```

C. Compile the overlay for booting and reboot
```
dtc -@ -I dts -O dtb -o mcp9808-personal.dtbo mcp9808-personal-overlay.dts
sudo cp mcp9808-personal.dtbo /boot/firmware/overlays/
sudo reboot
```

D. Verify the device and driver are seen:
* Input:
```
for d in /sys/class/hwmon/hwmon*; do printf "%s -> " "$d"; cat "$d/name"; done
```
should lead to output similar to:
```
/sys/class/hwmon/hwmon0 -> cpu_thermal 
/sys/class/hwmon/hwmon1 -> rpi_volt 
/sys/class/hwmon/hwmon2 -> mcp9808_personal
```
* Optionally, read the temperature directly from hwmon by reading its output:
	* (Replace `hwmon2` with the path to the driver if necessary)
```
cat /sys/class/hwmon/hwmon2/temp1_input
```
* The output will be provided in milli-degrees Celsius.

### Step 3: Setting up HomeKit integration
* HomeKit integration should work immediately once all dependencies are installed and the device is connected to the internet. 
* Simply run `python3 mcp9808_homekit.py` from the device's terminal to get going.
* The HomeKit script automatically finds the driver from `hwmon` and reliably broadcasts it to HomeKit. Scan the setup code from your iOS/iPadOS device to add to Apple Home.

## Testing the driver
* To test the driver, use a device with a recent version of the Linux kernel and `CONFIG_I2C_STUB` enabled or available as a module. 
	* I used Ubuntu 24.04 (ARM64) under a VM, which satisfied this requirement without needing a custom kernel build.
	* One could instead choose to build the kernel for Raspberry Pi OS Lite with this configuration option on if desired.
	* I've quickly detailed steps for testing on Ubuntu 24.04 below.

### 1. Install dependencies 
* Install dependencies and load the kernel module for `i2c-stub`.
```
sudo apt install linux-headers-$(uname -r) build-essential i2c-tools
modprobe i2c-stub 
```

### 2. Build and load the driver
```
make
sudo insmod ./mcp9808-personal.ko
sudo modprobe i2c-dev
```

### 3. Bind the driver to the stubbed device and double-check that it loaded
```
echo mcp9808-personal 0x18 | sudo tee /sys/bus/i2c/devices/i2c-0/new_device
ls -l /sys/bus/i2c/devices/0-0018/driver
```
should see output similar to:
```
lrwxrwxrwx 1 root root 0 Sep 7 20:00 /sys/bus/i2c/devices/0-0018/driver -> ../../../bus/i2c/drivers/mcp9808-personal
```

### 4. Set the hwmon device for the script
* Run:
```
for d in /sys/class/hwmon/hwmon*; do printf "%s -> " "$d"; cat "$d/name"; done
```
* Output should be similar to:
```
/sys/class/hwmon/hwmon0 -> cpu_thermal 
/sys/class/hwmon/hwmon1 -> rpi_volt 
/sys/class/hwmon/hwmon2 -> mcp9808_personal
```
* Set the HWMON environment variable (the script defaults to hwmon2):
```
export HWMON=/sys/class/hwmon/hwmon2
```

### 5. Run the tests
* Execute the script:
```
chmod +x ./run_stub_tests.sh
./run_stub_tests.sh
```

* Verify that all output is marked "OK" and not "FAIL."
