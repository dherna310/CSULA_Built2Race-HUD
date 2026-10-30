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
#include "string.h"
#include "ctype.h"
#include "stdint.h"
#include "stdlib.h"

/* ---- GATT handles from BLE_config.h ---- */
#define RX_HANDLE       CY_BLE_UART_SERVICE_RX_CHAR_HANDLE
#define TX_HANDLE       CY_BLE_UART_SERVICE_TX_CHAR_HANDLE
#define TX_CCCD_HANDLE  CY_BLE_UART_SERVICE_TX_CLIENT_CHARACTERISTIC_CONFIGURATION_DESC_HANDLE

/* ---- Settings ---- */
#define MAX_PAYLOAD      (20u)     /* default safe ATT payload */
#define RX_LINE_BUF_SZ   (128u)

/* ---- Connection / notify state ---- */
static cy_stc_ble_conn_handle_t g_connHandle;
static uint8_t g_isConnected = 0u;
static uint8_t notifyEnabled = 0u;

/* ---- RX command buffer ---- */
static uint8_t lineBuffer[RX_LINE_BUF_SZ];
static uint16_t lineIndex = 0u;

/* ---- TX queue (IMPORTANT: we do NOT notify inside event handler) ---- */
static volatile uint8_t txPending = 0u;
static uint8_t txBuf[MAX_PAYLOAD];
static uint8_t txLen = 0u;

/* -------- LED control (active-low) --------
 * Best: create a Pin component named LED_GREEN mapped to P7[1] output strong drive.
 * Fallback below directly drives P7[1] if LED_GREEN_Write() is not generated.
 */

/* ---- Helpers ---- */

static int str_equal_ci(const char *a, const char *b)
{
    while(*a && *b)
    {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

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

/* Parse incoming bytes into newline-terminated commands */
static void ProcessIncomingRx(const uint8_t *data, uint16_t len)
{
    /* If the incoming chunk contains CR/LF, use line-buffer mode */
    for(uint16_t i = 0; i < len; i++)
    {
        uint8_t c = data[i];

        if(c == '\r' || c == '\n')
        {
            /* finalize */
            lineBuffer[lineIndex] = '\0';
            break;
        }

        if(lineIndex < (RX_LINE_BUF_SZ - 1u))
        {
            lineBuffer[lineIndex++] = c;
        }
    }

    /* If we didn't see CR/LF, assume this write is the full command */
    if((len > 0u) && (data[len - 1u] != '\r') && (data[len - 1u] != '\n'))
    {
        lineBuffer[lineIndex] = '\0';
    }

    /* If we still have nothing, do nothing */
    if(lineIndex == 0u)
        return;

    /* Compare commands */
    
    /*
    if(str_equal_ci((char*)lineBuffer, "GREEN ON"))
    {
        Cy_GPIO_Write(GPIO_PRT7, 1u, 0u);          // ON (active-low) 
        QueueTxString("GREEN ON\n");
        lineIndex = 0u;
    }
    else if(str_equal_ci((char*)lineBuffer, "GREEN OFF") || str_equal_ci((char*)lineBuffer, "GREENOFF"))
    {
        Cy_GPIO_Write(GPIO_PRT7, 1u, 1u);              // OFF 
        QueueTxString("GREEN OFF\n");
        lineIndex = 0u;
    } else if(str_equal_ci((char*)lineBuffer, "RED ON") || str_equal_ci((char*)lineBuffer, "REDON"))
    {
        Cy_GPIO_Write(GPIO_PRT6, 3u, 0u);              // OFF 
        QueueTxString("RED ON\n");
        lineIndex = 0u;
    }
    else if (str_equal_ci((char*)lineBuffer, "RED OFF") || str_equal_ci((char*)lineBuffer, "REDOFF"))
    {
        Cy_GPIO_Write(GPIO_PRT6, 3u, 1u);              // OFF 
        QueueTxString("RED OFF\n");
        lineIndex = 0u;
    } else
    {
        QueueTxString("ERR Unknown cmd\n");
        lineIndex = 0u;
    }
    */
    char *value;
    char *data = (char*)lineBuffer;

// Make sure string ends before ';'
        char *end = strchr(data, ';');
        if (end != NULL)
        {
            *end = '\0';  // terminate string at ';'
        }

        // Skip the '#' character
        if (data[0] == '#')
        {
            data++;
        }

        // Parse tokens
        data = strtok(data, ":");
        if (value != NULL)
        {
            int rpm = atoi(value);

            value = strtok(NULL, ":");
            int speed = value ? atoi(value) : 0;

            value = strtok(NULL, ":");
            int fuel = value ? atoi(value) : 0;

            value = strtok(NULL, ":");
            int oil_temp = value ? atoi(value) : 0;

            value = strtok(NULL, ":");
            int gear = value ? atoi(value) : 0;

            value = strtok(NULL, ":");
            int lap_time = value ? atoi(value) : 0;

        // Now you have all values
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

            /* Start advertising (must be enabled in BLE GAP settings) */
            (void)Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST, 0u);
            break;

        case CY_BLE_EVT_GAP_DEVICE_DISCONNECTED:
            g_isConnected = 0u;
            notifyEnabled = 0u;
            txPending     = 0u;

            /* Restart advertising so it comes back after disconnect */
            (void)Cy_BLE_GAPP_StartAdvertisement(CY_BLE_ADVERTISING_FAST, 0u);
            break;

        case CY_BLE_EVT_GAP_DEVICE_CONNECTED:
            /* Mark connected; we also capture connHandle from write events */
            g_isConnected = 1u;
            notifyEnabled = 0u;
            break;

        case CY_BLE_EVT_GATTS_WRITE_REQ:
        case CY_BLE_EVT_GATTS_WRITE_CMD_REQ:
        {
            if(eventParam == NULL)
            {
                /* Respond only to WRITE_REQ */
                if(event == CY_BLE_EVT_GATTS_WRITE_REQ)
                {
                    (void)Cy_BLE_GATTS_WriteRsp(g_connHandle);
                }
                break;
            }

            /* PDL BLE uses this param struct for write req/cmd events */
            cy_stc_ble_gatts_write_cmd_req_param_t *wr = (cy_stc_ble_gatts_write_cmd_req_param_t*)eventParam;

            /* Save connection handle from event */
            g_connHandle  = wr->connHandle;
            g_isConnected = 1u;

            /* CCCD write enables notifications */
            if(wr->handleValPair.attrHandle == TX_CCCD_HANDLE && wr->handleValPair.value.len >= 2u)
            {
                uint16_t cccd = (uint16_t)wr->handleValPair.value.val[0] | ((uint16_t)wr->handleValPair.value.val[1] << 8);
                notifyEnabled = (cccd & 0x0001u) ? 1u : 0u;

                /* Queue a small confirmation (optional) */
                if(notifyEnabled)
                    QueueTxString("NOTIFY ON\n");
                else
                    QueueTxString("NOTIFY OFF\n");
            }

            /* RX write (phone -> PSoC) */
            if(wr->handleValPair.attrHandle == RX_HANDLE)
            {
                if(wr->handleValPair.value.val != NULL && wr->handleValPair.value.len > 0u)
                {
                    ProcessIncomingRx(wr->handleValPair.value.val, wr->handleValPair.value.len);
                }
            }

            /* Must respond to WRITE_REQ (not needed for WRITE_CMD) */
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
    __enable_irq(); /* Enable global interrupts. */
    /* Enable CM4.  CY_CORTEX_M4_APPL_ADDR must be updated if CM4 memory layout is changed. */
    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR); 

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */
    
    Cy_GPIO_Write(GPIO_PRT7, 1u, 1u);
    Cy_GPIO_Write(GPIO_PRT6, 3u, 1u);

    /* Start BLE stack */
    Cy_BLE_Start(BleEventHandler);

    for(;;)
    {
        Cy_BLE_ProcessEvents();
        
        if(txPending && g_isConnected && notifyEnabled)
        {
            txPending = 0u;
            SendTxNotification(txBuf, txLen);
        }
        
    }
}





/* [] END OF FILE */
