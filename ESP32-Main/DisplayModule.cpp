#include "DisplayModule.h"

DisplayModule::DisplayModule(int width, int height, int resetPin) 
    : _width(width), _height(height), _display(width, height, &Wire, resetPin) {
}

bool DisplayModule::begin(uint8_t address) {
    if(!_display.begin(SSD1306_SWITCHCAPVCC, address)) {
        return false;
    }
    return true;
}

void DisplayModule::clear() {
    _display.clearDisplay();
}

void DisplayModule::display() {
    _display.display();
}

void DisplayModule::setCursor(int16_t x, int16_t y) {
    _display.setCursor(x, y);
}

void DisplayModule::setTextSize(uint8_t s) {
    _display.setTextSize(s);
}

void DisplayModule::setTextColor(uint16_t c) {
    _display.setTextColor(c);
}

void DisplayModule::print(const String &s) {
    _display.print(s);
}

void DisplayModule::print(const char* s) {
    _display.print(s);
}

void DisplayModule::print(int n) {
    _display.print(n);
}

void DisplayModule::println(const String &s) {
    _display.println(s);
}

void DisplayModule::println(const char* s) {
    _display.println(s);
}

void DisplayModule::println(int n) {
    _display.println(n);
}

void DisplayModule::drawPixel(int16_t x, int16_t y, uint16_t color) {
    _display.drawPixel(x, y, color);
}

int16_t DisplayModule::width() const {
    return _width;
}

int16_t DisplayModule::height() const {
    return _height;
}
