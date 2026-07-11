# Tetris ESP32 Button Fixes & Full Classic Feature Set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the button-matrix bugs that leave Right/Rotate/Hard-Drop permanently dead, close the array-out-of-bounds risk in rotation, and add the full classic-Tetris feature set (wall-kick rotation, ghost piece, 7-bag randomizer, standard scoring/leveling, start/game-over screens, expanded sound) to `Tetris_ESP32_ST7789_FULL.ino`, per `docs/superpowers/specs/2026-07-11-tetris-full-rewrite-design.md`.

**Architecture:** Extract the hardware-independent game logic (collision detection, rotation+wall-kick, ghost-piece landing, 7-bag randomizer, scoring/leveling math, line-clear) into a new header `game_logic.h`, included by the `.ino`. This is the one deliberate deviation from "everything in one file": it costs one extra file and buys a real automated test loop (native `g++`/`clang++`, no ESP32 toolchain needed) for exactly the code that caused today's bugs. Everything hardware-facing (display, buttons, speaker, `setup()`/`loop()`) stays in the single `.ino`, verified by manual trace plus an on-device checklist, matching the spec's approved verification approach.

**Tech Stack:** Arduino/ESP32 (`Adafruit_GFX`, `Adafruit_ST7789`, `SPI`), C++17 native (`g++`/`clang++`) for the logic unit tests.

## Global Constraints

- Production display/input/sound code stays in `Tetris_ESP32_ST7789_FULL.ino` — only pure game logic moves to `game_logic.h`. (Spec: "у межах одного файлу .ino"; this plan's one addition is scoped and justified above.)
- No new hardware or pin changes — only reassignment of already-wired, currently-unused matrix combinations from `rows[]={32,26,27}` × `cols[]={14,25,33}`.
- No external test framework — native tests use `<cassert>` only, since nothing else is guaranteed installed.
- No `arduino-cli` is available in this environment — every `.ino`-only change is verified by manual trace in its task, and the final task adds an on-device checklist for the user to run after flashing.
- Speaker stays on `SPEAKER_PIN` (pin 4) via `tone()`, matching current hardware wiring.
- Button reassignment locked in by the approved spec: Right → `(26,14)`, Rotate → `(26,25)`, Hard Drop → `(27,33)`.

---

## File Structure

- **Create:** `game_logic.h` — pure game logic, zero Arduino/display dependencies: `Point`/`Block` structs, `blocks[7]` (piece shapes, with placeholder color IDs 1–7 instead of `ST77XX_*` values), `screen[][]`, `getBlocks()`, `tryRotate()`, `ghostPosition()`, the 7-bag randomizer (`Bag`, `refillBag()`, `nextFromBag()`), `scoreForLines()`, `levelForLines()`, `fallDelayForLevel()`, `clearLinesLogic()`.
- **Create:** `test/native/test_game_logic.cpp` — native (non-Arduino) unit tests for everything in `game_logic.h`, using `<cassert>`. Grows incrementally, one task at a time.
- **Modify:** `Tetris_ESP32_ST7789_FULL.ino` — becomes the hardware/rendering/input/sound/state-machine layer: includes `game_logic.h`, maps color IDs to real `ST77XX_*` values for drawing, wires button matrix to actions, drives `setup()`/`loop()` via a `GameState` enum, non-blocking debounce, ghost-piece rendering, start/pause/game-over screens, and expanded sound effects.

---

### Task 1: Fix the button matrix pin bugs (Right / Rotate / Hard Drop)

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino:217-221` (inside `readButtons()`)

**Interfaces:**
- Consumes: existing `isButtonPressed(int rowPin, int colPin)` (unchanged), `rows[]={32,26,27}`, `cols[]={14,25,33}`.
- Produces: no new symbols — this task only changes literal pin arguments.

- [ ] **Step 1: Replace the broken pin arguments**

Find these 5 lines in `readButtons()`:

```cpp
  if (isButtonPressed(27, 14) && canMove(-1, 0)) { pos.x--; playClick(); delay(100); }
  if (isButtonPressed(26, 27) && canMove(1, 0))  { pos.x++; playClick(); delay(100); }
  if (isButtonPressed(32, 14) && canMove(0, 1))  { pos.y++; playClick(); delay(100); }
  if (isButtonPressed(25, 26))                   { rot = (rot + 1) % current.rotations; playClick(); delay(150); }
  if (isButtonPressed(32, 26))                   { dropInstant(); playClick(); delay(150); }
```

Replace with:

```cpp
  if (isButtonPressed(27, 14) && canMove(-1, 0)) { pos.x--; playClick(); delay(100); }
  if (isButtonPressed(26, 14) && canMove(1, 0))  { pos.x++; playClick(); delay(100); }
  if (isButtonPressed(32, 14) && canMove(0, 1))  { pos.y++; playClick(); delay(100); }
  if (isButtonPressed(26, 25))                   { rot = (rot + 1) % current.rotations; playClick(); delay(150); }
  if (isButtonPressed(27, 33))                   { dropInstant(); playClick(); delay(150); }
```

- [ ] **Step 2: Manually verify every combination is valid and unique**

Check each `(rowPin, colPin)` pair used anywhere in the file against `rows[]={32,26,27}` and `cols[]={14,25,33}`:

| Action     | rowPin | colPin | rowPin ∈ rows? | colPin ∈ cols? | Unique? |
|------------|--------|--------|-----------------|------------------|---------|
| Left       | 27     | 14     | yes             | yes              | yes |
| Right      | 26     | 14     | yes             | yes              | yes |
| Down       | 32     | 14     | yes             | yes              | yes |
| Rotate     | 26     | 25     | yes             | yes              | yes |
| Hard Drop  | 27     | 33     | yes             | yes              | yes |
| Pause      | 32     | 33     | yes             | yes              | yes |
| Reset      | 26 (RESET_ROW) | 13 (RESET_COL) | yes (dedicated scan) | yes (dedicated pin) | yes |

All 7 pairs are distinct and every `rowPin`/`colPin` is drawn from the pin actually configured as `OUTPUT`/`INPUT_PULLUP` for that role. No further code changes needed for this task.

- [ ] **Step 3: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "fix: correct button matrix pins for Right/Rotate/Hard Drop"
```

---

### Task 2: `game_logic.h` — Point/Block/blocks/getBlocks + native test harness

**Files:**
- Create: `game_logic.h`
- Create: `test/native/test_game_logic.cpp`

**Interfaces:**
- Produces: `struct Point {int x,y;}`, `struct Block {Point shape[4][4]; int rotations; int color;}`, `const int widthBlocks=10`, `const int heightBlocks=18`, `int screen[widthBlocks][heightBlocks]`, `Block blocks[7]` (color field holds placeholder IDs 1–7, in the same shape/rotation order as the original file), `bool getBlocks(const Block& block, Point pos, int rot, Point out[4])`.

- [ ] **Step 1: Create `game_logic.h` with a stub `getBlocks`**

```cpp
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
  (void)block; (void)pos; (void)rot; (void)out;
  return false; // stub, implemented in Step 4
}
```

- [ ] **Step 2: Create the native test file**

```cpp
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
```

- [ ] **Step 3: Run the tests and verify they fail**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: compiles, then aborts with an assertion failure inside `test_getBlocks_valid_empty_position` (the stub always returns `false`).

- [ ] **Step 4: Implement the real `getBlocks`**

In `game_logic.h`, replace the stub body:

```cpp
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
```

- [ ] **Step 5: Run the tests and verify they pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: prints `All game_logic tests passed.` and exits 0.

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: extract getBlocks into testable game_logic.h with native tests"
```

---

### Task 3: `game_logic.h` — `tryRotate` with wall-kick

**Files:**
- Modify: `game_logic.h`
- Modify: `test/native/test_game_logic.cpp`

**Interfaces:**
- Consumes: `getBlocks` (Task 2), `blocks[7]` (Task 2).
- Produces: `bool tryRotate(const Block& block, Point pos, int rot, int* outRot, Point* outPos)` — tries rotating one step clockwise; on success writes the new rotation/position and returns `true`; on failure leaves outputs untouched and returns `false`.

- [ ] **Step 1: Add a stub `tryRotate`**

Append to `game_logic.h`:

```cpp
bool tryRotate(const Block& block, Point pos, int rot, int* outRot, Point* outPos) {
  (void)block; (void)pos; (void)rot; (void)outRot; (void)outPos;
  return false; // stub, implemented in Step 3
}
```

- [ ] **Step 2: Add failing tests**

Append to `test/native/test_game_logic.cpp` (above `int main()`):

```cpp
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
```

And add the three calls inside `main()`, before `printf`:

```cpp
  test_tryRotate_succeeds_in_open_space();
  test_tryRotate_wall_kicks_off_left_edge();
  test_tryRotate_fails_when_fully_blocked();
```

- [ ] **Step 3: Run tests, verify failure**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: aborts with an assertion failure inside `test_tryRotate_succeeds_in_open_space` (stub always returns `false`).

- [ ] **Step 4: Implement the real `tryRotate`**

Replace the stub in `game_logic.h`:

```cpp
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
```

- [ ] **Step 5: Run tests, verify pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: prints `All game_logic tests passed.` and exits 0.

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: add tryRotate with wall-kick to game_logic.h"
```

---

### Task 4: `game_logic.h` — `ghostPosition`

**Files:**
- Modify: `game_logic.h`
- Modify: `test/native/test_game_logic.cpp`

**Interfaces:**
- Consumes: `getBlocks` (Task 2).
- Produces: `Point ghostPosition(const Block& block, Point pos, int rot)` — returns the resting position if the piece were hard-dropped from `pos`, without mutating any state.

- [ ] **Step 1: Add a stub**

Append to `game_logic.h`:

```cpp
Point ghostPosition(const Block& block, Point pos, int rot) {
  (void)block; (void)rot;
  return pos; // stub, implemented in Step 3
}
```

- [ ] **Step 2: Add failing tests**

Append to `test/native/test_game_logic.cpp`:

```cpp
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
```

Add the calls in `main()`:

```cpp
  test_ghostPosition_lands_on_floor();
  test_ghostPosition_lands_on_stack();
```

- [ ] **Step 3: Run tests, verify failure**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `test_ghostPosition_lands_on_floor` fails (stub returns the input position unchanged, which is not resting on the floor).

- [ ] **Step 4: Implement the real `ghostPosition`**

```cpp
Point ghostPosition(const Block& block, Point pos, int rot) {
  Point p = pos;
  Point test[4];
  while (getBlocks(block, {p.x, p.y + 1}, rot, test)) p.y++;
  return p;
}
```

- [ ] **Step 5: Run tests, verify pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `All game_logic tests passed.`

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: add ghostPosition to game_logic.h"
```

---

### Task 5: `game_logic.h` — 7-bag randomizer

**Files:**
- Modify: `game_logic.h`
- Modify: `test/native/test_game_logic.cpp`

**Interfaces:**
- Produces: `struct Bag { int order[7]; int index; }`, `void refillBag(Bag* bag, int (*randInt)(int))`, `int nextFromBag(Bag* bag, int (*randInt)(int))` — `randInt(n)` must return a uniform value in `[0, n)`; the caller supplies it so tests can be deterministic and the `.ino` can wrap Arduino's `random()`.

- [ ] **Step 1: Add stubs**

Append to `game_logic.h`:

```cpp
struct Bag {
  int order[7];
  int index;
};

void refillBag(Bag* bag, int (*randInt)(int)) {
  (void)bag; (void)randInt; // stub, implemented in Step 3
}

int nextFromBag(Bag* bag, int (*randInt)(int)) {
  (void)bag; (void)randInt;
  return 0; // stub, implemented in Step 3
}
```

- [ ] **Step 2: Add failing tests**

Append to `test/native/test_game_logic.cpp` (add `#include <cstdlib>` to the top of the file alongside the existing includes):

```cpp
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
```

Add the calls in `main()`:

```cpp
  test_refillBag_produces_permutation();
  test_bag_two_cycles_each_piece_exactly_twice();
```

- [ ] **Step 3: Run tests, verify failure**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: assertion failure inside `test_refillBag_produces_permutation` (`bag.order` is uninitialized garbage from the stub).

- [ ] **Step 4: Implement the real functions**

```cpp
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
```

- [ ] **Step 5: Run tests, verify pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `All game_logic tests passed.`

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: add 7-bag randomizer to game_logic.h"
```

---

### Task 6: `game_logic.h` — scoring and leveling math

**Files:**
- Modify: `game_logic.h`
- Modify: `test/native/test_game_logic.cpp`

**Interfaces:**
- Produces: `int scoreForLines(int n)` (0/100/300/500/800 for 0/1/2/3/4 lines), `int levelForLines(int totalLines)` (level = `1 + totalLines/10`, capped at 10), `int fallDelayForLevel(int level)` (`2000 - (level-1)*200`, floored at 200).

- [ ] **Step 1: Add stubs**

Append to `game_logic.h`:

```cpp
int scoreForLines(int n) {
  (void)n;
  return -1; // stub, implemented in Step 3
}

int levelForLines(int totalLines) {
  (void)totalLines;
  return -1; // stub, implemented in Step 3
}

int fallDelayForLevel(int level) {
  (void)level;
  return -1; // stub, implemented in Step 3
}
```

- [ ] **Step 2: Add failing tests**

Append to `test/native/test_game_logic.cpp`:

```cpp
static void test_scoreForLines_table() {
  assert(scoreForLines(0) == 0);
  assert(scoreForLines(1) == 100);
  assert(scoreForLines(2) == 300);
  assert(scoreForLines(3) == 500);
  assert(scoreForLines(4) == 800);
}

static void test_levelForLines_progression() {
  assert(levelForLines(0) == 1);
  assert(levelForLines(9) == 1);
  assert(levelForLines(10) == 2);
  assert(levelForLines(19) == 2);
  assert(levelForLines(20) == 3);
  assert(levelForLines(1000) == 10);
}

static void test_fallDelayForLevel_progression_and_floor() {
  assert(fallDelayForLevel(1) == 2000);
  assert(fallDelayForLevel(2) == 1800);
  assert(fallDelayForLevel(10) == 200);
  assert(fallDelayForLevel(50) == 200);
}
```

Add the calls in `main()`:

```cpp
  test_scoreForLines_table();
  test_levelForLines_progression();
  test_fallDelayForLevel_progression_and_floor();
```

- [ ] **Step 3: Run tests, verify failure**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: assertion failure inside `test_scoreForLines_table` (stub returns `-1`).

- [ ] **Step 4: Implement the real functions**

```cpp
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
```

- [ ] **Step 5: Run tests, verify pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `All game_logic tests passed.`

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: add scoreForLines/levelForLines/fallDelayForLevel to game_logic.h"
```

---

### Task 7: `game_logic.h` — `clearLinesLogic`

**Files:**
- Modify: `game_logic.h`
- Modify: `test/native/test_game_logic.cpp`

**Interfaces:**
- Consumes: `screen[][]`, `widthBlocks`, `heightBlocks` (Task 2).
- Produces: `int clearLinesLogic()` — clears every full row in `screen[][]`, shifts rows above down, and returns the count of rows cleared. Does **not** touch score/level/fallDelay — that wiring happens in Task 9.

- [ ] **Step 1: Add a stub**

Append to `game_logic.h`:

```cpp
int clearLinesLogic() {
  return -1; // stub, implemented in Step 3
}
```

- [ ] **Step 2: Add failing tests**

Append to `test/native/test_game_logic.cpp`:

```cpp
static void test_clearLinesLogic_no_full_lines() {
  resetScreen();
  screen[0][17] = 1;
  int cleared = clearLinesLogic();
  assert(cleared == 0);
  assert(screen[0][17] == 1);
}

static void test_clearLinesLogic_clears_single_full_line() {
  resetScreen();
  for (int x = 0; x < widthBlocks; x++) screen[x][17] = 1;
  screen[3][16] = 2;
  int cleared = clearLinesLogic();
  assert(cleared == 1);
  assert(screen[3][17] == 2);
  for (int x = 0; x < widthBlocks; x++)
    if (x != 3) assert(screen[x][17] == 0);
}

static void test_clearLinesLogic_clears_multiple_simultaneous_lines() {
  resetScreen();
  for (int x = 0; x < widthBlocks; x++) {
    screen[x][16] = 1;
    screen[x][17] = 1;
  }
  screen[5][15] = 3;
  int cleared = clearLinesLogic();
  assert(cleared == 2);
  assert(screen[5][17] == 3);
}
```

Add the calls in `main()`:

```cpp
  test_clearLinesLogic_no_full_lines();
  test_clearLinesLogic_clears_single_full_line();
  test_clearLinesLogic_clears_multiple_simultaneous_lines();
```

- [ ] **Step 3: Run tests, verify failure**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: assertion failure inside `test_clearLinesLogic_no_full_lines` (stub returns `-1`).

- [ ] **Step 4: Implement the real function**

```cpp
int clearLinesLogic() {
  int cleared = 0;
  for (int y = heightBlocks - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < widthBlocks; x++)
      if (!screen[x][y]) full = false;
    if (full) {
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < widthBlocks; x++)
          screen[x][yy] = screen[x][yy - 1];
      for (int x = 0; x < widthBlocks; x++) screen[x][0] = 0;
      cleared++;
      y++;
    }
  }
  return cleared;
}
```

- [ ] **Step 5: Run tests, verify pass**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `All game_logic tests passed.`

- [ ] **Step 6: Commit**

```bash
git add game_logic.h test/native/test_game_logic.cpp
git commit -m "feat: add clearLinesLogic to game_logic.h"
```

`game_logic.h` is now complete and fully covered by native tests. The remaining tasks wire it into the `.ino` and add the hardware-facing features.

---

### Task 8: Wire `game_logic.h` into the `.ino` (structural swap, no behavior change)

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Consumes: everything produced by Task 2 (`Point`, `Block`, `blocks[7]`, `widthBlocks`, `heightBlocks`, `screen[][]`, `getBlocks`).
- Produces: `const uint16_t blockColor565[8]` — maps a `screen[][]`/`Block.color` id (0 = empty, 1–7 = piece type) to the real `ST77XX_*` color used for drawing.

- [ ] **Step 1: Add the include and remove the now-duplicated definitions**

At the top of `Tetris_ESP32_ST7789_FULL.ino`, after the existing includes (after line 3), add:

```cpp
#include "game_logic.h"
```

Remove these blocks (now provided by `game_logic.h`):
- Lines 11–16 (`struct Point { ... }; struct Block { ... };`)
- The `widthBlocks`, `heightBlocks`, and `int screen[widthBlocks][heightBlocks] = {0};` declarations from the const block at lines 18–23 — keep `blockSize`, `offsetX`, `infoPanelX` (these are rendering-only and stay in the `.ino`).
- Lines 44–52 (the old `Block blocks[7] = { ... }` with `ST77XX_*` colors)
- Lines 77–87 (the old `getBlocks` function)

- [ ] **Step 2: Add the color lookup table**

Add near the top, after the `blocks[7]` array is now coming from the header (place this right after the `#include "game_logic.h"` line, or anywhere before `drawBlock`):

```cpp
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
```

This preserves the exact original per-piece colors (verified by matching the original `blocks[7]` array's color-per-index against this table's index order).

- [ ] **Step 3: Update `drawBlock` to map through the color table**

Find:

```cpp
void drawBlock(int x, int y, uint16_t color) {
  tft.fillRect(offsetX + x * blockSize, y * blockSize, blockSize - 1, blockSize - 1, color);
}
```

Replace with:

```cpp
void drawBlock(int x, int y, int colorId) {
  tft.fillRect(offsetX + x * blockSize, y * blockSize, blockSize - 1, blockSize - 1, blockColor565[colorId]);
}
```

- [ ] **Step 4: Update the "Next" preview draw in `drawInfoPanel`**

Find:

```cpp
  for (int i = 0; i < 4; i++) {
    tft.fillRect(infoPanelX + 10 + preview[i].x * 10, 20 + preview[i].y * 10, 9, 9, next.color);
  }
```

Replace with:

```cpp
  for (int i = 0; i < 4; i++) {
    tft.fillRect(infoPanelX + 10 + preview[i].x * 10, 20 + preview[i].y * 10, 9, 9, blockColor565[next.color]);
  }
```

- [ ] **Step 5: Manually verify no behavior change**

Trace through: `placeBlock` still writes `block.color` (now 1–7) into `screen[][]`; `drawScreen()` still calls `drawBlock(x, y, screen[x][y])` for every cell — now `screen[x][y]` holds a color *id* (0–7) instead of a raw `ST77XX_*` value, and `drawBlock` looks it up through `blockColor565`. Since `blockColor565[0] == ST77XX_BLACK` (same as the original bare `0` did when passed straight to `fillRect`), empty-cell rendering is unchanged. Since `blockColor565[1..7]` exactly reproduces each piece's original color (Step 2's table), rendering is pixel-identical to before this task.

- [ ] **Step 6: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "refactor: wire game_logic.h into the sketch, map color ids for rendering"
```

---

### Task 9: Wire scoring, leveling, and `clearLinesLogic` into the `.ino`

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Consumes: `clearLinesLogic()`, `scoreForLines()`, `levelForLines()`, `fallDelayForLevel()` (Task 6, 7).
- Produces: `void playLineClear()`, `void playTetris()`, `void playLevelUp()`.

- [ ] **Step 1: Remove the old `clearLines()` function**

Delete lines 153–172 (the old `void clearLines() { ... }`, which mixed line-shifting with scoring/leveling that is now handled separately).

- [ ] **Step 2: Add the new sound functions**

Add near `playClick()`/`playDrop()`:

```cpp
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
```

These use `delay()` internally (up to ~400ms for `playTetris`, a rare event) rather than a non-blocking sequencer — an intentional simplification since a full async melody player is unwarranted complexity for occasional celebratory jingles.

- [ ] **Step 3: Wire the new logic into `loop()`**

Find, inside `loop()`:

```cpp
    else {
      placeBlock(current, pos, rot, true);
      clearLines();
      drawInfoPanel();
      spawnBlock();
      playDrop();
    }
```

Replace with:

```cpp
    else {
      placeBlock(current, pos, rot, true);
      int cleared = clearLinesLogic();
      if (cleared > 0) {
        int prevLevel = level;
        score += scoreForLines(cleared);
        linesCleared += cleared;
        level = levelForLines(linesCleared);
        fallDelay = fallDelayForLevel(level);
        if (cleared == 4) playTetris(); else if (cleared > 0) playLineClear();
        if (level != prevLevel) playLevelUp();
      }
      drawInfoPanel();
      spawnBlock();
      playDrop();
    }
```

- [ ] **Step 4: Manually verify**

Trace with a concrete example: before this change, clearing 2 lines added a flat `+200` (100 per line) and only advanced the level every 100 lines. After this change, clearing 2 lines calls `clearLinesLogic()` (returns `2`), adds `scoreForLines(2) == 300` to `score`, adds `2` to `linesCleared`, and recomputes `level` via `levelForLines(linesCleared)` — e.g. going from 8 to 10 total lines crosses the `10`-line threshold and `level` becomes `2`, which is not equal to `prevLevel` (`1`), so `playLevelUp()` fires. This matches the spec's scoring/leveling table.

- [ ] **Step 5: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "feat: wire standard scoring/leveling and clearLinesLogic into the sketch"
```

---

### Task 10: Wire the 7-bag randomizer into `spawnBlock`/`resetGame`/`setup`

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Consumes: `Bag`, `refillBag()`, `nextFromBag()` (Task 5).
- Produces: `int arduinoRandInt(int n)`, global `Bag pieceBag`.

- [ ] **Step 1: Add the bag global and Arduino random adapter**

Add near the other globals (after `Block current; Block next;`):

```cpp
Bag pieceBag = {{0, 0, 0, 0, 0, 0, 0}, 7}; // index=7 forces a refill on first use

int arduinoRandInt(int n) { return random(n); }
```

- [ ] **Step 2: Replace `random(7)` call sites**

In `spawnBlock()`, find:

```cpp
  next = blocks[random(7)];
```

Replace with:

```cpp
  next = blocks[nextFromBag(&pieceBag, arduinoRandInt)];
```

In `resetGame()`, find:

```cpp
  next = blocks[random(7)];
  spawnBlock();
```

Replace with:

```cpp
  pieceBag.index = 7; // start each new game with a fresh, fair sequence
  next = blocks[nextFromBag(&pieceBag, arduinoRandInt)];
  spawnBlock();
```

In `setup()`, find:

```cpp
  next = blocks[random(7)];
  spawnBlock();
```

Replace with:

```cpp
  next = blocks[nextFromBag(&pieceBag, arduinoRandInt)];
  spawnBlock();
```

- [ ] **Step 3: Manually verify**

`nextFromBag`/`refillBag` are already covered by native tests (Task 5) for correctness of the shuffle/fairness. The only new risk here is wiring: confirm every one of the 3 former `random(7)` call sites now goes through `nextFromBag(&pieceBag, arduinoRandInt)`, and that `pieceBag.index` is forced to `7` at the start of `resetGame()` so each new game reshuffles instead of continuing the previous game's bag.

- [ ] **Step 4: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "feat: wire 7-bag randomizer into piece spawning"
```

---

### Task 11: Wire `tryRotate` (fixing the OOB bug for real) and ghost-piece rendering

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Consumes: `tryRotate()` (Task 3), `ghostPosition()` (Task 4), `getBlocks()` (Task 2).
- Produces: `void playRotate()`, `void drawGhost()`.

- [ ] **Step 1: Add the rotate sound**

Add near `playClick()`:

```cpp
void playRotate() {
  tone(SPEAKER_PIN, 1400, 50);
}
```

- [ ] **Step 2: Replace the naive rotation with `tryRotate`**

Find, in `readButtons()` (this is the line Task 1 already fixed the pin numbers on):

```cpp
  if (isButtonPressed(26, 25))                   { rot = (rot + 1) % current.rotations; playClick(); delay(150); }
```

Replace with:

```cpp
  if (isButtonPressed(26, 25)) {
    int newRot;
    Point newPos;
    if (tryRotate(current, pos, rot, &newRot, &newPos)) {
      rot = newRot;
      pos = newPos;
      playRotate();
    }
    delay(150);
  }
```

- [ ] **Step 3: Add ghost-piece rendering**

Add a new function near `drawScreen()`:

```cpp
void drawGhost() {
  Point ghost = ghostPosition(current, pos, rot);
  if (ghost.y == pos.y) return; // already resting, no separate ghost needed
  Point cells[4];
  getBlocks(current, ghost, rot, cells);
  for (int i = 0; i < 4; i++)
    tft.drawRect(offsetX + cells[i].x * blockSize, cells[i].y * blockSize, blockSize - 1, blockSize - 1, ST77XX_WHITE);
}
```

Call it immediately after every existing `drawScreen()` call:
- At the end of `readButtons()`, after `drawScreen();`, add `drawGhost();`.
- In `loop()`, after the `drawScreen();` inside the gravity-tick block, add `drawGhost();`.

- [ ] **Step 4: Manually verify**

`tryRotate` and `ghostPosition` are already covered by native tests. The wiring risk here is the OOB bug this task fixes: previously `rot` was mutated unconditionally with no bounds check, so `placeBlock(current, pos, rot, true)` right after could write to `screen[x][y]` with `x`/`y` outside `[0, widthBlocks)`/`[0, heightBlocks)`. Now `rot`/`pos` are only mutated when `tryRotate` returns `true`, which (per Task 3's tests) only happens when the resulting cells are confirmed in-bounds and unoccupied by `getBlocks`. So `placeBlock` afterward can never receive an invalid position.

- [ ] **Step 5: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "fix: use wall-kick rotation to close the array-bounds bug; add ghost piece"
```

---

### Task 12: Non-blocking button debounce

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Produces: `enum Action { ACT_PAUSE, ACT_LEFT, ACT_RIGHT, ACT_DOWN, ACT_ROTATE, ACT_HARDDROP, ACT_RESET, ACT_COUNT }`, `bool actionReady(Action a)`.

- [ ] **Step 1: Add the debounce table**

Add near the button matrix globals:

```cpp
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
```

- [ ] **Step 2: Replace `delay()`-based debounce in `readButtons()` with `actionReady`**

Replace the whole body of `readButtons()` with:

```cpp
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

  if (isButtonPressed(32, 33) && actionReady(ACT_PAUSE)) { paused = !paused; playClick(); return; }
  if (paused) return;
  placeBlock(current, pos, rot, false);

  if (isButtonPressed(27, 14) && canMove(-1, 0) && actionReady(ACT_LEFT)) { pos.x--; playClick(); }
  if (isButtonPressed(26, 14) && canMove(1, 0) && actionReady(ACT_RIGHT)) { pos.x++; playClick(); }
  if (isButtonPressed(32, 14) && canMove(0, 1) && actionReady(ACT_DOWN))  { pos.y++; playClick(); }
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
```

Note: for Left/Right/Down, `actionReady` is checked *after* `canMove`, matching the original semantics where the cooldown only started once the move actually happened (holding a button against a wall doesn't burn the cooldown). For Pause/Rotate/Hard-Drop/Reset, `actionReady` gates on the button press itself, matching the original's unconditional `delay()` on every press.

- [ ] **Step 3: Manually verify**

Trace with hypothetical `millis()` values: suppose `ACT_LEFT`'s last successful move was at `t=1000` (cooldown 100ms). At `t=1050`, `isButtonPressed(27,14)` is still `true` (held) and `canMove(-1,0)` is `true` — but `actionReady(ACT_LEFT)` checks `1050 - 1000 = 50 < 100`, returns `false` without updating `lastActionTime[ACT_LEFT]`, so no move happens. At `t=1100`, `1100 - 1000 = 100 >= 100`, `actionReady` returns `true` and updates `lastActionTime[ACT_LEFT] = 1100`. This reproduces the original ~10-moves/second repeat rate while holding, but without blocking `loop()` — so the gravity check (`millis() - lastFall > fallDelay`) right after `readButtons()` returns is no longer delayed by a `delay()` call sitting inside the button handling.

- [ ] **Step 4: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "refactor: replace blocking delay() debounce with non-blocking millis() cooldown"
```

---

### Task 13: `GameState` machine — start screen, non-blocking pause/game-over

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

**Interfaces:**
- Consumes: `readButtons()` (Task 12), `isButtonPressed()`, `rows[]`, `cols[]`, `RESET_ROW`, `RESET_COL`.
- Produces: `enum GameState { STATE_START, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER }`, `void drawStartScreen()`, `void drawPauseOverlay()`, `void drawGameOverScreen()`, `bool anyButtonPressed()`, `void checkPauseButton()`, `void playGameOver()`.

- [ ] **Step 1: Replace the `paused`/`gameOver` bools with a `GameState` enum**

Find:

```cpp
bool gameOver = false;
bool paused = false;
```

Replace with:

```cpp
enum GameState { STATE_START, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER };
GameState gameState = STATE_START;
```

- [ ] **Step 2: Add the screen-drawing and sound functions**

Add near `drawInfoPanel()`:

```cpp
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
```

Add near `playClick()`:

```cpp
void playGameOver() {
  tone(SPEAKER_PIN, 500, 100);
  delay(120);
  tone(SPEAKER_PIN, 350, 150);
}
```

- [ ] **Step 3: Add `anyButtonPressed` and `checkPauseButton`**

Add near `readButtons()`:

```cpp
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
```

- [ ] **Step 4: Remove pause handling from `readButtons`**

`readButtons()` will now only ever be called while `gameState == STATE_PLAYING` (pause is handled by `checkPauseButton()` in `loop()` instead). Remove these two lines from the top of `readButtons()`:

```cpp
  if (isButtonPressed(32, 33) && actionReady(ACT_PAUSE)) { paused = !paused; playClick(); return; }
  if (paused) return;
```

- [ ] **Step 5: Update `spawnBlock` and `resetGame` to use `gameState`**

In `spawnBlock()`, find:

```cpp
  if (!getBlocks(current, pos, rot, test)) gameOver = true;
```

Replace with:

```cpp
  if (!getBlocks(current, pos, rot, test)) gameState = STATE_GAMEOVER;
```

In `resetGame()`, find:

```cpp
  gameOver = false;
  paused = false;
```

Replace with nothing (delete those two lines), and add this as the **last line** of `resetGame()` (after the existing `drawInfoPanel();`):

```cpp
  gameState = STATE_PLAYING;
```

- [ ] **Step 6: Rewrite `setup()`**

Replace the whole `setup()` function with:

```cpp
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
```

(The game no longer auto-starts; it waits in `STATE_START` until the first button press calls `resetGame()`.)

- [ ] **Step 7: Rewrite `loop()`**

Replace the whole `loop()` function with:

```cpp
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
```

- [ ] **Step 8: Manually verify the state transitions**

Trace each transition:
- Power-on → `setup()` draws the start screen, `gameState == STATE_START`.
- Any button in `STATE_START` → `anyButtonPressed()` true → `resetGame()` runs (clears board, resets score/level/bag) and sets `gameState = STATE_PLAYING` as its last line.
- Pause button in `STATE_PLAYING` → `checkPauseButton()` flips to `STATE_PAUSED`, draws overlay once, and the `if (gameState != STATE_PLAYING) break;` right after prevents `readButtons()`/gravity from running that same frame.
- Pause button again in `STATE_PAUSED` → `checkPauseButton()` flips back to `STATE_PLAYING`; next `loop()` iteration resumes gravity/input normally (no stale "PAUSE" text since the next `drawScreen()` call, which happens on the very next gravity tick or button move, repaints the whole board over it — same self-healing behavior as the original).
- `spawnBlock()` detects no room for the new piece → sets `gameState = STATE_GAMEOVER` → the `if (gameState == STATE_GAMEOVER) { drawGameOverScreen(); playGameOver(); break; }` right after `spawnBlock()` fires immediately, skipping the final `placeBlock`/`drawScreen`/`drawGhost` (which would otherwise try to draw a piece that was never placed). No `while(1)` anywhere — `loop()` keeps returning normally, so `STATE_GAMEOVER`'s branch (checking Reset) runs on every subsequent call.
- Reset in `STATE_GAMEOVER` → `resetGame()` runs and sets `gameState = STATE_PLAYING`, matching the original's direct-restart behavior (no detour through the start screen).

- [ ] **Step 9: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "refactor: replace bool flags and blocking game-over loop with GameState machine; add start screen"
```

---

### Task 14: Final review and on-device test checklist

**Files:**
- Modify: `Tetris_ESP32_ST7789_FULL.ino`

- [ ] **Step 1: Add an on-device checklist comment**

Add this comment block right after the `#include` lines at the top of the file:

```cpp
/*
 * Ручний чек-лист після прошивки плати:
 * 1. Left/Right/Down/Rotate/HardDrop/Pause/Reset - кожна кнопка виконує свою дію.
 * 2. Обертання біля стін/дна не виходить за межі поля і не "зависає".
 * 3. Ghost piece показує правильну позицію приземлення.
 * 4. За 7-14 фігур поспіль немає підозріло довгих серій однакової фігури.
 * 5. Рахунок: 1 лінія = +100, 2 = +300, 3 = +500, 4 = +800.
 * 6. Рівень підвищується кожні 10 очищених ліній; швидкість падіння зростає.
 * 7. Стартовий екран показується при подачі живлення; гра стартує після першої кнопки.
 * 8. Game Over показує фінальний рахунок і не зависає; Reset одразу починає нову гру.
 * 9. Усі звукові події чутні і відрізняються одна від одної.
 */
```

- [ ] **Step 2: Run the full native test suite one more time**

Run: `g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic`
Expected: `All game_logic tests passed.`

- [ ] **Step 3: Read through the full diff against `main`**

Run: `git diff main -- Tetris_ESP32_ST7789_FULL.ino game_logic.h test/native/test_game_logic.cpp`

Confirm: no leftover references to the removed `paused`/`gameOver` bools or the old `clearLines()`/`getBlocks()` functions; every button pin pair matches Task 1's table; `game_logic.h` has no `#include <Arduino.h>` or any Adafruit/display dependency (keeps it natively testable going forward).

- [ ] **Step 4: Commit**

```bash
git add Tetris_ESP32_ST7789_FULL.ino
git commit -m "docs: add on-device test checklist for post-flash verification"
```

---

## Self-Review Notes

- **Spec coverage:** A. button fixes → Task 1. B. rotation bounds/wall-kick → Tasks 3, 11. C. `GameState` architecture → Task 13. D. ghost piece → Tasks 4, 11; 7-bag → Tasks 5, 10; scoring/leveling → Tasks 6, 9; start/game-over screens → Task 13. E. sound → `playRotate` (Task 11), `playLineClear`/`playTetris`/`playLevelUp` (Task 9), `playGameOver` (Task 13), existing `playClick`/`playDrop` untouched. F. verification → per-task manual trace steps plus the consolidated on-device checklist (Task 14). All spec sections are covered.
- **Placeholder scan:** no `TBD`/`TODO`/"add appropriate" language anywhere in the tasks; every step has complete, concrete code.
- **Type consistency:** `tryRotate(const Block&, Point, int, int*, Point*)` and `ghostPosition(const Block&, Point, int)` signatures are identical everywhere they're defined (Tasks 3, 4) and called (Tasks 11, 12, 13). `clearLinesLogic()` returns `int` consistently in Tasks 7, 9, 13. `Bag`/`refillBag`/`nextFromBag` signatures match between Task 5's definition and Task 10's call sites.
