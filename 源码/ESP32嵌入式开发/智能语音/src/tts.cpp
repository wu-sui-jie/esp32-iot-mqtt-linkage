#include "tts.h"
#include "my_config.h"

// 【注意】UTF8ToGB2312 这个库的头文件里带着一张几 KB 的查表数组和
// 一个 static 对象 GB，包含一次就多一份。所以整个工程【只在本文件
// 里包含它】，别的地方要用转换就调本文件的 tts_say()。
#include <UTF8ToGB2312.h>

// ============================================================
//  7 号板：智能语音播报模块
//
//  两件事在这里做：
//   1) 把 UTF-8 文本转成 GB2312，按语音模块的帧格式下发
//   2) 维护一个带优先级的播报队列，决定"先播哪句、要不要打断"
//
//  队列的作用见 readme 第四节 P6：火焰闪烁时同一句话会被反复上报，
//  不排队不限流喇叭就会卡在同一句上反复念；几块板同时触发时，
//  又必须让紧急的先念。所以规则是：高等级抢断低等级，
//  同等级排队，队列满了丢等级最低的。
// ============================================================

struct TtsItem
{
    char text[TTS_TEXT_MAX]; // UTF-8 原文
    uint8_t level;           // 播报等级
};

static TtsItem queue[TTS_QUEUE_LEN];
static int q_count = 0; // 队列里现有几条

static uint32_t busy_until_ms = 0; // 估算的"这一句念完"时刻
static uint8_t play_level = 255;   // 正在播的那条的等级（255 = 没在播）

static void play_now(const char *text, uint8_t level);

// ============================================================
//  算出一段 GB2312 文本在 max_bytes 以内、且切在字符边界上的长度
//
//  GB2312 里 ASCII 是单字节，汉字是双字节。直接按字节数硬截断
//  有可能把一个汉字劈成两半，模块念出来就是一个乱码字。
// ============================================================
static unsigned int gb2312_safe_len(const String &s, unsigned int max_bytes)
{
    unsigned int i = 0;
    unsigned int total = (unsigned int)s.length();

    while (i < total && i < max_bytes)
    {
        unsigned char c = (unsigned char)s[i];
        unsigned int step = (c < 0x80) ? 1 : 2; // ASCII 单字节，汉字双字节
        if (i + step > max_bytes)
            break; // 再放就超了，到此为止
        i += step;
    }
    return i;
}

// ============================================================
//  真正的下发：转码 → 组帧 → 写串口
// ============================================================
static void play_now(const char *text, uint8_t level)
{
    // 1) UTF-8 → GB2312。语音模块只认 GB2312，少这一步模块收到的
    //    是乱码，现象是"有声音但念的是杂音"，比完全不响更难查。
    String gb = GB.get(String(text));

    // 2) 超长截断，切在字符边界上
    unsigned int len = gb2312_safe_len(gb, TTS_MAX_BYTES);
    if (len < (unsigned int)gb.length())
        Serial.printf("[tts] 文本超长（%u 字节），已截断到 %u 字节\n",
                      (unsigned)gb.length(), len);

    // 3) 组帧：0xFD | 长度高字节 | 长度低字节 | 0x01 | 0x01 | GB2312 文本
    //    长度字段 = 文本字节数 + 2，填的是【字节数】不是字符数：
    //    一个汉字占 2 字节，"欢迎" 是 4 字节，长度字段写 4 + 2 = 6。
    static uint8_t buf[TTS_MAX_BYTES + 8];
    buf[0] = 0xFD;
    buf[1] = (uint8_t)((len + 2) >> 8);
    buf[2] = (uint8_t)((len + 2) & 0xFF);
    buf[3] = 0x01; // cmd
    buf[4] = 0x01; // para
    if (len > 0)
        memcpy(buf + 5, gb.c_str(), len);
    Serial2.write(buf, len + 5);

    // 4) 估算这一句要念多久。模块没有回传播放状态，只能按经验时长算；
    //    只影响"下一条什么时候开始"，估得不准不会出错，只是衔接松紧不同。
    uint32_t ms = TTS_BASE_MS + (uint32_t)len * TTS_MS_PER_BYTE;
    busy_until_ms = millis() + ms;
    play_level = level;

    Serial.printf("[tts] 播报（等级 %u，%u 字节，约 %u ms）：%s\n",
                  level, len, (unsigned)ms, text);
}

// ============================================================
//  排队：队列满了就把等级最低的那条换成新的
// ============================================================
static bool enqueue(const char *text, uint8_t level)
{
    if (q_count < TTS_QUEUE_LEN)
    {
        strncpy(queue[q_count].text, text, TTS_TEXT_MAX - 1);
        queue[q_count].text[TTS_TEXT_MAX - 1] = '\0';
        queue[q_count].level = level;
        q_count++;
        Serial.printf("[tts] 已入队（等级 %u），队列 %d 条\n", level, q_count);
        return true;
    }

    // 队列满：找等级最低的一条
    int worst = 0;
    for (int i = 1; i < TTS_QUEUE_LEN; i++)
        if (queue[i].level < queue[worst].level)
            worst = i;

    if (level > queue[worst].level)
    {
        uint8_t old = queue[worst].level;
        strncpy(queue[worst].text, text, TTS_TEXT_MAX - 1);
        queue[worst].text[TTS_TEXT_MAX - 1] = '\0';
        queue[worst].level = level;
        Serial.printf("[tts] 队列已满，丢弃等级 %u 的旧任务，换成等级 %u 的新任务\n",
                      old, level);
        return true;
    }

    Serial.printf("[tts] 队列已满且新任务等级 %u 不高于队内最低的 %u，直接丢弃\n",
                  level, queue[worst].level);
    return false;
}

// ============================================================
//  对外接口
// ============================================================
void tts_init()
{
    // 8E1：115200，8 位数据，偶校验，1 位停止位
    Serial2.begin(TTS_BAUD, SERIAL_8E1, TTS_RX_PIN, TTS_TX_PIN);
    Serial.printf("[tts] 语音模块串口就绪（RX=GPIO%d，TX=GPIO%d，%d 8E1）\n",
                  TTS_RX_PIN, TTS_TX_PIN, TTS_BAUD);

    // 开机自报一句，给现场演示一个明确起点
    tts_say("语音播报模块已启动，系统就绪", TTS_LEVEL_INFO);
}

bool tts_say(const char *text, uint8_t level)
{
    if (text == NULL || text[0] == '\0')
        return false;

    // 这里只入队，不下发：本函数会在 MQTT 回调链上被调用，
    // 串口下发统一留给 tts_loop()（readme 第五节 P2）
    return enqueue(text, level);
}

bool tts_busy()
{
    return (int32_t)(millis() - busy_until_ms) < 0;
}

void tts_loop()
{
    if (q_count == 0)
        return;

    // 挑出队列里等级最高的一条
    int best = 0;
    for (int i = 1; i < q_count; i++)
        if (queue[i].level > queue[best].level)
            best = i;

    if (tts_busy())
    {
        // 还在播：只有等级更高的才抢断（模块收到新帧会自动切换，
        // 不需要先发停止命令）。同等级或更低就让它把这一句念完。
        if (queue[best].level <= play_level)
            return;
        Serial.printf("[tts] 等级 %u 抢断正在播报的等级 %u\n",
                      queue[best].level, play_level);
    }

    // 从队列里摘出来（用最后一条填坑，顺序不重要，因为每次都挑最高的）
    TtsItem it;
    memcpy(&it, &queue[best], sizeof(TtsItem));
    memcpy(&queue[best], &queue[q_count - 1], sizeof(TtsItem));
    q_count--;

    play_now(it.text, it.level);
}
