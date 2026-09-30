// ============================================================
//  物联网多平台联动系统 · Web 控制台
//  协议依据：《MQTT 通信协议规范_第1组》V2.0
//
//  通过 WebSocket 连接 MQTT 服务器，订阅 report 与 online 主题，
//  向 cmd 主题发送命令。报文格式为「信封 + 载荷」两层 JSON。
// ============================================================

let mqttClient = null;
let txSeq = 1; // 发送序号，从 1 开始自增

// ── 协议常量 ──
const HOST = "yunyismart.tech";
const PORT = 9001; // WebSocket 端口
const TOPIC_CMD    = "202609_PP2/1/cmd";
const TOPIC_REPORT = "202609_PP2/1/report";
const TOPIC_ONLINE = "202609_PP2/1/online";

// 板号 → 名称映射
const BOARD_NAMES = {
    "1": "舵机", "2": "风扇", "3": "红外对射", "4": "温湿度",
    "5": "RGB/蜂鸣器", "6": "火焰", "7": "语音播报", "8": "光照度"
};

// 各板在线状态缓存
const boardOnline = {};

// ============================================================
//  初始化：在线状态网格 + MQTT 连接
// ============================================================
window.onload = function () {
    initOnlineGrid();
    connectMQTT();
    // RGB 滑块实时预览
    ["rgbR", "rgbG", "rgbB"].forEach(id => {
        document.getElementById(id).addEventListener("input", updateRgbPreview);
    });
    // 舵机滑块实时显示角度
    document.getElementById("servoSlider").addEventListener("input", function () {
        document.getElementById("servoAngleText").innerText = this.value + "°";
        updateServoArm(parseInt(this.value));
    });
};

// 生成在线状态网格
function initOnlineGrid() {
    const grid = document.getElementById("onlineGrid");
    for (let i = 1; i <= 8; i++) {
        const key = String(i);
        boardOnline[key] = false;
        const el = document.createElement("div");
        el.className = "online-item offline";
        el.id = "online-" + i;
        el.innerHTML = '<span class="online-dot"></span>' + i + "号 " + BOARD_NAMES[key];
        grid.appendChild(el);
    }
}

// ============================================================
//  MQTT 连接
// ============================================================
function connectMQTT() {
    const url = "ws://" + HOST + ":" + PORT;
    mqttClient = mqtt.connect(url, {
        clientId: "web_" + Math.random().toString(16).substr(2, 8),
        clean: true,
        reconnectPeriod: 2000
    });

    mqttClient.on("connect", function () {
        document.getElementById("status").innerText = "已连接";
        document.getElementById("status").className = "status-online";
        // 订阅 report（传感器数据、事件、回执）和 online（上下线）
        mqttClient.subscribe(TOPIC_REPORT, { qos: 1 });
        mqttClient.subscribe(TOPIC_ONLINE, { qos: 1 });
        addLog("sys", "已连接 MQTT 服务器，已订阅 report 与 online");
    });

    mqttClient.on("offline", function () {
        document.getElementById("status").innerText = "已断开";
        document.getElementById("status").className = "status-offline";
        addLog("sys", "与 MQTT 服务器断开连接");
    });

    mqttClient.on("message", function (topic, payload) {
        const msg = payload.toString();
        document.getElementById("msg").value = msg.substring(0, 80) + (msg.length > 80 ? "..." : "");

        try {
            const data = JSON.parse(msg);
            handleReport(data, topic);
        } catch (e) {
            // 非 JSON 忽略
        }
    });
}

// ============================================================
//  收到报文分发：按 type 分流
// ============================================================
function handleReport(data, topic) {
    const ver = data.ver;
    const type = data.type;
    const src = String(data.src);

    if (ver !== "1.0") return;

    // online 主题：sys 报文处理上下线
    if (topic === TOPIC_ONLINE && type === "sys") {
        handleOnline(src, data.body);
        return;
    }

    // report 主题：按 type 分发
    if (type === "dat") {
        handleDat(src, data.body);
    } else if (type === "evt") {
        handleEvt(src, data.body);
    } else if (type === "ack") {
        handleAck(src, data.body);
    }

    addLog("recv", "[" + src + "号 " + type + "] " + JSON.stringify(data.body));
}

// ── 上下线处理 ──
function handleOnline(src, body) {
    if (!body || body.device !== "sys") return;
    const event = body.event;
    const el = document.getElementById("online-" + src);
    if (!el) return;

    if (event === "online") {
        boardOnline[src] = true;
        el.className = "online-item online";
    } else if (event === "offline") {
        boardOnline[src] = false;
        el.className = "online-item offline";
    }
}

// ── dat 周期数据处理 ──
function handleDat(src, body) {
    if (!body) return;
    const device = body.device;
    const samples = body.samples || [];

    if (src === "4" && device === "sht30") {
        // 温湿度：samples 里有 temperature 和 humidity
        let temp = null, humi = null;
        samples.forEach(s => {
            if (s.key === "temperature") temp = parseFloat(s.value);
            if (s.key === "humidity") humi = parseFloat(s.value);
        });
        if (temp !== null) updateSHT30(temp, humi);
    }
    else if (src === "8" && device === "light_sensor") {
        // 光照度
        samples.forEach(s => {
            if (s.key === "illuminance") updateLight(parseFloat(s.value));
        });
    }
    else if (src === "6" && device === "flame") {
        // 火焰强度
        samples.forEach(s => {
            if (s.key === "intensity") updateFlameIntensity(parseInt(s.value));
        });
    }
    else if (src === "3" && device === "ir_beam") {
        // 对射遮挡次数
        samples.forEach(s => {
            if (s.key === "blocked_count") {
                document.getElementById("irCount").innerText = s.value;
            }
        });
    }
}

// ── evt 事件处理 ──
function handleEvt(src, body) {
    if (!body) return;
    const device = body.device;
    const event = body.event;
    const level = body.level;

    if (src === "3" && device === "ir_beam") {
        updateIrBeam(event);
    }
    else if (src === "6" && device === "flame") {
        updateFlameEvent(event);
    }
    // 扩展联动事件（如 fan/auto_on, rgb/scene_fire 等）
    else if (src === "2" && device === "fan") {
        if (event === "auto_on") updateFan(true, "联动开启");
        else if (event === "auto_off") updateFan(false, "联动关闭");
    }
    else if (src === "5" && device === "rgb") {
        if (event === "scene_light") updateRgbFromLinkage(255, 200, 120, "光照照明");
        else if (event === "scene_fire") updateRgbFromLinkage(255, 0, 0, "火焰报警");
        else if (event === "scene_off") updateRgbFromLinkage(0, 0, 0, "已关闭");
    }
}

// ── ack 回执处理 ──
function handleAck(src, body) {
    if (!body) return;
    const device = body.device;
    const action = body.action;
    const result = body.result;
    const param = body.param || {};

    if (src === "1" && device === "servo" && action === "set_angle") {
        if (result === "ok") {
            updateServoStatus("已转到 " + param.angle + "°", true);
        } else {
            updateServoStatus("设定失败（参数非法）", false);
        }
    }
    else if (src === "2" && device === "fan" && action === "set_power") {
        if (result === "ok") {
            updateFan(param.on === true, "平台控制");
        }
    }
    else if (src === "5" && device === "rgb" && action === "set_color") {
        if (result === "ok" && param) {
            updateRgbPreviewColor(param.r, param.g, param.b);
        }
    }
    else if (src === "5" && device === "buzzer" && action === "set_power") {
        if (result === "ok") {
            document.getElementById("buzzerText").innerText = param.on ? "响" : "关";
        }
    }
    else if (src === "7" && device === "tts" && action === "play_text") {
        if (result === "ok") {
            document.getElementById("ttsStatus").innerText = "播报指令已发送";
        } else {
            document.getElementById("ttsStatus").innerText = "播报失败";
        }
    }
}

// ============================================================
//  命令发送（封装信封格式）
// ============================================================
function sendCmd(dst, device, action, param) {
    if (!mqttClient || !mqttClient.connected) {
        alert("未连接 MQTT 服务器");
        return;
    }

    const msg = {
        ver: "1.0",
        type: "cmd",
        seq: txSeq++,
        src: "web",
        dst: dst,
        body: {
            device: device,
            action: action
        }
    };
    if (param && Object.keys(param).length > 0) {
        msg.body.param = param;
    }
    if (txSeq > 65535) txSeq = 1;

    const payload = JSON.stringify(msg);
    mqttClient.publish(TOPIC_CMD, payload, { qos: 1 });
    addLog("send", "[web -> " + dst + "号] " + payload);
}

// ── 各设备命令 ──

// 1号板：舵机角度
function cmdServoAngle() {
    const angle = parseInt(document.getElementById("servoSlider").value);
    sendCmd(1, "servo", "set_angle", { angle: angle });
    document.getElementById("servoStatus").innerText = "正在转动到 " + angle + "°...";
}

// 2号板：风扇开关
function cmdFanPower(on) {
    sendCmd(2, "fan", "set_power", { on: on });
}

// 5号板：RGB 颜色
function cmdRgbColor() {
    const r = parseInt(document.getElementById("rgbR").value);
    const g = parseInt(document.getElementById("rgbG").value);
    const b = parseInt(document.getElementById("rgbB").value);
    sendCmd(5, "rgb", "set_color", { r: r, g: g, b: b });
}

// 5号板：蜂鸣器开关
function cmdBuzzerPower(on) {
    sendCmd(5, "buzzer", "set_power", { on: on });
}

// 7号板：语音播报
function cmdTtsPlay() {
    const text = document.getElementById("ttsText").value.trim();
    if (!text) {
        alert("请输入播报文本");
        return;
    }
    sendCmd(7, "tts", "play_text", { text: text });
    document.getElementById("ttsStatus").innerText = "正在发送...";
}

// ============================================================
//  UI 更新函数
// ============================================================

// ── 1号板：舵机 ──
function updateServoArm(angle) {
    // 角度映射到 CSS rotate：0° → -90deg, 90° → 0deg, 180° → 90deg
    const deg = angle - 90;
    document.getElementById("servoArm").style.transform = "rotate(" + deg + "deg)";
    document.getElementById("servoAngleText").innerText = angle + "°";
}

function updateServoStatus(text, ok) {
    const el = document.getElementById("servoStatus");
    el.innerText = text;
    el.style.color = ok ? "#27ae60" : "#e74c3c";
}

// ── 2号板：风扇 ──
function updateFan(on, reason) {
    const display = document.getElementById("fanDisplay");
    const text = document.getElementById("fanText");
    if (on) {
        display.classList.add("running");
        text.innerText = "转" + (reason ? "（" + reason + "）" : "");
    } else {
        display.classList.remove("running");
        text.innerText = "关" + (reason ? "（" + reason + "）" : "");
    }
}

// ── 3号板：红外对射 ──
function updateIrBeam(event) {
    const el = document.getElementById("irStatus");
    const timeEl = document.getElementById("irTime");
    if (event === "blocked") {
        el.innerText = "遮挡";
        el.classList.add("blocked");
    } else if (event === "clear") {
        el.innerText = "通畅";
        el.classList.remove("blocked");
    }
    timeEl.innerText = "更新: " + new Date().toLocaleTimeString();
}

// ── 4号板：温湿度 ──
function updateSHT30(temp, humi) {
    document.getElementById("tempVal").innerText = temp.toFixed(1);
    document.getElementById("humiVal").innerText = humi.toFixed(1);

    // 温度进度条：-10~50℃ 映射到 0~100%
    const tempPct = Math.max(0, Math.min(100, Math.round((temp + 10) / 60 * 100)));
    document.getElementById("tempFill").style.width = tempPct + "%";

    // 湿度进度条：0~100%
    document.getElementById("humiFill").style.width = Math.round(humi) + "%";

    document.getElementById("sht30Time").innerText = "更新: " + new Date().toLocaleTimeString();
}

// ── 5号板：RGB ──
function updateRgbPreview() {
    const r = parseInt(document.getElementById("rgbR").value);
    const g = parseInt(document.getElementById("rgbG").value);
    const b = parseInt(document.getElementById("rgbB").value);
    const el = document.getElementById("rgbPreview");
    const color = "rgb(" + r + "," + g + "," + b + ")";
    el.style.backgroundColor = color;
    if (r + g + b > 100) {
        el.style.boxShadow = "0 0 20px " + color;
    } else {
        el.style.boxShadow = "none";
    }
}

function updateRgbPreviewColor(r, g, b) {
    document.getElementById("rgbR").value = r;
    document.getElementById("rgbG").value = g;
    document.getElementById("rgbB").value = b;
    updateRgbPreview();
}

function updateRgbFromLinkage(r, g, b, reason) {
    updateRgbPreviewColor(r, g, b);
    addLog("sys", "5号板联动: " + reason + " → RGB(" + r + "," + g + "," + b + ")");
}

// ── 6号板：火焰 ──
function updateFlameEvent(event) {
    const icon = document.getElementById("flameIcon");
    const status = document.getElementById("flameStatus");
    if (event === "detected") {
        icon.classList.add("active");
        status.innerText = "检测到火焰!";
        status.classList.add("danger");
    } else if (event === "clear") {
        icon.classList.remove("active");
        status.innerText = "正常";
        status.classList.remove("danger");
    }
    document.getElementById("flameTime").innerText = "更新: " + new Date().toLocaleTimeString();
}

function updateFlameIntensity(val) {
    if (isNaN(val)) return;
    document.getElementById("flameVal").innerText = "强度: " + val;
    // 进度条：0~4095 映射到 0~100%
    const pct = Math.min(100, Math.round(val / 4095 * 100));
    document.getElementById("flameFill").style.width = pct + "%";
}

// ── 8号板：光照度 ──
function updateLight(lux) {
    if (isNaN(lux)) return;
    document.getElementById("lightVal").innerText = lux.toFixed(1);

    // 进度条：0~1000 lx
    const pct = Math.min(100, Math.round(lux / 10));
    document.getElementById("lightFill").style.width = pct + "%";

    // 光照等级
    let level, icon;
    if (lux < 50)       { level = "黑暗";     icon = "🌑"; }
    else if (lux < 120) { level = "较暗";     icon = "🌥"; }
    else if (lux < 500) { level = "光照适宜"; icon = "☀️"; }
    else                { level = "强光";     icon = "🔆"; }
    document.getElementById("lightLevel").innerText = level + "（" + pct + "%）";
    document.getElementById("lightIcon").innerText = icon;
}

// ============================================================
//  消息日志
// ============================================================
function addLog(type, text) {
    const box = document.getElementById("logBox");
    const line = document.createElement("div");
    line.className = "log-line";
    const time = new Date().toLocaleTimeString();
    const prefix = type === "send" ? "→ " : type === "recv" ? "← " : "★ ";
    line.classList.add(type === "send" ? "log-send" : type === "recv" ? "log-recv" : "log-sys");
    line.textContent = "[" + time + "] " + prefix + text;
    box.appendChild(line);
    box.scrollTop = box.scrollHeight;

    // 限制日志条数
    while (box.children.length > 200) {
        box.removeChild(box.firstChild);
    }
}

function clearLog() {
    document.getElementById("logBox").innerHTML = "";
}

// ============================================================
//  联动调试：模拟传感器报文
//
//  向 report 主题发布模拟的 evt / dat 报文，
//  各板的本地联动逻辑会旁听这些报文并作出响应。
//  这样无需真实传感器，就能在 Web 端触发板端联动效果。
// ============================================================

// 向 report 主题发布一条模拟报文
function sendReport(msg) {
    if (!mqttClient || !mqttClient.connected) {
        alert("未连接 MQTT 服务器");
        return;
    }
    const payload = JSON.stringify(msg);
    mqttClient.publish(TOPIC_REPORT, payload, { qos: 0 });
    addLog("send", "[联动模拟] " + payload);
}

// 生成模拟报文的基础信封
function buildReport(type, src, device) {
    return {
        ver: "1.0",
        type: type,
        seq: txSeq++,
        src: String(src),
        dst: 0,
        body: { device: device }
    };
}

// ── 联动1：模拟火焰事件（6号板 → 5号板 RGB+蜂鸣器）──
// 发送 evt detected/clear，5号板旁听后会触发声光报警
function simFlame(detected) {
    const msg = buildReport("evt", 6, "flame");
    msg.body.event = detected ? "detected" : "clear";
    msg.body.level = detected ? 3 : 0;
    sendReport(msg);

    // 同步更新 Web 界面上的火焰状态显示
    updateFlameEvent(msg.body.event);

    // 火焰检测时额外发一条强度 dat（模拟高读数），让界面更直观
    if (detected) {
        const dat = buildReport("dat", 6, "flame");
        dat.body.samples = [{ key: "intensity", value: 3800 }];
        sendReport(dat);
        updateFlameIntensity(3800);
    }

    addLog("sys", "联动模拟: 火焰" + (detected ? "检测到（触发RGB+蜂鸣器报警）" : "消失（解除报警）"));
}

// ── 联动2：模拟红外对射事件（3号板 → 1号板舵机）──
// 发送 evt blocked/clear，1号板旁听后会自动转动舵机
function simIrBeam(blocked) {
    const msg = buildReport("evt", 3, "ir_beam");
    msg.body.event = blocked ? "blocked" : "clear";
    msg.body.level = blocked ? 2 : 0;
    sendReport(msg);

    // 同步更新 Web 界面上的对射状态显示
    updateIrBeam(msg.body.event);

    addLog("sys", "联动模拟: 红外对射" + (blocked ? "遮挡（触发舵机转到0°）" : "恢复（舵机回到90°）"));
}

// ── 联动3：模拟温湿度数据（4号板 → 2号板风扇）──
// 发送 dat 报文，温度≥30或湿度≥80触发开风扇，回落到正常值触发关风扇
function simTempHumi(high) {
    const temp = high ? 35.2 : 24.5;
    const humi = high ? 85.0 : 55.0;

    const msg = buildReport("dat", 4, "sht30");
    msg.body.samples = [
        { key: "temperature", value: temp },
        { key: "humidity", value: humi }
    ];
    sendReport(msg);

    // 同步更新 Web 界面上的温湿度显示
    updateSHT30(temp, humi);

    addLog("sys", "联动模拟: 温湿度" + (high
        ? "超标（温度35.2℃/湿度85%，触发风扇开启）"
        : "正常（温度24.5℃/湿度55%，风扇将延时关闭）"));
}
