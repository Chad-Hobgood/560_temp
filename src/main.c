#include <MKL25Z4.h>
#include "ADC.h"
#include "CLI.h"
#include "LEDs.h"
#include "UART.h"
#include "project.h"
#include "sampling.h"
#include "timers.h"

#define SHIELD_BACKLIGHT_PIN 12U

static void Init_Shield_Backlight_Off(void) {
    /* PTA12 controls the shield backlight. Low leaves the display dark. */
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    PORTA->PCR[SHIELD_BACKLIGHT_PIN] =
        (PORTA->PCR[SHIELD_BACKLIGHT_PIN] & ~PORT_PCR_MUX_MASK) |
        PORT_PCR_MUX(1);
    GPIOA->PCOR = (1UL << SHIELD_BACKLIGHT_PIN);
    GPIOA->PDDR |= (1UL << SHIELD_BACKLIGHT_PIN);
}

static void Show_State(uint8_t state) {
    /* Control_RGB_LEDs handles the board's active-low LED pins. */
    switch (state) {
        case APP_CLI:           Control_RGB_LEDs(0, 0, 1); break; /* Blue */
        case APP_AWAIT_TRIGGER: Control_RGB_LEDs(1, 1, 0); break; /* Yellow */
        case APP_RECORDING:     Control_RGB_LEDs(0, 1, 0); break; /* Green */
        case APP_SEND_DATA:     Control_RGB_LEDs(1, 0, 1); break; /* Magenta */
        default:                Control_RGB_LEDs(0, 0, 0); break;
    }
}

int main(void) {
    uint8_t shown_state = APP_CLI;

    Init_Shield_Backlight_Off();
    Init_RGB_LEDs();
    Show_State(shown_state);
    Init_UART2();
    Init_ADC();
    Init_TPM0(sample_period_us);
    CLI_Show_Prompt();

    while (1) {
        uint8_t current_state = app_state;
        uint8_t previous_state = shown_state;

        if (current_state != shown_state) {
            shown_state = current_state;
            Show_State(current_state);
        }

        if (current_state == APP_CLI && Sampling_Take_Complete()) {
            /* Print even if a one-sample capture ended before this loop ran. */
            CLI_Show_Prompt();
        } else if (current_state == APP_CLI && previous_state == APP_SEND_DATA) {
            CLI_Show_Prompt();
        }

        if (current_state == APP_CLI) {
            CLI_Process();
        } else if (current_state == APP_SEND_DATA) {
            CLI_Send_Data();
        } else {
            /* Commands are ignored until the capture finishes. */
            while (UART2_Read_Char() >= 0) {
            }
        }
    }
}
