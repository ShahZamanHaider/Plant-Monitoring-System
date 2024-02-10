#define BLYNK_PRINT Serial
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

const int RGB =  12;
const int IR_night = 13;
const int UV = 15;
const int LED_simple = 14;

unsigned long photoInterval = 0;
unsigned long lastPhotoTime = 0;

//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD

#include "camera_pins.h"


char ssid[] = "REPLACE_WITH_YOUR_SSID";
char password[] = "REPLACE_WITH_YOUR_PASSWORD";
char token[] = "token_number_of_blink";


void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(RGB, OUTPUT);
  pinMode(IR_night, OUTPUT);
  pinMode(UV, OUTPUT);
  pinMode(LED_simple, OUTPUT);

  Blynk.begin(token, ssid, password);


  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  startCameraServer();

  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.println(WiFi.softAPIP());
  WiFi.setAutoReconnect(true);

  lastPhotoTime = millis(); //millis is used to current time
}

void loop() {

  Blynk.run();   
  delay(10);
  
  unsigned long currentTime = millis();
  if (photoInterval > 0 && currentTime - lastPhotoTime >= photoInterval) {  //logic for picture capturing
    takePicture();
    lastPhotoTime = currentTime;
  }
  delay(5);

}

BLYNK_WRITE(V0) { //for UV LIGHT
  int uvValue = param.asInt();

  switch(uvValue){
  case 0:
   digitalWrite(UV, LOW); 
     break;
  case 1:
   digitalWrite(UV, HIGH); 
     break;
  default:
   digitalWrite(UV, LOW); 
  }
  
}

BLYNK_WRITE(V1) { //for IR(N) LIGHT
  int irValue = param.asInt();

   switch(irValue){
  case 0:
   digitalWrite(IR_night, LOW); 
     break;
  case 1:
   digitalWrite(IR_night, HIGH); 
     break;
  default:
   digitalWrite(IR_night, LOW); 
  }
  
}

BLYNK_WRITE(V2) { //for LEDsimple
  int led_simple_Value = param.asInt();

   switch(led_simple_Value){
  case 0:
   digitalWrite(LED_simple, LOW); 
     break;
  case 1:
   digitalWrite(LED_simple, HIGH); 
     break;
  default:
   digitalWrite(LED_simple, LOW); 
  }
}

BLYNK_WRITE(V3) { //for RGB LIGHT
  int rgbValue = param.asInt();

   switch(rgbValue){
  case 0:
   digitalWrite(RGB, LOW); 
     break;
  case 1:
   digitalWrite(RGB, HIGH); 
     break;
  default:
   digitalWrite(RGB, LOW); 
  }
}

BLYNK_WRITE(V4) { //for frame size
  sensor_t * s = esp_camera_sensor_get();
  int resolutionValue = param.asInt();
  switch (resolutionValue) {
    case 0:
      s->set_framesize(s, FRAMESIZE_UXGA);  // (1600 x 1200)
      break;
    case 1:
      s->set_framesize(s, FRAMESIZE_QVGA); // (320 x 240)
      break;
    case 2:
      s->set_framesize(s, FRAMESIZE_CIF); // (352 x 288)
      break;
    case 3:
      s->set_framesize(s, FRAMESIZE_VGA); // (640 x 480)
      break;
    case 4:
      s->set_framesize(s, FRAMESIZE_SVGA); // (800 x 600)
      break;
    case 5:
      s->set_framesize(s, FRAMESIZE_XGA); // (1024 x 768)
      break;
    case 6:
     s->set_framesize(s, FRAMESIZE_SXGA); // (1280 x 1024)
      break;
    default:
     s->set_framesize(s, FRAMESIZE_QVGA); // (320 x 240)
      
  }
  
}

BLYNK_WRITE(V5) { //for brightness
  sensor_t * s = esp_camera_sensor_get();
  int brightness_value = param.asInt();

  switch(brightness_value){
  case -2:
   s->set_brightness(s, -2);
     break;
  case -1:
   s->set_brightness(s, -1);
     break;
  case 0:
   s->set_brightness(s, 0);
     break;
  case 1:
   s->set_brightness(s, 1);
     break;
  case 2:
   s->set_brightness(s, 2);
     break;
  default:
    s->set_brightness(s, 0);
  }

}

BLYNK_WRITE(V6) { //for contrast
  sensor_t * s = esp_camera_sensor_get();
  int contrast_value = param.asInt();

  switch(contrast_value){
  case -2:
   s->set_contrast(s, -2);
     break;
  case -1:
   s->set_contrast(s, -1);
     break;
  case 0:
   s->set_contrast(s, 0);
     break;
  case 1:
   s->set_contrast(s, 1);
     break;
  case 2:
   s->set_contrast(s, 2);
     break;
  default:
    s->set_contrast(s, 0);
  }

}

BLYNK_WRITE(V7) { //for saturation
  sensor_t * s = esp_camera_sensor_get();
  int saturation_value = param.asInt();

  switch(saturation_value){
  case -2:
   s->set_saturation(s, -2);
     break;
  case -1:
   s->set_saturation(s, -1);
     break;
  case 0:
   s->set_saturation(s, 0);
     break;
  case 1:
   s->set_saturation(s, 1);
     break;
  case 2:
   s->set_saturation(s, 2);
     break;
  default:
    s->set_saturation(s, 0);
  }

}

BLYNK_WRITE(V8) {
  photoInterval = param.asInt() * 1000 * 60;
}

void takePicture() {
  camera_fb_t *fb = NULL;
  // Take a picture
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  esp_camera_fb_return(fb); //reset the video stream
}
