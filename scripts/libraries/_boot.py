# SPDX-License-Identifier: MIT
#
# Copyright (C) 2024 OpenMV, LLC.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import os
import sys
import vfs

main_py = """import time
from machine import LED

led = LED("LED_BLUE")

while True:
    led.on()
    time.sleep_ms(150)
    led.off()
    time.sleep_ms(100)
    led.on()
    time.sleep_ms(150)
    led.off()
    time.sleep_ms(600)
"""

readme_txt = """Thank you for supporting the OpenMV project!

To download the IDE, please visit:
https://openmv.io/pages/download

For tutorials and documentation, please visit:
http://docs.openmv.io/

For technical support and projects, please visit the forums:
http://forums.openmv.io/

Please use github to report bugs and issues:
https://github.com/openmv/openmv
"""

bdev = None
sdcard = None

if bdev is None:
    try:
        import pyb

        bdev = pyb.Flash(start=0)
        sdcard = pyb.SDCard()
        del pyb
    except Exception:
        pass

if bdev is None:
    try:
        import mimxrt
        import machine

        bdev = mimxrt.Flash()
        sdcard = machine.SDCard(1)
        del mimxrt, machine
    except Exception:
        pass

if bdev is None:
    try:
        import rp2

        bdev = rp2.Flash()
        del rp2
    except Exception:
        pass


def create_file(path, data=None):
    with open(path, "w") as f:
        if data is not None:
            f.write(data)


try:
    fat = vfs.VfsFat(bdev)
    vfs.mount(fat, "/flash")
except Exception:
    vfs.VfsFat.mkfs(bdev)
    fat = vfs.VfsFat(bdev)
    vfs.mount(fat, "/flash")
    create_file("/flash/main.py", main_py)
    create_file("/flash/README.txt", readme_txt)

os.chdir("/flash")
sys.path.append("/flash")
sys.path.append("/flash/lib")

try:
    vfs.mount(vfs.VfsRom(vfs.rom_ioctl(1)), "/rom")
    sys.path.append("/rom")
except Exception:
    pass

try:
    os.stat("SKIPSD")
    sdcard = None  # Skip SD mount
except Exception:
    pass

if sdcard is not None:
    try:
        fat = vfs.VfsFat(sdcard)
        vfs.mount(fat, "/sdcard")
        os.chdir("/sdcard")
        sys.path.append("/sdcard")
        sys.path.append("/sdcard/lib")
    except Exception:
        pass  # Fail silently

try:
    os.stat(".openmv_disk")
except Exception:
    create_file(".openmv_disk")

del os, sys, vfs, fat, bdev, sdcard
