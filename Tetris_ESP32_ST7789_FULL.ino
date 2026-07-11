#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "game_logic.h"

/*
 * Ручний чек-лист після прошивки плати:
 * 1. Left/Right/Down/Rotate/HardDrop/Pause/Reset - кожна кнопка виконує свою дію.
 * 2. Обертання біля стін/дна не виходить за межи поля і не "зависає".
 * 3. Ghost piece показує правильну позицію приземлення.
 * 4. За 7-14 фігур поспіль немає підозріло довгих серій однакової фігури.
 * 5. Рахунок: 1 лінія = +100, 2 = +300, 3 = +500, 4 = +800.
 * 6. Рівень підвищується кожні 10 очищених ліній; швидкість падіння зростає.
 * 7. Стартовий екран показується при подачі живлення; гра стартує після першої кнопки.
 * 8. Game Over показує фінальний рахунок і не зависає; Reset одразу починає нову гру.
 * 9. Усі звукові події чутні і відрізняються одна від одної.
 */

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
enum GameState { STATE_START, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER };
GameState gameState = STATE_START;

// Матрица кнопок
int rows[] = {32, 26, 27};
int cols[] = {14, 25, 33};

// Non-blocking button debounce
enum Action { ACT_PAUSE, ACT_LEFT, ACT_RIGHT, ACT_DOWN, ACT_ROTATE, ACT_HARDDROP, ACT_RESET, ACT_COUNT };
unsigned long lastActionTime[ACT_COUNT] = {0};
const unsigned long actionCooldown[ACT_COUNT] = {300, 100, 100, 100, 150, 150, 500};

bool actionReady(Action a) {
  unsigned long now = millis();
  if (now - lastActionTime[a] >= actionCooldown[a]) {
    lastActionTime[a] = now;
    return true;
  }
  return false;
}

// RESET
#define RESET_ROW 26
#define RESET_COL 13

// Спикер
#define SPEAKER_PIN 4

Point pos;
int rot;
Block current;
Block next;

Bag pieceBag = {{0, 0, 0, 0, 0, 0, 0}, 7}; // index=7 forces a refill on first use

int arduinoRandInt(int n) { return random(n); }

// --- Функции звука ---
void playClick() {
  tone(SPEAKER_PIN, 1000, 50);  // короткий клик
}
void playRotate() {
  tone(SPEAKER_PIN, 1400, 50);
}
void playDrop() {
  tone(SPEAKER_PIN, 200, 150);  // низкий звук падения
}

void playGameOver() {
  tone(SPEAKER_PIN, 500, 100);
  delay(120);
  tone(SPEAKER_PIN, 350, 150);
}

void playLineClear() {
  tone(SPEAKER_PIN, 600, 60);
  delay(70);
  tone(SPEAKER_PIN, 900, 80);
}

void playTetris() {
  int notes[4] = {600, 800, 1000, 1300};
  for (int i = 0; i < 4; i++) {
    tone(SPEAKER_PIN, notes[i], 70);
    delay(80);
  }
}

void playLevelUp() {
  tone(SPEAKER_PIN, 1000, 80);
  delay(90);
  tone(SPEAKER_PIN, 1500, 120);
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

void drawGhost() {
  placeBlock(current, pos, rot, false);
  Point ghost = ghostPosition(current, pos, rot);
  placeBlock(current, pos, rot, true);
  if (ghost.y == pos.y) return; // already resting, no separate ghost needed
  Point cells[4];
  getBlocks(current, ghost, rot, cells);
  for (int i = 0; i < 4; i++)
    tft.drawRect(offsetX + cells[i].x * blockSize, cells[i].y * blockSize, blockSize - 1, blockSize - 1, ST77XX_WHITE);
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

void drawStartScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(30, 100);
  tft.println("TETRIS");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(15, 150);
  tft.println("Press any button to start");
}

void drawPauseOverlay() {
  tft.setCursor(50, 140);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.println("PAUSE");
}

void drawGameOverScreen() {
  tft.setCursor(40, 120);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(2);
  tft.println("GAME OVER");
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(40, 150);
  tft.print("Score: ");
  tft.println(score);
}

void placeBlock(Block block, Point pos, int rot, bool value) {
  Point parts[4];
  getBlocks(block, pos, rot, parts);
  for (int i = 0; i < 4; i++)
    screen[parts[i].x][parts[i].y] = value ? block.color : 0;
}

void spawnBlock() {
  current = next;
  next = blocks[nextFromBag(&pieceBag, arduinoRandInt)];
  rot = 0;
  pos = {5, 1};
  Point test[4];
  if (!getBlocks(current, pos, rot, test)) gameState = STATE_GAMEOVER;
  else placeBlock(current, pos, rot, true);
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
  lastFall = millis();

  pieceBag.index = 7; // start each new game with a fresh, fair sequence
  next = blocks[nextFromBag(&pieceBag, arduinoRandInt)];
  spawnBlock();
  drawScreen();
  drawInfoPanel();
  gameState = STATE_PLAYING;
}

bool anyButtonPressed() {
  for (int r : rows)
    for (int c : cols)
      if (isButtonPressed(r, c)) return true;
  return isButtonPressed(RESET_ROW, RESET_COL);
}

void checkPauseButton() {
  if (isButtonPressed(32, 33) && actionReady(ACT_PAUSE)) {
    gameState = (gameState == STATE_PAUSED) ? STATE_PLAYING : STATE_PAUSED;
    playClick();
    if (gameState == STATE_PAUSED) drawPauseOverlay();
  }
}

void readButtons() {
  // RESET
  for (int r : rows) digitalWrite(r, HIGH);
  digitalWrite(RESET_ROW, LOW);
  delayMicroseconds(3);
  if (digitalRead(RESET_COL) == LOW && actionReady(ACT_RESET)) {
    resetGame();
    return;
  }
  digitalWrite(RESET_ROW, HIGH);

  placeBlock(current, pos, rot, false);

  if (isButtonPressed(27, 14) && canMove(-1, 0) && actionReady(ACT_LEFT))  { pos.x--; playClick(); }
  if (isButtonPressed(26, 14) && canMove(1, 0)  && actionReady(ACT_RIGHT)) { pos.x++; playClick(); }
  if (isButtonPressed(32, 14) && canMove(0, 1)  && actionReady(ACT_DOWN))  { pos.y++; playClick(); }
  if (isButtonPressed(26, 25) && actionReady(ACT_ROTATE)) {
    int newRot;
    Point newPos;
    if (tryRotate(current, pos, rot, &newRot, &newPos)) {
      rot = newRot;
      pos = newPos;
      playRotate();
    }
  }
  if (isButtonPressed(27, 33) && actionReady(ACT_HARDDROP)) { dropInstant(); playClick(); }

  placeBlock(current, pos, rot, true);
  drawScreen();
  drawGhost();
}

void setup() {
  Serial.begin(115200);
  tft.init(240, 320);
  tft.setRotation(0);

  for (int r : rows) pinMode(r, OUTPUT);
  for (int c : cols) pinMode(c, INPUT_PULLUP);
  pinMode(RESET_COL, INPUT_PULLUP);

  pinMode(SPEAKER_PIN, OUTPUT);

  drawStartScreen();
}

void loop() {
  switch (gameState) {
    case STATE_START:
      if (anyButtonPressed()) {
        resetGame();
      }
      break;

    case STATE_PAUSED:
      checkPauseButton();
      break;

    case STATE_GAMEOVER:
      if (isButtonPressed(RESET_ROW, RESET_COL) && actionReady(ACT_RESET)) {
        resetGame();
      }
      break;

    case STATE_PLAYING:
      checkPauseButton();
      if (gameState != STATE_PLAYING) break;
      readButtons();
      if (gameState != STATE_PLAYING) break;
      if (millis() - lastFall > fallDelay) {
        lastFall = millis();
        placeBlock(current, pos, rot, false);
        if (canMove(0, 1)) {
          pos.y++;
        } else {
          placeBlock(current, pos, rot, true);
          int cleared = clearLinesLogic();
          if (cleared > 0) {
            int prevLevel = level;
            score += scoreForLines(cleared);
            linesCleared += cleared;
            level = levelForLines(linesCleared);
            fallDelay = fallDelayForLevel(level);
            if (cleared == 4) playTetris(); else playLineClear();
            if (level != prevLevel) playLevelUp();
          }
          drawInfoPanel();
          spawnBlock();
          playDrop();
          if (gameState == STATE_GAMEOVER) {
            drawGameOverScreen();
            playGameOver();
            break;
          }
        }
        placeBlock(current, pos, rot, true);
        drawScreen();
        drawGhost();
      }
      break;
  }
}
