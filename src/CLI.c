#include "CLI.h"
#include "UART.h"
#include "project.h"
#include "sampling.h"
#include "timers.h"

#define CLI_LINE_SIZE 16U

/* A command argument must be unsigned decimal and within its limit. */
static int Parse_Number(const char *text, uint16_t limit, uint16_t *result) {
    uint32_t value = 0;
    if (*text == '\0') return 0;
    while (*text != '\0') {
        if ((*text < '0') || (*text > '9')) return 0;
        value = value * 10U + (uint32_t)(*text - '0');
        if (value > limit) return 0;
        ++text;
    }
    *result = (uint16_t)value;
    return 1;
}

static void Handle_Command(const char *line) {
    uint16_t value;

    switch (line[0]) {
        case 'C':
            if (Parse_Number(line + 1, 27U, &value) &&
                ((value == 12U) || (value == 26U) || (value == 27U))) {
                sample_channel = (uint8_t)value;
                UART2_Write_String("Channel ");
                UART2_Write_Unsigned(value);
                UART2_Write_String(" selected.\r\n");
            }
            break;
        case 'P':
            if (Parse_Number(line + 1, 1000U, &value) && (value >= 5U)) {
                Set_Sample_Period(value);
                UART2_Write_String("Sampling period = ");
                UART2_Write_Unsigned(value);
                UART2_Write_String(" us.\r\n");
            }
            break;
        case 'N':
            if (Parse_Number(line + 1, PROJECT_MAX_SAMPLES, &value) &&
                (value >= 1U)) {
                sample_count = value;
                UART2_Write_String("Sample count = ");
                UART2_Write_Unsigned(value);
                UART2_Write_String(".\r\n");
            }
            break;
        case 'A':
            if (line[1] == '\0') {
                UART2_Write_String("Awaiting trigger.\r\n");
                Sampling_Arm();
            }
            break;
        case 'F':
            if (line[1] == '\0') {
                UART2_Write_String("Recording started.\r\n");
                Sampling_Force();
            }
            break;
        case 'S':
            if (line[1] == '\0') app_state = APP_SEND_DATA;
            break;
        default:
            break; /* Invalid commands have no effect. */
    }
}

void CLI_Show_Prompt(void) {
    UART2_Write_String("> ");
}

void CLI_Process(void) {
    static char line[CLI_LINE_SIZE];
    static uint8_t length = 0;
    static uint8_t discard = 0;
    static uint8_t last_was_cr = 0;
    int input;
    uint8_t command_leaves_cli;

    while ((input = UART2_Read_Char()) >= 0) {
        if ((input == '\r') || (input == '\n')) {
            /* Treat CRLF as one Enter, but accept CR or LF alone. */
            if ((input == '\n') && last_was_cr) {
                last_was_cr = 0;
                continue;
            }
            last_was_cr = (input == '\r');
            command_leaves_cli = 0;
            if (!discard && (length > 0U)) {
                line[length] = '\0';
                command_leaves_cli = (line[1] == '\0') &&
                    ((line[0] == 'A') || (line[0] == 'F') ||
                     (line[0] == 'S'));
                Handle_Command(line);
            }
            length = 0;
            discard = 0;
            /* A one-sample capture may already have finished in its IRQ. */
            if (command_leaves_cli || app_state != APP_CLI) return;
            CLI_Show_Prompt();
        } else {
            last_was_cr = 0;
            if ((input < 32) || (input > 126) ||
                (length >= CLI_LINE_SIZE - 1U)) {
                discard = 1;
            } else if (!discard) {
                line[length++] = (char)input;
            }
        }
    }
}

void CLI_Send_Data(void) {
    uint16_t i;
    uint16_t count = recorded_count;

    UART2_Write_String("Sending ");
    UART2_Write_Unsigned(count);
    UART2_Write_String(" samples.\r\n");
    for (i = 0; i < count; ++i) {
        /* Convert the 16-bit ADC code using the 3.3 V reference. */
        uint32_t millivolts = ((uint32_t)samples[i] * PROJECT_ADC_REFERENCE_MV +
                                32767U) / 65535U;
        UART2_Write_Unsigned((uint32_t)i + 1U);
        UART2_Write_String(": ");
        UART2_Write_Unsigned(millivolts / 1000U);
        UART2_Write_Char('.');
        UART2_Write_Char((char)('0' + (millivolts / 100U) % 10U));
        UART2_Write_Char((char)('0' + (millivolts / 10U) % 10U));
        UART2_Write_Char((char)('0' + millivolts % 10U));
        UART2_Write_String(" V\r\n");
    }
    app_state = APP_CLI;
}
