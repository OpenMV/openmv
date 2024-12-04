/*
 * This file is part of the OpenMV project.
 *
 * Copyright (c) 2019 STMicroelectronics
 *
 * This work is licensed under the MIT license, see the file LICENSE for
 * details.
 */

/* System headers */
#include "nn_st.h"
#include "ai_platform_interface.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void crc_init(void) {
  CRC_HandleTypeDef hcrc;

  __HAL_RCC_CRC_CLK_ENABLE();

  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  HAL_CRC_Init(&hcrc);
}

AI_ALIGNED(4)
static ai_float out_data[AI_NETWORK_OUT_1_SIZE];
ai_network_exec_ctx network_handle;

#define AI_BUFFER_NULL(ptr_)                                                   \
  AI_BUFFER_OBJ_INIT(AI_BUFFER_FORMAT_NONE | AI_BUFFER_FMT_FLAG_CONST, 0, 0,   \
                     0, 0, AI_HANDLE_PTR(ptr_))

/**
 * @brief Prints the layout of the ai_buffer (mostly for debug)
 *
 * @param msg null-terminated string added to the message
 * @param idx An integer added to the message
 * @param buffer ai_buffer to be inspected
 * @return void
 */
__STATIC_INLINE void aiPrintLayoutBuffer(const char *msg, int idx,
                                         const ai_buffer *buffer) {
  uint32_t type_id = AI_BUFFER_FMT_GET_TYPE(buffer->format);
  printf("%s [%d]          : shape(HWC):(%d,%d,%ld) format=", msg, idx,
         buffer->height, buffer->width, buffer->channels);
  if (type_id == AI_BUFFER_FMT_TYPE_Q)
    printf("Q%d.%d (%dbits, %s)",
           (int)AI_BUFFER_FMT_GET_BITS(buffer->format) -
               ((int)AI_BUFFER_FMT_GET_FBITS(buffer->format) +
                (int)AI_BUFFER_FMT_GET_SIGN(buffer->format)),
           AI_BUFFER_FMT_GET_FBITS(buffer->format),
           (int)AI_BUFFER_FMT_GET_BITS(buffer->format),
           AI_BUFFER_FMT_GET_SIGN(buffer->format) ? "signed" : "unsigned");
  else if (type_id == AI_BUFFER_FMT_TYPE_FLOAT)
    printf("FLOAT (%dbits, %s)", (int)AI_BUFFER_FMT_GET_BITS(buffer->format),
           AI_BUFFER_FMT_GET_SIGN(buffer->format) ? "signed" : "unsigned");
  else
    printf("NONE");
  printf(" size=%ldbytes\r\n",
         AI_BUFFER_BYTE_SIZE(AI_BUFFER_SIZE(buffer), buffer->format));
}

/**
 * @brief Displays information about the network to serial port
 *
 * @param report - An ai_network_report structure to be displayed
 */
void aiPrintNetworkInfo(const ai_network_report *report) {
  printf("Network configuration...\r\n");
  printf(" Model name         : %s\r\n", report->model_name);
  printf(" Model signature    : %s\r\n", report->model_signature);
  printf(" Model datetime     : %s\r\n", report->model_datetime);
  printf(" Compile datetime   : %s\r\n", report->compile_datetime);
  printf(" Runtime revision   : %s (%d.%d.%d)\r\n", report->runtime_revision,
         report->runtime_version.major, report->runtime_version.minor,
         report->runtime_version.micro);
  printf(" Tool revision      : %s (%d.%d.%d)\r\n", report->tool_revision,
         report->tool_version.major, report->tool_version.minor,
         report->tool_version.micro);
  printf("Network info...\r\n");
  printf("  nodes             : %ld\r\n", report->n_nodes);
  printf("  complexity        : %ld MACC\r\n", report->n_macc);
  printf("  activation        : %ld bytes\r\n",
         aiBufferSize(&report->activations));
  printf("  params            : %ld bytes\r\n", aiBufferSize(&report->params));
  printf("  inputs/outputs    : %u/%u\r\n", report->n_inputs,
         report->n_outputs);
  int i;
  for (i = 0; i < report->n_inputs; i++) {
    aiPrintLayoutBuffer("   IN ", i, &report->inputs[i]);
  }
  for (i = 0; i < report->n_outputs; i++) {
    aiPrintLayoutBuffer("   OUT", i, &report->outputs[i]);
  }
}

ai_u32 aiBufferSize(const ai_buffer *buffer) {
  return buffer->height * buffer->width * buffer->channels;
}

/**
 * @brief Displays information about the errors which can occur with Cube.AI C
 * API
 *
 * @param err an ai_error struct
 * @param fct a string to be displayed with the message
 */
void aiLogErr(const ai_error err, const char *fct) {
  if (fct) {
    printf("E: AI error (%s) - type=%d code=%d\r\n", fct, err.type, err.code);
  } else {
    printf("E: AI error - type=%d code=%d\r\n", err.type, err.code);
  }
}

/**
 * @brief Initialization code for the network
 *
 * @param nn_name the name of the network
 * @return int error code, 0 if it's ok, anything else is error
 */
static int aiBootstrap(const char *nn_name) {
  crc_init();
  ai_error err;

  // Creating the network
  printf("Creating the network \"%s\"..\r\n", nn_name);
  err = ai_network_create(&network_handle.network, NULL);
  if (err.type) {
    aiLogErr(err, "ai_network_create");
    return -1;
  }

  // Query the created network to get relevant info from it
  if (ai_network_get_info(network_handle.network, &network_handle.report)) {
    aiPrintNetworkInfo(&network_handle.report);
  } else {
    err = ai_network_get_error(network_handle.network);
    aiLogErr(err, "ai_network_get_info");
    ai_network_destroy(network_handle.network);
    network_handle.network = AI_HANDLE_NULL;
    return -2;
  }
  // Initialize the instance
  printf("Initializing the network\r\n");
  return 0;
}

AI_DECLARE_STATIC
ai_bool ai_mnetwork_is_valid(const char *network_name, const char *name) {
  if (network_name && (strlen(name) == strlen(network_name)) &&
      (strncmp(name, network_name, strlen(name)) == 0)) {
    return true;
  }
  return false;
}

/**
 * @brief Network initialziation
 *
 * @param network_name name of the network
 * @param net stnn_t structure
 */
void aiInit(const char *network_name, stnn_t *net) {
  const char *name;
  printf("\r\nAI platform (API %d.%d.%d - RUNTIME %d.%d.%d)\r\n",
         AI_PLATFORM_API_MAJOR, AI_PLATFORM_API_MINOR, AI_PLATFORM_API_MICRO,
         AI_PLATFORM_RUNTIME_MAJOR, AI_PLATFORM_RUNTIME_MINOR,
         AI_PLATFORM_RUNTIME_MICRO);

  // Discover and init the embedded network
  name = (const char *)AI_NETWORK_MODEL_NAME;
  if (ai_mnetwork_is_valid(network_name, name)) {
    printf("\r\nFound network \"%s\"\r\n", name);
    aiBootstrap(name);

  } else {
    printf("\r\error network name!, please enter the right name \"%s\"\r\n",
           name);
  }
  net->nn_exec_ctx_ptr = &network_handle;
}

/**
 * @brief Runs the inference of the network. Calls preprocessing function on the
 * img before running the inference
 *
 * @param net Python object
 * @param img input image
 * @param roi region of interest
 * @return int error code
 */
int aiRun(stnn_t *net, image_t *img, rectangle_t *roi) {
  ai_i32 nbatch;
  ai_error err;

  fb_alloc_mark();
  AI_ALIGNED(4)
  ai_u8 *activations = fb_alloc(AI_NETWORK_DATA_ACTIVATIONS_SIZE);
  AI_ALIGNED(4)
  ai_u8 *in_data = fb_alloc(AI_NETWORK_IN_1_SIZE_BYTES);

  // build params structure to provide the reference of the
  // activation and weight buffers
  const ai_network_params params = {
      AI_NETWORK_DATA_WEIGHTS(ai_network_data_weights_get()),
      AI_NETWORK_DATA_ACTIVATIONS(activations)};

  if (!ai_network_init(net->nn_exec_ctx_ptr->network, &params)) {
    err = ai_network_get_error(net->nn_exec_ctx_ptr->network);
    aiLogErr(err, "ai_network_init");
    ai_network_destroy(net->nn_exec_ctx_ptr->network);
    net->nn_exec_ctx_ptr->network = AI_HANDLE_NULL;
  }

  ai_transform_input(net->nn_exec_ctx_ptr->report.inputs, img, in_data, roi);

  /* Create the AI buffer IO handlers */
  ai_buffer ai_input[1];
  ai_buffer ai_output[1];

  ai_input[0] = net->nn_exec_ctx_ptr->report.inputs[0];
  ai_output[0] = net->nn_exec_ctx_ptr->report.outputs[0];

  /* Initialize input/output buffer handlers */
  ai_input[0].n_batches = 1;
  ai_input[0].data = AI_HANDLE_PTR(in_data);

  ai_output[0].n_batches = 1;
  ai_output[0].data = AI_HANDLE_PTR(out_data);

  /* Perform the inference */
  nbatch = ai_network_run(net->nn_exec_ctx_ptr->network, &ai_input[0],
                          &ai_output[0]);
  if (nbatch != 1) {
    err = ai_network_get_error(net->nn_exec_ctx_ptr->network);
    printf("AI error (ai_network_run) code= %d\n", err.code);
  }

  net->nn_exec_ctx_ptr->report.outputs->data = out_data;

  fb_alloc_free_till_mark();

  return 0;
}

/**
 * @brief Preprocessing function to prepare the data before inference
 *
 * @param input_net[in] structure holding information about the expected input
 * by the network
 * @param img[in] input image from the sensor
 * @param input_data[] transformed data to be feed to the network
 * @param roi[in] region of interest
 */
void ai_transform_input(ai_buffer *input_net, image_t *img, ai_u8 *input_data,
                        rectangle_t *roi) {

  // Example for MNIST CNN
  // Cast to float pointer
  ai_float *_input_data = (ai_float *)input_data;
  int x_ratio = (int)((roi->w << 16) / input_net->width) + 1;
  int y_ratio = (int)((roi->h << 16) / input_net->height) + 1;

  for (int y = 0, i = 0; y < input_net->height; y++) {
    int sy = (y * y_ratio) >> 16;
    for (int x = 0; x < input_net->width; x++, i++) {
      int sx = (x * x_ratio) >> 16;
      uint8_t p = IM_GET_GS_PIXEL(img, sx + roi->x, sy + roi->y);
      _input_data[i] = (float)(p / 255.0f);
    }
  }

  ///////////////////////////////////////////////////////////////////////////
  // Below is an example code for a quantized model expecting a RGB565 image

  /*
// Scale, convert and normalize input image.
int q_input_shift = 1;
int x_ratio = (int)((roi->w << 16) / input_net->width) + 1;
int y_ratio = (int)((roi->h << 16) / input_net->height) + 1;

if ((img->bpp == 2) && (input_net->channels == 3)) {

for (int y = 0, i = 0; y < input_net->height; y++) {
int sy = (y * y_ratio) >> 16;
for (int x = 0; x < input_net->width; x++, i += 3) {
  int sx = (x * x_ratio) >> 16;
  uint16_t p = IM_GET_RGB565_PIXEL(img, sx + roi->x, sy + roi->y);
  input_data[i + 0] =
      (ai_u8)__USAT((COLOR_RGB565_TO_R8(p) + (1 << q_input_shift)) >>
                        (q_input_shift + 1),
                    8);
  input_data[i + 1] =
      (ai_u8)__USAT((COLOR_RGB565_TO_G8(p) + (1 << q_input_shift)) >>
                        (q_input_shift + 1),
                    8);
  input_data[i + 2] =
      (ai_u8)__USAT((COLOR_RGB565_TO_B8(p) + (1 << q_input_shift)) >>
                        (q_input_shift + 1),
                    8);
}
}
}
  */
}

/**
 * @brief Free network memory
 *
 */
void aiDeInit(void) {
  ai_error err;

  printf("Releasing the network(s)...\r\n");

  if (network_handle.network != AI_HANDLE_NULL) {
    if (ai_network_destroy(network_handle.network) != AI_HANDLE_NULL) {
      err = ai_network_get_error(network_handle.network);
      aiLogErr(err, "ai_network_destroy");
    }
  }
}
