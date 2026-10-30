/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "command_function.h"
#include "project.h"



/* XOR checksum of payload array */
uint8_t ezGF_checksum(const uint8_t *payload, uint8_t len) {
    uint8_t chk = 0;
    for (uint8_t i = 0; i < len; i++) chk ^= payload[i];
    return chk;
}

void send_clear_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t payload[] = { 0x02, r, g, b };
    uint8_t frame[] = {
    0x55,
    MSPM0_MODE_SIZE(4),
    0x02, r, g, b,
    ezGF_checksum(payload, 4),
    0x0A
    };
    UART_PutArray(frame, sizeof(frame));
}

void send_draw_pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b) {
uint8_t frame[] = {
    0x55,
    MSPM0_MODE_SIZE(7),
    0x10,
    INT16_HI(x), INT16_LO(x),
    INT16_HI(y), INT16_LO(y),
    RGB565_HI(r,g,b), RGB565_LO(r,g,b),
    0x00, /* checksum placeholder */
    0x0A
};
    frame[9] = ezGF_checksum(&frame[2], 7);
    UART_PutArray(frame, sizeof(frame));
}

void send_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
    uint8_t r, uint8_t g, uint8_t b) {
        uint8_t pl[11] = {
        0x11,
        INT16_HI(x0), INT16_LO(x0),
        INT16_HI(y0), INT16_LO(y0),
        INT16_HI(x1), INT16_LO(x1),
        INT16_HI(y1), INT16_LO(y1),
        RGB565_HI(r,g,b), RGB565_LO(r,g,b)
};
    uint8_t frame[15] = { 0x55, MSPM0_MODE_SIZE(11) };
    for (int i=0;i<11;i++) frame[2+i] = pl[i];
        frame[13] = ezGF_checksum(pl, 11);
        frame[14] = 0x0A; /* Note: declared 14 elements, need 15 — use dynamic */
        UART_PutArray(frame, 15);
}
    
void send_vline(int16_t x, int16_t y,
        int16_t h, uint8_t r,
        uint8_t g, uint8_t b) {
            uint8_t pl[] = {
            0x12,
            INT16_HI(x), INT16_LO(x),
            INT16_HI(y), INT16_LO(y),
            INT16_HI(h), INT16_LO(h),
            RGB565_HI(r,g,b),
            RGB565_LO(r,g,b)
            };
        uint8_t frame[13];
        frame[0]=0x55;
        frame[1]=MSPM0_MODE_SIZE(9);
        for(int i=0;i<9;i++) frame[2+i]=pl[i];
        frame[11]=ezGF_checksum(pl,9);
        frame[12]=0x0A;
        UART_PutArray(frame,13);
}
        
void send_hline(int16_t x, int16_t y,
                int16_t w, uint8_t r,
                uint8_t g, uint8_t b) {
    uint8_t pl[] = {
        0x13,
        INT16_HI(x), INT16_LO(x),
        INT16_HI(y), INT16_LO(y),
        INT16_HI(w), INT16_LO(w),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)
    };
    uint8_t frame[13];
    frame[0]=0x55;
    frame[1]=MSPM0_MODE_SIZE(9);
    for(int i=0;i<9;i++) frame[2+i]=pl[i];
    frame[11]=ezGF_checksum(pl,9);
    frame[12]=0x0A;
    UART_PutArray(frame,13);
}
                
void send_fill_screen(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t pl[] = { 0x14, RGB565_HI(r,g,b), RGB565_LO(r,g,b) };
    uint8_t frame[] = {
    0x55, MSPM0_MODE_SIZE(3),
    pl[0], pl[1], pl[2],
    ezGF_checksum(pl, 3),
    0x0A
    };
   UART_PutArray(frame, sizeof(frame));
}

void send_draw_rect(int16_t x, int16_t y,
                    int16_t w, int16_t h,
                    uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x20,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        INT16_HI(w),INT16_LO(w),
        INT16_HI(h),INT16_LO(h),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)};
    uint8_t fr[15];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(11);
    for(int i=0;i<11;i++) fr[2+i]=pl[i];
    fr[13]=ezGF_checksum(pl,11);
    fr[14]=0x0A;
    UART_PutArray(fr, 15);
    
}
                    
void send_fill_rect(int16_t x, int16_t y,
                    int16_t w, int16_t h,
                    uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x21,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        INT16_HI(w),INT16_LO(w),
        INT16_HI(h),INT16_LO(h),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)};
    uint8_t fr[15];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(11);
    for(int i=0;i<11;i++) fr[2+i]=pl[i];
    fr[13]=ezGF_checksum(pl,11);
    fr[14]=0x0A;
    UART_PutArray(fr,15);
    }
                    
void send_draw_round_rect(
                        int16_t x, int16_t y,
                        int16_t w, int16_t h,
                        uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x22,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        INT16_HI(w),INT16_LO(w),
        INT16_HI(h),INT16_LO(h),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)};
    uint8_t fr[15];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(11);
    for(int i=0;i<11;i++) fr[2+i]=pl[i];
    fr[13]=ezGF_checksum(pl,11);
    fr[14]=0x0A;
    UART_PutArray(fr,15);
}
     
void send_fill_round_rect(
                            int16_t x, int16_t y,
                            int16_t w, int16_t h,
                            uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x23,
                INT16_HI(x),INT16_LO(x),
                INT16_HI(y),INT16_LO(y),
                INT16_HI(w),INT16_LO(w),
                INT16_HI(h),INT16_LO(h),
                RGB565_HI(r,g,b),
                RGB565_LO(r,g,b)};
    uint8_t fr[15];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(11);
    for(int i=0;i<11;i++) fr[2+i]=pl[i];
    fr[13]=ezGF_checksum(pl,11);
    fr[14]=0x0A;
    UART_PutArray(fr,15);
}
                        
void send_draw_circle(int16_t x, int16_t y,
                        int16_t rad,
                        uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x30,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        INT16_HI(rad),INT16_LO(rad),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)};
    uint8_t fr[13];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(9);
    for(int i=0;i<9;i++) fr[2+i]=pl[i];
    fr[11]=ezGF_checksum(pl,9);
    fr[12]=0x0A;
    UART_PutArray(fr,13);
}
                        
void send_fill_circle(int16_t x, int16_t y,
                        int16_t rad,
                        uint8_t r,uint8_t g,uint8_t b){
    uint8_t pl[]={0x31,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        INT16_HI(rad),INT16_LO(rad),
        RGB565_HI(r,g,b),
        RGB565_LO(r,g,b)};
    uint8_t fr[13];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(9);
    for(int i=0;i<9;i++) fr[2+i]=pl[i];
    fr[11]=ezGF_checksum(pl,9);
    fr[12]=0x0A;
    UART_PutArray(fr,13);
}

static void send_triangle_cmd(uint8_t sub_cmd,
                            int16_t x0,int16_t y0, int16_t x1,int16_t y1,
                            int16_t x2,int16_t y2,
                            uint8_t r, uint8_t g, uint8_t b) {
    uint8_t pl[15] = {
        sub_cmd,
        INT16_HI(x0),INT16_LO(x0),
        INT16_HI(y0),INT16_LO(y0),
        INT16_HI(x1),INT16_LO(x1),
        INT16_HI(y1),INT16_LO(y1),
        INT16_HI(x2),INT16_LO(x2),
        INT16_HI(y2),INT16_LO(y2),
        RGB565_HI(r,g,b), RGB565_LO(r,g,b)
        };
    uint8_t fr[19];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(15);
    for(int i=0;i<15;i++) fr[2+i]=pl[i];
    fr[17]=ezGF_checksum(pl,15);
    fr[18]=0x0A;
    UART_PutArray(fr,19);
}

void send_set_font(uint8_t font_id){
/* font_id: 0=Small, 1=Big */
    uint8_t pl[]={0x52,font_id};
    uint8_t fr[]={
        0x55,MSPM0_MODE_SIZE(2),
        0x52,font_id,
        ezGF_checksum(pl,2),
        0x0A};
        UART_PutArray(fr,sizeof(fr));
}         

void send_set_text_color(
                        uint8_t fr,uint8_t fg,uint8_t fb,
                        uint8_t br,uint8_t bg,uint8_t bb){
    uint8_t pl[]={0x53,
        RGB565_HI(fr,fg,fb),
        RGB565_LO(fr,fg,fb),
        RGB565_HI(br,bg,bb),
        RGB565_LO(br,bg,bb)};
    uint8_t frame[9];
    frame[0]=0x55;
    frame[1]=MSPM0_MODE_SIZE(5);
    for(int i=0;i<5;i++) frame[2+i]=pl[i];
    frame[7]=ezGF_checksum(pl,5);
    frame[8]=0x0A;
    UART_PutArray(frame,9);
}

void send_set_cursor(int16_t cx,int16_t cy){
    uint8_t pl[]={0x51,
        INT16_HI(cx),INT16_LO(cx),
        INT16_HI(cy),INT16_LO(cy)};
    uint8_t fr[9];
    fr[0]=0x55; fr[1]=MSPM0_MODE_SIZE(5);
    for(int i=0;i<5;i++) fr[2+i]=pl[i];
    fr[7]=ezGF_checksum(pl,5);
    fr[8]=0x0A;
    UART_PutArray(fr,9);
}

void send_draw_char(int16_t x,int16_t y,
                    char c,
                    uint8_t fr,uint8_t fg,uint8_t fb,
                    uint8_t br,uint8_t bg,uint8_t bb){
    uint8_t pl[]={0x50,
        INT16_HI(x),INT16_LO(x),
        INT16_HI(y),INT16_LO(y),
        (uint8_t)c,
        RGB565_HI(fr,fg,fb),
        RGB565_LO(fr,fg,fb),
        RGB565_HI(br,bg,bb),
        RGB565_LO(br,bg,bb)};
    uint8_t frame[14];
    frame[0]=0x55; frame[1]=MSPM0_MODE_SIZE(10);
    for(int i=0;i<10;i++) frame[2+i]=pl[i];
    frame[12]=ezGF_checksum(pl,10);
    frame[13]=0x0A;
    UART_PutArray(frame,14);
}
      
void send_print_string(const char *msg) {
    uint8_t str_len = (uint8_t)strlen(msg);
    uint8_t pl_len = 2 + str_len; /* sub_cmd + len byte + string */
    uint8_t frame[256];
    uint8_t i = 0;
    frame[i++] = 0x55;
    frame[i++] = MSPM0_MODE_SIZE(pl_len);
    /* Payload */
    frame[i++] = 0x54; /* sub-command */
    frame[i++] = str_len; /* string length */
    for (uint8_t k = 0; k < str_len; k++)
    frame[i++] = (uint8_t)msg[k]; /* string bytes */
    frame[i++] = ezGF_checksum(&frame[2], pl_len); /* XOR checksum */
    frame[i++] = 0x0A; /* delimiter */
    UART_PutArray(frame, i);
}

/* [] END OF FILE */
