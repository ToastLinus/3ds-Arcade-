// Mini Arcade 2 - 8 weitere Spiele (oben + unten Bildschirm, mit Ton)
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

#define COL(r,g,b) (0xFF000000u|((u32)(b)<<16)|((u32)(g)<<8)|(u32)(r))
#define COLA(r,g,b,a) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(g)<<8)|(u32)(r))
#define WHITE COL(255,255,255)
#define GRAY  COL(170,175,195)
#define GOLD  COL(255,215,80)
#define SAVEFILE "sdmc:/3ds/arcade2.sav"

enum { G_TTT, G_C4, G_SIMON, G_MOLE, G_INV, G_DODGE, G_STACK, G_LIGHTS, NG };
static const char *NAME[NG] = {"Tic-Tac-Toe","4 Gewinnt","Simon","Maulwurf","Invaders","Ausweichen","Stack","Lights Out"};
static const char *DESC[NG] = {"Du gegen den Computer (Touch)","Vier in einer Reihe (Touch)","Merk dir die Farben (Touch)",
  "Hau die Maulwürfe (Touch)","Schieß die Aliens ab","Weich den Blöcken aus","Stapel den Turm so hoch es geht","Mach alle Lichter aus (Touch)"};
static const u32 ACC[NG] = {COL(110,200,255),COL(240,200,50),COL(80,200,100),COL(190,130,70),COL(120,240,160),COL(240,90,90),COL(240,150,60),COL(250,230,100)};

static C2D_TextBuf tbuf;
static int hs[NG], score, over, cur = -1, sel, tick, sv;
static u32 kd, kh;
static touchPosition tp;
static circlePosition cp;

// ---------- Helfer ----------
static int RND(int n) { return rand() % n; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static u32 sh(u32 c, int d) {
  int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255; r += d; g += d; b += d;
  r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return COL(r, g, b);
}
static void rect(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, 0, w, h, c); }
static void grad(float x, float y, float w, float h, u32 a, u32 b) { C2D_DrawRectangle(x, y, 0, w, h, a, a, b, b); }
static void circ(float x, float y, float r, u32 c) { C2D_DrawCircleSolid(x, y, 0, r, c); }
static void tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) { C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, 0); }
static void box(float x, float y, float w, float h, u32 c) { rect(x, y, w, h, sh(c, -70)); rect(x + 1, y + 1, w - 2, h - 2, c); rect(x + 1, y + 1, w - 2, 2, sh(c, 70)); }
static void T(float x, float y, float s, u32 c, int al, const char *f, ...) {
  char b[96]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a);
  C2D_Text t; C2D_TextParse(&t, tbuf, b); C2D_TextOptimize(&t);
  float w = 0, h = 0; C2D_TextGetDimensions(&t, s, s, &w, &h);
  if (al == 1) x -= w / 2; else if (al == 2) x -= w; else if (al == 3) { x -= w / 2; y -= h / 2; }
  C2D_DrawText(&t, C2D_WithColor, x, y, 0, s, s, c);
}
static void TT(float x, float y, float s, u32 c, const char *str) { T(x + 1, y + 1, s, COL(0, 0, 0), 3, "%s", str); T(x, y, s, c, 3, "%s", str); }
static void btn(float x, float y, float w, float h, u32 c, int selected) {
  if (selected) rect(x - 3, y - 3, w + 6, h + 6, WHITE); else rect(x - 1, y - 1, w + 2, h + 2, sh(c, -90));
  grad(x, y, w, h, sh(c, 45), sh(c, -35)); rect(x, y, w, 2, sh(c, 90));
}
static void overlay(const char *t) {
  rect(0, 0, 400, 240, COLA(0, 0, 0, 120)); rect(90, 70, 220, 100, COL(20, 22, 40)); rect(92, 72, 216, 96, COL(40, 44, 80));
  T(200, 80, 0.9, GOLD, 1, "%s", t); T(200, 116, 0.6, WHITE, 1, "Punkte: %d", score);
  T(200, 142, 0.42, GRAY, 1, "A = Nochmal   START = Menü");
}
static void infobot(const char *hint) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  T(160, 16, 0.9, ACC[cur], 1, "%s", NAME[cur]); T(160, 70, 0.65, WHITE, 1, "Punkte: %d", score);
  T(160, 100, 0.5, GOLD, 1, "Rekord: %d", hs[cur]); T(160, 160, 0.42, GRAY, 1, "%s", hint);
  T(160, 210, 0.4, COL(120, 125, 150), 1, "START = Menü");
}
static void gametop(const char *l1, const char *l2) {
  grad(0, 0, 400, 240, COL(30, 34, 70), COL(12, 14, 32));
  T(200, 20, 1.2, WHITE, 1, "%s", NAME[cur]); T(200, 96, 0.6, GRAY, 1, "%s", l1); T(200, 128, 0.6, GOLD, 1, "%s", l2);
  T(200, 212, 0.4, GRAY, 1, "Rekord: %d    START = Menü", hs[cur]);
}
#define DEADZONE 12
static float stick(int v) {
  if (v > -DEADZONE && v < DEADZONE) return 0;
  return (v > 0 ? v - DEADZONE : v + DEADZONE) / (float)(156 - DEADZONE);
}

// ---------- Töne (im Code erzeugt) ----------
enum { S_TICK, S_BLIP, S_COIN, S_HIT, S_LOW, S_LOSE, S_WIN, S_N0, S_N1, S_N2, S_N3, NSFX };
static const struct { int n; float f[4]; int ms[4]; int tri; } SND[NSFX] = {
  {1, {1200}, {30}, 0}, {2, {660, 880}, {50, 70}, 0}, {2, {880, 1320}, {60, 110}, 0}, {1, {300}, {50}, 1},
  {2, {220, 160}, {80, 120}, 1}, {4, {440, 370, 311, 220}, {120, 120, 120, 300}, 0}, {4, {523, 659, 784, 1047}, {100, 100, 100, 300}, 0},
  {1, {262}, {260}, 1}, {1, {330}, {260}, 1}, {1, {392}, {260}, 1}, {1, {523}, {260}, 1}};
static s16 *sbuf[NSFX]; static int slen[NSFX], snd_ok; static ndspWaveBuf swb[NSFX];
static void sndInit(void) {
  if (R_FAILED(ndspInit())) return;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  for (int i = 0; i < NSFX; i++) {
    int tot = 0; for (int k = 0; k < SND[i].n; k++) tot += SND[i].ms[k] * 22;
    s16 *b = (s16 *)linearAlloc(tot * sizeof(s16)); if (!b) continue;
    int p = 0; float ph = 0;
    for (int k = 0; k < SND[i].n; k++) {
      int len = SND[i].ms[k] * 22;
      for (int j = 0; j < len; j++) {
        ph += SND[i].f[k] / 22050.f; ph -= (int)ph;
        float w = SND[i].tri ? (ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph) : (ph < 0.5f ? 1.f : -1.f);
        b[p++] = (s16)(w * (1.f - 0.8f * j / len) * 7000);
      }
    }
    DSP_FlushDataCache(b, tot * sizeof(s16)); sbuf[i] = b; slen[i] = tot;
    ndspChnReset(i); ndspChnSetInterp(i, NDSP_INTERP_NONE); ndspChnSetRate(i, 22050); ndspChnSetFormat(i, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {0}; mix[0] = mix[1] = 0.8f; ndspChnSetMix(i, mix);
  }
  snd_ok = 1;
}
static void snd(int id) {
  if (!snd_ok || !sbuf[id]) return;
  ndspChnWaveBufClear(id); memset(&swb[id], 0, sizeof swb[id]);
  swb[id].data_vaddr = sbuf[id]; swb[id].nsamples = slen[id]; swb[id].looping = false; ndspChnWaveBufAdd(id, &swb[id]);
}
static void sndExit(void) { if (!snd_ok) return; ndspExit(); for (int i = 0; i < NSFX; i++) if (sbuf[i]) linearFree(sbuf[i]); }

// ================= 1) TIC-TAC-TOE =================
static int tb3[9], ttw;
static const int WL[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
static int ttWin(void) {
  for (int i = 0; i < 8; i++) { int a = tb3[WL[i][0]]; if (a && a == tb3[WL[i][1]] && a == tb3[WL[i][2]]) return a; }
  for (int i = 0; i < 9; i++) if (!tb3[i]) return 0;
  return 3;
}
static int ttFind(int p) { for (int i = 0; i < 9; i++) if (!tb3[i]) { tb3[i] = p; int w = ttWin(); tb3[i] = 0; if (w == p) return i; } return -1; }
static void ttInit(void) { memset(tb3, 0, sizeof tb3); score = 0; over = 0; ttw = 0; }
static void ttUpd(void) {
  if (!(kd & KEY_TOUCH)) return;
  int px = tp.px - 46, py = tp.py - 6; if (px < 0 || py < 0) return;
  int c = px / 78, r = py / 78; if (c > 2 || r > 2 || px % 78 >= 72 || py % 78 >= 72) return;
  int i = r * 3 + c; if (tb3[i]) return;
  tb3[i] = 1; snd(S_TICK); int w = ttWin();
  if (!w) {
    int m = ttFind(2); if (m < 0) m = ttFind(1); if (m < 0 && !tb3[4]) m = 4;
    if (m < 0) { int f[9], n = 0; for (int k = 0; k < 9; k++) if (!tb3[k]) f[n++] = k; m = f[RND(n)]; }
    tb3[m] = 2; snd(S_BLIP); w = ttWin();
  }
  if (w) { ttw = w; over = 1; score = w == 1 ? 100 : (w == 3 ? 30 : 0); }
}
static void ttTop(void) {
  gametop("Du bist X - tippe unten", "Gewinn = 100 Punkte");
  if (over) overlay(ttw == 1 ? "GEWONNEN!" : (ttw == 3 ? "UNENTSCHIEDEN" : "VERLOREN"));
}
static void ttBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  for (int i = 0; i < 9; i++) {
    float x = 46 + (i % 3) * 78, y = 6 + (i / 3) * 78; rect(x, y, 72, 72, COL(50, 56, 96));
    if (tb3[i] == 1) { u32 k = COL(255, 110, 110); C2D_DrawLine(x + 14, y + 14, k, x + 58, y + 58, k, 8, 0); C2D_DrawLine(x + 58, y + 14, k, x + 14, y + 58, k, 8, 0); }
    else if (tb3[i] == 2) { circ(x + 36, y + 36, 24, COL(110, 200, 255)); circ(x + 36, y + 36, 15, COL(50, 56, 96)); }
  }
}

// ================= 2) 4 GEWINNT =================
static int c4[6][7], c4m, c4w;
static int c4Drop(int col, int p) { for (int r = 5; r >= 0; r--) if (!c4[r][col]) { c4[r][col] = p; return r; } return -1; }
static void c4Undo(int col) { for (int r = 0; r < 6; r++) if (c4[r][col]) { c4[r][col] = 0; return; } }
static int c4Line(int p) {
  static const int D[4][2] = {{0,1},{1,0},{1,1},{1,-1}};
  for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++) for (int d = 0; d < 4; d++) {
    int k; for (k = 0; k < 4; k++) { int rr = r + D[d][0] * k, cc = c + D[d][1] * k; if (rr < 0 || rr > 5 || cc < 0 || cc > 6 || c4[rr][cc] != p) break; }
    if (k == 4) return 1;
  }
  return 0;
}
static int c4Full(void) { for (int c = 0; c < 7; c++) if (!c4[0][c]) return 0; return 1; }
static void c4Init(void) { memset(c4, 0, sizeof c4); score = 0; over = 0; c4m = 0; c4w = 0; }
static void c4Fin(int w) { c4w = w; over = 1; score = w == 1 ? (150 - c4m * 3 < 20 ? 20 : 150 - c4m * 3) : (w == 3 ? 30 : 0); }
static void c4Upd(void) {
  if (!(kd & KEY_TOUCH)) return;
  int col = (tp.px - 27) / 38; if (tp.px < 27 || col > 6) return;
  if (c4Drop(col, 1) < 0) return;
  c4m++; snd(S_TICK);
  if (c4Line(1)) { c4Fin(1); return; }
  if (c4Full()) { c4Fin(3); return; }
  static const int pref[7] = {3, 2, 4, 1, 5, 0, 6}; int pick = -1;
  for (int i = 0; i < 7 && pick < 0; i++) { int c = pref[i]; if (c4Drop(c, 2) >= 0) { int w = c4Line(2); c4Undo(c); if (w) pick = c; } }
  for (int i = 0; i < 7 && pick < 0; i++) { int c = pref[i]; if (c4Drop(c, 1) >= 0) { int w = c4Line(1); c4Undo(c); if (w) pick = c; } }
  while (pick < 0) { int c = RND(10) < 6 ? pref[RND(3)] : RND(7); if (!c4[0][c]) pick = c; }
  c4Drop(pick, 2); snd(S_BLIP);
  if (c4Line(2)) { c4Fin(2); return; }
  if (c4Full()) c4Fin(3);
}
static void c4Top(void) {
  gametop("Du bist Rot - tippe auf eine Spalte", "Schneller Sieg = mehr Punkte");
  if (over) overlay(c4w == 1 ? "GEWONNEN!" : (c4w == 3 ? "UNENTSCHIEDEN" : "VERLOREN"));
}
static void c4Bot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24)); rect(23, 2, 274, 236, COL(30, 70, 200));
  for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++)
    circ(27 + c * 38 + 19, 6 + r * 38 + 19, 15, c4[r][c] == 1 ? COL(230, 50, 50) : (c4[r][c] == 2 ? COL(250, 220, 50) : COL(12, 14, 40)));
}

// ================= 3) SIMON =================
static int smq[64], sml, sms, smp, smt, smlit, smlt;
static void smInit(void) { score = 0; over = 0; sml = 1; smq[0] = RND(4); sms = 0; smp = 0; smt = -30; smlit = -1; smlt = 0; }
static void smUpd(void) {
  if (smlt > 0 && --smlt == 0) smlit = -1;
  if (sms == 0) {
    smt++;
    if (smt % 40 == 1 && smp < sml) { smlit = smq[smp]; smlt = 26; snd(S_N0 + smlit); }
    if (smt > 0 && smt % 40 == 0) { smp++; if (smp >= sml) { sms = 1; smp = 0; smt = 0; } }
    return;
  }
  if (!(kd & KEY_TOUCH)) return;
  int q = (tp.px >= 160) + 2 * (tp.py >= 120); smlit = q; smlt = 14; snd(S_N0 + q);
  if (q != smq[smp]) { over = 1; return; }
  if (++smp >= sml) { score += 10; if (sml < 63) smq[sml++] = RND(4); sms = 0; smp = 0; smt = -30; }
}
static void smTop(void) {
  char b[32]; snprintf(b, sizeof b, "Runde: %d", sml);
  gametop(sms == 0 ? "Merk dir die Folge..." : "Du bist dran!", b);
  if (over) overlay("FALSCH!");
}
static void smBot(void) {
  static const u32 c[4] = {COL(220, 50, 50), COL(50, 190, 70), COL(50, 90, 230), COL(240, 200, 40)};
  for (int i = 0; i < 4; i++) rect((i & 1) * 160 + 3, (i >> 1) * 120 + 3, 154, 114, smlit == i ? sh(c[i], 70) : sh(c[i], -70));
}

// ================= 4) MAULWURF =================
static int wm[9], wt, wsp;
static void moInit(void) { memset(wm, 0, sizeof wm); score = 0; over = 0; wt = 1800; wsp = 20; }
static void moUpd(void) {
  if (--wt <= 0) { over = 1; return; }
  int el = 1800 - wt;
  if (--wsp <= 0) { wsp = 22 - el / 150; if (wsp < 8) wsp = 8; int h = RND(9); if (!wm[h]) { wm[h] = 48 - el / 80; if (wm[h] < 22) wm[h] = 22; } }
  for (int i = 0; i < 9; i++) { if (wm[i] > 0) wm[i]--; else if (wm[i] < 0) wm[i]++; }
  if (kd & KEY_TOUCH) { int i = (tp.py * 3 / 240) * 3 + (tp.px * 3 / 320); if (i >= 0 && i < 9 && wm[i] > 0) { wm[i] = -12; score += 10; snd(S_COIN); } }
}
static void moTop(void) {
  char b[32]; snprintf(b, sizeof b, "Punkte: %d", score);
  gametop("Tippe die Maulwürfe an!", b);
  rect(20, 160, 360, 16, COL(40, 44, 80)); rect(20, 160, 360.f * wt / 1800, 16, COL(120, 220, 120));
  if (over) overlay("ZEIT ABGELAUFEN");
}
static void moBot(void) {
  grad(0, 0, 320, 240, COL(70, 110, 50), COL(40, 70, 30));
  for (int i = 0; i < 9; i++) {
    float cx = 53 + (i % 3) * 107, cy = 40 + (i / 3) * 80; circ(cx, cy + 16, 32, COL(40, 25, 15));
    if (wm[i] > 0) { circ(cx, cy - 2, 26, COL(150, 100, 60)); circ(cx - 9, cy - 8, 5, WHITE); circ(cx + 9, cy - 8, 5, WHITE); circ(cx - 9, cy - 8, 2, COL(0, 0, 0)); circ(cx + 9, cy - 8, 2, COL(0, 0, 0)); circ(cx, cy + 4, 6, COL(230, 120, 140)); }
    else if (wm[i] < 0) circ(cx, cy, 18, GOLD);
  }
}

// ================= 5) INVADERS =================
static int iv[4][8], ivx, ivy, ivd, ivt, ivn, pbon, silv; static float six, pbx, pby, eb[4][3];
static void siRound(void) { for (int r = 0; r < 4; r++) for (int c = 0; c < 8; c++) iv[r][c] = 1; ivx = 40; ivy = 24; ivd = 1; ivt = 0; ivn = 32; pbon = 0; memset(eb, 0, sizeof eb); }
static void siInit(void) { score = 0; over = 0; silv = 3; six = 200; siRound(); }
static void siUpd(void) {
  if (kh & KEY_LEFT) six -= 3; if (kh & KEY_RIGHT) six += 3;
  six += stick(cp.dx) * 4; if (kh & KEY_TOUCH) six = tp.px * 1.25f; six = clampf(six, 14, 386);
  if ((kd & (KEY_A | KEY_B | KEY_X)) && !pbon) { pbon = 1; pbx = six; pby = 206; snd(S_BLIP); }
  if (pbon) {
    pby -= 7; if (pby < 0) pbon = 0;
    for (int r = 0; r < 4 && pbon; r++) for (int c = 0; c < 8 && pbon; c++) if (iv[r][c]) {
      float x = ivx + c * 36, y = ivy + r * 28;
      if (pbx > x && pbx < x + 26 && pby > y && pby < y + 18) { iv[r][c] = 0; ivn--; pbon = 0; score += 10 * (4 - r); snd(S_HIT); }
    }
  }
  int per = 3 + ivn * 4 / 5;
  if (++ivt >= per) {
    ivt = 0; int cmin = 8, cmax = -1, low = -1;
    for (int r = 0; r < 4; r++) for (int c = 0; c < 8; c++) if (iv[r][c]) { if (c < cmin) cmin = c; if (c > cmax) cmax = c; if (r > low) low = r; }
    if (cmax >= 0) {
      float xl = ivx + cmin * 36, xr = ivx + cmax * 36 + 26;
      if ((ivd > 0 && xr + 6 >= 396) || (ivd < 0 && xl - 6 <= 4)) { ivd = -ivd; ivy += 14; } else ivx += ivd * 6;
      if (ivy + low * 28 + 18 >= 200) over = 1;
    }
  }
  if (RND(50) == 0) for (int t = 0; t < 12; t++) {
    int r = RND(4), c = RND(8);
    if (iv[r][c]) { for (int b = 0; b < 4; b++) if (!eb[b][2]) { eb[b][0] = ivx + c * 36 + 13; eb[b][1] = ivy + r * 28 + 18; eb[b][2] = 1; break; } break; }
  }
  for (int b = 0; b < 4; b++) if (eb[b][2]) {
    eb[b][1] += 3.2f;
    if (eb[b][1] >= 210 && eb[b][1] <= 232 && fabsf(eb[b][0] - six) < 16) { eb[b][2] = 0; snd(S_LOW); if (--silv <= 0) over = 1; }
    else if (eb[b][1] > 240) eb[b][2] = 0;
  }
  if (ivn == 0) { score += 50; siRound(); }
}
static void siTop(void) {
  static const u32 rc[4] = {COL(240, 70, 70), COL(240, 160, 50), COL(80, 210, 100), COL(90, 160, 250)};
  grad(0, 0, 400, 240, COL(6, 6, 24), COL(20, 10, 50));
  for (int i = 0; i < 30; i++) rect((i * 97) % 400, (i * 53 + tick / 3) % 240, 2, 2, COL(120, 120, 170));
  for (int r = 0; r < 4; r++) for (int c = 0; c < 8; c++) if (iv[r][c]) {
    float x = ivx + c * 36, y = ivy + r * 28; box(x, y, 26, 18, rc[r]); rect(x + 7, y + 6, 4, 4, COL(0, 0, 0)); rect(x + 15, y + 6, 4, 4, COL(0, 0, 0));
  }
  tri(six - 14, 226, six + 14, 226, six, 206, COL(120, 240, 160)); rect(six - 14, 226, 28, 6, COL(90, 200, 130));
  if (pbon) rect(pbx - 1, pby, 3, 10, WHITE);
  for (int b = 0; b < 4; b++) if (eb[b][2]) rect(eb[b][0] - 2, eb[b][1], 4, 10, COL(255, 120, 120));
  for (int i = 0; i < silv; i++) circ(14 + i * 14, 12, 5, COL(240, 80, 100));
  T(200, 4, 0.7, WHITE, 1, "%d", score);
  if (over) overlay("GAME OVER");
}

// ================= 6) AUSWEICHEN =================
static float dgp, dgx[12], dgy[12], dgw[12], dgv[12]; static int dgon[12], dgt, dgs;
static void dgInit(void) { memset(dgon, 0, sizeof dgon); dgp = 200; dgt = 0; dgs = 20; score = 0; over = 0; }
static void dgUpd(void) {
  dgt++; score = dgt / 6;
  if (kh & KEY_LEFT) dgp -= 4; if (kh & KEY_RIGHT) dgp += 4;
  dgp += stick(cp.dx) * 5; if (kh & KEY_TOUCH) dgp = tp.px * 1.25f; dgp = clampf(dgp, 10, 390);
  if (--dgs <= 0) {
    dgs = 30 - dgt / 120; if (dgs < 7) dgs = 7;
    for (int i = 0; i < 12; i++) if (!dgon[i]) { dgon[i] = 1; dgw[i] = 20 + RND(40); dgx[i] = RND(400 - (int)dgw[i]); dgy[i] = -20; dgv[i] = 2.f + RND(3) + dgt / 600.f; break; }
  }
  for (int i = 0; i < 12; i++) if (dgon[i]) {
    dgy[i] += dgv[i];
    if (dgy[i] > 240) dgon[i] = 0;
    else if (dgp + 10 > dgx[i] && dgp - 10 < dgx[i] + dgw[i] && dgy[i] + 20 > 200 && dgy[i] < 220) over = 1;
  }
}
static void dgTop(void) {
  grad(0, 0, 400, 240, COL(40, 20, 50), COL(14, 8, 24));
  for (int i = 0; i < 12; i++) if (dgon[i]) box(dgx[i], dgy[i], dgw[i], 20, sh(COL(240, 90, 90), -(i % 4) * 30));
  box(dgp - 10, 200, 20, 20, COL(120, 240, 160)); T(200, 6, 0.8, WHITE, 1, "%d", score);
  if (over) overlay("GAME OVER");
}

// ================= 7) STACK =================
static float stx[400], stw[400], scx, scw, scd, scs; static int stn;
static void stInit(void) { score = 0; over = 0; stx[0] = 100; stw[0] = 200; stn = 1; scx = 0; scw = 200; scd = 1; scs = 2.2f; }
static void stUpd(void) {
  scx += scd * scs;
  if (scx < 0) { scx = 0; scd = 1; } if (scx + scw > 400) { scx = 400 - scw; scd = -1; }
  if (kd & (KEY_A | KEY_TOUCH)) {
    float l = fmaxf(scx, stx[stn - 1]), r = fminf(scx + scw, stx[stn - 1] + stw[stn - 1]), ov = r - l;
    if (ov <= 0) { over = 1; return; }
    int perfect = fabsf(scx - stx[stn - 1]) < 4; if (perfect) { l = stx[stn - 1]; ov = stw[stn - 1]; }
    stx[stn] = l; stw[stn] = ov; stn++; score++; snd(perfect ? S_WIN : S_TICK);
    if (stn >= 399) over = 1;
    scw = ov; scx = 0; scd = 1; scs += 0.12f; if (scs > 6) scs = 6;
  }
}
static void stTop(void) {
  static const u32 pal[6] = {COL(240, 90, 90), COL(240, 160, 60), COL(240, 220, 70), COL(90, 210, 110), COL(80, 160, 250), COL(180, 110, 240)};
  grad(0, 0, 400, 240, COL(20, 24, 56), COL(50, 30, 80));
  float off = stn > 8 ? (stn - 8) * 14 : 0;
  for (int k = 0; k < stn; k++) { float y = 226 - k * 14 + off; if (y < 240 && y > -14) box(stx[k], y, stw[k], 14, pal[k % 6]); }
  if (!over) box(scx, 226 - stn * 14 + off, scw, 14, pal[stn % 6]);
  T(200, 6, 1.2, WHITE, 1, "%d", score);
  if (over) overlay("GAME OVER");
}

// ================= 8) LIGHTS OUT =================
static int lo[5][5], lom;
static void loTog(int r, int c) {
  static const int d[5][2] = {{0,0},{1,0},{-1,0},{0,1},{0,-1}};
  for (int i = 0; i < 5; i++) { int rr = r + d[i][0], cc = c + d[i][1]; if (rr >= 0 && rr < 5 && cc >= 0 && cc < 5) lo[rr][cc] ^= 1; }
}
static int loDone(void) { for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) if (lo[r][c]) return 0; return 1; }
static void loInit(void) { memset(lo, 0, sizeof lo); score = 0; over = 0; lom = 0; for (int i = 0; i < 12; i++) loTog(RND(5), RND(5)); if (loDone()) loTog(2, 2); }
static void loUpd(void) {
  if (!(kd & KEY_TOUCH)) return;
  int px = tp.px - 42, py = tp.py - 2; if (px < 0 || py < 0) return;
  int c = px / 48, r = py / 48; if (c > 4 || r > 4 || px % 48 >= 44 || py % 48 >= 44) return;
  loTog(r, c); lom++; snd(S_TICK);
  if (loDone()) { over = 1; score = 1000 - lom * 20; if (score < 100) score = 100; }
}
static void loTop(void) {
  char b[32]; snprintf(b, sizeof b, "Züge: %d", lom);
  gametop("Tippe unten: alle Lichter aus!", b);
  if (over) overlay("GESCHAFFT!");
}
static void loBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) box(42 + c * 48, 2 + r * 48, 44, 44, lo[r][c] ? COL(250, 230, 100) : COL(40, 46, 80));
}

// ================= Spieltabelle =================
typedef struct { void (*init)(void); void (*upd)(void); void (*top)(void); void (*bot)(void); } Game;
static void snBot(void) { infobot("Steuerkreuz / Kreispad / Touch"); }
static void siBot(void) { infobot("Bewegen + A = Schießen"); }
static void stBot(void) { infobot("A / Tippen = Block setzen"); }
static const Game G[NG] = {
  {ttInit, ttUpd, ttTop, ttBot}, {c4Init, c4Upd, c4Top, c4Bot}, {smInit, smUpd, smTop, smBot}, {moInit, moUpd, moTop, moBot},
  {siInit, siUpd, siTop, siBot}, {dgInit, dgUpd, dgTop, snBot}, {stInit, stUpd, stTop, stBot}, {loInit, loUpd, loTop, loBot}};

static void loadhs(void) {
  FILE *f = fopen(SAVEFILE, "rb"); if (f) { if (fread(hs, sizeof hs, 1, f) != 1) memset(hs, 0, sizeof hs); fclose(f); }
  for (int i = 0; i < NG; i++) if (hs[i] < 0 || hs[i] > 10000000) hs[i] = 0;
}
static void savehs(void) {
  if (cur < 0 || score <= hs[cur]) return;
  hs[cur] = score; FILE *f = fopen(SAVEFILE, "wb"); if (f) { fwrite(hs, sizeof hs, 1, f); fclose(f); }
}
static void begin(int i) { snd(S_BLIP); cur = i; srand((unsigned)time(NULL) + tick); G[cur].init(); sv = 0; }

// ================= Menü =================
static void menuUpd(void) {
  int old = sel;
  if (kd & KEY_DOWN) sel = (sel + 2) % NG; if (kd & KEY_UP) sel = (sel + NG - 2) % NG;
  if (kd & KEY_RIGHT) sel = (sel + 1) % NG; if (kd & KEY_LEFT) sel = (sel + NG - 1) % NG;
  if (sel != old) snd(S_TICK);
  if (kd & KEY_TOUCH) for (int i = 0; i < NG; i++) {
    float x = 8 + (i % 2) * 156, y = 6 + (i / 2) * 58;
    if (tp.px >= x && tp.px < x + 148 && tp.py >= y && tp.py < y + 50) { sel = i; begin(i); return; }
  }
  if (kd & KEY_A) begin(sel);
}
static void menuTop(void) {
  grad(0, 0, 400, 240, COL(18, 40, 50), COL(60, 30, 94));
  for (int i = 0; i < 14; i++) {
    float x = fmodf(i * 61.f + tick * (0.25f + (i % 4) * 0.15f), 440.f) - 20, y = 10 + (i * 47) % 220 + sinf(tick * 0.03f + i) * 8, s = 14 + (i % 3) * 6;
    rect(x, y, s, s, (ACC[i % NG] & 0x00FFFFFF) | 0x50000000);
  }
  T(202, 20, 1.5, COL(0, 0, 0), 1, "ARCADE 2"); T(200, 18, 1.5, WHITE, 1, "ARCADE 2");
  T(200, 74, 0.5, GRAY, 1, "8 neue Spiele");
  rect(40, 108, 320, 104, COLA(0, 0, 0, 120));
  T(200, 114, 0.95, ACC[sel], 1, "%s", NAME[sel]); T(200, 152, 0.5, WHITE, 1, "%s", DESC[sel]); T(200, 180, 0.5, GOLD, 1, "Rekord: %d", hs[sel]);
  T(200, 222, 0.38, GRAY, 1, "A / Tippen = Starten      START = Beenden");
  if (!snd_ok) T(394, 6, 0.34, GRAY, 2, "Kein Ton (dspfirm.cdc fehlt)");
}
static void menuBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  for (int i = 0; i < NG; i++) { float x = 8 + (i % 2) * 156, y = 6 + (i / 2) * 58; btn(x, y, 148, 50, ACC[i], i == sel); TT(x + 74, y + 25, 0.6, WHITE, NAME[i]); }
}

int main(void) {
  gfxInitDefault(); C3D_Init(C3D_DEFAULT_CMDBUF_SIZE); C2D_Init(C2D_DEFAULT_MAX_OBJECTS); C2D_Prepare();
  C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT), *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
  tbuf = C2D_TextBufNew(8192); srand((unsigned)time(NULL)); loadhs(); sndInit();
  while (aptMainLoop()) {
    hidScanInput(); kd = hidKeysDown(); kh = hidKeysHeld(); hidTouchRead(&tp); hidCircleRead(&cp); tick++;
    if (kd & KEY_START) { if (cur >= 0) { savehs(); cur = -1; } else break; }
    if (cur < 0) menuUpd();
    else if (over) { if (kd & KEY_A) begin(cur); }
    else {
      G[cur].upd();
      if (over && !sv) { sv = 1; savehs(); snd((cur == G_TTT || cur == G_C4 || cur == G_MOLE || cur == G_LIGHTS) && score > 0 ? S_WIN : S_LOSE); }
    }
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW); C2D_TextBufClear(tbuf);
    C2D_TargetClear(top, COL(0, 0, 0)); C2D_SceneBegin(top); if (cur < 0) menuTop(); else G[cur].top();
    C2D_TargetClear(bot, COL(0, 0, 0)); C2D_SceneBegin(bot); if (cur < 0) menuBot(); else G[cur].bot();
    C3D_FrameEnd(0);
  }
  sndExit(); C2D_TextBufDelete(tbuf); C2D_Fini(); C3D_Fini(); gfxExit();
  return 0;
}
