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
#include "project.h"
#include "command_function.h"

void speedometer(uint16_t rpm);
void speed(uint16_t speed);
void curr_gear(uint8_t gear);
void speedometer_init(void);
void speed_init(void);
void built_2_race(void);
void lap_time_init(void);
void lap_time(uint16_t min, uint16_t secs);
void gas_temp_init(void);
void gas_temp(uint16_t gas_lvl, uint16_t oil_temp);

int main(void)
{
    __enable_irq(); /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */

    UART_Start();
    
    int i[]    = { 0, 1500, 2000, 3500, 3000, 4000, 5500, 6000, 7500, 8000, 9500};
    int s[]    = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    int g[]    = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5 };
    int min[]  = { 0, 1, 2, 3, 4, 5, 4, 3, 2, 1, 0 };
    int secs[] = { 0, 1001, 2029, 3003, 4041, 5005, 4004, 3031, 2021, 1000, 0};
    int x = 0;
    
    send_clear_screen(0,0,0);
    CyDelay(180);
    speedometer_init();
    speed_init();
    built_2_race();
    lap_time_init();
    gas_temp_init();
    
    for(;;)
    {
        /* Place your application code here. */
        
        for( ;x < 11 ; x++) {
            speedometer(i[x]);
            
            speed(s[x]);
            
            curr_gear(g[x]);
            
            lap_time( min[x], secs[x]);
            CyDelay(10);
            gas_temp(s[x], s[x]);
        }
        
        for ( ; x > 0; x--) {
            speedometer(i[x]);
            
            speed(s[x]);
            
            curr_gear(g[x]);
            
            lap_time( min[x], secs[x]);
            CyDelay(10);
            gas_temp(s[x], s[x]);
            
        }
    }
}


void gas_temp_init(void) {
    
    send_set_font(0x00);
    send_set_text_color(255,255,255, 0,0,0);
    send_set_cursor(47, 228);
    send_print_string("GAS");
    CyDelay(5);
    
    send_set_font(0x00);
    send_set_text_color(255,255,255, 0,0,0);
    send_set_cursor(77, 228);
    send_print_string("EOT");
    CyDelay(5);
    
    send_set_font(0x00);
    send_set_text_color(255,255,255, 0,0,0);
    send_set_cursor(23, 286);
    send_print_string("1/2");
    CyDelay(5);
    
    send_draw_rect(48, 240, 19, 102, 255, 255, 255);
    CyDelay(5);
    send_draw_line(48, 265, 67, 267, 255, 255, 255);
    CyDelay(5);
    send_draw_line(48, 290, 67, 292, 255, 255, 255);
    CyDelay(5);
    send_draw_line(48, 315, 67, 317, 255, 255, 255);
    CyDelay(5);
    
    send_draw_rect(78, 240, 19, 102, 255, 255, 255);
    CyDelay(5);
    send_draw_line(78, 265, 97, 267, 255, 255, 255);
    CyDelay(5);
    send_draw_line(78, 290, 97, 292, 255, 255, 255);
    CyDelay(5);
    send_draw_line(78, 315, 97, 317, 255, 255, 255);
    CyDelay(5);
}

void gas_temp(uint16_t gas_lvl, uint16_t oil_temp) {
    
    int  gas_light = (gas_lvl <= 25)? 255 : 0;
    int  oil_hot   = (oil_temp >= 75)? 255 : 0;
    int  oil_cool  = (oil_temp < 75)? 255 : 0;
    
    int  oil_temp_lvl = oil_temp;
    
    send_fill_rect(51, 241, 13, 100 - gas_lvl, 0, 0, 0);
    CyDelay(10);
    send_fill_rect(81, 241, 13, 100 - oil_temp_lvl, 0, 0, 0);
    CyDelay(10);
    send_fill_rect(51, 341 - gas_lvl, 13, gas_lvl, gas_light, 255, 0);
    CyDelay(5);
    send_fill_rect(81, 341 - oil_temp_lvl, 13, oil_temp_lvl, oil_hot, oil_cool, 0);
    CyDelay(5);
}

void lap_time_init(void){
    send_draw_line(500, 0, 500, 20, 255, 255, 255);
    CyDelay(5);
    send_draw_line(500, 20, 639, 20, 255, 255, 255);
    CyDelay(5);
    send_set_font(0x01);
    send_set_text_color(255,255,255, 0,0,0);
    send_set_cursor(501, 1);
    send_print_string("00:00.00");
    CyDelay(20);
    
}
void lap_time(uint16_t min, uint16_t secs){
    char str[32];

    int sec   = secs / 100;   
    int m_sec = secs % 100;
    
    send_set_font(0x01);
    send_set_text_color(255,255,255, 0,0,0);
    send_set_cursor(501, 1);
    sprintf(str, "%02u:%02u.%02u", min, sec, m_sec);
        
    send_print_string(str);
    CyDelay(5);
    
}
void built_2_race(void){
        send_draw_round_rect(280, 2, 83, 15, 255, 255, 255);
        CyDelay(5);
        send_set_font(0x00); 
        send_set_text_color(255,0,0, 0, 0, 0); 
        send_set_cursor(281, 3); 
        send_print_string("BUILT2");
        CyDelay(10);
        send_set_font(0x00); 
        send_set_text_color(255,255,255, 0, 0, 0); 
        send_set_cursor(331, 3); 
        send_print_string("RACE");
        CyDelay(10);   
}
void curr_gear(uint8_t gear) {
        char str[8] ;
        
        send_set_font(0x01);
        send_set_text_color(255,255,255, 255,0,0);
        send_set_cursor(579, 310);
        sprintf(str, "%d", gear);
        
        send_print_string(str);
        CyDelay(5);
    
}

void speed_init(void) {
        send_draw_circle(599, 300, 40, 255, 255, 255);
        CyDelay(10);
        send_fill_circle(599, 300, 39, 128, 128, 128);
        CyDelay(10);
        send_set_font(0x00); 
        send_set_text_color(255,255,255, 128, 128, 128); 
        send_set_cursor(609, 310); 
        send_print_string("MPH");
        CyDelay(10);
        
        
}
void speed(uint16_t speed) {
        
        char str[8] ;
        
        send_set_font(0x01);
        send_set_text_color(255,255,255, 128,128,128);
        send_set_cursor(579, 290);
        sprintf(str, "%3u", speed);
        
        send_print_string(str);
        CyDelay(10);
}
void speedometer_init(void) {
        
        send_draw_circle(220, 340, 20, 0, 255, 0); // Left
        CyDelay(5);
        send_draw_circle(270, 340, 20, 0, 255, 0);
        CyDelay(5);
        send_draw_circle(320, 340, 20, 0, 255, 0); // Middle
        CyDelay(5);
        send_draw_circle(370, 340, 20, 255, 255, 0);
        CyDelay(5);
        send_draw_circle(420, 340, 20, 255, 0, 0); // Right
        CyDelay(5);
}

void speedometer(uint16_t rpm) {
     
    int g1_fill =     (rpm > 0)?  255 : 0;
    int g2_fill = (rpm >= 2000)? 255 : 0;
    int g3_fill = (rpm >= 4000)? 255 : 0;
    int y_fill =  (rpm >= 6000)? 255 : 0;
    int r_fill =  (rpm >= 8000)? 255 : 0;
    
    send_fill_circle(220, 340, 19, 0, g1_fill, 0); // Left
    CyDelay(5);
    send_fill_circle(270, 340, 19, 0, g2_fill, 0);
    CyDelay(5);
    send_fill_circle(320, 340, 19, 0, g3_fill, 0); // Middle
    CyDelay(5);
    send_fill_circle(370, 340, 19, y_fill, y_fill, 0);
    CyDelay(5);
    send_fill_circle(420, 340, 19, r_fill, 0, 0); // Right
    CyDelay(5);
    
}


/* [] END OF FILE */
