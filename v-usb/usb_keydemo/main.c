/**
 * Project: AVR ATtiny USB Tutorial at http://codeandlife.com/
 * Author: Joonas Pihlajamaa, joonas.pihlajamaa@iki.fi
 * Base on V-USB example code by Christian Starkjohann
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v3 (see License.txt)
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>

#include "usbdrv.h"

// ************************
// *** USB HID ROUTINES ***
// ************************

// From Frank Zhao's USB Business Card project
// http://www.frank-zhao.com/cache/usbbusinesscard_details.php
const PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

typedef struct {
	uint8_t modifier;
	uint8_t reserved;
	uint8_t keycode[6];
} keyboard_report_t;

static keyboard_report_t keyboard_report; // sent to PC
volatile static uchar LED_state = 0xff; // received from PC
static uchar idleRate; // repeat rate for keyboards

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
    usbRequest_t *rq = (void *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT: // send "no keys pressed" if asked here
            // wValue: ReportType (highbyte), ReportID (lowbyte)
            usbMsgPtr = (void *)&keyboard_report; // we only have this one
            keyboard_report.modifier = 0;
            keyboard_report.keycode[0] = 0;
            return sizeof(keyboard_report);
		case USBRQ_HID_SET_REPORT: // if wLength == 1, should be LED state
            return (rq->wLength.word == 1) ? USB_NO_MSG : 0;
        case USBRQ_HID_GET_IDLE: // send idle rate to PC as required by spec
            usbMsgPtr = &idleRate;
            return 1;
        case USBRQ_HID_SET_IDLE: // save idle rate as required by spec
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }
    
    return 0; // by default don't return any data
}

#define NUM_LOCK 1
#define CAPS_LOCK 2
#define SCROLL_LOCK 4

usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	if (data[0] == LED_state)
        return 1;
    else
        LED_state = data[0];
	
    // LED state changed
	// if(LED_state & CAPS_LOCK)
	// 	PORTB |= 1 << PB0; // LED on
	// else
	// 	PORTB &= ~(1 << PB0); // LED off
	
	return 1; // Data read, not expecting more
}

// Now only supports letters 'a' to 'z' and 0 (NULL) to clear buttons
void buildReport(uchar send_key) {
	keyboard_report.modifier = 0;
	
	if(send_key >= 'a' && send_key <= 'z')
		keyboard_report.keycode[0] = 4+(send_key-'a');
	else
		keyboard_report.keycode[0] = 0;
}

#define STATE_WAIT 0
#define STATE_RELEASE_KEY 1
#define STATE_SEND_KEY_0 2
#define STATE_SEND_KEY_1 3
#define STATE_SEND_KEY_2 4
#define STATE_SEND_KEY_3 5
#define STATE_SEND_KEY_4 6
#define STATE_SEND_KEY_5 7

int main() {
	uchar i, state = STATE_WAIT;
    uchar button_press_counter_0 = 0,button_press_counter_1 = 0,button_press_counter_2 = 0;
    uchar button_press_counter_3 = 0,button_press_counter_4 = 0,button_press_counter_5 = 0;
    uchar button_release_counter_0 = 0,button_release_counter_1 = 0,button_release_counter_2 = 0;
    uchar button_release_counter_3 = 0,button_release_counter_4 = 0,button_release_counter_5 = 0;
    uchar pressed_0 = 0,pressed_1 = 0,pressed_2 = 0,pressed_3 = 0,pressed_4 = 0,pressed_5 = 0;
	// PORTB = 1 << PB0; // PB0 as output
	// PORTB = 1 << PB1; // PB1 is input with internal pullup resistor activated
    DDRB &= ~(1<<PB0);
    DDRB &= ~(1<<PB1);
    DDRB &= ~(1<<PB2);
    DDRB &= ~(1<<PB3);
    DDRB &= ~(1<<PB4);
    DDRD &= ~(1<<PD7);
    PORTB &= ~(1<<PB0);
    PORTB &= ~(1<<PB1);
    PORTB &= ~(1<<PB2);
    PORTB &= ~(1<<PB3);
    PORTB &= ~(1<<PB4);
    PORTD &= ~(1<<PD7);

	
    for(i=0; i<sizeof(keyboard_report); i++) // clear report initially
        ((uchar *)&keyboard_report)[i] = 0;
    
    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    usbInit();
	
    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
    usbDeviceConnect();
    //Enalbe the Global Interrupt
    sei(); // Enable interrupts after re-enumeration
    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();
        
		if(!(PINB & (1<<PB0))) { // button pressed (PB1 at ground voltage)
            button_press_counter_0++;
			// also check if some time has elapsed since last button press
			if(state == STATE_WAIT && button_press_counter_0 >= 200 && pressed_0 == 1){
				state = STATE_SEND_KEY_0;
                button_press_counter_0 = 0;
                pressed_0 = 0;
            }
		}
        else{
            button_release_counter_0++; // increase release counter
            if(button_release_counter_0 >= 200 && pressed_0 == 0){
                button_release_counter_0 = 0; // now button needs to be released a while until retrigger
                pressed_0 = 1;
            } 
        }
        if(!(PINB & (1<<PB1))) { // button pressed (PB1 at ground voltage)
            button_press_counter_1++;
            // also check if some time has elapsed since last button press
            if(state == STATE_WAIT && button_press_counter_1 >= 200 && pressed_1 == 1){
                state = STATE_SEND_KEY_1;
                button_press_counter_1 = 0;
                pressed_1 = 0;
            }
        }
        else{
            button_release_counter_1++; // increase release counter
            if(button_release_counter_1 >= 200 && pressed_1 == 0){
                button_release_counter_1 = 0; // now button needs to be released a while until retrigger
                pressed_1 = 1;
            }
        }
        if(!(PINB & (1<<PB2))) { // button pressed (PB1 at ground voltage)
            button_press_counter_2++;
            // also check if some time has elapsed since last button press
            if(state == STATE_WAIT && button_press_counter_2 >= 200 && pressed_2 == 1){
                state = STATE_SEND_KEY_2;
                button_press_counter_2 = 0;
                pressed_2 = 0;
            }
        }
        else{
                        button_release_counter_2++; // increase release counter
            if(button_release_counter_2 >= 200 && pressed_2 == 0){
                button_release_counter_2 = 0; // now button needs to be released a while until retrigger
                pressed_2 = 1;
            }
        }
        if(!(PINB & (1<<PB3))) { // button pressed (PB1 at ground voltage)
            button_press_counter_3++;
            // also check if some time has elapsed since last button press
            if(state == STATE_WAIT && button_press_counter_3 >= 200 && pressed_3 == 1){
                state = STATE_SEND_KEY_3;
                button_press_counter_3 = 0;
                pressed_3 = 0;
            }
        }
        else{
                        button_release_counter_3++; // increase release counter
            if(button_release_counter_3 >= 200 && pressed_3 == 0){
                button_release_counter_3 = 0; // now button needs to be released a while until retrigger
                pressed_3 = 1;
            }
        }
        if(!(PINB & (1<<PB4))) { // button pressed (PB1 at ground voltage)
            button_press_counter_4++;
            // also check if some time has elapsed since last button press
            if(state == STATE_WAIT && button_press_counter_4 >= 200 && pressed_4 == 1){
                state = STATE_SEND_KEY_4;
                button_press_counter_4 = 0;
                pressed_4 = 0;
            }
        }
        else{
            button_release_counter_4++; // increase release counter
            if(button_release_counter_4 >= 200 && pressed_4 == 0){
                button_release_counter_4 = 0; // now button needs to be released a while until retrigger
                pressed_4 = 1;
            }
        }
        if(!(PIND & (1<<PD7))) { // button pressed (PB1 at ground voltage)
            button_press_counter_5++;
            // also check if some time has elapsed since last button press
            if(state == STATE_WAIT && button_press_counter_5 >= 200 && pressed_5 == 1){
                state = STATE_SEND_KEY_5;
                button_press_counter_5 = 0;
                pressed_5 = 0;
            }
        }
        else{
            button_release_counter_5++; // increase release counter
            if(button_release_counter_5 >= 200 && pressed_5 == 0){
                button_release_counter_5 = 0; // now button needs to be released a while until retrigger
                pressed_5 = 1;
            }
        }
	
        // characters are sent when messageState == STATE_SEND and after receiving
        // the initial LED state from PC (good way to wait until device is recognized)
        if(usbInterruptIsReady() && state != STATE_WAIT && LED_state != 0xff){
			switch(state) {
            case STATE_RELEASE_KEY:
                buildReport(NULL);
                state = STATE_WAIT; // go back to waiting
                break;
			case STATE_SEND_KEY_0:
				buildReport('a');
				state = STATE_RELEASE_KEY; // release next
				break;
            case STATE_SEND_KEY_1:
                buildReport('s');
                state = STATE_RELEASE_KEY; // release next
                break;
            case STATE_SEND_KEY_2:
                buildReport('d');
                state = STATE_RELEASE_KEY; // release next
                break;
            case STATE_SEND_KEY_3:
                buildReport('f');
                state = STATE_RELEASE_KEY; // release next
                break;
            case STATE_SEND_KEY_4:
                buildReport('w');
                state = STATE_RELEASE_KEY; // release next
                break;
            case STATE_SEND_KEY_5:
                buildReport('e');
                state = STATE_RELEASE_KEY; // release next
                break;                                                                            
			default:
				state = STATE_WAIT; // should not happen
			}
			
			// start sending
            usbSetInterrupt((void *)&keyboard_report, sizeof(keyboard_report));
        }
    }
	
    return 0;
}
