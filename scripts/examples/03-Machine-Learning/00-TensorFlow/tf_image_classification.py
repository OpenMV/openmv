# This work is licensed under the MIT license.
# Copyright (c) 2013-2024 OpenMV LLC. All rights reserved.
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# TensorFlow Lite Person Detection Example
#
# This example runs on the OpenMV RT1062 to detect people
# using the built-in MobileNet model.

import sensor
import time
import ml

sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.skip_frames(time=2000)

model = ml.Model("/rom/person_detect.tflite")
print(model)

clock = time.clock()
while True:
    clock.tick()
    img = sensor.snapshot()

    # This combines the labels and confidence values into a list of tuples
    # and then sorts that list by the confidence values.
    scores = sorted(
        zip(model.labels, model.predict([img])[0].flatten().tolist()),
        key=lambda x: x[1],
        reverse=True
    )

    print(clock.fps(), "fps\t", "%s = %f\t" % (scores[0][0], scores[0][1]))
