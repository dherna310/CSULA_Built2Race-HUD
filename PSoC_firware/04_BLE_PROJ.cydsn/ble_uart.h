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
#ifndef _BLE_UART_H
#define _BLE_UART_H
    
#include "project.h"
#include "string.h"
#include "ctype.h"
#include "stdint.h"
    
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

static int str_equal_ci(const char *a, const char *b);
static void QueueTxString(const char *s);
static void SendTxNotification(const uint8_t *data, uint16_t len);
static void ProcessIncomingRx(const uint8_t *data, uint16_t len);
void BleEventHandler(uint32_t event, void *eventParam);
    
#endif

/* [] END OF FILE */
