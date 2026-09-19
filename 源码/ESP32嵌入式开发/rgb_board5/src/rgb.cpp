#include "rgb.h"

#if HAS_RGB

#include <Adafruit_NeoPixel.h>

// WS2812B 灯带对象，引脚与灯珠数在 my_config.h 里配置
static Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);

// 当前颜色。只做纯色，不需要帧计数、不需要计时基准，
// 因此本模块没有 rgb_loop() —— 颜色设一次就保持。
static uint8_t cur_r = 0;
static uint8_t cur_g = 0;
static uint8_t cur_b = 0;

void rgb_init()
{
    strip.begin();
    // 本次不做亮度调节，保持默认满值即可。
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

uint8_t rgb_get_r() { return cur_r; }
uint8_t rgb_get_g() { return cur_g; }
uint8_t rgb_get_b() { return cur_b; }

#endif
