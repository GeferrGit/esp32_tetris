#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS     5
#define TFT_DC     16
#define TFT_RST    17

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

struct Point { int x, y; };
struct Block {
  Point shape[4][4];
  int rotations;
  int color;
};

const int blockSize = 16;
const int widthBlocks = 10;
const int heightBlocks = 18;
const int offsetX = 0;
const int infoPanelX = widthBlocks * blockSize + 1; // 160
int screen[widthBlocks][heightBlocks] = {0};

int score = 0;
int linesCleared = 0;
int level = 1;
unsigned long lastFall = 0;
int fallDelay = 2000;
bool gameOver = false;
bool paused = false;

#define BTN_LEFT    32
#define BTN_RIGHT   33
#define BTN_DOWN    25
#define BTN_ROT_L   26
#define BTN_ROT_R   27
#define BTN_DROP    14
#define BTN_PAUSE   12

Block blocks[7] = {
  {{{{0,-1},{0,0},{0,1},{0,2}},{{-1,0},{0,0},{1,0},{2,0}}}, 2, ST77XX_RED},
  {{{{0,0},{1,0},{0,1},{1,1}}}, 1, ST77XX_YELLOW},
  {{{{-1,0},{0,0},{1,0},{0,-1}},{{0,-1},{0,0},{0,1},{1,0}},{{-1,0},{0,0},{1,0},{0,1}},{{0,-1},{0,0},{0,1},{-1,0}}}, 4, ST77XX_GREEN},
  {{{{-1,0},{0,0},{0,-1},{1,-1}},{{0,-1},{0,0},{1,0},{1,1}}}, 2, ST77XX_BLUE},
  {{{{-1,-1},{0,-1},{0,0},{1,0}},{{1,-1},{1,0},{0,0},{0,1}}}, 2, ST77XX_CYAN},
  {{{{-1,0},{0,0},{1,0},{1,-1}},{{0,-1},{0,0},{0,1},{1,1}},{{-1,0},{0,0},{1,0},{-1,1}},{{0,-1},{0,0},{0,1},{-1,-1}}}, 4, ST77XX_MAGENTA},
  {{{{-1,0},{0,0},{1,0},{-1,-1}},{{0,-1},{0,0},{0,1},{1,-1}},{{-1,0},{0,0},{1,0},{1,1}},{{0,-1},{0,0},{0,1},{-1,1}}}, 4, ST77XX_ORANGE}
};

Point pos;
int rot;
Block current;
Block next;

bool getBlocks(Block block, Point pos, int rot, Point* out) {
  bool valid = true;
  for (int i = 0; i < 4; i++) {
    Point p = { pos.x + block.shape[rot][i].x, pos.y + block.shape[rot][i].y };
    if (p.x < 0 || p.x >= widthBlocks || p.y < 0 || p.y >= heightBlocks || screen[p.x][p.y])
      valid = false;
    out[i] = p;
  }
  return valid;
}

void drawBlock(int x, int y, uint16_t color) {
  tft.fillRect(offsetX + x * blockSize, y * blockSize, blockSize - 1, blockSize - 1, color);
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
    tft.fillRect(infoPanelX + 10 + preview[i].x * 10, 20 + preview[i].y * 10, 9, 9, next.color);
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

  if (paused) {
    tft.setCursor(50, 140);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.println("PAUSE");
  }
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

void readButtons() {
  if (digitalRead(BTN_PAUSE) == LOW) { paused = !paused; delay(300); return; }
  if (paused) return;
  placeBlock(current, pos, rot, false);
  if (digitalRead(BTN_LEFT) == LOW && canMove(-1, 0)) { pos.x--; delay(100); }
  if (digitalRead(BTN_RIGHT) == LOW && canMove(1, 0))  { pos.x++; delay(100); }
  if (digitalRead(BTN_DOWN) == LOW && canMove(0, 1))   { pos.y++; delay(100); }
  if (digitalRead(BTN_ROT_L) == LOW)                   { rot = (rot + current.rotations - 1) % current.rotations; delay(150); }
  if (digitalRead(BTN_ROT_R) == LOW)                   { rot = (rot + 1) % current.rotations; delay(150); }
  if (digitalRead(BTN_DROP) == LOW)                    { dropInstant(); delay(150); }
  placeBlock(current, pos, rot, true);
  drawScreen();
}

void setup() {
  Serial.begin(115200);
  tft.init(240, 320);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ROT_L, INPUT_PULLUP);
  pinMode(BTN_ROT_R, INPUT_PULLUP);
  pinMode(BTN_DROP, INPUT_PULLUP);
  pinMode(BTN_PAUSE, INPUT_PULLUP);

  next = blocks[random(7)];
  spawnBlock();
  drawScreen();
  drawInfoPanel();
}

void loop() {
  if (gameOver) {
    tft.setCursor(60, 140);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.println("GAME OVER");
    while (1);
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
    }
    placeBlock(current, pos, rot, true);
    drawScreen();
  }
}
