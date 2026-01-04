// coded with help from Gemini. Adapted from Keyestudio example to work with Makerlab kit.

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"           // Disable brownout
#include "soc/rtc_cntl_reg.h"  // Disable brownout
#include "esp_http_server.h"

// =========================================
// WIFI CONFIGURATION
// =========================================
const char *ssid = "ESP32-Robot-Cam"; 
const char *password = "12345678";

// =========================================
// MOTOR PIN DEFINITIONS (SWAPPED FOR CORRECT TURNING)
// =========================================

// LEFT MOTOR (Channel B pins are now controlling the physical Left wheels)
#define LEFT_ENABLE_PIN  2   // CH_B (Was 12)
#define LEFT_IN1_PIN     15  // Input 3 (Was 13)
#define LEFT_IN2_PIN     33  // Input 4 (Was 14)

// RIGHT MOTOR (Channel A pins are now controlling the physical Right wheels)
#define RIGHT_ENABLE_PIN 12  // CH_A (Was 2)
#define RIGHT_IN1_PIN    13  // Input 1 (Was 15)
#define RIGHT_IN2_PIN    14  // Input 2 (Was 33)

// LED Pin
#define LED_GPIO_NUM 4

// CAMERA PINS
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Variables
int MOTOR_Speed = 170; // Start at mid-speed

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t camera_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

void startCameraServer();

// =========================================
// HELPER: MOTOR CONTROL FUNCTION
// =========================================
void setCarState(int L_IN1, int L_IN2, int R_IN1, int R_IN2, int speed) {
  // Set Direction
  digitalWrite(LEFT_IN1_PIN, L_IN1);
  digitalWrite(LEFT_IN2_PIN, L_IN2);
  digitalWrite(RIGHT_IN1_PIN, R_IN1);
  digitalWrite(RIGHT_IN2_PIN, R_IN2);
  
  // Set Speed (The "Gas Pedal")
  // If direction is Stop (all LOW), speed should be 0
  if(L_IN1 == LOW && L_IN2 == LOW) {
    analogWrite(LEFT_ENABLE_PIN, 0);
    analogWrite(RIGHT_ENABLE_PIN, 0);
  } else {
    analogWrite(LEFT_ENABLE_PIN, speed);
    analogWrite(RIGHT_ENABLE_PIN, speed);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout

  // 1. Setup Motor Pins
  pinMode(LEFT_ENABLE_PIN, OUTPUT);
  pinMode(LEFT_IN1_PIN, OUTPUT);
  pinMode(LEFT_IN2_PIN, OUTPUT);
  
  pinMode(RIGHT_ENABLE_PIN, OUTPUT);
  pinMode(RIGHT_IN1_PIN, OUTPUT);
  pinMode(RIGHT_IN2_PIN, OUTPUT);
  
  pinMode(LED_GPIO_NUM, OUTPUT);
  
  // Initialize Motors to STOP
  setCarState(LOW, LOW, LOW, LOW, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(false);

  // 2. Configure Camera
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
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_HVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // 3. FLIP CAMERA (Based on your request)
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);   // Flip Vertical
  s->set_hmirror(s, 1); // Flip Horizontal (keep left/right correct)

  // 4. Start WiFi
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP Started");
  Serial.print("Connect to: ");
  Serial.println(WiFi.softAPIP());

  startCameraServer();
}

void loop() {
}

// HTML & CSS
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Robot</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8"/>
    <style>
      body { font-family: Arial; text-align: center; margin: 0; padding-top: 20px; background-color: #f2f2f2;}
      .button-container {
        display: grid;
        grid-template-areas:
          ". forward led"
          "left stop right"
          "plus backward minus";
        grid-gap: 12px;
        justify-content: center;
        margin-top: 20px;
      }
      .button {
        background-color: #2f4468; color: white; border: none;
        padding: 20px; font-size: 20px; cursor: pointer;
        width: 80px; height: 60px; border-radius: 10px;
        user-select: none;
      }
      .button:active { background-color: #1a2a45; transform: translateY(2px); }
      .led-button { background-color: #777; width: 80px; height: 60px; border-radius: 10px; border:none; color:white;}
      .led-on { background-color: #f0c40f; color: black; }
      
      .forward { grid-area: forward; }
      .led { grid-area: led; }
      .left { grid-area: left; }
      .stop { grid-area: stop; }
      .right { grid-area: right; }
      .backward { grid-area: backward; }
      .plus { grid-area: plus; }
      .minus { grid-area: minus; }

      img { width: auto; max-width: 100%; height: auto; border-radius: 10px; margin-top: 20px; box-shadow: 0px 0px 10px rgba(0,0,0,0.5);}
    </style>
  </head>
  <body>
    <h1>ESP32 Robot</h1>
    <img src="" id="photo">
    <div class="button-container">
      <button class="button forward" onmousedown="send('/action?go=forward');" ontouchstart="send('/action?go=forward');" onmouseup="send('/action?go=stop');" ontouchend="send('/action?go=stop');">↑</button>
      <button id="ledButton" class="led-button led" onclick="toggleLED()">LED</button>
      <button class="button left" onmousedown="send('/action?go=left');" ontouchstart="send('/action?go=left');" onmouseup="send('/action?go=stop');" ontouchend="send('/action?go=stop');">←</button>
      <button class="button stop" onclick="send('/action?go=stop');">STOP</button>
      <button class="button right" onmousedown="send('/action?go=right');" ontouchstart="send('/action?go=right');" onmouseup="send('/action?go=stop');" ontouchend="send('/action?go=stop');">→</button>
      <button class="button backward" onmousedown="send('/action?go=backward');" ontouchstart="send('/action?go=backward');" onmouseup="send('/action?go=stop');" ontouchend="send('/action?go=stop');">↓</button>
      <button class="button plus" onclick="send('/action?go=plus');">+</button>
      <button class="button minus" onclick="send('/action?go=minus');">-</button>
    </div>
    <script>
      window.onload = function () {
        document.getElementById("photo").src = window.location.protocol + "//" + window.location.hostname + ":81/stream";
      };
      
      function send(url) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", url, true);
        xhr.send();
      }
      
      let ledState = false;
      function toggleLED() {
        ledState = !ledState;
        var btn = document.getElementById("ledButton");
        if (ledState) {
          btn.classList.add("led-on");
          send("/action?led=on");
        } else {
          btn.classList.remove("led-on");
          send("/action?led=off");
        }
      }
    </script>
  </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char *part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) res = ESP_FAIL;
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

// =========================================
// ACTION HANDLER (The Controller Logic)
// =========================================
static esp_err_t action_handler(httpd_req_t *req) {
  char query[100];
  int len = httpd_req_get_url_query_len(req) + 1;
  if (len > sizeof(query) || httpd_req_get_url_query_str(req, query, len) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_OK;
  }

  if (strstr(query, "go=forward")) {
    // Forward: LEFT=High/Low, RIGHT=High/Low
    setCarState(HIGH, LOW, HIGH, LOW, MOTOR_Speed);
    
  } else if (strstr(query, "go=backward")) {
    // Backward: LEFT=Low/High, RIGHT=Low/High
    setCarState(LOW, HIGH, LOW, HIGH, MOTOR_Speed);
    
  } else if (strstr(query, "go=left")) {
    // Pivot Left: LEFT=Back (Low/High), RIGHT=Forward (High/Low)
    setCarState(LOW, HIGH, HIGH, LOW, MOTOR_Speed);
    
  } else if (strstr(query, "go=right")) {
    // Pivot Right: LEFT=Forward (High/Low), RIGHT=Back (Low/High)
    setCarState(HIGH, LOW, LOW, HIGH, MOTOR_Speed);
    
  } else if (strstr(query, "go=stop")) {
    setCarState(LOW, LOW, LOW, LOW, 0);
    
  } else if (strstr(query, "led=on")) {
    digitalWrite(LED_GPIO_NUM, HIGH);
  } else if (strstr(query, "led=off")) {
    digitalWrite(LED_GPIO_NUM, LOW);
  } else if (strstr(query, "go=plus")) {
    MOTOR_Speed = min(255, MOTOR_Speed + 40);
  } else if (strstr(query, "go=minus")) {
    MOTOR_Speed = max(80, MOTOR_Speed - 40);
  }
  
  httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  
  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL };
  httpd_uri_t cmd_uri = { .uri = "/action", .method = HTTP_GET, .handler = action_handler, .user_ctx = NULL };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &cmd_uri);
  }
  
  config.server_port += 1;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}
