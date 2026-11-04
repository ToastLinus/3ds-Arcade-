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
#define SAVEFILE "sdmc:/3ds/miniarcade.sav"

enum { G_SNAKE, G_2048, G_TETRIS, G_MEMORY, G_REACT, G_FLAPPY, G_MINES, G_PONG, G_BREAK, G_BJ, NG };
static const char *NAME[NG] = {"Snake","2048","Tetris","Memory","Reaktion","Flappy","Minesweeper","Pong","Breakout","Blackjack"};
static const char *DESC[NG] = {
  "Friss Äpfel und werde lang","Schiebe und verbinde Zahlen","Fallende Blöcke, Reihen löschen",
  "Finde alle Paare (Touch)","Wie schnell bist du? 5 Runden","Flieg durch die Röhren",
  "Finde die Minen (Touch)","Du gegen den Computer","Zerstöre alle Steine","Schlage den Dealer auf 21"};
static const u32 ACC[NG] = {
  COL(80,200,90),COL(240,170,60),COL(170,90,230),COL(70,150,240),COL(240,80,80),
  COL(70,200,220),COL(150,150,175),COL(200,200,215),COL(240,120,60),COL(60,160,100)};

static C2D_TextBuf tbuf;
static int hs[NG], score, over, cur = -1, sel, tick, sv;
static u32 kd, kh;
static touchPosition tp;
static circlePosition cp;

// ---------- Helfer ----------
static int RND(int n) { return rand() % n; }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static u32 sh(u32 c, int d) {
  int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
  r += d; g += d; b += d;
  r = r < 0 ? 0 : (r > 255 ? 255 : r); g = g < 0 ? 0 : (g > 255 ? 255 : g); b = b < 0 ? 0 : (b > 255 ? 255 : b);
  return COL(r, g, b);
}
static void rect(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x, y, 0, w, h, c); }
static void grad(float x, float y, float w, float h, u32 a, u32 b) { C2D_DrawRectangle(x, y, 0, w, h, a, a, b, b); }
static void circ(float x, float y, float r, u32 c) { C2D_DrawCircleSolid(x, y, 0, r, c); }
static void tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) { C2D_DrawTriangle(x0, y0, c, x1, y1, c, x2, y2, c, 0); }
static void cell(float x, float y, float s, u32 c) {
  rect(x, y, s, s, sh(c, -70)); rect(x + 1, y + 1, s - 2, s - 2, c);
  rect(x + 1, y + 1, s - 2, 2, sh(c, 70)); rect(x + 1, y + 1, 2, s - 2, sh(c, 40));
}
// al: 0 links, 1 mittig (x), 2 rechts, 3 mittig (x und y)
static void T(float x, float y, float s, u32 c, int al, const char *f, ...) {
  char b[96]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a);
  C2D_Text t; C2D_TextParse(&t, tbuf, b); C2D_TextOptimize(&t);
  float w = 0, h = 0; C2D_TextGetDimensions(&t, s, s, &w, &h);
  if (al == 1) x -= w / 2; else if (al == 2) x -= w; else if (al == 3) { x -= w / 2; y -= h / 2; }
  C2D_DrawText(&t, C2D_WithColor, x, y, 0, s, s, c);
}
static void TT(float x, float y, float s, u32 c, const char *str) {
  T(x + 1, y + 1, s, COL(0, 0, 0), 3, "%s", str); T(x, y, s, c, 3, "%s", str);
}
static void btn(float x, float y, float w, float h, u32 c, int selected) {
  if (selected) rect(x - 3, y - 3, w + 6, h + 6, WHITE); else rect(x - 1, y - 1, w + 2, h + 2, sh(c, -90));
  grad(x, y, w, h, sh(c, 45), sh(c, -35)); rect(x, y, w, 2, sh(c, 90));
}
static void overlay(const char *t) {
  rect(0, 0, 400, 240, COLA(0, 0, 0, 120));
  rect(90, 70, 220, 100, COL(20, 22, 40)); rect(92, 72, 216, 96, COL(40, 44, 80));
  T(200, 80, 0.9, GOLD, 1, "%s", t);
  T(200, 116, 0.6, WHITE, 1, "Punkte: %d", score);
  T(200, 142, 0.42, GRAY, 1, "A = Nochmal   START = Menü");
}
static void infobot(const char *hint, const char *extra) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  T(160, 16, 0.9, ACC[cur], 1, "%s", NAME[cur]);
  T(160, 66, 0.65, WHITE, 1, "Punkte: %d", score);
  T(160, 94, 0.5, GOLD, 1, "Rekord: %d", hs[cur]);
  if (extra) T(160, 124, 0.5, GRAY, 1, "%s", extra);
  T(160, 162, 0.42, GRAY, 1, "%s", hint);
  T(160, 210, 0.4, COL(120, 125, 150), 1, "START = Menü");
}
static char xb[48];

// ================= 1) SNAKE =================
static int sx[240], sy[240], sl, sdx, sdy, sndx, sndy, sfx, sfy, stm;
static void snFood(void) {
  for (;;) { sfx = RND(20); sfy = RND(12); int ok = 1;
    for (int i = 0; i < sl; i++) if (sx[i] == sfx && sy[i] == sfy) ok = 0;
    if (ok) return; }
}
static void snInit(void) {
  score = 0; over = 0; sl = 3; for (int i = 0; i < 3; i++) { sx[i] = 10 - i; sy[i] = 6; }
  sdx = sndx = 1; sdy = sndy = 0; stm = 0; snFood();
}
static void snUpd(void) {
  if ((kd & KEY_UP) && sdy == 0) { sndx = 0; sndy = -1; }
  if ((kd & KEY_DOWN) && sdy == 0) { sndx = 0; sndy = 1; }
  if ((kd & KEY_LEFT) && sdx == 0) { sndx = -1; sndy = 0; }
  if ((kd & KEY_RIGHT) && sdx == 0) { sndx = 1; sndy = 0; }
  int sp = 9 - sl / 5; if (sp < 4) sp = 4;
  if (++stm < sp) return;
  stm = 0; sdx = sndx; sdy = sndy;
  int nx = sx[0] + sdx, ny = sy[0] + sdy;
  if (nx < 0 || ny < 0 || nx >= 20 || ny >= 12) { over = 1; return; }
  for (int i = 0; i < sl - 1; i++) if (sx[i] == nx && sy[i] == ny) { over = 1; return; }
  int eat = (nx == sfx && ny == sfy);
  if (eat && sl < 239) sl++;
  for (int i = sl - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
  sx[0] = nx; sy[0] = ny;
  if (eat) { score += 10; snFood(); }
}
static void snTop(void) {
  for (int y = 0; y < 12; y++) for (int x = 0; x < 20; x++)
    rect(x * 20, y * 20, 20, 20, ((x + y) & 1) ? COL(34, 54, 42) : COL(28, 46, 36));
  circ(sfx * 20 + 10, sfy * 20 + 11, 7, COL(235, 55, 55)); circ(sfx * 20 + 8, sfy * 20 + 8, 2, COL(255, 210, 210));
  rect(sfx * 20 + 9, sfy * 20 + 1, 2, 4, COL(110, 70, 30));
  for (int i = sl - 1; i >= 0; i--) {
    int d = i < 20 ? i * 4 : 80;
    cell(sx[i] * 20 + 1, sy[i] * 20 + 1, 18, i == 0 ? COL(130, 245, 100) : sh(COL(60, 205, 85), -d));
  }
  float hx = sx[0] * 20 + 10 + sdx * 3, hy = sy[0] * 20 + 10 + sdy * 3;
  circ(hx - 4 + (sdy ? 0 : 0), hy - 3, 3, WHITE); circ(hx + 4, hy - 3, 3, WHITE);
  circ(hx - 4, hy - 3, 1.4f, COL(0, 0, 0)); circ(hx + 4, hy - 3, 1.4f, COL(0, 0, 0));
  if (over) overlay("GAME OVER");
}
static void snBot(void) { infobot("Steuerkreuz = Richtung", NULL); }

// ================= 2) 2048 =================
static int g2[4][4];
static void g2Add(void) {
  int fr[16][2], n = 0;
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (!g2[y][x]) { fr[n][0] = y; fr[n][1] = x; n++; }
  if (!n) return; int k = RND(n); g2[fr[k][0]][fr[k][1]] = RND(10) ? 2 : 4;
}
static void g2Init(void) { memset(g2, 0, sizeof g2); score = 0; over = 0; g2Add(); g2Add(); }
static int g2Line(int *p[4]) {
  int v[4], n = 0, old[4]; for (int i = 0; i < 4; i++) old[i] = *p[i];
  for (int i = 0; i < 4; i++) if (*p[i]) v[n++] = *p[i];
  for (int i = 0; i < n - 1; i++) if (v[i] == v[i + 1]) {
    v[i] *= 2; score += v[i];
    for (int j = i + 1; j < n - 1; j++) v[j] = v[j + 1];
    n--;
  }
  int ch = 0; for (int i = 0; i < 4; i++) { *p[i] = i < n ? v[i] : 0; if (*p[i] != old[i]) ch = 1; }
  return ch;
}
static void g2Upd(void) {
  int d = -1;
  if (kd & KEY_LEFT) d = 0; else if (kd & KEY_RIGHT) d = 1; else if (kd & KEY_UP) d = 2; else if (kd & KEY_DOWN) d = 3;
  if (d < 0) return;
  int ch = 0;
  for (int k = 0; k < 4; k++) {
    int *p[4];
    for (int i = 0; i < 4; i++) {
      if (d == 0) p[i] = &g2[k][i]; else if (d == 1) p[i] = &g2[k][3 - i];
      else if (d == 2) p[i] = &g2[i][k]; else p[i] = &g2[3 - i][k];
    }
    ch |= g2Line(p);
  }
  if (ch) g2Add();
  int can = 0;
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
    if (!g2[y][x]) can = 1;
    if (x < 3 && g2[y][x] == g2[y][x + 1]) can = 1;
    if (y < 3 && g2[y][x] == g2[y + 1][x]) can = 1;
  }
  if (!can) over = 1;
}
static void g2Top(void) {
  grad(0, 0, 400, 240, COL(60, 50, 40), COL(35, 28, 22));
  float x0 = 82, y0 = 2; rect(x0, y0, 236, 236, COL(120, 105, 90));
  static const u32 tc[12] = {COL(238,228,218),COL(237,224,200),COL(242,177,121),COL(245,149,99),COL(246,124,95),COL(246,94,59),
    COL(237,207,114),COL(237,204,97),COL(237,200,80),COL(237,197,63),COL(237,194,46),COL(60,58,50)};
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) {
    float px = x0 + 4 + x * 58, py = y0 + 4 + y * 58; int v = g2[y][x];
    if (!v) { rect(px, py, 54, 54, COL(150, 135, 120)); continue; }
    int li = 0; while ((2 << li) < v) li++; if (li > 11) li = 11;
    cell(px, py, 54, tc[li]);
    T(px + 27, py + 27, v < 100 ? 0.85f : (v < 1000 ? 0.7f : 0.55f), v < 8 ? COL(90, 80, 70) : WHITE, 3, "%d", v);
  }
  T(8, 8, 0.5, GRAY, 0, "Ziel: 2048");
  if (over) overlay("GAME OVER");
}
static void g2Bot(void) { infobot("Steuerkreuz = Schieben", NULL); }

// ================= 3) TETRIS =================
static const char *TS[7][4] = {{"....","####","....","...."},{"##","##","",""},{".#.","###","...",""},
  {".##","##.","...",""},{"##.",".##","...",""},{"#..","###","...",""},{"..#","###","...",""}};
static const int TSZ[7] = {4, 2, 3, 3, 3, 3, 3};
static const u32 TC[7] = {COL(0,230,240),COL(240,220,0),COL(180,60,230),COL(60,220,70),COL(240,50,50),COL(60,90,240),COL(250,150,20)};
static int bd[20][10], pc[4][4], pt, psz, ppx, ppy, nxt, tfall, tlines, tlvl, lh, rh;
static void tLoad(int t) {
  pt = t; psz = TSZ[t]; memset(pc, 0, sizeof pc);
  for (int y = 0; y < psz; y++) for (int x = 0; x < psz; x++) pc[y][x] = TS[t][y][x] == '#';
}
static int tFit(int px, int py) {
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (pc[y][x]) {
    int gx = px + x, gy = py + y;
    if (gx < 0 || gx >= 10 || gy >= 20) return 0;
    if (gy >= 0 && bd[gy][gx]) return 0;
  }
  return 1;
}
static void tSpawn(void) { tLoad(nxt); nxt = RND(7); ppx = 3; ppy = 0; tfall = 0; if (!tFit(ppx, ppy)) over = 1; }
static void tInit(void) { memset(bd, 0, sizeof bd); score = 0; over = 0; tlines = 0; tlvl = 0; lh = rh = 0; nxt = RND(7); tSpawn(); }
static void tRot(void) {
  int n[4][4] = {{0}}, o[4][4];
  for (int y = 0; y < psz; y++) for (int x = 0; x < psz; x++) n[x][psz - 1 - y] = pc[y][x];
  memcpy(o, pc, sizeof o); memcpy(pc, n, sizeof n);
  static const int k[5] = {0, -1, 1, -2, 2};
  for (int i = 0; i < 5; i++) if (tFit(ppx + k[i], ppy)) { ppx += k[i]; return; }
  memcpy(pc, o, sizeof o);
}
static void tLock(void) {
  for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (pc[y][x] && ppy + y >= 0) bd[ppy + y][ppx + x] = pt + 1;
  int n = 0;
  for (int y = 19; y >= 0; y--) {
    int f = 1; for (int x = 0; x < 10; x++) if (!bd[y][x]) f = 0;
    if (f) { n++; for (int r = y; r > 0; r--) memcpy(bd[r], bd[r - 1], sizeof bd[0]); memset(bd[0], 0, sizeof bd[0]); y++; }
  }
  static const int pts[5] = {0, 100, 300, 500, 800};
  score += pts[n] * (tlvl + 1); tlines += n; tlvl = tlines / 10; tSpawn();
}
static void tUpd(void) {
  if (kd & KEY_UP) { while (tFit(ppx, ppy + 1)) ppy++; tLock(); return; }
  if (kd & (KEY_A | KEY_X)) tRot();
  if (kd & KEY_LEFT) { if (tFit(ppx - 1, ppy)) ppx--; lh = 0; }
  if (kh & KEY_LEFT) { if (++lh > 10 && lh % 3 == 0 && tFit(ppx - 1, ppy)) ppx--; } else lh = 0;
  if (kd & KEY_RIGHT) { if (tFit(ppx + 1, ppy)) ppx++; rh = 0; }
  if (kh & KEY_RIGHT) { if (++rh > 10 && rh % 3 == 0 && tFit(ppx + 1, ppy)) ppx++; } else rh = 0;
  int sp = 48 - tlvl * 4; if (sp < 6) sp = 6; if (kh & KEY_DOWN) sp = 2;
  if (++tfall >= sp) { tfall = 0; if (tFit(ppx, ppy + 1)) ppy++; else tLock(); }
}
static void tTop(void) {
  grad(0, 0, 400, 240, COL(20, 16, 44), COL(52, 22, 84));
  for (int i = 0; i < 8; i++) { float y = fmodf(i * 53 + tick * (0.3f + (i % 3) * 0.2f), 260) - 10;
    rect(10 + (i % 2) * 330 + (i % 3) * 20, y, 18, 18, (TC[i % 7] & 0x00FFFFFF) | 0x40000000); }
  rect(138, 0, 124, 240, COL(6, 6, 18));
  for (int x = 0; x <= 10; x++) rect(140 + x * 12, 0, 1, 240, COL(22, 22, 42));
  for (int y = 0; y <= 20; y++) rect(140, y * 12, 120, 1, COL(22, 22, 42));
  for (int y = 0; y < 20; y++) for (int x = 0; x < 10; x++) if (bd[y][x]) cell(140 + x * 12, y * 12, 12, TC[bd[y][x] - 1]);
  if (!over) {
    int gy = ppy; while (tFit(ppx, gy + 1)) gy++;
    for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) if (pc[y][x]) {
      rect(140 + (ppx + x) * 12, (gy + y) * 12, 12, 12, COLA(255, 255, 255, 45));
      if (ppy + y >= 0) cell(140 + (ppx + x) * 12, (ppy + y) * 12, 12, TC[pt]);
    }
  }
  if (over) overlay("GAME OVER");
}
static void tBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  T(160, 10, 0.8, ACC[G_TETRIS], 1, "Tetris");
  T(20, 50, 0.55, WHITE, 0, "Punkte: %d", score); T(20, 76, 0.5, GOLD, 0, "Rekord: %d", hs[G_TETRIS]);
  T(20, 102, 0.5, GRAY, 0, "Level: %d", tlvl); T(20, 126, 0.5, GRAY, 0, "Reihen: %d", tlines);
  T(255, 40, 0.5, GRAY, 1, "Weiter");
  rect(215, 64, 80, 80, COL(8, 8, 22));
  int s = TSZ[nxt]; float cs = 18, ox = 255 - s * cs / 2, oy = 104 - s * cs / 2;
  for (int y = 0; y < s; y++) for (int x = 0; x < s; x++) if (TS[nxt][y][x] == '#') cell(ox + x * cs, oy + y * cs, cs, TC[nxt]);
  T(160, 168, 0.4, GRAY, 1, "A = Drehen   Oben = Fallen lassen");
  T(160, 188, 0.4, GRAY, 1, "Links/Rechts = Bewegen   Unten = Schneller");
  T(160, 214, 0.4, COL(120, 125, 150), 1, "START = Menü");
}

// ================= 4) MEMORY (Touch unten) =================
static int mv[16], mf[16], m1, m2, mtm, mmoves, mdone;
static void meInit(void) {
  score = 0; over = 0; for (int i = 0; i < 16; i++) { mv[i] = i / 2; mf[i] = 0; }
  for (int i = 15; i > 0; i--) { int j = RND(i + 1), t = mv[i]; mv[i] = mv[j]; mv[j] = t; }
  m1 = m2 = -1; mtm = 0; mmoves = 0; mdone = 0;
}
static void meUpd(void) {
  if (mtm > 0) { if (--mtm == 0) { mf[m1] = 0; mf[m2] = 0; m1 = m2 = -1; } return; }
  if (!(kd & KEY_TOUCH)) return;
  int px = tp.px - 8, py = tp.py - 8; if (px < 0 || py < 0) return;
  int c = px / 78, r = py / 58; if (c > 3 || r > 3 || px % 78 >= 70 || py % 58 >= 50) return;
  int i = r * 4 + c; if (mf[i]) return;
  mf[i] = 1;
  if (m1 < 0) m1 = i;
  else {
    m2 = i; mmoves++;
    if (mv[m1] == mv[m2]) { mf[m1] = mf[m2] = 2; mdone++; m1 = m2 = -1;
      if (mdone == 8) { over = 1; score = 1200 - mmoves * 50; if (score < 100) score = 100; } }
    else mtm = 45;
  }
}
static void sym(float x, float y, int v) {
  static const u32 c[8] = {COL(230,50,60),COL(50,100,230),COL(40,190,80),COL(240,200,30),COL(170,70,220),COL(240,130,30),COL(30,190,200),COL(240,80,160)};
  u32 k = c[v];
  switch (v) {
    case 0: circ(x, y, 15, k); circ(x - 5, y - 5, 4, sh(k, 90)); break;
    case 1: rect(x - 13, y - 13, 26, 26, k); rect(x - 13, y - 13, 26, 4, sh(k, 80)); break;
    case 2: tri(x - 16, y + 13, x + 16, y + 13, x, y - 15, k); break;
    case 3: tri(x - 15, y, x + 15, y, x, y - 18, k); tri(x - 15, y, x + 15, y, x, y + 18, k); break;
    case 4: rect(x - 5, y - 16, 10, 32, k); rect(x - 16, y - 5, 32, 10, k); break;
    case 5: circ(x, y, 15, k); circ(x, y, 7, COL(250, 250, 250)); break;
    case 6: rect(x - 15, y - 13, 30, 7, k); rect(x - 15, y - 3, 30, 7, k); rect(x - 15, y + 7, 30, 7, k); break;
    default: circ(x - 7, y - 4, 8, k); circ(x + 7, y - 4, 8, k); tri(x - 15, y, x + 15, y, x, y + 16, k); break;
  }
}
static void meTop(void) {
  grad(0, 0, 400, 240, COL(40, 30, 90), COL(16, 12, 40));
  T(200, 18, 1.2, WHITE, 1, "Memory");
  T(200, 84, 0.55, GRAY, 1, "Tippe unten auf die Karten");
  T(200, 118, 0.8, GOLD, 1, "Züge: %d", mmoves);
  T(200, 152, 0.8, COL(120, 240, 140), 1, "Paare: %d / 8", mdone);
  T(200, 212, 0.4, GRAY, 1, "Rekord: %d    START = Menü", hs[G_MEMORY]);
  if (over) overlay("GEWONNEN!");
}
static void meBot(void) {
  grad(0, 0, 320, 240, COL(30, 24, 60), COL(12, 10, 30));
  for (int i = 0; i < 16; i++) {
    float x = 8 + (i % 4) * 78, y = 8 + (i / 4) * 58;
    if (mf[i] == 0) { grad(x, y, 70, 50, COL(120, 90, 230), COL(60, 40, 150)); rect(x + 3, y + 3, 64, 2, COL(170, 150, 255)); T(x + 35, y + 25, 0.9, COL(210, 200, 255), 3, "?"); }
    else { rect(x, y, 70, 50, mf[i] == 2 ? COL(170, 235, 170) : COL(250, 250, 250)); sym(x + 35, y + 25, mv[i]); }
  }
}

// ================= 5) REAKTION =================
static int rs, rt, rn, rms; static u64 r0;
static void reInit(void) { score = 0; over = 0; rn = 0; rs = 0; rt = 60 + RND(120); rms = 0; }
static void reUpd(void) {
  int press = (kd & (KEY_A | KEY_TOUCH)) != 0;
  if (rs == 0) { if (press) { rs = 3; rn++; if (rn >= 5) over = 1; } else if (--rt <= 0) { rs = 1; r0 = osGetTime(); } }
  else if (rs == 1) {
    if (press) { rms = (int)(osGetTime() - r0); int p = 1000 - rms * 2; if (p < 0) p = 0; score += p; rs = 2; rn++; if (rn >= 5) over = 1; }
  } else if (press) { rs = 0; rt = 60 + RND(120); }
}
static u32 reCol(void) { return rs == 0 ? COL(200, 40, 55) : rs == 1 ? COL(35, 195, 85) : rs == 2 ? COL(40, 95, 210) : COL(220, 130, 30); }
static void reTop(void) {
  u32 c = reCol(); grad(0, 0, 400, 240, sh(c, 30), sh(c, -40));
  if (rs == 0) T(200, 110, 1.5, WHITE, 3, "Warte...");
  else if (rs == 1) T(200, 110, 2.0, WHITE, 3, "JETZT!");
  else if (rs == 2) { T(200, 90, 1.6, WHITE, 3, "%d ms", rms); T(200, 140, 0.7, WHITE, 3, "+%d Punkte", 1000 - rms * 2 < 0 ? 0 : 1000 - rms * 2); }
  else T(200, 110, 1.5, WHITE, 3, "Zu früh!");
  T(200, 14, 0.6, WHITE, 1, "Runde %d / 5", rn + (over ? 0 : 1) > 5 ? 5 : rn + (over ? 0 : 1));
  if (over) overlay("FERTIG!");
}
static void reBot(void) {
  u32 c = reCol(); grad(0, 0, 320, 240, sh(c, 30), sh(c, -40));
  T(160, 100, 0.8, WHITE, 3, "Tippe hier oder A");
  T(160, 140, 0.6, WHITE, 3, "Punkte: %d", score);
  T(160, 215, 0.4, WHITE, 1, "START = Menü");
}

// ================= 6) FLAPPY =================
static float fy, fvy, fpx[3]; static int fpg[3], fpp[3], fst;
static void flInit(void) {
  score = 0; over = 0; fy = 110; fvy = 0; fst = 0;
  for (int i = 0; i < 3; i++) { fpx[i] = 300 + i * 150; fpg[i] = 30 + RND(110); fpp[i] = 0; }
}
static void flUpd(void) {
  int flap = (kd & (KEY_A | KEY_TOUCH | KEY_UP)) != 0;
  if (!fst) { fy = 110 + sinf(tick * 0.1f) * 6; if (flap) { fst = 1; fvy = -4.6f; } return; }
  if (flap) fvy = -4.6f;
  fvy += 0.28f; if (fvy > 7) fvy = 7; fy += fvy;
  for (int i = 0; i < 3; i++) {
    fpx[i] -= 2.2f;
    if (fpx[i] < -40) { fpx[i] += 450; fpg[i] = 30 + RND(110); fpp[i] = 0; }
    if (!fpp[i] && fpx[i] + 36 < 80) { fpp[i] = 1; score++; }
    if (89 > fpx[i] && 71 < fpx[i] + 36 && (fy - 9 < fpg[i] || fy + 9 > fpg[i] + 76)) over = 1;
  }
  if (fy + 9 > 216 || fy < -20) over = 1;
}
static void flTop(void) {
  grad(0, 0, 400, 240, COL(90, 190, 255), COL(195, 238, 255));
  for (int i = 0; i < 4; i++) { float x = fmodf(i * 140.f - tick * 0.4f * (1 + (i & 1) * .5f) + 960.f, 480.f) - 40, y = 30 + i * 32;
    circ(x, y, 14, WHITE); circ(x + 16, y + 4, 11, WHITE); circ(x - 14, y + 5, 10, WHITE); }
  for (int i = 0; i < 3; i++) {
    float x = fpx[i]; int g = fpg[i]; u32 pg = COL(70, 190, 60), dk = COL(40, 120, 40);
    rect(x - 1, 0, 38, g, dk); rect(x, 0, 36, g, pg); rect(x + 4, 0, 6, g, sh(pg, 50)); rect(x - 4, g - 14, 44, 14, dk); rect(x - 3, g - 13, 42, 12, pg);
    rect(x - 1, g + 76, 38, 216 - g - 76, dk); rect(x, g + 76, 36, 216 - g - 76, pg); rect(x + 4, g + 76, 6, 216 - g - 76, sh(pg, 50)); rect(x - 4, g + 76, 44, 14, dk); rect(x - 3, g + 77, 42, 12, pg);
  }
  rect(0, 216, 400, 24, COL(220, 200, 120)); rect(0, 216, 400, 6, COL(90, 200, 70));
  circ(80, fy, 10, COL(255, 210, 40)); circ(76, fy + 3, 5, COL(240, 165, 30));
  circ(84, fy - 3, 3.5f, WHITE); circ(85, fy - 3, 1.6f, COL(0, 0, 0)); tri(88, fy - 1, 88, fy + 4, 97, fy + 2, COL(250, 120, 30));
  T(201, 11, 1.2, COL(0, 0, 0), 1, "%d", score); T(200, 10, 1.2, WHITE, 1, "%d", score);
  if (!fst) T(200, 170, 0.5, COL(30, 60, 100), 1, "A / Tippen = Flattern");
  if (over) overlay("GAME OVER");
}
static void flBot(void) { infobot("A / Tippen = Flattern", NULL); }

// ================= 7) MINESWEEPER (Touch unten) =================
static int mm[8][10], mr[8][10], mfl[8][10], mst, mflag, mfr, mrev, mlost;
static void mnInit(void) {
  memset(mm, 0, sizeof mm); memset(mr, 0, sizeof mr); memset(mfl, 0, sizeof mfl);
  mst = 0; mflag = 0; mfr = 0; mrev = 0; mlost = 0; score = 0; over = 0;
}
static int mnCount(int x, int y) {
  int c = 0;
  for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
    int nx = x + dx, ny = y + dy; if (nx >= 0 && ny >= 0 && nx < 10 && ny < 8 && mm[ny][nx]) c++;
  }
  return c;
}
static void mnFlood(int x, int y) {
  if (x < 0 || y < 0 || x >= 10 || y >= 8 || mr[y][x] || mfl[y][x]) return;
  mr[y][x] = 1; mrev++;
  if (!mm[y][x] && mnCount(x, y) == 0) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) mnFlood(x + dx, y + dy);
}
static void mnUpd(void) {
  if (kd & KEY_X) mflag = !mflag;
  if (mst) mfr++;
  if (!(kd & KEY_TOUCH)) return;
  int px = tp.px - 20, py = tp.py - 8; if (px < 0 || py < 0) return;
  int cx = px / 28, cy = py / 28; if (cx > 9 || cy > 7) return;
  if (!mst) { mst = 1; for (int k = 0; k < 10;) { int x = RND(10), y = RND(8); if (mm[y][x] || (x == cx && y == cy)) continue; mm[y][x] = 1; k++; } }
  if (mr[cy][cx]) return;
  if (mflag) { mfl[cy][cx] = !mfl[cy][cx]; return; }
  if (mfl[cy][cx]) return;
  if (mm[cy][cx]) { mlost = 1; over = 1; score = 0; return; }
  mnFlood(cx, cy);
  if (mrev >= 70) { over = 1; score = 1500 - (mfr / 60) * 10; if (score < 100) score = 100; }
}
static void mnTop(void) {
  grad(0, 0, 400, 240, COL(34, 38, 60), COL(14, 16, 28));
  int fl = 0; for (int y = 0; y < 8; y++) for (int x = 0; x < 10; x++) fl += mfl[y][x];
  T(200, 14, 1.1, WHITE, 1, "Minesweeper");
  T(200, 80, 0.8, COL(240, 90, 90), 1, "Minen: %d", 10 - fl);
  T(200, 116, 0.8, GOLD, 1, "Zeit: %d s", mfr / 60);
  T(200, 156, 0.55, mflag ? COL(250, 120, 120) : COL(120, 240, 140), 1, mflag ? "Modus: FLAGGE" : "Modus: AUFDECKEN");
  T(200, 184, 0.42, GRAY, 1, "X = Modus wechseln");
  T(200, 212, 0.4, GRAY, 1, "Rekord: %d    START = Menü", hs[G_MINES]);
  if (over) overlay(mlost ? "BOOM!" : "GEWONNEN!");
}
static void mnBot(void) {
  static const u32 NC[9] = {0, COL(70,120,255), COL(40,170,60), COL(240,60,60), COL(160,70,220), COL(240,140,30), COL(30,180,180), COL(240,240,240), COL(170,170,170)};
  grad(0, 0, 320, 240, COL(30, 34, 50), COL(14, 16, 26));
  for (int y = 0; y < 8; y++) for (int x = 0; x < 10; x++) {
    float cx = 20 + x * 28, cy = 8 + y * 28;
    if (mr[y][x]) { rect(cx, cy, 27, 27, COL(40, 46, 66)); int n = mnCount(x, y); if (n) T(cx + 13.5f, cy + 13.5f, 0.62, NC[n], 3, "%d", n); }
    else {
      cell(cx, cy, 27, COL(120, 135, 190));
      if (mfl[y][x]) { rect(cx + 12, cy + 6, 3, 15, COL(20, 20, 30)); tri(cx + 14, cy + 6, cx + 14, cy + 14, cx + 6, cy + 10, COL(230, 40, 40)); }
      if (over && mlost && mm[y][x] && !mfl[y][x]) { rect(cx, cy, 27, 27, COL(200, 60, 60)); circ(cx + 13.5f, cy + 13.5f, 7, COL(20, 20, 20)); }
    }
  }
}

// ================= 8) PONG =================
static float pgy, cgy, bx, by, bvx, bvy; static int lives;
static void poServe(int dir) { bx = 200; by = 120; bvx = 3.2f * dir; bvy = (RND(100) - 50) / 25.f; }
static void poInit(void) { score = 0; over = 0; lives = 3; pgy = cgy = 120; poServe(1); }
static void poUpd(void) {
  if (kh & KEY_UP) pgy -= 4; if (kh & KEY_DOWN) pgy += 4;
  pgy -= cp.dy / 40.0f; if (kh & KEY_TOUCH) pgy = tp.py;
  pgy = clampf(pgy, 24, 216);
  float sp = 2.6f + score / 80.0f; if (sp > 4.6f) sp = 4.6f;
  float tg = bvx > 0 ? by : 120; cgy += clampf(tg - cgy, -sp, sp); cgy = clampf(cgy, 24, 216);
  bx += bvx; by += bvy;
  if (by < 5) { by = 5; bvy = -bvy; } if (by > 235) { by = 235; bvy = -bvy; }
  if (bvx < 0 && bx - 5 <= 28 && bx > 10 && fabsf(by - pgy) < 31) { bx = 34; bvx = -bvx * 1.06f; if (bvx > 9) bvx = 9; bvy = (by - pgy) * 0.16f; }
  if (bvx > 0 && bx + 5 >= 372 && bx < 390 && fabsf(by - cgy) < 31) { bx = 366; bvx = -bvx * 1.03f; bvy = (by - cgy) * 0.16f; }
  if (bx < -10) { lives--; if (lives <= 0) over = 1; else poServe(1); }
  if (bx > 410) { score += 10; poServe(-1); }
}
static void poTop(void) {
  grad(0, 0, 400, 240, COL(10, 18, 40), COL(24, 40, 80));
  for (int y = 0; y < 240; y += 20) rect(198, y + 4, 4, 12, COL(70, 90, 140));
  rect(20, pgy - 24, 8, 48, COL(120, 200, 255)); rect(372, cgy - 24, 8, 48, COL(255, 140, 140));
  circ(bx, by, 9, COLA(255, 255, 255, 60)); circ(bx, by, 5, WHITE);
  T(200, 6, 0.8, WHITE, 1, "%d", score);
  for (int i = 0; i < lives; i++) circ(14 + i * 14, 12, 5, COL(240, 80, 100));
  if (over) overlay("GAME OVER");
}
static void poBot(void) { snprintf(xb, sizeof xb, "Leben: %d", lives); infobot("Steuerkreuz, Kreispad oder Touch", xb); }

// ================= 9) BREAKOUT =================
static float pdx, bkx, bky, bkvx, bkvy; static int br[5][10], blive, bstart, blevel, bleft;
static void brInit(void) { for (int r = 0; r < 5; r++) for (int c = 0; c < 10; c++) br[r][c] = 1; bleft = 50; pdx = 200; bstart = 0; bkvx = bkvy = 0; }
static void bkInit(void) { score = 0; over = 0; blive = 3; blevel = 0; brInit(); }
static int bkHit(float x, float y) {
  int r = (int)((y - 30) / 14), c = (int)((x - 5) / 39);
  if (y < 30 || x < 5 || r < 0 || r > 4 || c < 0 || c > 9 || !br[r][c]) return 0;
  br[r][c] = 0; bleft--; score += 10 * (5 - r); return 1;
}
static void bkUpd(void) {
  if (kh & KEY_LEFT) pdx -= 5; if (kh & KEY_RIGHT) pdx += 5;
  pdx += cp.dx / 30.0f; if (kh & KEY_TOUCH) pdx = tp.px * 1.25f; pdx = clampf(pdx, 30, 370);
  if (!bstart) { bkx = pdx; bky = 216; if (kd & (KEY_A | KEY_UP)) { bstart = 1; bkvx = RND(2) ? 2.4f : -2.4f; bkvy = -(3.4f + blevel * 0.3f); } return; }
  bkx += bkvx; if (bkHit(bkx, bky)) bkvx = -bkvx;
  if (bkx < 4) { bkx = 4; bkvx = -bkvx; } if (bkx > 396) { bkx = 396; bkvx = -bkvx; }
  bky += bkvy; if (bkHit(bkx, bky)) bkvy = -bkvy;
  if (bky < 4) { bky = 4; bkvy = -bkvy; }
  if (bkvy > 0 && bky + 4 >= 222 && bky < 232 && fabsf(bkx - pdx) < 34) { bky = 218; bkvy = -fabsf(bkvy); bkvx = clampf((bkx - pdx) * 0.09f, -4.5f, 4.5f); }
  if (bky > 245) { blive--; if (blive <= 0) over = 1; else bstart = 0; }
  if (bleft == 0) { blevel++; score += 50; brInit(); }
}
static void bkTop(void) {
  grad(0, 0, 400, 240, COL(24, 14, 40), COL(60, 24, 60));
  static const u32 rc[5] = {COL(230,60,60), COL(240,140,40), COL(240,210,50), COL(70,200,90), COL(70,140,240)};
  for (int r = 0; r < 5; r++) for (int c = 0; c < 10; c++) if (br[r][c]) cell(5 + c * 39, 30 + r * 14, 38, rc[r]), rect(5 + c * 39, 30 + r * 14 + 12, 38, 2, sh(rc[r], -60));
  rect(pdx - 30, 224, 60, 8, COL(220, 225, 245)); rect(pdx - 30, 224, 60, 2, WHITE);
  circ(bkx, bky, 4, WHITE);
  for (int i = 0; i < blive; i++) circ(14 + i * 14, 12, 5, COL(240, 80, 100));
  T(200, 4, 0.7, WHITE, 1, "%d", score);
  if (!bstart && !over) T(200, 150, 0.5, GOLD, 1, "A = Ball starten");
  if (over) overlay("GAME OVER");
}
static void bkBot(void) { snprintf(xb, sizeof xb, "Leben: %d   Level: %d", blive, blevel + 1); infobot("Steuerkreuz, Kreispad oder Touch", xb); }

// ================= 10) BLACKJACK =================
static int ph[12], pn, dh[12], dn, bst, chips, bmsg;
static int bjCard(void) { return RND(52); }
static int hv(int *h, int n) {
  int s = 0, a = 0;
  for (int i = 0; i < n; i++) { int r = h[i] % 13 + 1, v = r > 10 ? 10 : r; if (r == 1) { a++; v = 11; } s += v; }
  while (s > 21 && a) { s -= 10; a--; }
  return s;
}
static void bjFinish(void) {
  int p = hv(ph, pn); if (p <= 21) while (hv(dh, dn) < 17 && dn < 11) dh[dn++] = bjCard();
  int d = hv(dh, dn);
  if (p > 21) { bmsg = 5; chips -= 10; }
  else if (p == 21 && pn == 2 && !(d == 21 && dn == 2)) { bmsg = 4; chips += 15; }
  else if (d > 21 || p > d) { bmsg = 1; chips += 10; }
  else if (p < d) { bmsg = 2; chips -= 10; }
  else bmsg = 3;
  bst = 1; score = chips - 100; if (chips <= 0) over = 1;
}
static void bjDeal(void) {
  pn = dn = 0; ph[pn++] = bjCard(); dh[dn++] = bjCard(); ph[pn++] = bjCard(); dh[dn++] = bjCard(); bst = 0; bmsg = 0;
  if (hv(ph, pn) == 21) bjFinish();
}
static void bjInit(void) { chips = 100; over = 0; score = 0; bjDeal(); }
static void bjUpd(void) {
  int t = (kd & KEY_TOUCH) != 0;
  if (bst == 0) {
    int hit = (kd & KEY_A) || (t && tp.py >= 150 && tp.px < 160);
    int st = (kd & KEY_B) || (t && tp.py >= 150 && tp.px >= 160);
    if (hit) { ph[pn++] = bjCard(); if (hv(ph, pn) >= 21 || pn >= 10) bjFinish(); }
    else if (st) bjFinish();
  } else if (kd & (KEY_A | KEY_TOUCH)) bjDeal();
}
static void suit(float x, float y, int s, float r) {
  u32 red = COL(210, 30, 40), blk = COL(20, 20, 30);
  if (s == 0) { circ(x - r * .5f, y - r * .3f, r * .55f, red); circ(x + r * .5f, y - r * .3f, r * .55f, red); tri(x - r * 1.02f, y - r * .05f, x + r * 1.02f, y - r * .05f, x, y + r, red); }
  else if (s == 1) { tri(x - r * .8f, y, x + r * .8f, y, x, y - r, red); tri(x - r * .8f, y, x + r * .8f, y, x, y + r, red); }
  else if (s == 2) { tri(x - r * .95f, y + r * .2f, x + r * .95f, y + r * .2f, x, y - r, blk); circ(x - r * .5f, y + r * .2f, r * .5f, blk); circ(x + r * .5f, y + r * .2f, r * .5f, blk); rect(x - r * .12f, y + r * .3f, r * .24f, r * .7f, blk); }
  else { circ(x, y - r * .45f, r * .5f, blk); circ(x - r * .5f, y + r * .2f, r * .5f, blk); circ(x + r * .5f, y + r * .2f, r * .5f, blk); rect(x - r * .12f, y + r * .2f, r * .24f, r * .8f, blk); }
}
static void card(float x, float y, int c, int hid) {
  if (hid) { rect(x, y, 44, 62, COL(20, 30, 100)); grad(x + 2, y + 2, 40, 58, COL(80, 100, 230), COL(40, 50, 150)); rect(x + 7, y + 7, 30, 48, COL(30, 40, 130)); return; }
  static const char *rk[13] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};
  int r = c % 13, s = c / 13; u32 col = s < 2 ? COL(210, 30, 40) : COL(20, 20, 30);
  rect(x, y, 44, 62, COL(110, 115, 125)); rect(x + 1, y + 1, 42, 60, COL(252, 252, 252));
  T(x + 4, y + 1, 0.5, col, 0, "%s", rk[r]); suit(x + 22, y + 38, s, 10);
}
static void bjTop(void) {
  grad(0, 0, 400, 240, COL(16, 96, 54), COL(8, 52, 30));
  int dv = bst ? hv(dh, dn) : hv(dh, 1), pv = hv(ph, pn);
  T(12, 8, 0.55, WHITE, 0, "Dealer: %d", dv);
  float dx0 = 200 - (dn * 48 - 4) / 2.f; for (int i = 0; i < dn; i++) card(dx0 + i * 48, 34, dh[i], bst == 0 && i == 1);
  T(12, 134, 0.55, WHITE, 0, "Du: %d", pv);
  float px0 = 200 - (pn * 48 - 4) / 2.f; for (int i = 0; i < pn; i++) card(px0 + i * 48, 164, ph[i], 0);
  static const char *msg[6] = {"", "Gewonnen! +10", "Verloren -10", "Unentschieden", "Blackjack! +15", "Überkauft! -10"};
  if (bmsg) T(200, 104, 0.7, GOLD, 1, "%s", msg[bmsg]);
  if (over) overlay("PLEITE!");
}
static void bjBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  T(160, 12, 0.8, ACC[G_BJ], 1, "Blackjack");
  T(160, 56, 0.65, GOLD, 1, "Chips: %d", chips);
  T(160, 90, 0.45, GRAY, 1, "Einsatz: 10   Gewinn-Rekord: %d", hs[G_BJ]);
  if (bst == 0) { btn(10, 150, 145, 80, COL(50, 170, 90), 0); TT(82, 190, 0.7, WHITE, "Karte (A)"); btn(165, 150, 145, 80, COL(200, 70, 70), 0); TT(237, 190, 0.7, WHITE, "Halten (B)"); }
  else { btn(10, 150, 300, 80, COL(70, 130, 230), 0); TT(160, 190, 0.7, WHITE, "Neue Runde (A)"); }
  T(160, 120, 0.4, COL(120, 125, 150), 1, "START = Menü");
}

// ================= Spieltabelle =================
typedef struct { void (*init)(void); void (*upd)(void); void (*top)(void); void (*bot)(void); } Game;
static const Game G[NG] = {
  {snInit, snUpd, snTop, snBot}, {g2Init, g2Upd, g2Top, g2Bot}, {tInit, tUpd, tTop, tBot},
  {meInit, meUpd, meTop, meBot}, {reInit, reUpd, reTop, reBot}, {flInit, flUpd, flTop, flBot},
  {mnInit, mnUpd, mnTop, mnBot}, {poInit, poUpd, poTop, poBot}, {bkInit, bkUpd, bkTop, bkBot},
  {bjInit, bjUpd, bjTop, bjBot}};

static void loadhs(void) {
  FILE *f = fopen(SAVEFILE, "rb"); if (f) { if (fread(hs, sizeof hs, 1, f) != 1) memset(hs, 0, sizeof hs); fclose(f); }
  for (int i = 0; i < NG; i++) if (hs[i] < 0 || hs[i] > 10000000) hs[i] = 0;
}
static void savehs(void) {
  if (cur < 0 || score <= hs[cur]) return;
  hs[cur] = score; FILE *f = fopen(SAVEFILE, "wb"); if (f) { fwrite(hs, sizeof hs, 1, f); fclose(f); }
}
static void begin(int i) { cur = i; srand((unsigned)time(NULL) + tick); G[cur].init(); sv = 0; }

// ================= Menü =================
static void menuUpd(void) {
  if (kd & KEY_DOWN) sel = (sel + 2) % NG; if (kd & KEY_UP) sel = (sel + NG - 2) % NG;
  if (kd & KEY_RIGHT) sel = (sel + 1) % NG; if (kd & KEY_LEFT) sel = (sel + NG - 1) % NG;
  if (kd & KEY_TOUCH) for (int i = 0; i < NG; i++) {
    float x = 8 + (i % 2) * 156, y = 6 + (i / 2) * 46;
    if (tp.px >= x && tp.px < x + 148 && tp.py >= y && tp.py < y + 40) { sel = i; begin(i); return; }
  }
  if (kd & KEY_A) begin(sel);
}
static void menuTop(void) {
  grad(0, 0, 400, 240, COL(18, 20, 50), COL(62, 30, 94));
  for (int i = 0; i < 14; i++) {
    float x = fmodf(i * 61.f + tick * (0.25f + (i % 4) * 0.15f), 440.f) - 20, y = 10 + (i * 47) % 220 + sinf(tick * 0.03f + i) * 8, s = 14 + (i % 3) * 6;
    rect(x, y, s, s, (ACC[i % NG] & 0x00FFFFFF) | 0x50000000);
  }
  T(202, 20, 1.5, COL(0, 0, 0), 1, "MINI ARCADE"); T(200, 18, 1.5, WHITE, 1, "MINI ARCADE");
  T(200, 74, 0.5, GRAY, 1, "10 Spiele in einer App");
  rect(40, 108, 320, 104, COLA(0, 0, 0, 120));
  T(200, 114, 0.95, ACC[sel], 1, "%s", NAME[sel]);
  T(200, 152, 0.5, WHITE, 1, "%s", DESC[sel]);
  T(200, 180, 0.5, GOLD, 1, "Rekord: %d", hs[sel]);
  T(200, 222, 0.38, GRAY, 1, "A / Tippen = Starten      START = Beenden");
}
static void menuBot(void) {
  grad(0, 0, 320, 240, COL(24, 28, 48), COL(10, 12, 24));
  for (int i = 0; i < NG; i++) { float x = 8 + (i % 2) * 156, y = 6 + (i / 2) * 46; btn(x, y, 148, 40, ACC[i], i == sel); TT(x + 74, y + 20, 0.62, WHITE, NAME[i]); }
}

int main(void) {
  gfxInitDefault(); C3D_Init(C3D_DEFAULT_CMDBUF_SIZE); C2D_Init(C2D_DEFAULT_MAX_OBJECTS); C2D_Prepare();
  C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT), *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
  tbuf = C2D_TextBufNew(8192); srand((unsigned)time(NULL)); loadhs();
  while (aptMainLoop()) {
    hidScanInput(); kd = hidKeysDown(); kh = hidKeysHeld(); hidTouchRead(&tp); hidCircleRead(&cp); tick++;
    if (kd & KEY_START) { if (cur >= 0) { savehs(); cur = -1; } else break; }
    if (cur < 0) menuUpd();
    else if (over) { if (kd & KEY_A) begin(cur); }
    else { G[cur].upd(); if (over && !sv) { sv = 1; savehs(); } }
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW); C2D_TextBufClear(tbuf);
    C2D_TargetClear(top, COL(0, 0, 0)); C2D_SceneBegin(top); if (cur < 0) menuTop(); else G[cur].top();
    C2D_TargetClear(bot, COL(0, 0, 0)); C2D_SceneBegin(bot); if (cur < 0) menuBot(); else G[cur].bot();
    C3D_FrameEnd(0);
  }
  C2D_TextBufDelete(tbuf); C2D_Fini(); C3D_Fini(); gfxExit();
  return 0;
}
