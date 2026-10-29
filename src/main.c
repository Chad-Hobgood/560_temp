#include <MKL25Z4.h>
#include "ADC.h"
#include "LEDs.h"
#include "UART.h"
#include "project.h"
#include "timers.h"

#define SHIELD_BACKLIGHT_PIN 12U

static void Init_Shield_Backlight_Off(void) {
    /* PTA12 drives the shield backlight. It is active high, so low is off.
       Only this pin is changed; the RGB LEDs use PTB18/PTB19/PTD1. */
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    PORTA->PCR[SHIELD_BACKLIGHT_PIN] =
        (PORTA->PCR[SHIELD_BACKLIGHT_PIN] & ~PORT_PCR_MUX_MASK) |
        PORT_PCR_MUX(1);
    GPIOA->PCOR = (1UL << SHIELD_BACKLIGHT_PIN);
    GPIOA->PDDR |= (1UL << SHIELD_BACKLIGHT_PIN);
}

/* Watch these variables in the debugger. Interrupt handlers update them. */
volatile uint8_t app_state = APP_CLI;
volatile uint32_t debug_irq_flags = 0;
volatile uint32_t debug_uart_rx_count = 0;
volatile uint32_t debug_uart_error_count = 0;
volatile uint32_t debug_uart_drop_count = 0;
volatile uint32_t debug_adc_count = 0;
volatile uint32_t debug_timer_count = 0;
volatile uint8_t debug_uart_byte = 0;
volatile uint8_t debug_uart_status = 0;
volatile uint16_t debug_adc_value = 0;

/* Recording settings from the project specification. */
uint16_t samples[PROJECT_MAX_SAMPLES];
uint16_t sample_count = PROJECT_DEFAULT_SAMPLES;
uint16_t sample_period_us = PROJECT_DEFAULT_PERIOD_US;
uint8_t sample_channel = PROJECT_DEFAULT_CHANNEL;
volatile uint16_t recorded_count = 0;

#define CLI_LINE_SIZE 16U

/* Parse an unsigned decimal argument without accepting signs or spaces. */
static int Parse_Number(const char *text, uint16_t limit, uint16_t *result) {
    uint32_t value = 0;

    if (*text == '\0') {
        return 0;
    }
    while (*text != '\0') {
        if ((*text < '0') || (*text > '9')) {
            return 0;
        }
        value = value * 10U + (uint32_t)(*text - '0');
        if (value > limit) {
            return 0;
        }
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
                app_state = APP_AWAIT_TRIGGER;
            }
            break;

        case 'F':
            if (line[1] == '\0') {
                UART2_Write_String("Recording started.\r\n");
                app_state = APP_RECORDING;
            }
            break;

        case 'S':
            if (line[1] == '\0') {
                app_state = APP_SEND_DATA;
            }
            break;

        default:
            break; /* Unrecognized input is ignored. */
    }
}

static void Process_CLI(void) {
    static char line[CLI_LINE_SIZE];
    static uint8_t length = 0;
    static uint8_t discard = 0;
    static uint8_t last_was_cr = 0;
    int input;

    while ((input = UART2_Read_Char()) >= 0) {
        if ((input == '\r') || (input == '\n')) {
            /* A terminal may send CR, LF, or CRLF for Enter. */
            if ((input == '\n') && last_was_cr) {
                last_was_cr = 0;
                continue;
            }
            last_was_cr = (input == '\r');

            if (!discard && (length > 0U)) {
                line[length] = '\0';
                Handle_Command(line);
            }
            length = 0;
            discard = 0;

            if (app_state != APP_CLI) {
                return;
            }
            UART2_Write_String("> ");
        } else {
            last_was_cr = 0;
            if ((input < 32) || (input > 126) ||
                (length >= CLI_LINE_SIZE - 1U)) {
                discard = 1; /* Ignore invalid or overlong commands. */
            } else if (!discard) {
                line[length++] = (char)input;
            }
        }
    }
}

static void Send_Data(void) {
    uint16_t i;
    uint16_t count = recorded_count;

    if (count > PROJECT_MAX_SAMPLES) {
        count = PROJECT_MAX_SAMPLES;
    }
    UART2_Write_String("Sending ");
    UART2_Write_Unsigned(count);
    UART2_Write_String(" samples.\r\n");
    for (i = 0; i < count; ++i) {
        /* Convert a 16-bit ADC code using the specified 3.3 V reference. */
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

static void Show_State(uint8_t state) {
    /* The board LEDs are active low; Control_RGB_LEDs handles that detail. */
    switch (state) {
        case APP_CLI:
            Control_RGB_LEDs(0, 0, 1); /* Blue */
            break;
        case APP_AWAIT_TRIGGER:
            Control_RGB_LEDs(1, 1, 0); /* Yellow */
            break;
        case APP_RECORDING:
            Control_RGB_LEDs(0, 1, 0); /* Green */
            break;
        case APP_SEND_DATA:
            Control_RGB_LEDs(1, 0, 1); /* Magenta */
            break;
        default:
            Control_RGB_LEDs(0, 0, 0); /* Invalid state: LEDs off. */
            break;
    }
}

int main(void) {
    uint8_t shown_state = APP_CLI;

    /* Initialize the serial link, ADC, and sample timer once at startup. */
    Init_Shield_Backlight_Off();
    Init_RGB_LEDs();
    Show_State(shown_state);
    Init_UART2();
    Init_ADC();
    Init_TPM0(sample_period_us);
#if PROJECT_DEBUG_START_INTERRUPTS
    /* Optional smoke test: one ADC result and recurring timer ticks. */
    Start_ADC_Conversion(sample_channel);
    Start_Sample_Timer();
#endif

    UART2_Write_String("> ");

    /* Handle commands only in CLI. Other states will gain acquisition logic. */
    while (1) {
        uint8_t current_state = app_state;
        if (current_state != shown_state) {
            shown_state = current_state;
            Show_State(current_state);
            if (current_state == APP_CLI) {
                UART2_Write_String("> ");
            }
        }

        if (current_state == APP_CLI) {
            Process_CLI();
        } else if (current_state == APP_SEND_DATA) {
            Send_Data();
        } else {
            /* Ignore UART input while waiting or recording. */
            while (UART2_Read_Char() >= 0) {
            }
        }
    }
}
