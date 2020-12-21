/* 
 * File:   newmain.c
 * Author: sihes
 *
 * Created on November 29, 2020, 2:28 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <avr/interrupt.h>

static volatile int *LED_PORTS[10] = {&PORTB, &PORTB, &PORTB, &PORTB, &PORTB, &PORTD, &PORTD, &PORTD, &PORTD, &PORTD};
static volatile int *LED_DDRS[10]  = {&DDRB,  &DDRB,  &DDRB,  &DDRB,  &DDRB,  &DDRD,  &DDRD,  &DDRD,  &DDRD,  &DDRD};
static volatile int LED_BITS[10]   = {  4,      3,      2,      1,      0,      7,      6,      5,      4,      3};
static int LEDcount = sizeof(LED_PORTS)/sizeof(LED_PORTS[0]);

#define DELAY_CONST (10)
#define LONG_PRESS_DELAY (150*DELAY_CONST)

#define CTRL_PIN (PIND)
#define CTRL_BIT (1)

#define LEFT_BTTN_PIN (PIND)
#define LEFT_BTTN_DDR (DDRD)
#define LEFT_BTTN_BIT (PIND0)

#define RIGHT_BTTN_PIN (PINC)
#define RIGHT_BTTN_DDR (DDRC)
#define RIGHT_BTTN_BIT (PINC5)

#define CTRL_BTTN_PIN (PIND)
#define CTRL_BTTN_DDR (DDRD)
#define CTRL_BTTN_BIT (PIND1)
volatile int CTRL_BTTN_CLICKED = 0;
volatile int CTRL_BTTN_CLICK_PERIOD = 0;
volatile int PLAYER_NEEDS_ATTN = 0;
volatile int CTRL_NEEDS_ATTN = 0;

volatile int ctrlHoldCounter = 0;
volatile int prevCtrlCnt = 0;

static volatile int difficultyDelays[3] = {15*DELAY_CONST, 4*DELAY_CONST, DELAY_CONST};
volatile int difficulty = 1;
volatile int gameEnabled = 0;
volatile int global_time = 0;
volatile int delayEnabled = 0;
volatile int stopGameLEDloop = 0;

volatile int PLAYER_CLICKED = 0;
volatile int leftPlayerScore = 0;
volatile int rightPlayerScore = 0;

void handleControl();
void endGame();
void runGame();
void onLED(int idx);
void offLED(int idx);

/*
 * 
 */
int main(int argc, char** argv) {
    
    //setup timer, interrupt, and initial port/pin values
    setupGame();
    
    while(1) {
        if (gameEnabled) {
            runGame();
        }else {
            mainMenu();
        }
    }

    return (EXIT_SUCCESS);
}

void runGame() {
    int blink_period = difficultyDelays[difficulty];
    for (int i=0; i<LEDcount; i=i+1) {
        handleCtrlBttn();
        onLED(i);
        handlePlayerInput();
        delay(blink_period);
        handlePlayerInput();
        offLED(i);
        if (stopGameLEDloop) {
            stopGameLEDloop = 0;
            break;
        }
    }
    for (int i=LEDcount-2; i>0; i=i-1) {
        handleCtrlBttn();
        onLED(i);
        handlePlayerInput();
        delay(blink_period);
        handlePlayerInput();
        offLED(i);
        if (stopGameLEDloop) {
            stopGameLEDloop = 0;
            break;
        }
    }
}

void mainMenu() {
    if (CTRL_NEEDS_ATTN) {
        handleCtrlBttn();
        return;
    }
    for (int i=0; i<LEDcount && !CTRL_NEEDS_ATTN; i++) {   //this condition is used so it breaks out of the for loop when button pressed
        if (gameEnabled) {
            break;
        }
        onLED(i);
        delay(difficultyDelays[difficulty]);
        offLED(i);
    }
    delay(500*DELAY_CONST);
}

void handlePlayerInput() {
    PLAYER_NEEDS_ATTN = 0;
    if (PLAYER_CLICKED && gameEnabled) {
        if (PLAYER_CLICKED==1) {
            verifyLeftScored();
        }else if (PLAYER_CLICKED==2) {
            verifyRightScored();
        }
        stopGameLEDloop = 1;
    }
    PLAYER_CLICKED = 0;
}

void handleCtrlBttn() {
    CTRL_NEEDS_ATTN = 0;
    
    //if button was held for "LONG_PRESS_DELAY" periods and it was just released, toggle the game
    if (CTRL_BTTN_CLICKED && CTRL_BTTN_CLICK_PERIOD >= LONG_PRESS_DELAY) {
        CTRL_BTTN_CLICK_PERIOD = 0;
        if (gameEnabled) {
            endGame();
        }else {
            restartGame();
        }
    }else if (CTRL_BTTN_CLICKED && !gameEnabled) {
        difficulty++;
        int numOfDifficulties = 3;
        if (difficulty>=numOfDifficulties) {
            difficulty = 0;
        }
        displayDifficulty();
    }
    
    CTRL_BTTN_CLICKED = 0;
}

ISR(TIMER0_COMPA_vect) {
    global_time++;
    
    if (leftBttnIsOn() && !PLAYER_CLICKED) {
        PLAYER_CLICKED = 1;
        PLAYER_NEEDS_ATTN = PLAYER_CLICKED && gameEnabled;
    }else if (rightBttnIsOn() && !PLAYER_CLICKED) {
        PLAYER_CLICKED = 2;
        PLAYER_NEEDS_ATTN = PLAYER_CLICKED && gameEnabled;
    }
    
    if (ctrlBttnIsOn()) {
        ctrlHoldCounter++;
    }else {
        if (prevCtrlCnt) {  //if button was JUST previously on
            CTRL_BTTN_CLICKED = 1;
            CTRL_BTTN_CLICK_PERIOD = ctrlHoldCounter;
            
            CTRL_NEEDS_ATTN = (CTRL_BTTN_CLICKED && !gameEnabled) || (CTRL_BTTN_CLICK_PERIOD >= LONG_PRESS_DELAY);
        }
        ctrlHoldCounter = 0;
    }
    prevCtrlCnt = ctrlHoldCounter;
}

void delay(volatile int period) {
    int end_time = global_time + period;
    delayEnabled = 1;
    while (delayEnabled && (global_time <= end_time) && !CTRL_NEEDS_ATTN && !PLAYER_NEEDS_ATTN) {
        asm("NOP");
    }
    delayEnabled = 0;
}

void playerWins() {
    for (int i=0; i<=10; i++) {
        for (int j=0; j<LEDcount; j++) {
            onLED(j);
        }
        delay(15*DELAY_CONST);
        for (int j=0; j<LEDcount; j++) {
            offLED(j);
        }
        delay(15*DELAY_CONST);
    }
    endGame();
}


void displayPlayerScore(int startLED, int score, int increment) {
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    delay(40*DELAY_CONST);
    
    int endLED = startLED + (increment*(score));
    for (int i=startLED; i!=endLED; i=i+increment) {
        onLED(i);
        delay(40*DELAY_CONST);
    }
    delay(100*DELAY_CONST);
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    delay(40*DELAY_CONST);
    delayEnabled = 0;
}

void displayDifficulty() {
    delayEnabled = 0;
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    for (int i=0; i<=difficulty; i++) {
        onLED(i);
    }
    delay(100*DELAY_CONST);
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    delay(50*DELAY_CONST);
}

void verifyLeftScored() {
    if (isLEDon(LEDcount-1)) {  //if left LED is on
        leftPlayerScore++;
        int startDisplayAtLED = LEDcount-1;
        int score = leftPlayerScore;
        int increment = -1;
        displayPlayerScore(startDisplayAtLED, score, increment);
        if (leftPlayerScore==LEDcount) {
            playerWins();
            stopGameLEDloop = 1;
        }
    }else {
        delay(40*DELAY_CONST);
    }
}

void verifyRightScored() {
    if (isLEDon(0)) {  //if left LED is on
        rightPlayerScore++;
        int startDisplayAtLED = 0;
        int score = rightPlayerScore;
        int increment = +1;
        displayPlayerScore(startDisplayAtLED, score, increment);
        if (rightPlayerScore==LEDcount) {
            playerWins();
            stopGameLEDloop = 1;
        }
    }else {
        delay(70*DELAY_CONST);
    }
}

void onLED(int idx) {
    int *port = LED_PORTS[idx];
    int *dirREG = LED_DDRS[idx];
    *dirREG |= (1<<LED_BITS[idx]);
    *port |= (1<<LED_BITS[idx]);
}

void offLED(int idx) {
    int *port = LED_PORTS[idx];
    int *dirREG = LED_DDRS[idx];
    *dirREG &= ~(1<<LED_BITS[idx]);
    *port &= ~(1<<LED_BITS[idx]);
}

int isLEDon(int idx) {
    int LEDport = *LED_PORTS[idx];
    int LEDbit = LED_BITS[idx];
    return (LEDport & (1<<LEDbit));
}

int leftBttnIsOn() {
    LEFT_BTTN_DDR |= (1<<LEFT_BTTN_BIT);
    int pinOn = LEFT_BTTN_PIN & (1<<LEFT_BTTN_BIT);
    return pinOn;
}

int rightBttnIsOn() {
    RIGHT_BTTN_DDR |= (1<<RIGHT_BTTN_BIT);
    int pinOn = RIGHT_BTTN_PIN & (1<<RIGHT_BTTN_BIT);
    return pinOn;
}

int ctrlBttnIsOn() {
    CTRL_BTTN_DDR |= (1<<CTRL_BTTN_BIT);
    int pinOn = CTRL_BTTN_PIN & (1<<CTRL_BTTN_BIT);
    return pinOn;
}

void restartGame() {
    stopGameLEDloop = 1;
    gameEnabled = 1;
    IOinitiate();
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    setupGame();
    delayEnabled = 0;
    leftPlayerScore = 0;
    rightPlayerScore = 0;
}

void endGame() {
    gameEnabled = 0;
    IOinitiate();
    for (int i=0; i<LEDcount; i++) {
        offLED(i);
    }
    delayEnabled = 0;
}

void setupGame() {
    TCCR0A |= (1 << WGM01);                 // Set the Timer Mode to CTC
    OCR0A = 0x11;                           //Set the value that you want to count to
    TIMSK0 |= (1<<OCIE0A);
    TCCR0B |=    (1 << CS00)|(1 << CS02);   //1024    times    using the    101    bit    pattern.
    
    asm("sei");                             //Enable Interrupts
}

void IOinitiate() {
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PORTB)) : 
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PORTC)) : 
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PORTD)) : 
        );
    
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(DDRB)) :
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(DDRC)) : // Bit 4 on Port B
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(DDRD)) : // Bit 4 on Port B
        );
    
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PINB)) :
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PINC)) : // Bit 4 on Port B
        );
    asm ("sbi %0, 0 \n" // clear Bit
        : : "M" (_SFR_IO_ADDR(PIND)) : // Bit 4 on Port B
        );
}