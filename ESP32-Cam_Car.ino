#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-CAM-AP";
const char* password = "12345678";

WebServer server(80);



uint8_t speedVal = 60; // 初始速度（0~127）

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
      <input type="range" id="speed" min="0" max="127" value="60">
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

void setMotor0(int speed) {
  if (speed >= 0) {
    Serial.write(0x06); // Motor0 forward
    Serial.write(speed > 127 ? 127 : speed);
  } else {
    Serial.write(0x07); // Motor0 reverse
    Serial.write((-speed) > 127 ? 127 : -speed);
  }
}

void setMotor1(int speed) {
  if (speed >= 0) {
    Serial.write(0x08); // Motor1 forward
    Serial.write(speed > 127 ? 127 : speed);
  } else {
    Serial.write(0x09); // Motor1 reverse
    Serial.write((-speed) > 127 ? 127 : -speed);
  }
}

void stopMotors() {
  setMotor0(0);
  setMotor1(0);
}


unsigned long lastCommandTime = 0;
bool isMoving = false;
// 全域變數：記錄上一次狀態
bool lastForward = false;
bool lastBackward = false;
bool lastLeft = false;
bool lastRight = false;
int lastSpeed = 0;


void setup() {
  Serial.begin(9600);

  // 啟動 WiFi 熱點
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());

  // 初始化 qik
  Serial.write(0xAA); // 起始位元
  Serial.write(0x0A); // 裝置位址 (預設)
  
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

#if 0  
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
        setMotor1(-speedVal); // 馬達B 左轉
      } else if (right) {
        setMotor1(speedVal);  // 馬達B 右轉
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
}
