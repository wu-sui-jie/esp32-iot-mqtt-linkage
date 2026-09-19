
let mqttClient = null;
const HOST = "yunyismart.tech";
const PORT = 9001;
const TOPIC = "202609_PP2/15";

// 连接MQTT
function connectMQTT() {
    // 连接MQTT服务器
    const connectUrl = `ws://${HOST}:${PORT}`;
    mqttClient = mqtt.connect(connectUrl, {
        clientId: 'web_' + Math.random().toString(16).substr(2, 8),
        clean: true,
        reconnectPeriod: 1000
    });

    // 连接成功后
    mqttClient.on('connect', function () {
        document.getElementById('status').innerHTML = '已连接';
        document.getElementById('status').style.color = 'green';
        // 订阅主题
        mqttClient.subscribe(TOPIC);
    });

    // 收到消息处理
    mqttClient.on('message', function (topic, payload) {
        const msg = payload.toString();
        // 改变HTML页面中元素的值，此处是文本框的内容
        document.getElementById('msg').value = msg;

        // 解析JSON控制LED
        try {
            const data = JSON.parse(msg);
            if (data.device == "led1" && data.key == "control") {
                update_LED1(data.id, data.value == "on" ? 1 : 0);
            }
        } catch (e) { }
    });
}

// 控制LED函数 (id:0是全部，其余是某个设备id，cmd: 1开 0关)
function control_LED1(id, cmd) {
    if (!mqttClient || !mqttClient.connected) {
        alert('未连接服务器');
        return;
    }

    // 构造消息
    const message = {
        id: id,                    // 设备ID
        device: "led1",            // 设备类型
        key: "control",            // 操作类型
        value: cmd == 1 ? "on" : "off"  // 值：开或关
    };

    // 发布MQTT消息
    mqttClient.publish(TOPIC, JSON.stringify(message));
}

// 更新LED的界面显示
function update_LED1(id, cmd) {
    const ledElement = document.getElementById('led1');

    if (cmd == 1) {
        ledElement.classList.add('on');  // 开灯：添加on类
    } else {
        ledElement.classList.remove('on'); // 关灯：移除on类
    }
}

// 页面加载完成后的初始化
window.onload = function () {
    // 自动连接
    connectMQTT();
};