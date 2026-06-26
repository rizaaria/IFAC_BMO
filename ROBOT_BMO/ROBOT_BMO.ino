/*
 * ============================================================
 *  BMO FACE - Cheap Yellow Display (ESP32-2432S028R)
 * ============================================================
 *  Muka BMO (Adventure Time) persis seperti aslinya!
 *  - Background hijau pastel muda
 *  - Mata hitam bulat dengan refleksi putih
 *  - Mulut hijau tua dengan gigi putih
 *  - Pipi pink (blush)
 *  - 7 ekspresi, touch untuk ganti
 *
 *  Hardware: ESP32-2432S028R (CYD) - 320x240 ILI9341
 *  Library:  TFT_eSPI, XPT2046_Touchscreen
 * ============================================================
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ─── Backlight Pin ───
#define TFT_BL_PIN 21

// ─── Touch Pin Configuration for CYD ───
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

bool useSprite = false;

#define SCREEN_W 320
#define SCREEN_H 240

// ═══════════════════════════════════════════
//  WARNA BMO (RGB565) - disesuaikan dari referensi
// ═══════════════════════════════════════════
//  Background: hijau pastel muda (#C4E6C3)
#define BMO_BG          0xC738
//  Mata: hitam solid
#define BMO_BLACK       0x0000
//  Refleksi mata & gigi: putih
#define BMO_WHITE       0xFFFF
//  Mulut dalam: hijau tua/olive (#4B6E42)
#define BMO_MOUTH_CLR   0x4B48
//  Outline mulut: hitam
//  Pipi blush: pink pastel (#FFB8C8)
#define BMO_BLUSH       0xFDD7
//  Hati: merah
#define BMO_HEART       0xF800
//  Air mata: biru-teal
#define BMO_TEAR        0x065F
//  Aksen: kuning
#define BMO_ACCENT      0xFFE0

// ─── Posisi Muka (320x240 landscape) ───
#define EYE_LEFT_X    115
#define EYE_RIGHT_X   205
#define EYE_Y         85
#define MOUTH_X       160
#define MOUTH_Y       158

// ─── Expression Types ───
enum Expression {
  EXPR_HAPPY,
  EXPR_SAD,
  EXPR_ANGRY,
  EXPR_SURPRISED,
  EXPR_LOVE,
  EXPR_NEUTRAL,
  EXPR_SLEEPY,
  EXPR_COUNT
};

// ─── Animation State ───
struct BMOFace {
  Expression currentExpr = EXPR_HAPPY;
  float eyeOpenLeft = 1.0;
  float eyeOpenRight = 1.0;
  bool isBlinking = false;
  unsigned long lastBlinkTime = 0;
  unsigned long blinkInterval = 3000;
  unsigned long lastExprChange = 0;
  int blinkPhase = 0;
  unsigned long blinkPhaseTime = 0;
} bmo;

unsigned long lastFrameTime = 0;
const int FRAME_DELAY = 33;  // ~30 FPS

// ═══════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.println("BMO Face Starting...");

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BMO_BG);

  sprite.setColorDepth(8);
  void* spritePtr = sprite.createSprite(SCREEN_W, SCREEN_H);
  if (spritePtr != nullptr) {
    useSprite = true;
    Serial.println("Sprite OK!");
  } else {
    useSprite = false;
    Serial.println("Sprite gagal, direct draw.");
  }

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  startupAnimation();

  bmo.lastBlinkTime = millis();
  bmo.lastExprChange = millis();

  Serial.println("BMO Ready! Touch to change expression.");
}

// ═══════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════
void loop() {
  unsigned long now = millis();
  if (now - lastFrameTime < FRAME_DELAY) return;
  lastFrameTime = now;

  handleTouch();
  updateBlink(now);
  drawFrame();
}

// ═══════════════════════════════════════════
//  STARTUP ANIMATION
// ═══════════════════════════════════════════
void startupAnimation() {
  tft.fillScreen(BMO_BG);
  // Mata muncul perlahan
  for (int r = 0; r < 12; r++) {
    tft.fillCircle(EYE_LEFT_X, EYE_Y, r, BMO_BLACK);
    tft.fillCircle(EYE_RIGHT_X, EYE_Y, r, BMO_BLACK);
    delay(30);
  }
  delay(400);
  tft.fillScreen(BMO_BG);
  delay(200);
}

// ═══════════════════════════════════════════
//  TOUCH
// ═══════════════════════════════════════════
void handleTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    static unsigned long lastTouch = 0;
    if (millis() - lastTouch > 500) {
      lastTouch = millis();
      bmo.currentExpr = (Expression)((bmo.currentExpr + 1) % EXPR_COUNT);
      bmo.lastExprChange = millis();
      Serial.print("BMO: ");
      Serial.println(bmo.currentExpr);
    }
  }
}

// ═══════════════════════════════════════════
//  BLINK
// ═══════════════════════════════════════════
void updateBlink(unsigned long now) {
  if (!bmo.isBlinking && (now - bmo.lastBlinkTime > bmo.blinkInterval)) {
    bmo.isBlinking = true;
    bmo.blinkPhase = 1;
    bmo.blinkPhaseTime = now;
    bmo.blinkInterval = random(2500, 5000);
  }

  if (bmo.isBlinking) {
    unsigned long elapsed = now - bmo.blinkPhaseTime;
    switch (bmo.blinkPhase) {
      case 1:  // Menutup
        bmo.eyeOpenLeft = 1.0 - (elapsed / 70.0);
        bmo.eyeOpenRight = 1.0 - (elapsed / 70.0);
        if (bmo.eyeOpenLeft <= 0.05) {
          bmo.eyeOpenLeft = 0.05;
          bmo.eyeOpenRight = 0.05;
          bmo.blinkPhase = 2;
          bmo.blinkPhaseTime = now;
        }
        break;
      case 2:  // Tertutup
        if (elapsed > 50) {
          bmo.blinkPhase = 3;
          bmo.blinkPhaseTime = now;
        }
        break;
      case 3:  // Membuka
        bmo.eyeOpenLeft = 0.05 + (elapsed / 70.0);
        bmo.eyeOpenRight = 0.05 + (elapsed / 70.0);
        if (bmo.eyeOpenLeft >= 1.0) {
          bmo.eyeOpenLeft = 1.0;
          bmo.eyeOpenRight = 1.0;
          bmo.isBlinking = false;
          bmo.lastBlinkTime = now;
        }
        break;
    }
  }
}

// ═══════════════════════════════════════════
//  MAIN DRAW
// ═══════════════════════════════════════════
void drawFrame() {
  if (useSprite) {
    sprite.fillSprite(BMO_BG);
    drawAllElements(&sprite);
    sprite.pushSprite(0, 0);
  } else {
    tft.fillScreen(BMO_BG);
    drawAllElements(&tft);
  }
}

void drawAllElements(TFT_eSPI* gfx) {
  drawEyes(gfx);
  drawMouth(gfx);
  drawExpressionExtras(gfx);
  //drawExpressionLabel(gfx);
}

// ═══════════════════════════════════════════
//  HELPER: Gambar mata dot hitam (dengan blink)
//  Untuk happy, angry, neutral
// ═══════════════════════════════════════════
void drawDotEyes(TFT_eSPI* gfx, int radius) {
  // Blink: squish vertical
  int hL = (int)(radius * 2 * bmo.eyeOpenLeft);
  if (hL < 2) hL = 2;
  int hR = (int)(radius * 2 * bmo.eyeOpenRight);
  if (hR < 2) hR = 2;
  gfx->fillRoundRect(EYE_LEFT_X - radius, EYE_Y - hL / 2, radius * 2, hL, radius, BMO_BLACK);
  gfx->fillRoundRect(EYE_RIGHT_X - radius, EYE_Y - hR / 2, radius * 2, hR, radius, BMO_BLACK);
}

// ═══════════════════════════════════════════
//  HELPER: Gambar mata besar (orb) dengan refleksi
//  Untuk surprised, love
// ═══════════════════════════════════════════
void drawOrbEyes(TFT_eSPI* gfx, int radius) {
  if (bmo.eyeOpenLeft < 0.3) {
    // Saat blink, jadi garis hitam
    gfx->fillRect(EYE_LEFT_X - radius, EYE_Y - 1, radius * 2, 3, BMO_BLACK);
    gfx->fillRect(EYE_RIGHT_X - radius, EYE_Y - 1, radius * 2, 3, BMO_BLACK);
    return;
  }

  int r = radius;
  int reflR = (int)(r * 0.7);  // Refleksi putih besar

  // Blink squish
  int h = (int)(r * bmo.eyeOpenLeft);
  if (h < r / 2) h = r / 2;

  // ── Mata kiri ──
  // Hitam besar
  gfx->fillEllipse(EYE_LEFT_X, EYE_Y, r, h, BMO_BLACK);
  // Refleksi putih besar (geser ke kiri-atas)
  gfx->fillCircle(EYE_LEFT_X - 4, EYE_Y - (int)(h * 0.2), reflR, BMO_WHITE);
  // Titik highlight kecil (kiri-bawah)
  gfx->fillCircle(EYE_LEFT_X + (int)(r * 0.2), EYE_Y + (int)(h * 0.35), 4, BMO_WHITE);

  // ── Mata kanan ──
  gfx->fillEllipse(EYE_RIGHT_X, EYE_Y, r, h, BMO_BLACK);
  gfx->fillCircle(EYE_RIGHT_X - 4, EYE_Y - (int)(h * 0.2), reflR, BMO_WHITE);
  gfx->fillCircle(EYE_RIGHT_X + (int)(r * 0.2), EYE_Y + (int)(h * 0.35), 4, BMO_WHITE);
}

// ═══════════════════════════════════════════
//  DRAW EYES
// ═══════════════════════════════════════════
void drawEyes(TFT_eSPI* gfx) {
  switch (bmo.currentExpr) {
    case EXPR_HAPPY:     drawHappyEyes(gfx);     break;
    case EXPR_SAD:       drawSadEyes(gfx);       break;
    case EXPR_ANGRY:     drawAngryEyes(gfx);     break;
    case EXPR_SURPRISED: drawSurprisedEyes(gfx); break;
    case EXPR_LOVE:      drawLoveEyes(gfx);      break;
    case EXPR_NEUTRAL:   drawNeutralEyes(gfx);   break;
    case EXPR_SLEEPY:    drawSleepyEyes(gfx);    break;
    default:             drawNeutralEyes(gfx);   break;
  }
}

// ── HAPPY: titik hitam kecil (seperti referensi gambar 1) ──
void drawHappyEyes(TFT_eSPI* gfx) {
  drawDotEyes(gfx, 10);
}

// ── SAD: oval vertikal hitam + refleksi + alis lengkung + air mata ──
void drawSadEyes(TFT_eSPI* gfx) {
  int rx = 13;  // half-width
  int ry = 20;  // half-height

  if (bmo.eyeOpenLeft < 0.3) {
    gfx->fillRect(EYE_LEFT_X - rx, EYE_Y - 1, rx * 2, 3, BMO_BLACK);
    gfx->fillRect(EYE_RIGHT_X - rx, EYE_Y - 1, rx * 2, 3, BMO_BLACK);
    return;
  }

  int h = (int)(ry * bmo.eyeOpenLeft);
  if (h < 5) h = 5;

  // Oval hitam vertikal
  gfx->fillEllipse(EYE_LEFT_X, EYE_Y, rx, h, BMO_BLACK);
  gfx->fillEllipse(EYE_RIGHT_X, EYE_Y, rx, h, BMO_BLACK);

  // Refleksi putih kecil
  gfx->fillCircle(EYE_LEFT_X - 3, EYE_Y - (int)(h * 0.3), 5, BMO_WHITE);
  gfx->fillCircle(EYE_RIGHT_X - 3, EYE_Y - (int)(h * 0.3), 5, BMO_WHITE);

  // Tetesan hijau-teal di bawah mata
  int tearR = 5;
  float tearAnim = (millis() / 15) % 40;
  gfx->fillCircle(EYE_LEFT_X + 5, EYE_Y + h + 3 + (int)tearAnim, tearR, BMO_TEAR);
  gfx->fillCircle(EYE_RIGHT_X + 5, EYE_Y + h + 3 + (int)(tearAnim * 0.7), tearR, BMO_TEAR);

  // Alis lengkung sedih (melengkung turun ke tengah)
  for (int i = -22; i <= 22; i++) {
    float t = (float)i / 22.0;
    int lx = EYE_LEFT_X + i;
    int ly = EYE_Y - 30 - (int)((1.0 - t * t) * 8);  // lengkung ke atas
    gfx->fillCircle(lx, ly, 1, BMO_BLACK);

    int rx2 = EYE_RIGHT_X + i;
    int ry2 = EYE_Y - 30 - (int)((1.0 - t * t) * 8);
    gfx->fillCircle(rx2, ry2, 1, BMO_BLACK);
  }
}

// ── ANGRY: dot hitam + alis V tebal + garis bawah mata ──
void drawAngryEyes(TFT_eSPI* gfx) {
  drawDotEyes(gfx, 12);

  if (bmo.eyeOpenLeft < 0.3) return;

  // Alis garis miring marah: luar=atas, dalam=bawah
  // Kiri: dari luar-atas ke dalam-bawah
  for (int t = 0; t < 4; t++) {
    gfx->drawLine(EYE_LEFT_X - 28, EYE_Y - 32 - t,
                   EYE_LEFT_X + 5,  EYE_Y - 20 - t, BMO_BLACK);
  }
  // Kanan: mirror (luar-atas ke dalam-bawah)
  for (int t = 0; t < 4; t++) {
    gfx->drawLine(EYE_RIGHT_X + 28, EYE_Y - 32 - t,
                   EYE_RIGHT_X - 5,  EYE_Y - 20 - t, BMO_BLACK);
  }
}

// ── SURPRISED: mata besar orb + pipi pink ──
void drawSurprisedEyes(TFT_eSPI* gfx) {
  drawOrbEyes(gfx, 30);

  // Pipi blush pink (oval horizontal) di samping kiri-kanan
  if (bmo.eyeOpenLeft > 0.3) {
    gfx->fillEllipse(EYE_LEFT_X - 50, EYE_Y + 30, 18, 10, BMO_BLUSH);
    gfx->fillEllipse(EYE_RIGHT_X + 50, EYE_Y + 30, 18, 10, BMO_BLUSH);
  }
}

// ── LOVE: mata besar orb + pipi pink ──
void drawLoveEyes(TFT_eSPI* gfx) {
  if (bmo.eyeOpenLeft > 0.3) {
    // Pulsing hearts
    float pulse = sin(millis() / 200.0) * 3;
    int sz = 22 + (int)max(0.0f, (float)pulse);
    drawHeart(gfx, EYE_LEFT_X, EYE_Y, sz, BMO_HEART);
    drawHeart(gfx, EYE_RIGHT_X, EYE_Y, sz, BMO_HEART);

    // Pipi blush
    gfx->fillEllipse(EYE_LEFT_X - 45, EYE_Y + 30, 16, 9, BMO_BLUSH);
    gfx->fillEllipse(EYE_RIGHT_X + 45, EYE_Y + 30, 16, 9, BMO_BLUSH);
  } else {
    gfx->fillRect(EYE_LEFT_X - 18, EYE_Y - 1, 36, 3, BMO_HEART);
    gfx->fillRect(EYE_RIGHT_X - 18, EYE_Y - 1, 36, 3, BMO_HEART);
  }
}

// ── NEUTRAL: dot hitam kecil ──
void drawNeutralEyes(TFT_eSPI* gfx) {
  drawDotEyes(gfx, 10);
}

// ── SLEEPY: mata setengah tutup (garis horizontal) + Zzz ──
void drawSleepyEyes(TFT_eSPI* gfx) {
  float sleepyOpen = min(bmo.eyeOpenLeft, 0.25f);
  int r = 10;
  int h = (int)(r * 2 * sleepyOpen);
  if (h < 2) h = 2;
  gfx->fillRoundRect(EYE_LEFT_X - r, EYE_Y - h / 2, r * 2, h, r, BMO_BLACK);
  gfx->fillRoundRect(EYE_RIGHT_X - r, EYE_Y - h / 2, r * 2, h, r, BMO_BLACK);

  // Zzz
  float zOff = sin(millis() / 500.0) * 4;
  gfx->setTextColor(BMO_BLACK, BMO_BG);
  gfx->setTextSize(2);
  gfx->drawString("Z", 252 + (int)zOff, 48 - (int)abs(zOff));
  gfx->setTextSize(1);
  gfx->drawString("z", 267, 66);
  gfx->drawString("z", 272 + (int)zOff, 78);
}

// ═══════════════════════════════════════════
//  MOUTH
// ═══════════════════════════════════════════
void drawMouth(TFT_eSPI* gfx) {
  switch (bmo.currentExpr) {
    case EXPR_HAPPY:     drawHappyMouth(gfx);     break;
    case EXPR_SAD:       drawSadMouth(gfx);       break;
    case EXPR_ANGRY:     drawAngryMouth(gfx);     break;
    case EXPR_SURPRISED: drawSurprisedMouth(gfx); break;
    case EXPR_LOVE:      drawLoveMouth(gfx);      break;
    case EXPR_NEUTRAL:   drawNeutralMouth(gfx);   break;
    case EXPR_SLEEPY:    drawSleepyMouth(gfx);    break;
  }
}

// ── HAPPY MOUTH: senyum melengkung ke atas (crescent smile) ──
void drawHappyMouth(TFT_eSPI* gfx) {
  int mx = MOUTH_X;
  int my = MOUTH_Y;
  int hw = 34;  // half width

  // Bentuk crescent: ujung kiri-kanan tertutup, tengah terbuka ke bawah
  for (int x = -hw; x <= hw; x++) {
    float t = (float)x / hw;  // -1 ke 1
    float curve = 1.0 - t * t;  // 1 di tengah, 0 di ujung

    int topY = my + (int)(curve * 2);   // garis atas hampir lurus
    int botY = my + (int)(curve * 16);  // lengkung bawah lebih dalam

    if (botY <= topY + 1) continue;

    int h = botY - topY;
    // Gigi putih di bagian atas
    int teethH = min(5, h / 3);
    if (teethH > 0)
      gfx->drawFastVLine(mx + x, topY, teethH, BMO_WHITE);
    // Hijau tua di bawah
    if (h - teethH > 0)
      gfx->drawFastVLine(mx + x, topY + teethH, h - teethH, BMO_MOUTH_CLR);
  }

  // Outline hitam atas (garis senyum)
  for (int x = -hw; x <= hw; x++) {
    float t = (float)x / hw;
    float curve = 1.0 - t * t;
    int topY = my + (int)(curve * 2);
    gfx->fillCircle(mx + x, topY - 1, 1, BMO_BLACK);
  }
  // Outline hitam bawah (lengkungan)
  for (int x = -hw; x <= hw; x++) {
    float t = (float)x / hw;
    float curve = 1.0 - t * t;
    int botY = my + (int)(curve * 16);
    gfx->fillCircle(mx + x, botY, 1, BMO_BLACK);
  }
}

// ── SAD MOUTH: kecil, hijau tua, agak terbuka ──
void drawSadMouth(TFT_eSPI* gfx) {
  int mx = MOUTH_X;
  int my = MOUTH_Y + 5;
  // Mulut kecil terbuka - hijau tua dengan sedikit lighter area
  gfx->fillRoundRect(mx - 18, my, 36, 16, 8, BMO_MOUTH_CLR);
  // Area lebih terang di atas (lidah/gusi)
  gfx->fillRoundRect(mx - 14, my + 2, 28, 5, 3, tft.color565(90, 130, 80));
  // Outline
  gfx->drawRoundRect(mx - 18, my, 36, 16, 8, BMO_BLACK);
  gfx->drawRoundRect(mx - 19, my - 1, 38, 18, 9, BMO_BLACK);
}

// ── ANGRY MOUTH: garis lurus ──
void drawAngryMouth(TFT_eSPI* gfx) {
  int mx = MOUTH_X;
  int my = MOUTH_Y + 5;
  // Garis lurus hitam
  gfx->fillRect(mx - 22, my, 44, 3, BMO_BLACK);
}

// ── SURPRISED MOUTH: bulat kecil, atas putih bawah hijau tua ──
void drawSurprisedMouth(TFT_eSPI* gfx) {
  int mx = MOUTH_X;
  int my = MOUTH_Y + 3;
  int r = 13;

  // Gambar lingkaran: atas putih, bawah hijau tua
  for (int y = -r; y <= r; y++) {
    int halfW = (int)sqrt((float)(r * r - y * y));
    if (halfW < 1) continue;
    uint16_t color = (y < -1) ? BMO_WHITE : BMO_MOUTH_CLR;
    gfx->drawFastHLine(mx - halfW, my + y, halfW * 2, color);
  }
  // Outline hitam
  gfx->drawCircle(mx, my, r, BMO_BLACK);
  gfx->drawCircle(mx, my, r + 1, BMO_BLACK);
}

// ── LOVE MOUTH: senyum kecil manis ──
void drawLoveMouth(TFT_eSPI* gfx) {
  int mx = MOUTH_X;
  int my = MOUTH_Y;
  int hw = 22;
  int mh = 14;
  gfx->fillRoundRect(mx - hw, my, hw * 2, mh, 7, BMO_MOUTH_CLR);
  gfx->fillRoundRect(mx - hw + 2, my + 1, hw * 2 - 4, 5, 3, BMO_WHITE);
  gfx->drawRoundRect(mx - hw, my, hw * 2, mh, 7, BMO_BLACK);
  gfx->drawRoundRect(mx - hw - 1, my - 1, hw * 2 + 2, mh + 2, 8, BMO_BLACK);
}

// ── NEUTRAL MOUTH: garis lurus hijau tua ──
void drawNeutralMouth(TFT_eSPI* gfx) {
  gfx->fillRoundRect(MOUTH_X - 20, MOUTH_Y + 2, 40, 4, 2, BMO_MOUTH_CLR);
  gfx->drawRoundRect(MOUTH_X - 20, MOUTH_Y + 2, 40, 4, 2, BMO_BLACK);
}

// ── SLEEPY MOUTH: mulut kecil menguap ──
void drawSleepyMouth(TFT_eSPI* gfx) {
  float yawnPulse = sin(millis() / 800.0);
  int yawnH = 8 + (int)(yawnPulse * 5);
  int mx = MOUTH_X;
  int my = MOUTH_Y + 5;
  gfx->fillRoundRect(mx - 10, my, 20, yawnH, 6, BMO_MOUTH_CLR);
  gfx->drawRoundRect(mx - 10, my, 20, yawnH, 6, BMO_BLACK);
  gfx->drawRoundRect(mx - 11, my - 1, 22, yawnH + 2, 7, BMO_BLACK);
}

// ═══════════════════════════════════════════
//  EXPRESSION EXTRAS
// ═══════════════════════════════════════════
void drawExpressionExtras(TFT_eSPI* gfx) {
  switch (bmo.currentExpr) {
    case EXPR_HAPPY: {
      // Sparkle effects
      float sparkle = sin(millis() / 300.0);
      if (sparkle > 0.5) {
        drawSparkle(gfx, 55, 45, 5, BMO_ACCENT);
        drawSparkle(gfx, 270, 42, 4, BMO_WHITE);
      }
      break;
    }
    case EXPR_LOVE: {
      // Floating hearts
      float hf = (millis() / 25) % 50;
      drawHeart(gfx, 50, 45 + (int)hf / 4, 6, BMO_HEART);
      drawHeart(gfx, 275, 50 - (int)hf / 5, 5, BMO_HEART);
      break;
    }
    case EXPR_ANGRY: {
      // Anger marks
      int mx = 260, my = 40;
      for (int t = 0; t < 2; t++) {
        gfx->drawLine(mx + t, my, mx + 8 + t, my + 8, BMO_BLACK);
        gfx->drawLine(mx + 8 + t, my, mx + t, my + 8, BMO_BLACK);
      }
      break;
    }
    default: break;
  }
}

// ═══════════════════════════════════════════
//  EXPRESSION LABEL
// ═══════════════════════════════════════════
void drawExpressionLabel(TFT_eSPI* gfx) {
  gfx->setTextSize(1);
  gfx->setTextDatum(BC_DATUM);

  const char* label;
  uint16_t labelColor;

  switch (bmo.currentExpr) {
    case EXPR_HAPPY:     label = "HAPPY";     labelColor = 0x3666;     break;
    case EXPR_SAD:       label = "SAD";       labelColor = BMO_TEAR;   break;
    case EXPR_ANGRY:     label = "ANGRY";     labelColor = BMO_HEART;  break;
    case EXPR_SURPRISED: label = "WOW";       labelColor = BMO_ACCENT; break;
    case EXPR_LOVE:      label = "LOVE";      labelColor = BMO_HEART;  break;
    case EXPR_NEUTRAL:   label = "BMO";       labelColor = BMO_BLACK;  break;
    case EXPR_SLEEPY:    label = "ZZZ";       labelColor = BMO_BLACK;  break;
    default:             label = "BMO";       labelColor = BMO_BLACK;  break;
  }

  gfx->setTextColor(labelColor, BMO_BG);
  gfx->drawString(label, SCREEN_W / 2, SCREEN_H - 8);
}

// ═══════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════
void drawHeart(TFT_eSPI* gfx, int cx, int cy, int size, uint16_t color) {
  int r = size / 2;
  gfx->fillCircle(cx - r + 1, cy - r / 2, r, color);
  gfx->fillCircle(cx + r - 1, cy - r / 2, r, color);
  gfx->fillTriangle(cx - size, cy - 2, cx + size, cy - 2, cx, cy + size, color);
}

void drawSparkle(TFT_eSPI* gfx, int cx, int cy, int size, uint16_t color) {
  gfx->drawFastHLine(cx - size, cy, size * 2 + 1, color);
  gfx->drawFastVLine(cx, cy - size, size * 2 + 1, color);
  gfx->drawLine(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, color);
  gfx->drawLine(cx + size / 2, cy - size / 2, cx - size / 2, cy + size / 2, color);
}
