# Sensor data pipeline

Collect data from sensor and send the readings for further processing.

## Environment and dependency

- `cURL`: `apt install libcurl4-gnutls-dev`
- JSON: `apt install libjson-c-dev`
- [libiotctrl](https://github.com/alex-lt-kong/libiotctrl): GPIO support
- Enable `I2C interface` with `raspi-config`.
- Check status of `I2C` device with `dmesg | grep i2c`.

### Installation

<img src="./assets/installation.jpg"></img>
