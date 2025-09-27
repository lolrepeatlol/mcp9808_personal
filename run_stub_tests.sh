#!/usr/bin/env bash
set -euo pipefail

# discover defaults if not pre-set
: "${BUS:=0}"
: "${HWMON:=/sys/class/hwmon/hwmon2}"

# swap a 16-bit word from big endian to little endian
swap16() {
  local w=$1
  printf "0x%04X" $(( ((w & 0xFF) << 8) | ((w >> 8) & 0xFF) ))
}

FAILS=0

assert_temp() {
  local be=$1 expected=$2  # grab big endian and create an expected var
  local le  
  le=$(swap16 "$be")  # swap endianness from smbus to get expected results

  sudo i2cset -f -y "$BUS" 0x18 0x05 "$le" w  # write to the stub defined by the datasheet

  local raw drv_be val  
  raw=$(i2cget -f -y "$BUS" 0x18 0x05 w)  # get back the raw bits from smbus
  drv_be=$(( ((raw & 0xff) << 8) | (raw >> 8) ))  # what driver masks/scales
  printf "stub_raw=0x%04x driver_reg=0x%04x\n" "$raw" "$drv_be"

  val=$(cat "$HWMON/temp1_input")  # read sysfs file from hwmon
  if [[ "$val" != "$expected" ]]; then  # check if correct value
    echo "FAIL be=$(printf '0x%04X' "$be")(le=$le) got=$val expected=$expected"
    ((FAILS++))
  else
    echo "OK   be=$(printf '0x%04X' "$be")(le=$le) -> $val"
  fi
}

# core vectors (temperature in mc as second parameter)
assert_temp 0x0000 0
assert_temp 0x0190 25000
assert_temp 0x07D0 125000
assert_temp 0x1FFF -63
assert_temp 0x1D80 -40000
assert_temp 0xE190 25000  # flags ignored

# boundary edges
assert_temp 0x07CF 124938  # +124.9375 C
assert_temp 0x1D81 -39938  # -39.9375 C

# concurrency testing
export HWMON
seq 1 50 | xargs -P50 -I{} bash -lc "for i in {1..1500}; do cat \"$HWMON/temp1_input\" >/dev/null; done"  # 50 parallel readers with 1500 reads, ensure mutex stability

# repeatedly delete and recreate client to run probe/remove over and over again
for i in {1..30}; do
  echo 0x18 | sudo tee "/sys/bus/i2c/devices/i2c-$BUS/delete_device" >/dev/null || true
  echo mcp9808-personal 0x18 | sudo tee "/sys/bus/i2c/devices/i2c-$BUS/new_device" >/dev/null || exit 1
done

# final status
if [[ $FAILS -ne 0 ]]; then
  echo "Test failures: $FAILS"
  exit 1
fi
echo "All tests passed."
