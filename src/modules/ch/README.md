## Module-specific dependency

- `sudo apt install libgpiod-dev`: Underlying GPIO library
- `sudo apt install libmodbus-dev`: modbus protocol support
- [libiotctrl](https://github.com/alex-lt-kong/libiotctrl): GPIO support

## Hardware configuration

- Enable `I2C interface` with `raspi-config`.
- Check status of `I2C` device with `dmesg | grep i2c`.
