/*
 * Copyright (C) 2024 OpenMV, LLC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Any redistribution, use, or modification in source or binary form
 *    is done solely for personal benefit and not for any commercial
 *    purpose or for monetary gain. For commercial licensing options,
 *    please contact openmv@openmv.io
 *
 * THIS SOFTWARE IS PROVIDED BY THE LICENSOR AND COPYRIGHT OWNER "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE LICENSOR OR COPYRIGHT
 * OWNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Python Machine Learning Module.
 */
#ifndef __PY_ML_H__
#define __PY_ML_H__
// TF Model Object.
typedef struct py_ml_model_obj {
    mp_obj_base_t base;
    unsigned int size;
    unsigned char *_raw;
    unsigned char *data;
    size_t memory_size;
    uint32_t memory_addr;
    bool fb_alloc;
    size_t inputs_size;
    mp_obj_tuple_t *input_shape;
    mp_obj_tuple_t *input_scale;
    mp_obj_tuple_t *input_zero_point;
    mp_obj_tuple_t *input_dtype;
    size_t outputs_size;
    mp_obj_tuple_t *output_shape;
    mp_obj_tuple_t *output_scale;
    mp_obj_tuple_t *output_zero_point;
    mp_obj_tuple_t *output_dtype;
    void *state; // Private context for the backend.
} py_ml_model_obj_t;

// Initialize a model.
int ml_backend_init_model(py_ml_model_obj_t *model);

// Run inference.
int ml_backend_run_inference(py_ml_model_obj_t *model);

// Return an input tensor by index.
void *ml_backend_get_input(py_ml_model_obj_t *model, size_t index);

// Return an output tensor by index.
void *ml_backend_get_output(py_ml_model_obj_t *model, size_t index);
#endif // __PY_ML_H__
