/*
Nintendo Switch Fightstick - Proof-of-Concept

Based on the LUFA library's Low-Level Joystick Demo
	(C) Dean Camera
Based on the HORI's Pokken Tournament Pro Pad design
	(C) HORI

This project implements a modified version of HORI's Pokken Tournament Pro Pad
USB descriptors to allow for the creation of custom controllers for the
Nintendo Switch. This also works to a limited degree on the PS3.

Since System Update v3.0.0, the Nintendo Switch recognizes the Pokken
Tournament Pro Pad as a Pro Controller. Physical design limitations prevent
the Pokken Controller from functioning at the same level as the Pro
Controller. However, by default most of the descriptors are there, with the
exception of Home and Capture. Descriptor modification allows us to unlock
these buttons for our use.
*/

/** \file
 *
 *  Main source file for the posts printer demo. This file contains the main tasks of
 *  the demo and is responsible for the initial application hardware configuration.
 */

#include "Joystick.h"

extern const uint8_t image_data[0x7094] PROGMEM;

// Main entry point.
int main(void)
{
    // We'll start by performing hardware and peripheral setup.
    SetupHardware();
    // We'll then enable global interrupts for our use.
    GlobalInterruptEnable();
    // Once that's done, we'll enter an infinite loop.
    // ReadHeader();
    for (;;)
    {
        // We need to run our task to process and deliver data for our IN and OUT endpoints.
        HID_Task();
        // We also need to run the main USB management task.
        USB_USBTask();
    }
}

// Configures hardware and peripherals, such as the USB peripherals.
void SetupHardware(void)
{
    // We need to disable watchdog if enabled by bootloader/fuses.
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    // We need to disable clock division before initializing the USB hardware.
    clock_prescale_set(clock_div_1);
    // We can then initialize our hardware and peripherals, including the USB stack.

#ifdef ALERT_WHEN_DONE
// Both PORTD and PORTB will be used for the optional LED flashing and buzzer.
#warning LED and Buzzer functionality enabled. All pins on both PORTB and \
PORTD will toggle when printing is done.
    DDRD = 0xFF; //Teensy uses PORTD
    PORTD = 0x0;
    //We'll just flash all pins on both ports since the UNO R3
    DDRB = 0xFF; //uses PORTB. Micro can use either or, but both give us 2 LEDs
    PORTB = 0x0; //The ATmega328P on the UNO will be resetting, so unplug it?
#endif
    // The USB stack should be initialized last.
    USB_Init();
}

// Fired to indicate that the device is enumerating.
void EVENT_USB_Device_Connect(void)
{
    // We can indicate that we're enumerating here (via status LEDs, sound, etc.).
}

// Fired to indicate that the device is no longer connected to a host.
void EVENT_USB_Device_Disconnect(void)
{
    // We can indicate that our device is not ready (via status LEDs, sound, etc.).
}

// Fired when the host set the current configuration of the USB device after enumeration.
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    // We setup the HID report endpoints.
    ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_OUT_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(JOYSTICK_IN_EPADDR, EP_TYPE_INTERRUPT, JOYSTICK_EPSIZE, 1);

    // We can read ConfigSuccess to indicate a success or failure at this point.
}

// Process control requests sent to the device from the USB host.
void EVENT_USB_Device_ControlRequest(void)
{
    // We can handle two control requests: a GetReport and a SetReport.

    // Not used here, it looks like we don't receive control request from the Switch.
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void)
{
    // If the device isn't connected and properly configured, we can't do anything here.
    if (USB_DeviceState != DEVICE_STATE_Configured)
        return;

    // We'll start with the OUT endpoint.
    Endpoint_SelectEndpoint(JOYSTICK_OUT_EPADDR);
    // We'll check to see if we received something on the OUT endpoint.
    if (Endpoint_IsOUTReceived())
    {
        // If we did, and the packet has data, we'll react to it.
        if (Endpoint_IsReadWriteAllowed())
        {
            // We'll create a place to store our data received from the host.
            USB_JoystickReport_Output_t JoystickOutputData;
            // We'll then take in that data, setting it up in our storage.
            while (Endpoint_Read_Stream_LE(&JoystickOutputData, sizeof(JoystickOutputData), NULL) != ENDPOINT_RWSTREAM_NoError)
                ;
            // At this point, we can react to this data.

            // However, since we're not doing anything with this data, we abandon it.
        }
        // Regardless of whether we reacted to the data, we acknowledge an OUT packet on this endpoint.
        Endpoint_ClearOUT();
    }

    // We'll then move on to the IN endpoint.
    Endpoint_SelectEndpoint(JOYSTICK_IN_EPADDR);
    // We first check to see if the host is ready to accept data.
    if (Endpoint_IsINReady())
    {
        // We'll create an empty report.
        USB_JoystickReport_Input_t JoystickInputData;
        // We'll then populate this report with what we want to send to the host.
        GetNextReport(&JoystickInputData);
        // Once populated, we can output this data to the host. We do this by first writing the data to the control stream.
        while (Endpoint_Write_Stream_LE(&JoystickInputData, sizeof(JoystickInputData), NULL) != ENDPOINT_RWSTREAM_NoError)
            ;
        // We then send an IN packet on this endpoint.
        Endpoint_ClearIN();
    }
}

typedef enum
{
    SYNC_CONTROLLER,
    CLEAN_SCREEN,
    READY,
    SHIFT_COLOR,
    MOVE_RIGHT,
    MOVE_DOWN,
    MOVE_COLUMN,
    PAINT,
    DONE
} State_t;
State_t state = SYNC_CONTROLLER;

#define ECHOES 2
int echoes = 0;
USB_JoystickReport_Input_t last_report;

int report_count = 0;
short xpos = 0;
short ypos = 0;
bool paint_done = false;
int portsval = 0;

// TODO: SWITCH TO READING ACTUAL HEADER
uint8_t colors_used[16] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0x10, 0x11};
short header_length = 19;
uint8_t current_color = 16;
uint8_t new_color = 1;
uint8_t index = 0;
// d = done; r = right; l = left;
char shift_color = 'd';

void ChangeColorIndex(void)
{
    if ((current_color == 17) && (shift_color == 'r'))
    {
        current_color = 1;
    }
    else if ((current_color == 1) && (shift_color == 'l'))
    {
        current_color = 17;
    }
    else
    {
        current_color += (shift_color == 'r') ? 1 : -1;
    }
    shift_color = (current_color == new_color) ? 'd' : shift_color;
}

void ReadHeader(void)
{
    // First byte = 1 if compressed else 0
    // EMPTY LINE = 0xFF
    // ...COLORS, 1 byte each
    // EMPTY LINE = 0xFF
    uint8_t color_count = 0;
    bool stop = false;
    uint8_t temp = 0;
    while (!stop)
    {
        temp = pgm_read_byte(&(image_data[color_count + 2]));
        if (temp == 0xff)
        {
            stop = true;
        }
        else
        {
            colors_used[color_count] = temp;
            color_count++;
        }
    }
    header_length = color_count + 1;
}

// Prepare the next report for the host.
void GetNextReport(USB_JoystickReport_Input_t *const ReportData)
{

    // Prepare an empty report
    memset(ReportData, 0, sizeof(USB_JoystickReport_Input_t));
    ReportData->LX = STICK_CENTER;
    ReportData->LY = STICK_CENTER;
    ReportData->RX = STICK_CENTER;
    ReportData->RY = STICK_CENTER;
    ReportData->HAT = HAT_CENTER;

    // Repeat ECHOES times the last report
    if (echoes > 0)
    {
        memcpy(ReportData, &last_report, sizeof(USB_JoystickReport_Input_t));
        echoes--;
        return;
    }

    // States and moves management
    switch (state)
    {
    case SYNC_CONTROLLER:
        if (report_count > 125)
        {
            report_count = 0;
            state = CLEAN_SCREEN;
        }
        else if (report_count == 25 || report_count == 50)
        {
            ReportData->Button |= SWITCH_L | SWITCH_R;
        }
        else if (report_count == 100 || report_count == 125)
        {
            ReportData->Button |= SWITCH_A;
        }
        report_count++;
        break;
    case CLEAN_SCREEN:
        if (report_count == 25)
        {
            ReportData->Button |= SWITCH_MINUS;
        }
        else if (report_count > 25 && report_count < (25 + 150))
        {
            ReportData->LY = STICK_MIN;
        }
        else if (report_count > (25 + 150) && report_count < 25 + (150 * 2))
        {
            ReportData->LX = STICK_MIN;
        }
        else if (report_count > 25 + (150 * 2))
        {
            report_count = 0;
            state = READY;
        }

        report_count++;
        break;
    case READY:

        // TODO: MAKE READABLE
        index = pgm_read_byte(&(image_data[header_length + (ypos + (xpos * 320)) / 2])) & (15 << (((ypos + (xpos * 320)) % 2) * 4));
        if (index > 15)
        {
            index = index >> 4;
        }
        new_color = colors_used[index];

        if (!paint_done)
        {
            state = PAINT;
        }
        else
        {
            if (new_color != current_color)
            {
                state = SHIFT_COLOR;
                if (((new_color + 17) - current_color) < (current_color - new_color))
                {
                    shift_color = 'r';
                }
                else
                {
                    shift_color = 'l';
                }
            }
            else
            {
                if (ypos < 320)
                {
                    state = MOVE_DOWN;
                }
                else
                {
                    if (xpos < 90)
                    {
                        state = MOVE_COLUMN;
                    }
                    else
                    {
                        state = DONE;
                    }
                }
            }
        }

        report_count++;
        break;
    case SHIFT_COLOR:
        if (report_count > 25)
        {
            ChangeColorIndex();
            report_count = 0;
            state = (shift_color == 'd') ? READY : SHIFT_COLOR;
        }
        else if (report_count == 25)
        {
            ReportData->Button |= (shift_color == 'r') ? SWITCH_ZR : SWITCH_ZL;
        }
        report_count++;
        break;
    case MOVE_DOWN:
        if (report_count > 25)
        {
            report_count = 0;
            ypos += 1;
            paint_done = false;
            state = READY;
        }
        else if (report_count == 25)
        {
            ReportData->HAT = HAT_BOTTOM;
        }
        report_count++;
        break;
    case MOVE_RIGHT:
        if (report_count > 25)
        {
            report_count = 0;
            xpos += 1;
            paint_done = false;
            state = READY;
        }
        else if (report_count == 25)
        {
            ReportData->HAT = HAT_RIGHT;
        }
        report_count++;
        break;
    case MOVE_COLUMN:
        if (report_count > 125)
        {
            report_count = 0;
            ypos = 0;
            state = MOVE_RIGHT;
        }
        else if (report_count > 25)
        {
            ReportData->LY = STICK_MIN;
        }
        report_count++;
        break;
    case PAINT:
        if (report_count == 25)
        {
            ReportData->Button = SWITCH_A;
        }
        else if (report_count > 25)
        {
            paint_done = true;
            state = READY;
        }
        report_count++;
        break;
    case DONE:
        return;
    }

    // if (state != SYNC_CONTROLLER)
    //     ReportData->Button |= SWITCH_A;

    // Prepare to echo this report
    memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
    echoes = ECHOES;
}
