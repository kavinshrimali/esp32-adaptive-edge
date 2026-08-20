#include "esp_camera.h"

const byte PIR_PIN = 14;
enum state {IDLE, ACTIVE, DECAY}; // The 3 possible states
state curState = IDLE; // Initializing the current state

unsigned long lastMotionTime = 0; // Time since the last motion recorded by the camera
const unsigned long holdTime = 5000; // Time before the camera switches from ACTIVE to DECAY

sensor_t* s; // Pointer that gets assigned to the settings of the ESP-32 Camera after confirming that its initalization succeeded
bool hasPSRAM = false;
const int STRIDE = 16; // The increment in the indexes when looping over the 1D array of the image returned by the camera
const int NOISE = 28; // !!! TODO: Measure the actual base-level noise from the PIR sensor and camera !!!
const size_t MAX_SAMPLES = 307200 / STRIDE;
uint8_t prevPixels[MAX_SAMPLES]; // An array to store previous pixels for the comparison carried out when looping over the 1D image array
                                     // The size of the array is declared as 307200 / STRIDE because VGA framesize is 640 x 480 = 307200 pixels,
                                     // and we are only looking at the pixels that will be looped over. So, we divide the total no. of the pixels
                                     // by the stride (i.e. value by which the index is incremented in the loop). 
bool isFirstFrame = true; // If the current frame being processed is the first, there are no pixels stored in prevPixels to compare with. This boolean 
                          // keeps track of whether or not we are on the first frame.

const int HIGH_MOTION_THRESHOLD = 1000;
const int LOW_MOTION_THRESHOLD = 250;
unsigned long lastCaptureTime = 0;
unsigned long frameDelay = 0;
const unsigned long DECAY_PERIOD = 3000;
unsigned long decayStartTime = 0;
const unsigned long DECAY_CAPTURE_RATE = 500;

void setFramesize(framesize_t size) {
  if (s != NULL) {
    s->set_framesize(s, size);
    delay(30);
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      esp_camera_fb_return(fb);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT); // GPIO 13 on the PIR Sensor

  camera_config_t config;
  config.pin_pwdn = 32; // Configuring the pins on the ESP-32 camera
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

  config.ledc_channel = LEDC_CHANNEL_0; // Configuring settings for the ESP-32 camera
  config.ledc_timer = LEDC_TIMER_0;
  config.xclk_freq_hz = 8000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_VGA;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM; // PSRAM (Pseudo-Static RAM): Larger memory, but higher latency
    config.fb_count = 2;
    hasPSRAM = true;
  }
  else {
    config.fb_location = CAMERA_FB_IN_DRAM; // DRAM (Data RAM): Smaller memory, but lower latency processing
    config.fb_count  = 1;
  }
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err == ESP_OK) {
    Serial.println("Initialization complete.");
    s = esp_camera_sensor_get();
    // setFramesize(FRAMESIZE_QQVGA);
  }
  else {
    Serial.println("Error occurred during initialization.");
  }
}

void loop() {
  bool motionDetected = (digitalRead(PIR_PIN) == HIGH);
  
  switch(curState) {
    case IDLE:
      gpio_wakeup_enable((gpio_num_t)PIR_PIN, GPIO_INTR_HIGH_LEVEL); // Setting the condition that if the PIR_PIN is HIGH then the camera should wakeup. Typesetting PIR_PIN to gpio_num_t because gpio_wakeup_enable() expects this type.
      esp_sleep_enable_gpio_wakeup(); // 
      Serial.flush(); // Sending any Serial output before putting the camera to sleep
      esp_light_sleep_start();

      if (digitalRead(PIR_PIN) == HIGH) { // We have to re-check whether motion has been detected to override the previous value stored by motionDetected  
        lastMotionTime = millis();
        curState = ACTIVE;
        isFirstFrame = true;
        // if (s != NULL && hasPSRAM) {
        //   setFramesize(FRAMESIZE_VGA); // Increasing the frame size from 160 x 120 (QQVGA) to 640 x 480 (VGA)
        // }
        Serial.println("State changed: IDLE -> ACTIVE");
      }
      break;

    case ACTIVE:
      if (millis() - lastCaptureTime >= frameDelay) {
        camera_fb_t* fb = esp_camera_fb_get(); // Declaring a pointer to the memory block assigned to the most recent picture captured by the camera
        if (fb != NULL) {
          int totalDelta = 0;
          lastCaptureTime = millis();
          uint8_t* imageArr = fb->buf;
          size_t imageArrLen = fb->len;
          for (size_t n = 0; n < imageArrLen; n += STRIDE) {
            if (isFirstFrame) {
              prevPixels[n/STRIDE] = imageArr[n];
              continue;
            }
            if (abs(int(imageArr[n]) - int(prevPixels[n/STRIDE])) >= NOISE) { // Typesetting imageArr elements to int and taking abs of difference to prevent errors due to negative values
              totalDelta += 1;
            }
            prevPixels[n/STRIDE] = imageArr[n];
          }
          isFirstFrame = false;
          if (totalDelta >= HIGH_MOTION_THRESHOLD) {
            frameDelay = 0;
          }
          else if (totalDelta >= LOW_MOTION_THRESHOLD) {
            frameDelay = 150;
          }
          else {
            frameDelay = 300;
          }
          esp_camera_fb_return(fb); // Freeing up the memory block so that it can be reused by the driver
        }
        else {
          Serial.println("Error occurred in fetching image.");
        }
      }
      
      if (motionDetected) {
        lastMotionTime = millis();
      }
      else if (millis() - lastMotionTime >= holdTime) {
        curState = DECAY;
        isFirstFrame = true;
        // if (s != NULL) {
        //   setFramesize(FRAMESIZE_QQVGA); // Decreasing the frame size from 640 x 480 (VGA) to 160 x 120 (QQVGA)
        // }
        decayStartTime = millis();
        Serial.println("State changed: ACTIVE -> DECAY");
      }
      break;

    case DECAY:
      if (motionDetected) {
        lastMotionTime = millis();
        // if (s != NULL && hasPSRAM) {
        //   setFramesize(FRAMESIZE_VGA);
        // }
        isFirstFrame = true;
        curState = ACTIVE;
        Serial.println("State changed: DECAY -> ACTIVE");
        break;
      }

      if (millis() - decayStartTime <= DECAY_PERIOD) { // Same logic for image processing as in the ACTIVE state, but the time between captures has been set to DECAY_PERIOD
        if (millis() - lastCaptureTime >= DECAY_CAPTURE_RATE) {
        camera_fb_t* fb = esp_camera_fb_get(); // Declaring a pointer to the memory block assigned to the most recent picture captured by the camera
        if (fb != NULL) {
          lastCaptureTime = millis();
          uint8_t* imageArr = fb->buf;
          size_t imageArrLen = fb->len;
          for (size_t n = 0; n < imageArrLen; n += STRIDE) {
            prevPixels[n/STRIDE] = imageArr[n];
          }
          isFirstFrame = false;
          esp_camera_fb_return(fb); // Freeing up the memory block so that it can be reused by the driver
        }
        else {
          Serial.println("Error occurred in fetching image.");
        }
      }
      }
      else {
        curState = IDLE;
        Serial.println("State changed: DECAY -> IDLE");
      }
      break;
  }
}
