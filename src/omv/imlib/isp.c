/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2013-2024 OpenMV, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * AWB Functions
 */
#include "imlib.h"

#ifdef IMLIB_ENABLE_ISP_OPS

void imlib_awb_rgb_avg(image_t *img, uint32_t *r_out, uint32_t *g_out, uint32_t *b_out) {
    uint32_t area = img->w * img->h;
    uint32_t r_acc = 0, g_acc = 0, b_acc = 0;

    switch (img->pixfmt) {
        case PIXFORMAT_RGB565: {
            uint16_t *ptr = (uint16_t *) img->data;
            long n = area; // must be signed for count down loop

            #if defined(ARM_MATH_DSP)
            uint32_t *ptr32 = (uint32_t *) ptr;

            for (; n > 1; n -= 2) {
                uint32_t pixels = *ptr32++;

                long r = (pixels >> 11) & 0x1F001F;
                r_acc = __USADA8(r, 0, r_acc);

                long g = (pixels >> 5) & 0x3F003F;
                g_acc = __USADA8(g, 0, g_acc);

                long b = pixels & 0x1F001F;
                b_acc = __USADA8(b, 0, b_acc);
            }

            ptr = (uint16_t *) ptr32;
            #endif

            for (; n > 0; n -= 1) {
                int pixel = *ptr++;
                r_acc += COLOR_RGB565_TO_R5(pixel);
                g_acc += COLOR_RGB565_TO_G6(pixel);
                b_acc += COLOR_RGB565_TO_B5(pixel);
            }

            break;
        }
        case PIXFORMAT_BAYER_BGGR: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        g_acc = __USADA8(g, 0, g_acc);

                        long b = __UXTB16_RORn(pixels, 8);
                        b_acc = __USADA8(b, 0, b_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            b_acc += *ptr++;
                        } else {
                            g_acc += *ptr++;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long r = __UXTB16_RORn(pixels, 0);
                        r_acc = __USADA8(r, 0, r_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        g_acc = __USADA8(g, 0, g_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            g_acc += *ptr++;
                        } else {
                            r_acc += *ptr++;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GBRG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long b = __UXTB16_RORn(pixels, 0);
                        b_acc = __USADA8(b, 0, b_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        g_acc = __USADA8(g, 0, g_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            g_acc += *ptr++;
                        } else {
                            b_acc += *ptr++;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        g_acc = __USADA8(g, 0, g_acc);

                        long r = __UXTB16_RORn(pixels, 8);
                        r_acc = __USADA8(r, 0, r_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            r_acc += *ptr++;
                        } else {
                            g_acc += *ptr++;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GRBG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long r = __UXTB16_RORn(pixels, 0);
                        r_acc = __USADA8(r, 0, r_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        g_acc = __USADA8(g, 0, g_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            g_acc += *ptr++;
                        } else {
                            r_acc += *ptr++;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        g_acc = __USADA8(g, 0, g_acc);

                        long b = __UXTB16_RORn(pixels, 8);
                        b_acc = __USADA8(b, 0, b_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            b_acc += *ptr++;
                        } else {
                            g_acc += *ptr++;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_RGGB: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        g_acc = __USADA8(g, 0, g_acc);

                        long r = __UXTB16_RORn(pixels, 8);
                        r_acc = __USADA8(r, 0, r_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            r_acc += *ptr++;
                        } else {
                            g_acc += *ptr++;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long b = __UXTB16_RORn(pixels, 0);
                        b_acc = __USADA8(b, 0, b_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        g_acc = __USADA8(g, 0, g_acc);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            g_acc += *ptr++;
                        } else {
                            b_acc += *ptr++;
                        }
                    }
                }
            }

            break;
        }
        default: {
            break;
        }
    }

    if (img->is_bayer) {
        *r_out = ((r_acc * 4) + (area >> 1)) / area;
        *g_out = ((g_acc * 2) + (area >> 1)) / area;
        *b_out = ((b_acc * 4) + (area >> 1)) / area;
    } else {
        *r_out = ((r_acc * 2) + (area >> 1)) / area;
        *g_out = (g_acc + (area >> 1)) / area;
        *b_out = ((b_acc * 2) + (area >> 1)) / area;
    }
}

void imlib_awb_rgb_max(image_t *img, uint32_t *r_out, uint32_t *g_out, uint32_t *b_out) {
    uint32_t area = img->w * img->h;
    uint32_t r_acc = 0, g_acc = 0, b_acc = 0;

    switch (img->pixfmt) {
        case PIXFORMAT_RGB565: {
            uint16_t *ptr = (uint16_t *) img->data;
            long n = area; // must be signed for count down loop

            #if defined(ARM_MATH_DSP)
            uint32_t *ptr32 = (uint32_t *) ptr;

            for (; n > 1; n -= 2) {
                uint32_t pixels = *ptr32++;

                long r = (pixels >> 11) & 0x1F001F;
                long r_tmp = __USUB8(r, r_acc); (void) r_tmp;
                r_acc = __SEL(r, r_acc);

                long g = (pixels >> 5) & 0x3F003F;
                long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                g_acc = __SEL(g, g_acc);

                long b = pixels & 0x1F001F;
                long b_tmp = __USUB8(b, b_acc); (void) b_tmp;
                b_acc = __SEL(b, b_acc);
            }

            long r_tmp = r_acc >> 16;
            long r_tmp2 = __USUB8(r_tmp, r_acc); (void) r_tmp2;
            r_acc = __SEL(r_tmp, r_acc) & 0xff;

            long g_tmp = g_acc >> 16;
            long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
            g_acc = __SEL(g_tmp, g_acc) & 0xff;

            long b_tmp = b_acc >> 16;
            long b_tmp2 = __USUB8(b_tmp, b_acc); (void) b_tmp2;
            b_acc = __SEL(b_tmp, b_acc) & 0xff;

            ptr = (uint16_t *) ptr32;
            #endif

            for (; n > 0; n -= 1) {
                int pixel = *ptr++;
                int r = COLOR_RGB565_TO_R5(pixel);
                r_acc = (r > r_acc) ? r : r_acc;
                int g = COLOR_RGB565_TO_G6(pixel);
                g_acc = (g > g_acc) ? g : g_acc;
                int b = COLOR_RGB565_TO_B5(pixel);
                b_acc = (b > b_acc) ? b : b_acc;
            }

            break;
        }
        case PIXFORMAT_BAYER_BGGR: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);

                        long b = __UXTB16_RORn(pixels, 8);
                        long b_tmp = __USUB8(b, b_acc); (void) b_tmp;
                        b_acc = __SEL(b, b_acc);
                    }

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    long b_tmp = b_acc >> 16;
                    long b_tmp2 = __USUB8(b_tmp, b_acc); (void) b_tmp2;
                    b_acc = __SEL(b_tmp, b_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int b = *ptr++;
                            b_acc = (b > b_acc) ? b : b_acc;
                        } else {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long r = __UXTB16_RORn(pixels, 0);
                        long r_tmp = __USUB8(r, r_acc); (void) r_tmp;
                        r_acc = __SEL(r, r_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);
                    }

                    long r_tmp = r_acc >> 16;
                    long r_tmp2 = __USUB8(r_tmp, r_acc); (void) r_tmp2;
                    r_acc = __SEL(r_tmp, r_acc) & 0xff;

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        } else {
                            int r = *ptr++;
                            r_acc = (r > r_acc) ? r : r_acc;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GBRG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long b = __UXTB16_RORn(pixels, 0);
                        long b_tmp = __USUB8(b, b_acc); (void) b_tmp;
                        b_acc = __SEL(b, b_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);
                    }

                    long b_tmp = b_acc >> 16;
                    long b_tmp2 = __USUB8(b_tmp, b_acc); (void) b_tmp2;
                    b_acc = __SEL(b_tmp, b_acc) & 0xff;

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        } else {
                            int b = *ptr++;
                            b_acc = (b > b_acc) ? b : b_acc;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);

                        long r = __UXTB16_RORn(pixels, 8);
                        long r_tmp = __USUB8(r, r_acc); (void) r_tmp;
                        r_acc = __SEL(r, r_acc);
                    }

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    long r_tmp = r_acc >> 16;
                    long r_tmp2 = __USUB8(r_tmp, r_acc); (void) r_tmp2;
                    r_acc = __SEL(r_tmp, r_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int r = *ptr++;
                            r_acc = (r > r_acc) ? r : r_acc;
                        } else {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GRBG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long r = __UXTB16_RORn(pixels, 0);
                        long r_tmp = __USUB8(r, r_acc); (void) r_tmp;
                        r_acc = __SEL(r, r_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);
                    }

                    long r_tmp = r_acc >> 16;
                    long r_tmp2 = __USUB8(r_tmp, r_acc); (void) r_tmp2;
                    r_acc = __SEL(r_tmp, r_acc) & 0xff;

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        } else {
                            int r = *ptr++;
                            r_acc = (r > r_acc) ? r : r_acc;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);

                        long b = __UXTB16_RORn(pixels, 8);
                        long b_tmp = __USUB8(b, b_acc); (void) b_tmp;
                        b_acc = __SEL(b, b_acc);
                    }

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    long b_tmp = b_acc >> 16;
                    long b_tmp2 = __USUB8(b_tmp, b_acc); (void) b_tmp2;
                    b_acc = __SEL(b_tmp, b_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int b = *ptr++;
                            b_acc = (b > b_acc) ? b : b_acc;
                        } else {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_RGGB: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long g = __UXTB16_RORn(pixels, 0);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);

                        long r = __UXTB16_RORn(pixels, 8);
                        long r_tmp = __USUB8(r, r_acc); (void) r_tmp;
                        r_acc = __SEL(r, r_acc);
                    }

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    long r_tmp = r_acc >> 16;
                    long r_tmp2 = __USUB8(r_tmp, r_acc); (void) r_tmp2;
                    r_acc = __SEL(r_tmp, r_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int r = *ptr++;
                            r_acc = (r > r_acc) ? r : r_acc;
                        } else {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        }
                    }
                } else {
                    int n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32++;

                        long b = __UXTB16_RORn(pixels, 0);
                        long b_tmp = __USUB8(b, b_acc); (void) b_tmp;
                        b_acc = __SEL(b, b_acc);

                        long g = __UXTB16_RORn(pixels, 8);
                        long g_tmp = __USUB8(g, g_acc); (void) g_tmp;
                        g_acc = __SEL(g, g_acc);
                    }

                    long b_tmp = b_acc >> 16;
                    long b_tmp2 = __USUB8(b_tmp, b_acc); (void) b_tmp2;
                    b_acc = __SEL(b_tmp, b_acc) & 0xff;

                    long g_tmp = g_acc >> 16;
                    long g_tmp2 = __USUB8(g_tmp, g_acc); (void) g_tmp2;
                    g_acc = __SEL(g_tmp, g_acc) & 0xff;

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1) {
                        if (n % 2) {
                            int g = *ptr++;
                            g_acc = (g > g_acc) ? g : g_acc;
                        } else {
                            int b = *ptr++;
                            b_acc = (b > b_acc) ? b : b_acc;
                        }
                    }
                }
            }

            break;
        }
        default: {
            break;
        }
    }

    if (img->is_bayer) {
        *r_out = r_acc;
        *g_out = g_acc;
        *b_out = b_acc;
    } else {
        *r_out = r_acc * 2;
        *g_out = g_acc;
        *b_out = b_acc * 2;
    }
}

void imlib_awb(image_t *img, uint32_t r_out, uint32_t g_out, uint32_t b_out) {
    uint32_t area = img->w * img->h;

    int red_gain = IM_DIV(g_out * 32, r_out);
    red_gain = IM_MIN(red_gain, 128);
    int blue_gain = IM_DIV(g_out * 32, b_out);
    blue_gain = IM_MIN(blue_gain, 128);

    switch (img->pixfmt) {
        case PIXFORMAT_RGB565: {
            uint16_t *ptr = (uint16_t *) img->data;
            long n = area; // must be signed for count down loop

            #if defined(ARM_MATH_DSP)
            uint32_t *ptr32 = (uint32_t *) ptr;

            for (; n > 1; n -= 2) {
                long pixels = *ptr32;
                long r_pixels = (__USAT16(((pixels >> 11) & 0x1F001F) * red_gain, 10) << 6) & 0xF800F800;
                long g_pixels = pixels & 0x7E007E0;
                long b_pixels = (__USAT16((pixels & 0x1F001F) * blue_gain, 10) >> 5) & 0x1F001F;
                *ptr32++ = r_pixels | g_pixels | b_pixels;
            }

            ptr = (uint16_t *) ptr32;
            #endif

            for (; n > 0; n -= 1) {
                int pixel = *ptr;
                int r = __USAT_ASR(COLOR_RGB565_TO_R5(pixel) * red_gain, 5, 5);
                int g = COLOR_RGB565_TO_G6(pixel);
                int b = __USAT_ASR(COLOR_RGB565_TO_B5(pixel) * blue_gain, 5, 5);
                *ptr++ = COLOR_R5_G6_B5_TO_RGB565(r, g, b);
            }

            break;
        }
        case PIXFORMAT_BAYER_BGGR: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long b = __USAT16(__UXTB16_RORn(pixels, 8) * blue_gain, 13) << 3;
                        long tmp = __USUB8(0xFF00FF, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, b);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (n % 2) {
                            *ptr = __USAT_ASR(*ptr * blue_gain, 8, 5);
                        }
                    }
                } else {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long r = __USAT16(__UXTB16_RORn(pixels, 0) * red_gain, 13) >> 5;
                        long tmp = __USUB8(0xFF00FF00, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, r);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (!(n % 2)) {
                            *ptr = __USAT_ASR(*ptr * red_gain, 8, 5);
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GBRG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long b = __USAT16(__UXTB16_RORn(pixels, 0) * blue_gain, 13) >> 5;
                        long tmp = __USUB8(0xFF00FF00, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, b);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (!(n % 2)) {
                            *ptr = __USAT_ASR(*ptr * blue_gain, 8, 5);
                        }
                    }
                } else {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long r = __USAT16(__UXTB16_RORn(pixels, 8) * red_gain, 13) << 3;
                        long tmp = __USUB8(0xFF00FF, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, r);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (n % 2) {
                            *ptr = __USAT_ASR(*ptr * red_gain, 8, 5);
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_GRBG: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long r = __USAT16(__UXTB16_RORn(pixels, 0) * red_gain, 13) >> 5;
                        long tmp = __USUB8(0xFF00FF00, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, r);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (!(n % 2)) {
                            *ptr = __USAT_ASR(*ptr * red_gain, 8, 5);
                        }
                    }
                } else {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long b = __USAT16(__UXTB16_RORn(pixels, 8) * blue_gain, 13) << 3;
                        long tmp = __USUB8(0xFF00FF, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, b);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (n % 2) {
                            *ptr = __USAT_ASR(*ptr * blue_gain, 8, 5);
                        }
                    }
                }
            }

            break;
        }
        case PIXFORMAT_BAYER_RGGB: {
            uint8_t *ptr = (uint8_t *) img->data;
            for (int y = 0, yy = img->h; y < yy; y++) {
                if (y % 2) {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long r = __USAT16(__UXTB16_RORn(pixels, 8) * red_gain, 13) << 3;
                        long tmp = __USUB8(0xFF00FF, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, r);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (!(n % 2)) {
                            *ptr = __USAT_ASR(*ptr * red_gain, 8, 5);
                        }
                    }
                } else {
                    long n = img->w;

                    #if defined(ARM_MATH_DSP)
                    uint32_t *ptr32 = (uint32_t *) ptr;

                    for (; n > 3; n -= 4) {
                        uint32_t pixels = *ptr32;
                        long b = __USAT16(__UXTB16_RORn(pixels, 0) * blue_gain, 13) >> 5;
                        long tmp = __USUB8(0xFF00FF00, pixels); (void) tmp;
                        *ptr32++ = __SEL(pixels, b);
                    }

                    ptr = (uint8_t *) ptr32;
                    #endif

                    for (; n > 0; n -= 1, ptr++) {
                        if (!(n % 2)) {
                            *ptr = __USAT_ASR(*ptr * blue_gain, 8, 5);
                        }
                    }
                }
            }

            break;
        }
        default: {
            break;
        }
    }
}

void imlib_ccm(image_t *img, float *ccm, bool offset) {
    float rr = ccm[0], rg = ccm[1], rb = ccm[2], ro = ccm[3];
    float gr = ccm[4], gg = ccm[5], gb = ccm[6], go = ccm[7];
    float br = ccm[8], bg = ccm[9], bb = ccm[10], bo = ccm[11];

    int i_rr = IM_MIN(fast_roundf(rr * 64), 1024);
    int i_rg = IM_MIN(fast_roundf(rg * 32), 512);
    int i_rb = IM_MIN(fast_roundf(rb * 64), 1024);

    int i_gr = IM_MIN(fast_roundf(gr * 64), 1024);
    int i_gg = IM_MIN(fast_roundf(gg * 32), 512);
    int i_gb = IM_MIN(fast_roundf(gb * 64), 1024);

    int i_br = IM_MIN(fast_roundf(br * 64), 1024);
    int i_bg = IM_MIN(fast_roundf(bg * 32), 512);
    int i_bb = IM_MIN(fast_roundf(bb * 64), 1024);

    int i_ro = IM_MIN(fast_roundf(ro * 64), 1024);
    int i_go = IM_MIN(fast_roundf(go * 32), 512);
    int i_bo = IM_MIN(fast_roundf(bo * 64), 1024);

    #if defined(ARM_MATH_DSP)
    long smuad_rr_rb = __PKHBT(i_rb, i_rr, 16);
    long smuad_gr_gb = __PKHBT(i_gb, i_gr, 16);
    long smuad_br_bb = __PKHBT(i_bb, i_br, 16);
    #endif

    switch (img->pixfmt) {
        case PIXFORMAT_RGB565: {
            uint16_t *ptr = (uint16_t *) img->data;
            long n = img->w * img->h; // must be signed for count down loop

            if (offset) {
                for (; n > 0; n -= 1) {
                    int pixel = *ptr;
                    int g = COLOR_RGB565_TO_G6(pixel);
                    #if defined(ARM_MATH_DSP)
                    // This code is only slightly faster than the non-DSP version...
                    int r_b = __PKHBT(pixel & 0x1F, pixel, 5);
                    int new_r = __USAT_ASR(__SMLAD(r_b, smuad_rr_rb, (i_rg * g) + i_ro), 5, 6);
                    int new_g = __USAT_ASR(__SMLAD(r_b, smuad_gr_gb, (i_gg * g) + i_go), 6, 5);
                    int new_b = __USAT_ASR(__SMLAD(r_b, smuad_br_bb, (i_bg * g) + i_bo), 5, 6);
                    #else
                    int r = COLOR_RGB565_TO_R5(pixel);
                    int b = COLOR_RGB565_TO_B5(pixel);
                    int new_r = __USAT_ASR((i_rr * r) + (i_rg * g) + (i_rb * b) + i_ro, 5, 6);
                    int new_g = __USAT_ASR((i_gr * r) + (i_gg * g) + (i_gb * b) + i_go, 6, 5);
                    int new_b = __USAT_ASR((i_br * r) + (i_bg * g) + (i_bb * b) + i_bo, 5, 6);
                    #endif
                    *ptr++ = COLOR_R5_G6_B5_TO_RGB565(new_r, new_g, new_b);
                }
            } else {
                for (; n > 0; n -= 1) {
                    int pixel = *ptr;
                    int g = COLOR_RGB565_TO_G6(pixel);
                    #if defined(ARM_MATH_DSP)
                    // This code is only slightly faster than the non-DSP version...
                    int r_b = __PKHBT(pixel & 0x1F, pixel, 5);
                    int new_r = __USAT_ASR(__SMLAD(r_b, smuad_rr_rb, i_rg * g), 5, 6);
                    int new_g = __USAT_ASR(__SMLAD(r_b, smuad_gr_gb, i_gg * g), 6, 5);
                    int new_b = __USAT_ASR(__SMLAD(r_b, smuad_br_bb, i_bg * g), 5, 6);
                    #else
                    int r = COLOR_RGB565_TO_R5(pixel);
                    int b = COLOR_RGB565_TO_B5(pixel);
                    int new_r = __USAT_ASR((i_rr * r) + (i_rg * g) + (i_rb * b), 5, 6);
                    int new_g = __USAT_ASR((i_gr * r) + (i_gg * g) + (i_gb * b), 6, 5);
                    int new_b = __USAT_ASR((i_br * r) + (i_bg * g) + (i_bb * b), 5, 6);
                    #endif
                    *ptr++ = COLOR_R5_G6_B5_TO_RGB565(new_r, new_g, new_b);
                }
            }

            break;
        }
        default: {
            break;
        }
    }
}

void imlib_gamma(image_t *img, float gamma, float contrast, float brightness) {
    gamma = IM_DIV(1.0f, gamma);
    switch (img->pixfmt) {
        case PIXFORMAT_BINARY: {
            float pScale = COLOR_BINARY_MAX - COLOR_BINARY_MIN;
            float pDiv = 1 / pScale;
            int *p_lut = fb_alloc((COLOR_BINARY_MAX - COLOR_BINARY_MIN + 1) * sizeof(int));

            for (int i = COLOR_BINARY_MIN; i <= COLOR_BINARY_MAX; i++) {
                int p = ((fast_powf(i * pDiv, gamma) * contrast) + brightness) * pScale;
                p_lut[i] = __USAT(p, 1);
            }

            for (int y = 0, yy = img->h; y < yy; y++) {
                uint32_t *data = IMAGE_COMPUTE_BINARY_PIXEL_ROW_PTR(img, y);
                for (int x = 0, xx = img->w; x < xx; x++) {
                    int dataPixel = IMAGE_GET_BINARY_PIXEL_FAST(data, x);
                    int p = p_lut[dataPixel];
                    IMAGE_PUT_BINARY_PIXEL_FAST(data, x, p);
                }
            }

            fb_free();
            break;
        }
        case PIXFORMAT_GRAYSCALE:
        case PIXFORMAT_BAYER_ANY:
        case PIXFORMAT_YUV_ANY: {
            float pScale = COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN;
            float pDiv = 1 / pScale;
            int *p_lut = fb_alloc((COLOR_GRAYSCALE_MAX - COLOR_GRAYSCALE_MIN + 1) * sizeof(int));

            for (int i = COLOR_GRAYSCALE_MIN; i <= COLOR_GRAYSCALE_MAX; i++) {
                int p = ((fast_powf(i * pDiv, gamma) * contrast) + brightness) * pScale;
                p_lut[i] = __USAT(p, 8);
            }

            uint8_t *ptr = (uint8_t *) img->data;
            int n = img->w * img->h;

            if (img->bpp == 2) {
                for (; n > 0; n--, ptr += 2) {
                    *ptr = p_lut[*ptr];
                }
            } else {
                for (; n > 0; n--, ptr += 1) {
                    *ptr = p_lut[*ptr];
                }
            }

            fb_free();
            break;
        }
        case PIXFORMAT_RGB565: {
            float rScale = COLOR_R5_MAX - COLOR_R5_MIN;
            float gScale = COLOR_G6_MAX - COLOR_G6_MIN;
            float bScale = COLOR_B5_MAX - COLOR_B5_MIN;
            float rDiv = 1 / rScale;
            float gDiv = 1 / gScale;
            float bDiv = 1 / bScale;
            int *r_lut = fb_alloc((COLOR_R5_MAX - COLOR_R5_MIN + 1) * sizeof(int));
            int *g_lut = fb_alloc((COLOR_G6_MAX - COLOR_G6_MIN + 1) * sizeof(int));
            int *b_lut = fb_alloc((COLOR_B5_MAX - COLOR_B5_MIN + 1) * sizeof(int));

            for (int i = COLOR_R5_MIN; i <= COLOR_R5_MAX; i++) {
                int r = ((fast_powf(i * rDiv, gamma) * contrast) + brightness) * rScale;
                r_lut[i] = __USAT(r, 5);
            }

            for (int i = COLOR_G6_MIN; i <= COLOR_G6_MAX; i++) {
                int g = ((fast_powf(i * gDiv, gamma) * contrast) + brightness) * gScale;
                g_lut[i] = __USAT(g, 6);
            }

            for (int i = COLOR_B5_MIN; i <= COLOR_B5_MAX; i++) {
                int b = ((fast_powf(i * bDiv, gamma) * contrast) + brightness) * bScale;
                b_lut[i] = __USAT(b, 5);
            }

            uint16_t *ptr = (uint16_t *) img->data;
            int n = img->w * img->h;

            for (; n > 0; n--) {
                int dataPixel = *ptr;
                int r = r_lut[COLOR_RGB565_TO_R5(dataPixel)];
                int g = g_lut[COLOR_RGB565_TO_G6(dataPixel)];
                int b = b_lut[COLOR_RGB565_TO_B5(dataPixel)];
                *ptr++ = COLOR_R5_G6_B5_TO_RGB565(r, g, b);
            }

            fb_free();
            fb_free();
            fb_free();
            break;
        }
        default: {
            break;
        }
    }
}

#endif // IMLIB_ENABLE_ISP_OPS
