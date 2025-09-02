#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>

// OLED 參數
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 編碼器接腳
#define CLK 2
#define DT 3
#define SW 4
Encoder myEnc(CLK, DT);

long oldPosition  = -999;
int value = 0;
int confirmIndex = 0;     // 0=YES, 1=NO
int rotationDir = 1;      // 1=順時針, 0=逆時針

// 狀態機
int STATE = 0;            // 0=選擇模式, 1=傳送模式
bool inConfirm = false;   // 是否在確認畫面
bool inReselect = false;  // 是否在「重新選擇？」畫面

// 數字上限
int MAX_VALUE = 20;

// ---------- 靈敏度模式 ----------
int sensitivityMode = 2;  // 預設模式 (0–4)
int sensitivityTable[5] = {1, 2, 4, 6, 8};

int getSensitivity() {
  return sensitivityTable[sensitivityMode];
}

void setup() {
  pinMode(SW, INPUT_PULLUP);

  // 初始化 USB Serial
  Serial.begin(115200);

  // 初始化 OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;); // 如果初始化失敗，卡死
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Ready!");
  display.display();
  delay(1000);
}

void loop() {
  long newPosition = myEnc.read() / getSensitivity();

  if (STATE == 0) {
    // 選擇模式
    if (!inConfirm) {
      if (newPosition != oldPosition) {
        if (newPosition > oldPosition) rotationDir = 1;
        else rotationDir = 0;

        oldPosition = newPosition;

        if (rotationDir == 1) value++;
        else value--;

        if (value > MAX_VALUE) value = 0;
        if (value < 0) value = MAX_VALUE;

        showValue(value);
      }
    } else {
      if (newPosition != oldPosition) {
        if (newPosition > oldPosition) rotationDir = 1;
        else rotationDir = 0;

        oldPosition = newPosition;
        confirmIndex = (newPosition % 2 + 2) % 2;
        showConfirm(confirmIndex);
      }
    }
  } 
  else if (STATE == 1) {
    // 傳送模式
    if (!inReselect) {
      showTransmitting(value);     // 更新畫面
      Serial.println(value);       // 連續傳送整數
      delay(50);                   // 控制傳送速率，可自行調整
    } else {
      if (newPosition != oldPosition) {
        if (newPosition > oldPosition) rotationDir = 1;
        else rotationDir = 0;

        oldPosition = newPosition;
        confirmIndex = (newPosition % 2 + 2) % 2;
        showReselect(confirmIndex);
      }
    }
  }

  // 按鈕處理
  if (digitalRead(SW) == LOW) {
    delay(200); // 去抖
    if (STATE == 0) {
      if (!inConfirm) {
        inConfirm = true;
        oldPosition = -999;
        showConfirm(confirmIndex);
      } else {
        if (confirmIndex == 0) {
          STATE = 1; // 進入傳送模式
          inConfirm = false;
          oldPosition = -999;
        } else {
          inConfirm = false;
          oldPosition = -999;
          showValue(value);
        }
      }
    } else if (STATE == 1) {
      if (!inReselect) {
        inReselect = true;
        oldPosition = -999;
        showReselect(confirmIndex);
      } else {
        if (confirmIndex == 0) {
          // YES → 取消傳送，回選擇模式
          inReselect = false;
          STATE = 0;
          oldPosition = -999;
          showValue(value);
        } else {
          // NO → 繼續傳送
          inReselect = false;
        }
      }
    }
  }
}


// ---------- 顯示數字 ----------
void showValue(int v) {
  display.clearDisplay();

  char buf[4];
  sprintf(buf, "%02d", v);

  display.setTextSize(5);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;

  display.setCursor(x, y);
  display.print(buf);
  display.display();
}

// ---------- 確認畫面 ----------
void showConfirm(int index) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Confirm?");

  display.setCursor(0, 30);
  if (index == 0) display.print("> ");
  display.println("YES");

  display.setCursor(0, 50);
  if (index == 1) display.print("> ");
  display.println("NO");

  display.display();
}

// ---------- 傳送畫面 ----------
void showTransmitting(int v) {
  display.clearDisplay();

  // 第一行文字 (置中)
  display.setTextSize(2);
  char text[] = "Transmit";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 0);
  display.println(text);

  // 數字
  char buf[4];
  sprintf(buf, "%02d", v);

  display.setTextSize(4);
  display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2 + 10;

  display.setCursor(x, y);
  display.print(buf);

  display.display();

  // ---------- USB 傳送 ----------
  Serial.println(v); // 將數字送給電腦
}

// ---------- 重新選擇畫面 ----------
void showReselect(int index) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Cancel?");

  display.setCursor(0, 30);
  if (index == 0) display.print("> ");
  display.println("YES");

  display.setCursor(0, 50);
  if (index == 1) display.print("> ");
  display.println("NO");

  display.display();
}
