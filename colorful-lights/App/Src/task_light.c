#include "task_light.h"

#define LightMode_Normal 0x01

void LightTaskLoop(void)
{
    uint8_t command[60];

    uint8_t length = Command_GetCommand(command);
    if (length < 4)
    {
        return;
    }

    if (command[2] == LightMode_Normal)
    {
        /* 帧结构：[0]=0xAA [1]=总长度 [2]=模式 [3..] 每颗灯 RGB 三元组 [末]=校验和 */
        uint8_t colorCount = (uint8_t)((length - 4) / 3);
        for (uint8_t i = 0; i < LED_COUNT; i++)
        {
            /* 帧里给了第 i 颗灯的颜色就用它，不够就用第 1 颗的颜色兜底 */
            uint8_t offset = 3 + 3 * ((colorCount > 0 && i < colorCount) ? i : 0);
            WS2812_Set(i, command[offset], command[offset + 1], command[offset + 2]);
        }
        WS2812_Update();
    }
}
