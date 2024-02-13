#define BLYNK_PRINT Serial
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
//#include <BlynkSimpleEsp32.h>

const int RGB = 12;
const int IR_night = 13;
const int UV = 15;
const int LED_simple = 14;

//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#define CAMERA_MODEL_AI_THINKER  // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD

#include "camera_pins.h"


const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";  //High or Low for LED's

//char token[] = "token_number_of_blink";

// SOME USER INPUTS FOR FIREBASE

// Insert Firebase project API Key
#define API_KEY "REPLACE_WITH_YOUR_FIREBASE_PROJECT_API_KEY"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "REPLACE_WITH_THE_AUTHORIZED_USER_EMAIL"
#define USER_PASSWORD "REPLACE_WITH_THE_AUTHORIZED_USER_PASSWORD"

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "REPLACE_WITH_YOUR_STORAGE_BUCKET_ID"
// For example:
//#define STORAGE_BUCKET_ID "esp-iot-app.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/data/photo.jpg"


AsyncWebServer server(80);

void startCameraServer();

//HTML PART

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
</head>
<body>
  <h2>ESP Web Server</h2>
  %BUTTONPLACEHOLDER%
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}
</script>
</body>
</html>
)rawliteral";


String processor(const String& var) {
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    buttons += "<h4>Output - RGB</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"RGB\" " + outputState(RGB) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - IR_night</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"IR_night\" " + outputState(IR_night) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - UV</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"UV\" " + outputState(UV) + "><span class=\"slider\"></span></label>";
    buttons += "<h4>Output - LED_simple</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"LED_simple\" " + outputState(LED_simple) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

String outputState(int output) {
  if (digitalRead(output)) {
    return "checked";
  } else {
    return "";
  }
}


//FIREBASE PART

void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = false;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void) {

  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  // Photo file name
  Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);  // payload (image), payload length
    Serial.print("The picture has been saved in ");
    Serial.print(FILE_PHOTO_PATH);
    Serial.print(" - Size: ");
    Serial.print(fb->len);
    Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  } else {
    delay(500);
    Serial.println("LittleFS mounted successfully");
  }
}



void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(RGB, OUTPUT);
  pinMode(IR_night, OUTPUT);
  pinMode(UV, OUTPUT);
  pinMode(LED_simple, OUTPUT);

  digitalWrite(RGB, LOW);
  digitalWrite(IR_night, LOW);
  digitalWrite(UV, LOW);
  digitalWrite(LED_simple, LOW);

  //Blynk.begin(token, ssid, password);


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
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;


  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    } else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });

  server.begin();

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
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

  sensor_t* s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif


  //Firebase

  initLittleFS();
  configF.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  configF.token_status_callback = tokenStatusCallback;

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);

  //WIFI

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
}

void loop() {

  //Blynk.run();
  delay(10);


  if (takeNewPhoto) {
    capturePhotoSaveLittleFS();
    takeNewPhoto = false;
  }
  delay(1);
  if (Firebase.ready() && !taskCompleted) {
    taskCompleted = true;
    Serial.print("Uploading picture... ");

    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO_PATH /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, BUCKET_PHOTO /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */, fcsUploadCallback)) {
      Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
    } else {
      Serial.println(fbdo.errorReason());
    }
  }
}


void fcsUploadCallback(FCS_UploadStatusInfo info) {
  if (info.status == firebase_fcs_upload_status_init) {
    Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  } else if (info.status == firebase_fcs_upload_status_upload) {
    Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
  } else if (info.status == firebase_fcs_upload_status_complete) {
    Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
    Serial.printf("Name: %s\n", meta.name.c_str());
    Serial.printf("Bucket: %s\n", meta.bucket.c_str());
    Serial.printf("contentType: %s\n", meta.contentType.c_str());
    Serial.printf("Size: %d\n", meta.size);
    Serial.printf("Generation: %lu\n", meta.generation);
    Serial.printf("Metageneration: %lu\n", meta.metageneration);
    Serial.printf("ETag: %s\n", meta.etag.c_str());
    Serial.printf("CRC32: %s\n", meta.crc32.c_str());
    Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
    Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
  } else if (info.status == firebase_fcs_upload_status_error) {
    Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
  }
}

/*
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

*/
