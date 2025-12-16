#ifndef JPEG_DECODER_MODULE_H
#define JPEG_DECODER_MODULE_H

#include <Arduino.h>
#include "DisplayModule.h"
#include <TJpg_Decoder.h>
#include "mbedtls/base64.h"

class JpegDecoderModule {
public:
    static void setup();
    static void decodeAndRender(String payload, DisplayModule* display);

private:
    static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    static DisplayModule* _activeDisplay;
};

#endif // JPEG_DECODER_MODULE_H
