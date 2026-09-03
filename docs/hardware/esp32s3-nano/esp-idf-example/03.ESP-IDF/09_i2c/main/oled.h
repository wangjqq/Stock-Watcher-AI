
#ifndef __OLED_H
#define __OLED_H 

#include "stdlib.h" 

#ifndef u8
#define u8 __uint8_t
#endif

#ifndef u16
#define u16 __uint16_t
#endif

#ifndef u32
#define u32 __uint32_t
#endif


#define OLED_CMD  0 //写命令
#define OLED_DATA 1 //写数据


 void OLED_WR_Byte(u8 dat,u8 mode);
void OLED_Refresh(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x,u8 y,u8 t);

void OLED_Init(void);

#endif
