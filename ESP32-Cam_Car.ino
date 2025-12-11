#include <WiFi.h>
#include <WebServer.h>


const char* ssid = "ESP32-CAM-AP";
const char* password = "12345678";

WebServer server(80);

// 定義 UART2，使用 GPIO 13 作為 TX，GPIO 14 作為 RX

uint8_t speedVal = 50; // 初始速度（0~127）

// HTML 控制頁面（含滑桿）
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Car</title>
  <style>
    body { 
      display: flex; 
      justify-content: space-between; 
      height: 100vh; 
      margin: 0; 
      user-select: none;
      -webkit-user-select: none;

    }
    .column, .row {
      display: flex; flex-direction: column; justify-content: center; align-items: center; width: 50%;
    }
    .row { flex-direction: row; }
    button {
      width: 100px; height: 100px; margin: 20px;
      font-size: 24px; border-radius: 10px;
      touch-action: none;
      outline: none;
      user-select: none;
    }
    input[type=range] {
      width: 200px;
    }
  </style>
</head>
<body>
  <div class="column">
    <button id="forward">Up</button>
    <button id="backward">Down</button>
    <label>speed:
      <input type="range" id="speed" min="50" max="100" value="50">
    </label>
  </div>
  <div class="row">
    <button id="left"><</button>
    <button id="right">></button>
  </div>

  <script>
    const state = { forward: false, backward: false, left: false, right: false, speed: 50 };
    let lastSent = 0;
  
    function sendControl() {
      const now = Date.now();
      if (now - lastSent >= 100) {
        lastSent = now;
        const query = Object.entries(state)
          .map(([k, v]) => `${k}=${v}`)
          .join("&");
        fetch(`/control?${query}`);
      }
      requestAnimationFrame(sendControl);
    }
  
    function setState(key, value) {
      state[key] = value;
    }
  
    function setupButton(id) {
      const btn = document.getElementById(id);
      btn.addEventListener("mousedown", () => { setState(id, true); btn.classList.add("active"); });
      btn.addEventListener("mouseup", () => { setState(id, false); btn.classList.remove("active"); });
      btn.addEventListener("mouseleave", () => { setState(id, false); btn.classList.remove("active"); });
      btn.addEventListener("touchstart", () => { setState(id, true); btn.classList.add("active"); });
      btn.addEventListener("touchend", () => { setState(id, false); btn.classList.remove("active"); });
    }
  
    ["forward", "backward", "left", "right"].forEach(setupButton);
  
    document.getElementById("speed").addEventListener("input", (e) => {
      state.speed = e.target.value;
    });
  
    sendControl(); // 啟動循環

  </script>
</body>
</html>
)rawliteral";


#define UART2_RX 15
#define UART2_TX 14
#define ERR_PIN 13
//#define ERR_PIN_INT_ENABLE
#define RET_PIN 12

#ifdef ERR_PIN
volatile bool qikErrorFlag = false;
#endif
unsigned long lastCommandTime = 0;
bool isMoving = false;
// 全域變數：記錄上一次狀態
bool lastForward = false;
bool lastBackward = false;
bool lastLeft = false;
bool lastRight = false;
int lastSpeed = 0;


#ifdef ERR_PIN_INT_ENABLE
void IRAM_ATTR onQikError() {
  qikErrorFlag = true; // 只設旗標
}
#endif

void uart2_init(){
  #ifdef RET_PIN
    pinMode(RET_PIN, OUTPUT);
    // 初始設為 LOW（低電位，安全狀態）
    digitalWrite(RET_PIN, LOW);
    delay(100);  // 延遲觀察
  #endif
    
    // 初始化額外 UART2，波特率 9600, RX=15, TX=14
    Serial2.begin(9600, SERIAL_8N1, UART2_RX, UART2_TX);
    
    Serial.println("額外 UART 已初始化");
  
  #ifdef RET_PIN
    // 設為 HIGH（高電位）
    digitalWrite(RET_PIN, HIGH);
  #endif
}

void setup() {
  Serial.begin(115200);

  pinMode(ERR_PIN, INPUT_PULLDOWN); // 如果 ERR 是開漏輸出，建議用 PULLUP
#ifdef ERR_PIN_INT_ENABLE
  attachInterrupt(digitalPinToInterrupt(ERR_PIN), onQikError, HIGH);
#endif

  uart2_init();

  // 啟動 WiFi 熱點
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  // 初始化 qik
  Serial2.write(0xAA); // 起始位元
  
  // 控制頁面
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  

  // 控制指令處理
  server.on("/control", []() {
    bool forward = server.arg("forward") == "true";
    bool backward = server.arg("backward") == "true";
    bool left = server.arg("left") == "true";
    bool right = server.arg("right") == "true";
    int speedVal = server.arg("speed").toInt();

    // 更新時間戳記
    lastCommandTime = millis();
  
    // 控制馬達（略）
    if (forward || backward || left || right) {
      isMoving = true;
      // 控制馬達...
    } else {
      isMoving = false;
      // 停止馬達...
    }

#if 1
    Serial.print("時間：");
    Serial.print(lastCommandTime);
    Serial.print(" 指令：");
    if (forward) Serial.print("前進 ");
    if (backward) Serial.print("後退 ");
    if (left) Serial.print("左轉 ");
    if (right) Serial.print("右轉 ");
    if (!forward && !backward && !left && !right) Serial.print("停止 ");
    Serial.print(" 速度：");
    Serial.println(speedVal);
#endif

    // 判斷是否有變化
    if (forward != lastForward || backward != lastBackward || 
        left != lastLeft || right != lastRight || 
        speedVal != lastSpeed) {
  
      // 更新狀態
      lastForward = forward;
      lastBackward = backward;
      lastLeft = left;
      lastRight = right;
      lastSpeed = speedVal;



      // 馬達A 控制
      if (forward) {
        setMotor0(speedVal); // 馬達A 前進
      } else if (backward) {
        setMotor0(-speedVal); // 馬達A 後退
      } else {
        setMotor0(0);
      }
  
      // 馬達B 控制
      if (left) {
        setMotor1(-100); // 馬達B 左轉
      } else if (right) {
        setMotor1(100);  // 馬達B 右轉
      } else {
        setMotor1(0);
      }
    }

    server.send(200, "text/plain", "OK");
  });


  server.begin();
}

void loop() {
  server.handleClient();

  // 若超過 300ms 沒收到指令 → 自動停止
  if (isMoving && millis() - lastCommandTime > 300) {
    isMoving = false;
    // 停止馬達
    stopMotors();
  }
  
#ifdef ERR_PIN_INT_ENABLE
  if (qikErrorFlag) {
    qikErrorFlag = false;
    // 在這裡安全地讀取錯誤碼
    checkQikError();
  }
#else
  int value = digitalRead(ERR_PIN);  // 讀取 ERR_PIN 的狀態
  if (value) {
    Serial.printf("value=%d\n", value);
    // 在這裡安全地讀取錯誤碼
    checkQikError();
  }
#endif
}

bool checkQikError() {
  Serial2.write(0x82);  // 送 Get Error Byte
  delay(10);  // 短延遲等回應 (Qik 回傳 1 byte)
  if (Serial2.available()) {
    uint8_t err = Serial2.read();
    if (err == 0) {
      return true;  // 無錯誤，假設指令 OK
    } else {
      Serial.printf("Qik 錯誤: 0x%02X\n", err);  // Debug 到 Serial
      // 可根據 err 處理 (e.g., bit 4: Frame Error 表示 baud 不匹配)
      return false;
    }
  }
  return false;  // 無回應，視為失敗
}

void setMotor0(int speed) {
  if (speed > 0) {
    Serial2.write(0x88); // Motor0 forward
    Serial2.write(speed > 127 ? 127 : speed);
  } else if (speed < 0) {
    Serial2.write(0x8A); // Motor0 reverse
    Serial2.write((-speed) > 127 ? 127 : -speed);
  } else {
    Serial2.write(0x86); // Motor0 coast
  }
  delay(5);
}

void setMotor1(int speed) {
  if (speed > 0) {
    Serial2.write(0x8C); // Motor1 forward
    Serial2.write(speed > 127 ? 127 : speed);
  } else if (speed < 0) {
    Serial2.write(0x8E); // Motor1 reverse
    Serial2.write((-speed) > 127 ? 127 : -speed);
  } else {
    Serial2.write(0x87); // Motor1 coast
  }
  delay(5);
}

void stopMotors() {
  setMotor0(0);
  setMotor1(0);
}
