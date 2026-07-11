#include <cassert>
#include <cstdio>
#include <cstdlib>
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

static void test_tryRotate_succeeds_in_open_space() {
  resetScreen();
  int outRot;
  Point outPos;
  bool rotated = tryRotate(blocks[2], {5, 5}, 0, &outRot, &outPos); // T piece, open board
  assert(rotated);
  assert(outRot == 1);
  assert(outPos.x == 5 && outPos.y == 5);
}

static void test_tryRotate_wall_kicks_off_left_edge() {
  resetScreen();
  int outRot;
  Point outPos;
  // I piece standing vertically flush against the left wall; rotating to
  // horizontal at the same x would go out of bounds and needs a kick.
  bool rotated = tryRotate(blocks[0], {0, 5}, 0, &outRot, &outPos);
  assert(rotated);
  assert(outRot == 1);
  assert(outPos.x == 1 && outPos.y == 5);
}

static void test_tryRotate_fails_when_fully_blocked() {
  resetScreen();
  for (int x = 0; x < widthBlocks; x++)
    for (int y = 0; y < heightBlocks; y++)
      screen[x][y] = 1;
  Point pos = {5, 5};
  int rot = 0;
  for (int i = 0; i < 4; i++) {
    Point p = { pos.x + blocks[2].shape[rot][i].x, pos.y + blocks[2].shape[rot][i].y };
    screen[p.x][p.y] = 0; // clear only the T piece's own current footprint
  }
  int outRot = -999;
  Point outPos = {-999, -999};
  bool rotated = tryRotate(blocks[2], pos, rot, &outRot, &outPos);
  assert(!rotated);
  assert(outRot == -999);
  assert(outPos.x == -999 && outPos.y == -999);
}

static void test_ghostPosition_lands_on_floor() {
  resetScreen();
  Point ghost = ghostPosition(blocks[1], {4, 0}, 0);
  Point cells[4];
  assert(getBlocks(blocks[1], ghost, 0, cells));
  assert(!getBlocks(blocks[1], {ghost.x, ghost.y + 1}, 0, cells));
}

static void test_ghostPosition_lands_on_stack() {
  resetScreen();
  for (int x = 0; x < widthBlocks; x++) screen[x][10] = 1;
  Point ghost = ghostPosition(blocks[1], {4, 0}, 0);
  assert(ghost.y == 8);
}

static int stdRandInt(int n) { return rand() % n; }

static void test_refillBag_produces_permutation() {
  srand(99);
  Bag bag;
  refillBag(&bag, stdRandInt);
  bool seen[7] = {false,false,false,false,false,false,false};
  for (int i = 0; i < 7; i++) {
    assert(bag.order[i] >= 0 && bag.order[i] < 7);
    assert(!seen[bag.order[i]]);
    seen[bag.order[i]] = true;
  }
  assert(bag.index == 0);
}

static void test_bag_two_cycles_each_piece_exactly_twice() {
  srand(7);
  Bag bag = {{0,0,0,0,0,0,0}, 7}; // index=7 forces a refill on the first draw
  int counts[7] = {0,0,0,0,0,0,0};
  for (int i = 0; i < 14; i++) {
    int piece = nextFromBag(&bag, stdRandInt);
    counts[piece]++;
  }
  for (int i = 0; i < 7; i++) assert(counts[i] == 2);
}

int main() {
  test_getBlocks_valid_empty_position();
  test_getBlocks_rejects_out_of_bounds_left();
  test_getBlocks_rejects_out_of_bounds_right();
  test_getBlocks_rejects_collision_with_occupied_cell();
  test_tryRotate_succeeds_in_open_space();
  test_tryRotate_wall_kicks_off_left_edge();
  test_tryRotate_fails_when_fully_blocked();
  test_ghostPosition_lands_on_floor();
  test_ghostPosition_lands_on_stack();
  test_refillBag_produces_permutation();
  test_bag_two_cycles_each_piece_exactly_twice();
  printf("All game_logic tests passed.\n");
  return 0;
}
