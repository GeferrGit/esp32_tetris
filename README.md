# ESP32 Tetris (ST7789 + матриця кнопок + спікер)

Класичний Tetris на ESP32: дисплей ST7789 (SPI), матриця кнопок 3x3 + Reset, п'єзо-спікер.

## Структура проєкту

- `Tetris_ESP32_ST7789_FULL.ino` — прошивка: дисплей, кнопки, звук, машина станів гри (`setup()`/`loop()`).
- `game_logic.h` — уся апаратно-незалежна логіка гри (колізії, обертання з wall-kick, ghost piece, 7-bag генератор фігур, рахунок/рівні, очищення ліній). Без залежностей від Arduino — компілюється і тестується нативно.
- `test/native/test_game_logic.cpp` — нативні тести для `game_logic.h` (17 тестів, `<cassert>`, без зовнішніх фреймворків).
- `docs/superpowers/specs/` — дизайн-документ.
- `docs/superpowers/plans/` — план імплементації (14 завдань).

## Корисні команди

### Нативні тести логіки (без плати, без Arduino toolchain)

Компілює і запускає тести `game_logic.h` прямо на цій машині (`g++`/`clang++`):

```bash
g++ -std=c++17 -Wall -Wextra -o /tmp/tetris_test_game_logic test/native/test_game_logic.cpp && /tmp/tetris_test_game_logic
```

Очікуваний вивід: `All game_logic tests passed.` (без попереджень).

### Прошивка плати (Arduino IDE)

1. Встановити бібліотеки через Library Manager: `Adafruit GFX Library`, `Adafruit ST7735 and ST7789 Library`.
2. Board: ESP32 Dev Module (Tools → Board).
3. Відкрити `Tetris_ESP32_ST7789_FULL.ino`, обрати правильний порт (Tools → Port).
4. Upload.

### Прошивка плати (arduino-cli, якщо встановлено)

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p <PORT> --fqbn esp32:esp32:esp32 .
```

### Git

Проєкт має гілку розробки `tetris-buttons-and-features`, замерджену чи ще не замерджену в `main`:

```bash
git log --oneline main..tetris-buttons-and-features   # переглянути коміти гілки
git diff main...tetris-buttons-and-features            # повний діф проти main
```

## Розпіновка

| Призначення | Піни |
|---|---|
| TFT CS / DC / RST | 5 / 16 / 17 |
| Матриця кнопок: рядки (OUTPUT) | 32, 26, 27 |
| Матриця кнопок: стовпці (INPUT_PULLUP) | 14, 25, 33 |
| Reset: рядок / стовпець | 26 (спільний з матрицею) / 13 |
| Спікер | 4 |

### Мапа кнопок (row, col)

| Дія | (row, col) |
|---|---|
| Left | (27, 14) |
| Right | (26, 14) |
| Down | (32, 14) |
| Rotate | (26, 25) |
| Hard Drop | (27, 33) |
| Pause | (32, 33) |
| Reset | (26, 13) |

## Чек-лист після прошивки

Той самий чек-лист продубльовано коментарем на початку `Tetris_ESP32_ST7789_FULL.ino`:

1. Left/Right/Down/Rotate/HardDrop/Pause/Reset — кожна кнопка виконує свою дію.
2. Обертання біля стін/дна не виходить за межі поля і не "зависає".
3. Ghost piece показує правильну позицію приземлення.
4. За 7-14 фігур поспіль немає підозріло довгих серій однакової фігури.
5. Рахунок: 1 лінія = +100, 2 = +300, 3 = +500, 4 = +800.
6. Рівень підвищується кожні 10 очищених ліній; швидкість падіння зростає.
7. Стартовий екран показується при подачі живлення; гра стартує після першої кнопки.
8. Game Over показує фінальний рахунок і не зависає; Reset одразу починає нову гру.
9. Усі звукові події чутні і відрізняються одна від одної.
