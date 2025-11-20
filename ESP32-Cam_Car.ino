#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-CAM-AP";
const char* password = "12345678";

WebServer server(80);



uint8_t speedVal = 200; // 初始速度（0~255）

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
      <input type="range" id="speed" min="0" max="255" value="200">
    </label>
  </div>
  <div class="row">
    <button id="left"><</button>
    <button id="right">></button>
  </div>

  <script>
    const state = { forward: false, backward: false, left: false, right: false, speed: 200 };
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

// 馬達A（驅動輪）
#define A_IN1 12
#define A_IN2 13
#define A_ENA 14  // PWM 控速

// 馬達B（方向輪）
#define B_IN1 15
#define B_IN2 2

unsigned long lastCommandTime = 0;
bool isMoving = false;

void setup() {
  Serial.begin(115200);

  // 馬達腳位初始化
  pinMode(A_IN1, OUTPUT);
  pinMode(A_IN2, OUTPUT);
  pinMode(B_IN1, OUTPUT);
  pinMode(B_IN2, OUTPUT);
  ledcSetup(0, 5000, 8); // 通道0, 頻率5kHz, 解析度8bit
  ledcAttachPin(A_ENA, 0);


  // 啟動 WiFi 熱點
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

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
    int speed = server.arg("speed").toInt();

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
  
    Serial.print("時間：");
    Serial.print(lastCommandTime);
    Serial.print(" 指令：");
    if (forward) Serial.print("前進 ");
    if (backward) Serial.print("後退 ");
    if (left) Serial.print("左轉 ");
    if (right) Serial.print("右轉 ");
    if (!forward && !backward && !left && !right) Serial.print("停止 ");
    Serial.print(" 速度：");
    Serial.println(speed);




    // 馬達A 控制
    if (forward) {
      digitalWrite(A_IN1, HIGH);
      digitalWrite(A_IN2, LOW);
      ledcWrite(0, speedVal);
    } else if (backward) {
      digitalWrite(A_IN1, LOW);
      digitalWrite(A_IN2, HIGH);
      ledcWrite(0, speedVal);
    } else {
      digitalWrite(A_IN1, LOW);
      digitalWrite(A_IN2, LOW);
      ledcWrite(0, 0);
    }

    // 馬達B 控制
    if (left) {
      digitalWrite(B_IN1, HIGH);
      digitalWrite(B_IN2, LOW);
    } else if (right) {
      digitalWrite(B_IN1, LOW);
      digitalWrite(B_IN2, HIGH);
    } else {
      digitalWrite(B_IN1, LOW);
      digitalWrite(B_IN2, LOW);
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
    digitalWrite(A_IN1, LOW);
    digitalWrite(A_IN2, LOW);
    ledcWrite(0, 0);
    digitalWrite(B_IN1, LOW);
    digitalWrite(B_IN2, LOW);
    Serial.println("⚠️ 指令逾時，自動停止");
  }

}
