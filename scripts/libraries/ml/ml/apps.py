# Copyright (C) 2024 OpenMV, LLC.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Any redistribution, use, or modification in source or binary form
#    is done solely for personal benefit and not for any commercial
#    purpose or for monetary gain. For commercial licensing options,
#    please contact openmv@openmv.io
#
# THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT
# OWNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
import time
from ml import Model
from micropython import const
from ulab import numpy as np

try:
    import audio
except (ImportError, AttributeError):
    pass


class MicroSpeech:
    _SLICE_SIZE = const(40)
    _SLICE_COUNT = const(49)
    _SLICE_TIME_MS = const(30)
    _AUDIO_FREQUENCY = const(16000)
    _SAMPLES_PER_STEP = const(10 * (_AUDIO_FREQUENCY // 1000))  # 10ms * 16 Samples/ms
    _CATEGORY_COUNT = const(4)
    _AVERAGE_WINDOW_SAMPLES = const(1020 // _SLICE_TIME_MS)

    def __init__(self, preprocessor=None, micro_speech=None, labels=None, **kwargs):
        self.preprocessor = preprocessor
        if preprocessor is None:
            self.preprocessor = Model("/rom/audio_preprocessor.tflite")
        self.labels, self.micro_speech = (labels, micro_speech)
        if micro_speech is None:
            self.micro_speech = Model("/rom/micro_speech.tflite")
            self.labels = self.micro_speech.labels
        # 16 samples/1ms
        self.audio_buffer = np.zeros((1, _SAMPLES_PER_STEP * 3), dtype=np.int16)
        self.spectrogram = np.zeros((1, _SLICE_COUNT * _SLICE_SIZE), dtype=np.int8)
        self.pred_history = np.zeros((_AVERAGE_WINDOW_SAMPLES, _CATEGORY_COUNT), dtype=np.float)
        self.audio_started = False
        audio.init(channels=1, frequency=_AUDIO_FREQUENCY, samples=_SAMPLES_PER_STEP * 2, **kwargs)

    def audio_callback(self, buf):
        # Roll the audio buffer to the left, and add the new samples.
        self.audio_buffer = np.roll(self.audio_buffer, -(_SAMPLES_PER_STEP * 2), axis=1)
        self.audio_buffer[0, _SAMPLES_PER_STEP:] = np.frombuffer(buf, dtype=np.int16)

        # Roll the spectrogram to the left and add the new slice.
        self.spectrogram = np.roll(self.spectrogram, -_SLICE_SIZE, axis=1)
        self.spectrogram[0, -_SLICE_SIZE:] = self.preprocessor.predict([self.audio_buffer])

        # Roll the prediction history and add the new prediction.
        self.pred_history = np.roll(self.pred_history, -1, axis=0)
        self.pred_history[-1] = self.micro_speech.predict([self.spectrogram])[0]

    def start_audio_streaming(self):
        if self.audio_started is False:
            self.spectrogram[:] = 0
            self.pred_history[:] = 0
            audio.start_streaming(self.audio_callback)
            self.audio_started = True

    def stop_audio_streaming(self):
        audio.stop_streaming()
        self.audio_started = False

    def listen(self, timeout=0, callback=None, threshold=0.65, filter=["Yes", "No"]):
        self.start_audio_streaming()
        stat_ms = time.ticks_ms()
        while True:
            average_scores = np.mean(self.pred_history, axis=0)
            max_score_index = np.argmax(average_scores)
            max_score = average_scores[max_score_index]
            label = self.labels[max_score_index]
            if max_score > threshold and label in filter:
                self.pred_history[:] = 0
                self.spectrogram[:] = 0
                if callback is None:
                    if timeout != -1:  # non-blocking mode
                        self.stop_audio_streaming()
                    return (label, average_scores)
                callback(label, average_scores)
            if timeout == -1:  # non-blocking mode
                return (None, average_scores)
            if timeout != 0 and (time.ticks_ms() - stat_ms) > timeout:
                self.stop_audio_streaming()
                return (None, average_scores)
            time.sleep_ms(1)
