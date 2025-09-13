//需安裝以下package
//Baud rate = 115200, Can justify the ssensitive of encoder and Dir of encoder

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

long oldPosition  = 0;   // 會在進入某個畫面時初始化為當前編碼器位置
long rawOldPosition = 0;

int value = 0;
int confirmIndex = 0;     // 0=YES, 1=NO

// 狀態機
int STATE = 0;            // 0=選擇模式, 1=連續傳送模式 （仍保留，但你選 confirm 要單次傳送會用另一段邏輯）
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

// ---------- 可調整的全域反向（如果要反向把它改成 true） ----------
bool invertEncoder = true; // false = 正常，true = 反向

// Sent 畫面顯示時間 (毫秒)
const unsigned long SENT_DISPLAY_MS = 800;

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

  // 初始化編碼器參考位置（避免第一次跳動）
  rawOldPosition = myEnc.read();
  oldPosition = rawOldPosition / getSensitivity();
}

void loop() {
  // 使用 raw count，再除靈敏度得到"步進"位置
  long rawNew = myEnc.read();
  long newPosition = rawNew / getSensitivity();

  if (STATE == 0) {
    // 選擇模式
    if (!inConfirm) {
      if (newPosition != oldPosition) {
        long delta = newPosition - oldPosition;
        // 考慮反向設定
        if (invertEncoder) delta = -delta;

        value += (int)delta;
        // 繞回處理
        while (value > MAX_VALUE) value -= (MAX_VALUE + 1);
        while (value < 0) value += (MAX_VALUE + 1);

        oldPosition = newPosition;
        showValue(value);
      }
    } else {
      // 在 confirm 畫面，用 delta 控制 YES/NO 切換
      if (newPosition != oldPosition) {
        long delta = newPosition - oldPosition;
        if (invertEncoder) delta = -delta;
        // confirmIndex 只有 0 或 1
        confirmIndex = (confirmIndex + (int)delta) % 2;
        if (confirmIndex < 0) confirmIndex += 2;
        oldPosition = newPosition;
        showConfirm(confirmIndex);
      }
    }
  } 
  else if (STATE == 1) {
    // 連續傳送模式（保留給你以後要連續傳送時使用）
    if (!inReselect) {
      showTransmitting(value);     // 更新畫面
      Serial.println(value);       // 連續傳送整數
      delay(50);                   // 控制傳送速率，可自行調整
    } else {
      if (newPosition != oldPosition) {
        long delta = newPosition - oldPosition;
        if (invertEncoder) delta = -delta;
        confirmIndex = (confirmIndex + (int)delta) % 2;
        if (confirmIndex < 0) confirmIndex += 2;
        oldPosition = newPosition;
        showReselect(confirmIndex);
      }
    }
  }

  // 按鈕處理（按下 SW）
  if (digitalRead(SW) == LOW) {
    delay(200); // 去抖
    // 按下時讀當前位置，避免一按下就產生多餘 delta
    rawOldPosition = myEnc.read();
    oldPosition = rawOldPosition / getSensitivity();

    if (STATE == 0) {
      if (!inConfirm) {
        // 進入確認畫面：預設為 YES（confirmIndex = 0）
        inConfirm = true;
        confirmIndex = 0; // 預設 YES
        // 重新設定 oldPosition 為當前，避免跳動
        rawOldPosition = myEnc.read();
        oldPosition = rawOldPosition / getSensitivity();
        showConfirm(confirmIndex);
      } else {
        // 在確認畫面按下（選 YES 或 NO）
        if (confirmIndex == 0) {
          // YES：**只傳一次**，然後顯示 Sent! 畫面，再回選擇畫面且從 0 開始
          Serial.println(value); // 傳一次
          showSent();            // 顯示 Sent! 畫面
          delay(SENT_DISPLAY_MS);
          // reset
          value = 0;
          inConfirm = false;
          // 回到選擇畫面並顯示 0
          rawOldPosition = myEnc.read();
          oldPosition = rawOldPosition / getSensitivity();
          showValue(value);
        } else {
          // NO：取消確認，回到選擇畫面（保持目前 value）
          inConfirm = false;
          rawOldPosition = myEnc.read();
          oldPosition = rawOldPosition / getSensitivity();
          showValue(value);
        }
      }
    } else if (STATE == 1) {
      // 連續傳送模式的重新選擇（保留你原本邏輯）
      if (!inReselect) {
        inReselect = true;
        confirmIndex = 0; // 預設 YES (要取消)
        rawOldPosition = myEnc.read();
        oldPosition = rawOldPosition / getSensitivity();
        showReselect(confirmIndex);
      } else {
        if (confirmIndex == 0) {
          // YES → 取消傳送，回選擇模式
          inReselect = false;
          STATE = 0;
          rawOldPosition = myEnc.read();
          oldPosition = rawOldPosition / getSensitivity();
          showValue(value);
        } else {
          // NO → 繼續傳送
          inReselect = false;
        }
      }
    }
    // 等到手放開按鈕（避免多次觸發）
    while (digitalRead(SW) == LOW) delay(10);
    delay(50);
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

// ---------- Sent 畫面 ----------
void showSent() {
  display.clearDisplay();
  display.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  char t[] = "Sent!";
  display.getTextBounds(t, 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  int y = (SCREEN_HEIGHT - h) / 2;
  display.setCursor(x, y);
  display.println(t);
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
