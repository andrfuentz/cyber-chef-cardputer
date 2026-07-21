#include <M5Cardputer.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <vector>
#include <lgfx/utility/lgfx_qrcode.h>
#include "chef_splash.h"

#define FW_VERSION "v1.6.0"
#define RECIPE_DIR "/RECIPES"
#define IMPORT_DIR "/IMPORT"
#define BACKUP_FILE "/MISEDECK_BACKUP.txt"
#define MAX_VISIBLE 3
#define MAX_NAME_LENGTH 64

// Adjust these pins if your Cardputer model uses a different SD pinout.
#define SD_CS_PIN   12
#define SD_SCK_PIN  40
#define SD_MISO_PIN 39
#define SD_MOSI_PIN 14

const uint16_t BG = 0x0000;
const uint16_t UI_BLACK = 0x0000;

struct AccentPalette {
  uint16_t main;
  uint16_t bright;
  uint16_t dim;
  uint16_t dark;
};

constexpr AccentPalette ACCENT_PALETTES[] = {
  {0xFD20, 0xFFE0, 0x7A80, 0x3180},  // Amber
  {0x07E0, 0x9FE7, 0x03E0, 0x0180},  // Terminal green
  {0x07FF, 0xBFFF, 0x03EF, 0x0186},  // Cyan
  {0xA81F, 0xE39F, 0x600F, 0x2806},  // Violet
  {0xC618, 0xFFFF, 0x7BEF, 0x2945},  // Monochrome
  {0xFAF4, 0xFE1B, 0x89AB, 0x38A4},  // Pink
  {0xF208, 0xFCD1, 0x88E3, 0x3861},  // Red
  {0xD81F, 0xFBDF, 0x7010, 0x3007},  // Magenta
};
constexpr uint8_t ACCENT_COLOR_COUNT=sizeof(ACCENT_PALETTES)/sizeof(ACCENT_PALETTES[0]);

uint8_t accentColor=0;
uint8_t themePreviewOriginal=0;
bool portuguese=false;
bool languagePreviewOriginal=false;
uint16_t AMBER=ACCENT_PALETTES[0].main;
uint16_t AMBER_BRIGHT=ACCENT_PALETTES[0].bright;
uint16_t AMBER_DIM=ACCENT_PALETTES[0].dim;
uint16_t AMBER_DARK=ACCENT_PALETTES[0].dark;

SPIClass sdSPI(FSPI);
M5Canvas uiCanvas(&M5Cardputer.Display);
bool sdOK = false;
bool soundOn = true;
int soundVolume = 6;
WebServer webServer(80);
DNSServer dnsServer;
Preferences prefs;
bool webStarted = false;
bool mdnsStarted = false;
bool dnsStarted = false;
String wifiSsid = "";
String wifiPass = "";
bool offlineShareActive = false;
int offlineShareIndex = -1;
String offlineShareSsid = "";
const IPAddress OFFLINE_SHARE_IP(192, 168, 4, 1);
const IPAddress OFFLINE_SHARE_GW(192, 168, 4, 1);
const IPAddress OFFLINE_SHARE_MASK(255, 255, 255, 0);
const byte DNS_PORT = 53;

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
  // Internal SD path. Not part of the recipe TXT format.
  String storagePath;
  bool favorite = false;
  bool composite = false;
  std::vector<Ingredient> ingredients;
  std::vector<Prep> preps;
  float total = 0;
};

std::vector<Recipe> recipes;
const char* cats[] = {"FAVORITES", "MAINS", "DOUGHS", "SWEETS", "DRINKS", "SAUCES", "OTHER"};
const int CAT_COUNT = 7;
const char* realCats[] = {"MAINS", "DOUGHS", "SWEETS", "DRINKS", "SAUCES", "OTHER"};
const int REAL_CAT_COUNT = 6;

String currentCategory = "FAVORITES";
int selected = 0;
int page = 0;
int recipeIndex = -1;
int prepIndex = -1;
int ingIndex = -1;
String inputBuf = "";
int inputCursor = 0;
String messageText = "";
unsigned long messageUntil = 0;
unsigned long marqueeEpoch = 0;
unsigned long marqueeLastFrame = 0;
bool marqueeActive = false;

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
  ST_RECIPES,
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
  ST_TOOLS,
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
  ST_SYSTEM,
  ST_STATUS,
  ST_BATTERY,
  ST_ABOUT,
  ST_LANGUAGE,
  ST_THEME,
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
bool existingPrepBackupValid = false;
Recipe existingPrepBackup;
int aboutEggStep = 0;

String cleanName(String s);

void applyAccentPalette() {
  accentColor%=ACCENT_COLOR_COUNT;
  const AccentPalette &palette=ACCENT_PALETTES[accentColor];
  AMBER=palette.main;
  AMBER_BRIGHT=palette.bright;
  AMBER_DIM=palette.dim;
  AMBER_DARK=palette.dark;
}

const char* ui(const char* pt, const char* en) {
  return portuguese ? pt : en;
}

const char* accentName(uint8_t index) {
  static const char* namesPt[] = {"AMBAR","VERDE","CIANO","VIOLETA","BRANCO","ROSA","VERMELHO","MAGENTA"};
  static const char* namesEn[] = {"AMBER","GREEN","CYAN","VIOLET","WHITE","PINK","RED","MAGENTA"};
  return portuguese ? namesPt[index%ACCENT_COLOR_COUNT] : namesEn[index%ACCENT_COLOR_COUNT];
}

String categoryLabel(String code) {
  if(code=="FAVORITES")return ui("FAVORITAS","FAVORITES");
  if(code=="MAINS")return ui("PRATOS","MAINS");
  if(code=="DOUGHS")return ui("MASSAS","DOUGHS");
  if(code=="SWEETS")return ui("DOCES","SWEETS");
  if(code=="DRINKS")return ui("BEBIDAS","DRINKS");
  if(code=="SAUCES")return ui("MOLHOS","SAUCES");
  return ui("OUTRAS","OTHER");
}

String normalizeCategory(String value) {
  value=cleanName(value);
  if(value=="PRATOS" || value=="MAINS")return "MAINS";
  if(value=="MASSAS" || value=="PASTA" || value=="DOUGHS")return "DOUGHS";
  if(value=="DOCES" || value=="SWEETS")return "SWEETS";
  if(value=="BEBIDAS" || value=="DRINKS")return "DRINKS";
  if(value=="MOLHOS" || value=="SAUCES")return "SAUCES";
  return "OTHER";
}

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
  if (out.length() > MAX_NAME_LENGTH) out = out.substring(0,MAX_NAME_LENGTH);
  return out;
}
String cleanInput(String s) {
  s.toUpperCase();
  String out;
  for (int i=0;i<s.length();i++) {
    char c=s[i];
    if ((c>='A'&&c<='Z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-' || c=='.') out += c;
  }
  if (out.length()>MAX_NAME_LENGTH) out=out.substring(0,MAX_NAME_LENGTH);
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

bool pollEnterOnly();

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
// Original flute-call style opening: quick, high, airy, and generated directly by frequency.
static const uint16_t OPENING_NOTES[] = {1047, 1397, 1865, 1760, 1175, 1760, 1661, 2093, 2217, 1760, 2093, 2349};
static const uint16_t OPENING_DURATIONS[] = {90, 90, 115, 150, 90, 105, 120, 150, 95, 115, 145, 260};
static const uint16_t OPENING_NOTE_GAP_MS = 18;
static const uint8_t OPENING_VOLUME = 112;
static const uint8_t OPENING_REPEATS = 1;

bool waitWithBootCancel(uint16_t ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (pollEnterOnly()) {
      M5Cardputer.Speaker.stop();
      applySoundVolume();
      return true;
    }
    delay(10);
  }
  return false;
}
bool playOpeningMotif() {
  if (!soundOn || soundVolume <= 0) return false;
  uint8_t oldVol = speakerVolumeValue();
  uint8_t motifVol = min<uint8_t>(oldVol, OPENING_VOLUME);
  M5Cardputer.Speaker.setVolume(motifVol);
  int count = sizeof(OPENING_NOTES) / sizeof(OPENING_NOTES[0]);
  for (int rep=0; rep<OPENING_REPEATS; rep++) {
    for (int i=0; i<count; i++) {
      if (pollEnterOnly()) {
        M5Cardputer.Speaker.stop();
        applySoundVolume();
        return true;
      }
      M5Cardputer.Speaker.tone(OPENING_NOTES[i], OPENING_DURATIONS[i]);
      if (waitWithBootCancel(OPENING_DURATIONS[i] + OPENING_NOTE_GAP_MS)) return true;
    }
  }
  M5Cardputer.Speaker.stop();
  applySoundVolume();
  return false;
}
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
  uiCanvas.fillScreen(BG);
  uiCanvas.setTextColor(AMBER, BG);
  uiCanvas.setTextSize(big ? 2 : 1);
  uiCanvas.setCursor(2, 2);
  uiCanvas.print(s);
  uiCanvas.pushSprite(0,0);
}

int sw() { return uiCanvas.width(); }
int sh() { return uiCanvas.height(); }
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
String marqueeText(String s, int width) {
  if(width<1)return "";
  if((int)s.length()<=width)return s;
  marqueeActive=true;
  const unsigned long startPause=1000;
  const unsigned long endPause=900;
  const unsigned long stepMs=230;
  int maxOffset=s.length()-width;
  unsigned long travel=(unsigned long)maxOffset*stepMs;
  unsigned long cycle=startPause+travel+endPause+travel;
  unsigned long phase=(millis()-marqueeEpoch)%max(1UL,cycle);
  int offset=0;
  if(phase<startPause)offset=0;
  else if(phase<startPause+travel)offset=min(maxOffset,(int)((phase-startPause)/stepMs));
  else if(phase<startPause+travel+endPause)offset=maxOffset;
  else offset=max(0,maxOffset-(int)((phase-startPause-travel-endPause)/stepMs));
  return s.substring(offset,offset+width);
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
  uiCanvas.setTextSize(size);
  uiCanvas.setTextColor(fg, bg);
}
void uiFrame(String title, String foot="") {
  if(!foot.length())foot=ui("; . ITEM  , / PAG  OK  ` VOLTAR","; . ITEM  , / PAGE  OK  ` BACK");
  uiCanvas.fillScreen(BG);
  uiText(1, AMBER, BG);
  title.toUpperCase();
  uiCanvas.setCursor(4, 3);
  uiCanvas.print(marqueeText(title, 20));
  String bat = batteryLabel();
  uiText(1, (batteryPct >= 0 && batteryPct < 15) ? AMBER : AMBER_DIM, BG);
  uiCanvas.setCursor(sw() - 6 - bat.length()*6, 3);
  uiCanvas.print(bat);
  uiCanvas.drawLine(0, 15, sw(), 15, AMBER_DARK);
  uiCanvas.drawLine(0, sh()-14, sw(), sh()-14, AMBER_DARK);
  uiText(1, AMBER_DIM, BG);
  uiCanvas.setCursor(4, sh()-11);
  uiCanvas.print(clipText(foot, 38));
}
void uiMenu(String title, const std::vector<String>& items, String foot="") {
  uiFrame(title, foot);
  int total = items.size();
  int start = page * MAX_VISIBLE;
  int rowH = 31;
  int y0 = 20;
  for (int i=0; i<MAX_VISIBLE && start+i<total; i++) {
    int y = y0 + i*rowH;
    bool sel = (i == selected);
    if (sel) {
      uiCanvas.fillRect(3, y-2, sw()-6, rowH-4, AMBER);
      uiCanvas.drawRect(3, y-2, sw()-6, rowH-4, AMBER);
      uiText(2, UI_BLACK, AMBER);
    } else {
      uiCanvas.drawRect(3, y-2, sw()-6, rowH-4, AMBER_DARK);
      uiText(2, AMBER, BG);
    }
    uiCanvas.setCursor(9, y+4);
    uiCanvas.print(sel ? marqueeText(items[start+i], 18) : clipText(items[start+i], 18));
  }
  if (total > MAX_VISIBLE) {
    uiText(1, AMBER_DIM, BG);
    uiCanvas.setCursor(sw()-45, 3);
    uiCanvas.print(String(page+1)+"/"+String((total+MAX_VISIBLE-1)/MAX_VISIBLE));
  }
}
void uiPanel(String title, String body, String foot="", int size=1) {
  if(!foot.length())foot=ui("` VOLTAR","` BACK");
  uiFrame(title, foot);
  uiText(size, AMBER, BG);
  uiCanvas.setCursor(5, 22);
  uiCanvas.print(body);
}
void uiInput(String title, String label, String value, String info="", bool numeric=false) {
  bool wifiField=state==ST_WIFI_SSID || state==ST_WIFI_PASS;
  String editFoot=wifiField ? ui("FN+SETAS CURSOR  DEL  OK  ` VOLTAR","FN+ARROWS CURSOR  DEL  OK  ` BACK") : (numeric ? ui(", / CURSOR  USE .  OK  ` VOLTAR",", / CURSOR  USE .  OK  ` BACK") : ui(", / CURSOR  DEL APAGA  OK  ` VOLTAR",", / CURSOR  DEL ERASE  OK  ` BACK"));
  uiFrame(title,editFoot);
  uiText(1, AMBER_DIM, BG);
  uiCanvas.setCursor(5, 22);
  if (info.length()) {
    uiCanvas.print(clipText(info, 32));
    uiCanvas.setCursor(5, 35);
  }
  uiCanvas.print(label + ":");
  uiCanvas.drawRect(4, 50, sw()-8, 40, AMBER_DARK);
  uiText(2, AMBER_BRIGHT, BG);
  uiCanvas.setCursor(10, 61);
  uiCanvas.print(inputWithCursor(value));
}
void uiTimer() {
  unsigned long remain = timerPaused ? timerLeft : (timerDeadline>millis()?timerDeadline-millis():0);
  int sec=remain/1000; char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d", sec/60, sec%60);
  uiFrame("TIMER", ui("OK PAUSA/CONTINUA  ` CANCELA","OK PAUSE/RESUME  ` CANCEL"));
  uiText(4, AMBER_BRIGHT, BG);
  uiCanvas.setCursor(55, 35);
  uiCanvas.print(buf);
  uiText(2, timerPaused ? AMBER_DIM : AMBER, BG);
  uiCanvas.setCursor(55, 84);
  uiCanvas.print(timerPaused ? ui("PAUSADO","PAUSED") : ui("RODANDO","RUNNING"));
}
void uiListRows(String title, const std::vector<String>& rows, String foot="") {
  uiFrame(title, foot);
  int start=page*MAX_VISIBLE;
  for (int i=0; i<MAX_VISIBLE && start+i<(int)rows.size(); i++) {
    int y=22+i*29;
    bool sel=(i==selected);
    if(sel) uiCanvas.fillRect(4, y-3, sw()-8, 24, AMBER_DARK);
    uiText(1, AMBER, sel ? AMBER_DARK : BG);
    uiCanvas.setCursor(8, y);
    uiCanvas.print(sel ? marqueeText(rows[start+i], 36) : clipText(rows[start+i], 36));
  }
}
void uiBatteryScreen() {
  updateBattery(true);
  uiFrame(ui("BATERIA","BATTERY"), ui("OK ATUALIZA  ` VOLTAR","OK REFRESH  ` BACK"));
  int pct = batteryPct < 0 ? 0 : batteryPct;
  if (pct > 100) pct = 100;
  uiText(4, AMBER_BRIGHT, BG);
  uiCanvas.setCursor(42, 24);
  uiCanvas.print(batteryPct < 0 ? "--%" : String(batteryPct) + "%");
  int x=22, y=70, w=196, h=22;
  uiCanvas.drawRect(x, y, w, h, AMBER);
  uiCanvas.fillRect(x+2, y+2, max(0, (w-4) * pct / 100), h-4, AMBER);
  uiText(2, AMBER, BG);
  uiCanvas.setCursor(50, 101);
  uiCanvas.print(batteryMv > 0 ? String(batteryMv / 1000.0f, 2) + "V" : "-- V");
}
void uiSoundScreen() {
  uiFrame(ui("SOM","SOUND"));
  std::vector<String> items = {String(ui("SOM: ","SOUND: ")) + (soundOn ? ui("LIGADO","ON") : ui("DESLIGADO","OFF")), "VOLUME +", "VOLUME -", ui("TESTAR","TEST"), ui("MUDO RAPIDO","QUICK MUTE")};
  int start = page * MAX_VISIBLE;
  for (int i=0; i<MAX_VISIBLE && start+i<(int)items.size(); i++) {
    int y = 20 + i * 22;
    bool sel = (i == selected);
    if (sel) uiCanvas.fillRect(4, y-3, sw()-8, 18, AMBER_DARK);
    uiText(1, AMBER, sel ? AMBER_DARK : BG);
    uiCanvas.setCursor(8, y);
    uiCanvas.print(clipText(items[start+i], 34));
  }
  if ((int)items.size() > MAX_VISIBLE) {
    uiText(1, AMBER_DIM, BG);
    uiCanvas.setCursor(sw()-45, 3);
    uiCanvas.print(String(page+1)+"/"+String(((int)items.size()+MAX_VISIBLE-1)/MAX_VISIBLE));
  }
  int x=34, y=108, w=172, h=12;
  uiText(1, AMBER_DIM, BG);
  uiCanvas.setCursor(8, 94);
  uiCanvas.print("VOL " + String(soundVolume) + "/10");
  uiCanvas.drawRect(x, y, w, h, AMBER);
  uiCanvas.fillRect(x+2, y+2, max(0, (w-4) * soundVolume / 10), h-4, AMBER);
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
void resetMarquee() { marqueeEpoch=millis(); marqueeLastFrame=0; }
void resetNav() { selected=0; page=0; resetMarquee(); }
int gi() { return page*MAX_VISIBLE+selected; }
struct FramePresenter { ~FramePresenter(){ uiCanvas.pushSprite(0,0); } };
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
  out += String(ui("CATEGORIA: ","CATEGORY: ")) + categoryLabel(r.category) + "\n";
  out += String(ui("FAVORITA: ","FAVORITE: ")) + (r.favorite ? ui("SIM","YES") : ui("NAO","NO")) + "\n";
  out += "TOTAL: " + String(r.total,1) + "\n\n";
  if (!r.composite) {
    out += String(ui("[INGREDIENTES]\n\n","[INGREDIENTS]\n\n"));
    for (auto &i: r.ingredients) out += i.name + "|" + String(i.weight,1) + "|g\n";
  } else {
    out += String(ui("TIPO: COMPOSTA\n\n","TYPE: COMPOSITE\n\n"));
    for (auto &p: r.preps) {
      out += String(ui("[PREPARO]\n","[PREP]\n"));
      out += String(ui("NOME: ","NAME: ")) + p.name + "\n";
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

void startOfflineShare();
void stopOfflineShare(bool wifiOff=true);

void uiShareQr() {
  if (recipeIndex < 0 || recipeIndex >= (int)recipes.size()) { uiPanel("SHARE", ui("RECEITA INVALIDA","INVALID RECIPE"), "", 1); return; }
  if (!offlineShareActive || offlineShareIndex != recipeIndex) startOfflineShare();
  String wifiQr = "WIFI:T:nopass;S:" + offlineShareSsid + ";;";
  QRCode qr;
  static uint8_t qrbuff[1024];
  if (!offlineShareActive || !buildQrCode(qr, qrbuff, wifiQr)) {
    uiPanel("SHARE", ui("HOTSPOT FALHOU\nTENTE DE NOVO.","HOTSPOT FAILED\nTRY AGAIN."), "", 1);
    return;
  }
  uiCanvas.fillScreen(BG);
  int quiet = 4;
  int modules = qr.size + quiet*2;
  int scale = min(sw() / modules, (sh() - 12) / modules);
  if (scale < 1) scale = 1;
  int qrPix = (qr.size + quiet*2) * scale;
  int x0 = max(0, (sw() - qrPix) / 2);
  int y0 = max(0, (sh() - 12 - qrPix) / 2);
  uiCanvas.fillRect(x0, y0, qrPix, qrPix, 0xFFFF);
  for (int y=0; y<qr.size; y++) {
    for (int x=0; x<qr.size; x++) {
      if (lgfx_qrcode_getModule(&qr, x, y)) {
        uiCanvas.fillRect(x0 + (x + quiet)*scale, y0 + (y + quiet)*scale, scale, scale, UI_BLACK);
      }
    }
  }
  uiText(1, AMBER_DIM, BG);
  String back = ui("` PARAR","` STOP");
  uiCanvas.setCursor(sw() - 6 - back.length()*6, sh()-11);
  uiCanvas.print(back);
}

Recipe txtToRecipe(String txt) {
  // Older versions opened FILE_WRITE in append mode. If that happened,
  // the last full copy is the newest recipe version.
  int lastHeader=max(txt.lastIndexOf("MISEDECK RECIPE"), txt.lastIndexOf("CYBER CHEF RECIPE"));
  if (lastHeader>0) txt=txt.substring(lastHeader);
  Recipe r; r.id = nowId(); r.name=ui("SEM NOME","UNTITLED"); r.category="OTHER";
  int pos=0; Prep *currentPrep = nullptr; bool inIng=false;
  bool sawHeader=false;
  while (pos < txt.length()) {
    int nl = txt.indexOf('\n', pos);
    if (nl < 0) nl = txt.length();
    String line = txt.substring(pos, nl); line.trim(); pos = nl+1;
    if (!line.length()) continue;
    if (line == "MISEDECK RECIPE" || line == "CYBER CHEF RECIPE") { sawHeader=true; continue; }
    if (line == "[INGREDIENTS]" || line == "[INGREDIENTES]") { inIng=true; currentPrep=nullptr; continue; }
    if (line == "[PREP]" || line == "[PREPARO]") { r.composite=true; r.preps.push_back(Prep()); currentPrep=&r.preps.back(); inIng=false; continue; }
    if (line.indexOf('|') > 0) {
      int a=line.indexOf('|'), b=line.indexOf('|', a+1);
      if (b < 0) continue;
      Ingredient ing; ing.name=cleanName(line.substring(0,a)); ing.weight=line.substring(a+1,b).toFloat();
      if (!ing.name.length() || ing.weight <= 0) continue;
      if (currentPrep) currentPrep->ingredients.push_back(ing); else r.ingredients.push_back(ing);
      continue;
    }
    int c=line.indexOf(':');
    if (c<0) {
      if (!sawHeader && (r.name=="SEM NOME" || r.name=="UNTITLED")) r.name=cleanName(line);
      continue;
    }
    String k=line.substring(0,c); String v=line.substring(c+1); k.trim(); v.trim();
    if (currentPrep && (k=="NAME" || k=="NOME")) currentPrep->name=cleanName(v);
    else if (k=="TYPE" || k=="TIPO") r.composite = (v == "COMPOSITE" || v == "COMPOSTA");
    else if (k=="ID") r.id=v;
    else if (k=="NAME" || k=="NOME") r.name=cleanName(v);
    else if (k=="CATEGORY" || k=="CATEGORIA") {
      r.category=normalizeCategory(v);
    }
    else if (k=="FAVORITE" || k=="FAVORITA") r.favorite=(v=="YES" || v=="SIM");
  }
  recalc(r);
  return r;
}

bool recipeIsValid(const Recipe& r) {
  if (!r.name.length()) return false;
  if (!r.composite) {
    if (!r.ingredients.size()) return false;
    for (auto &ing : r.ingredients) if (!ing.name.length() || ing.weight <= 0) return false;
    return sumIngredients(r.ingredients) > 0;
  }
  if (!r.preps.size()) return false;
  for (auto &prep : r.preps) {
    if (!prep.name.length() || !prep.ingredients.size()) return false;
    for (auto &ing : prep.ingredients) if (!ing.name.length() || ing.weight <= 0) return false;
  }
  return r.total > 0;
}

bool sameRecipeContent(const Recipe& a, const Recipe& b) {
  if (a.name != b.name || a.category != b.category || a.favorite != b.favorite || a.composite != b.composite) return false;
  if (a.composite) {
    if (a.preps.size() != b.preps.size()) return false;
    for (int p=0; p<(int)a.preps.size(); p++) {
      if (a.preps[p].name != b.preps[p].name || a.preps[p].ingredients.size() != b.preps[p].ingredients.size()) return false;
      for (int i=0; i<(int)a.preps[p].ingredients.size(); i++) {
        if (a.preps[p].ingredients[i].name != b.preps[p].ingredients[i].name || fabsf(a.preps[p].ingredients[i].weight - b.preps[p].ingredients[i].weight) > 0.05f) return false;
      }
    }
    return true;
  }
  if (a.ingredients.size() != b.ingredients.size()) return false;
  for (int i=0; i<(int)a.ingredients.size(); i++) {
    if (a.ingredients[i].name != b.ingredients[i].name || fabsf(a.ingredients[i].weight - b.ingredients[i].weight) > 0.05f) return false;
  }
  return true;
}

bool saveRecipe(Recipe& r) {
  if (!sdOK || !recipeIsValid(r)) return false;
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

bool persistRecipeUpdate(int index, const Recipe& previous) {
  if (index < 0 || index >= (int)recipes.size()) return false;
  if (saveRecipe(recipes[index])) return true;
  recipes[index] = previous;
  flash(ui("FALHA AO SALVAR SD","SD SAVE FAILED"), true);
  return false;
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
  if (recipes.size()) return;
  Recipe a; a.id="focaccia-andre"; a.name=ui("FOCACCIA DO ANDRE","ANDRE'S FOCACCIA"); a.category="DOUGHS"; a.favorite=true;
  a.ingredients.push_back({ui("AGUA","WATER"),293});
  a.ingredients.push_back({ui("ACUCAR","SUGAR"),8});
  a.ingredients.push_back({ui("AZEITE","OLIVE OIL"),20});
  a.ingredients.push_back({ui("FARINHA DE TRIGO","WHEAT FLOUR"),366});
  a.ingredients.push_back({ui("FERMENTO","YEAST"),3.3});
  a.ingredients.push_back({ui("SAL","SALT"),10});
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
  r.name = ui("BOLO DO PORTAL","PORTAL CAKE");
  r.category = "SWEETS";
  r.favorite = true;
  r.composite = true;
  Prep massa; massa.name = ui("MASSA","CAKE BATTER");
  massa.ingredients.push_back({ui("FARINHA DE TRIGO","WHEAT FLOUR"),200});
  massa.ingredients.push_back({ui("CACAU EM PO","COCOA POWDER"),57});
  massa.ingredients.push_back({ui("BICARBONATO DE SODIO","BAKING SODA"),7});
  massa.ingredients.push_back({ui("SAL KOSHER","KOSHER SALT"),3});
  massa.ingredients.push_back({ui("ACUCAR","SUGAR"),300});
  massa.ingredients.push_back({ui("GORDURA VEGETAL","VEGETABLE SHORTENING"),92});
  massa.ingredients.push_back({ui("OVOS","EGGS"),100});
  massa.ingredients.push_back({ui("EXTRATO DE BAUNILHA","VANILLA EXTRACT"),5});
  massa.ingredients.push_back({ui("LEITELHO","BUTTERMILK"),368});
  massa.ingredients.push_back({ui("LICOR DE CEREJA","CHERRY LIQUEUR"),120});
  Prep recheio; recheio.name = ui("RECHEIO","FILLING");
  recheio.ingredients.push_back({ui("LICOR DE CEREJA","CHERRY LIQUEUR"),60});
  recheio.ingredients.push_back({ui("CEREJAS AZEDAS","SOUR CHERRIES"),680});
  recheio.ingredients.push_back({ui("CREME DE LEITE FRESCO","HEAVY CREAM"),720});
  recheio.ingredients.push_back({ui("ACUCAR DE CONFEITEIRO","POWDERED SUGAR"),30});
  recheio.ingredients.push_back({ui("CACAU EM PO","COCOA POWDER"),16});
  Prep finalizacao; finalizacao.name = ui("FINALIZACAO","GARNISH");
  finalizacao.ingredients.push_back({ui("CHOCOLATE MEIO AMARGO","SEMISWEET CHOCOLATE"),100});
  finalizacao.ingredients.push_back({ui("CEREJAS MARRASQUINO","MARASCHINO CHERRIES"),40});
  r.preps.push_back(massa);
  r.preps.push_back(recheio);
  r.preps.push_back(finalizacao);
  recalc(r);
  return r;
}
bool unlockPortalCake() {
  if (hasRecipeId("portal-cake")) {
    flash("THE CAKE IS REAL");
    return false;
  }
  Recipe r = portalCakeRecipe();
  recipes.push_back(r);
  recipeIndex = recipes.size() - 1;
  if (!saveRecipe(recipes[recipeIndex])) {
    recipes.pop_back();
    recipeIndex = -1;
    flash(ui("FALHA AO SALVAR NO SD","SD SAVE FAILED"), true);
    return false;
  }
  beepRecipe();
  flash("THE CAKE IS REAL");
  return true;
}
Recipe shokupanRecipe() {
  Recipe r;
  r.id = "shokupan-andre";
  r.name = "SHOKUPAN";
  r.category = "DOUGHS";
  r.favorite = true;
  r.composite = true;
  Prep massa; massa.name = ui("MASSA","DOUGH");
  massa.ingredients.push_back({ui("FARINHA DE TRIGO","WHEAT FLOUR"),320});
  massa.ingredients.push_back({ui("FERMENTO","YEAST"),9});
  massa.ingredients.push_back({ui("SAL","SALT"),3});
  massa.ingredients.push_back({ui("LEITE","MILK"),120});
  massa.ingredients.push_back({ui("ACUCAR","SUGAR"),56});
  massa.ingredients.push_back({ui("MANTEIGA","BUTTER"),42});
  massa.ingredients.push_back({ui("OVO","EGG"),50});
  Prep tangzhong; tangzhong.name = "TANGZHONG";
  tangzhong.ingredients.push_back({ui("FARINHA DE TRIGO","WHEAT FLOUR"),20});
  tangzhong.ingredients.push_back({ui("AGUA","WATER"),27});
  tangzhong.ingredients.push_back({ui("LEITE","MILK"),40});
  r.preps.push_back(massa);
  r.preps.push_back(tangzhong);
  recalc(r);
  return r;
}
bool unlockShokupan() {
  // The Cardputer default display font does not render Japanese glyphs reliably.
  const char* msg = "OHAYOU, PAN-YA SAN!";
  if (hasRecipeId("shokupan-andre")) {
    flash(msg);
    return false;
  }
  Recipe r = shokupanRecipe();
  recipes.push_back(r);
  recipeIndex = recipes.size() - 1;
  if (!saveRecipe(recipes[recipeIndex])) {
    recipes.pop_back();
    recipeIndex = -1;
    flash(ui("FALHA AO SALVAR NO SD","SD SAVE FAILED"), true);
    return false;
  }
  beepRecipe();
  flash(msg);
  return true;
}

std::vector<int> recipeIndicesFor(String cat) {
  std::vector<int> idx;
  for (int i=0;i<(int)recipes.size();i++) {
    if (cat=="FAVORITES" && recipes[i].favorite) idx.push_back(i);
    else if (recipes[i].category == cat) idx.push_back(i);
  }
  // Favorites first, then simple alphabetical order.
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
  uiCanvas.setCursor(x, y);
  uiCanvas.print(s);
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
void drawFocusOverview(String title, String meta, String name, String weight, int idx, int total, String foot) {
  uiFrame(title, foot);
  uiText(1, AMBER_DIM, BG);
  uiCanvas.setCursor(6, 21);
  uiCanvas.print(clipText(meta, 25));
  String pos = total > 0 ? String(idx + 1) + "/" + String(total) : "0/0";
  uiCanvas.setCursor(sw() - 6 - pos.length() * 6, 21);
  uiCanvas.print(pos);

  if (total <= 0) {
    drawCenteredText(ui("SEM ITENS","NO ITEMS"), 58, 2, AMBER);
    return;
  }

  name.toUpperCase();
  int maxChars = 18;
  String visibleName=marqueeText(name,maxChars*2);
  String line1=visibleName.substring(0,min(maxChars,(int)visibleName.length()));
  String line2=visibleName.length()>maxChars?visibleName.substring(maxChars):"";
  drawCenteredText(line1, line2.length() ? 43 : 51, 2, AMBER);
  if (line2.length()) drawCenteredText(line2, 67, 2, AMBER);

  uiText(3, AMBER_BRIGHT, BG);
  int x = max(0, (sw() - (int)weight.length() * 18) / 2);
  uiCanvas.setCursor(x, 91);
  uiCanvas.print(weight);
}
void drawBootScreen(String title, String slogan, bool cursorOn=false) {
  uiCanvas.fillScreen(BG);
  drawCenteredText(title + (cursorOn ? "_" : ""), 42, 2, AMBER);
  drawCenteredWrapped(slogan, 76, 1, AMBER);
  uiText(1, AMBER_DIM, BG);
  String hint = ui("OK ENTRA","OK ENTER");
  uiCanvas.setCursor(sw() - 6 - hint.length()*6, sh()-11);
  uiCanvas.print(hint);
  uiCanvas.pushSprite(0,0);
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
  if (!soundOn || soundVolume <= 0) return;
  if (noteStep >= total) {
    if (noteStep == total) {
      M5Cardputer.Speaker.stop();
      applySoundVolume();
      noteStep++;
    }
    return;
  }
  int idx = noteStep % count;
  if (!lastNote || millis() - lastNote >= (unsigned long)OPENING_DURATIONS[idx] + OPENING_NOTE_GAP_MS) {
    uint8_t oldVol = speakerVolumeValue();
    uint8_t motifVol = min<uint8_t>(oldVol, OPENING_VOLUME);
    M5Cardputer.Speaker.setVolume(motifVol);
    if (OPENING_NOTES[idx] > 0) M5Cardputer.Speaker.tone(OPENING_NOTES[idx], OPENING_DURATIONS[idx]);
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
  String title = "Mise_Deck";
  String slogan = ui("\"Mise en place de bolso para o Cardputer\"","\"A pocket mise en place for the Cardputer\"");
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
    "MISE_DECK BOOT\n--------------------\n",
    "INIT DISPLAY....OK\n",
    "MAP TECLADO.....OK\n",
    "MOUNT SD CARD...OK\n",
    "LOAD RECIPES...OK\n"
  };
  String acc="";
  for (int i=0;i<5;i++) { acc += lines[i]; drawText(acc); bootDelay(i==0?700:850); if (state==ST_MAIN) return; }
  String g1[] = {"%$#@!_SYS_BOOT\nCÂ¥B3R_CH3F_ERR\nREADY_//_OK", "@#!_CORE_STREAM\nSYS//HANDOFF_\nC4RD//CHEF", "NOISE_MAP_OK\n%$READY$%\n"};
  for (int i=0;i<3;i++) { drawText(g1[i]); bootDelay(160); if (state==ST_MAIN) return; }
  M5Cardputer.Display.fillScreen(BG);
  M5Cardputer.Display.drawPng(CHEF_SPLASH_PNG, CHEF_SPLASH_PNG_SIZE, 0, 0);
  bootDelay(1450);
  if (state==ST_MAIN) return;
  String slogan = "O Seu Canivete Suico\nDigital Gastronomico";
  String typed="";
  for (int i=0;i<=slogan.length();i++) {
    typed = slogan.substring(0,i);
    drawText(" [ MISE_DECK " + String(FW_VERSION) + " ]\n--------------------\n" + typed + "_\n--------------------");
    bootDelay(58);
    if (state==ST_MAIN) return;
  }
  bootDelay(900);
  beepOK();
  go(ST_MAIN);
#endif
}

bool hasHidKey(const Keyboard_Class::KeysState& keys, uint8_t code) {
  for (uint8_t key : keys.hid_keys) if (key == code) return true;
  return false;
}

bool hasKeyChar(const Keyboard_Class::KeysState& keys, char target) {
  for (char raw : keys.word) {
    char c=raw;
    if(c>='a'&&c<='z')c-=32;
    char t=target;
    if(t>='a'&&t<='z')t-=32;
    if(c==t)return true;
  }
  return false;
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
  e.left=hasHidKey(ks,0x50); e.right=hasHidKey(ks,0x4F);
  e.down=hasHidKey(ks,0x51); e.up=hasHidKey(ks,0x52);
  e.back=hasHidKey(ks,0x29);
  bool fn = ks.fn;
  if (fn) {
    e.up |= hasKeyChar(ks,'L');
    e.down |= hasKeyChar(ks,'M');
    e.left |= hasKeyChar(ks,'N');
    e.right |= hasKeyChar(ks,',') || hasKeyChar(ks,'.') || hasKeyChar(ks,'/');
    return e;
  }
  if (ks.space) e.ch=' ';
  for (auto c: ks.word) {
    if (!wifiTyping && c>='a' && c<='z') c -= 32;
    if (c=='`' || c==27) e.back=true;
    else if (editing && !wifiTyping && (c==',' || c=='<')) e.left=true;
    else if (editing && !wifiTyping && (c=='/' || c=='?')) e.right=true;
    else if (!editing && !wifiTyping && (c==';' || c==':')) e.up=true;
    else if (!editing && !wifiTyping && (c=='.' || c=='>')) e.down=true;
    else if (!editing && !wifiTyping && (c==',' || c=='<')) e.left=true;
    else if (!editing && !wifiTyping && (c=='/' || c=='?')) e.right=true;
    else if(c!=' ') e.ch = c;
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
    else beepBack();
  }
  bool enterNow = e.enter;
  if (e.right) {
    if ((page+1)*MAX_VISIBLE<total) { page++; selected=0; moved=true; }
    else beepBack();
  }
  if (moved) { resetMarquee(); beepNav(); render(); }
  if (enterNow && onEnter) { toneMs(1040, 18); onEnter(); }
}
void focusMove(int delta, int total) {
  if (total <= 0) { beepBack(); return; }
  int idx = gi() + delta;
  if (idx < 0) idx = 0;
  if (idx >= total) idx = total - 1;
  if (idx == gi()) { beepBack(); return; }
  page = idx / MAX_VISIBLE;
  selected = idx % MAX_VISIBLE;
  resetMarquee();
  beepNav();
  render();
}

void renderMessageIfAny() {
  if (messageUntil && millis() < messageUntil) { drawText(header(ui("SISTEMA","SYSTEM")) + "\n" + messageText); }
}
void flash(String m, bool err) {
  messageText = m; messageUntil = millis() + 700; if (err) beepErr(); else beepOK();
  drawText(header(ui("SISTEMA","SYSTEM")) + "\n" + messageText);
}

void render() {
  if (messageUntil && millis() < messageUntil) return;
  FramePresenter frame;
  messageUntil = 0;
  marqueeActive=false;
  String out;
  switch(state) {
    case ST_MAIN: uiMenu("Mise_Deck " + String(FW_VERSION), {ui("RECEITAS","RECIPES"),ui("FERRAMENTAS","TOOLS"),"WIFI",ui("SISTEMA","SYSTEM")}); return;
    case ST_RECIPES: uiMenu(ui("RECEITAS","RECIPES"), {ui("FAVORITAS","FAVORITES"),ui("BIBLIOTECA","LIBRARY"),ui("NOVA","NEW"),ui("RAPIDO","QUICK")}); return;
    case ST_CATEGORIES: {
      std::vector<String> it; for(int i=0;i<CAT_COUNT;i++) it.push_back(categoryLabel(cats[i]));
      uiMenu(ui("BIBLIOTECA","LIBRARY"), it); return;
    }
    case ST_RECIPE_LIST: {
      auto idx=recipeIndicesFor(currentCategory);
      if (!idx.size()) uiPanel(categoryLabel(currentCategory), ui("SEM RECEITAS\n\nCrie uma receita\nou importe arquivos TXT.","NO RECIPES\n\nCreate a new recipe\nor import TXT files."));
      else { std::vector<String> it; for(auto n:idx) it.push_back(String(recipes[n].favorite?"* ":"  ")+recipes[n].name); uiMenu(categoryLabel(currentCategory), it); }
      return;
    }
    case ST_RECIPE_VIEW: {
      Recipe &r=recipes[recipeIndex];
      if (r.composite) {
        int total = r.preps.size();
        int idx = total ? min(gi(), total - 1) : 0;
        if (total) { page = idx / MAX_VISIBLE; selected = idx % MAX_VISIBLE; }
        String name = total ? r.preps[idx].name : ui("SEM PREPAROS","NO PREPS");
        String weight = total ? fw(r.preps[idx].total) : "--";
        drawFocusOverview(String(r.favorite?"* ":"") + r.name, "TOTAL " + fw(r.total), name, weight, idx, total, ui("; . , / ITEM  OK ABRE  SPC ACOES  `","; . , / ITEM  OK OPEN  SPC ACTIONS  `"));
      } else {
        float base=sumIngredients(r.ingredients); if(base<=0)base=1;
        int total = r.ingredients.size();
        int idx = total ? min(gi(), total - 1) : 0;
        if (total) { page = idx / MAX_VISIBLE; selected = idx % MAX_VISIBLE; }
        String name = total ? r.ingredients[idx].name : ui("SEM INGREDIENTES","NO INGREDIENTS");
        String weight = total ? fw(r.ingredients[idx].weight*r.total/base) : "--";
        drawFocusOverview(String(r.favorite?"* ":"") + r.name, "TOTAL " + fw(r.total), name, weight, idx, total, ui("; . , / ITEM  OK/SPC ACOES  TAB FAV","; . , / ITEM  OK/SPC ACTIONS  TAB FAV"));
      }
      return;
    }
    case ST_SHARE_QR: uiShareQr(); return;
    case ST_PREP_VIEW: {
      Recipe &r=recipes[recipeIndex]; Prep &p=r.preps[prepIndex];
      float base=sumIngredients(p.ingredients); if(base<=0)base=1;
      int total = p.ingredients.size();
      int idx = total ? min(gi(), total - 1) : 0;
      if (total) { page = idx / MAX_VISIBLE; selected = idx % MAX_VISIBLE; }
      String name = total ? p.ingredients[idx].name : ui("SEM INGREDIENTES","NO INGREDIENTS");
      String weight = total ? fw(p.ingredients[idx].weight*p.total/base) : "--";
      drawFocusOverview(p.name, String(ui("TOTAL PREPARO ","PREP TOTAL ")) + fw(p.total), name, weight, idx, total, ui("; . , / ITEM  OK/SPC ACOES  ` VOLTA","; . , / ITEM  OK/SPC ACTIONS  ` BACK"));
      return;
    }
    case ST_ACTIONS: {
      Recipe &r=recipes[recipeIndex];
      if(r.composite) uiMenu(ui("ACOES","ACTIONS"), {ui("ALTERAR TOTAL","CHANGE TOTAL"),ui("PREPAROS","PREPS"),ui("DUPLICAR","DUPLICATE"),"TIMER","SHARE",ui("APAGAR","DELETE")});
      else uiMenu(ui("ACOES","ACTIONS"), {ui("ALTERAR TOTAL","CHANGE TOTAL"),ui("ESCALAR ING.","SCALE BY ING."),ui("INGREDIENTES","INGREDIENTS"),ui("DUPLICAR","DUPLICATE"),"TIMER","SHARE",ui("APAGAR","DELETE")});
      return;
    }
    case ST_PREP_ACTIONS: uiMenu(ui("ACOES PREPARO","PREP ACTIONS"), {ui("ALTERAR TOTAL","CHANGE TOTAL"),ui("ESCALAR ING.","SCALE BY ING."),ui("INGREDIENTES","INGREDIENTS"),ui("DUPLICAR PREPARO","DUPLICATE PREP"),ui("REMOVER PREPARO","REMOVE PREP")}); return;
    case ST_PREP_MENU: {
      Recipe &r=recipes[recipeIndex];
      std::vector<String> it={ui("ADICIONAR PREPARO","ADD PREP")};
      for(auto &p:r.preps) it.push_back(p.name);
      uiMenu(ui("PREPAROS","PREPS"), it);
      return;
    }
    case ST_ING_MENU: uiMenu(ui("INGREDIENTES","INGREDIENTS"), {ui("ADICIONAR","ADD"),ui("ALTERAR PESO","CHANGE WEIGHT"),ui("REMOVER","REMOVE"),ui("RENOMEAR ING.","RENAME ING.")}); return;
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
      uiFrame(pending.title, ui("SETAS SELEC  OK CONFIRMA  ` CANCELA","ARROWS SELECT  OK CONFIRM  ` CANCEL"));
      uiText(1, AMBER, BG);
      uiCanvas.setCursor(6, 24);
      uiCanvas.print(clipText(pending.info, 36));
      std::vector<String> yesno={ui("NAO","NO"),ui("SIM","YES")};
      int y=60;
      for(int i=0;i<2;i++) {
        bool sel=(i==selected);
        if(sel) uiCanvas.fillRect(18+i*108, y-4, 96, 30, AMBER);
        else uiCanvas.drawRect(18+i*108, y-4, 96, 30, AMBER_DARK);
        uiText(2, sel?UI_BLACK:AMBER, sel?AMBER:BG);
        uiCanvas.setCursor(42+i*108, y+3);
        uiCanvas.print(yesno[i]);
      }
      return;
    }
    case ST_NEW_TYPE: uiMenu(ui("NOVA RECEITA","NEW RECIPE"), {ui("SIMPLES","SIMPLE"),ui("COMPOSTA","COMPOSITE")}); return;
    case ST_NEW_CAT: { std::vector<String> it; for(int i=0;i<REAL_CAT_COUNT;i++) it.push_back(categoryLabel(realCats[i])); uiMenu(ui("CATEGORIA","CATEGORY"), it); return; }
    case ST_NEW_PREP_NEXT: uiMenu(ui("PREPARO COMPLETO","PREP COMPLETE"), {ui("ADICIONAR OUTRO","ADD ANOTHER"),ui("SALVAR RECEITA","SAVE RECIPE")}); return;
    case ST_ADD_ING_NAME: {
      bool prepFlow=(hasDraft&&draft.composite)||addingExistingPrep;
      uiInput(ui("NOVO INGREDIENTE","NEW INGREDIENT"), ui("NOME","NAME"), inputBuf, prepFlow?ui("VAZIO+OK FINALIZA","EMPTY+OK FINISH"):ui("VAZIO+OK SALVA","EMPTY+OK SAVE"), false);
      return;
    }
    case ST_ADD_ING_WEIGHT: uiInput(ui("PESO DO INGREDIENTE","INGREDIENT WEIGHT"), ui("GRAMAS","GRAMS"), inputBuf, pending.text, true); return;
    case ST_QUICK: {
      float t=0; for(auto w:quickWeights)t+=w;
      out="TOTAL: "+fw(t)+"\n\n";
      int start=max(0,(int)quickWeights.size()-3); for(int i=start;i<(int)quickWeights.size();i++) out += String(i+1)+") "+fw(quickWeights[i])+"\n";
      out += String("\n")+ui("PESO: ","WEIGHT: ")+inputBuf+"_";
      uiPanel(ui("RAPIDO","QUICK"), out, ui("OK ADICIONA  VAZIO+OK FIM","OK ADD  EMPTY+OK DONE"), 1); return;
    }
    case ST_QUICK_TARGET: uiInput(ui("TOTAL RAPIDO","QUICK TOTAL"), ui("NOVO TOTAL","NEW TOTAL"), inputBuf, String(ui("ORIGINAL: ","ORIGINAL: "))+fw(quickOriginal), true); return;
    case ST_QUICK_RESULT: {
      out="TOTAL: "+fw(quickTarget)+"\n\n"; int start=page*MAX_VISIBLE;
      for(int i=0;i<MAX_VISIBLE && start+i<(int)quickWeights.size();i++) out += String(start+i+1)+") "+fw(quickWeights[start+i]*quickTarget/quickOriginal)+"\n";
      uiPanel(ui("RESULTADO RAPIDO","QUICK RESULT"), out, ui("` SAIR","` EXIT"), 1); return;
    }
    case ST_TOOLS: uiMenu(ui("FERRAMENTAS","TOOLS"), {"TIMER",ui("CONVERTER","CONVERT")}); return;
    case ST_TIMER_MENU: uiMenu("TIMER", {ui("PERSONALIZADO","CUSTOM"),"01 MIN","03 MIN","05 MIN","10 MIN","15 MIN"}); return;
    case ST_TIMER_CUSTOM: uiInput(ui("TIMER PERSONALIZADO","CUSTOM TIMER"), ui("MINUTOS","MINUTES"), inputBuf, "", true); return;
    case ST_TIMER_RUN: uiTimer(); return;
    case ST_TIMER_DONE: uiPanel(ui("TIMER FINALIZADO","TIMER DONE"), ui("TEMPO ESGOTADO\n\nOK ou ` para parar\no alarme.","TIME IS UP\n\nOK or ` to stop\nthe alarm."), ui("OK PARA  ` VOLTAR","OK STOP  ` BACK"), 1); return;
    case ST_CONV: uiMenu(ui("CONVERTER","CONVERT"), {ui("TEMPERATURA","TEMPERATURE"),ui("PESO","WEIGHT")}); return;
    case ST_TEMP_DIR: uiMenu(ui("TEMPERATURA","TEMPERATURE"), {ui("C PARA F","C TO F"),ui("F PARA C","F TO C")}); return;
    case ST_WEIGHT_FROM: uiMenu(ui("PESO DE","WEIGHT FROM"), {"g","kg","lb","oz"}); return;
    case ST_WEIGHT_TO: uiMenu(ui("PESO PARA","WEIGHT TO"), {"g","kg","lb","oz"}, String(ui("DE: ","FROM: "))+pending.text+ui("  OK SELECIONA","  OK SELECT")); return;
    case ST_RESULT: uiPanel(ui("RESULTADO","RESULT"), pending.info); return;
    case ST_WIFI: {
      String foot = WiFi.status()==WL_CONNECTED ? "ON " + wifiIp() : (wifiSsid.length() ? String(ui("SALVA ","SAVED ")) + wifiSsid : ui("OFF  configure e conecte","OFF  set up and connect"));
      uiMenu("WIFI", {ui("REDE + SENHA","NETWORK + PASS"),ui("CONECTAR","CONNECT"),"STATUS",ui("DESCONECTAR","DISCONNECT")}, foot);
      return;
    }
    case ST_WIFI_SSID: uiInput("WIFI", ui("NOME DA REDE","NETWORK NAME"), inputBuf, ui("SSID / rede de casa","SSID / home network"), false); return;
    case ST_WIFI_PASS: {
      String masked="";
      for(int i=0;i<inputBuf.length();i++) masked += "*";
      uiInput("WIFI", ui("SENHA","PASSWORD"), masked, wifiSsid, false);
      return;
    }
    case ST_WIFI_STATUS: {
      String body;
      if (WiFi.status()==WL_CONNECTED) {
        body = String(ui("CONECTADO\nRede: ","CONNECTED\nNetwork: ")) + wifiSsid + "\nIP: " + wifiIp() + ui("\n\nAbra:\nmisedeck.local\nou ","\n\nOpen:\nmisedeck.local\nor ") + wifiIp();
      } else {
        body = ui("DESCONECTADO\n\nConfigure rede/senha\ne pressione conectar.","DISCONNECTED\n\nSet network/password\nand press connect.");
      }
      uiPanel("WIFI STATUS", body, ui("OK ATUALIZA  ` VOLTAR","OK REFRESH  ` BACK"), 1);
      return;
    }
    case ST_SYSTEM: uiMenu(ui("SISTEMA","SYSTEM"), {"STATUS",String(ui("IDIOMA: ","LANGUAGE: "))+(portuguese?"PORTUGUES":"ENGLISH"),ui("BATERIA","BATTERY"),ui("SOBRE","ABOUT"),String(ui("COR: ","COLOR: "))+accentName(accentColor),"BACKUP TXT",ui("IMPORTAR SD","IMPORT SD"),ui("LIMPAR DADOS","RESET DATA"),ui("SOM","SOUND")}); return;
    case ST_STATUS: uiPanel("STATUS", "SD: "+String(sdOK?"OK":"FAIL")+String(ui("\nRECEITAS: ","\nRECIPES: "))+String(recipes.size())+String(ui("\nSOM: ","\nSOUND: "))+String(soundOn?ui("LIGADO","ON"):ui("DESLIGADO","OFF"))+" VOL "+String(soundVolume)+String(ui("/10\nVERSAO: ","/10\nVERSION: "))+FW_VERSION, "", 1); return;
    case ST_BATTERY: uiBatteryScreen(); return;
    case ST_ABOUT: {
      uiFrame(ui("SOBRE","ABOUT"));
      drawCenteredText("Mise_Deck", 22, 1, AMBER);
      drawCenteredText(ui("Conceito: Andre Fuentes","Concept: Andre Fuentes"), 38, 1, AMBER);
      drawCenteredText("@anfuentz", 50, 1, AMBER_DIM);
      drawCenteredText(ui("Vibecodeado por Codex","Vibecoded by Codex"), 66, 1, AMBER);
      drawCenteredText(ui("\"nos somos os criadores", "\"we are the music"), 84, 1, AMBER_DIM);
      drawCenteredText(ui("de musica e os sonhadores", "makers and we are"), 96, 1, AMBER_DIM);
      drawCenteredText(ui("de sonhos\"", "the dreamers of dreams\""), 108, 1, AMBER_DIM);
      return;
    }
    case ST_LANGUAGE: uiMenu(ui("IDIOMA","LANGUAGE"), {"ENGLISH","PORTUGUES"},ui("NAVEGUE PARA PREVER  OK SALVA  ` CANCELA","MOVE TO PREVIEW  OK SAVE  ` CANCEL")); return;
    case ST_THEME: {
      std::vector<String> items; for(int i=0;i<ACCENT_COLOR_COUNT;i++)items.push_back(accentName(i));
      uiMenu(ui("COR","COLOR"),items,ui("NAVEGUE PARA PREVER  OK SALVA  ` CANCELA","MOVE TO PREVIEW  OK SAVE  ` CANCEL"));
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
  if (e.back) { beepBack(); go(inputReturn); return; }
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
        if (inputBuf.length()>MAX_NAME_LENGTH) inputBuf=inputBuf.substring(0,MAX_NAME_LENGTH);
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

void discardDraft() {
  draft = Recipe();
  hasDraft = false;
  prepIndex = -1;
  ingIndex = -1;
  addingExistingPrep = false;
  existingPrepBackupValid = false;
  setInput("");
}

void cbScaleRecipe(float v) {
  if(v<=0 || recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("VALOR INVALIDO","INVALID VALUE"),true);return;}
  Recipe previous=recipes[recipeIndex];
  scaleRecipe(recipes[recipeIndex],v);
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  flash(ui("TOTAL ALTERADO","TOTAL CHANGED")); go(ST_RECIPE_VIEW);
}
void cbScalePrep(float v) {
  if(v<=0 || recipeIndex<0 || recipeIndex>=(int)recipes.size() || prepIndex<0 || prepIndex>=(int)recipes[recipeIndex].preps.size()){flash(ui("VALOR INVALIDO","INVALID VALUE"),true);return;}
  Recipe previous=recipes[recipeIndex];
  scalePrep(recipes[recipeIndex].preps[prepIndex],v); recalc(recipes[recipeIndex]);
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  flash(ui("PREPARO ALTERADO","PREP CHANGED")); go(ST_PREP_VIEW);
}
void cbScaleByIngredient(float v) {
  if(v<=0 || recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("VALOR INVALIDO","INVALID VALUE"),true);return;}
  Recipe previous=recipes[recipeIndex];
  Recipe &r=recipes[recipeIndex];
  std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex<0 || ingIndex>=(int)vec->size()){flash(ui("ING. INVALIDO","INVALID ING."),true);go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);return;}
  float old=(*vec)[ingIndex].weight;
  if(old<=0){flash(ui("BASE INVALIDA","INVALID BASE"),true);go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);return;}
  float factor=v/old;
  if(r.composite && prepIndex>=0) {
    scalePrep(r.preps[prepIndex], r.preps[prepIndex].total * factor);
    recalc(r);
    if(!persistRecipeUpdate(recipeIndex,previous))return;
    flash(ui("PREPARO ESCALADO","PREP SCALED"));
    go(ST_PREP_VIEW);
  } else {
    float base=r.total>0?r.total:sumIngredients(r.ingredients);
    scaleRecipe(r, base * factor);
    if(!persistRecipeUpdate(recipeIndex,previous))return;
    flash(ui("RECEITA ESCALADA","RECIPE SCALED"));
    go(ST_RECIPE_VIEW);
  }
}
void cbSetIngWeight(float v) {
  if(v<=0 || recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("PESO INVALIDO","INVALID WEIGHT"),true);return;}
  Recipe previous=recipes[recipeIndex];
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex<0 || ingIndex>=(int)vec->size()){flash(ui("ING. INVALIDO","INVALID ING."),true);return;}
  (*vec)[ingIndex].weight=r1(v); recalc(r);
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  flash(ui("PESO ALTERADO","WEIGHT CHANGED")); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void cbRenameIng(String s) {
  if(!s.length() || recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("NOME INVALIDO","INVALID NAME"),true);return;}
  Recipe previous=recipes[recipeIndex];
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex<0 || ingIndex>=(int)vec->size()){flash(ui("ING. INVALIDO","INVALID ING."),true);return;}
  (*vec)[ingIndex].name=s;
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  flash(ui("ING. RENOMEADO","ING. RENAMED")); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void doRemoveIng() {
  if(recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("RECEITA INVALIDA","INVALID RECIPE"),true);return;}
  Recipe previous=recipes[recipeIndex];
  Recipe &r=recipes[recipeIndex]; std::vector<Ingredient>* vec = (r.composite && prepIndex>=0) ? &r.preps[prepIndex].ingredients : &r.ingredients;
  if(ingIndex<0 || ingIndex>=(int)vec->size()){flash(ui("ING. INVALIDO","INVALID ING."),true);return;}
  if(vec->size()<=1){flash("MIN. 1 ING.",true);return;}
  vec->erase(vec->begin()+ingIndex); recalc(r);
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  ingIndex=-1; flash(ui("ING. REMOVIDO","ING. REMOVED")); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);
}
void cbNewRecipeName(String s) { draft.name=s.length()?s:ui("NOVA RECEITA","NEW RECIPE"); setInput(""); go(ST_NEW_CAT); }
void cbNewPrepName(String s) { Prep p; p.name=s.length()?s:ui("PREPARO","PREP"); draft.preps.push_back(p); prepIndex=draft.preps.size()-1; setInput(""); go(ST_ADD_ING_NAME); }
void cbAddExistingPrepName(String s) {
  if(recipeIndex<0||recipeIndex>=(int)recipes.size())return;
  existingPrepBackup=recipes[recipeIndex];
  existingPrepBackupValid=true;
  Prep p; p.name=s.length()?s:ui("PREPARO","PREP");
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
    if(recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("RECEITA INVALIDA","INVALID RECIPE"),true);return;}
    Recipe previous=recipes[recipeIndex];
    Recipe &r=recipes[recipeIndex]; if(r.composite && prepIndex>=0) r.preps[prepIndex].ingredients.push_back(ing); else r.ingredients.push_back(ing);
    recalc(r);
    if(addingExistingPrep){setInput("");go(ST_ADD_ING_NAME);}
    else {if(!persistRecipeUpdate(recipeIndex,previous))return; flash(ui("ING. ADICIONADO","ING. ADDED")); go(r.composite&&prepIndex>=0?ST_PREP_VIEW:ST_RECIPE_VIEW);}
  }
}
void finalizeDraft() {
  if(!hasDraft)return;
  recalc(draft); draft.storagePath="";
  if(!recipeIsValid(draft)){flash(ui("RECEITA INCOMPLETA","INCOMPLETE RECIPE"),true);return;}
  recipes.push_back(draft);
  int newIndex=recipes.size()-1;
  if(!saveRecipe(recipes[newIndex])){recipes.pop_back();flash(ui("FALHA AO SALVAR SD","SD SAVE FAILED"),true);return;}
  recipeIndex=newIndex; hasDraft=false; prepIndex=-1; ingIndex=-1;
  flash(ui("RECEITA SALVA","RECIPE SAVED")); beepRecipe(); go(ST_RECIPE_VIEW);
}
void finishExistingPrep() {
  if(recipeIndex<0||prepIndex<0)return;
  Recipe &r=recipes[recipeIndex];
  if(prepIndex>=(int)r.preps.size()||!r.preps[prepIndex].ingredients.size()){flash(ui("ADICIONE ING.","ADD ING."),true);return;}
  recalc(r);
  Recipe previous=existingPrepBackupValid?existingPrepBackup:r;
  if(!persistRecipeUpdate(recipeIndex,previous)){addingExistingPrep=false;existingPrepBackupValid=false;prepIndex=-1;go(ST_PREP_MENU);return;}
  addingExistingPrep=false; existingPrepBackupValid=false; flash(ui("PREPARO SALVO","PREP SAVED")); go(ST_PREP_VIEW);
}
void doDeleteRecipe() {
  if(!sdOK || recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("FALHA SD","SD FAIL"),true);return;}
  String path=recipes[recipeIndex].storagePath.length()?recipeDirPath(recipes[recipeIndex].storagePath):"";
  if(!path.length() || (SD.exists(path) && !SD.remove(path))){flash(ui("FALHA AO APAGAR","DELETE FAILED"),true);return;}
  SD.remove(path+".tmp"); SD.remove(path+".bak");
  recipes.erase(recipes.begin()+recipeIndex); recipeIndex=-1; prepIndex=-1; ingIndex=-1;
  flash(ui("RECEITA APAGADA","RECIPE DELETED")); go(ST_RECIPES);
}
void doResetData() {
  if(!sdOK){flash(ui("FALHA SD","SD FAIL"),true);return;}
  std::vector<String> paths; File dir=SD.open(RECIPE_DIR); File f=dir.openNextFile();
  while(f){ String n=recipeDirPath(f.name()); bool recipeData=n.endsWith(".txt")||n.endsWith(".txt.tmp")||n.endsWith(".txt.bak"); if(!f.isDirectory()&&recipeData)paths.push_back(n); f.close(); f=dir.openNextFile(); }
  dir.close();
  bool ok=true; for(auto &path:paths) if(SD.exists(path) && !SD.remove(path))ok=false;
  if(!ok){loadRecipes();flash(ui("LIMPEZA INCOMPLETA","RESET INCOMPLETE"),true);return;}
  recipes.clear(); recipeIndex=-1; prepIndex=-1; ingIndex=-1; discardDraft(); flash(ui("DADOS LIMPOS","DATA CLEARED")); go(ST_MAIN);
}
void doRemovePrep() {
  if(recipeIndex<0 || recipeIndex>=(int)recipes.size()){flash(ui("RECEITA INVALIDA","INVALID RECIPE"),true);return;}
  Recipe previous=recipes[recipeIndex]; Recipe &r=recipes[recipeIndex];
  if(r.preps.size()<=1){flash(ui("MIN. 1 PREPARO","MIN. 1 PREP"),true);go(ST_PREP_VIEW);return;}
  if(prepIndex<0 || prepIndex>=(int)r.preps.size()){flash(ui("PREPARO INVALIDO","INVALID PREP"),true);return;}
  r.preps.erase(r.preps.begin()+prepIndex); recalc(r);
  if(!persistRecipeUpdate(recipeIndex,previous))return;
  prepIndex=-1; flash(ui("PREPARO REMOVIDO","PREP REMOVED")); go(ST_RECIPE_VIEW);
}

float unitToG(float v, String u) { if(u=="g")return v; if(u=="kg")return v*1000.0; if(u=="lb")return v*453.59237; if(u=="oz")return v*28.349523; return v; }
float gToUnit(float g, String u) { if(u=="g")return g; if(u=="kg")return g/1000.0; if(u=="lb")return g/453.59237; if(u=="oz")return g/28.349523; return g; }
String weightFrom, weightTo;
void cbTempC(float v) { pending.info=String(v,1)+"C\n= "+String(v*9.0/5.0+32.0,1)+"F"; go(ST_RESULT); }
void cbTempF(float v) { pending.info=String(v,1)+"F\n= "+String((v-32.0)*5.0/9.0,1)+"C"; go(ST_RESULT); }
void cbWeight(float v) { float g=unitToG(v,weightFrom); pending.info=String(v,1)+weightFrom+"\n= "+String(gToUnit(g,weightTo),1)+weightTo; go(ST_RESULT); }
void cbTimerCustom(float v);

void exportBackup() {
  if(!sdOK){ flash(ui("FALHA SD","SD FAIL"), true); return; }
  SD.remove(BACKUP_FILE);
  File f=SD.open(BACKUP_FILE, FILE_WRITE); if(!f){ flash(ui("FALHA NO BACKUP","BACKUP FAILED"), true); return; }
  bool ok=true;
  for(auto &r:recipes){ if(!f.println(recipeToTxt(r)) || !f.println("====================")){ok=false;break;} }
  f.flush(); f.close();
  if(!ok){SD.remove(BACKUP_FILE);flash(ui("FALHA NO BACKUP","BACKUP FAILED"),true);return;}
  flash(ui("BACKUP EXPORTADO","BACKUP EXPORTED"));
}
void importTxts() {
  if(!sdOK){ flash(ui("FALHA SD","SD FAIL"), true); return; }
  ensureDirs(); int count=0, skipped=0, failed=0; File dir=SD.open(IMPORT_DIR); File f=dir.openNextFile();
  while(f){
    String n=f.name();
    if(!f.isDirectory() && n.endsWith(".txt")){
      String txt; while(f.available())txt+=(char)f.read(); Recipe r=txtToRecipe(txt); r.storagePath="";
      if(!recipeIsValid(r)){failed++;}
      else {
        bool duplicate=false; for(auto &existing:recipes) if(sameRecipeContent(existing,r)){duplicate=true;break;}
        if(duplicate) skipped++;
        else {recipes.push_back(r); if(saveRecipe(recipes.back()))count++; else {recipes.pop_back();failed++;}}
      }
    }
    f.close(); f=dir.openNextFile();
  }
  dir.close();
  flash(String(ui("IMPORTADOS ","IMPORTED "))+count+ui("  IGNORADOS ","  SKIPPED ")+skipped+(failed?String(ui("  FALHAS ","  FAILED "))+failed:""),failed>0);
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
void loadAppearanceConfig() {
  prefs.begin("misedeck", true);
  accentColor=prefs.getUChar("accent",0);
  portuguese=prefs.getBool("ptbr",false);
  prefs.end();
  if(accentColor>=ACCENT_COLOR_COUNT)accentColor=0;
  applyAccentPalette();
}
void saveAppearanceConfig() {
  if(accentColor>=ACCENT_COLOR_COUNT)accentColor=0;
  prefs.begin("misedeck", false);
  prefs.putUChar("accent",accentColor);
  prefs.putBool("ptbr",portuguese);
  prefs.end();
  applyAccentPalette();
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
  if (!s.length()) s="RECIPE";
  return s + ".txt";
}
String backupTxt() {
  String out;
  for(auto &r:recipes){ out += recipeToTxt(r); out += "\n====================\n"; }
  return out;
}
uint32_t crc32Text(const String& s) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i=0; i<s.length(); i++) {
    crc ^= (uint8_t)s[i];
    for (int b=0; b<8; b++) crc = (crc >> 1) ^ (0xEDB88320UL & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}
void zipWrite16(WiFiClient &c, uint16_t v) { uint8_t b[2]={(uint8_t)(v&255),(uint8_t)(v>>8)}; c.write(b,2); }
void zipWrite32(WiFiClient &c, uint32_t v) { uint8_t b[4]={(uint8_t)(v&255),(uint8_t)((v>>8)&255),(uint8_t)((v>>16)&255),(uint8_t)((v>>24)&255)}; c.write(b,4); }
void sendBackupZip() {
  struct ZipEntry { String name; String txt; uint32_t crc; uint32_t offset; };
  std::vector<ZipEntry> entries;
  for (auto &r:recipes) {
    ZipEntry e; e.name=fileSafeName(r.name); e.txt=recipeToTxt(r); e.crc=crc32Text(e.txt); e.offset=0; entries.push_back(e);
  }
  uint32_t totalSize=0, centralSize=0;
  for (auto &e:entries) { totalSize += 30 + e.name.length() + e.txt.length(); centralSize += 46 + e.name.length(); }
  totalSize += centralSize + 22;
  webServer.setContentLength(totalSize);
  webServer.sendHeader("Content-Disposition", "attachment; filename=\"Mise_Deck_Recipes.zip\"");
  webServer.send(200, "application/zip", "");
  WiFiClient c = webServer.client();
  uint32_t offset=0;
  for (auto &e:entries) {
    e.offset=offset;
    zipWrite32(c,0x04034b50); zipWrite16(c,20); zipWrite16(c,0); zipWrite16(c,0); zipWrite16(c,0); zipWrite16(c,0);
    zipWrite32(c,e.crc); zipWrite32(c,e.txt.length()); zipWrite32(c,e.txt.length()); zipWrite16(c,e.name.length()); zipWrite16(c,0);
    c.write((const uint8_t*)e.name.c_str(), e.name.length()); c.write((const uint8_t*)e.txt.c_str(), e.txt.length());
    offset += 30 + e.name.length() + e.txt.length();
  }
  uint32_t centralStart=offset;
  for (auto &e:entries) {
    zipWrite32(c,0x02014b50); zipWrite16(c,20); zipWrite16(c,20); zipWrite16(c,0); zipWrite16(c,0); zipWrite16(c,0); zipWrite16(c,0);
    zipWrite32(c,e.crc); zipWrite32(c,e.txt.length()); zipWrite32(c,e.txt.length()); zipWrite16(c,e.name.length()); zipWrite16(c,0); zipWrite16(c,0);
    zipWrite16(c,0); zipWrite16(c,0); zipWrite32(c,0); zipWrite32(c,e.offset);
    c.write((const uint8_t*)e.name.c_str(), e.name.length());
    offset += 46 + e.name.length();
  }
  zipWrite32(c,0x06054b50); zipWrite16(c,0); zipWrite16(c,0); zipWrite16(c,entries.size()); zipWrite16(c,entries.size());
  zipWrite32(c,centralSize); zipWrite32(c,centralStart); zipWrite16(c,0);
}
bool validRecipeTxt(String txt) {
  txt.trim();
  if (txt.length() <= 0 || txt.indexOf('|') < 0) return false;
  if (txt.indexOf("[INGREDIENTS]") < 0 && txt.indexOf("[PREP]") < 0 && txt.indexOf("[INGREDIENTES]") < 0 && txt.indexOf("[PREPARO]") < 0) return false;
  Recipe parsed=txtToRecipe(txt);
  return recipeIsValid(parsed);
}
String recipeCard(int idx) {
  Recipe &r=recipes[idx];
  String txt=htmlEscape(recipeToTxt(r));
  String card="<article class='recipe' data-id='"+String(idx)+"'>";
  card += "<div><h3>" + htmlEscape(String(r.favorite?"* ":"") + r.name) + "</h3>";
  card += "<p class='dim'>" + htmlEscape(categoryLabel(r.category)) + " // " + String(r.composite?ui("COMPOSTA","COMPOSITE"):ui("SIMPLES","SIMPLE")) + " // TOTAL " + fw(r.total) + "</p></div>";
  card += "<div class='actions'><button onclick='openRecipe("+String(idx)+")'>"+ui("ABRIR","OPEN")+"</button><a class='btn' href='/txt?id="+String(idx)+"'>"+ui("BAIXAR TXT","DOWNLOAD TXT")+"</a></div>";
  card += "<pre id='txt"+String(idx)+"' class='hidden'>" + txt + "</pre></article>";
  return card;
}
String portalThemeVars() {
  static const char* mainColors[] = {"#FFA600","#00FF00","#00FFFF","#AD00FF","#C5C2C5","#FF5DA5","#F74142","#DE00FF"};
  static const char* brightColors[] = {"#FFFF00","#9CFF3A","#BDFFFF","#E671FF","#FFFFFF","#FFC2DE","#FF9A8C","#FF79FF"};
  static const char* dimColors[] = {"#7B5100","#007D00","#007D7B","#63007B","#7B7D7B","#8C355A","#8C1C19","#730084"};
  static const char* darkColors[] = {"#313100","#003100","#003131","#290031","#292829","#3A1421","#3A0C08","#31003A"};
  uint8_t c=accentColor%ACCENT_COLOR_COUNT;
  return String(":root{--bg:#030303;--panel:#0B0B0B;--amber:")+mainColors[c]+";--dim:"+dimColors[c]+";--line:"+darkColors[c]+";--hot:"+brightColors[c]+";--glow:"+mainColors[c]+"}";
}
String portalCategoryOptions() {
  String out;
  for(int i=0;i<REAL_CAT_COUNT;i++)out+="<option value='"+String(realCats[i])+"'>"+htmlEscape(categoryLabel(realCats[i]))+"</option>";
  return out;
}
String offlineShareHtml() {
  if (offlineShareIndex < 0 || offlineShareIndex >= (int)recipes.size()) {
    return String("<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Mise_Deck Share</title></head><body>")+ui("Receita nao encontrada","Recipe not found")+"</body></html>";
  }
  Recipe &r = recipes[offlineShareIndex];
  String txt = recipeToTxt(r);
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Mise_Deck Share</title><style>";
  html += portalThemeVars();
  html += "*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,var(--panel) 0,var(--bg) 48%,#000 100%);color:var(--amber);font-family:ui-monospace,Consolas,monospace;-webkit-text-size-adjust:100%}";
  html += ".wrap{max-width:820px;margin:auto;padding:18px}header{border-bottom:1px solid var(--line);padding-bottom:14px;margin-bottom:16px}h1{margin:0;color:var(--hot);font-size:clamp(26px,7vw,44px);text-shadow:0 0 10px var(--glow)}h2{margin:8px 0 0;font-size:clamp(20px,5vw,30px)}";
  html += ".dim{color:var(--dim)}.meta{border:1px solid var(--line);background:var(--panel);padding:12px;margin:12px 0;line-height:1.55}.tools{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:14px 0}";
  html += "button,.btn{border:1px solid var(--amber);background:#101010;color:var(--hot);padding:13px;text-decoration:none;font:inherit;text-align:center;cursor:pointer}.btn:hover,button:hover{background:var(--amber);color:#000}";
  html += "pre{white-space:pre-wrap;line-height:1.55;color:var(--hot);font-size:clamp(16px,4.5vw,20px);border:1px solid var(--line);background:var(--panel);padding:14px;overflow:auto}";
  html += ".note{font-size:13px;color:var(--dim);line-height:1.4}@media(max-width:560px){.wrap{padding:14px}.tools{grid-template-columns:1fr}button,.btn{font-size:17px}pre{padding:12px}}";
  html += "</style></head><body><div class='wrap'><header><div class='dim'>Mise_Deck // OFFLINE SHARE</div><h1>" + htmlEscape(r.name) + "</h1>";
  html += "<div class='meta'>"+String(ui("CATEGORIA: ","CATEGORY: ")) + htmlEscape(categoryLabel(r.category)) + "<br>"+ui("TIPO: ","TYPE: ") + String(r.composite ? ui("COMPOSTA","COMPOSITE") : ui("SIMPLES","SIMPLE")) + "<br>TOTAL: " + fw(r.total) + "</div></header>";
  html += "<div class='tools'><button onclick='copyRecipe()'>"+String(ui("COPIAR TEXTO","COPY TEXT"))+"</button><a class='btn' href='/txt'>"+ui("BAIXAR TXT","DOWNLOAD TXT")+"</a></div>";
  html += "<pre id='recipeTxt'>" + htmlEscape(txt) + "</pre><p class='note'>"+String(ui("Esta pagina foi aberta pelo hotspot do Mise_Deck. Nao requer internet. Se a janela fechar, reconecte a ","This page opened from the Mise_Deck hotspot. No internet required. If the captive window closes, reconnect to ")) + htmlEscape(offlineShareSsid) + ui(" e abra http://192.168.4.1"," and open http://192.168.4.1")+"</p></div>";
  html += "<script>const copied='"+String(ui("Receita copiada.","Recipe copied."))+"';function copyRecipe(){let t=document.getElementById('recipeTxt').textContent;if(navigator.clipboard){navigator.clipboard.writeText(t).then(()=>alert(copied)).catch(()=>fallback(t));}else fallback(t);}function fallback(t){let a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();document.execCommand('copy');a.remove();alert(copied);}</script>";
  html += "</body></html>";
  return html;
}
String portalHtml() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Mise_Deck</title><style>";
  html += portalThemeVars();
  html += "*{box-sizing:border-box}html{font-size:16px}body{margin:0;background:radial-gradient(circle at top,var(--panel) 0,var(--bg) 48%,#000 100%);color:var(--amber);font-family:ui-monospace,Consolas,monospace;-webkit-text-size-adjust:100%}";
  html += ".wrap{max-width:1100px;margin:auto;padding:22px}.top{display:flex;gap:12px;justify-content:space-between;align-items:flex-start;border-bottom:1px solid var(--line);padding-bottom:14px}";
  html += "h1{margin:0;letter-spacing:2px;text-shadow:0 0 9px var(--glow);font-size:clamp(24px,5vw,42px);color:var(--amber)}.dim{color:var(--dim)}.slogan{display:block;color:var(--hot);font-size:15px;margin-top:4px}.topright{display:flex;flex-direction:column;gap:8px;align-items:flex-end}.topstatus{display:flex;gap:10px;align-items:stretch}.createbar{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}";
  html += ".status,.tab,.recipe,.modalbox{border:1px solid var(--line);background:var(--panel);box-shadow:0 0 18px var(--line)}";
  html += ".status{padding:10px 12px;text-align:right}.tabs{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0}.tab{color:var(--amber);padding:10px 12px;cursor:pointer;min-height:42px}.tab.active{background:var(--amber);color:#000}";
  html += ".panel{display:none}.panel.active{display:block}.recipe{display:flex;justify-content:space-between;gap:14px;align-items:center;padding:14px;margin:10px 0}.recipe h3{margin:0 0 6px 0;font-size:20px}.actions{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}";
  html += "button,.btn{border:1px solid var(--amber);background:#101010;color:var(--hot);padding:10px 13px;text-decoration:none;font:inherit;cursor:pointer;min-height:42px;border-radius:0}button:hover,.btn:hover{background:var(--amber);color:#000}.create{border-width:2px;background:#141414;color:var(--hot);box-shadow:0 0 14px var(--line)}.syncTop{min-width:86px;background:var(--amber);color:#000;font-weight:bold}.backupFoot{margin:28px 0 8px;padding-top:14px;border-top:1px solid var(--line);text-align:right}";
  html += ".empty{border:1px dashed var(--line);padding:18px;color:var(--dim)}.hidden{display:none}.modal{display:none;position:fixed;inset:0;z-index:20;background:rgba(0,0,0,.78);padding:18px}.modal.on{display:flex}.modalbox{width:min(920px,100%);max-height:92vh;margin:auto;padding:18px;overflow:auto}";
  html += ".modalbar{display:flex;justify-content:space-between;gap:10px;align-items:center;border-bottom:1px solid var(--line);padding-bottom:10px;margin-bottom:12px}#mtitle{font-size:22px;text-shadow:0 0 8px var(--glow)}pre{white-space:pre-wrap;line-height:1.45;color:var(--hot);font-size:17px}";
  html += ".scale{border:1px solid var(--line);padding:12px;margin:12px 0;background:var(--panel)}.scale input{background:var(--bg);border:1px solid var(--amber);color:var(--hot);padding:9px;font:inherit;width:150px}.calc{margin-top:12px}.calc table{width:100%;border-collapse:collapse}.calc td,.calc th{border-bottom:1px solid var(--line);padding:7px;text-align:left}.calc h4{margin:14px 0 6px}";
  html += ".form{border:1px solid var(--line);padding:12px;margin:12px 0;background:var(--panel)}.grid{display:grid;grid-template-columns:2fr 1fr 1fr 1fr;gap:10px}.field label{display:block;color:var(--dim);font-size:12px;margin-bottom:4px}.field input,.field select,.row input{width:100%;background:var(--bg);color:var(--hot);border:1px solid var(--amber);font:inherit;padding:9px;text-transform:uppercase}.row,.prepCard{border:1px solid var(--line);padding:10px;margin:8px 0;background:var(--panel)}.row{display:grid;grid-template-columns:2fr 110px 44px;gap:8px;align-items:center}.prepCard h4{margin:0 0 8px}.danger{border-color:#ff5a36;color:#ff9a7c}.editor{display:none;border:1px solid var(--line);padding:12px;margin:12px 0;background:var(--panel)}.editor.on{display:block}.editor textarea{width:100%;min-height:330px;background:var(--bg);color:var(--hot);border:1px solid var(--amber);font:14px ui-monospace,Consolas,monospace;padding:10px}.tools{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}.backupChoices{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}";
  html += "@media(max-width:650px){html{font-size:18px}.wrap{padding:14px}.top,.recipe,.topright,.topstatus{display:block}.topright{align-items:stretch}.slogan{font-size:14px;line-height:1.35}.status{text-align:left;margin-top:8px;font-size:13px}.syncTop{width:100%;margin-top:8px}.createbar{display:grid;grid-template-columns:1fr;gap:8px;margin-top:8px}.createbar button,.syncTop{font-size:16px;padding:13px}.tabs{position:sticky;top:0;z-index:5;background:var(--bg);margin:12px -14px 14px;padding:10px 14px;overflow-x:auto;flex-wrap:nowrap;-webkit-overflow-scrolling:touch;border-bottom:1px solid var(--line)}.tab{flex:0 0 auto;padding:12px 14px;font-size:15px}.panel h2{font-size:20px;margin:12px 0}.recipe{padding:15px;margin:12px 0}.recipe h3{font-size:22px;line-height:1.2}.recipe .dim{font-size:14px;line-height:1.35}.actions{display:grid;grid-template-columns:1fr;gap:8px;margin-top:12px}.actions button,.actions .btn,.tools button,.tools .btn{width:100%;text-align:center;padding:13px}.modal{padding:0;align-items:stretch}.modalbox{width:100%;height:100vh;max-height:none;margin:0;border:0;padding:14px}.modalbar{position:sticky;top:0;background:var(--panel);z-index:6}.modalbar button{padding:12px 14px}#mtitle{font-size:20px;line-height:1.2}pre{font-size:18px;line-height:1.5;overflow:auto}.scale{padding:12px}.scale p{line-height:1.45}.scale input{width:100%;font-size:18px;margin:6px 0}.scale button{width:100%;margin-top:8px}.calc{overflow-x:auto}.calc table{min-width:440px}.grid,.row,.backupChoices{display:grid;grid-template-columns:1fr}.field input,.field select,.row input{font-size:18px}.editor textarea{min-height:55vh;font-size:16px;line-height:1.45}.backupFoot{text-align:center}.backupFoot button{width:100%}}";
  html += "</style></head><body><div class='wrap'><header class='top'><div><h1 id='brandTitle'>Mise_Deck</h1><div class='slogan'><em id='brandSlogan'>"+String(ui("\"Uma mise en place de bolso para o Cardputer\"","\"A pocket mise en place for the Cardputer\""))+"</em></div></div>";
  html += "<div class='topright'><div class='topstatus'><button class='syncTop' onclick='syncPortal()'>SYNC</button><div class='status'>IP " + htmlEscape(wifiIp()) + "<br>FW " + String(FW_VERSION) + "<br>" + String(recipes.size()) + " "+ui("receitas","recipes")+"</div></div>";
  html += "<div class='createbar'><button class='create' onclick='newRecipe(false)'>+ "+String(ui("NOVA SIMPLES","NEW SIMPLE"))+"</button><button class='create' onclick='newRecipe(true)'>+ "+ui("NOVA COMPOSTA","NEW COMPOSITE")+"</button></div></div></header>";
  html += "<nav class='tabs'>";
  html += "<button class='tab catTab active' onclick='showTab(0)'>"+String(ui("TODAS","ALL"))+"</button>";
  for(int c=0;c<CAT_COUNT;c++) html += "<button class='tab catTab' onclick='showTab("+String(c+1)+")'>" + htmlEscape(categoryLabel(cats[c])) + "</button>";
  html += "</nav>";
  html += "<section id='tab0' class='panel active'><h2>"+String(ui("TODAS","ALL"))+"</h2>";
  if(!recipes.size()) html += "<div class='empty'>"+String(ui("Nenhuma receita ainda.","No recipes yet."))+"</div>";
  else for(int i=0;i<(int)recipes.size();i++) html += recipeCard(i);
  html += "</section>";
  for(int c=0;c<CAT_COUNT;c++) {
    String cat=cats[c];
    html += "<section id='tab"+String(c+1)+"' class='panel'>";
    html += "<h2>" + htmlEscape(categoryLabel(cat)) + "</h2>";
    auto idx=recipeIndicesFor(cat);
    if(!idx.size()) html += "<div class='empty'>"+String(ui("Nenhuma receita nesta categoria.","No recipes in this category."))+"</div>";
    else for(auto i:idx) html += recipeCard(i);
    html += "</section>";
  }
  html += "<div class='backupFoot'><button onclick='openBackup()'>BACKUP</button></div>";
  html += "<div id='backupModal' class='modal'><div class='modalbox'><div class='modalbar'><strong>BACKUP</strong><button onclick='closeBackup()'>"+String(ui("FECHAR","CLOSE"))+"</button></div><div class='backupChoices'><a class='btn' href='/backup'>"+ui("BAIXAR TXT","DOWNLOAD TXT")+"</a><a class='btn' href='/backup.zip'>"+ui("BAIXAR ZIP","DOWNLOAD ZIP")+"</a></div></div></div>";
  html += "</div><div id='modal' class='modal'><div class='modalbox'><div class='modalbar'><strong id='mtitle'>"+String(ui("RECEITA","RECIPE"))+"</strong><button onclick='closeRecipe()'>"+ui("FECHAR","CLOSE")+"</button></div>";
  html += "<div id='guided' class='form'><div class='grid'><div class='field'><label>"+String(ui("NOME","NAME"))+"</label><input id='fname'></div><div class='field'><label>"+ui("CATEGORIA","CATEGORY")+"</label><select id='fcat'>"+portalCategoryOptions()+"</select></div><div class='field'><label>"+ui("TIPO","TYPE")+"</label><select id='ftype' onchange='typeChanged()'><option value='SIMPLE'>"+ui("SIMPLES","SIMPLE")+"</option><option value='COMPOSITE'>"+ui("COMPOSTA","COMPOSITE")+"</option></select></div><div class='field'><label>"+ui("FAVORITA","FAVORITE")+"</label><select id='ffav'><option value='NO'>"+ui("NAO","NO")+"</option><option value='YES'>"+ui("SIM","YES")+"</option></select></div></div><div class='field'><label>TOTAL G</label><input id='ftotal' type='number' step='0.1' min='0'></div><div id='simpleBox'><h3>"+ui("INGREDIENTES","INGREDIENTS")+"</h3><div id='ingRows'></div><button onclick='addIngRow()'>+ "+ui("ADICIONAR INGREDIENTE","ADD INGREDIENT")+"</button></div><div id='prepBox' class='hidden'><h3>"+ui("PREPAROS","PREPS")+"</h3><div id='prepRows'></div><button onclick='addPrepBlock()'>+ "+ui("ADICIONAR PREPARO","ADD PREP")+"</button></div><div class='tools'><button onclick='saveGuided()'>"+ui("SALVAR RECEITA","SAVE RECIPE")+"</button><button class='danger' onclick='deleteCurrent()'>"+ui("APAGAR RECEITA","DELETE RECIPE")+"</button><a id='mdown' class='btn' href='#'>"+ui("BAIXAR TXT","DOWNLOAD TXT")+"</a><button onclick='toggleTxt()'>"+ui("TXT AVANCADO","TXT ADVANCED")+"</button></div></div>";
  html += "<pre id='mtext' class='hidden'></pre>";
  html += "<div class='scale'><div class='dim'>"+String(ui("RECALCULAR PROPORCAO / pode salvar na receita original","RECALCULATE PROPORTION / can save to original recipe"))+"</div><p>"+ui("Total atual: ","Current total: ")+"<strong id='baseTotal'>--</strong> g</p><p>"+ui("Novo total: ","New total: ")+"<input id='newTotal' type='number' step='0.1' min='0'> <button onclick='recalcRecipe()'>"+ui("RECALCULAR","RECALCULATE")+"</button> <button id='saveScale' onclick='saveScale()' disabled>"+ui("SALVAR ORIGINAL","SAVE ORIGINAL")+"</button></p><div id='calc' class='calc'></div></div>";
  html += "<div id='editor' class='editor'><div class='dim'>"+String(ui("EDITOR TXT AVANCADO // ingrediente: NOME|PESO|g","ADVANCED TXT EDITOR // ingredient: NAME|WEIGHT|g"))+"</div><textarea id='editTxt'></textarea><div class='tools'><button onclick='saveEdit()'>"+ui("SALVAR TXT","SAVE TXT")+"</button><button onclick='hideEditor()'>"+ui("CANCELAR","CANCEL")+"</button></div></div></div></div>";
  html += "<script>const UI_PT="+String(portuguese?"true":"false")+";const T=UI_PT?{ingredient:'INGREDIENTE',weight:'PESO',prep:'PREPARO',name:'NOME',total:'TOTAL G',addIng:'+ ADICIONAR INGREDIENTE',removePrep:'REMOVER PREPARO',newTotal:'NOVO TOTAL',recalcFirst:'Recalcule antes de salvar.',saveScale:'Salvar a nova proporcao na receita original?',saveFail:'Falha ao salvar',needIng:'Adicione pelo menos um ingrediente com peso.',saveChanges:'Salvar as alteracoes desta receita?',createRecipe:'Criar esta receita no Cardputer?',saveBeforeDelete:'Salve esta receita antes de apagar.',deleteRecipe:'Apagar esta receita do Cardputer?',deleteFail:'Falha ao apagar',invalidTxt:'TXT invalido: use nome, categoria/total e NOME|PESO|g',saveTxt:'Salvar as alteracoes TXT?',createTxt:'Criar esta receita TXT no Cardputer?'}:{ingredient:'INGREDIENT',weight:'WEIGHT',prep:'PREP',name:'NAME',total:'TOTAL G',addIng:'+ ADD INGREDIENT',removePrep:'REMOVE PREP',newTotal:'NEW TOTAL',recalcFirst:'Recalculate before saving.',saveScale:'Save new proportion to the original recipe?',saveFail:'Save failed',needIng:'Add at least one ingredient with weight.',saveChanges:'Save changes to this recipe?',createRecipe:'Create this recipe on the Cardputer?',saveBeforeDelete:'Save this recipe before deleting.',deleteRecipe:'Delete this recipe from the Cardputer?',deleteFail:'Delete failed',invalidTxt:'Invalid TXT: use name, category/total and ingredients NAME|WEIGHT|g',saveTxt:'Save TXT changes to this recipe?',createTxt:'Create this TXT recipe on the Cardputer?'};function showTab(n){document.querySelectorAll('.catTab').forEach((b,i)=>b.classList.toggle('active',i===n));document.querySelectorAll('.panel').forEach((p,i)=>p.classList.toggle('active',i===n));}";
  html += "function syncPortal(){location.href='/?sync='+Date.now();}";
  html += "function openBackup(){document.getElementById('backupModal').classList.add('on');}function closeBackup(){document.getElementById('backupModal').classList.remove('on');}function capsEl(e){if(e.target&&e.target.tagName==='INPUT'&&e.target.type!=='number'){let p=e.target.selectionStart;e.target.value=e.target.value.toUpperCase();try{e.target.setSelectionRange(p,p);}catch(_){}}}document.addEventListener('input',capsEl);";
  html += "const glyphs=' #$%&/_*+01';function scrambleText(id){let el=document.getElementById(id),base=el.dataset.base||el.textContent;el.dataset.base=base;let a=base.split('');for(let i=0;i<Math.min(3,a.length);i++){let n=Math.floor(Math.random()*a.length);if(a[n]!=' ')a[n]=glyphs[Math.floor(Math.random()*glyphs.length)];}el.textContent=a.join('');setTimeout(()=>el.textContent=base,70);}setInterval(()=>{scrambleText('brandTitle');if(Math.random()>.45)scrambleText('brandSlogan');},2300);let eggBuffer='';function isTypingTarget(el){if(!el)return false;let t=(el.tagName||'').toLowerCase();return t=='input'||t=='textarea'||t=='select'||el.isContentEditable;}document.addEventListener('keydown',e=>{if(isTypingTarget(document.activeElement))return;if(e.key=='Enter'){if(eggBuffer.trim().toLowerCase()=='the cake is real'){fetch('/portal-cake',{method:'POST'}).then(r=>r.text()).then(t=>{alert(t);load();});}eggBuffer='';return;}if(e.key.length==1){eggBuffer=(eggBuffer+e.key).slice(-32);}});";
  html += "let currentTxt='',currentId=-1,scaleReady=false;function esc(s){return String(s).replace(/[&<>\"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[m]));}function val(id){return document.getElementById(id).value.trim();}function num(v){return parseFloat(String(v||'0').replace(',','.'))||0;}";
  html += "function parseRecipe(t){let r={name:'RECIPE',category:'OTHER',favorite:'NO',total:0,type:'SIMPLE',ings:[],preps:[]},p=null,seen=false;for(let raw of t.split(/\\r?\\n/)){let line=raw.trim();if(!line)continue;if(line==='MISEDECK RECIPE'||line==='CYBER CHEF RECIPE'){seen=true;continue;}if(line==='[INGREDIENTS]'||line==='[INGREDIENTES]'){p=null;seen=true;continue;}if(line==='[PREP]'||line==='[PREPARO]'){p={name:'PREP',total:0,ings:[]};r.preps.push(p);seen=true;continue;}if(line.includes('|')){let a=line.split('|');let ing={name:a[0],w:num(a[1])};if(p)p.ings.push(ing);else r.ings.push(ing);seen=true;continue;}let ix=line.indexOf(':');if(ix<0){if(!seen)r.name=line;seen=true;continue;}let k=line.split(':')[0].trim(),v=line.substring(ix+1).trim();if((k==='NAME'||k==='NOME')&&!p)r.name=v;if(k==='CATEGORY'||k==='CATEGORIA')r.category=v;if(k==='FAVORITE'||k==='FAVORITA')r.favorite=(v==='YES'||v==='SIM')?'YES':'NO';if(k==='TYPE'||k==='TIPO')r.type=(v==='COMPOSITE'||v==='COMPOSTA')?'COMPOSITE':'SIMPLE';if(k==='TOTAL'){if(p)p.total=num(v);else r.total=num(v);}if((k==='NAME'||k==='NOME')&&p)p.name=v;seen=true;}if(r.preps.length)r.type='COMPOSITE';return r;}";
  html += "function openRecipe(id){currentId=id;scaleReady=false;currentTxt=document.getElementById('txt'+id).textContent;let r=parseRecipe(currentTxt);loadForm(r);document.getElementById('mdown').href='/txt?id='+id;document.getElementById('modal').classList.add('on');document.querySelector('.modalbox').scrollTop=0;}";
  html += "function catEn(c){return ({'PRATOS':'MAINS','MASSAS':'DOUGHS','PASTA':'DOUGHS','DOCES':'SWEETS','BEBIDAS':'DRINKS','MOLHOS':'SAUCES','OUTRAS':'OTHER','OUTROS':'OTHER'})[c]||c||'OTHER';}function loadForm(r){document.getElementById('fname').value=r.name||'';document.getElementById('fcat').value=catEn(r.category);document.getElementById('ffav').value=r.favorite||'NO';document.getElementById('ftype').value=r.type||'SIMPLE';document.getElementById('ftotal').value=(r.total||'');document.getElementById('mtitle').textContent=r.name||'RECIPE';document.getElementById('mtext').textContent=currentTxt;document.getElementById('baseTotal').textContent=(r.total||0).toFixed(1);document.getElementById('newTotal').value=r.total||'';document.getElementById('calc').innerHTML='';document.getElementById('saveScale').disabled=true;document.getElementById('editor').classList.remove('on');document.getElementById('ingRows').innerHTML='';document.getElementById('prepRows').innerHTML='';if(r.type==='COMPOSITE'){for(let p of r.preps)addPrepBlock(p.name,p.total,p.ings);}else{for(let i of r.ings)addIngRow(i.name,i.w);if(!r.ings.length)addIngRow();}typeChanged();}";
  html += "function typeChanged(){let comp=document.getElementById('ftype').value==='COMPOSITE';document.getElementById('simpleBox').classList.toggle('hidden',comp);document.getElementById('prepBox').classList.toggle('hidden',!comp);}";
  html += "function addIngRow(n='',w=''){document.getElementById('ingRows').insertAdjacentHTML('beforeend',`<div class='row'><input placeholder='${T.ingredient}' value='${esc(n)}'><input type='number' step='0.1' min='0' placeholder='g' value='${w||''}'><button onclick='this.parentNode.remove()'>X</button></div>`);}";
  html += "function addPrepBlock(n='',t='',ings){n=n||(UI_PT?'PREPARO':'PREP');let id='p'+Date.now()+Math.floor(Math.random()*999);document.getElementById('prepRows').insertAdjacentHTML('beforeend',`<div class='prepCard'><h4>${T.prep}</h4><div class='grid'><div class='field'><label>${T.name}</label><input class='pname' value='${esc(n)}'></div><div class='field'><label>${T.total}</label><input class='ptotal' type='number' step='0.1' min='0' value='${t||''}'></div></div><div id='${id}' class='pings'></div><div class='tools'><button onclick='addPrepIng(\"${id}\")'>${T.addIng}</button><button class='danger' onclick='this.closest(\".prepCard\").remove()'>${T.removePrep}</button></div></div>`);if(ings&&ings.length){for(let i of ings)addPrepIng(id,i.name,i.w);}else addPrepIng(id);}";
  html += "function addPrepIng(id,n='',w=''){document.getElementById(id).insertAdjacentHTML('beforeend',`<div class='row'><input placeholder='${T.ingredient}' value='${esc(n)}'><input type='number' step='0.1' min='0' placeholder='g' value='${w||''}'><button onclick='this.parentNode.remove()'>X</button></div>`);}";
  html += "function row(n,w,p){return '<tr><td>'+esc(n)+'</td><td>'+w.toFixed(1)+' g</td><td>'+p+'%</td></tr>'}";
  html += "function recalcRecipe(){let r=parseRecipe(currentTxt),nt=parseFloat(document.getElementById('newTotal').value)||0,out='';scaleReady=false;document.getElementById('saveScale').disabled=true;if(!nt||!r.total){document.getElementById('calc').innerHTML='<p class=dim>'+(UI_PT?'Informe um total valido.':'Enter a valid total.')+'</p>';return;}let f=nt/r.total;if(r.preps.length){out+='<h4>'+T.newTotal+' '+nt.toFixed(1)+' g</h4>';for(let p of r.preps){let pt=(p.total||p.ings.reduce((s,i)=>s+i.w,0))*f;let base=p.total||p.ings.reduce((s,i)=>s+i.w,0)||1;out+='<h4>'+esc(p.name)+' // '+pt.toFixed(1)+' g</h4><table><tr><th>'+T.ingredient+'</th><th>'+T.weight+'</th><th>%</th></tr>';for(let i of p.ings)out+=row(i.name,i.w*f,Math.round(i.w/base*100));out+='</table>';}}else{let base=r.ings.reduce((s,i)=>s+i.w,0)||r.total||1;out+='<h4>'+T.newTotal+' '+nt.toFixed(1)+' g</h4><table><tr><th>'+T.ingredient+'</th><th>'+T.weight+'</th><th>%</th></tr>';for(let i of r.ings)out+=row(i.name,i.w*nt/base,Math.round(i.w/base*100));out+='</table>';}document.getElementById('calc').innerHTML=out;scaleReady=true;document.getElementById('saveScale').disabled=false;}";
  html += "function saveScale(){let nt=parseFloat(document.getElementById('newTotal').value)||0;if(!scaleReady||currentId<0||!nt)return alert(T.recalcFirst);if(!confirm(T.saveScale))return;fetch('/scale?id='+currentId+'&total='+encodeURIComponent(nt),{method:'POST'}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert(T.saveFail));}";
  html += "function readRows(root){return [...root.querySelectorAll('.row')].map(r=>({name:r.children[0].value.trim(),w:num(r.children[1].value)})).filter(i=>i.name&&i.w>0);}function rowsTxt(rows){return rows.map(i=>i.name.toUpperCase()+'|'+i.w.toFixed(1)+'|g').join('\\n')+'\\n';}";
  html += "function buildTxt(){let name=val('fname')||(UI_PT?'NOVA RECEITA':'NEW RECIPE'),cat=val('fcat')||'OTHER',fav=val('ffav')||'NO',type=val('ftype'),total=0,catPt=({MAINS:'PRATOS',DOUGHS:'MASSAS',SWEETS:'DOCES',DRINKS:'BEBIDAS',SAUCES:'MOLHOS',OTHER:'OUTRAS'})[cat]||'OUTRAS',head=()=>name.toUpperCase()+'\\n'+(UI_PT?'CATEGORIA: '+catPt+'\\nFAVORITA: '+(fav==='YES'?'SIM':'NAO'):'CATEGORY: '+cat+'\\nFAVORITE: '+fav)+'\\nTOTAL: '+total.toFixed(1)+'\\n',body='';if(type==='COMPOSITE'){body+=(UI_PT?'TIPO: COMPOSTA':'TYPE: COMPOSITE')+'\\n\\n';let preps=[...document.querySelectorAll('.prepCard')];for(let p of preps){let pn=p.querySelector('.pname').value.trim()||(UI_PT?'PREPARO':'PREP');let rows=readRows(p.querySelector('.pings'));let pt=rows.reduce((s,i)=>s+i.w,0);total+=pt;body+=(UI_PT?'[PREPARO]\\nNOME: ':'[PREP]\\nNAME: ')+pn.toUpperCase()+'\\nTOTAL: '+pt.toFixed(1)+'\\n'+rowsTxt(rows)+'\\n';}}else{let rows=readRows(document.getElementById('ingRows'));total=rows.reduce((s,i)=>s+i.w,0);body='\\n'+(UI_PT?'[INGREDIENTES]':'[INGREDIENTS]')+'\\n\\n'+rowsTxt(rows);}document.getElementById('ftotal').value=total.toFixed(1);return head()+body;}";
  html += "function saveGuided(){let txt=buildTxt();if(!validTxt(txt))return alert(T.needIng);let url=currentId>=0?'/save?id='+currentId:'/new';if(!confirm(currentId>=0?T.saveChanges:T.createRecipe))return;fetch(url,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:txt}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert(T.saveFail));}";
  html += "function deleteCurrent(){if(currentId<0)return alert(T.saveBeforeDelete);if(!confirm(T.deleteRecipe))return;fetch('/delete?id='+currentId,{method:'POST'}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert(T.deleteFail));}";
  html += "function toggleTxt(){document.getElementById('mtext').classList.toggle('hidden');editCurrent();}";
  html += "function editCurrent(){currentTxt=buildTxt();document.getElementById('mtext').textContent=currentTxt;document.getElementById('editTxt').value=currentTxt;document.getElementById('editor').classList.add('on');}";
  html += "function hideEditor(){document.getElementById('editor').classList.remove('on');}";
  html += "function validTxt(txt){return txt.trim().length>0&&txt.includes('|')&&(txt.includes('[INGREDIENTS]')||txt.includes('[PREP]')||txt.includes('[INGREDIENTES]')||txt.includes('[PREPARO]'));}";
  html += "function saveEdit(){let txt=document.getElementById('editTxt').value;if(!validTxt(txt))return alert(T.invalidTxt);let url=currentId>=0?'/save?id='+currentId:'/new';if(!confirm(currentId>=0?T.saveTxt:T.createTxt))return;fetch(url,{method:'POST',headers:{'Content-Type':'text/plain; charset=utf-8'},body:txt}).then(r=>r.text()).then(t=>{alert(t);syncPortal();}).catch(()=>alert(T.saveFail));}";
  html += "function newRecipe(comp){currentId=-1;scaleReady=false;let r={name:comp?(UI_PT?'NOVA COMPOSTA':'NEW COMPOSITE'):(UI_PT?'NOVA RECEITA':'NEW RECIPE'),category:'OTHER',favorite:'NO',total:comp?300:100,type:comp?'COMPOSITE':'SIMPLE',ings:comp?[]:[{name:UI_PT?'INGREDIENTE 1':'INGREDIENT 1',w:100}],preps:comp?[{name:UI_PT?'PREPARO 1':'PREP 1',total:100,ings:[{name:UI_PT?'INGREDIENTE 1':'INGREDIENT 1',w:100}]},{name:UI_PT?'PREPARO 2':'PREP 2',total:200,ings:[{name:UI_PT?'INGREDIENTE 2':'INGREDIENT 2',w:200}]}]:[]};currentTxt='';loadForm(r);document.getElementById('mdown').href='#';document.getElementById('modal').classList.add('on');}";
  html += "function closeRecipe(){document.getElementById('editor').classList.remove('on');document.getElementById('modal').classList.remove('on');}</script></body></html>";
  return html;
}

bool isRecipeScreen(State s) {
  return s==ST_RECIPE_VIEW || s==ST_SHARE_QR || s==ST_PREP_VIEW || s==ST_ACTIONS || s==ST_PREP_ACTIONS || s==ST_PREP_MENU || s==ST_ING_MENU || s==ST_ING_LIST || s==ST_TEXT_INPUT || s==ST_NUM_INPUT || s==ST_CONFIRM || s==ST_ADD_ING_NAME || s==ST_ADD_ING_WEIGHT;
}

void settleAfterPortalEdit(int id) {
  if(recipeIndex!=id)return;
  prepIndex=-1; ingIndex=-1; addingExistingPrep=false; existingPrepBackupValid=false;
  if(isRecipeScreen(state)){state=ST_RECIPE_VIEW;resetNav();}
}

void settleAfterPortalDelete(int id) {
  if(offlineShareIndex==id)stopOfflineShare(true);
  else if(offlineShareIndex>id)offlineShareIndex--;
  if(recipeIndex==id){
    recipeIndex=-1; prepIndex=-1; ingIndex=-1; addingExistingPrep=false; existingPrepBackupValid=false;
    if(isRecipeScreen(state)){state=ST_RECIPES;resetNav();}
  } else if(recipeIndex>id) recipeIndex--;
}

void startPortalServer() {
  if (!webStarted) {
    auto sendOffline = [](){
      webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
      webServer.sendHeader("Pragma", "no-cache");
      webServer.send(200, "text/html; charset=utf-8", offlineShareHtml());
    };
    webServer.on("/", [](){ webServer.send(200, "text/html; charset=utf-8", offlineShareActive ? offlineShareHtml() : portalHtml()); });
    webServer.on("/generate_204", HTTP_GET, sendOffline);                  // Android
    webServer.on("/gen_204", HTTP_GET, sendOffline);                       // Android / Chrome
    webServer.on("/hotspot-detect.html", HTTP_GET, sendOffline);           // iOS / macOS
    webServer.on("/library/test/success.html", HTTP_GET, sendOffline);     // iOS newer probes
    webServer.on("/ncsi.txt", HTTP_GET, sendOffline);                      // Windows
    webServer.on("/connecttest.txt", HTTP_GET, sendOffline);               // Windows
    webServer.on("/redirect", HTTP_GET, sendOffline);                      // Windows
    webServer.on("/canonical.html", HTTP_GET, sendOffline);                // Firefox / Linux
    webServer.on("/success.txt", HTTP_GET, sendOffline);                   // Fire OS / generic
    webServer.on("/txt", [](){
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : (offlineShareActive ? offlineShareIndex : -1);
      if(id<0 || id>=(int)recipes.size()) { webServer.send(404, "text/plain; charset=utf-8", ui("Receita nao encontrada","Recipe not found")); return; }
      webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + fileSafeName(recipes[id].name) + "\"");
      webServer.send(200, "text/plain; charset=utf-8", recipeToTxt(recipes[id]));
    });
    webServer.on("/backup", [](){
      webServer.sendHeader("Content-Disposition", "attachment; filename=\"MISEDECK_BACKUP.txt\"");
      webServer.send(200, "text/plain; charset=utf-8", backupTxt());
    });
    webServer.on("/backup.zip", [](){
      sendBackupZip();
    });
    webServer.on("/portal-cake", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", ui("FALHA NO SD","SD FAIL")); return; }
      if (hasRecipeId("portal-cake")) { webServer.send(200, "text/plain; charset=utf-8", "THE CAKE IS REAL"); return; }
      Recipe r = portalCakeRecipe();
      recipes.push_back(r);
      if(saveRecipe(recipes.back())) { beepRecipe(); webServer.send(200, "text/plain; charset=utf-8", "THE CAKE IS REAL"); }
      else { recipes.pop_back(); webServer.send(500, "text/plain; charset=utf-8", ui("Falha ao desbloquear receita","Failed to unlock recipe")); }
    });
    webServer.on("/scale", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", ui("FALHA NO SD","SD FAIL")); return; }
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      float total=webServer.hasArg("total") ? webServer.arg("total").toFloat() : 0;
      if(id<0 || id>=(int)recipes.size() || total<=0){ webServer.send(400, "text/plain; charset=utf-8", ui("Dados invalidos","Invalid data")); return; }
      Recipe previous=recipes[id];
      scaleRecipe(recipes[id], total);
      if(saveRecipe(recipes[id])) {settleAfterPortalEdit(id);webServer.send(200, "text/plain; charset=utf-8", ui("Receita salva. Use SYNC para atualizar.","Recipe saved. Use SYNC to refresh."));}
      else {recipes[id]=previous;webServer.send(500, "text/plain; charset=utf-8", ui("Falha ao salvar receita","Failed to save recipe"));}
    });
    webServer.on("/save", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", ui("FALHA NO SD","SD FAIL")); return; }
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      if(id<0 || id>=(int)recipes.size()){ webServer.send(400, "text/plain; charset=utf-8", ui("Receita invalida","Invalid recipe")); return; }
      String txt=webServer.arg("plain");
      if(!validRecipeTxt(txt)){ webServer.send(400, "text/plain; charset=utf-8", ui("TXT invalido","Invalid TXT")); return; }
      Recipe previous=recipes[id];
      String oldPath=recipes[id].storagePath;
      Recipe r=txtToRecipe(txt);
      r.id=recipes[id].id;
      r.storagePath=oldPath;
      recipes[id]=r;
      if(saveRecipe(recipes[id])) {settleAfterPortalEdit(id);webServer.send(200, "text/plain; charset=utf-8", ui("Receita atualizada. Use SYNC.","Recipe updated. Use SYNC."));}
      else {recipes[id]=previous;webServer.send(500, "text/plain; charset=utf-8", ui("Falha ao salvar receita","Failed to save recipe"));}
    });
    webServer.on("/delete", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", ui("FALHA NO SD","SD FAIL")); return; }
      int id=webServer.hasArg("id") ? webServer.arg("id").toInt() : -1;
      if(id<0 || id>=(int)recipes.size()){ webServer.send(400, "text/plain; charset=utf-8", ui("Receita invalida","Invalid recipe")); return; }
      String path=recipes[id].storagePath.length()?recipeDirPath(recipes[id].storagePath):"";
      if(!path.length() || (SD.exists(path) && !SD.remove(path))){webServer.send(500,"text/plain; charset=utf-8",ui("Falha ao apagar receita","Failed to delete recipe"));return;}
      SD.remove(path+".tmp");
      SD.remove(path+".bak");
      recipes.erase(recipes.begin()+id);
      settleAfterPortalDelete(id);
      webServer.send(200, "text/plain; charset=utf-8", ui("Receita apagada. Use SYNC.","Recipe deleted. Use SYNC."));
    });
    webServer.on("/new", HTTP_POST, [](){
      if(!sdOK){ webServer.send(500, "text/plain; charset=utf-8", ui("FALHA NO SD","SD FAIL")); return; }
      String txt=webServer.arg("plain");
      if(!validRecipeTxt(txt)){ webServer.send(400, "text/plain; charset=utf-8", ui("TXT invalido","Invalid TXT")); return; }
      Recipe r=txtToRecipe(txt);
      r.id=nowId();
      r.storagePath="";
      recipes.push_back(r);
      if(saveRecipe(recipes.back())) webServer.send(200, "text/plain; charset=utf-8", ui("Nova receita criada. Use SYNC.","New recipe created. Use SYNC."));
      else { recipes.pop_back(); webServer.send(500, "text/plain; charset=utf-8", ui("Falha ao criar receita","Failed to create recipe")); }
    });
    webServer.onNotFound([](){
      if (offlineShareActive) {
        webServer.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        webServer.send(200, "text/html; charset=utf-8", offlineShareHtml());
      } else {
        webServer.sendHeader("Location", "/", true);
        webServer.send(302, "text/plain", "");
      }
    });
    webServer.begin();
    webStarted = true;
  }
  if (!offlineShareActive && !mdnsStarted) {
    mdnsStarted = MDNS.begin("misedeck");
  }
}
void stopPortalServer() {
  if (webStarted) { webServer.stop(); webStarted=false; }
  if (mdnsStarted) { MDNS.end(); mdnsStarted=false; }
}
void startCaptiveDns() {
  if (dnsStarted) dnsServer.stop();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsStarted = dnsServer.start(DNS_PORT, "*", OFFLINE_SHARE_IP);
}
void stopCaptiveDns() {
  if (dnsStarted) {
    dnsServer.stop();
    dnsStarted = false;
  }
}
void startOfflineShare() {
  if (recipeIndex < 0 || recipeIndex >= (int)recipes.size()) return;
  stopPortalServer();
  stopCaptiveDns();
  offlineShareActive = true;
  offlineShareIndex = recipeIndex;
  uint32_t chip = (uint32_t)ESP.getEfuseMac();
  offlineShareSsid = "Mise_Deck_" + String(chip & 0xFFFFFF, HEX);
  offlineShareSsid.toUpperCase();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(OFFLINE_SHARE_IP, OFFLINE_SHARE_GW, OFFLINE_SHARE_MASK);
  bool ok = WiFi.softAP(offlineShareSsid.c_str());
  if (!ok) {
    offlineShareActive = false;
    offlineShareIndex = -1;
    offlineShareSsid = "";
    WiFi.mode(WIFI_OFF);
    return;
  }
  startCaptiveDns();
  startPortalServer();
}
void stopOfflineShare(bool wifiOff) {
  if (!offlineShareActive) return;
  stopCaptiveDns();
  stopPortalServer();
  WiFi.softAPdisconnect(true);
  if (wifiOff) WiFi.mode(WIFI_OFF);
  offlineShareActive = false;
  offlineShareIndex = -1;
  offlineShareSsid = "";
}
void disconnectWifi() {
  stopOfflineShare(false);
  stopPortalServer();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}
void connectWifiNow() {
  if (!wifiSsid.length()) { flash(ui("INFORME A REDE","ENTER NETWORK"), true); go(ST_WIFI_SSID); return; }
  saveWifiConfig();
  uiPanel("WIFI", String(ui("Conectando a:\n","Connecting to:\n")) + wifiSsid + ui("\n\nAguarde...","\n\nPlease wait..."), ui("` CANCELA","` CANCEL"), 1);
  uiCanvas.pushSprite(0,0);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  unsigned long start=millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-start<16000) {
    if (pollBackKey()) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      beepBack();
      flash(ui("CONEXAO CANCELADA","CONNECTION CANCELED"), true);
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
    flash(ui("FALHA NO WIFI","WIFI FAILED"), true);
    go(ST_WIFI);
  }
}
void editWifiInput(const KeyEvent& e, bool password) {
  if (e.back) { beepBack(); go(ST_WIFI); return; }
  bool changed=false;
  if (e.left && inputCursor>0) {inputCursor--;changed=true;beepNav();}
  if (e.right && inputCursor<(int)inputBuf.length()) {inputCursor++;changed=true;beepNav();}
  if (e.del && inputBuf.length() && inputCursor>0) { inputBuf.remove(inputCursor-1,1); inputCursor--; changed=true; }
  if (e.ch && e.ch>=32 && e.ch<=126) {
    if(inputBuf.length()<48){
      inputBuf=inputBuf.substring(0,inputCursor)+String(e.ch)+inputBuf.substring(inputCursor);
      inputCursor++;
      changed=true;
    }
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
    case ST_MAIN: nav(e,4, [](){ if(gi()==0)go(ST_RECIPES); else if(gi()==1)go(ST_TOOLS); else if(gi()==2)go(ST_WIFI); else go(ST_SYSTEM); }, ST_MAIN); break;
    case ST_RECIPES: nav(e,4, [](){ if(gi()==0){currentCategory="FAVORITES";go(ST_RECIPE_LIST);} else if(gi()==1)go(ST_CATEGORIES); else if(gi()==2)go(ST_NEW_TYPE); else {quickWeights.clear(); setInput(""); go(ST_QUICK);} }, ST_MAIN); break;
    case ST_CATEGORIES: nav(e,CAT_COUNT, [](){ currentCategory=cats[gi()]; go(ST_RECIPE_LIST); }, ST_RECIPES); break;
    case ST_RECIPE_LIST: {
      auto idx=recipeIndicesFor(currentCategory); nav(e,idx.size(), [](){ auto idx=recipeIndicesFor(currentCategory); if(gi()<(int)idx.size()){ recipeIndex=idx[gi()]; prepIndex=-1; go(ST_RECIPE_VIEW);} }, ST_CATEGORIES); break;
    }
    case ST_RECIPE_VIEW: {
      Recipe &r=recipes[recipeIndex]; int total=r.composite?r.preps.size():r.ingredients.size();
      if(e.back)go(ST_RECIPE_LIST); else if(e.tab){Recipe previous=r;r.favorite=!r.favorite;if(persistRecipeUpdate(recipeIndex,previous))flash(r.favorite?ui("FAVORITA","FAVORITE"):ui("NAO FAVORITA","UNFAVORITED"));}
      else if(e.up||e.left) focusMove(-1,total);
      else if(e.down||e.right) focusMove(1,total);
      else if(e.enter){ if(r.composite){ prepIndex=gi(); if(prepIndex<(int)r.preps.size())go(ST_PREP_VIEW);} else go(ST_ACTIONS); }
      else if(e.ch==' ') go(ST_ACTIONS);
      break;
    }
    case ST_PREP_VIEW: {
      Recipe &r=recipes[recipeIndex]; Prep &p=r.preps[prepIndex];
      if(e.back)go(ST_RECIPE_VIEW);
      else if(e.up||e.left) focusMove(-1,p.ingredients.size());
      else if(e.down||e.right) focusMove(1,p.ingredients.size());
      else if(e.enter||e.ch==' ')go(ST_PREP_ACTIONS);
      break;
    }
    case ST_ACTIONS: {
      Recipe &r=recipes[recipeIndex]; int total=6;
      if(!r.composite) total=7;
      nav(e,total, [](){ Recipe &r=recipes[recipeIndex]; int c=gi(); if(c==0)askNumber(ui("ALTERAR TOTAL","CHANGE TOTAL"),ui("NOVO TOTAL","NEW TOTAL"),String(ui("ATUAL: ","CURRENT: "))+fw(r.total),r.total,cbScaleRecipe); else if(!r.composite && c==1){ pending.mode=4; pending.title=ui("ESCALAR ING.","SCALE BY ING."); go(ST_ING_LIST); } else if((!r.composite && c==2) || (r.composite && c==1)){ if(r.composite)go(ST_PREP_MENU); else go(ST_ING_MENU); } else if((!r.composite && c==3) || (r.composite && c==2)){ Recipe cp=r; cp.id=nowId(); cp.name=cleanName(cp.name+ui(" COPIA"," COPY")); cp.storagePath=""; cp.favorite=false; recipes.push_back(cp); int newIndex=recipes.size()-1; if(saveRecipe(recipes[newIndex])){recipeIndex=newIndex;flash(ui("DUPLICADA","DUPLICATED"));go(ST_RECIPE_VIEW);}else{recipes.pop_back();flash(ui("FALHA AO SALVAR SD","SD SAVE FAILED"),true);} } else if((!r.composite && c==4) || (r.composite && c==3))go(ST_TIMER_MENU); else if((!r.composite && c==5) || (r.composite && c==4))go(ST_SHARE_QR); else askConfirm(ui("APAGAR RECEITA?","DELETE RECIPE?"),r.name,doDeleteRecipe); }, ST_RECIPE_VIEW); break;
    }
    case ST_SHARE_QR: if(e.back||e.enter){ stopOfflineShare(true); go(ST_ACTIONS); } break;
    case ST_PREP_ACTIONS: nav(e,5, [](){ Recipe&r=recipes[recipeIndex]; Prep&p=r.preps[prepIndex]; int c=gi(); if(c==0)askNumber(ui("ALTERAR TOTAL","CHANGE TOTAL"),ui("NOVO TOTAL","NEW TOTAL"),String(ui("ATUAL: ","CURRENT: "))+fw(p.total),p.total,cbScalePrep); else if(c==1){ pending.mode=4; pending.title=ui("ESCALAR ING.","SCALE BY ING."); go(ST_ING_LIST); } else if(c==2)go(ST_ING_MENU); else if(c==3){ Recipe previous=r; Prep cp=p; cp.name=cleanName(cp.name+ui(" COPIA"," COPY")); r.preps.push_back(cp); recalc(r); if(persistRecipeUpdate(recipeIndex,previous)){flash(ui("PREPARO DUPLICADO","PREP DUPLICATED"));go(ST_RECIPE_VIEW);} } else askConfirm(ui("REMOVER PREPARO?","REMOVE PREP?"),p.name,doRemovePrep); }, ST_PREP_VIEW); break;
    case ST_PREP_MENU: {
      Recipe&r=recipes[recipeIndex];
      nav(e,r.preps.size()+1, [](){ int c=gi(); if(c==0)askText(ui("NOVO PREPARO","NEW PREP"),ui("NOME","NAME"),"",cbAddExistingPrepName); else {prepIndex=c-1;go(ST_PREP_VIEW);} }, ST_ACTIONS);
      break;
    }
    case ST_ING_MENU: nav(e,4, [](){ int c=gi(); if(c==0){setInput("");go(ST_ADD_ING_NAME);} else { pending.mode=c; pending.title=(c==1?ui("ALTERAR PESO","CHANGE WEIGHT"):c==2?ui("REMOVER ING.","REMOVE ING."):ui("RENOMEAR ING.","RENAME ING.")); go(ST_ING_LIST);} }, prepIndex>=0?ST_PREP_ACTIONS:ST_ACTIONS); break;
    case ST_ING_LIST: {
      Recipe&r=recipes[recipeIndex]; std::vector<Ingredient>* vec=(r.composite&&prepIndex>=0)?&r.preps[prepIndex].ingredients:&r.ingredients;
      nav(e,vec->size(), [](){ Recipe&r=recipes[recipeIndex]; std::vector<Ingredient>* vec=(r.composite&&prepIndex>=0)?&r.preps[prepIndex].ingredients:&r.ingredients; ingIndex=gi(); if(pending.mode==4)askNumber(ui("ESCALAR ING.","SCALE BY ING."),ui("NOVO PESO","NEW WEIGHT"),String(ui("ATUAL: ","CURRENT: "))+fw((*vec)[ingIndex].weight),(*vec)[ingIndex].weight,cbScaleByIngredient); else if(pending.mode==1)askNumber(ui("ALTERAR PESO","CHANGE WEIGHT"),ui("NOVO PESO","NEW WEIGHT"),String(ui("ATUAL: ","CURRENT: "))+fw((*vec)[ingIndex].weight),(*vec)[ingIndex].weight,cbSetIngWeight); else if(pending.mode==2)askConfirm(ui("REMOVER ING.?","REMOVE ING.?"),(*vec)[ingIndex].name,doRemoveIng); else askText(ui("RENOMEAR ING.","RENAME ING."),ui("NOME","NAME"),(*vec)[ingIndex].name,cbRenameIng); }, pending.mode==4?(prepIndex>=0?ST_PREP_ACTIONS:ST_ACTIONS):ST_ING_MENU); break;
    }
    case ST_TEXT_INPUT: editInput(e,false); break;
    case ST_NUM_INPUT: editInput(e,true); break;
    case ST_CONFIRM:
      if(e.back){ beepBack(); go(confirmReturn); }
      else if(e.left||e.up){ selected=0; beepNav(); render(); }
      else if(e.right||e.down){ selected=1; beepNav(); render(); }
      else if(e.enter){ if(gi()==1 && confirmCallback) confirmCallback(); else go(confirmReturn); }
      break;
    case ST_NEW_TYPE:
      if(e.back){discardDraft();beepBack();go(ST_RECIPES);}
      else nav(e,2, [](){ discardDraft(); hasDraft=true; draft=Recipe(); draft.id=nowId(); draft.composite=(gi()==1); if(draft.composite) draft.preps.clear(); else draft.ingredients.clear(); askText(ui("NOME DA RECEITA","RECIPE NAME"),ui("NOME","NAME"),"",cbNewRecipeName); }, ST_RECIPES);
      break;
    case ST_NEW_CAT: nav(e,REAL_CAT_COUNT, [](){ draft.category=realCats[gi()]; if(draft.composite) askText(ui("NOVO PREPARO","NEW PREP"),ui("NOME","NAME"),"",cbNewPrepName); else { setInput(""); go(ST_ADD_ING_NAME); } }, ST_NEW_TYPE); break;
    case ST_NEW_PREP_NEXT: nav(e,2, [](){ if(gi()==0)askText(ui("NOVO PREPARO","NEW PREP"),ui("NOME","NAME"),"",cbNewPrepName); else finalizeDraft(); }, ST_ADD_ING_NAME); break;
    case ST_ADD_ING_NAME:
      if(e.back){
        if(addingExistingPrep){if(existingPrepBackupValid&&recipeIndex>=0&&recipeIndex<(int)recipes.size())recipes[recipeIndex]=existingPrepBackup;addingExistingPrep=false;existingPrepBackupValid=false;prepIndex=-1;go(ST_PREP_MENU);}
        else if(hasDraft&&draft.composite&&prepIndex>=0&&prepIndex<(int)draft.preps.size()&&draft.preps[prepIndex].ingredients.size())go(ST_NEW_PREP_NEXT);
        else if(hasDraft)go(ST_NEW_CAT); else go(ST_ING_MENU);
      } else if(e.enter){
        if(inputBuf.length()==0){
          if(hasDraft&&draft.composite){if(prepIndex>=0&&prepIndex<(int)draft.preps.size()&&draft.preps[prepIndex].ingredients.size())go(ST_NEW_PREP_NEXT);else flash(ui("ADICIONE ING.","ADD ING."),true);}
          else if(addingExistingPrep)finishExistingPrep();
          else if(hasDraft)finalizeDraft(); else go(ST_ING_MENU);
        } else { pending.text=cleanName(inputBuf); setInput(""); go(ST_ADD_ING_WEIGHT); }
      } else editInput(e,false);
      break;
    case ST_ADD_ING_WEIGHT:
      if(e.back)go(ST_ADD_ING_NAME);
      else if(e.enter){inputBuf.replace(',','.');float v=inputBuf.toFloat();if(v>0)cbAddIngWeight(v);else flash(ui("PESO INVALIDO","INVALID WEIGHT"),true);}
      else editInput(e,true);
      break;
    case ST_QUICK: if(e.back)go(ST_RECIPES); else if(e.enter){ if(inputBuf.length()==0){ if(quickWeights.size()){ quickOriginal=0; for(auto w:quickWeights)quickOriginal+=w; setInput(""); go(ST_QUICK_TARGET);} else flash(ui("SEM PESOS","NO WEIGHTS"),true); } else { float v=r1(inputBuf.toFloat());if(v>0){quickWeights.push_back(v);setInput("");}else flash(ui("PESO INVALIDO","INVALID WEIGHT"),true); } } else editInput(e,true); break;
    case ST_QUICK_TARGET: if(e.back)go(ST_QUICK); else if(e.enter){ quickTarget=inputBuf.toFloat(); if(quickTarget>0)go(ST_QUICK_RESULT); else flash(ui("TOTAL INVALIDO","INVALID TOTAL"),true); } else editInput(e,true); break;
    case ST_QUICK_RESULT: if(e.back)go(ST_RECIPES); else nav(e,quickWeights.size(),nullptr,ST_RECIPES); break;
    case ST_TOOLS: nav(e,2, [](){ if(gi()==0)go(ST_TIMER_MENU); else go(ST_CONV); }, ST_MAIN); break;
    case ST_TIMER_MENU: nav(e,6, [](){ int c=gi(); if(c==0){ setInput(""); go(ST_TIMER_CUSTOM); } else { int mins[5]={1,3,5,10,15}; timerDeadline=millis()+mins[c-1]*60000UL; timerRunning=true; timerPaused=false; go(ST_TIMER_RUN); } }, ST_TOOLS); break;
    case ST_TIMER_CUSTOM: if(e.back)go(ST_TIMER_MENU); else if(e.enter){ int m=inputBuf.toInt(); if(m>0){timerDeadline=millis()+m*60000UL;timerRunning=true;timerPaused=false;go(ST_TIMER_RUN);} } else editInput(e,true); break;
    case ST_TIMER_RUN: if(e.back){timerRunning=false;go(ST_TIMER_MENU);} else if(e.enter){ if(timerPaused){ timerDeadline=millis()+timerLeft; timerPaused=false; } else { timerLeft=timerDeadline-millis(); timerPaused=true; } } break;
    case ST_TIMER_DONE: if(e.back||e.enter){ stopTimerAlarm(); beepBack(); go(ST_TIMER_MENU); } break;
    case ST_CONV: nav(e,2, [](){ if(gi()==0)go(ST_TEMP_DIR); else go(ST_WEIGHT_FROM); }, ST_TOOLS); break;
    case ST_TEMP_DIR: nav(e,2, [](){ if(gi()==0)askNumber("C TO F","CELSIUS","",0,cbTempC); else askNumber("F TO C","FAHRENHEIT","",0,cbTempF); }, ST_CONV); break;
    case ST_WEIGHT_FROM: nav(e,4, [](){ const char* u[]={"g","kg","lb","oz"}; weightFrom=u[gi()]; pending.text=weightFrom; go(ST_WEIGHT_TO); }, ST_CONV); break;
    case ST_WEIGHT_TO: nav(e,4, [](){ const char* u[]={"g","kg","lb","oz"}; weightTo=u[gi()]; askNumber(weightFrom+ui(" PARA "," TO ")+weightTo,ui("VALOR","VALUE"),"",0,cbWeight); }, ST_WEIGHT_FROM); break;
    case ST_RESULT: if(e.back||e.enter)go(ST_CONV); break;
    case ST_WIFI: nav(e,4, [](){ int c=gi(); if(c==0){ setInput(wifiSsid); go(ST_WIFI_SSID); } else if(c==1)connectWifiNow(); else if(c==2)go(ST_WIFI_STATUS); else { disconnectWifi(); flash(ui("WIFI DESLIGADO","WIFI OFF")); go(ST_WIFI); } }, ST_MAIN); break;
    case ST_WIFI_SSID: editWifiInput(e,false); break;
    case ST_WIFI_PASS: editWifiInput(e,true); break;
    case ST_WIFI_STATUS: if(e.back)go(ST_WIFI); else if(e.enter)render(); break;
    case ST_SYSTEM: nav(e,9, [](){ int c=gi(); if(c==0)go(ST_STATUS); else if(c==1){languagePreviewOriginal=portuguese;state=ST_LANGUAGE;page=0;selected=portuguese?1:0;render();} else if(c==2)go(ST_BATTERY); else if(c==3){ aboutEggStep=0; go(ST_ABOUT); } else if(c==4){themePreviewOriginal=accentColor;state=ST_THEME;page=accentColor/MAX_VISIBLE;selected=accentColor%MAX_VISIBLE;render();} else if(c==5)exportBackup(); else if(c==6)importTxts(); else if(c==7)askConfirm(ui("LIMPAR DADOS?","RESET DATA?"),ui("APAGAR RECEITAS?","DELETE RECIPES?"),doResetData); else go(ST_SOUND); }, ST_MAIN); break;
    case ST_STATUS: if(e.back||e.enter)go(ST_SYSTEM); break;
    case ST_BATTERY: if(e.back)go(ST_SYSTEM); else if(e.enter)render(); break;
    case ST_ABOUT: {
      if(e.back){ aboutEggStep=0; go(ST_SYSTEM); break; }
      int expected[9] = {1,1,2,2,3,4,3,4,5};
      int got = 0;
      if(e.up) got=1; else if(e.down) got=2; else if(e.left) got=3; else if(e.right) got=4; else if(e.enter) got=5;
      if(got) {
        if(got == expected[aboutEggStep]) {
          aboutEggStep++;
          toneMs(680 + aboutEggStep*45, 22);
          if(aboutEggStep >= 9) {
            aboutEggStep=0;
            if (unlockShokupan()) go(ST_RECIPE_VIEW);
            else goKeep(ST_ABOUT);
          }
        } else {
          aboutEggStep = (got == expected[0]) ? 1 : 0;
          if(got != 5) beepBack();
        }
      }
      break;
    }
    case ST_LANGUAGE: {
      if(e.back){portuguese=languagePreviewOriginal;beepBack();go(ST_SYSTEM);break;}
      int before=gi();
      nav(e,2,nullptr,ST_SYSTEM);
      if(state!=ST_LANGUAGE)break;
      int current=gi();
      if(current!=before){portuguese=current==1;render();}
      if(e.enter){portuguese=current==1;saveAppearanceConfig();flash(ui("IDIOMA SALVO","LANGUAGE SAVED"));go(ST_SYSTEM);}
      break;
    }
    case ST_THEME: {
      if(e.back){accentColor=themePreviewOriginal;applyAccentPalette();beepBack();go(ST_SYSTEM);break;}
      int before=gi();
      nav(e,ACCENT_COLOR_COUNT,nullptr,ST_SYSTEM);
      if(state!=ST_THEME)break;
      int current=gi();
      if(current>=0 && current<ACCENT_COLOR_COUNT && current!=before){accentColor=current;applyAccentPalette();render();}
      if(e.enter){accentColor=current;saveAppearanceConfig();flash(ui("COR SALVA","COLOR SAVED"));go(ST_SYSTEM);}
      break;
    }
    case ST_SOUND: nav(e,5, [](){
      int c=gi();
      if(c==0){ soundOn=!soundOn; if(soundOn && soundVolume==0) soundVolume=6; saveSoundConfig(); flash(soundOn?ui("SOM LIGADO","SOUND ON"):ui("SOM DESLIGADO","SOUND OFF")); goKeep(ST_SOUND); }
      else if(c==1){ soundOn=true; if(soundVolume<10) soundVolume++; saveSoundConfig(); beepOK(); goKeep(ST_SOUND); }
      else if(c==2){ if(soundVolume>0) soundVolume--; if(soundVolume==0) soundOn=false; saveSoundConfig(); beepBack(); goKeep(ST_SOUND); }
      else if(c==3){ beepRecipe(); goKeep(ST_SOUND); }
      else { soundOn=false; soundVolume=0; saveSoundConfig(); flash(ui("SOM DESLIGADO","SOUND OFF")); goKeep(ST_SOUND); }
    }, ST_SYSTEM); break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  uiCanvas.setColorDepth(16);
  uiCanvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  uiCanvas.setFont(&fonts::Font0);
  uiCanvas.setTextSize(1);
  loadAppearanceConfig();
  uiCanvas.setTextColor(AMBER, BG);
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
  if (dnsStarted) dnsServer.processNextRequest();
  if (webStarted) webServer.handleClient();
  updateTimerAlarm();
  if (state == ST_TIMER_RUN) {
    if (timerRunning && !timerPaused && millis()>=timerDeadline) { finishTimer(); }
    static unsigned long last=0; if (millis()-last>500){ last=millis(); render(); }
  }
  if (state == ST_BATTERY) {
    static unsigned long lastBat=0; if (millis()-lastBat>3000){ lastBat=millis(); render(); }
  }
  if (marqueeActive && millis()-marqueeLastFrame>=230) {
    marqueeLastFrame=millis();
    render();
  }
  delay(15);
}

