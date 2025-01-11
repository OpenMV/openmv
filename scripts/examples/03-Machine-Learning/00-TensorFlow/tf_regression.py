# This work is licensed under the MIT license.
# Copyright (c) 2013-2024 OpenMV LLC. All rights reserved.
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# TensorFlow Lite Regression Example
#
# This example shows off running a regression model on the OpenMV Cam.
# A regression model takes an input list of numbers and produces an
# output list of numbers. You may pass ndarrays arrays to predict()
# and you will get a list of the results back.
#
# Note: The input list of numbers must be the same size as the input
# tensor size of the model.

import ml
from ulab import numpy as np

# The model is built-in on the RT1062. On other OpenMV Cam's with limited flash space please grab
# the model from here: https://github.com/openmv/openmv/tree/master/src/lib/tflm/models and
# copy it to the OpenMV Cam's file system. E.g. model = ml.Model("force_int_quant.tflite")
model = ml.Model("/rom/force_int_quant.tflite")
print(model)

i = np.array([-3, -1, -2, 5, -2, 10, -1, 9, 0, # noqa
               2,  0,  9, 1, 10,  2, -1, 3, 5, # noqa
               3,  9,  3, 9,  6,  2,  6, 7, 5, # noqa
               10, 6, -1, 7,  4,  7,  8, 5, 7], # noqa
               dtype=np.int8).reshape(model.input_shape[0]) # noqa

print(model.predict([i])[0])
# Should print 53.78332
