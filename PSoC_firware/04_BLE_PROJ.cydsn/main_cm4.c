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
#include "projector_function.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/* ---- GATT handles from BLE_config.h ---- */
#define RX_HANDLE       CY_BLE_UART_SERVICE_RX_CHAR_HANDLE
#define TX_HANDLE       CY_BLE_UART_SERVICE_TX_CHAR_HANDLE
#define TX_CCCD_HANDLE  CY_BLE_UART_SERVICE_TX_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE

/* ---- Settings ---- */
#define MAX_PAYLOAD      (20u)
#define RX_LINE_BUF_SZ   (128u)

/* ---- Connection / notify state ---- */
static cy_stc_ble_conn_handle_t g_connHandle;
static uint8_t g_isConnected = 0u;
static uint8_t notifyEnabled = 0u;
static uint8_t connectEventSeen = 0u;
static uint8_t displayInitPending = 0u;

/* ---- RX buffer state ---- */
static uint8_t LINE_BUFFER[RX_LINE_BUF_SZ];
static uint16_t LINE_INDEX = 0u;
static uint8_t PACKET_ACTIVE = 0u;

/* ---- TX queue ---- */
static volatile uint8_t txPending = 0u;
static uint8_t txBuf[MAX_PAYLOAD];
static uint8_t txLen = 0u;

/* ---- Parsed data globals ---- */
int RPM      = 0;
int FUEL     = 0;
int SPEED    = 0;
int OIL_TEMP = 0;
int LAP_TIME = 0;
int GEAR     = 0;

/* Queue up to 20 bytes to send as a notification later (from main loop) */
static void QueueTxString(const char *s)
{
    if(s == NULL) return;

    uint8_t i = 0u;
    while(s[i] != '\0' && i < (uint8_t)sizeof(txBuf))
    {
        txBuf[i] = (uint8_t)s[i];
        i++;
    }
    txLen = i;
    txPending = 1u;
}

/* Send one TX notification (call ONLY from main loop) */
static void SendTxNotification(const uint8_t *data, uint16_t len)
{
    if(!g_isConnected || !notifyEnabled || data == NULL || len == 0u)
        return;

    if(len > MAX_PAYLOAD)
        len = MAX_PAYLOAD;

    cy_stc_ble_gatts_handle_value_ntf_t ntf;
    ntf.connHandle               = g_connHandle;
    ntf.handleValPair.attrHandle = TX_HANDLE;
    ntf.handleValPair.value.val  = (uint8_t*)data;
    ntf.handleValPair.value.len  = len;

    (void)Cy_BLE_GATTS_Notification(&ntf);
}

/* Parse telemetry packet after full packet has been received.
   lineBuffer contents should look like:
   "2000:50:50:50:4:100000"
*/
static void ParseTelemetryPacket(void)
{
    char *data;
    char *chunk = (char*)LINE_BUFFER;

    data = strtok(chunk, ":");
    RPM = atoi(data);

    data = strtok(NULL, ":");
    SPEED = atoi(data);

    data = strtok(NULL, ":");
    FUEL = atoi(data);

    data = strtok(NULL, ":");
    OIL_TEMP = atoi(data);

    data = strtok(NULL, ":");
    GEAR = atoi(data);

    data = strtok(NULL, ":");
    LAP_TIME = atoi(data);
    
    speedometer(RPM);
    speed(SPEED);
    curr_gear(GEAR);

    lap_time(LAP_TIME / 10000, LAP_TIME % 10000);
    CyDelay(10);
    gas_temp(FUEL, OIL_TEMP);
    
    QueueTxString("DATA OK\n");
}

/* Process incoming BLE data, including chunked telemetry packets */
static void ProcessIncomingRx(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if(data == NULL || len == 0u)
        return;

    for(i = 0u; i < len; i++){
        
        uint8_t telemetry = data[i];
        if(telemetry == '#'){ // Start
            PACKET_ACTIVE = 1u;
            LINE_INDEX = 0u;
            continue;
        }
        if(PACKET_ACTIVE){
            if(telemetry== ';'){ // End
                LINE_BUFFER[LINE_INDEX] = '\0';
                ParseTelemetryPacket();
                PACKET_ACTIVE = 0u;
                LINE_INDEX = 0u;
                continue;
            }
            if(LINE_INDEX < (RX_LINE_BUF_SZ - 1u)){
                LINE_BUFFER[LINE_INDEX++] = telemetry;
            }
            else{
                QueueTxString("ERR OVF\n");
                PACKET_ACTIVE = 0u;
                LINE_INDEX = 0u;
            }

            continue;
        }
    }
}

/* ---- BLE event handler ---- */
void BleEventHandler(uint32_t event, void *eventParam)
{
    switch(event)
    {
        case CY_BLE_EVT_STACK_ON:
            g_isConnected = 0u;
            notifyEnabled = 0u;
            txPending     = 0u;
            PACKET_ACTIVE  = 0u;
            LINE_INDEX     = 0u;

            (void)Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST, 0u);
            break;

        case CY_BLE_EVT_GAP_DEVICE_DISCONNECTED:
            g_isConnected = 0u;
            notifyEnabled = 0u;
            txPending     = 0u;
            PACKET_ACTIVE  = 0u;
            LINE_INDEX     = 0u;

            (void)Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST, 0u);
            break;

        case CY_BLE_EVT_GAP_DEVICE_CONNECTED:
            
            g_isConnected = 1u;
            notifyEnabled = 0u;
            PACKET_ACTIVE  = 0u;
            LINE_INDEX     = 0u;
            
            
            
            break;

        case CY_BLE_EVT_GATTS_WRITE_REQ:
        case CY_BLE_EVT_GATTS_WRITE_CMD_REQ:
        {
            if(eventParam == NULL)
            {
                if(event == CY_BLE_EVT_GATTS_WRITE_REQ)
                {
                    (void)Cy_BLE_GATTS_WriteRsp(g_connHandle);
                }
                break;
            }

            cy_stc_ble_gatts_write_cmd_req_param_t *wr =
                (cy_stc_ble_gatts_write_cmd_req_param_t*)eventParam;

            g_connHandle  = wr->connHandle;
            g_isConnected = 1u;

            if(wr->handleValPair.attrHandle == TX_CCCD_HANDLE && wr->handleValPair.value.len >= 2u)
            {
                uint16_t cccd = (uint16_t)wr->handleValPair.value.val[0] |
                               ((uint16_t)wr->handleValPair.value.val[1] << 8);
                notifyEnabled = (cccd & 0x0001u) ? 1u : 0u;

                if(notifyEnabled){
                    connectEventSeen = 1u;
                    displayInitPending = 1u;
                    QueueTxString("NOTIFY ON\n");
                    
                }else {
                    QueueTxString("NOTIFY OFF\n");
                }
            }

            if(wr->handleValPair.attrHandle == RX_HANDLE)
            {
                if(wr->handleValPair.value.val != NULL && wr->handleValPair.value.len > 0u)
                {
                    ProcessIncomingRx(wr->handleValPair.value.val,
                                      wr->handleValPair.value.len);
                }
            }

            if(event == CY_BLE_EVT_GATTS_WRITE_REQ)
            {
                (void)Cy_BLE_GATTS_WriteRsp(wr->connHandle);
            }
            break;
        }

        default:
            break;
    }
}

int main(void)
{
    __enable_irq();

    UART_Start();
    
    Cy_GPIO_Write(GPIO_PRT7, 1u, 1u);

    Cy_BLE_Start(BleEventHandler);

    for(;;)
    {
        Cy_BLE_ProcessEvents();
        
        if(connectEventSeen)
        {
            connectEventSeen = 0u;

            /* turn green LED on */
            Cy_GPIO_Write(GPIO_PRT7, 1u, 0u);
        }
        
        if(displayInitPending)
        {
            displayInitPending = 0u;

            send_clear_screen(0,0,0);
            CyDelay(180);
            speedometer_init();
            speed_init();
            built_2_race();
            lap_time_init();
            gas_temp_init();
        }

        if(txPending && g_isConnected && notifyEnabled)
        {
            txPending = 0u;
            SendTxNotification(txBuf, txLen);
        }
        

        
        
        
    }
}

/* [] END OF FILE */