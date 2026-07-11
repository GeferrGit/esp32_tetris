#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "game_logic.h"

const uint16_t blockColor565[8] = {
  ST77XX_BLACK,   // 0: empty cell
  ST77XX_RED,     // 1: I piece
  ST77XX_YELLOW,  // 2: O piece
  ST77XX_GREEN,   // 3: T piece
  ST77XX_BLUE,    // 4: S piece
  ST77XX_CYAN,    // 5: Z piece
  ST77XX_MAGENTA, // 6: J piece
  ST77XX_ORANGE   // 7: L piece
};

#define TFT_CS     5
#define TFT_DC     16
#define TFT_RST    17

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

const int blockSize = 16;
const int offsetX = 0;
const int infoPanelX = widthBlocks * blockSize + 1;

int score = 0;
int linesCleared = 0;
int level = 1;
unsigned long lastFall = 0;
int fallDelay = 2000;
bool gameOver = false;
bool paused = false;

// Матрица кнопок
int rows[] = {32, 26, 27};
int cols[] = {14, 25, 33};

// RESET
#define RESET_ROW 26
#define RESET_COL 13

// Спикер
#define SPEAKER_PIN 4

Point pos;
int rot;
Block current;
Block next;

// --- Функции звука ---
void playClick() {
  tone(SPEAKER_PIN, 1000, 50);  // короткий клик
}
void playDrop() {
  tone(SPEAKER_PIN, 200, 150);  // низкий звук падения
}

// --- Проверка кнопки ---
bool isButtonPressed(int rowPin, int colPin) {
  for (int r : rows) digitalWrite(r, HIGH);
  digitalWrite(rowPin, LOW);
  delayMicroseconds(3);
  bool pressed = (digitalRead(colPin) == LOW);
  digitalWrite(rowPin, HIGH);
  return pressed;
}

void drawBlock(int x, int y, int colorId) {
  tft.fillRect(offsetX + x * blockSize, y * blockSize, blockSize - 1, blockSize - 1, blockColor565[colorId]);
}

void drawScreen() {
  for (int x = 0; x < widthBlocks; x++)
    for (int y = 0; y < heightBlocks; y++)
      drawBlock(x, y, screen[x][y]);
}

void drawInfoPanel() {
  tft.drawRect(infoPanelX, 0, 78, 60, ST77XX_WHITE);
  tft.setCursor(infoPanelX + 5, 2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(1);
  tft.print("Next:");
  Point preview[4];
  getBlocks(next, {0, 0}, 0, preview);
  for (int i = 0; i < 4; i++) {
    tft.fillRect(infoPanelX + 10 + preview[i].x * 10, 20 + preview[i].y * 10, 9, 9, blockColor565[next.color]);
  }

  tft.drawRect(infoPanelX, 65, 78, 50, ST77XX_WHITE);
  tft.setCursor(infoPanelX + 5, 70);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("Level:");
  tft.setCursor(infoPanelX + 5, 85);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(level);

  tft.drawRect(infoPanelX, 120, 78, 50, ST77XX_WHITE);
  tft.setCursor(infoPanelX + 5, 125);
  tft.setTextColor(ST77XX_GREEN);
  tft.print("Lines:");
  tft.setCursor(infoPanelX + 5, 140);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(linesCleared);

  tft.drawRect(infoPanelX, 175, 78, 50, ST77XX_WHITE);
  tft.setCursor(infoPanelX + 5, 180);
  tft.setTextColor(ST77XX_YELLOW);
  tft.print("Score:");
  tft.setCursor(infoPanelX + 5, 195);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(score);
}

void placeBlock(Block block, Point pos, int rot, bool value) {
  Point parts[4];
  getBlocks(block, pos, rot, parts);
  for (int i = 0; i < 4; i++)
    screen[parts[i].x][parts[i].y] = value ? block.color : 0;
}

void spawnBlock() {
  current = next;
  next = blocks[random(7)];
  rot = 0;
  pos = {5, 1};
  Point test[4];
  if (!getBlocks(current, pos, rot, test)) gameOver = true;
  else placeBlock(current, pos, rot, true);
}

void clearLines() {
  for (int y = heightBlocks - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < widthBlocks; x++)
      if (!screen[x][y]) full = false;
    if (full) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < widthBlocks; x++)
          screen[x][yy] = screen[x][yy - 1];
      for (int x = 0; x < widthBlocks; x++) screen[x][0] = 0;
      linesCleared++;
      score += 100;
      if (linesCleared % 100 == 0 && level < 10) {
        level++;
        fallDelay = max(200, 2000 - (level - 1) * 200);
      }
      y++;
    }
  }
}

bool canMove(int dx, int dy) {
  Point test[4];
  return getBlocks(current, {pos.x + dx, pos.y + dy}, rot, test);
}

void dropInstant() {
  while (canMove(0, 1)) pos.y++;
}

void resetGame() {
  for (int x = 0; x < widthBlocks; x++)
    for (int y = 0; y < heightBlocks; y++)
      screen[x][y] = 0;

  score = 0;
  linesCleared = 0;
  level = 1;
  fallDelay = 2000;
  gameOver = false;
  paused = false;

  next = blocks[random(7)];
  spawnBlock();
  drawScreen();
  drawInfoPanel();
}

void readButtons() {
  // RESET
  for (int r : rows) digitalWrite(r, HIGH);
  digitalWrite(RESET_ROW, LOW);
  delayMicroseconds(3);
  if (digitalRead(RESET_COL) == LOW) {
    resetGame();
    delay(500);
    return;
  }
  digitalWrite(RESET_ROW, HIGH);

  if (isButtonPressed(32, 33)) { paused = !paused; playClick(); delay(300); return; }
  if (paused) return;
  placeBlock(current, pos, rot, false);

  if (isButtonPressed(27, 14) && canMove(-1, 0)) { pos.x--; playClick(); delay(100); }
  if (isButtonPressed(26, 14) && canMove(1, 0))  { pos.x++; playClick(); delay(100); }
  if (isButtonPressed(32, 14) && canMove(0, 1))  { pos.y++; playClick(); delay(100); }
  if (isButtonPressed(26, 25))                   { rot = (rot + 1) % current.rotations; playClick(); delay(150); }
  if (isButtonPressed(27, 33))                   { dropInstant(); playClick(); delay(150); }

  placeBlock(current, pos, rot, true);
  drawScreen();
}

void setup() {
  Serial.begin(115200);
  tft.init(240, 320);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  for (int r : rows) pinMode(r, OUTPUT);
  for (int c : cols) pinMode(c, INPUT_PULLUP);
  pinMode(RESET_COL, INPUT_PULLUP);

  pinMode(SPEAKER_PIN, OUTPUT);

  next = blocks[random(7)];
  spawnBlock();
  drawScreen();
  drawInfoPanel();
}

void loop() {
  if (paused) {
    tft.setCursor(50, 140);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.println("PAUSE");
  }
  if (gameOver) {
    tft.setCursor(60, 140);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.println("GAME OVER");
    while (1) {
      if (isButtonPressed(RESET_ROW, RESET_COL)) { resetGame(); break; }
    }
  }
  readButtons();
  if (!paused && millis() - lastFall > fallDelay) {
    lastFall = millis();
    placeBlock(current, pos, rot, false);
    if (canMove(0, 1)) pos.y++;
    else {
      placeBlock(current, pos, rot, true);
      clearLines();
      drawInfoPanel();
      spawnBlock();
      playDrop();
    }
    placeBlock(current, pos, rot, true);
    drawScreen();
  }
}
