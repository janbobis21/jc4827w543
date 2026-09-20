#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TAMC_GT911.h>

// --- Display Configuration ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    45 /* cs */, 47 /* sck */, 21 /* d0 */, 48 /* d1 */, 40 /* d2 */, 39 /* d3 */
);

Arduino_GFX *g = new Arduino_NV3041A(bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */, true /* IPS */);
Arduino_Canvas *gfx = new Arduino_Canvas(480 /* width */, 272 /* height */, g);

#define GFX_BL 1 

// --- Touch Configuration ---
#define I2C_SDA 8
#define I2C_SCL 4
#define TOUCH_INT 3
#define TOUCH_RST 38

TAMC_GT911 touch = TAMC_GT911(I2C_SDA, I2C_SCL, TOUCH_INT, TOUCH_RST, 480, 272);

// --- Theme Management Struct ---
struct ColorTheme {
  uint16_t bg_editor;
  uint16_t bg_kb;
  uint16_t key_reg;
  uint16_t key_spec;
  uint16_t key_active;
  uint16_t border;
  uint16_t text;
  uint16_t text_muted;
};

ColorTheme darkTheme  = {0x0000, 0x10A2, 0x2104, 0x3186, 0x0460, 0x2945, 0xFFFF, 0x7BEF};
ColorTheme lightTheme = {0xFFFF, 0xE71C, 0xFFFF, 0xD6BA, 0x34BF, 0xB5B6, 0x0000, 0x7BEF};
ColorTheme* currentTheme = &darkTheme;
bool isDarkThemeActive = true;

// --- Application State Variables ---
String typedText = "";
bool screenNeedsUpdate = true;
bool wasTouchedLastFrame = false;
bool isShiftActive = false;
bool isCapsActive = false;

// Active Touch Feedback Coordinates Tracker
int pressedRow = -1;
int pressedCol = -1;
bool isScrollActive = false;
int scrollStartTouchY = 0;
int scrollStartOffset = 0;

// Layout Metrics
const int KEY_ROWS = 5;         
const int KEY_COLS = 14;       
const int KEY_HEIGHT = 28;       
const int KEYBOARD_TOP_Y = 132;  

// Scroll Metrics
int currentScrollOffset = 0; 
int totalLinesCached = 0;
const int maxVisibleLines = 6;

// --- Keyboard Layout Tables ---
const char keysNormal[KEY_ROWS][KEY_COLS] = {
  {'`','1','2','3','4','5','6','7','8','9','0','-','='},
  {' ','q','w','e','r','t','y','u','i','o','p','[',']','\\'}, 
  {' ','a','s','d','f','g','h','j','k','l',';','\'','\n'},     
  {' ','z','x','c','v','b','n','m',',','.','/',' '}            
};

const char keysShift[KEY_ROWS][KEY_COLS] = {
  {'~','!','@','#','$','%','^','&','*','(',')','_','+'},
  {' ','Q','W','E','R','T','Y','U','I','O','P','{','}','|'},
  {' ','A','S','D','F','G','H','J','K','L',':','"','\n'},
  {' ','Z','X','C','V','B','N','M','<','>','?',' '}
};

// Scrollbar dimensions
const int barX = 2, barY = 5, barW = 8, barH = KEYBOARD_TOP_Y - 12;

// --- Helper Functions ---
char getProcessedChar(int row, int col) {
  char chr = isShiftActive ? keysShift[row][col] : keysNormal[row][col];
  if (isCapsActive && chr >= 'a' && chr <= 'z') chr -= 32;
  else if (isCapsActive && isShiftActive && chr >= 'A' && chr <= 'Z') chr += 32;
  return chr;
}

void drawCustomKey(int x, int y, int w, int h, const char* label, uint16_t bgColor, bool isPressedState = false) {
  uint16_t bg = isPressedState ? 0xFEE0 : bgColor; 
  uint16_t txtColor = isPressedState ? 0x0000 : currentTheme->text;

  gfx->fillRoundRect(x + 1, y + 1, w - 2, h - 2, 3, bg);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, currentTheme->border);
  
  gfx->setTextColor(txtColor);
  int labelLen = strlen(label);
  
  if (labelLen > 2) {
    gfx->setTextSize(1);
    gfx->setCursor(x + (w - (labelLen * 6)) / 2, y + (h - 8) / 2);
  } else {
    gfx->setTextSize(2);
    gfx->setCursor(x + (w - (labelLen * 12)) / 2, y + (h - 16) / 2 - 1);
  }
  gfx->print(label);
}

void renderEditorArea() {
  gfx->fillRect(0, 0, 480, KEYBOARD_TOP_Y, currentTheme->bg_editor);
  gfx->drawFastHLine(0, KEYBOARD_TOP_Y - 2, 480, currentTheme->border);

  // 1. Text Rendering Engine
  int lineCount = 0;
  String lines[60];
  String tempText = typedText;
  
  while (tempText.length() > 0 && lineCount < 60) {
    int idx = tempText.indexOf('\n');
    if (idx == -1) { lines[lineCount++] = tempText; break; }
    else { lines[lineCount++] = tempText.substring(0, idx); tempText = tempText.substring(idx + 1); }
  }
  if (typedText.length() == 0) { lines[0] = ""; lineCount = 1; }
  else if (typedText.endsWith("\n") && lineCount < 60) { lines[lineCount++] = ""; }
  
  totalLinesCached = lineCount;

  if (currentScrollOffset < 0) currentScrollOffset = 0;
  if (currentScrollOffset > totalLinesCached - maxVisibleLines) currentScrollOffset = totalLinesCached - maxVisibleLines;
  if (totalLinesCached <= maxVisibleLines) currentScrollOffset = 0;

  gfx->setTextSize(2); 
  int currentY = 12;
  int endLine = min(currentScrollOffset + maxVisibleLines, totalLinesCached);

  for (int i = currentScrollOffset; i < endLine; i++) {
    gfx->setCursor(20, currentY);
    if (typedText.length() == 0) {
      gfx->setTextColor(currentTheme->text_muted);
      gfx->print("Type something...");
    } else {
      gfx->setTextColor(currentTheme->text);
      gfx->print(lines[i]);
    }
    currentY += 18;
  }

  // 2. Scrollbar Handle 
  gfx->fillRoundRect(barX, barY, barW, barH, 2, currentTheme->key_reg);
  float viewRatio = (float)maxVisibleLines / (float)totalLinesCached;
  if (viewRatio > 1.0) viewRatio = 1.0;
  int handleH = barH * viewRatio;
  if (handleH < 14) handleH = 14;

  int maxScrollOffset = totalLinesCached - maxVisibleLines;
  int handleY = barY;
  if (maxScrollOffset > 0) {
    float scrollPercent = (float)currentScrollOffset / (float)maxScrollOffset;
    handleY += (barH - handleH) * scrollPercent;
  }
  gfx->fillRoundRect(barX, handleY, barW, handleH, 3, currentTheme->key_active);

  // 3. Single Theme Toggle Button Placed on Top Right 
  drawCustomKey(390, 4, 85, 22, isDarkThemeActive ? "LIGHT" : "DARK", currentTheme->key_spec);
}

void updateInterface() {
  renderEditorArea();

  gfx->fillRect(0, KEYBOARD_TOP_Y, 480, 272 - KEYBOARD_TOP_Y, currentTheme->bg_kb);
  int y = KEYBOARD_TOP_Y;

  // ROW 0: Numbers
  for (int i = 0; i < 13; i++) {
    char chr = isShiftActive ? keysShift[0][i] : keysNormal[0][i];
    char str[] = {chr, '\0'};
    drawCustomKey(i * 34, y, 34, KEY_HEIGHT, str, currentTheme->key_reg, (pressedRow == 0 && pressedCol == i));
  }
  drawCustomKey(13 * 34, y, 38, KEY_HEIGHT, "BKSP", currentTheme->key_spec, (pressedRow == 0 && pressedCol == 13));
  y += KEY_HEIGHT;

  // ROW 1: TAB + QWERTY
  drawCustomKey(0, y, 51, KEY_HEIGHT, "TAB", currentTheme->key_spec, (pressedRow == 1 && pressedCol == 0));
  for (int i = 1; i <= 12; i++) {
    char chr = getProcessedChar(1, i);
    char str[] = {chr, '\0'};
    drawCustomKey(51 + ((i - 1) * 34), y, 34, KEY_HEIGHT, str, currentTheme->key_reg, (pressedRow == 1 && pressedCol == i));
  }
  char slashChr = getProcessedChar(1, 13);
  char slashStr[] = {slashChr, '\0'};
  drawCustomKey(51 + (12 * 34), y, 21, KEY_HEIGHT, slashStr, currentTheme->key_reg, (pressedRow == 1 && pressedCol == 13));
  y += KEY_HEIGHT;

  // ROW 2: CAPS + ASDF
  uint16_t capsBg = isCapsActive ? currentTheme->key_active : currentTheme->key_spec;
  drawCustomKey(0, y, 51, KEY_HEIGHT, "CAPS", capsBg, (pressedRow == 2 && pressedCol == 0));
  for (int i = 1; i <= 11; i++) {
    char chr = getProcessedChar(2, i);
    char str[] = {chr, '\0'};
    drawCustomKey(51 + ((i - 1) * 34), y, 34, KEY_HEIGHT, str, currentTheme->key_reg, (pressedRow == 2 && pressedCol == i));
  }
  drawCustomKey(51 + (11 * 34), y, 55, KEY_HEIGHT, "ENTER", currentTheme->key_spec, (pressedRow == 2 && pressedCol == 12));
  y += KEY_HEIGHT;

  // ROW 3: SHIFT + ZXCV
  uint16_t shiftBg = isShiftActive ? currentTheme->key_active : currentTheme->key_spec;
  drawCustomKey(0, y, 51, KEY_HEIGHT, "SHFT", shiftBg, (pressedRow == 3 && pressedCol == 0));
  for (int i = 1; i <= 10; i++) {
    char chr = getProcessedChar(3, i);
    char str[] = {chr, '\0'};
    drawCustomKey(51 + ((i - 1) * 34), y, 34, KEY_HEIGHT, str, currentTheme->key_reg, (pressedRow == 3 && pressedCol == i));
  }
  drawCustomKey(51 + (10 * 34), y, 89, KEY_HEIGHT, "SHFT", shiftBg, (pressedRow == 3 && pressedCol == 11));
  y += KEY_HEIGHT;

  // ROW 4: Utilities
  drawCustomKey(0, y, 68, KEY_HEIGHT, "CLR", currentTheme->key_spec, (pressedRow == 4 && pressedCol == 0));
  drawCustomKey(68, y, 344, KEY_HEIGHT, "SPACE", currentTheme->key_reg, (pressedRow == 4 && pressedCol == 1));
  drawCustomKey(412, y, 68, KEY_HEIGHT, ".com", currentTheme->key_spec, (pressedRow == 4 && pressedCol == 2));

  gfx->flush();
}

void setup() {
  Serial.begin(115200);
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  if (!gfx->begin()) Serial.println("GFX Init Failed!");
  g->invertDisplay(true); 
  touch.begin();
  touch.setRotation(ROTATION_NORMAL);
  updateInterface();
}

void loop() {
  touch.read();

  if (touch.isTouched) {
    // FIXED: Added multi-touch bracket arrays explicitly here
    int tx = touch.points[0].x;
    int ty = touch.points[0].y;

    // --- CASE A: Handle Scrollbar Touch Dragging ---
    if ((tx <= 14 && ty < KEYBOARD_TOP_Y) || isScrollActive) {
      if (!isScrollActive) {
        isScrollActive = true;
        scrollStartTouchY = ty;
        scrollStartOffset = currentScrollOffset;
      } else {
        int deltaY = ty - scrollStartTouchY;
        int maxScrollOffset = totalLinesCached - maxVisibleLines;
        if (maxScrollOffset > 0) {
          int lineDelta = (deltaY * maxScrollOffset) / (barH - 30); 
          currentScrollOffset = scrollStartOffset + lineDelta;
          screenNeedsUpdate = true;
        }
      }
    }
    // --- CASE B: Intercept Single Top-Right Toggle Button ---
    else if (ty < KEYBOARD_TOP_Y) {
      if (!wasTouchedLastFrame) {
        if (tx >= 390 && tx <= 475 && ty >= 4 && ty <= 26) {
          isDarkThemeActive = !isDarkThemeActive;
          if (isDarkThemeActive) {
            currentTheme = &darkTheme;
          } else {
            currentTheme = &lightTheme;
          }
          screenNeedsUpdate = true;
        }
      }
    }
    // --- CASE C: Intercept Keyboard Panel Matrix ---
    else if (ty >= KEYBOARD_TOP_Y) {
      int localY = ty - KEYBOARD_TOP_Y;
      int row = localY / KEY_HEIGHT;
      if (row >= KEY_ROWS) row = KEY_ROWS - 1;

      int col = -1;
      if (row == 0) col = (tx >= 13 * 34) ? 13 : tx / 34;
      else if (row == 1) col = (tx < 51) ? 0 : (tx >= 51 + (12 * 34)) ? 13 : 1 + ((tx - 51) / 34);
      else if (row == 2) col = (tx < 51) ? 0 : (tx >= 51 + (11 * 34)) ? 12 : 1 + ((tx - 51) / 34);
      else if (row == 3) col = (tx < 51) ? 0 : (tx >= 51 + (10 * 34)) ? 11 : 1 + ((tx - 51) / 34);
      else if (row == 4) col = (tx < 68) ? 0 : (tx < 412) ? 1 : 2;
      
      if (pressedRow != row || pressedCol != col) {
        pressedRow = row; 
        pressedCol = col;
        screenNeedsUpdate = true;
      } 
      if (!wasTouchedLastFrame) {
        if (row == 0) {
          if (col == 13) { 
            if (typedText.length() > 0) typedText.remove(typedText.length() - 1); 
          } else typedText += isShiftActive ? keysShift[row][col] : keysNormal[row][col];
        } else if (row == 1) {
          if (col == 0) typedText += "    ";
          else if (col == 13) typedText += getProcessedChar(1, 13);
          else typedText += getProcessedChar(1, col);
        } else if (row == 2) {
          if (col == 0) isCapsActive = !isCapsActive;
          else if (col == 12) { 
            typedText += "\n"; 
            currentScrollOffset = totalLinesCached; 
          }
          else typedText += getProcessedChar(2, col);
        } else if (row == 3) {
          if (col == 0 || col == 11) isShiftActive = !isShiftActive;
          else typedText += getProcessedChar(3, col);
        } else if (row == 4) {
          if (col == 0) typedText = "";
          else if (col == 1) typedText += " ";
          else if (col == 2) typedText += ".com";
        } 
        
        if (isShiftActive && row != 3 && !(row == 0 && col == 13) && !(row == 2 && col == 0)) {
          isShiftActive = false;
        }
        screenNeedsUpdate = true;
      }
    }
    wasTouchedLastFrame = true;
  } else {
    if (wasTouchedLastFrame || isScrollActive) {
      pressedRow = -1; 
      pressedCol = -1;
      isScrollActive = false;
      screenNeedsUpdate = true;
    }
    wasTouchedLastFrame = false;
  }
  if (screenNeedsUpdate) {
    updateInterface();
    screenNeedsUpdate = false;
  }
  delay(10);
}