#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "ESP32OTAPull.h"


#define GFX_BL 38

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  39 /* CS */, 48 /* SCK */, 47 /* SDA */,
  18 /* DE */, 17 /* VSYNC */, 16 /* HSYNC */, 21 /* PCLK */,
  11 /* R0 */, 12 /* R1 */, 13 /* R2 */, 14 /* R3 */, 0 /* R4 */,
  8 /* G0 */, 20 /* G1 */, 3 /* G2 */, 46 /* G3 */, 9 /* G4 */, 10 /* G5 */,
  4 /* B0 */, 5 /* B1 */, 6 /* B2 */, 7 /* B3 */, 15 /* B4 */
);
Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
  bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
  true /* IPS */, 480 /* width */, 480 /* height */,
  st7701_type1_init_operations, sizeof(st7701_type1_init_operations),     true /* BGR */,
  10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
  10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

//#include "touch.h"

static uint32_t screenWidth;
static uint32_t screenHeight;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;
static lv_coord_t tab_h;

static lv_obj_t * tv;
static lv_obj_t * label;

float Kp_val = 9.9;
int Index = 999;

unsigned long startMillis;  //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long period = 3 * 60 * 1000;  //the value is a number of milliseconds

// https://github.com/JimSHED/ESP32-OTA-Pull-GitHub
#define RELEASE_URL "https://raw.githubusercontent.com/hugomeiland/aurora-kp-display/refs/heads/main/release.json"
#define VERSION    "0.0" // The current version of this program

WiFiManager wm;

void getKp() {
  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setInsecure();
    HTTPClient https;
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://services.swpc.noaa.gov/text/3-day-forecast.txt")) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      int httpCode = https.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          int payloadIndex = 0;
          int lineStart = 0;
          int lineEnd = 0;
          int payloadLength = payload.length();
          while (payloadIndex < payload.length()) {
            lineEnd = payload.indexOf("\n", payloadIndex);
            String line = payload.substring(lineStart, lineEnd);
            if (line.startsWith("The greatest expected 3 hr Kp for")) {
              Serial.println(line);
              int kpStart = line.indexOf(" is ");
              int kpEnd = line.indexOf(" (");
              String KpString = line.substring(kpStart + 4, kpEnd);
              Serial.println(KpString);
              Kp_val = KpString.toFloat();
            }
            //Serial.println(line);
            payloadIndex = lineEnd + 1;
            lineStart = lineEnd + 1;
          }
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  } else { Serial.printf("[HTTPS] Unable to connect\n"); }
  client->stop();
}

void getIndex() {
  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setInsecure();
    HTTPClient https;
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, "https://services.swpc.noaa.gov/text/aurora-nowcast-hemi-power.txt")) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      int httpCode = https.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          String SouthIndexString = payload.substring(payload.length() - 4, payload.length() - 1);
          String NorthIndexString = payload.substring(payload.length() - 12, payload.length() - 9);
          Serial.println(NorthIndexString);
          Serial.println(SouthIndexString);
          Index = NorthIndexString.toInt();
          Serial.println(Index);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  client->stop();
}


void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
  lv_disp_flush_ready(disp);
}

/*void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  if (touch_has_signal())
  {
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*//*
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else if (touch_released())
    {
      data->state = LV_INDEV_STATE_REL;
    }
  }
  else
  {
    data->state = LV_INDEV_STATE_REL;
  }
}*/

void setup()
{
  Serial.begin(115200);
  delay(3000);
  Serial.println("AuroraAlert");
  //touch_init();
  gfx->begin(16000000); /* specify data bus speed */
  gfx->fillScreen(BLACK);
#ifdef GFX_BL
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
#endif
  lv_init();
  screenWidth = gfx->width();
  screenHeight = gfx->height();
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * 10, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  
  Serial.println("Setting up screen");

  lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, screenWidth * 10);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  //indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
      
  Serial.println("Setting up main page");

  font_large = &lv_font_montserrat_30; 
  font_normal = &lv_font_montserrat_24; 
  tab_h = 80;
  lv_theme_default_init(NULL, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), LV_THEME_DEFAULT_DARK, font_normal);
  tv = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, tab_h);
  //lv_obj_set_style_text_font(lv_scr_act(), font_large, 0);
  lv_obj_t * t1 = lv_tabview_add_tab(tv, "AuroraAlert");
  lv_obj_set_style_text_font(tv, font_large, 0);

  label = lv_label_create(t1);
  char buf[64];
  snprintf(buf, 64, "Please configure wifi \n using the AuroraAlert hotspot...");
  lv_label_set_text(label, buf);
  lv_obj_set_style_text_font(label, font_normal, 0);
  lv_obj_set_style_text_color(tv, lv_color_hex(0x00ff00), LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  
  Serial.println("Update screen");
  lv_timer_handler(); /* let the GUI do its work */
  
  Serial.println("Starting Wifi");
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalTimeout(300);
  bool res = wm.autoConnect("AuroraAlert");
  
  Serial.println("Checking for updates");
  snprintf(buf, 64, "Wifi started, checking for updates...");
  lv_label_set_text(label, buf);
  lv_timer_handler(); /* let the GUI do its work */
  ESP32OTAPull ota;
  Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s'.\n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str());
  int ret = ota.CheckForOTAUpdate(RELEASE_URL, VERSION);
  Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));


  startMillis = millis() - period;  //initial start time, triggers initial update in the loop
  Serial.println("Setup done");
}


void label_refresher_task()
{
    getKp();
    getIndex();

    if(lv_obj_get_screen(label) == lv_scr_act()) {
            char buf[64];
            snprintf(buf, 64, "Highest expected 3-day Kp: %.1f \n\n Expected 1h energy index: %d", Kp_val, Index);
            lv_label_set_text(label, buf);
    }
    
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  if (currentMillis - startMillis >= period)  //test whether the period has elapsed
  {
    label_refresher_task();
    startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.
  }
}

const char *errtext(int code)
{
	switch(code)
	{
		case ESP32OTAPull::UPDATE_AVAILABLE:
			return "An update is available but wasn't installed";
		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
			return "No profile matches";
		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
			return "Profile matched, but update not applicable";
		case ESP32OTAPull::UPDATE_OK:
			return "An update was done, but no reboot";
		case ESP32OTAPull::HTTP_FAILED:
			return "HTTP GET failure";
		case ESP32OTAPull::WRITE_ERROR:
			return "Write error";
		case ESP32OTAPull::JSON_PROBLEM:
			return "Invalid JSON";
		case ESP32OTAPull::OTA_UPDATE_FAIL:
			return "Update fail (no OTA partition?)";
		default:
			if (code > 0)
				return "Unexpected HTTP response code";
			break;
	}
	return "Unknown error";
}
