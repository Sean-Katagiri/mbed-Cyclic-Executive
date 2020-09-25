#include "mbed.h"
#include "WattBob_TextLCD.h"
#include <vector>

#define WATCHDOG_PIN p25
#define EXECUTION_PULSE_PIN p26
#define INPUT_WAVE_PIN p5
#define DIGITAL_SWITCH_PIN p8
#define SHUTDOWN_SWITCH_PIN p29
#define ANALOGUE_INPUT_1_PIN p18
#define ANALOGUE_INPUT_2_PIN p20
// Code function:
// - 1. Measure frequency of 3.3v square wave once every second
// - 2. Read one digital input every 1/3 second (300ms for simplicity OR 3 times a second)
// - 3. Output a watchdog pulse every 4 seconds
// - 4. Read two analogue inputs every 1/2 second (500ms)
// - 5. Display values on LCD every 2 seconds
// - 6. Switch check every 1/2 second (500ms) and display on LCD
// - 7. Log values every 5 seconds
// - 8. Check shutdown switch every other slot

// P5~P8 for digital
// P15 ~ P20 for analogue

//**********************************************
//              Input & Output pins
//**********************************************
DigitalOut watchdog(WATCHDOG_PIN);
DigitalOut executionPulse(EXECUTION_PULSE_PIN);
DigitalIn wave(INPUT_WAVE_PIN);
DigitalIn digIn(DIGITAL_SWITCH_PIN);
DigitalIn shutdown(SHUTDOWN_SWITCH_PIN);

AnalogIn input1(ANALOGUE_INPUT_1_PIN);
AnalogIn input2(ANALOGUE_INPUT_2_PIN);

//Serial connection for logging
Serial pc(USBTX, USBRX);


//**********************************************
//                  Variables
//**********************************************

//Period and frequency of input wave signal
float period = 0;
int frequency = 0;

//Digital input switch
int switch_1 = 0;

//Analogue inputs
float analogue_in_1;
float analogue_in_2;

//Filtered analogue inputs
float average_analogue_in_1 = 0;
float average_analogue_in_2 = 0;

//Analogue input data array vectors
vector<float> analog1(4,0);
vector<float> analog2(4,0);

//Error code
int errorCode = 0;

//Execution time for each task
float exec1 = 0;
float exec2 = 0;
float exec3 = 0;
float exec4 = 0;
float exec5 = 0;
float exec6 = 0;
float exec7 = 0;

//Tick number
int ticks = 0;

//Pointer to 16-bit I/O object
MCP23017 *par_port;

//Pointer to LCD object
WattBob_TextLCD *lcd;

//Ticker for cyclic executive
Ticker ticker;

//Timers for wave period and task execution times
Timer timer;
Timer execTimer;

//**********************************************
//                  Tasks
//**********************************************

//Task 1: measure frequency of 3.3v square wave
void measureFrequency(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Reset timer for signal width
    timer.reset();
    
    
    //Case for encountering rising edge
    if(wave == 0){
        while(wave==0){}
        timer.start();
        while(wave==1){}
    }
    
    //Case for encountering falling edge
    else{
        while(wave==1){}
        timer.start();
        while(wave==0){}
    }
    
    //Stop the signal width timer
    timer.stop();
    
    //Calculate period = time(low->high) OR time(high->low) * 2 
    //multiplied by 2 because of 50% duty cycle wave
    period = timer.read_us()*2;
    frequency = 1000000/period;
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec1 = execTimer.read_us();
}

//Task 2: Read digital switch input
void readDigitalInput(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Read digital input switch
    switch_1 = digIn.read();

    //Stop and store time taken to perform task
    execTimer.stop();
    exec2 = execTimer.read_us();
}

//Task 3: Output a watchdog signal of width 7ms
void outputWatchdog(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Generate pulse of 7 ms
    watchdog = 1;
    wait_ms(7);
    watchdog = 0;
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec3 = execTimer.read_us();
}

//Task 4: Read analogue inputs and store average over past 4 inputs
void readAnalogueInput(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Read analogue inputs
    analogue_in_1 = input1.read();
    analogue_in_2 = input2.read();
    
    //Insert new reading at head of array, and remove oldest data from tail
    analog1.insert(analog1.begin(), analogue_in_1);
    analog1.erase(analog1.end());
    analog2.insert(analog2.begin(), analogue_in_2);
    analog2.erase(analog2.end());
    
    float sum1 = 0;
    float sum2 = 0;
    
    //Calculate sum of data
    for(int i = 0; i< analog1.size(); i++){
        sum1 += analog1[i];
        sum2 += analog2[i];
    }
    
    //Calculate average and set to 0~3.3v range
    average_analogue_in_1 = (sum1/4)*3.3;
    average_analogue_in_2 = (sum2/4)*3.3;
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec4 = execTimer.read_us();
}

//Task 5: Display variables on LCD
void display(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    lcd -> locate(0,0);
    lcd -> printf("F:%i  SW:%i", frequency, switch_1);
    lcd -> locate(1,0);
    lcd -> printf("%.2f  %.2f", average_analogue_in_1, average_analogue_in_2);
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec5 = execTimer.read_us();
}

//Task 6: Check error status
void errorCodes(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Check error condition
    if((switch_1 == 1) && (average_analogue_in_1 > average_analogue_in_2)){
        errorCode = 3;
    }
    else{
        errorCode = 0;
    }
    
    //Print error state to LCD
    lcd -> locate(0,13);
    if(errorCode == 3){
        lcd -> printf("E:3");
        }
    else{
        lcd -> printf("E:0");
    }
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec6 = execTimer.read_us();
}

//Task 7: Log variables to serial port
void log(){
    
    //Start timer
    execTimer.reset();
    execTimer.start();
    
    //Print to serial
    pc.printf("%i, %i, %.2f, %.2f \r\n", frequency, switch_1, average_analogue_in_1, average_analogue_in_2);
    //for debugging purposes, prints out execution times
    //pc.printf("%.1f, %.1f, %.1f, %.1f, %.1f, %.1f, %.1f \r\n", exec1, exec2, exec3, exec4, exec5, exec6, exec7);
    
    //Stop and store time taken to perform task
    execTimer.stop();
    exec7 = execTimer.read_us();
}

//Task 8: Check shutdown switch for system shutdown
void checkShutdown(){
    
    //Read shutdown switch
    int shutdownSwitch = shutdown.read();
    
    if(shutdownSwitch){
        //Print out shutdown message
        lcd -> cls();
        lcd -> locate(0,0);
        lcd -> printf("Shutdown");
        exit(0);
    }
}

//**********************************************
//                Cyclic Executive
//**********************************************

//Clock time of 50 ms
void CyclicExecutive(){
    
    //Task 1
    if(ticks % 20 == 1){
        measureFrequency();
    }
    
    //Task 2
    //else if(ticks % 6 == 2){
    else if(ticks % 20 == 2 || ticks % 20 == 8 || ticks % 20 == 14){
        readDigitalInput();
    }
    
    //Task 3
    else if(ticks % 80 == 3){
        outputWatchdog();
    }
    
    //Task 4
    else if(ticks % 10 == 5){
        readAnalogueInput();
    }
    
    //Task 5
    else if(ticks % 40 == 7){
        display();
    }
    
    //Task 6
    else if(ticks % 10 == 9){
        executionPulse = 1;
        errorCodes();
        executionPulse = 0;
    }
    
    //Task 7
    else if(ticks % 100 == 11){
        log();
    }
    
    //Task 8
    else{
        checkShutdown();
    }
    
    //Increment ticks
    ticks++;
}


int main(){
    //Initialise I/O
    par_port = new MCP23017(p9,p10,0x40);
    
    //Initialise LCD
    lcd = new WattBob_TextLCD(par_port);
    
    //Clear LCD & enable backlight
    lcd -> cls();   
    par_port -> write_bit(1,BL_BIT);
    
    //Start ticker at 50ms clock time
    ticker.attach(&CyclicExecutive, 0.050);
}