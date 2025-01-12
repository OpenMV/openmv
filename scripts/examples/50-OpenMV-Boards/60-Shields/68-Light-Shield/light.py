# This work is licensed under the MIT license.
# Copyright (c) 2013-2024 OpenMV LLC. All rights reserved.
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# This example shows off using the light shield with both the PYB and Machine Module.

import time


try:
    from pyb import Pin, Timer

    # 50kHz pin6 timer2 channel1
    light = Timer(2, freq=50000).channel(1, Timer.PWM, pin=Pin("P6"))

    while True:
        for i in range(101):
            light.pulse_width_percent(i)
            time.sleep_ms(10)
        for i in range(101):
            light.pulse_width_percent(100 - i)
            time.sleep_ms(10)


except ImportError:
    from machine import PWM, Pin

    # 50kHz pin6 timer2 channel1
    pwm = PWM(Pin("P6"), freq=50000, duty_u16=0)

    while True:
        for i in range(101):
            pwm.duty_u16((i * 65535) // 100)
            time.sleep_ms(10)
        for i in range(101):
            pwm.duty_u16(((100 - i) * 65535) // 100)
            time.sleep_ms(10)
