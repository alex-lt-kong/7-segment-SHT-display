#!/usr/bin/python3

import importlib
import json
import logging
import os
import Pi7SegPy
import signal
import time
import threading

# app_dir: the app's real address on the filesystem
app_dir = os.path.dirname(os.path.realpath(__file__))
INVALID_READING = -65536
settings = None
stop_signal = False
temperature = 0

def temp_refresh_loop():

    max_retry = 360
    global temperature
    while stop_signal is False:
        retry = 0
        while retry < max_retry and stop_signal is False:
            retry += 1
            try:
                with open('/sys/bus/w1/devices/28-01131a3efcd4/w1_slave', 'r') as f:
                    data = f.read()
                    if "YES" in data:
                        (discard, sep, reading) = data.partition(' t=')
                        temperature = round(float(reading) / 1000.0, 1)
                        break
                    else:
                        raise ValueError('YES tag not found in w1_slave')
            except Exception as e:
                logging.error(f'{e}, retry={retry}')
                time.sleep(5)
                
        if retry >= max_retry:
            temperature = 123.4
        elif retry > 1:
            logging.info('Temp sensor recovered from error')

        for i in range(600):
            if stop_signal:
                return
            time.sleep(1)

def display_refresh_loop():

    Pi7SegPy.init(
            data_pin=22,  # a.k.a. DI0
            clock_pin=11, # a.k.a. SCLK
            latch_pin=18, # a.k.a. RCLK
            registers=2,
            no_of_displays=4,
            common_cathode_type=False)

    while stop_signal is False:
        Pi7SegPy.show(
            values=[int(temperature / 100),
                    int(int(temperature) % 100 / 10),
                    int(temperature) % 10, 
                    int((temperature - int(temperature)) * 10)],
            dots=[2])


def cleanup(*args):

    global stop_signal
    stop_signal = True
    logging.info('Stop signal received, exiting')
    sys.exit(0)


def main():
    global settings
    with open(os.path.join(app_dir, 'settings.json'), 'r') as json_file:
        json_str = json_file.read()
        settings = json.loads(json_str)
    logging.basicConfig(
        filename=settings['app']['log_path'],
        level=logging.INFO,
        format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S',
    )

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)
    #emailer = importlib.machinery.SourceFileLoader(
    #                'emailer',
    #                settings['email']['path']
    #            ).load_module()
    #th_email = threading.Thread(target=emailer.send_service_start_notification,
    #                            kwargs={'settings_path': os.path.join(app_dir, 'settings.json'),
    #                                    'service_name': '7Seg Temperature Display',
    #                                    'log_path': settings['app']['log_path']})

    threading.Thread(target=temp_refresh_loop, args=()).start()
    threading.Thread(target=display_refresh_loop, args=()).start()

if __name__ == '__main__':

    main()