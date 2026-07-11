#pragma once

struct Point { int x, y; };

struct Block {
  Point shape[4][4];
  int rotations;
  int color; // piece-type id 1-7; Tetris_ESP32_ST7789_FULL.ino maps this to a real color for drawing
};

const int widthBlocks = 10;
const int heightBlocks = 18;

int screen[widthBlocks][heightBlocks] = {};

Block blocks[7] = {
  {{{{0,-1},{0,0},{0,1},{0,2}},{{-1,0},{0,0},{1,0},{2,0}}}, 2, 1},
  {{{{0,0},{1,0},{0,1},{1,1}}}, 1, 2},
  {{{{-1,0},{0,0},{1,0},{0,-1}},{{0,-1},{0,0},{0,1},{1,0}},{{-1,0},{0,0},{1,0},{0,1}},{{0,-1},{0,0},{0,1},{-1,0}}}, 4, 3},
  {{{{-1,0},{0,0},{0,-1},{1,-1}},{{0,-1},{0,0},{1,0},{1,1}}}, 2, 4},
  {{{{-1,-1},{0,-1},{0,0},{1,0}},{{1,-1},{1,0},{0,0},{0,1}}}, 2, 5},
  {{{{-1,0},{0,0},{1,0},{1,-1}},{{0,-1},{0,0},{0,1},{1,1}},{{-1,0},{0,0},{1,0},{-1,1}},{{0,-1},{0,0},{0,1},{-1,-1}}}, 4, 6},
  {{{{-1,0},{0,0},{1,0},{-1,-1}},{{0,-1},{0,0},{0,1},{1,-1}},{{-1,0},{0,0},{1,0},{1,1}},{{0,-1},{0,0},{0,1},{-1,1}}}, 4, 7}
};

bool getBlocks(const Block& block, Point pos, int rot, Point out[4]) {
  bool valid = true;
  for (int i = 0; i < 4; i++) {
    Point p = { pos.x + block.shape[rot][i].x, pos.y + block.shape[rot][i].y };
    if (p.x < 0 || p.x >= widthBlocks || p.y < 0 || p.y >= heightBlocks || screen[p.x][p.y])
      valid = false;
    out[i] = p;
  }
  return valid;
}

bool tryRotate(const Block& block, Point pos, int rot, int* outRot, Point* outPos) {
  int newRot = (rot + 1) % block.rotations;
  const Point offsets[4] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}};
  Point test[4];
  for (int i = 0; i < 4; i++) {
    Point tryPos = { pos.x + offsets[i].x, pos.y + offsets[i].y };
    if (getBlocks(block, tryPos, newRot, test)) {
      *outRot = newRot;
      *outPos = tryPos;
      return true;
    }
  }
  return false;
}

Point ghostPosition(const Block& block, Point pos, int rot) {
  Point p = pos;
  Point test[4];
  while (getBlocks(block, {p.x, p.y + 1}, rot, test)) p.y++;
  return p;
}

struct Bag {
  int order[7];
  int index;
};

void refillBag(Bag* bag, int (*randInt)(int)) {
  for (int i = 0; i < 7; i++) bag->order[i] = i;
  for (int i = 6; i > 0; i--) {
    int j = randInt(i + 1);
    int tmp = bag->order[i];
    bag->order[i] = bag->order[j];
    bag->order[j] = tmp;
  }
  bag->index = 0;
}

int nextFromBag(Bag* bag, int (*randInt)(int)) {
  if (bag->index >= 7) refillBag(bag, randInt);
  return bag->order[bag->index++];
}

int scoreForLines(int n) {
  switch (n) {
    case 1: return 100;
    case 2: return 300;
    case 3: return 500;
    case 4: return 800;
    default: return 0;
  }
}

int levelForLines(int totalLines) {
  int lvl = 1 + totalLines / 10;
  return lvl > 10 ? 10 : lvl;
}

int fallDelayForLevel(int level) {
  int d = 2000 - (level - 1) * 200;
  return d < 200 ? 200 : d;
}
