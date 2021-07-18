#include "Joystick_test.h"

// Main entry point.
int main(void)
{
    for (;;)
    {
        HID_Task();
        usleep(125);
    }
}

// Process and deliver data from IN and OUT endpoints.
void HID_Task(void)
{
    USB_JoystickReport_Input_t JoystickInputData;

    GetNextReport(&JoystickInputData);
}

typedef enum
{
    SYNC_CONTROLLER,
    CLEAN_SCREEN,
    READ_HEADER,
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

uint8_t colors_used[19] = {};
uint8_t color_index = 0;

short header_length;

uint8_t current_color = 16;
uint8_t new_color = 1;
uint8_t last_color = 99;
// d = done; r = right; l = left;
char shift_color = 'd';

uint8_t ReadBitFromImage(uint16_t idx)
{
    if (idx < 9600)
    {
        return image_data_1[idx];
    }
    else if (idx < 19200)
    {
        return image_data_2[idx - 9600];
    }
    else
    {
        return image_data_3[idx - 19200];
    }
}

uint8_t ReadNextBitFromImage(void)
{
    // Get First half 00001111
    if ((ypos + (xpos * 180)) % 2)
    {
        return ReadBitFromImage((ypos + (xpos * 180)) / 2) & 0xF;
    }
    // Get Second half 11110000
    else
    {
        return (ReadBitFromImage((ypos + (xpos * 180)) / 2) & 0xF0) >> 4;
    }
}

void ReadHeader(void)
{
    uint8_t color_count = 0;
    uint8_t temp = colors[color_count];
    while (temp != '\0')
    {
        colors_used[color_count] = temp;
        color_count++;
        temp = colors[color_count];
    }
    header_length = color_count;
}

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

bool MoveColorRight(void)
{
    short steps = new_color - current_color;
    // Move Right IF:
    // - Positive and under 9 steps
    // - Negative and over 9 steps
    return (((steps > 0) && (steps < 9)) || ((steps < 0) && (steps < -9)));
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
            state = READ_HEADER;
        }

        report_count++;
        break;
    case READ_HEADER:
        ReadHeader();
        state = READY;
        break;
    case READY:
        color_index = ReadNextBitFromImage();
        new_color = colors_used[color_index];

        if (new_color != current_color)
        {
            state = SHIFT_COLOR;
        }
        else if (paint_done == false)
        {
            // TESTING AREA
            if (color_index != 14)
            {
                printf("[X:%d,Y:%d] IDX:%d, colorIDX: %d, New color: %d\n", xpos, ypos, (ypos + (xpos * 180)), color_index, new_color);

                // printf("R=1: %d\n", (ypos + (xpos * 180)) % 2);
                // printf("Byte: %d\n", ReadBitFromImage(header_length + (ypos + (xpos * 180)) / 2));
                // printf("Right: %d\n", ReadBitFromImage(header_length + (ypos + (xpos * 180)) / 2) & 0xF);
                // printf("Left: %d\n", (ReadBitFromImage(header_length + (ypos + (xpos * 180)) / 2) & 0xF) >> 4);
                // exit(1);
            }
            // TESTING
            paint_done = true;
            state = PAINT;
        }
        // Move
        else
        {
            paint_done = false;
            if (ypos < 179)
            {
                state = MOVE_DOWN;
            }
            else
            {
                if (xpos < 319)
                {
                    state = MOVE_COLUMN;
                }
                else
                {
                    state = DONE;
                }
            }
        }
        report_count++;
        break;
    case SHIFT_COLOR:
        if (report_count > 15)
        {
            ChangeColorIndex();
            report_count = 0;
            state = (shift_color == 'd') ? READY : SHIFT_COLOR;
        }
        else if (report_count == 15)
        {
            if (MoveColorRight())
            {
                shift_color = 'r';
            }
            else
            {
                shift_color = 'l';
            }
            ReportData->Button |= (shift_color == 'r') ? SWITCH_ZR : SWITCH_ZL;
        }
        report_count++;
        break;
    case MOVE_DOWN:
        if (report_count > 15)
        {
            report_count = 0;
            ypos += 1;
            state = READY;
        }
        else if (report_count == 15)
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
        if (report_count == 15)
        {
            ReportData->Button = SWITCH_A;
        }
        else if (report_count > 15)
        {
            report_count = 0;
            state = READY;
        }
        report_count++;
        break;
    case DONE:
        exit(0);
        return;
    }

    // if (state != SYNC_CONTROLLER)
    //     ReportData->Button |= SWITCH_A;

    // Prepare to echo this report
    memcpy(&last_report, ReportData, sizeof(USB_JoystickReport_Input_t));
    echoes = ECHOES;
}
