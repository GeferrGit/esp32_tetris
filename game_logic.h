#pragma once

struct Point { int x, y; };

struct Block {
  Point shape[4][4];
  int rotations;
  int color; // piece-type id 1-7; Tetris_ESP32_ST7789_FULL.ino maps this to a real color for drawing
};

const int widthBlocks = 10;
const int heightBlocks = 18;

int screen[widthBlocks][heightBlocks] = {0};

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
