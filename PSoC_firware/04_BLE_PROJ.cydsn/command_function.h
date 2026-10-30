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
#ifndef _COMMAND_FUNCTION_H
#define _COMMAND_FUNCTION_H
    
#include "project.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
    
    /* Encode int16_t into 2 payload bytes (big-endian) */
#define INT16_HI(v) (((uint16_t)(v) >> 8) & 0xFF)
#define INT16_LO(v) ( (uint16_t)(v) & 0xFF)
    
/* Convert RGB888 to RGB565 */
#define RGB565(r,g,b) (((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | ((b) >> 3))
#define RGB565_HI(r,g,b) ((RGB565(r,g,b) >> 8) & 0xFF)
#define RGB565_LO(r,g,b) ( RGB565(r,g,b) & 0xFF)
    
/* Build mode+size byte for command_mode = 0x02 */
#define MSPM0_MODE_SIZE(payload_len) (0x02u | (((payload_len) & 0x1Fu) << 2))
    

    
    void send_clear_screen (uint8_t r, uint8_t g, uint8_t b);
    void send_draw_pixel (int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
    void send_draw_line (int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t r, uint8_t g, uint8_t b);
    void send_vline (int16_t x, int16_t y, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void send_hline (int16_t x, int16_t y, int16_t w, uint8_t r, uint8_t g, uint8_t b);
    void send_fill_screen (uint8_t r, uint8_t g, uint8_t b);
    void send_draw_rect (int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void send_fill_rect (int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void send_fill_round_rect ( int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r,uint8_t g,uint8_t b);
    void send_draw_round_rect ( int16_t x, int16_t y, int16_t w, int16_t h, uint8_t r, uint8_t g, uint8_t b);
    void send_draw_circle (int16_t x, int16_t y, int16_t rad, uint8_t r,uint8_t g,uint8_t b);
    void send_fill_circle(int16_t x, int16_t y, int16_t rad, uint8_t r,uint8_t g,uint8_t b);
    static void send_triangle_cmd(uint8_t sub_cmd, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b);
    void send_set_font(uint8_t font_id);
    void send_set_text_color( uint8_t fr,uint8_t fg,uint8_t fb, uint8_t br,uint8_t bg,uint8_t bb);
    void send_set_cursor(int16_t cx,int16_t cy);
    void send_draw_char(int16_t x,int16_t y, char c, uint8_t fr,uint8_t fg,uint8_t fb, uint8_t br,uint8_t bg,uint8_t bb);
    void send_print_string(const char *msg);
    uint8_t ezGF_checksum(const uint8_t *payload, uint8_t len);
    
#endif
/* [] END OF FILE */
