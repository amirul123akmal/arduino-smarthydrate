#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

class DisplayModule {
public:
    DisplayModule(int width, int height, int resetPin = -1);
    
    // Core methods
    bool begin(uint8_t address = 0x3C);
    void clear();
    void display();
    
    // Text methods
    void setCursor(int16_t x, int16_t y);
    void setTextSize(uint8_t s);
    void setTextColor(uint16_t c);
    
    // Expose print methods via template or direct forwarding if needed
    // Simple wrappers for common use cases
    void print(const String &s);
    void print(const char* s);
    void print(int n);
    void println(const String &s);
    void println(const char* s);
    void println(int n);
    
    // Graphics
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    // Helpers
    int16_t width() const;
    int16_t height() const;

private:
    int _width;
    int _height;
    Adafruit_SSD1306 _display;
};

#endif // DISPLAY_MODULE_H
