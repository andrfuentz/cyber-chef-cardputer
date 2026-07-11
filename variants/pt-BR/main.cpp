#include <M5Cardputer.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <vector>
#include <lgfx/utility/lgfx_qrcode.h>
#include "chef_splash.h"

#define FW_VERSION "v1.1"
#define RECIPE_DIR "/RECEITAS"
#define IMPORT_DIR "/IMPORTAR"
#define BACKUP_FILE "/MISEDECK_BACKUP.txt"
#define MAX_VISIBLE 3

// Ajuste estes pinos se seu modelo de Cardputer usar pinagem diferente.
#define SD_CS_PIN   12
#define SD_SCK_PIN  40
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14

const uint16_t AMBER = 0xFD20;
const uint16_t BG = 0x0000;
const uint16_t AMBER_DIM = 0x7A80;
const uint16_t AMBER_DARK = 0x3180;
const uint16_t UI_BLACK = 0x0000;

SPIClass sdSPI(FSPI);
bool sdOK = false;
bool soundOn = true;
int soundVolume = 6;
WebServer webServer(80);
Preferences prefs;
bool webStarted = false;
bool mdnsStarted = false;
String wifiSsid = "";
String wifiPass = "";

int batteryPct = -1;
int batteryMv = -1;
unsigned long batteryLastRead = 0;

struct Ingredient {
  String name;
  float weight;
};

struct Prep {
  String name;
  std::vector<Ingredient> ingredients;
  float total = 0;
};

struct Recipe {
  String id;
  String name;
  String category;
  // Caminho interno no SD. Nao faz parte do formato TXT da receita.
  String storagePath;
  bool favorite = false;
  bool composite = false;
  std::vector<Ingredient> ingredients;
  std::vector<Prep> preps;
  float total = 0;
};

std::vector<Recipe> recipes;
const char* cats[] = {"FAVORITAS", "PRATOS", "MASSAS", "DOCES", "BEBIDAS", "MOLHOS", "OUTROS"};
const int CAT_COUNT = 7;
const char* realCats[] = {"PRATOS", "MASSAS", "DOCES", "BEBIDAS", "MOLHOS", "OUTROS"};
const int REAL_CAT_COUNT = 6;

String currentCategory = "FAVORITAS";
int selected = 0;
int page = 0;
int recipeIndex = -1;
int prepIndex = -1;
int ingIndex = -1;
String inputBuf = "";
int inputCursor = 0;
String messageText = "";
unsigned long messageUntil = 0;

unsigned long timerDeadline = 0;
unsigned long timerLeft = 0;
bool timerRunning = false;
bool timerPaused = false;
bool timerAlarmActive = false;
unsigned long timerAlarmLast = 0;
unsigned long timerAlarmUntil = 0;
int timerAlarmStep = 0;

struct Pending {
  String title;
  String label;
  String info;
  int mode = 0;
  float number = 0;
  String text;
  bool yesNo = false;
} pending;

struct KeyEvent {
  bool enter=false, back=false, del=false, tab=false, up=false, down=false, left=false, right=false;
  char ch=0;
};

enum State {
  ST_MAIN,
  ST_RECEITAS,
  ST_CATEGORIES,
  ST_RECIPE_LIST,
  ST_RECIPE_VIEW,
  ST_SHARE_QR,
  ST_PREP_VIEW,
  ST_ACTIONS,
  ST_PREP_ACTIONS,
  ST_PREP_MENU,
  ST_ING_MENU,
  ST_ING_LIST,
  ST_TEXT_INPUT,
  ST_NUM_INPUT,
  ST_CONFIRM,
  ST_NEW_TYPE,
  ST_NEW_CAT,
  ST_NEW_PREP_NEXT,
  ST_ADD_ING_NAME,
  ST_ADD_ING_WEIGHT,
  ST_QUICK,
  ST_QUICK_TARGET,
  ST_QUICK_RESULT,
  ST_FERRAMENTAS,
  ST_TIMER_MENU,
  ST_TIMER_CUSTOM,
  ST_TIMER_RUN,
  ST_TIMER_DONE,
  ST_CONV,
  ST_TEMP_DIR,
  ST_WEIGHT_FROM,
  ST_WEIGHT_TO,
  ST_RESULT,
  ST_WIFI,
  ST_WIFI_SSID,
  ST_WIFI_PASS,
  ST_WIFI_STATUS,
  ST_SISTEMA,
  ST_STATUS,
  ST_BATTERY,
  ST_ABOUT,
  ST_SOUND
};

State state = ST_MAIN;
State previousState = ST_MAIN;
State inputReturn = ST_MAIN;
State confirmReturn = ST_MAIN;
void (*inputNumberCallback)(float) = nullptr;
void (*inputTextCallback)(String) = nullptr;
void (*confirmCallback)() = nullptr;

std::vector<float> quickWeights;
float quickOriginal = 0;
float quickTarget = 0;

Recipe draft;
bool hasDraft = false;
bool addingExistingPrep = false;
int aboutEggStep = 0;

String nowId() { return String("r") + String(millis(), HEX); }
String cleanName(String s) {
  s.trim();
  s.toUpperCase();
  String out;
  for (int i=0;i<s.length();i++) {
    char c = s[i];
    if ((c>='A'&&c<='Z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-' || c=='.') out += c;
  }
  out.trim();
  if (out.length() > 22) out = out.substring(0,22);
  return out;
}
String cleanInput(String s) {
  s.toUpperCase();
  String out;
  for (int i=0;i<s.length();i++) {
    char c=s[i];
    if ((c>='A'&&c<='Z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-' || c=='.') out += c;
  }
  if (out.length()>22) out=out.substring(0,22);
  return out;
}
String safeFileKey(String s) {
  s.trim();
  String out;
  for (int i=0;i<s.length();i++) {
    char c=s[i];
    if ((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-') out += c;
  }
  if (!out.length()) out=nowId();
  return out;
}
String recipeDirPath(String path) {
  if (path.startsWith("/")) return path;
  return String(RECIPE_DIR) + "/" + path;
}
String newRecipePath(const Recipe& r) {
  String base=safeFileKey(r.id.length()?r.id:r.name);
  String path=String(RECIPE_DIR)+"/"+base+".txt";
  for (int n=2; SD.exists(path); n++) path=String(RECIPE_DIR)+"/"+base+"_"+String(n)+".txt";
  return path;
}
float r1(float v) { return roundf(v * 10.0f) / 10.0f; }
String fw(float v) { return String(r1(v), 1) + "g"; }
float sumIngredients(const std::vector<Ingredient>& ing) {
  float t=0; for (auto &i: ing) t += i.weight; return r1(t);
}
void recalc(Recipe &r) {
  if (!r.composite) r.total = sumIngredients(r.ingredients);
  else {
    float t=0;
    for (auto &p: r.preps) { p.total = sumIngredients(p.ingredients); t += p.total; }
    r.total = r1(t);
  }
}
void scaleIngredients(std::vector<Ingredient>& ing, float newTotal) {
  float old = sumIngredients(ing);
  if (old <= 0.0f) return;
  float f = newTotal / old;
  for (auto &i: ing) i.weight = r1(i.weight * f);
}
void scaleRecipe(Recipe &r, float newTotal) {
  if (!r.composite) { scaleIngredients(r.ingredients, newTotal); r.total = r1(newTotal); }
  else {
    float old = r.total;
    if (old <= 0) return;
    float f = newTotal / old;
    for (auto &p: r.preps) { scaleIngredients(p.ingredients, p.total * f); p.total = sumIngredients(p.ingredients); }
    recalc(r);
  }
}
void scalePrep(Prep &p, float newTotal) { scaleIngredients(p.ingredients, newTotal); p.total = sumIngredients(p.ingredients); }

int speakerVolumeValue() {
  if (!soundOn || soundVolume <= 0) return 0;
  return 24 + soundVolume * 16;
}
void applySoundVolume() {
  M5Cardputer.Speaker.setVolume(speakerVolumeValue());
}
void toneMs(int hz, int ms) {
  if (!soundOn || soundVolume <= 0 || hz <= 0) return;
  applySoundVolume();
  M5Cardputer.Speaker.tone(hz, ms);
}
void beepNav() { toneMs(760, 14); }
void beepBack() { toneMs(430, 28); }
void beepOK() { toneMs(980, 28); delay(24); toneMs(1320, 34); }
void beepErr() { toneMs(240, 70); delay(55); toneMs(180, 80); }
void beepRecipe() { toneMs(880, 55); delay(60); toneMs(1175, 60); delay(65); toneMs(1568, 85); }
void beepWifiOK() { toneMs(660, 45); delay(50); toneMs(990, 45); delay(50); toneMs(1320, 70); }
void beepWifiErr() { toneMs(330, 70); delay(65); toneMs(220, 90); }
static const uint16_t OPENING_NOTES[] = {1568, 440, 466, 784, 220, 233};
static const uint16_t OPENING_DURATIONS[] = {610, 610, 1220, 610, 610, 1220};
static const uint16_t OPENING_NOTE_GAP_MS = 20;
static const uint8_t OPENING_VOLUME = 96;
static const uint8_t OPENING_REPEATS = 2;
void startTimerAlarm() {
  timerAlarmActive = true;
  timerAlarmLast = 0;
  timerAlarmUntil = millis() + 60000UL;
  timerAlarmStep = 0;
}
void stopTimerAlarm() {
  timerAlarmActive = false;
}
void updateTimerAlarm() {
  if (!timerAlarmActive) return;
  if (!soundOn || soundVolume <= 0 || millis() > timerAlarmUntil) { stopTimerAlarm(); return; }
  static const int notes[] = {1568, 1175, 1568, 1760, 1568, 1175, 880, 0};
  static const int durs[]  = {110,  110,  110,  170,  110,  110,  240, 180};
  int count = sizeof(notes) / sizeof(notes[0]);
  if (!timerAlarmLast || millis() - timerAlarmLast >= (unsigned long)durs[timerAlarmStep]) {
    if (notes[timerAlarmStep] > 0) toneMs(notes[timerAlarmStep], durs[timerAlarmStep] - 10);
    timerAlarmLast = millis();
    timerAlarmStep = (timerAlarmStep + 1) % count;
  }
}

void drawText(String s, bool big=false) {
  M5Cardputer.Display.fillScreen(BG);
  M5Cardputer.Display.setTextColor(AMBER, BG);
  M5Cardputer.Display.setTextSize(big ? 2 : 1);
  M5Cardputer.Display.setCursor(2, 2);
  M5Cardputer.Display.print(s);
}

int sw() { return M5Cardputer.Display.width(); }
int sh() { return M5Cardputer.Display.height(); }
void updateBattery(bool force=false) {
  if (!force && batteryLastRead && millis() - batteryLastRead < 5000) return;
  batteryLastRead = millis();
  batteryMv = M5Cardputer.Power.getBatteryVoltage();
  batteryPct = -1;
  if (batteryMv >= 3200 && batteryMv <= 4300) {
    batteryPct = (batteryMv - 3300) * 100 / 900;
    if (batteryPct < 0) batteryPct = 0;
    if (batteryPct > 100) batteryPct = 100;
  }
}
String batteryLabel() {
  updateBattery();
  if (batteryPct < 0) return "--%";
  return String(batteryPct) + "%";
}
String clipText(String s, int maxChars) {
  if (maxChars < 1) return "";
  if ((int)s.length() <= maxChars) return s;
  if (maxChars <= 2) return s.substring(0, maxChars);
  return s.substring(0, maxChars - 1) + "~";
}
void setInput(String s) {
  inputBuf = s;
  inputCursor = inputBuf.length();
}
char cleanInputChar(char c) {
  if (c>='a'&&c<='z') c -= 32;
  if ((c>='A'&&c<='Z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-' || c=='.') return c;
  return 0;
}
String inputWithCursor(String s) {
  if (inputCursor < 0) inputCursor = 0;
  if (inputCursor > (int)s.length()) inputCursor = s.length();
  String out = s.substring(0, inputCursor) + "|" + s.substring(inputCursor);
  int maxChars = 18;
  if ((int)out.length() <= maxChars) return out;
  int start = max(0, inputCursor - maxChars/2);
  if (start + maxChars > (int)out.length()) start = max(0, (int)out.length() - maxChars);
  return out.substring(start, min((int)out.length(), start + maxChars));
}
void uiText(int size=1, uint16_t fg=AMBER, uint16_t bg=BG) {
  M5Cardputer.Display.setTextSize(size);
  M5Cardputer.Display.setTextColor(fg, bg);
}
void uiFrame(String title, String foot=";/ . NAV  OK ENTRA  ` VOLTA") {
  M5Cardputer.Display.fillScreen(BG);
  uiText(1, AMBER, BG);
  title.toUpperCase();
  M5Cardputer.Display.setCursor(4, 3);
  M5Cardputer.Display.print(clipText(title, 20));
  String bat = batteryLabel();
  uiText(1, (batteryPct >= 0 && batteryPct < 15) ? AMBER : AMBER_DIM, BG);
  M5Cardputer.Display.setCursor(sw() - 6 - bat.length()*6, 3);
  M5Cardputer.Display.print(bat);
  M5Cardputer.Display.drawLine(0, 15, sw(), 15, AMBER_DARK);
  M5Cardputer.Display.drawLine(0, sh()-14, sw(), sh()-14, AMBER_DARK);
  uiText(1, AMBER_DIM, BG);
  M5Cardputer.Display.setCursor(4, sh()-11);
  M5Cardputer.Display.print(clipText(foot, 38));
}
void uiMenu(String title, const std::vector<String>& items, String foot=";/ . NAV  OK ENTRA  ` VOLTA") {
  uiFrame(title, foot);
  int total = items.size();
  int start = page * MAX_VISIBLE;
  int rowH = 31;
  int y0 = 20;
  for (int i=0; i<MAX_VISIBLE && start+i<total; i++) {
    int y = y0 + i*rowH;
    bool sel = (i == selected);
    if (sel) {
      M5Cardputer.Display.fillRect(3, y-2, sw()-6, rowH-4, AMBER);
      M5Cardputer.Display.drawRect(3, y-2, sw()-6, rowH-4, AMBER);
      uiText(2, UI_BLACK, AMBER);
    } else {
      M5Cardputer.Display.drawRect(3, y-2, sw()-6, rowH-4, AMBER_DARK);
      uiText(2, AMBER, BG);
    }
    M5Cardputer.Display.setCursor(9, y+4);
    M5Cardputer.Display.print(clipText(items[start+i], 18));
  }
  if (total > MAX_VISIBLE) {
    uiText(1, AMBER_DIM, BG);
    M5Cardputer.Display.setCursor(sw()-45, 3);
    M5Cardputer.Display.print(String(page+1)+"/"+String((total+MAX_VISIBLE-1)/MAX_VISIBLE));
  }
}
void uiPanel(String title, String body, String foot="` VOLTA", int size=1) {
  uiFrame(title, foot);
  uiText(size, AMBER, BG);
  M5Cardputer.Display.setCursor(5, 22);
  M5Cardputer.Display.print(body);
}
void uiInput(String title, String label, String value, String info="", bool numeric=false) {
  uiFrame(title, numeric ? "DIGITE NUM  OK CONFIRMA  ` VOLTA" : "DIGITE TEXTO  OK CONFIRMA  ` VOLTA");
  uiText(1, AMBER_DIM, BG);
  M5Cardputer.Display.setCursor(5, 22);
  if (info.length()) {
    M5Cardputer.Display.print(clipText(info, 32));
    M5Cardputer.Display.setCursor(5, 35);
  }
  M5Cardputer.Display.print(label + ":");
  M5Cardputer.Display.drawRect(4, 50, sw()-8, 40, AMBER_DARK);
  uiText(2, AMBER, BG);
  M5Cardputer.Display.setCursor(10, 61);
  M5Cardputer.Display.print(inputWithCursor(value));
}
void uiTimer() {
  unsigned long remain = timerPaused ? timerLeft : (timerDeadline>millis()?timerDeadline-millis():0);
  int sec=remain/1000; char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d", sec/60, sec%60);
  uiFrame("TIMER", "OK PAUSA/CONTINUA  ` CANCELA");
  uiText(4, AMBER, BG);
  M5Cardputer.Display.setCursor(55, 35);
  M5Cardputer.Display.print(buf);
  uiText(2, timerPaused ? AMBER_DIM : AMBER, BG);
  M5Cardputer.Display.setCursor(55, 84);
  M5Cardputer.Display.print(timerPaused ? "PAUSADO" : "RODANDO");
}
void uiListRows(String title, const std::vector<String>& rows, String foot=";/ . NAV  OK ENTRA  ` VOLTA") {
  uiFrame(title, foot);
  int start=page*MAX_VISIBLE;
  for (int i=0; i<MAX_VISIBLE && start+i<(int)rows.size(); i++) {
    int y=22+i*29;
    bool sel=(i==selected);
    if(sel) M5Cardputer.Display.fillRect(4, y-3, sw()-8, 24, AMBER_DARK);
    uiText(1, AMBER, sel ? AMBER_DARK : BG);
    M5Cardputer.Display.setCursor(8, y);
    M5Cardputer.Display.print(clipText(rows[start+i], 36));
  }
}
void uiBatteryScreen() {
  updateBattery(true);
  uiFrame("BATERIA", "OK ATUALIZA  ` VOLTA");
  int pct = batteryPct < 0 ? 0 : batteryPct;
  if (pct > 100) pct = 100;
  uiText(4, AMBER, BG);
  M5Cardputer.Display.setCursor(42, 24);
  M5Cardputer.Display.print(batteryPct < 0 ? "--%" : String(batteryPct) + "%");
  int x=22, y=70, w=196, h=22;
  M5Cardputer.Display.drawRect(x, y, w, h, AMBER);
  M5Cardputer.Display.fillRect(x+2, y+2, max(0, (w-4) * pct / 100), h-4, AMBER);
  uiText(2, AMBER, BG);
  M5Cardputer.Display.setCursor(50, 101);
  M5Cardputer.Display.print(batteryMv > 0 ? String(batteryMv / 1000.0f, 2) + "V" : "-- V");
}
void uiSoundScreen() {
  uiFrame("SOM", ";/ . NAV  OK ALTERA  ` VOLTA");
  std::vector<String> items = {"SOM: " + String(soundOn ? "LIGADO" : "DESLIGADO"), "VOLUME +", "VOLUME -", "TESTAR", "MUDO RAPIDO"};
  int start = page * MAX_VISIBLE;
  for (int i=0; i<MAX_VISIBLE && start+i<(int)items.size(); i++) {
    int y = 20 + i * 22;
    bool sel = (i == selected);
    if (sel) M5Cardputer.Display.fillRect(4, y-3, sw()-8, 18, AMBER_DARK);
    uiText(1, AMBER, sel ? AMBER_DARK : BG);
    M5Cardputer.Display.setCursor(8, y);
    M5Cardputer.Display.print(clipText(items[start+i], 34));
  }
  if ((int)items.size() > MAX_VISIBLE) {
    uiText(1, AMBER_DIM, BG);
    M5Cardputer.Display.setCursor(sw()-45, 3);
    M5Cardputer.Display.print(String(page+1)+"/"+String(((int)items.size()+MAX_VISIBLE-1)/MAX_VISIBLE));
  }
  int x=34, y=108, w=172, h=12;
  uiText(1, AMBER_DIM, BG);
  M5Cardputer.Display.setCursor(8, 94);
  M5Cardputer.Display.print("VOL " + String(soundVolume) + "/10");
  M5Cardputer.Display.drawRect(x, y, w, h, AMBER);
  M5Cardputer.Display.fillRect(x+2, y+2, max(0, (w-4) * soundVolume / 10), h-4, AMBER);
}
String header(String t) {
  if (t.length() > 18) t = t.substring(0,18);
  while (t.length() < 18) t += " ";
  return t + "/\n--------------------\n";
}
String menuLines(const std::vector<String>& items) {
  String out;
  int start = page * MAX_VISIBLE;
  for (int i=0;i<MAX_VISIBLE && start+i<(int)items.size();i++) {
    out += (i==selected ? "> " : "  ");
    out += items[start+i].substring(0,18);
    out += "\n";
  }
  return out;
}
void resetNav() { selected=0; page=0; }
int gi() { return page*MAX_VISIBLE+selected; }
void render();
String wifiIp();
void connectWifiNow();
void disconnectWifi();
void editWifiInput(const KeyEvent& e, bool password);
void flash(String m, bool err=false);
void go(State s) { state=s; resetNav(); render(); }
void goKeep(State s) { state=s; render(); }
void finishTimer() {
  timerRunning = false;
  timerPaused = false;
  startTimerAlarm();
  go(ST_TIMER_DONE);
}

void ensureDirs() {
  if (!sdOK) return;
  if (!SD.exists(RECIPE_DIR)) SD.mkdir(RECIPE_DIR);
  if (!SD.exists(IMPORT_DIR)) SD.mkdir(IMPORT_DIR);
}

String recipeToTxt(const Recipe& r) {
  String out = r.name + "\n";
  out += "CATEGORIA: " + r.category + "\n";
  out += String("FAVORITA: ") + (r.favorite ? "SIM" : "NAO") + "\n";
  out += "TOTAL: " + String(r.total,1) + "\n\n";
  if (!r.composite) {
    out += "[INGREDIENTES]\n\n";
    for (auto &i: r.ingredients) out += i.name + "|" + String(i.weight,1) + "|g\n";
  } else {
    out += "TIPO: COMPOSTA\n\n";
    for (auto &p: r.preps) {
      out += "[PREPARO]\n";
      out += "NOME: " + p.name + "\n";
      out += "TOTAL: " + String(p.total,1) + "\n";
      for (auto &i: p.ingredients) out += i.name + "|" + String(i.weight,1) + "|g\n";
      out += "\n";
    }
  }
  return out;
}

String shareCardText(const Recipe& r) {
  String out = r.name + "\n";
  out += "TOTAL:" + String(r.total,1) + "g\n";
  String line = "";
  int count = 0;
  int omitted = 0;
  auto addIng = [&](const Ingredient& ing) {
    String part = ing.name + " " + String(ing.weight,1) + "g";
    if (count >= 8 || out.length() + line.length() + part.length() + 6 > 210) { omitted++; return; }
    if (count > 0) line += "-";
    line += part;
    count++;
  };
  if (!r.composite) {
    for (auto &ing : r.ingredients) addIng(ing);
  } else {
    for (auto &p : r.preps) for (auto &ing : p.ingredients) addIng(ing);
  }
  out += line;
  if (omitted > 0) out += "-+" + String(omitted);
  if (out.length() > 220) out = out.substring(0, 217) + "...";
  return out;
}

bool buildQrCode(QRCode &qr, uint8_t *buf, String text) {
  for (int version=8; version>=4; version--) {
    int8_t ok = lgfx_qrcode_initText(&qr, buf, version, ECC_LOW, text.c_str());
    if (ok == 0) return true;
  }
  return false;
}

void uiShareQr() {
  if (recipeIndex < 0 || recipeIndex >= (int)recipes.size()) { uiPanel("COMPARTILHAR", "RECEITA INVALIDA", "` VOLTA", 1); return; }
  Recipe &r = recipes[recipeIndex];
  String payload = shareCardText(r);
  QRCode qr;
  static uint8_t qrbuff[1024];
  if (!buildQrCode(qr, qrbuff, payload)) {
    uiPanel("COMPARTILHAR", "RECEITA GRANDE\nDEMAIS PARA QR\nRESUMIDO.", "` VOLTA", 1);
    return;
  }
  M5Cardputer.Display.fillScreen(BG);
  int quiet = 4;
  int scale = 2;
  int qrPix = (qr.size + quiet*2) * scale;
  int x0 = max(0, (sw() - qrPix) / 2);
  int y0 = max(0, (sh() - qrPix - 12) / 2);
  M5Cardputer.Display.fillRect(x0, y0, qrPix, qrPix, 0xFFFF);
  for (int y=0; y<qr.size; y++) {
    for (int x=0; x<qr.size; x++) {
      if (lgfx_qrcode_getModule(&qr, x, y)) {
        M5Cardputer.Display.fillRect(x0 + (x + quiet)*scale, y0 + (y + quiet)*scale, scale, scale, UI_BLACK);
      }
    }
  }
  uiText(1, AMBER_DIM, BG);
  String back = "` VOLTAR";
  M5Cardputer.Display.setCursor(sw() - 6 - back.length()*6, sh()-11);
  M5Cardputer.Display.print(back);
}

Recipe txtToRecipe(String txt) {
  // Versoes antigas abriam FILE_WRITE em modo append. Se isso ocorreu,
  // a ultima copia completa e a versao mais recente da receita.
  int lastHeader=max(txt.lastIndexOf("MISEDECK RECIPE"), txt.lastIndexOf("CYBER CHEF RECIPE"));
  if (lastHeader>0) txt=txt.substring(lastHeader);
  Recipe r; r.id = nowId(); r.name="SEM NOME"; r.category="OUTROS";
  int pos=0; Prep *currentPrep = nullptr; bool inIng=false;
  bool sawHeader=false;
  while (pos < txt.length()) {
    int nl = txt.indexOf('\n', pos);
    if (nl < 0) nl = txt.length();
    String line = txt.substring(pos, nl); line.trim(); pos = nl+1;
    if (!line.length()) continue;
    if (line == "MISEDECK RECIPE" || line == "CYBER CHEF RECIPE") { sawHeader=true; continue; }
    if (line == "[INGREDIENTES]") { inIng=true; currentPrep=nullptr; continue; }
    if (line == "[PREPARO]") { r.composite=true; r.preps.push_back(Prep()); currentPrep=&r.preps.back(); inIng=false; continue; }
    if (line.indexOf('|') > 0) {
      int a=line.indexOf('|'), b=line.indexOf('|', a+1);
      Ingredient ing; ing.name=cleanName(line.substring(0,a)); ing.weight=line.substring(a+1,b).toFloat();
      if (currentPrep) currentPrep->ingredients.push_back(ing); else r.ingredients.push_back(ing);
      continue;
    }
    int c=line.indexOf(':');
    if (c<0) {
      if (!sawHeader && r.name=="SEM NOME") r.name=cleanName(line);
      continue;
    }
    String k=line.substring(0,c); String v=line.substring(c+1); k.trim(); v.trim();
    if (currentPrep && k=="NOME") currentPrep->name=cleanName(v);
    else if (k=="TIPO") r.composite = (v == "COMPOSTA");
    else if (k=="ID") r.id=v;
    else if (k=="NOME") r.name=cleanName(v);
    else if (k=="CATEGORIA") r.category=cleanName(v);
    else if (k=="FAVORITA") r.favorite=(v=="SIM");
  }
  recalc(r);
  return r;
}

bool saveRecipe(Recipe& r) {
  if (!sdOK) return false;
  ensureDirs();
  String target=r.storagePath.length()?recipeDirPath(r.storagePath):newRecipePath(r);
  String temp=target+".tmp";
  String backup=target+".bak";
  String txt=recipeToTxt(r);

  SD.remove(temp);
  File f=SD.open(temp, FILE_WRITE);
  if (!f) return false;
  size_t written=f.print(txt);
  f.flush();
  f.close();
  if (written!=txt.length()) { SD.remove(temp); return false; }

  SD.remove(backup);
  bool hadTarget=SD.exists(target);
  if (hadTarget && !SD.rename(target, backup)) { SD.remove(temp); return false; }
  if (!SD.rename(temp, target)) {
    if (hadTarget) SD.rename(backup, target);
    SD.remove(temp);
    return false;
  }
  if (hadTarget) SD.remove(backup);
  r.storagePath=target;
  return true;
}
void saveAll() {
  if (!sdOK) return;
  ensureDirs();
  for (auto &r: recipes) saveRecipe(r);
}
void loadRecipes() {
  recipes.clear();
  if (!sdOK) return;
  ensureDirs();
  // Recupera uma gravacao interrompida entre a criacao do temporario e a troca.
  std::vector<String> temps, backups;
  File recoveryDir=SD.open(RECIPE_DIR);
  File recoveryFile=recoveryDir.openNextFile();
  while (recoveryFile) {
    String path=recipeDirPath(recoveryFile.name());
    if (!recoveryFile.isDirectory() && path.endsWith(".txt.tmp")) temps.push_back(path);
    else if (!recoveryFile.isDirectory() && path.endsWith(".txt.bak")) backups.push_back(path);
    recoveryFile.close();
    recoveryFile=recoveryDir.openNextFile();
  }
  recoveryDir.close();
  for (auto &temp:temps) {
    String target=temp.substring(0,temp.length()-4);
    if (!SD.exists(target)) SD.rename(temp,target); else SD.remove(temp);
  }
  for (auto &backup:backups) {
    String target=backup.substring(0,backup.length()-4);
    if (!SD.exists(target)) SD.rename(backup,target); else SD.remove(backup);
  }

  File dir = SD.open(RECIPE_DIR);
  if (!dir) return;
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    if (!file.isDirectory() && name.endsWith(".txt")) {
      String txt;
      while (file.available()) txt += (char)file.read();
      Recipe r = txtToRecipe(txt);
      r.storagePath=recipeDirPath(name);
      recipes.push_back(r);
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
}
void seedDemoIfEmpty() {
  for (auto &r: recipes) if (r.id == "focaccia-andre") return;
  Recipe a; a.id="focaccia-andre"; a.name="FOCACCIA DO ANDRE"; a.category="MASSAS"; a.favorite=true;
  a.ingredients.push_back({"AGUA",293});
  a.ingredients.push_back({"ACUCAR",8});
  a.ingredients.push_back({"AZEITE",20});
  a.ingredients.push_back({"FARINHA DE TRIGO",366});
  a.ingredients.push_back({"FERMENTO",3.3});
  a.ingredients.push_back({"SAL",10});
  recalc(a);
  recipes.push_back(a);
  saveAll();
}

bool hasRecipeId(String id) {
  for (auto &r: recipes) if (r.id == id) return true;
  return false;
}
Recipe portalCakeRecipe() {
  Recipe r;
  r.id = "portal-cake";
  r.name = "BOLO DE PORTAL";
  r.category = "DOCES";
  r.favorite = true;
  r.composite = true;
  Prep massa; massa.name = "MASSA DO BOLO";
  massa.ingredients.push_back({"FARINHA DE TRIGO",200});
  massa.ingredients.push_back({"CACAU EM PO",57});
  massa.ingredients.push_back({"BICARBONATO",7});
  massa.ingredients.push_back({"SAL KOSHER",3});
  massa.ingredients.push_back({"ACUCAR",300});
  massa.ingredients.push_back({"GORDURA VEGETAL",92});
  massa.ingredients.push_back({"OVOS",100});
  massa.ingredients.push_back({"EXTRATO DE BAUNILHA",5});
  massa.ingredients.push_back({"LEITELHO",368});
  massa.ingredients.push_back({"LICOR DE CEREJA",120});
  Prep recheio; recheio.name = "RECHEIO";
  recheio.ingredients.push_back({"LICOR DE CEREJA",60});
  recheio.ingredients.push_back({"CEREJAS AZEDAS",680});
  recheio.ingredients.push_back({"CREME DE LEITE FRESCO",720});
  recheio.ingredients.push_back({"ACUCAR DE CONFEITEIRO",30});
  recheio.ingredients.push_back({"CACAU EM PO",16});
  Prep finalizacao; finalizacao.name = "FINALIZACAO";
  finalizacao.ingredients.push_back({"CHOCOLATE MEIO AMARGO",100});
  finalizacao.ingredients.push_back({"CEREJAS MARASQUINO",40});
  r.preps.push_back(massa);
  r.preps.push_back(recheio);
  r.preps.push_back(finalizacao);
  recalc(r);
  return r;
}
bool unlockPortalCake() {
  for (int i=0; i<(int)recipes.size(); i++) {
    if (recipes[i].id == "portal-cake") {
      String oldPath = recipes[i].storagePath;
      recipes[i] = portalCakeRecipe();
      recipes[i].storagePath = oldPath;
      saveRecipe(recipes[i]);
      flash("THE CAKE IS REAL");
      return false;
    }
  }
  Recipe r = portalCakeRecipe();
  recipes.push_back(r);
  recipeIndex = recipes.size() - 1;
  saveRecipe(recipes[recipeIndex]);
  beepRecipe();
  flash("THE CAKE IS REAL");
  return true;
}

std::vector<int> recipeIndicesFor(String cat) {
  std::vector<int> idx;
  for (int i=0;i<(int)recipes.size();i++) {
    if (cat=="FAVORITAS" && recipes[i].favorite) idx.push_back(i);
    else if (recipes[i].category == cat) idx.push_back(i);
  }
  // Favoritas primeiro, depois alfabético simples.
  for (int a=0;a<(int)idx.size();a++) for (int b=a+1;b<(int)idx.size();b++) {
    Recipe &ra=recipes[idx[a]], &rb=recipes[idx[b]];
    bool swapIt = false;
    if (ra.favorite != rb.favorite) swapIt = !ra.favorite && rb.favorite;
    else swapIt = ra.name > rb.name;
    if (swapIt) { int t=idx[a]; idx[a]=idx[b]; idx[b]=t; }
  }
  return idx;
}

bool pollEnterOnly() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto ks = M5Cardputer.Keyboard.keysState();
    if (ks.enter) {
      M5Cardputer.Speaker.stop();
      applySoundVolume();
      return true;
    }
  }
  return false;
}
void bootDelay(int ms) {
  int steps = ms / 25;
  for (int i=0;i<steps;i++) { if (pollEnterOnly()) { go(ST_MAIN); return; } delay(25); }
}
void bootWait(int ms) {
  int steps = ms / 25;
  for (int i=0;i<steps;i++) { M5Cardputer.update(); delay(25); }
}
void drawCenteredText(String s, int y, int size, uint16_t color=AMBER) {
  uiText(size, color, BG);
  int charW = 6 * size;
  int x = max(0, (sw() - (int)s.length() * charW) / 2);
  M5Cardputer.Display.setCursor(x, y);
  M5Cardputer.Display.print(s);
}
void drawCenteredWrapped(String s, int y, int size, uint16_t color=AMBER) {
  int maxChars = sw() / (6 * size);
  if ((int)s.length() <= maxChars) { drawCenteredText(s, y, size, color); return; }
  int mid = s.length() / 2;
  int cut = s.lastIndexOf(' ', mid);
  if (cut < 0) cut = s.indexOf(' ', mid);
  if (cut < 0) cut = mid;
  drawCenteredText(s.substring(0, cut), y, size, color);
  drawCenteredText(s.substring(cut + 1), y + 12 * size, size, color);
}
void drawBootScreen(String title, String slogan, bool cursorOn=false) {
  M5Cardputer.Display.fillScreen(BG);
  drawCenteredText(title + (cursorOn ? "_" : ""), 42, 2, AMBER);
  drawCenteredWrapped(slogan, 76, 1, AMBER);
  uiText(1, AMBER_DIM, BG);
  String hint = "OK ENTRA";
  M5Cardputer.Display.setCursor(sw() - 6 - hint.length()*6, sh()-11);
  M5Cardputer.Display.print(hint);
}
void drawBootGlitch(String baseTitle, String baseSlogan) {
  const char glyphs[] = "#$%&/\\_*+01";
  String t = baseTitle;
  String s = baseSlogan;
  for (int i=0;i<3;i++) {
    if (t.length()) {
      int ti = random(0, (int)t.length());
      t.setCharAt(ti, glyphs[random(0, (int)sizeof(glyphs)-1)]);
    }
    if (s.length()) {
      int si = random(0, (int)s.length());
      if (s[si] != ' ') s.setCharAt(si, glyphs[random(0, (int)sizeof(glyphs)-1)]);
    }
  }
  drawBootScreen(t, s, true);
}
void updateBootJingle(unsigned long &lastNote, int &noteStep) {
  int count = sizeof(OPENING_NOTES) / sizeof(OPENING_NOTES[0]);
  int total = count * OPENING_REPEATS;
  if (!soundOn || soundVolume <= 0 || noteStep >= total) return;
  int idx = noteStep % count;
  if (!lastNote || millis() - lastNote >= (unsigned long)OPENING_DURATIONS[idx] + OPENING_NOTE_GAP_MS) {
    M5Cardputer.Speaker.setVolume(OPENING_VOLUME);
    if (OPENING_NOTES[idx] > 0) M5Cardputer.Speaker.tone(OPENING_NOTES[idx], OPENING_DURATIONS[idx]);
    applySoundVolume();
    lastNote = millis();
    noteStep++;
  }
}
bool bootPause(int ms, unsigned long &lastNote, int &noteStep) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    updateBootJingle(lastNote, noteStep);
    if (pollEnterOnly()) return true;
    delay(15);
  }
  return false;
}
void showBoot() {
  String title = "MiseDeck";
  String slogan = "\"Mise en place de bolso para o Cardputer\"";
  randomSeed(millis() + batteryMv + recipes.size());
  unsigned long bootNoteLast = 0;
  int bootNoteStep = 0;
  String typedTitle = "";
  String typedSlogan = "";
  for (int i=0;i<title.length();i++) {
    typedTitle += title[i];
    drawBootScreen(typedTitle, "", true);
    if (i%3==1) { drawBootGlitch(typedTitle, ""); if (bootPause(35, bootNoteLast, bootNoteStep)) { go(ST_MAIN); return; } }
    if (pollEnterOnly()) { go(ST_MAIN); return; }
    if (bootPause(95, bootNoteLast, bootNoteStep)) { go(ST_MAIN); return; }
  }
  for (int i=0;i<slogan.length();i++) {
    typedSlogan += slogan[i];
    drawBootScreen(title, typedSlogan, true);
    if (i%9==4) { drawBootGlitch(title, typedSlogan); if (bootPause(35, bootNoteLast, bootNoteStep)) { go(ST_MAIN); return; } }
    if (pollEnterOnly()) { go(ST_MAIN); return; }
    if (bootPause(34, bootNoteLast, bootNoteStep)) { go(ST_MAIN); return; }
  }
  drawBootScreen(title, slogan, false);
  unsigned long start = millis();
  while (millis() - start < 15000UL) {
    updateBootJingle(bootNoteLast, bootNoteStep);
    if (pollEnterOnly()) { beepOK(); go(ST_MAIN); return; }
    if ((millis() - start) % 2500 < 45) drawBootGlitch(title, slogan);
    else if ((millis() - start) % 2500 < 105) drawBootScreen(title, slogan, false);
    delay(25);
  }
  M5Cardputer.Speaker.stop();
  applySoundVolume();
  go(ST_MAIN);
  return;
#if 0
  String lines[] = {
    "MISEDECK BOOT\n--------------------\n",
    "INIT DISPLAY....OK\n",
    "MAP TECLADO.....OK\n",
    "MOUNT SD CARD...OK\n",
    "LOAD RECEITAS...OK\n"
  };
  String acc="";
  for (int i=0;i<5;i++) { acc += lines[i]; drawText(acc); bootDelay(i==0?700:850); if (state==ST_MAIN) return; }
  String g1[] = {"%$#@!_SYS_BOOT\nC¥B3R_CH3F_ERR\nREADY_//_OK", "@#!_CORE_STREAM\nSYS//HANDOFF_\nC4RD//CHEF", "NOISE_MAP_OK\n%$READY$%\n"};
  for (int i=0;i<3;i++) { drawText(g1[i]); bootDelay(160); if (state==ST_MAIN) return; }
  M5Cardputer.Display.fillScreen(BG);
  M5Cardputer.Display.drawPng(CHEF_SPLASH_PNG, CHEF_SPLASH_PNG_SIZE, 0, 0);
  bootDelay(1450);
  if (state==ST_MAIN) return;
  String slogan = "O Seu Canivete Suico\nDigital Gastronomico";
  String typed="";
  for (int i=0;i<=slogan.length();i++) {
    typed = slogan.substring(0,i);
    drawText(" [ MISEDECK " + String(FW_VERSION) + " ]\n--------------------\n" + typed + "_\n--------------------");
    bootDelay(58);
    if (state==ST_MAIN) return;
  }
  bootDelay(900);
  beepOK();
  go(ST_MAIN);
#endif
}

KeyEvent readKey() {
  KeyEvent e;
  M5Cardputer.update();
  if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) return e;
  auto ks = M5Cardputer.Keyboard.keysState();
  bool editing = state==ST_TEXT_INPUT || state==ST_NUM_INPUT || state==ST_ADD_ING_NAME || state==ST_ADD_ING_WEIGHT || state==ST_QUICK || state==ST_QUICK_TARGET || state==ST_TIMER_CUSTOM || state==ST_TIMER_RUN;
  bool wifiTyping = state==ST_WIFI_SSID || state==ST_WIFI_PASS;
  if (ks.enter) e.enter=true;
  if (ks.del) e.del=true;
  if (ks.tab) e.tab=true;
  bool fn = ks.fn;
  for (auto c: ks.word) {
    if (!wifiTyping && c>='a' && c<='z') c -= 32;
    if (fn) {
      if (c=='L') e.up=true;
      else if (c=='M') e.down=true;
      else if (c=='N') e.left=true;
      else if (c==',' || c=='/' || c=='.') e.right=true;
    } else {
      if (c=='`' || c==27) e.back=true;
      else if (editing && !wifiTyping && (c==',' || c=='<' || c=='J')) e.left=true;
      else if (editing && !wifiTyping && (c=='/' || c=='?' || c=='L')) e.right=true;
      else if (!editing && !wifiTyping && (c==';' || c==':' || c=='I' || c=='W')) e.up=true;
      else if (!editing && !wifiTyping && (c=='.' || c=='>' || c=='K' || c=='S')) e.down=true;
      else if (!editing && !wifiTyping && (c==',' || c=='<' || c=='J' || c=='A')) e.left=true;
      else if (!editing && !wifiTyping && (c=='/' || c=='?' || c=='L' || c=='D')) e.right=true;
      else e.ch = c;
    }
  }
  return e;
}

void nav(const KeyEvent& e, int total, void (*onEnter)(), State backState) {
  bool moved = false;
  if (e.back) { beepBack(); go(backState); return; }
  if (e.up && total>0) { selected--; moved=true; if (selected<0) { if (page>0) { page--; selected=MAX_VISIBLE-1; } else selected=0; } }
  if (e.down && total>0) { selected++; moved=true; int visible = min(MAX_VISIBLE, total - page*MAX_VISIBLE); if (selected>=visible) { if ((page+1)*MAX_VISIBLE<total) { page++; selected=0; } else selected=max(0,visible-1); } }
  if (e.left) {
    if (page>0) { page--; selected=0; moved=true; }
    else { beepBack(); go(backState); return; }
  }
  bool enterNow = e.enter;
  if (e.right) {
    if ((page+1)*MAX_VISIBLE<total) { page++; selected=0; moved=true; }
    else enterNow = true;
  }
  if (moved) { beepNav(); render(); }
  if (enterNow && onEnter) { toneMs(1040, 18); onEnter(); }
}

void renderMessageIfAny() {
  if (messageUntil && millis() < messageUntil) { drawText(header("SISTEMA") + "\n" + messageText); }
}
void flash(String m, bool err) {
  messageText = m; messageUntil = millis() + 700; if (err) beepErr(); else beepOK();
  drawText(header("SISTEMA") + "\n" + messageText);
}

void render() {
  if (messageUntil && millis() < messageUntil) return;
  messageUntil = 0;
  String out;
  switch(state) {
    case ST_MAIN: uiMenu("MISEDECK " + String(FW_VERSION), {"RECEITAS","FERRAMENTAS","WIFI","SISTEMA"}, ";/ . MOVE  / ENTRA  OK ENTRA"); return;
    case ST_RECEITAS: uiMenu("RECEITAS", {"FAVORITAS","BIBLIOTECA","NOVA","QUICK"}); return;
    case ST_CATEGORIES: {
      std::vector<String> it; for(int i=0;i<CAT_COUNT;i++) it.push_back(cats[i]);
      uiMenu("BIBLIOTECA", it); return;
    }
    case ST_RECIPE_LIST: {
      auto idx=recipeIndicesFor(currentCategory);
      if (!idx.size()) uiPanel(currentCategory, "SEM RECEITAS\n\nCrie uma nova receita\nou importe arquivos TXT.", "` VOLTA");
      else { std::vector<String> it; for(auto n:idx) it.push_back(String(recipes[n].favorite?"* ":"  ")+recipes[n].name); uiMenu(currentCategory, it, ";/ . MOVE  OK ABRE  ` VOLTA"); }
      return;
    }
    case ST_RECIPE_VIEW: {
      Recipe &r=recipes[recipeIndex];
      uiFrame(String(r.favorite?"* ":"") + r.name, r.composite ? "OK PREP  / PREP  O ACOES  ` VOLTA" : "OK ACOES  TAB FAV  ` VOLTA");
      uiText(2, AMBER, BG);
      M5Cardputer.Display.setCursor(6, 22);
      M5Cardputer.Display.print("TOTAL " + fw(r.total));
      std::vector<String> rows;
      if (r.composite) {
        int start=page*MAX_VISIBLE;
        for(int i=0;i<MAX_VISIBLE && start+i<(int)r.preps.size();i++) {
          Prep &p = r.preps[start+i];
          int y=52+i*20;
          bool sel=(i==selected);
          if(sel) M5Cardputer.Display.fillRect(4, y-3, sw()-8, 18, AMBER_DARK);
          uiText(1, AMBER, sel ? AMBER_DARK : BG);
          M5Cardputer.Display.setCursor(8, y);
          M5Cardputer.Display.print(clipText(p.name, 20) + "  " + fw(p.total));
        }
      } else {
        float base=sumIngredients(r.ingredients); if(base<=0)base=1;
        int start=page*MAX_VISIBLE;
        for(int i=0;i<MAX_VISIBLE && start+i<(int)r.ingredients.size();i++) {
          auto &ing=r.ingredients[start+i];
          int y=52+i*20;
          uiText(1, AMBER, BG);
          M5Cardputer.Display.setCursor(8, y);
          M5Cardputer.Display.print(clipText(ing.name, 15));
          M5Cardputer.Display.setCursor(118, y);
          M5Cardputer.Display.print(fw(ing.weight*r.total/base));
          M5Cardputer.Display.setCursor(190, y);
          M5Cardputer.Display.print(String((int)round(ing.weight/base*100)) + "%");
        }
      }
      return;
    }
    case ST_SHARE_QR: uiShareQr(); return;
    case ST_PREP_VIEW: {
      Recipe &r=recipes[recipeIndex]; Prep &p=r.preps[prepIndex];
      uiFrame(p.name, "OK ACOES  ` VOLTA");
      uiText(2, AMBER, BG);
      M5Cardputer.Display.setCursor(6, 22);
      M5Cardputer.Display.print("TOTAL " + fw(p.total));
      float base=sumIngredients(p.ingredients); if(base<=0)base=1;
      int start=page*MAX_VISIBLE;
      for(int i=0;i<MAX_VISIBLE && start+i<(int)p.ingredients.size();i++) {
        auto &ing=p.ingredients[start+i];
        int y=52+i*20;
        uiText(1, AMBER, BG);
        M5Cardputer.Display.setCursor(8, y);
        M5Cardputer.Display.print(clipText(ing.name, 15));
        M5Cardputer.Display.setCursor(118, y);
        M5Cardputer.Display.print(fw(ing.weight*p.total/base));
        M5Cardputer.Display.setCursor(190, y);
        M5Cardputer.Display.print(String((int)round(ing.weight/base*100)) + "%");
      }
      return;
    }
    case ST_ACTIONS: {
      Recipe &r=recipes[recipeIndex];
      if(r.composite) uiMenu("ACOES", {"ALTERAR TOTAL","PREPAROS","DUPLICAR","TIMER","COMPARTILHAR","APAGAR"});
      else uiMenu("ACOES", {"ALTERAR TOTAL","INGREDIENTES","DUPLICAR","TIMER","COMPARTILHAR","APAGAR"});
      return;
    }
    case ST_PREP_ACTIONS: uiMenu("ACOES PREPARO", {"ALTERAR TOTAL","INGREDIENTES","DUPLICAR PREP.","REMOVER PREP."}); return;
    case ST_PREP_MENU: {
      Recipe &r=recipes[recipeIndex];
      std::vector<String> it={"ADICIONAR PREP."};
      for(auto &p:r.preps) it.push_back(p.name);
      uiMenu("PREPAROS", it);
      return;
    }
    case ST_ING_MENU: uiMenu("INGREDIENTES", {"ADICIONAR","ALTERAR PESO","REMOVER","RENOMEAR ING."}); return;
    case ST_ING_LIST: {
      Recipe &r=recipes[recipeIndex]; std::vector<Ingredient> *vec;
      if(r.composite && prepIndex>=0) vec=&r.preps[prepIndex].ingredients; else vec=&r.ingredients;
      std::vector<String> rows;
      for(auto &ing:*vec) rows.push_back(ing.name + "  " + fw(ing.weight));
      uiListRows(pending.title, rows);
      return;
    }
    case ST_TEXT_INPUT: uiInput(pending.title, pending.label, inputBuf, "", false); return;
    case ST_NUM_INPUT: uiInput(pending.title, pending.label, inputBuf, pending.info, true); return;
    case ST_CONFIRM: {
      uiFrame(pending.title, "OK CONFIRMA  ` CANCELA");
      uiText(1, AMBER, BG);
      M5Cardputer.Display.setCursor(6, 24);
      M5Cardputer.Display.print(clipText(pending.info, 36));
      std::vector<String> yesno={"NAO","SIM"};
      int y=60;
      for(int i=0;i<2;i++) {
        bool sel=(i==selected);
        if(sel) M5Cardputer.Display.fillRect(18+i*108, y-4, 96, 30, AMBER);
        else M5Cardputer.Display.drawRect(18+i*108, y-4, 96, 30, AMBER_DARK);
        uiText(2, sel?UI_BLACK:AMBER, sel?AMBER:BG);
        M5Cardputer.Display.setCursor(42+i*108, y+3);
        M5Cardputer.Display.print(yesno[i]);
      }
      return;
    }
    case ST_NEW_TYPE: uiMenu("NOVA RECEITA", {"SIMPLES","COMPOSTA"}); return;
    case ST_NEW_CAT: { std::vector<String> it; for(int i=0;i<REAL_CAT_COUNT;i++) it.push_back(realCats[i]); uiMenu("CATEGORIA", it); return; }
    case ST_NEW_PREP_NEXT: uiMenu("PREPARO CONCLUIDO", {"ADICIONAR OUTRO","SALVAR RECEITA"}); return;
    case ST_ADD_ING_NAME: {
      bool prepFlow=(hasDraft&&draft.composite)||addingExistingPrep;
      uiInput("NOVO INGREDIENTE", "NOME", inputBuf, prepFlow?"VAZIO+OK CONCLUI":"VAZIO+OK SALVA", false);
      return;
    }
    case ST_ADD_ING_WEIGHT: uiInput("PESO INGREDIENTE", "GRAMAS", inputBuf, pending.text, true); return;
    case ST_QUICK: {
      float t=0; for(auto w:quickWeights)t+=w;
      out="TOTAL: "+fw(t)+"\n\n";
      int start=max(0,(int)quickWeights.size()-3); for(int i=start;i<(int)quickWeights.size();i++) out += String(i+1)+") "+fw(quickWeights[i])+"\n";
      out += "\nPESO: "+inputBuf+"_";
      uiPanel("QUICK", out, "OK ADD  VAZIO+OK FINAL", 1); return;
    }
    case ST_QUICK_TARGET: uiInput("QUICK TOTAL", "NOVO TOTAL", inputBuf, "ORIGINAL: "+fw(quickOriginal), true); return;
    case ST_QUICK_RESULT: {
      out="TOTAL: "+fw(quickTarget)+"\n\n"; int start=page*MAX_VISIBLE;
      for(int i=0;i<MAX_VISIBLE && start+i<(int)quickWeights.size();i++) out += String(start+i+1)+") "+fw(quickWeights[start+i]*quickTarget/quickOriginal)+"\n";
      uiPanel("QUICK RESULTADO", out, "` SAIR", 1); return;
    }
    case ST_FERRAMENTAS: uiMenu("FERRAMENTAS", {"TIMER","CONVERSOR"}); return;
    case ST_TIMER_MENU: uiMenu("TIMER", {"CUSTOM","01 MIN","03 MIN","05 MIN","10 MIN","15 MIN"}); return;
    case ST_TIMER_CUSTOM: uiInput("TIMER CUSTOM", "MINUTOS", inputBuf, "", true); return;
    case ST_TIMER_RUN: uiTimer(); return;
    case ST_TIMER_DONE: uiPanel("TIMER FINAL", "TEMPO ESGOTADO\n\nOK ou ` para parar\no alarme.", "OK PARA  ` VOLTA", 1); return;
    case ST_CONV: uiMenu("CONVERSOR", {"TEMPERATURA","PESO"}); return;
    case ST_TEMP_DIR: uiMenu("TEMPERATURA", {"C PARA F","F PARA C"}); return;
    case ST_WEIGHT_FROM: uiMenu("PESO DE", {"g","kg","lb","oz"}); return;
    case ST_WEIGHT_TO: uiMenu("PESO PARA", {"g","kg","lb","oz"}, "DE: "+pending.text+"  OK ESCOLHE"); return;
    case ST_RESULT: uiPanel("RESULTADO", pending.info, "` VOLTA", 1); return;
    case ST_WIFI: {
      String foot = WiFi.status()==WL_CONNECTED ? "ON " + wifiIp() : (wifiSsid.length() ? "SALVO " + wifiSsid : "OFF  configure e conecte");
      uiMenu("WIFI", {"REDE E SENHA","CONECTAR","STATUS","DESCONECTAR"}, foot);
      return;
    }
    case ST_WIFI_SSID: uiInput("WIFI", "NOME DA REDE", inputBuf, "SSID / rede da casa", false); return;
    case ST_WIFI_PASS: {
      String masked="";
      for(int i=0;i<inputBuf.length();i++) masked += "*";
      uiInput("WIFI", "SENHA", masked, wifiSsid, false);
      return;
    }
    case ST_WIFI_STATUS: {
      String body;
      if (WiFi.status()==WL_CONNECTED) {
        body = "CONECTADO\nRede: " + wifiSsid + "\nIP: " + wifiIp() + "\n\nAcesse:\nmisedeck.local\nou " + wifiIp();
      } else {
        body = "DESCONECTADO\n\nConfigure rede/senha\ne aperte conectar.";
      }
      uiPanel("WIFI STATUS", body, "OK ATUALIZA  ` VOLTA", 1);
      return;
    }
    case ST_SISTEMA: uiMenu("SISTEMA", {"STATUS","BATERIA","SOBRE","BACKUP TXT","IMPORTAR SD","RESETAR DADOS","SOM"}, "BACKUP/IMPORT usam arquivos TXT no SD"); return;
    case ST_STATUS: uiPanel("STATUS", "SD: "+String(sdOK?"OK":"FALHA")+"\nRECEITAS: "+String(recipes.size())+"\nSOM: "+String(soundOn?"ON":"OFF")+" VOL "+String(soundVolume)+"/10\nVERSAO: "+FW_VERSION, "` VOLTA", 1); return;
    case ST_BATTERY: uiBatteryScreen(); return;
    case ST_ABOUT: {
      uiFrame("SOBRE", "` VOLTA");
      drawCenteredText("MISEDECK", 22, 1, AMBER);
      drawCenteredText("Idealizado por Andre", 38, 1, AMBER);
      drawCenteredText("Fuentes  @anfuentz", 50, 1, AMBER_DIM);
      drawCenteredText("Vibecodeado por Codex", 66, 1, AMBER);
      drawCenteredText("\"nos somos os musicos", 84, 1, AMBER_DIM);
      drawCenteredText("e somos os", 96, 1, AMBER_DIM);
      drawCenteredText("sonhadores de sonhos\"", 108, 1, AMBER_DIM);
      return;
    }
    case ST_SOUND: uiSoundScreen(); return;
  }
  drawText(out);
}

void askNumber(String title, String label, String info, float initial, void (*cb)(float)) {
  inputReturn=state; inputNumberCallback=cb; setInput(initial>0 ? String(initial,1) : ""); pending.title=title; pending.label=label; pending.info=info; go(ST_NUM_INPUT);
}
void askText(String title, String label, String initial, void (*cb)(String)) {
  inputReturn=state; inputTextCallback=cb; setInput(initial); pending.title=title; pending.label=label; go(ST_TEXT_INPUT);
}
void askConfirm(String title, String info, void (*cb)()) {
  confirmReturn=state; confirmCallback=cb; pending.title=title; pending.info=info; go(ST_CONFIRM);
}
void editInput(const KeyEvent& e, bool numeric) {
  if (e.back) { go(inputReturn); return; }
  bool changed = false;
  if (e.left && inputCursor>0) { inputCursor--; changed = true; beepNav(); }
  if (e.right && inputCursor<(int)inputBuf.length()) { inputCursor++; changed = true; beepNav(); }
  if (e.del && inputBuf.length() && inputCursor>0) { inputBuf.remove(inputCursor-1, 1); inputCursor--; changed = true; }
  if (e.ch) {
    char c = e.ch;
    if (numeric) {
      if ((c>='0'&&c<='9') || c=='.' || c==',') {
        if (c==',') c='.';
        inputBuf = inputBuf.substring(0,inputCursor) + String(c) + inputBuf.substring(inputCursor);
        inputCursor++;
        changed = true;
      }
    } else {
      c = cleanInputChar(c);
      if (c) {
        inputBuf = inputBuf.substring(0,inputCursor) + String(c) + inputBuf.substring(inputCursor);
        if (inputBuf.length()>22) inputBuf=inputBuf.substring(0,22);
        inputCursor = min((int)inputBuf.length(), inputCursor+1);
        changed = true;
      }
    }
  }
  if (e.enter) {
    if (numeric) { inputBuf.replace(',', '.'); if (inputNumberCallback) inputNumberCallback(inputBuf.toFloat()); }
    else { if (inputTextCallback) inputTextCallback(cleanName(inputBuf)); }
    return;
  }
  if (changed) render();
}

void cbScaleRecipe(float v) { if(v<=0){flash("VALOR INVALIDO",true);return;} scaleRecipe(recipes[recipeIndex],v); saveRecipe(recipes[recipeIndex]); flash("TOTAL ALTERADO"); go(ST_RECIPE_VIEW); }
void cbScalePrep(float v) { if(v<=0){flash("VALOR INVALIDO",true);return;} scalePrep(recipes[recipeIndex].preps[prepIndex],v); recalc(recipes[recipeIndex]); saveRecipe(recipes[recipeIndex]); flash("PREPARO ALTERADO"); go(ST_PREP_VIEW); }
void cbSetIngWeight(float v) {
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex>=0 && ingIndex<(int)vec->size()) (*vec)[ingIndex].weight=r1(v); recalc(r); saveRecipe(r); flash("PESO ALTERADO"); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void cbRenameIng(String s) {
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(s.length() && ingIndex>=0 && ingIndex<(int)vec->size()) (*vec)[ingIndex].name=s; saveRecipe(r); flash("ING. RENOMEADO"); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void doRemoveIng() {
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex>=0 && ingIndex<(int)vec->size()) vec->erase(vec->begin()+ingIndex); recalc(r); saveRecipe(r); flash("ING. REMOVIDO"); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void cbNewRecipeName(String s) { draft.name=s.length()?s:"NOVA RECEITA"; setInput(""); go(ST_NEW_CAT); }
void cbNewPrepName(String s) { Prep p; p.name=s.length()?s:"PREPARO"; draft.preps.push_back(p); prepIndex=draft.preps.size()-1; setInput(""); go(ST_ADD_ING_NAME); }
void cbAddExistingPrepName(String s) {
  if(recipeIndex<0||recipeIndex>=(int)recipes.size())return;
  Prep p; p.name=s.length()?s:"PREPARO";
  recipes[recipeIndex].preps.push_back(p);
  prepIndex=recipes[recipeIndex].preps.size()-1;
  addingExistingPrep=true;
  setInput("");
  go(ST_ADD_ING_NAME);
}
void cbAddIngWeight(float v) {
  Ingredient ing; ing.name=pending.text; ing.weight=r1(v);
  if(hasDraft) {
    if(draft.composite) draft.preps[prepIndex].ingredients.push_back(ing); else draft.ingredients.push_back(ing);
    recalc(draft); setInput(""); go(ST_ADD_ING_NAME);
  } else {
    Recipe &r=recipes[recipeIndex]; if(r.composite && prepIndex>=0) r.preps[prepIndex].ingredients.push_back(ing); else r.ingredients.push_back(ing);
    recalc(r);
    if(addingExistingPrep){setInput("");go(ST_ADD_ING_NAME);}
    else {saveRecipe(r); flash("ING. ADICIONADO"); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);}
  }
}
void finalizeDraft() { if(hasDraft){ recalc(draft); draft.storagePath=""; recipes.push_back(draft); recipeIndex=recipes.size()-1; saveRecipe(recipes[recipeIndex]); hasDraft=false; flash("RECEITA SALVA"); beepRecipe(); go(ST_RECIPE_VIEW); } }
void finishExistingPrep() {
  if(recipeIndex<0||prepIndex<0)return;
  Recipe &r=recipes[recipeIndex];
  if(prepIndex>=(int)r.preps.size()||!r.preps[prepIndex].ingredients.size()){flash("ADICIONE ING.",true);return;}
  recalc(r); saveRecipe(r); addingExistingPrep=false; flash("PREPARO SALVO"); go(ST_PREP_VIEW);
}
void doDeleteRecipe() { if(recipeIndex>=0 && recipeIndex<(int)recipes.size()) { if(sdOK) { String path=recipes[recipeIndex].storagePath.length()?recipeDirPath(recipes[recipeIndex].storagePath):newRecipePath(recipes[recipeIndex]); SD.remove(path); SD.remove(path+".tmp"); SD.remove(path+".bak"); } recipes.erase(recipes.begin()+recipeIndex); recipeIndex=-1; flash("RECEITA APAGADA"); go(ST_RECEITAS); } }
void doResetData() { if(sdOK) { std::vector<String> paths; File dir=SD.open(RECIPE_DIR); File f=dir.openNextFile(); while(f){ String n=recipeDirPath(f.name()); bool recipeData=n.endsWith(".txt")||n.endsWith(".txt.tmp")||n.endsWith(".txt.bak"); if(!f.isDirectory()&&recipeData)paths.push_back(n); f.close(); f=dir.openNextFile(); } dir.close(); for(auto &path:paths)SD.remove(path); } recipes.clear(); flash("DADOS LIMPOS"); go(ST_MAIN); }
void doRemovePrep() { Recipe &r=recipes[recipeIndex]; if(r.preps.size()<=1){flash("MIN. 1 PREPARO",true);go(ST_PREP_VIEW);return;} if(prepIndex>=0 && prepIndex<(int)r.preps.size()) r.preps.erase(r.preps.begin()+prepIndex); recalc(r); saveRecipe(r); flash("PREPARO REMOVIDO"); go(ST_RECIPE_VIEW); }

float unitToG(float v, String u) { if(u=="g")return v; if(u=="kg")return v*1000.0; if(u=="lb")return v*453.59237; if(u=="oz")return v*28.349523; return v; }
float gToUnit(float g, String u) { if(u=="g")return g; if(u=="kg")return g/1000.0; if(u=="lb")return g/453.59237; if(u=="oz")return g/28.349523; return g; }
String weightFrom, weightTo;
void cbTempC(float v) { pending.info=String(v,1)+"C\n= "+String(v*9.0/5.0+32.0,1)+"F"; go(ST_RESULT); }
void cbTempF(float v) { pending.info=String(v,1)+"F\n= "+String((v-32.0)*5.0/9.0,1)+"C"; go(ST_RESULT); }
void cbWeight(float v) { float g=unitToG(v,weightFrom); pending.info=String(v,1)+weightFrom+"\n= "+String(gToUnit(g,weightTo),1)+weightTo; go(ST_RESULT); }
void cbTimerCustom(float v);

void exportBackup() {
  if(!sdOK){ flash("SD FALHA", true); return; }
  SD.remove(BACKUP_FILE);
  File f=SD.open(BACKUP_FILE, FILE_WRITE); if(!f){ flash("BACKUP FALHA", true); return; }
  for(auto &r:recipes){ f.println(recipeToTxt(r)); f.println("===================="); }
  f.close(); flash("BACKUP EXPORTADO");
}
void importTxts() {
  if(!sdOK){ flash("SD FALHA", true); return; }
  ensureDirs(); int count=0; File dir=SD.open(IMPORT_DIR); File f=dir.openNextFile();
  while(f){ String n=f.name(); if(!f.isDirectory() && n.endsWith(".txt")){ String txt; while(f.available())txt+=(char)f.read(); Recipe r=txtToRecipe(txt); r.storagePath=""; recipes.push_back(r); saveRecipe(recipes.back()); count++; } f.close(); f=dir.openNextFile(); }
  dir.close(); flash(String("IMPORTADO ")+count);
}

String wifiIp() {
  if (WiFi.status() != WL_CONNECTED) return "---";
  return WiFi.localIP().toString();
}
void loadWifiConfig() {
  prefs.begin("misedeck", true);
  wifiSsid = prefs.getString("wifi_ssid", "");
  wifiPass = prefs.getString("wifi_pass", "");
  prefs.end();
}
void saveWifiConfig() {
  prefs.begin("misedeck", false);
  prefs.putString("wifi_ssid", wifiSsid);
  prefs.putString("wifi_pass", wifiPass);
  prefs.end();
}
void loadSoundConfig() {
  prefs.begin("misedeck", true);
  soundOn = prefs.getBool("sound_on", true);
  soundVolume = prefs.getInt("sound_vol", 6);
  prefs.end();
  if (soundVolume < 0) soundVolume = 0;
  if (soundVolume > 10) soundVolume = 10;
  if (soundVolume == 0) soundOn = false;
  applySoundVolume();
}
void saveSoundConfig() {
  if (soundVolume < 0) soundVolume = 0;
  if (soundVolume > 10) soundVolume = 10;
  prefs.begin("misedeck", false);
  prefs.putBool("sound_on", soundOn);
  prefs.putInt("sound_vol", soundVolume);
  prefs.end();
  applySoundVolume();
}
bool pollBackKey() {
  M5Cardputer.update();
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto ks = M5Cardputer.Keyboard.keysState();
    for (auto c: ks.word) {
      if (c=='`' || c==27) return true;
    }
  }
  return false;
}
String htmlEscape(String s) {
  String out;
  for (int i=0;i<s.length();i++) {
    char c=s[i];
    if (c=='&') out += "&amp;";
    else if (c=='<') out += "&lt;";
    else if (c=='>') out += "&gt;";
    else if (c=='\"') out += "&quot;";
    else if (c=='\'') out += "&#39;";
    else out += c;
  }
  return out;
}
String fileSafeName(String s) {
  s=cleanName(s);
  s.replace(" ", "_");
  if (!s.length()) s="RECEITA";
  return s + ".txt";
}
String backupTxt() {
  String out;
  for(auto &r:recipes){ out += recipeToTxt(r); out += "\n====================\n"; }
  return out;
}
bool validRecipeTxt(String txt) {
  txt.trim();
  return txt.length() > 0 && txt.indexOf('|') >= 0 && (txt.indexOf("[INGREDIENTES]") >= 0 || txt.indexOf("[PREPARO]") >= 0);
}
String recipeCard(int idx) {
  Recipe &r=recipes[idx];
  String txt=htmlEscape(recipeToTxt(r));
  String card="<article class='recipe' data-id='"+String(idx)+"'>";
  card += "<div><h3>" + htmlEscape(String(r.favorite?"★ ":"") + r.name) + "</h3>";
  card += "<p class='dim'>" + htmlEscape(r.category) + " // " + String(r.composite?"COMPOSTA":"SIMPLES") + " // TOTAL " + fw(r.total) + "</p></div>";
  card += "<div class='actions'><button onclick='openRecipe("+String(idx)+")'>ABRIR</button><a class='btn' href='/txt?id="+String(idx)+"'>BAIXAR TXT</a></div>";
  card += "<pre id='txt"+String(idx)+"' class='hidden'>" + txt + "</pre></article>";
  return card;
}
String portalHtml() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>MiseDeck</title><style>";
  html += ":root{--bg:#050300;--panel:#120900;--amber:#ffb000;--dim:#9b6a20;--line:#5d3a00;--hot:#ffd36a}";
  html += "*{box-sizing:border-box}html{font-size:16px}body{margin:0;background:radial-gradient(circle at top,#1b0d00 0,#050300 45%,#000 100%);color:var(--amber);font-family:ui-monospace,Consolas,monospace;-webkit-text-size-adjust:100%}";
  html += ".wrap{max-width:1100px;margin:auto;padding:22px}.top{display:flex;gap:12px;justify-content:space-between;align-items:flex-start;border-bottom:1px solid var(--line);padding-bottom:14px}";
  html += "h1{margin:0;letter-spacing:2px;text-shadow:0 0 9px #ff8c00;font-size:clamp(24px,5vw,42px)}.dim{color:var(--dim)}.slogan{color:var(--hot);font-size:15px;margin-top:4px}.topright{display:flex;flex-direction:column;gap:8px;align-items:flex-end}.topstatus{display:flex;gap:10px;align-items:stretch}.createbar{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}";
  html += ".status,.tab,.recipe,.modalbox{border:1px solid var(--line);background:rgba(18,9,0,.92);box-shadow:0 0 18px #3a2200}";
  html += ".status{padding:10px 12px;text-align:right}.tabs{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0}.tab{color:var(--amber);padding:10px 12px;cursor:pointer;min-height:42px}.tab.active{background:var(--amber);color:#000}";
  html += ".panel{display:none}.panel.active{display:block}.recipe{display:flex;justify-content:space-between;gap:14px;align-items:center;padding:14px;margin:10px 0}.recipe h3{margin:0 0 6px 0;font-size:20px}.actions{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}";
  html += "button,.btn{border:1px solid var(--amber);background:#1b0d00;color:var(--hot);padding:10px 13px;text-decoration:none;font:inherit;cursor:pointer;min-height:42px;border-radius:0}button:hover,.btn:hover{background:var(--amber);color:#000}.create{border-width:2px;background:#2a1200;color:#ffd36a;box-shadow:0 0 14px #5d3a00}.syncTop{min-width:86px;background:var(--amber);color:#000;font-weight:bold}.backupFoot{margin:28px 0 8px;padding-top:14px;border-top:1px solid var(--line);text-align:right}";
  html += ".empty{border:1px dashed var(--line);padding:18px;color:var(--dim)}.hidden{display:none}.modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.78);padding:18px}.modal.on{display:flex}.modalbox{width:min(920px,100%);max-height:92vh;margin:auto;padding:18px;overflow:auto}";
  html += ".modalbar{display:flex;justify-content:space-between;gap:10px;align-items:center;border-bottom:1px solid var(--line);padding-bottom:10px;margin-bottom:12px}#mtitle{font-size:22px;text-shadow:0 0 8px #ff8c00}pre{white-space:pre-wrap;line-height:1.45;color:#ffd36a;font-size:17px}";
  html += ".scale{border:1px solid var(--line);padding:12px;margin:12px 0;background:#0b0500}.scale input{background:#050300;border:1px solid var(--amber);color:var(--hot);padding:9px;font:inherit;width:150px}.calc{margin-top:12px}.calc table{width:100%;border-collapse:collapse}.calc td,.calc th{border-bottom:1px solid var(--line);padding:7px;text-align:left}.calc h4{margin:14px 0 6px}";
  html += ".editor{display:none;border:1px solid var(--line);padding:12px;margin:12px 0;background:#070300}.editor.on{display:block}.editor textarea{width:100%;min-height:330px;background:#020100;color:var(--hot);border:1px solid var(--amber);font:14px ui-monospace,Consolas,monospace;padding:10px}.tools{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}";
  html += "@media(max-width:650px){html{font-size:18px}.wrap{padding:14px}.top,.recipe,.topright,.topstatus{display:block}.topright{align-items:stretch}.slogan{font-size:14px;line-height:1.35}.status{text-align:left;margin-top:8px;font-size:13px}.syncTop{width:100%;margin-top:8px}.createbar{display:grid;grid-template-columns:1fr;gap:8px;margin-top:8px}.createbar button,.syncTop{font-size:16px;padding:13px}.tabs{position:sticky;top:0;z-index:5;background:rgba(5,3,0,.96);margin:12px -14px 14px;padding:10px 14px;overflow-x:auto;flex-wrap:nowrap;-webkit-overflow-scrolling:touch;border-bottom:1px solid var(--line)}.tab{flex:0 0 auto;padding:12px 14px;font-size:15px}.panel h2{font-size:20px;margin:12px 0}.recipe{padding:15px;margin:12px 0}.recipe h3{font-size:22px;line-height:1.2}.recipe .dim{font-size:14px;line-height:1.35}.actions{display:grid;grid-template-columns:1fr;gap:8px;margin-top:12px}.actions button,.actions .btn,.tools button,.tools .btn{width:100%;text-align:center;padding:13px}.modal{padding:0;align-items:stretch}.modalbox{width:100%;height:100vh;max-height:none;margin:0;border:0;padding:14px}.modalbar{position:sticky;top:0;background:#120900;z-index:6}.modalbar button{padding:12px 14px}#mtitle{font-size:20px;line-height:1.2}pre{font-size:18px;line-height:1.5;overflow:auto}.scale{padding:12px}.scale p{line-height:1.45}.scale input{width:100%;font-size:18px;margin:6px 0}.scale button{width:100%;margin-top:8px}.calc{overflow-x:auto}.calc table{min-width:440px}.editor textarea{min-height:55vh;font-size:16px;line-height:1.45}.backupFoot{text-align:center}.backupFoot .btn{display:block;width:100%}}";
  html += "</style></head><body><div class='wrap'><header class='top'><div><h1>MISEDECK</h1><div class='slogan'><em>\"Mise en place de bolso para o Cardputer\"</em></div></div>";
  html += "<div class='topright'><div class='topstatus'><button class='syncTop' onclick='syncPortal()'>SYNC</button><div class='status'>IP " + htmlEscape(wifiIp()) + "<br>FW " + String(FW_VERSION) + "<br>" + String(recipes.size()) + " receitas</div></div>";
  html += "<div class='createbar'><button class='create' onclick='newRecipe(false)'>+ NOVA SIMPLES</button><button class='create' onclick='newRecipe(true)'>+ NOVA COMPOSTA</button></div></div></header>";
  html += "<nav class='tabs'>";
  html += "<button class='tab catTab active' onclick='showTab(0)'>TODAS</button>";
  for(int c=0;c<CAT_COUNT;c++) html += "<button class='tab catTab' onclick='showTab("+String(c+1)+")'>" + htmlEscape(cats[c]) + "</button>";
  html += "</nav>";
  html += "<section id='tab0' class='panel active'><h2>TODAS</h2>";
  if(!recipes.size()) html += "<div class='empty'>Nenhuma receita cadastrada.</div>";
  else for(int i=0;i<(int)recipes.size();i++) html += recipeCard(i);
  html += "</section>";
  for(int c=0;c<CAT_COUNT;c++) {
    String cat=cats[c];
    html += "<section id='tab"+String(c+1)+"' class='panel'>";
    html += "<h2>" + htmlEscape(cat) + "</h2>";
    auto idx=recipeIndicesFor(cat);
    if(!idx.size()) html += "<div class='empty'>Nenhuma receita nesta categoria.</div>";
    else for(auto i:idx) html += recipeCard(i);
    html += "</section>";
  }
  html += "<div class='backupFoot'><a class='btn' href='/backup'>BAIXAR BACKUP TXT</a></div>";
  html += "</div><div id='modal' class='modal'><div class='modalbox'><div class='modalbar'><strong id='mtitle'>RECEITA</strong><button onclick='closeRecipe()'>FECHAR</button></div>";
  html += "<pre id='mtext'></pre>";
  html += "<div class='scale'><div class='dim'>RECALCULAR PROPORCAO / pode salvar na receita original</div><p>Total atual: <strong id='baseTotal'>--</strong> g</p><p>Novo total: <input id='newTotal' type='number' step='0.1' min='0'> <button onclick='recalcRecipe()'>RECALCULAR</button> <button id='saveScale' onclick='saveScale()' disabled>SALVAR ORIGINAL</button></p><div id='calc' class='calc'></div></div>";
  html += "<div class='tools'><button onclick='editCurrent()'>EDITAR TXT</button><a id='mdown' class='btn' href='#'>BAIXAR TXT</a></div>";
  html += "<div id='editor' class='editor'><div class='dim'>EDITOR TXT SIMPLES // primeira linha = nome da receita // ingrediente: NOME|PESO|g</div><textarea id='editTxt'></textarea><div class='tools'><button onclick='saveEdit()'>SALVAR</button><button onclick='hideEditor()'>CANCELAR</button></div></div></div></div>";
  html += "<script>function showTab(n){document.querySelectorAll('.catTab').forEach((b,i)=>b.classList.toggle('active',i===n));document.querySelectorAll('.panel').forEach((p,i)=>p.classList.toggle('active',i===n));}";
  html += "function syncPortal(){location.href='/?sync='+Date.now();}";
  html += "let currentTxt='',currentId=-1,scaleReady=false;function esc(s){return String(s).replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]));}";
  html += "function parseRecipe(t){let r={name:'RECEITA',total:0,type:'SIMPLES',ings:[],preps:[]},p=null,seen=false;for(let raw of t.split(/\\r?\\n/)){let line=raw.trim();if(!line)continue;if(line==='MISEDECK RECIPE'||line==='CYBER CHEF RECIPE'){seen=true;continue;}if(line==='[INGREDIENTES]'){p=null;seen=true;continue;}if(line==='[PREPARO]'){p={name:'PREPARO',total:0,ings:[]};r.preps.push(p);seen=true;continue;}if(line.includes('|')){let a=line.split('|');let ing={name:a[0],w:parseFloat((a[1]||'0').replace(',','.'))||0};if(p)p.ings.push(ing);else r.ings.push(ing);seen=true;continue;}let ix=line.indexOf(':');if(ix<0){if(!seen)r.name=line;seen=true;continue;}let k=line.split(':')[0],v=line.substring(ix+1).trim();if(k==='NOME'&&!p)r.name=v;if(k==='TIPO')r.type=v;if(k==='TOTAL'){if(p)p.total=parseFloat(v.replace(',','.'))||0;else r.total=parseFloat(v.replace(',','.'))||0;}if(k==='NOME'&&p)p.name=v;seen=true;}return r;}";
  html += "function openRecipe(id){currentId=id;scaleReady=false;currentTxt=document.getElementById('txt'+id).textContent;let r=parseRecipe(currentTxt);document.getElementById('mtext').textContent=currentTxt;document.getElementById('mdown').href='/txt?id='+id;document.getElementById('mtitle').textContent=r.name||'RECEITA';document.getElementById('baseTotal').textContent=(r.total||0).toFixed(1);document.getElementById('newTotal').value=r.total||'';document.getElementById('calc').innerHTML='';document.getElementById('saveScale').disabled=true;document.getElementById('editor').classList.remove('on');document.getElementById('modal').classList.add('on');document.querySelector('.modalbox').scrollTop=0;}";
  html += "function row(n,w,p){return '<tr><td>'+esc(n)+'</td><td>'+w.toFixed(1)+' g</td><td>'+p+'%</td></tr>'}";
  html += "function recalcRecipe(){let r=parseRecipe(currentTxt),nt=parseFloat(document.getElementById('newTotal').value)||0,out='';scaleReady=false;document.getElementById('saveScale').disabled=true;if(!nt||!r.total){document.getElementById('calc').innerHTML='<p class=dim>Informe um total valido.</p>';return;}let f=nt/r.total;if(r.preps.length){out+='<h4>TOTAL NOVO '+nt.toFixed(1)+' g</h4>';for(let p of r.preps){let pt=(p.total||p.ings.reduce((s,i)=>s+i.w,0))*f;let base=p.total||p.ings.reduce((s,i)=>s+i.w,0)||1;out+='<h4>'+esc(p.name)+' // '+pt.toFixed(1)+' g</h4><table><tr><th>Ingrediente</th><th>Peso</th><th>%</th></tr>';for(let i of p.ings)out+=row(i.name,i.w*f,Math.round(i.w/base*100));out+='</table>';}}else{let base=r.ings.reduce((s,i)=>s+i.w,0)||r.total||1;out+='<h4>TOTAL NOVO '+nt.toFixed(1)+' g</h4><table><tr><th>Ingrediente</th><th>Peso</th><th>%</th></tr>';for(let i of r.ings)out+=row(i.name,i.w*nt/base,Math.round(i.w/base*100));out+='</table>';}document.getElementById('calc').innerHTML=out;scaleReady=true;document.getElementById('saveScale').disabled=false;}";
  html += "function saveScale(){let nt=parseFloat(document.getElementById('newTotal').value)||0;if(!scaleReady||currentId<0||!nt)return alert('Recalcule antes de salvar.');if(!confirm('Salvar nova proporcao na receita original?'))return;fetch('/scale?id='+currentId+'&total='+encodeURIComponent(nt),{method:'POST'}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert('Falha ao salvar'));}";
  html += "function editCurrent(){document.getElementById('editTxt').value=currentTxt;document.getElementById('editor').classList.add('on');}";
  html += "function hideEditor(){document.getElementById('editor').classList.remove('on');}";
  html += "function validTxt(txt){return txt.trim().length>0&&txt.includes('|')&&(txt.includes('[INGREDIENTES]')||txt.includes('[PREPARO]'));}";
  html += "function saveEdit(){let txt=document.getElementById('editTxt').value;if(!validTxt(txt))return alert('TXT invalido: use nome, categoria/total e ingredientes NOME|PESO|g');let url=currentId>=0?'/save?id='+currentId:'/new';if(!confirm(currentId>=0?'Salvar alteracoes na receita original?':'Criar nova receita no Cardputer?'))return;fetch(url,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:txt}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert('Falha ao salvar'));}";
  html += "function newRecipe(comp){currentId=-1;scaleReady=false;let simple='NOVA RECEITA\\nCATEGORIA: OUTROS\\nFAVORITA: NAO\\nTOTAL: 100.0\\n\\n[INGREDIENTES]\\n\\nINGREDIENTE 1|100.0|g\\n';let composite='NOVA RECEITA COMPOSTA\\nCATEGORIA: OUTROS\\nFAVORITA: NAO\\nTOTAL: 300.0\\nTIPO: COMPOSTA\\n\\n[PREPARO]\\nNOME: PREPARO 1\\nTOTAL: 100.0\\nINGREDIENTE 1|100.0|g\\n\\n[PREPARO]\\nNOME: PREPARO 2\\nTOTAL: 200.0\\nINGREDIENTE 2|200.0|g\\n';currentTxt=comp?composite:simple;document.getElementById('mtext').textContent=currentTxt;document.getElementById('mtitle').textContent=comp?'NOVA RECEITA COMPOSTA':'NOVA RECEITA';document.getElementById('baseTotal').textContent='--';document.getElementById('newTotal').value='';document.getElementById('calc').innerHTML='';document.getElementById('saveScale').disabled=true;document.getElementById('mdown').href='#';document.getElementById('modal').classList.add('on');editCurrent();}";
  html += "function closeRecipe(){document.getElementById('editor').classList.remove('on');document.getElementById('modal').classList.remove('on');}</script></body></html>";
  return html;
}
void startPortalServer() {
  if (!webStarted) {
    webServer.on("/", [](){ webServer.send(200, "text/html; charset=utf-8", portalHtml()); });
    webServer.on("/txt", [](){
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      if(id<0 || id>=(int)recipes.size()) { webServer.send(404, "text/plain; charset=utf-8", "Receita nao encontrada"); return; }
      webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + fileSafeName(recipes[id].name) + "\"");
      webServer.send(200, "text/plain; charset=utf-8", recipeToTxt(recipes[id]));
    });
    webServer.on("/backup", [](){
      webServer.sendHeader("Content-Disposition", "attachment; filename=\"MISEDECK_BACKUP.txt\"");
      webServer.send(200, "text/plain; charset=utf-8", backupTxt());
    });
    webServer.on("/scale", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", "SD FALHA"); return; }
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      float total=webServer.hasArg("total") ? webServer.arg("total").toFloat() : 0;
      if(id<0 || id>=(int)recipes.size() || total<=0){ webServer.send(400, "text/plain; charset=utf-8", "Dados invalidos"); return; }
      scaleRecipe(recipes[id], total);
      if(saveRecipe(recipes[id])) webServer.send(200, "text/plain; charset=utf-8", "Receita salva. Use SYNC para atualizar.");
      else webServer.send(500, "text/plain; charset=utf-8", "Falha ao salvar receita");
    });
    webServer.on("/save", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", "SD FALHA"); return; }
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      if(id<0 || id>=(int)recipes.size()){ webServer.send(400, "text/plain; charset=utf-8", "Receita invalida"); return; }
      String txt=webServer.arg("plain");
      if(!validRecipeTxt(txt)){ webServer.send(400, "text/plain; charset=utf-8", "TXT invalido"); return; }
      String oldPath=recipes[id].storagePath;
      Recipe r=txtToRecipe(txt);
      r.id=recipes[id].id;
      r.storagePath=oldPath;
      recipes[id]=r;
      if(saveRecipe(recipes[id])) webServer.send(200, "text/plain; charset=utf-8", "Receita atualizada. Use SYNC.");
      else webServer.send(500, "text/plain; charset=utf-8", "Falha ao salvar receita");
    });
    webServer.on("/new", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", "SD FALHA"); return; }
      String txt=webServer.arg("plain");
      if(!validRecipeTxt(txt)){ webServer.send(400, "text/plain; charset=utf-8", "TXT invalido"); return; }
      Recipe r=txtToRecipe(txt);
      r.id=nowId();
      r.storagePath="";
      recipes.push_back(r);
      if(saveRecipe(recipes.back())) webServer.send(200, "text/plain; charset=utf-8", "Nova receita criada. Use SYNC.");
      else { recipes.pop_back(); webServer.send(500, "text/plain; charset=utf-8", "Falha ao criar receita"); }
    });
    webServer.onNotFound([](){ webServer.sendHeader("Location", "/", true); webServer.send(302, "text/plain", ""); });
    webServer.begin();
    webStarted = true;
  }
  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin("misedeck");
  }
}
void stopPortalServer() {
  if (webStarted) { webServer.stop(); webStarted=false; }
  if (mdnsStarted) { MDNS.end(); mdnsStarted=false; }
}
void disconnectWifi() {
  stopPortalServer();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
void connectWifiNow() {
  if (!wifiSsid.length()) { flash("DIGITE A REDE", true); go(ST_WIFI_SSID); return; }
  saveWifiConfig();
  uiPanel("WIFI", "Conectando em:\n" + wifiSsid + "\n\nAguarde...", "` CANCELA", 1);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  unsigned long start=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-start<16000) {
    if (pollBackKey()) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      beepBack();
      flash("CONEXAO CANCELADA", true);
      go(ST_WIFI);
      return;
    }
    delay(250);
  }
  if (WiFi.status()==WL_CONNECTED) {
    startPortalServer();
    beepWifiOK();
    go(ST_WIFI_STATUS);
  } else {
    beepWifiErr();
    flash("WIFI FALHOU", true);
    go(ST_WIFI);
  }
}
void editWifiInput(const KeyEvent& e, bool password) {
  if (e.back) { go(ST_WIFI); return; }
  bool changed=false;
  if (e.del && inputBuf.length()) { inputBuf.remove(inputBuf.length()-1); inputCursor=inputBuf.length(); changed=true; }
  if (e.ch && e.ch>=32 && e.ch<=126) {
    inputBuf += e.ch;
    if (inputBuf.length()>48) inputBuf=inputBuf.substring(0,48);
    inputCursor=inputBuf.length();
    changed=true;
  }
  if (e.enter) {
    if (state==ST_WIFI_SSID) { wifiSsid=inputBuf; setInput(wifiPass); go(ST_WIFI_PASS); }
    else { wifiPass=inputBuf; saveWifiConfig(); connectWifiNow(); }
    return;
  }
  if (changed) render();
}

void handleState(const KeyEvent& e) {
  if(state==ST_TIMER_RUN) {
    if(timerRunning && !timerPaused && millis()>=timerDeadline){ finishTimer(); return; }
  }
  switch(state) {
    case ST_MAIN: nav(e,4, [](){ if(gi()==0)go(ST_RECEITAS); else if(gi()==1)go(ST_FERRAMENTAS); else if(gi()==2)go(ST_WIFI); else go(ST_SISTEMA); }, ST_MAIN); break;
    case ST_RECEITAS: nav(e,4, [](){ if(gi()==0){currentCategory="FAVORITAS";go(ST_RECIPE_LIST);} else if(gi()==1)go(ST_CATEGORIES); else if(gi()==2)go(ST_NEW_TYPE); else {quickWeights.clear(); setInput(""); go(ST_QUICK);} }, ST_MAIN); break;
    case ST_CATEGORIES: nav(e,CAT_COUNT, [](){ currentCategory=cats[gi()]; go(ST_RECIPE_LIST); }, ST_RECEITAS); break;
    case ST_RECIPE_LIST: {
      auto idx=recipeIndicesFor(currentCategory); nav(e,idx.size(), [](){ auto idx=recipeIndicesFor(currentCategory); if(gi()<(int)idx.size()){ recipeIndex=idx[gi()]; prepIndex=-1; go(ST_RECIPE_VIEW);} }, ST_CATEGORIES); break;
    }
    case ST_RECIPE_VIEW: {
      Recipe &r=recipes[recipeIndex]; int total=r.composite?r.preps.size():r.ingredients.size();
      if(e.back)go(ST_RECIPE_LIST); else if(e.tab){r.favorite=!r.favorite; saveRecipe(r); flash(r.favorite?"FAVORITA":"DESFAVORITADA");}
      else if(e.right && (page+1)*MAX_VISIBLE>=total){ if(r.composite){ prepIndex=gi(); if(prepIndex<(int)r.preps.size())go(ST_PREP_VIEW);} else go(ST_ACTIONS); }
      else if(e.up||e.down||e.left||e.right) nav(e,total,nullptr,ST_RECIPE_LIST);
      else if(e.enter){ if(r.composite){ prepIndex=gi(); if(prepIndex<(int)r.preps.size())go(ST_PREP_VIEW);} else go(ST_ACTIONS); }
      else if(e.ch=='O') go(ST_ACTIONS);
      break;
    }
    case ST_PREP_VIEW: {
      Recipe &r=recipes[recipeIndex]; Prep &p=r.preps[prepIndex];
      if(e.back)go(ST_RECIPE_VIEW);
      else if(e.right && (page+1)*MAX_VISIBLE>=(int)p.ingredients.size())go(ST_PREP_ACTIONS);
      else if(e.up||e.down||e.left||e.right)nav(e,p.ingredients.size(),nullptr,ST_RECIPE_VIEW);
      else if(e.enter)go(ST_PREP_ACTIONS);
      break;
    }
    case ST_ACTIONS: {
      Recipe &r=recipes[recipeIndex]; int total=6;
      nav(e,total, [](){ Recipe &r=recipes[recipeIndex]; int c=gi(); if(c==0)askNumber("ALTERAR TOTAL","NOVO TOTAL","ATUAL: "+fw(r.total),r.total,cbScaleRecipe); else if(c==1){ if(r.composite)go(ST_PREP_MENU); else go(ST_ING_MENU); } else if(c==2){ Recipe cp=r; cp.id=nowId(); cp.name=cleanName(cp.name+" COPY"); cp.storagePath=""; cp.favorite=false; recipes.push_back(cp); recipeIndex=recipes.size()-1; saveRecipe(recipes[recipeIndex]); flash("DUPLICADA"); go(ST_RECIPE_VIEW); } else if(c==3)go(ST_TIMER_MENU); else if(c==4)go(ST_SHARE_QR); else askConfirm("APAGAR RECEITA?",r.name,doDeleteRecipe); }, ST_RECIPE_VIEW); break;
    }
    case ST_SHARE_QR: if(e.back||e.enter)go(ST_ACTIONS); break;
    case ST_PREP_ACTIONS: nav(e,4, [](){ Recipe&r=recipes[recipeIndex]; Prep&p=r.preps[prepIndex]; int c=gi(); if(c==0)askNumber("ALTERAR TOTAL","NOVO TOTAL","ATUAL: "+fw(p.total),p.total,cbScalePrep); else if(c==1)go(ST_ING_MENU); else if(c==2){ Prep cp=p; cp.name=cleanName(cp.name+" COPY"); r.preps.push_back(cp); recalc(r); saveRecipe(r); flash("PREP DUPLICADO"); go(ST_RECIPE_VIEW);} else askConfirm("REMOVER PREPARO?",p.name,doRemovePrep); }, ST_PREP_VIEW); break;
    case ST_PREP_MENU: {
      Recipe&r=recipes[recipeIndex];
      nav(e,r.preps.size()+1, [](){ int c=gi(); if(c==0)askText("NOVO PREPARO","NOME","",cbAddExistingPrepName); else {prepIndex=c-1;go(ST_PREP_VIEW);} }, ST_ACTIONS);
      break;
    }
    case ST_ING_MENU: nav(e,4, [](){ int c=gi(); if(c==0){setInput("");go(ST_ADD_ING_NAME);} else { pending.mode=c; pending.title=(c==1?"ALTERAR PESO":c==2?"REMOVER ING.":"RENOMEAR ING."); go(ST_ING_LIST);} }, prepIndex>=0?ST_PREP_ACTIONS:ST_ACTIONS); break;
    case ST_ING_LIST: {
      Recipe&r=recipes[recipeIndex]; std::vector<Ingredient>* vec=(r.composite&&prepIndex>=0)?&r.preps[prepIndex].ingredients:&r.ingredients;
      nav(e,vec->size(), [](){ Recipe&r=recipes[recipeIndex]; std::vector<Ingredient>* vec=(r.composite&&prepIndex>=0)?&r.preps[prepIndex].ingredients:&r.ingredients; ingIndex=gi(); if(pending.mode==1)askNumber("ALTERAR PESO","NOVO","ATUAL: "+fw((*vec)[ingIndex].weight),(*vec)[ingIndex].weight,cbSetIngWeight); else if(pending.mode==2)askConfirm("REMOVER ING.?",(*vec)[ingIndex].name,doRemoveIng); else askText("RENOMEAR ING.","NOME",(*vec)[ingIndex].name,cbRenameIng); }, ST_ING_MENU); break;
    }
    case ST_TEXT_INPUT: editInput(e,false); break;
    case ST_NUM_INPUT: editInput(e,true); break;
    case ST_CONFIRM:
      if(e.back){ beepBack(); go(confirmReturn); }
      else if(e.left||e.up){ selected=0; beepNav(); render(); }
      else if(e.right||e.down){ selected=1; beepNav(); render(); }
      else if(e.enter){ if(gi()==1 && confirmCallback) confirmCallback(); else go(confirmReturn); }
      break;
    case ST_NEW_TYPE: nav(e,2, [](){ hasDraft=true; draft=Recipe(); draft.id=nowId(); draft.composite=(gi()==1); if(draft.composite) draft.preps.clear(); else draft.ingredients.clear(); askText("NOME RECEITA","NOME","",cbNewRecipeName); }, ST_RECEITAS); break;
    case ST_NEW_CAT: nav(e,REAL_CAT_COUNT, [](){ draft.category=realCats[gi()]; if(draft.composite) askText("NOVO PREPARO","NOME","",cbNewPrepName); else { setInput(""); go(ST_ADD_ING_NAME); } }, ST_NEW_TYPE); break;
    case ST_NEW_PREP_NEXT: nav(e,2, [](){ if(gi()==0)askText("NOVO PREPARO","NOME","",cbNewPrepName); else finalizeDraft(); }, ST_ADD_ING_NAME); break;
    case ST_ADD_ING_NAME:
      if(e.back){
        if(addingExistingPrep){Recipe&r=recipes[recipeIndex];if(prepIndex>=0&&prepIndex<(int)r.preps.size())r.preps.erase(r.preps.begin()+prepIndex);addingExistingPrep=false;go(ST_PREP_MENU);}
        else if(hasDraft&&draft.composite&&prepIndex>=0&&prepIndex<(int)draft.preps.size()&&draft.preps[prepIndex].ingredients.size())go(ST_NEW_PREP_NEXT);
        else if(hasDraft)go(ST_NEW_CAT); else go(ST_ING_MENU);
      } else if(e.enter){
        if(inputBuf.length()==0){
          if(hasDraft&&draft.composite){if(prepIndex>=0&&prepIndex<(int)draft.preps.size()&&draft.preps[prepIndex].ingredients.size())go(ST_NEW_PREP_NEXT);else flash("ADICIONE ING.",true);}
          else if(addingExistingPrep)finishExistingPrep();
          else if(hasDraft)finalizeDraft(); else go(ST_ING_MENU);
        } else { pending.text=cleanName(inputBuf); setInput(""); go(ST_ADD_ING_WEIGHT); }
      } else editInput(e,false);
      break;
    case ST_ADD_ING_WEIGHT:
      if(e.back)go(ST_ADD_ING_NAME);
      else if(e.enter){inputBuf.replace(',','.');float v=inputBuf.toFloat();if(v>0)cbAddIngWeight(v);else flash("PESO INVALIDO",true);}
      else editInput(e,true);
      break;
    case ST_QUICK: if(e.back)go(ST_RECEITAS); else if(e.enter){ if(inputBuf.length()==0){ if(quickWeights.size()){ quickOriginal=0; for(auto w:quickWeights)quickOriginal+=w; setInput(""); go(ST_QUICK_TARGET);} else flash("SEM PESOS",true); } else { quickWeights.push_back(r1(inputBuf.toFloat())); setInput(""); } } else editInput(e,true); break;
    case ST_QUICK_TARGET: if(e.back)go(ST_QUICK); else if(e.enter){ quickTarget=inputBuf.toFloat(); if(quickTarget>0)go(ST_QUICK_RESULT); else flash("TOTAL INVALIDO",true); } else editInput(e,true); break;
    case ST_QUICK_RESULT: if(e.back)go(ST_RECEITAS); else nav(e,quickWeights.size(),nullptr,ST_RECEITAS); break;
    case ST_FERRAMENTAS: nav(e,2, [](){ if(gi()==0)go(ST_TIMER_MENU); else go(ST_CONV); }, ST_MAIN); break;
    case ST_TIMER_MENU: nav(e,6, [](){ int c=gi(); if(c==0){ setInput(""); go(ST_TIMER_CUSTOM); } else { int mins[5]={1,3,5,10,15}; timerDeadline=millis()+mins[c-1]*60000UL; timerRunning=true; timerPaused=false; go(ST_TIMER_RUN); } }, ST_FERRAMENTAS); break;
    case ST_TIMER_CUSTOM: if(e.back)go(ST_TIMER_MENU); else if(e.enter){ int m=inputBuf.toInt(); if(m>0){timerDeadline=millis()+m*60000UL;timerRunning=true;timerPaused=false;go(ST_TIMER_RUN);} } else editInput(e,true); break;
    case ST_TIMER_RUN: if(e.back){timerRunning=false;go(ST_TIMER_MENU);} else if(e.enter){ if(timerPaused){ timerDeadline=millis()+timerLeft; timerPaused=false; } else { timerLeft=timerDeadline-millis(); timerPaused=true; } } break;
    case ST_TIMER_DONE: if(e.back||e.enter){ stopTimerAlarm(); beepBack(); go(ST_TIMER_MENU); } break;
    case ST_CONV: nav(e,2, [](){ if(gi()==0)go(ST_TEMP_DIR); else go(ST_WEIGHT_FROM); }, ST_FERRAMENTAS); break;
    case ST_TEMP_DIR: nav(e,2, [](){ if(gi()==0)askNumber("C PARA F","CELSIUS","",0,cbTempC); else askNumber("F PARA C","FAHRENHEIT","",0,cbTempF); }, ST_CONV); break;
    case ST_WEIGHT_FROM: nav(e,4, [](){ const char* u[]={"g","kg","lb","oz"}; weightFrom=u[gi()]; pending.text=weightFrom; go(ST_WEIGHT_TO); }, ST_CONV); break;
    case ST_WEIGHT_TO: nav(e,4, [](){ const char* u[]={"g","kg","lb","oz"}; weightTo=u[gi()]; askNumber(weightFrom+" PARA "+weightTo,"VALOR","",0,cbWeight); }, ST_WEIGHT_FROM); break;
    case ST_RESULT: if(e.back||e.enter)go(ST_CONV); break;
    case ST_WIFI: nav(e,4, [](){ int c=gi(); if(c==0){ setInput(wifiSsid); go(ST_WIFI_SSID); } else if(c==1)connectWifiNow(); else if(c==2)go(ST_WIFI_STATUS); else { disconnectWifi(); flash("WIFI DESLIGADO"); go(ST_WIFI); } }, ST_MAIN); break;
    case ST_WIFI_SSID: editWifiInput(e,false); break;
    case ST_WIFI_PASS: editWifiInput(e,true); break;
    case ST_WIFI_STATUS: if(e.back)go(ST_WIFI); else if(e.enter)render(); break;
    case ST_SISTEMA: nav(e,7, [](){ int c=gi(); if(c==0)go(ST_STATUS); else if(c==1)go(ST_BATTERY); else if(c==2){ aboutEggStep=0; go(ST_ABOUT); } else if(c==3)exportBackup(); else if(c==4)importTxts(); else if(c==5)askConfirm("RESETAR DADOS?","APAGAR RECEITAS?",doResetData); else go(ST_SOUND); }, ST_MAIN); break;
    case ST_STATUS: if(e.back||e.enter)go(ST_SISTEMA); break;
    case ST_BATTERY: if(e.back)go(ST_SISTEMA); else if(e.enter)render(); break;
    case ST_ABOUT: {
      if(e.back){ aboutEggStep=0; go(ST_SISTEMA); break; }
      int expected[9] = {1,1,2,2,3,4,3,4,5};
      int got = 0;
      if(e.up) got=1; else if(e.down) got=2; else if(e.left) got=3; else if(e.right) got=4; else if(e.enter) got=5;
      if(got) {
        if(got == expected[aboutEggStep]) {
          aboutEggStep++;
          toneMs(680 + aboutEggStep*45, 22);
          if(aboutEggStep >= 9) {
            aboutEggStep=0;
            if (unlockPortalCake()) go(ST_RECIPE_VIEW);
            else goKeep(ST_ABOUT);
          }
        } else {
          aboutEggStep = (got == expected[0]) ? 1 : 0;
          if(got != 5) beepBack();
        }
      }
      break;
    }
    case ST_SOUND: nav(e,5, [](){
      int c=gi();
      if(c==0){ soundOn=!soundOn; if(soundOn && soundVolume==0) soundVolume=6; saveSoundConfig(); flash(soundOn?"SOM LIGADO":"SOM DESLIGADO"); goKeep(ST_SOUND); }
      else if(c==1){ soundOn=true; if(soundVolume<10) soundVolume++; saveSoundConfig(); beepOK(); goKeep(ST_SOUND); }
      else if(c==2){ if(soundVolume>0) soundVolume--; if(soundVolume==0) soundOn=false; saveSoundConfig(); beepBack(); goKeep(ST_SOUND); }
      else if(c==3){ beepRecipe(); goKeep(ST_SOUND); }
      else { soundOn=false; soundVolume=0; saveSoundConfig(); flash("SOM DESLIGADO"); goKeep(ST_SOUND); }
    }, ST_SISTEMA); break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setTextColor(AMBER, BG);
  loadSoundConfig();
  loadWifiConfig();
  WiFi.mode(WIFI_OFF);

  sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdOK = SD.begin(SD_CS_PIN, sdSPI, 25000000);
  if (sdOK) { ensureDirs(); loadRecipes(); }
  seedDemoIfEmpty();
  showBoot();
}

void loop() {
  KeyEvent e = readKey();
  if (e.enter || e.back || e.del || e.tab || e.up || e.down || e.left || e.right || e.ch) handleState(e);
  if (webStarted) webServer.handleClient();
  updateTimerAlarm();
  if (state == ST_TIMER_RUN) {
    if (timerRunning && !timerPaused && millis()>=timerDeadline) { finishTimer(); }
    static unsigned long last=0; if (millis()-last>500){ last=millis(); render(); }
  }
  if (state == ST_BATTERY) {
    static unsigned long lastBat=0; if (millis()-lastBat>3000){ lastBat=millis(); render(); }
  }
  delay(15);
}
