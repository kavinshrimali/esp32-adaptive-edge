#include "esp_camera.h"

const byte PIR_PIN = 14;
sensor_t* s;
const int STRIDE = 16;
const size_t MAX_SAMPLES = 307200 / STRIDE;
uint8_t prevPixels[MAX_SAMPLES];
bool isFirstFrame = true;

bool hasPSRAM = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);

  camera_config_t config;

  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.pin_xclk = 0;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_d7 = 35;
  config.pin_d6 = 34;
  config.pin_d5 = 39;
  config.pin_d4 = 36;
  config.pin_d3 = 21;
  config.pin_d2 = 19;
  config.pin_d1 = 18;
  config.pin_d0 = 5;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_pclk = 22;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_VGA;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 2;
    hasPSRAM = true;
  }
  else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
  }
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    Serial.println("Initialization complete");
    s = esp_camera_sensor_get();
  }
  else {
    Serial.println("Error occurred during initialization");
  }
}

void loop() {
  bool isMotionDetected = (digitalRead(PIR_PIN) == HIGH);
  if (isMotionDetected) {
    Serial.println("Motion detected");
  }
  else {
    Serial.println("No motion detected");
  }

  camera_fb_t* fb = esp_camera_fb_get();
  int totalNoise = 0;
  int maxDiff = 0;

  if (fb != NULL) {
    uint8_t* imageArr = fb->buf;
    size_t imageArrLen = fb->len;
    for (size_t n = 0; n < imageArrLen; n += STRIDE) {
      if (isFirstFrame) {
        prevPixels[n/STRIDE] = imageArr[n];
        continue; 
      }
      else {
        int pixelDiff = abs(int(imageArr[n]) - int(prevPixels[n/STRIDE]));
        if (pixelDiff > 0) {
          totalNoise += 1;
          if (pixelDiff > maxDiff) {
            maxDiff = pixelDiff;
          }
        }
        prevPixels[n/STRIDE] = imageArr[n];
      }
    }
    isFirstFrame = false;
    esp_camera_fb_return(fb);
  }

  delay(500);

  Serial.println("Max Difference: ");
  Serial.println(maxDiff);
  Serial.println("Total Noise: ");
  Serial.println(totalNoise);
}
