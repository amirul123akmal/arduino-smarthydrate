#include "JpegDecoderModule.h"
#include "Config.h" // For screen dimensions if needed, or get from display

DisplayModule* JpegDecoderModule::_activeDisplay = nullptr;

void JpegDecoderModule::setup() {
    TJpgDec.setJpgScale(1); 
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);
}

bool JpegDecoderModule::tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (!_activeDisplay) return 0;
    
    // Hardcoded centering logic from original code or dynamic?
    // Original code:
    // Center the 80x60 image on the 128x64 screen
    // 128 - 80 = 48 -> x_offset = 24
    // 64 - 60 = 4 -> y_offset = 2
    int x_offset = 24;
    int y_offset = 2;

    int screen_w = _activeDisplay->width();
    int screen_h = _activeDisplay->height();

    // Stop if out of bounds
    if (y >= screen_h) return 0;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int screen_x = x + i + x_offset;
            int screen_y = y + j + y_offset;

            if (screen_x >= 0 && screen_x < screen_w && screen_y >= 0 && screen_y < screen_h) {
                // Convert RGB565 to Grayscale/Monochrome
                uint16_t color = bitmap[j * w + i];
                // Extract RGB
                uint8_t r = (color >> 11) & 0x1F;
                uint8_t g = (color >> 5) & 0x3F;
                uint8_t b = color & 0x1F;
                
                // Normalize to 8-bit for brightness calc (approximate)
                uint8_t brightness = (r * 8 * 0.299) + (g * 4 * 0.587) + (b * 8 * 0.114);

                if (brightness > 127) {
                    _activeDisplay->drawPixel(screen_x, screen_y, SSD1306_WHITE);
                } else {
                    _activeDisplay->drawPixel(screen_x, screen_y, SSD1306_BLACK);
                }
            }
        }
    }
    return 1;
}

void JpegDecoderModule::decodeAndRender(String payload, DisplayModule* display) {
    _activeDisplay = display;
    
    // 1. Extract Base64 String (Logic from original code)
    int keyIndex = payload.indexOf("\"image\"");
    if (keyIndex == -1) keyIndex = payload.indexOf("'image'");
    
    if (keyIndex == -1) {
        Serial.println("No 'image' key found");
        if (display) {
            display->println("No Image Data");
            display->display();
        }
        return;
    }

    int colonIndex = payload.indexOf(":", keyIndex);
    if (colonIndex == -1) return;

    int startQuote = payload.indexOf("\"", colonIndex);
    if (startQuote == -1) startQuote = payload.indexOf("'", colonIndex);
    if (startQuote == -1) return;

    int endQuote = payload.indexOf("\"", startQuote + 1);
    if (endQuote == -1) endQuote = payload.indexOf("'", startQuote + 1);
    if (endQuote == -1) return; // Malformed JSON

    String base64Str = payload.substring(startQuote + 1, endQuote);
    Serial.println("Base64 Length: " + String(base64Str.length()));

    if (display) {
        display->clear();
        display->setCursor(0,0);
        display->println("Decoding...");
        display->display();
    }

    // 2. Decode Base64 to Binary
    size_t outputLength = 0;
    unsigned char* jpgBuffer = (unsigned char*) malloc(base64Str.length()); 
    if (!jpgBuffer) {
        Serial.println("Malloc failed");
        if (display) {
            display->println("Mem Error");
            display->display();
        }
        return;
    }
    
    int ret = mbedtls_base64_decode(jpgBuffer, base64Str.length(), &outputLength, (const unsigned char*)base64Str.c_str(), base64Str.length());
    
    if (ret != 0) {
        Serial.println("Base64 decode failed");
        if (display) {
            display->println("Decode Fail");
            display->display();
        }
        free(jpgBuffer);
        return;
    }
    
    Serial.println("JPEG Size: " + String(outputLength));
    if (display) {
        display->clear();
        display->setCursor(0,0);
        display->println("Drawing...");
        display->display();
        
        display->clear(); // Clear before drawing
    }

    // 3. Render JPEG
    TJpgDec.setJpgScale(8); // Set scale to 1/8. 640/8 = 80, 480/8 = 60. Fits in 128x64.
    
    // Draw the image
    TJpgDec.drawJpg(0, 0, jpgBuffer, outputLength);
    
    if (display) {
        display->display();
    }
    Serial.println("Image Displayed");
    
    free(jpgBuffer);
    _activeDisplay = nullptr; // Clean up
}
