#include "rgb.h"

#if HAS_RGB

#include <Adafruit_NeoPixel.h>

// WS2812B 灯带对象，引脚与灯珠数在 my_config.h 里配置
static Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

// 当前颜色。纯色模式下由 rgb_apply 设定后保持不变，
// 火焰循环模式下由 rgb_loop 逐帧改写。
static uint8_t cur_r = 0;
static uint8_t cur_g = 0;
static uint8_t cur_b = 0;

// ============================================================
//  火焰循环动画
//
//  四个色相走一个来回：红 → 橙 → 黄 → 橙 → 红 …
//  不用彩虹那样扫过全部色相，因为"火"的语义就是红黄之间，
//  颜色跑偏了（出现绿色、紫色）反而不像火警。
//
//  帧推进放在 rgb_loop() 里做，不在回调里做：
//  主循环要保持轻快，动画只是每隔 FIRE_STEP_MS 改一次颜色，
//  不阻塞、不 delay。
// ============================================================
static const uint8_t FIRE_COLORS[][3] = {
    {255, 0, 0},   // 红
    {255, 80, 0},  // 橙
    {255, 180, 0}, // 黄
    {255, 80, 0},  // 橙
};
static const uint8_t FIRE_STEPS = sizeof(FIRE_COLORS) / sizeof(FIRE_COLORS[0]);

static bool fire_active = false;
static uint8_t fire_step = 0;
static unsigned long last_fire_ms = 0;

void rgb_init()
{
    strip.begin();
    // 不做亮度调节，保持默认满值。
    // 这样也不存在"回发逻辑颜色还是压暗后颜色"的歧义。
    strip.setBrightness(255);

    rgb_apply(0, 0, 0); // 上电先全灭
}

void rgb_apply(uint8_t r, uint8_t g, uint8_t b)
{
    cur_r = r;
    cur_g = g;
    cur_b = b;

    for (uint16_t i = 0; i < strip.numPixels(); i++)
        strip.setPixelColor(i, strip.Color(r, g, b));
    strip.show();

    Serial.printf("[rgb] 显示颜色 r=%u g=%u b=%u%s\n",
                  r, g, b, (r == 0 && g == 0 && b == 0) ? "（熄灭）" : "");
}

void rgb_set_fire(bool on)
{
    if (on == fire_active)
        return; // 状态没变，不打断正在走的动画

    fire_active = on;

    if (on)
    {
        fire_step = 0;
        last_fire_ms = 0; // 置 0 表示"下一轮立刻显示第一帧"
        Serial.println("[rgb] 进入火焰报警循环");
    }
    else
    {
        Serial.println("[rgb] 退出火焰报警循环");
    }
}

bool rgb_fire_active()
{
    return fire_active;
}

void rgb_loop()
{
    if (!fire_active)
        return;

    unsigned long now = millis();
    // last_fire_ms == 0 是"刚进入火焰模式、还没画第一帧"的标记，
    // 必须立刻画一帧，否则要等满 FIRE_STEP_MS 才亮，看起来像没反应。
    if (last_fire_ms != 0 && now - last_fire_ms < FIRE_STEP_MS)
        return;
    last_fire_ms = now;

    const uint8_t *c = FIRE_COLORS[fire_step];
    rgb_apply(c[0], c[1], c[2]);

    fire_step = (uint8_t)((fire_step + 1) % FIRE_STEPS);
}

uint8_t rgb_get_r() { return cur_r; }
uint8_t rgb_get_g() { return cur_g; }
uint8_t rgb_get_b() { return cur_b; }

#endif
