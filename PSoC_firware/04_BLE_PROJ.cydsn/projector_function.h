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

#ifndef _PROJECTOR_FUNCTION_H
#define _PROJECTOR_FUNCTION_H
    
#include "project.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
    
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
    
#endif

/* [] END OF FILE */
