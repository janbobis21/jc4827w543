#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TAMC_GT911.h> 

// --- Display Configuration ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* cs */, 
    47 /* sck */, 
    21 /* d0 */, 
    48 /* d1 */, 
    40 /* d2 */, 
    39 /* d3 */
);

//Arduino_GFX *g = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */, 2 /* rotation */, true /* IPS */);
Arduino_GFX *g = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */, 2 /* rotation */, true /* IPS */);
Arduino_Canvas *gfx = new Arduino_Canvas(480, 272, g);

#define GFX_BL 1 // Backlight pin for JC4827W543

// --- Touch Configuration (GT911 I2C) ---
#define I2C_SDA 8
#define I2C_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38

// Instantiate the GT911 controller with your hardware boundaries
TAMC_GT911 touch = TAMC_GT911(I2C_SDA, I2C_SCL, TOUCH_INT, TOUCH_RST, 480, 272);

// Tracking variables
int textX = 240; 
int textY = 136; 
bool screenNeedsUpdate = true;

void setup() {
    Serial.begin(115200);

    // Initialize Backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);

    // Initialize Display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(0x0000); // 0x0000 is Black in RGB565

    // Initialize Touch Panel
    touch.begin();
    //touch.setRotation(ROTATION_NORMAL); 
    touch.setRotation(ROTATION_INVERTED); 

    Serial.println("Setup Finished.");
}

void loop() {
    // Read the touch input data
    touch.read();

    // Check if the screen is being touched
    if (touch.isTouched) {
        // Grab the data from the first finger touch point
        textX = touch.points[0].x; // Accessing point zero index explicitly
        textY = touch.points[0].y;
        
        screenNeedsUpdate = true;
    }

    // Only redraw if a touch event actually registered
    if (screenNeedsUpdate) {
        gfx->fillScreen(0x0000); // Clear screen buffer (Black)
        
        // Target crosshair (0xF800 is Red in RGB565)
        gfx->fillCircle(textX, textY, 4, 0xF800);
        
        // Draw the text offset slightly from your physical fingertip position
        gfx->setTextColor(0xFFFF); // 0xFFFF is White in RGB565
        gfx->setTextSize(2); 
        gfx->setCursor(textX + 12, textY - 12);
        gfx->print("Hello World!");

        // Push canvas data to the display panel
        gfx->flush(); 
        
        screenNeedsUpdate = false;
    }
    
    delay(16); // Poll tracking at ~60Hz frame intervals
}
