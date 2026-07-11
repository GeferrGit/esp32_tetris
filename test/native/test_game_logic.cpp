#include <cassert>
#include <cstdio>
#include "../../game_logic.h"

static void resetScreen() {
  for (int x = 0; x < widthBlocks; x++)
    for (int y = 0; y < heightBlocks; y++)
      screen[x][y] = 0;
}

static void test_getBlocks_valid_empty_position() {
  resetScreen();
  Point out[4];
  bool valid = getBlocks(blocks[1], {4, 0}, 0, out); // O piece at (4,0)
  assert(valid);
  assert(out[0].x == 4 && out[0].y == 0);
  assert(out[1].x == 5 && out[1].y == 0);
  assert(out[2].x == 4 && out[2].y == 1);
  assert(out[3].x == 5 && out[3].y == 1);
}

static void test_getBlocks_rejects_out_of_bounds_left() {
  resetScreen();
  Point out[4];
  bool valid = getBlocks(blocks[1], {-1, 0}, 0, out);
  assert(!valid);
}

static void test_getBlocks_rejects_out_of_bounds_right() {
  resetScreen();
  Point out[4];
  bool valid = getBlocks(blocks[1], {widthBlocks - 1, 0}, 0, out);
  assert(!valid);
}

static void test_getBlocks_rejects_collision_with_occupied_cell() {
  resetScreen();
  screen[4][1] = 1;
  Point out[4];
  bool valid = getBlocks(blocks[1], {4, 0}, 0, out);
  assert(!valid);
}

int main() {
  test_getBlocks_valid_empty_position();
  test_getBlocks_rejects_out_of_bounds_left();
  test_getBlocks_rejects_out_of_bounds_right();
  test_getBlocks_rejects_collision_with_occupied_cell();
  printf("All game_logic tests passed.\n");
  return 0;
}
