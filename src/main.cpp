#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>

// esp32 sdk imports
#include "esp_heap_caps.h"
#include "esp_log.h"

// epd
#include "epd_driver.h"
#include "epd_highlevel.h"

// battery
#include <driver/adc.h>
#include "esp_adc_cal.h"

// deepsleep
#include "esp_sleep.h"

// fonts
#include "firasans.h"
//#include "firasans_12.h"
#include "OpenSans8B.h"

// HTTP
#include <WiFi.h>
#include <HTTPClient.h>
#include <WifiClientSecure.h>
#include <ArduinoJson.h>
#include "cryptos.h"
#include "coingecko-api.h"

#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//#include <Wire.h>
//#include <SPI.h>
//#include <SD.h>
//#include <hal/rmt_ll.h>

//#define SD_MISO 12
//#define SD_MOSI 13
//#define SD_SCLK 14
//#define SD_CS 15

#define White 0xF0
#define LightGrey 0xB0
#define Grey 0x80
#define DarkGrey 0x40
#define Black 0x00

int cursor_x;
int cursor_y;

int vref = 1100;

String date;

#define BATT_PIN 36

#define WAVEFORM EPD_BUILTIN_WAVEFORM

/**
 * Upper most button on side of device. Used to setup as wakeup source to start from deepsleep.
 * Pinout here https://ae01.alicdn.com/kf/H133488d889bd4dd4942fbc1415e0deb1j.jpg
 */
gpio_num_t FIRST_BTN_PIN = GPIO_NUM_39;

EpdiyHighlevelState hl;

// ambient temperature around device
int temperature = 25;
uint8_t *fb;
enum EpdDrawError err;

// CHOOSE HERE YOU IF YOU WANT PORTRAIT OR LANDSCAPE
// both orientations possible
// EpdRotation orientation = EPD_ROT_PORTRAIT;
EpdRotation orientation = EPD_ROT_LANDSCAPE;

// ----------------------------
// Configurations - Update these
// ----------------------------
const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
const boolean staticIp = 0;

IPAddress local_IP(192, 168, 31, 115);
IPAddress gateway(192, 168, 31, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 31, 254); // optional
// IPAddress secondaryDNS(8, 8, 4, 4); //optional

// ----------------------------
// End of area you need to change
// ----------------------------

double_t get_battery_percentage()
{
  // When reading the battery voltage, POWER_EN must be turned on
  epd_poweron();
  delay(50);

  Serial.println(epd_ambient_temperature());

  uint16_t v = analogRead(BATT_PIN);
  Serial.print("Battery analogRead value is");
  Serial.println(v);
  double_t battery_voltage = ((double_t)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

  // Better formula needed I suppose
  // experimental super simple percent estimate no lookup anything just divide by 100
  double_t percent_experiment = ((battery_voltage - 3.7) / 0.5) * 100;

  // cap out battery at 100%
  // on charging it spikes higher
  if (percent_experiment > 100)
  {
    percent_experiment = 100;
  }

  String voltage = "Battery Voltage :" + String(battery_voltage) + "V which is around " + String(percent_experiment) + "%";
  Serial.println(voltage);

  epd_poweroff();
  delay(50);

  return percent_experiment;
}

/**
 * Correct the ADC reference voltage. Was in example of lilygo epd47 repository to calc battery percentage
 */
void correct_adc_reference()
{
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
  {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    Serial.println();
    vref = adc_chars.vref;
  }
}

/**
 * Function that prints the reason by which ESP32 has been awaken from sleep
 */
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

void connectToWifi()
{

  // Serial.println("scan start");
  /**
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
  {
    Serial.println("no networks found");
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  // Wait a bit before scanning again
  delay(1000);
  **/
  if (staticIp && !WiFi.config(local_IP, gateway, subnet, primaryDNS))
  {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    delay(300);
  }

  Serial.println("Connected!");
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP Mac Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());
}

void renderFooter()
{
  cursor_y = 520 + 15;
  cursor_x = 20;
  date = date.substring(0, 26);
  date = "Last update: " + date;
  Serial.println(date);
  char ts[date.length()];
  date.toCharArray(ts, date.length());
  // Serial.println("Date length = " + String(date.length()));
  String str = String(ts[30]) + String(ts[31]);
  // Serial.println("Old hour = " + str);
  int hh = str.toInt() + 8;
  // Serial.println("New hour = " + String(hh));
  char strnew[1];
  sprintf(strnew, "%02d", hh);
  // Serial.println("First digit = " + String(strnew[0]));
  // Serial.println("Second digit = " + String(strnew[1]));

  if (hh > 23)
  {
    hh = hh - 24;
  }
  sprintf(strnew, "%02d", hh);
  ts[30] = strnew[0];
  ts[31] = strnew[1];
  EpdFontProperties font_props = epd_font_properties_default();
  font_props.flags = EPD_DRAW_ALIGN_LEFT;

  EpdRect area = {
      .x = 1,
      .y = 520,
      .width = 370,
      .height = 18,
  };
  epd_fill_rect(area, White, fb);
  // err = epd_hl_update_area(&hl, MODE_GC16, temperature, area);
  epd_write_string(&OpenSans8B, ts, &cursor_x, &cursor_y, fb, &font_props);
  epd_poweron();
  err = epd_hl_update_area(&hl, MODE_GC16, temperature, area);
}

String formatPercentageChange(double change)
{

  double absChange = change;

  if (change < 0)
  {
    absChange = -change;
  }

  if (absChange > 100)
  {
    return String(absChange, 0) + "%";
  }
  else if (absChange >= 10)
  {
    return String(absChange, 1) + "%";
  }
  else
  {
    return String(absChange) + "%";
  }
}

void renderCryptoCard(Crypto crypto)
{
  EpdFontProperties font_props = epd_font_properties_default();
  cursor_x = 220;

  Serial.print("price usd (double) - ");
  Serial.println(crypto.price.usd, 10);
  String Str;
  if (crypto.price.usd < 0.00001)
  {
    Str = String(crypto.price.usd, 10);
  }
  else if (crypto.price.usd < 1)
  {
    Str = String(crypto.price.usd, 5);
  }
  else
  {
    Str = (String)(crypto.price.usd);
  }
  char *string2 = &Str[0];

  epd_write_string(&FiraSans, string2, &cursor_x, &cursor_y, fb, &font_props);

  Serial.print("Day change - ");
  Serial.println(formatPercentageChange(crypto.dayChange));

  cursor_x = 530;

  Str = (String)(crypto.dayChange);
  char *string3 = &Str[0];

  epd_write_string(&FiraSans, string3, &cursor_x, &cursor_y, fb, &font_props);

  Serial.print("Week change - ");
  Serial.println(formatPercentageChange(crypto.weekChange));

  cursor_x = 800;

  Str = (String)(crypto.weekChange);
  char *string4 = &Str[0];

  epd_write_string(&FiraSans, string4, &cursor_x, &cursor_y, fb, &font_props);
}

void renderColumnSym(Crypto crypto)
{
  EpdFontProperties font_props = epd_font_properties_default();
  cursor_x = 50;
  char *string1 = &crypto.symbol[0];
  epd_write_string(&FiraSans, string1, &cursor_x, &cursor_y, fb, &font_props);
}

void renderCell(String str, int w)
{
  EpdFontProperties font_props = epd_font_properties_default();
  font_props.flags = EPD_DRAW_ALIGN_LEFT;
  char text[str.length()];
  sprintf(text, "%s", str);
  // Serial.print("Debug: ");
  // Serial.println(text);
  // Serial.printf("X: %d Y: %d", cursor_x, cursor_y);
  // Serial.println();
  EpdRect area = {
      .x = cursor_x,
      .y = cursor_y - 40,
      .width = w,
      .height = 50,
  };
  epd_fill_rect(area, White, fb);
  epd_write_string(&FiraSans, text, &cursor_x, &cursor_y, fb, &font_props);
  epd_poweron();
  err = epd_hl_update_area(&hl, MODE_GC16, temperature, area);
}

void renderHeader()
{
  EpdFontProperties font_props = epd_font_properties_default();
  cursor_x = 20;
  cursor_y = 50;
  char sym[] = "Symbol";
  // writeln((GFXfont *)&FiraSans, sym, &cursor_x, &cursor_y, NULL);
  epd_write_string(&FiraSans, sym, &cursor_x, &cursor_y, fb, &font_props);

  cursor_x = 290;
  cursor_y = 50;
  char prc[] = "Price";
  // writeln((GFXfont *)&FiraSans, prc, &cursor_x, &cursor_y, NULL);
  epd_write_string(&FiraSans, prc, &cursor_x, &cursor_y, fb, &font_props);

  cursor_x = 520;
  cursor_y = 50;
  char da[] = "Day(%)";
  // writeln((GFXfont *)&FiraSans, da, &cursor_x, &cursor_y, NULL);
  epd_write_string(&FiraSans, da, &cursor_x, &cursor_y, fb, &font_props);

  cursor_x = 790;
  cursor_y = 50;
  char we[] = "Week(%)";
  // writeln((GFXfont *)&FiraSans, we, &cursor_x, &cursor_y, NULL);
  epd_write_string(&FiraSans, we, &cursor_x, &cursor_y, fb, &font_props);
}

void display_center_message(const char *text)
{
  // first set full screen to white
  epd_hl_set_all_white(&hl);

  int cursor_x = EPD_WIDTH / 2;
  int cursor_y = EPD_HEIGHT / 2;
  if (orientation == EPD_ROT_PORTRAIT)
  {
    // height and width switched here because portrait mode
    cursor_x = EPD_HEIGHT / 2;
    cursor_y = EPD_WIDTH / 2;
  }
  EpdFontProperties font_props = epd_font_properties_default();
  font_props.flags = EPD_DRAW_ALIGN_CENTER;
  epd_write_string(&FiraSans, text, &cursor_x, &cursor_y, fb, &font_props);

  err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
}

void renderSymbols()
{
  for (int i = 0; i < cryptosCount; i++)
  {
    cursor_x = 50;
    cursor_y = (50 * (i + 2));
    renderCell(cryptos[i].symbol, 0);
  }
}

void renderRows()
{
  for (int i = 0; i < cryptosCount; i++)
  {
    // Render price column
    cursor_x = 220;
    cursor_y = (50 * (i + 2));
    if (cryptos[i].price.usd < 0.00001)
      renderCell(String(cryptos[i].price.usd, 10), 300);
    else if (cryptos[i].price.usd < 1)
      renderCell(String(cryptos[i].price.usd, 5), 300);
    else
      renderCell((String)(cryptos[i].price.usd), 300);
    // Render day change column
    cursor_x = 530;
    cursor_y = (50 * (i + 2));
    renderCell((String)(cryptos[i].dayChange), 150);
    // Render day change column
    cursor_x = 800;
    cursor_y = (50 * (i + 2));
    renderCell((String)(cryptos[i].weekChange), 150);
  }
}

void setup()
{
  Serial.begin(115200);
  correct_adc_reference();
  // First setup epd to use later
  epd_init(EPD_OPTIONS_DEFAULT);
  hl = epd_hl_init(WAVEFORM);
  epd_set_rotation(orientation);
  fb = epd_hl_get_framebuffer(&hl);
  epd_clear();
  epd_poweron();
  print_wakeup_reason();
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);
  Serial.println("Setup done, connecting to WiFi...");
  display_center_message("Connecting to WiFi...");
  connectToWifi();
  display_center_message("Please wait while downloading data...");
  downloadBaseData("usd");
  delay(500);
  downloadBtcAndEthPrice();
  epd_hl_set_all_white(&hl);
}

void loop()
{
  renderHeader();
  err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
  // Render Symbol column
  renderSymbols();
  err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
  // Render rows
  renderRows();
  renderFooter();
  // err = epd_hl_update_screen(&hl, MODE_GC16, temperature);
  epd_poweroff();
  delay(5000);
  downloadBaseData("usd");
  delay(500);
  downloadBtcAndEthPrice();
  epd_poweron;
  // epd_hl_set_all_white(&hl);
}