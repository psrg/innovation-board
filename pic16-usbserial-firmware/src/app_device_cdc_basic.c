/********************************************************************
 Software License Agreement:

 The software supplied herewith by Microchip Technology Incorporated
 (the "Company") for its PIC(R) Microcontroller is intended and
 supplied to you, the Company's customer, for use solely and
 exclusively on Microchip PIC Microcontroller products. The
 software is owned by the Company and/or its supplier, and is
 protected under applicable copyright laws. All rights are reserved.
 Any use in violation of the foregoing restrictions may subject the
 user to criminal sanctions under applicable laws, as well as to
 civil liability for the breach of the terms and conditions of this
 license.

 THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES,
 WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
 TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
 IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *******************************************************************/

/** INCLUDES *******************************************************/
#include <system.h>

#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include <usb.h>

#include <app_device_cdc_basic.h>
#include <usb_config.h>
#include <usart.h>
#include <xc.h>


/** VARIABLES ******************************************************/
static uint8_t USB_Out_Buffer[CDC_DATA_OUT_EP_SIZE];
static uint8_t RS232_Out_Data[CDC_DATA_IN_EP_SIZE];

unsigned char  NextUSBOut;
unsigned char    LastRS232Out;  // Number of characters in the buffer
unsigned char    RS232cp;       // current position within the buffer
unsigned char RS232_Out_Data_Rdy = 0;
USB_HANDLE  lastTransmission;

// LEDs
#define LED_TX_TRIS TRISC2;
#define LED_TX_LAT  LATC2;
#define LED_RX_TRIS TRISA4;
#define LED_RX_LAT  LATA4;

/*********************************************************************
* Function: void APP_DeviceCDCEmulatorInitialize(void);
*
* Overview: Initializes the demo code
*
* PreCondition: None
*
* Input: None
*
* Output: None
*
********************************************************************/
void APP_DeviceCDCEmulatorInitialize()
{
    // Turn on both LEDs initially until we connect.
    TRISA4 = 0;
    TRISC2 = 0;
    LATA4 = 1;
    LATC2 = 1;

    CDCInitEP();
    line_coding.bCharFormat = 0;
    line_coding.bDataBits = 8;
    line_coding.bParityType = 0;
    line_coding.dwDTERate = 19200;

    unsigned char i;
    USART_Initialize();

// 	 Initialize the arrays
	for (i=0; i<sizeof(USB_Out_Buffer); i++)
    {
		USB_Out_Buffer[i] = 0;
    }

	NextUSBOut = 0;
	LastRS232Out = 0;
	lastTransmission = 0;
    LATA4 = 0;
    LATC2 = 0;
}


    #define mDataRdyUSART() PIR1bits.RCIF
    #define mTxRdyUSART()   TXSTAbits.TRMT

/*********************************************************************
* Function: void APP_DeviceCDCEmulatorTasks(void);
*
* Overview: Keeps the demo running.
*
* PreCondition: The demo should have been initialized and started via
*   the APP_DeviceCDCEmulatorInitialize() and APP_DeviceCDCEmulatorStart() demos
*   respectively.
*
* Input: None
*
* Output: None
*
********************************************************************/
void APP_DeviceCDCEmulatorTasks()
{
    if((USBDeviceState < CONFIGURED_STATE)||(USBSuspendControl==1))
    {
        return;
    }

    if (RS232_Out_Data_Rdy == 0)  // only check for new USB buffer if the old RS232 buffer is
    {						  // empty.  This will cause additional USB packets to be NAK'd
        LastRS232Out = getsUSBUSART(RS232_Out_Data,64); //until the buffer is free.
        if(LastRS232Out > 0)
        {
            RS232_Out_Data_Rdy = 1;  // signal buffer full
            RS232cp = 0;  // Reset the current position
        }
    }

    //Check if one or more bytes are waiting in the physical UART transmit
    //queue.  If so, send it out the UART TX pin.
    if(RS232_Out_Data_Rdy && mTxRdyUSART())
    {
        LATC2 = 1;

        USART_putcUSART(RS232_Out_Data[RS232cp]);
        ++RS232cp;
        if (RS232cp == LastRS232Out)
        {
            LATC2 = 0;
            RS232_Out_Data_Rdy = 0;
        }
    }

    //Check if we received a character over the physical UART, and we need
    //to buffer it up for eventual transmission to the USB host.
    if(PIR1bits.RCIF && (NextUSBOut < (CDC_DATA_OUT_EP_SIZE - 1)))
    {
        LATA4 = 1;
        USB_Out_Buffer[NextUSBOut] = USART_getcUSART();
        ++NextUSBOut;
        USB_Out_Buffer[NextUSBOut] = 0;
    } else {
        LATA4 = 0;
    }

    //Check if any bytes are waiting in the queue to send to the USB host.
    //If any bytes are waiting, and the endpoint is available, prepare to
    //send the USB packet to the host.
    if((USBUSARTIsTxTrfReady()) && (NextUSBOut > 0))
    {
        putUSBUSART(&USB_Out_Buffer[0], NextUSBOut);
        NextUSBOut = 0;
    }

    CDCTxService();
}