/* ========================================
 *
 * Copyright PXL, 2020
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/
#include "project.h"
#include <stdio.h>
#include <string.h>

#define LEDON 0
#define LEDOFF 1
#define NumberOfCommands 16

typedef enum{ red, green, blue, yellow, magenta, cyan, white, black } color;
typedef enum{init, idle, clearConfig, sendCommand, configMode} fsmState;

void StackEventHandler(uint32 event, void *eventParam);
void setLedColor(color x);
CYBLE_GATT_ERR_CODE_T updateIsConfigPresent(uint8 newValue);
CYBLE_GATT_ERR_CODE_T updatenextChar(uint8 newValue);

int bleConnected = 0;
int writeEventOccured = 0;
CYBLE_CONN_HANDLE_T connectionHandle;

//SAHWD variables
uint8 isConfigPresent = 1;
uint8 configClearRequestByCLient = 0;
fsmState state = init;
uint8 commandRequestByClient = 0;
uint8 configInitRequestByClient = 0;
uint8 nextChar = 0x65;
uint8 commandIndex = 0;
uint8 commandToSend = 0;
char serialData[100] = "";
uint8 giveMeNextChar = 0;

const char commandList[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'V', 'W', 'C', 'D', 'P', 'F'};
char ircodeList[NumberOfCommands] = {0};
uint8 configCounter = 0;

int main(void)
{
    CYBLE_API_RESULT_T apiResult;
    
    CyGlobalIntEnable; /* Enable global interrupts. */

    /* Place your initialization/startup code here (e.g. MyInst_Start()) */
    SERIAL_Start();
    SERIAL_PutString("PSoC BLE Custom Service\r\n");
    setLedColor(red);
    CyDelay(500);
    setLedColor(green);
    CyDelay(500);
    setLedColor(blue);
    CyDelay(500);
    setLedColor(yellow);
    CyDelay(500);
    setLedColor(magenta);
    CyDelay(500);
    setLedColor(cyan);
    CyDelay(500);
    setLedColor(white);
    CyDelay(500);
    setLedColor(yellow);
    
    apiResult = CyBle_Start(StackEventHandler);

    if(apiResult != CYBLE_ERROR_OK)
    {
        /* BLE stack initialization failed, check your configuration */
        CYASSERT(0);
    }
    
    //DEBUG
    updateIsConfigPresent(0x01);

    for(;;)
    {    
        CyBle_ProcessEvents();
        switch(state)
        {
            case init:
            //bluetooth not connected 
            setLedColor(red);
            if(bleConnected)
            {
                setLedColor(blue);
                state = idle;
            }
            break;
            case idle:
            //bluetooth connected
            if(configClearRequestByCLient)
                state = clearConfig;
            else if(commandRequestByClient)
                state = sendCommand;
            else if(configInitRequestByClient)
                state = configMode;
            
            break;
            case clearConfig:
                isConfigPresent = 0;
                //TODO CLEAR CONFIG
                if(updateIsConfigPresent(isConfigPresent) == CYBLE_GATT_ERR_NONE)
                {
                    //TODO Clear SRAM
                    SERIAL_PutString("CLEARING CONFIG\r\n");
                    nextChar = 0x65;
                }
                configClearRequestByCLient = 0;
                state = idle;
            break;
            case sendCommand:
                //TODO Send Command
                SERIAL_PutString("IR----->\r\n");  
                state = idle;
                commandRequestByClient = 0;
                break;
             case configMode:
                if(giveMeNextChar){
                    SERIAL_PutString("CONFIG Mode\r\n");
                    if(configInitRequestByClient)
                    {
                        sprintf(serialData, "Command to gatt db = %d\r\n", commandList[commandIndex]);
                        SERIAL_PutString(serialData);
                        updatenextChar(commandList[commandIndex]);
                    }
                    else
                    {
                        configInitRequestByClient = 0;
                        SERIAL_PutString("Config MODE Finished!\r\n");  
                        state = idle;
                        updateIsConfigPresent(1);
                    }
                    giveMeNextChar = 0;
                }
                break;
            default:
            //Invalid state
            SERIAL_PutString("Invalid state --> going to init\r\n");
            state = init;
            break;
        }
    }
}

void StackEventHandler(uint32 event, void *eventParam)
{
    CYBLE_GATTS_WRITE_REQ_PARAM_T *wrReqParam;
    CYBLE_GATTS_CHAR_VAL_READ_REQ_T *readReqParam;
    switch(event)
    {
        /* Mandatory events to be handled by Find Me Target design */
        case CYBLE_EVT_STACK_ON:
        case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
            SERIAL_PutString("BLE Disconnected\r\n");
            /* Start BLE advertisement for 30 seconds and update link
             * status on LEDs */
            bleConnected = 0;
            CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
            break;

        case CYBLE_EVT_GAP_DEVICE_CONNECTED:
            /* BLE link is established */
            SERIAL_PutString("BLE Connected\r\n");
            bleConnected = 1;
            break;
        case CYBLE_EVT_GATTS_WRITE_REQ:
            SERIAL_PutString("BLE Write Event\r\n");
            
            /* Extract the Write data sent by Client */
            wrReqParam = (CYBLE_GATTS_WRITE_REQ_PARAM_T *) eventParam;
            if(CYBLE_CLEARCONFIG_CUSTOM_CHARACTERISTIC_CHAR_HANDLE == wrReqParam->handleValPair.attrHandle)
            {
                if( *(wrReqParam->handleValPair.value.val) == 1)
                {
                    configClearRequestByCLient = 1;
                }
            }
            else if(CYBLE_EXECUTECOMMAND_CUSTOM_CHARACTERISTIC_CHAR_HANDLE == wrReqParam->handleValPair.attrHandle)
            {
                //TODO Process commands 
                //TODO Fix Command translation
                commandRequestByClient = 1;
                commandToSend = (uint8) wrReqParam->handleValPair.value.val[0];      
                
                SERIAL_PutString("Command Received\r\n");      
                sprintf(serialData, "Data Received from client: %c\r\n", commandToSend);
                SERIAL_PutString(serialData);      
            }
            else if(CYBLE_CONFIGINIT_CUSTOM_CHARACTERISTIC_CHAR_HANDLE == wrReqParam->handleValPair.attrHandle)
            {
                configInitRequestByClient = 1;
                SERIAL_PutString("config init request Received\r\n");      
            }
            (void)CyBle_GattsWriteRsp(((CYBLE_GATTS_WRITE_REQ_PARAM_T *)eventParam)->connHandle);
            break;
        case CYBLE_EVT_GAPP_ADVERTISEMENT_START_STOP:
            if(CyBle_GetState() == CYBLE_STATE_DISCONNECTED)
            {
                /* Advertisement event timed out, go to low power
                 * mode (Stop mode) and wait for device reset
                 * event to wake up the device again */
                CySysPmSetWakeupPolarity(CY_PM_STOP_WAKEUP_ACTIVE_HIGH);
                CySysPmStop();
               
                /* Code execution will not reach here */
            }
            break;

        /* Other events that are generated by the BLE Component that
         * are not required for functioning of this design */
        /**********************************************************
        *                       General Events
        ***********************************************************/
        case CYBLE_EVT_HARDWARE_ERROR:    /* This event indicates that some internal HW error has occurred. */
            break;

        case CYBLE_EVT_HCI_STATUS:
            break;

        /**********************************************************
        *                       GAP Events
        ***********************************************************/
        case CYBLE_EVT_GAP_AUTH_REQ:
            break;

        case CYBLE_EVT_GAP_PASSKEY_ENTRY_REQUEST:
            break;

        case CYBLE_EVT_GAP_PASSKEY_DISPLAY_REQUEST:
            break;

        case CYBLE_EVT_GAP_AUTH_COMPLETE:

            break;
        case CYBLE_EVT_GAP_AUTH_FAILED:

            break;

        case CYBLE_EVT_GAP_ENCRYPT_CHANGE:
            break;

        case CYBLE_EVT_GAPC_CONNECTION_UPDATE_COMPLETE:
            break;

        /**********************************************************
        *                       GATT Events
        ***********************************************************/
        case CYBLE_EVT_GATT_CONNECT_IND:
            break;

        case CYBLE_EVT_GATT_DISCONNECT_IND:
            break;

        case CYBLE_EVT_GATTS_XCNHG_MTU_REQ:
            break;

        case CYBLE_EVT_GATTS_READ_CHAR_VAL_ACCESS_REQ:
            SERIAL_PutString("BLE read Event\r\n");
            readReqParam = (CYBLE_GATTS_CHAR_VAL_READ_REQ_T *) eventParam;
            sprintf(serialData, "Attribute Handle number = %d\r\n", readReqParam->attrHandle);
            SERIAL_PutString(serialData);
            if(CYBLE_NEXTCHAR_CUSTOM_CHARACTERISTIC_CHAR_HANDLE == readReqParam->attrHandle && state == configMode){
                SERIAL_PutString("next char read access\r\n");
                giveMeNextChar = 1;
                if(commandIndex < NumberOfCommands)
                {
                    commandIndex++;
                }
                else
                {
                    configInitRequestByClient = 0;
                    commandIndex = 0;
                }
                sprintf(serialData, "Command Index = %d\r\n", commandIndex);
                SERIAL_PutString(serialData);
            }

            break;

        /**********************************************************
        *                       Other Events
        ***********************************************************/
        case CYBLE_EVT_PENDING_FLASH_WRITE:
            break;

        default:
            break;
    }
}

void setLedColor(color x)
{
    // red, green, blue, yellow, magenta, cyan, white, black 
    switch(x)
    {
        case red:
            LED_RED_Write(LEDON);
            LED_GREEN_Write(LEDOFF);
            LED_BLUE_Write(LEDOFF);
            break;
        case green:
            LED_RED_Write(LEDOFF);
            LED_GREEN_Write(LEDON);
            LED_BLUE_Write(LEDOFF);
            break;
        case blue:
            LED_RED_Write(LEDOFF);
            LED_GREEN_Write(LEDOFF);
            LED_BLUE_Write(LEDON);
            break;
       case yellow:
            LED_RED_Write(LEDON);
            LED_GREEN_Write(LEDON);
            LED_BLUE_Write(LEDOFF);
            break;
       case magenta:
            LED_RED_Write(LEDON);
            LED_GREEN_Write(LEDOFF);
            LED_BLUE_Write(LEDON);
            break;  
       case cyan:
            LED_RED_Write(LEDOFF);
            LED_GREEN_Write(LEDON);
            LED_BLUE_Write(LEDON);
            break;             
       case white:
            LED_RED_Write(LEDON);
            LED_GREEN_Write(LEDON);
            LED_BLUE_Write(LEDON);
            break;  
       case black:
            LED_RED_Write(LEDOFF);
            LED_GREEN_Write(LEDOFF);
            LED_BLUE_Write(LEDOFF);
            break;              
        default:
        break;
    }
}

CYBLE_GATT_ERR_CODE_T updateIsConfigPresent(uint8 newValue)
{
    CYBLE_GATT_ERR_CODE_T apiGattErrCode = 0;
    
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myHandle;
    myHandle.attrHandle = CYBLE_ISCONFIGPRESENT_CUSTOM_CHARACTERISTIC_CHAR_HANDLE; /* Attribute Handle of the characteristic in GATT Database*/
    myHandle.value.val = &newValue; /*Array where you want to store the data*/
    myHandle.value.len = sizeof(newValue); /* Length of data*/
    apiGattErrCode = CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    return apiGattErrCode;
}

CYBLE_GATT_ERR_CODE_T updatenextChar(uint8 newValue)
{
    CYBLE_GATT_ERR_CODE_T apiGattErrCode = 0;
    
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myHandle;
    myHandle.attrHandle = CYBLE_NEXTCHAR_CUSTOM_CHARACTERISTIC_CHAR_HANDLE; /* Attribute Handle of the characteristic in GATT Database*/
    myHandle.value.val = &newValue; /*Array where you want to store the data*/
    myHandle.value.len = sizeof(newValue); /* Length of data*/
    apiGattErrCode = CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    return apiGattErrCode;
}



/* [] END OF FILE */
