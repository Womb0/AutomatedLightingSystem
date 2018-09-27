//Nicholas DiGiovacchino | Last Modified 4/27/18 | smartLighting.c

#include <MKL25Z4.H>
#include <stdbool.h>

#define MODE_MASK 0x00000004 					//2
#define SWITCH_MASK 0x00000002				//1
#define SENSOR1_MASK 0x00000020				//5
#define AUTOSIGNAL_MASK 0x00001000		//12
#define SENSOR1PWR_MASK 0x00000001		//0
#define LIGHTSIGNAL_MASK 0x00002000		//13


#define MINUTE_TIMER 1000000				//~1minute, 11000000 (30s * 2count)


volatile bool isOn = false;
volatile bool isManual = true;
volatile bool sensor1IRQ = false;

volatile uint16_t halfSeconds = 0;
volatile uint16_t historyIndex = 0; // 0 to 2015

bool history[144*14] = {false}; //2016 elements, 2016 ten minute slices in two weeks
	
unsigned long timer = MINUTE_TIMER; 
int count = 0;
int maxCount = 1;
//extern void asm_main(void);

void GPIO_Init(void) {
	// enable clocks on port A B C
	SIM->SCGC5 |= (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK);
	
	// initialize port parameters
	PORTA->PCR[1] = 0x000B0103; //GPIO in, B(5)interrupt on both edge, Manual on/off, switch L
	PORTA->PCR[2] = 0x000B0103; //GPIO in, B(5)interrupt on both edge, Mode M/A, switch R
	PORTA->PCR[5] = 0x000A0103; //GPIO in, motion sensor, 9(5)interupt on rising edge, pull down 3->2
	
	PORTB->PCR[0] = 0x00000140; //GPIO out, power for motion sensor
	
	PORTC->PCR[13] = 0x00000100; //GPIO out, light control signal
	PORTC->PCR[12] = 0x00000100; //GPIO out, LED tied to auto
	
	// initialize PDDR,0 (default) is input,1 output
	PTC->PDDR |= (LIGHTSIGNAL_MASK | AUTOSIGNAL_MASK);
	PTB->PDDR |= SENSOR1PWR_MASK;
	
	PTB->PSOR |= SENSOR1PWR_MASK; //enable motion sensor power
	
	for(int j = 0; j<10000; j++){} //let clocks stabilize
}

void Interrupt_Init(void) {
	//interrupts for PortA
	NVIC->ICPR[0] |= (1<<30);
	NVIC->ISER[0] |= (1<<30);
	NVIC->IP[7] = 0x03;
}

void setOn(void) {
	PTC->PSOR |= LIGHTSIGNAL_MASK;
	isOn = true;
}

void setOff(void) {
	PTC->PCOR |= LIGHTSIGNAL_MASK;
	isOn = false;
}
void setAutoLight() {
	if (isManual)
		PTC->PCOR |= AUTOSIGNAL_MASK;
	else
		PTC->PSOR |= AUTOSIGNAL_MASK;
}
int calculateNewCount(void) {
	// The bloody algorithm noone believes exists
	int total = 0;
	int newIndex;
	
	//last week
	if(historyIndex - 1014 < 0) 
		newIndex = historyIndex + 1014;
	else
		newIndex = historyIndex - 1014;
	for(int i = 0; i < 12; i++) {
		if(newIndex+i > 2015)
			newIndex = 0;
		if(history[newIndex] == true)
			total++;
	}
	
	//yesterday
		if(historyIndex - 150 < 0) 
		newIndex = 2016 - (150 - historyIndex);
	else
		newIndex = historyIndex - 150;
	for(int i = 0; i < 12; i++) {
		if(newIndex+i > 2015)
			newIndex = 0;
		if(history[newIndex] == true)
			total++;
	}
	
	//last hour
	if(historyIndex - 6 < 0) 
		newIndex = 2016 - (6 - historyIndex);
	else
		newIndex = historyIndex - 6;
	for(int i = 0; i < 6; i++) {
		if(newIndex+i > 2015)
			newIndex = 0;
		if(history[newIndex] == true)
			total++;
	}
	//return count, 1 count = ~1 minute
	return 2 + total;
}

void PORTA_IRQHandler() {
	//Clear interrupt
	NVIC->ICPR[0] |= (1<<30);
	
	//ISR for manual buttons
	if (PORTA->ISFR & MODE_MASK){ //check mode ISR
		if(PTA->PDIR & MODE_MASK) {
			isManual = true;
			if(PTA->PDIR & SWITCH_MASK)
				setOn();
		}
		else
			isManual = false;
		setAutoLight();
		PORTA->ISFR = MODE_MASK;
	}
	if (PORTA->ISFR & SWITCH_MASK){ // check switch ISR
		if (isManual) { 
			if(PTA->PDIR & SWITCH_MASK)
				setOn();
			else
				setOff();
		}
		PORTA->ISFR = SWITCH_MASK;
	}
	if (PORTA->ISFR & SENSOR1_MASK){ // ckeck sensor ISR
		sensor1IRQ = true;
	PORTA->ISFR = SENSOR1_MASK;
	}
} 

void SysTick_Handler(void) {
	halfSeconds++;
	if (halfSeconds == 6) {
		history[historyIndex] = isOn;
		halfSeconds = 0;
		historyIndex++;
		if (historyIndex >= 2015)
			historyIndex = 0;
	}
}
int main (void) {
	//asm_main();  // uncomment to use assembly
	
	SystemCoreClockUpdate();
	Interrupt_Init();
	SysTick_Config(10485760); //number is sysclock/2 (0.5s)
	
	GPIO_Init();

	//initial switch check
	if(PTA->PDIR & MODE_MASK)
		isManual = true;
	else
		isManual = false;
	setAutoLight();
	
	if (isManual) { 
		if(PTA->PDIR & SWITCH_MASK)
			setOn();
		else
			setOff();
	}
	while(1) { //Control loop
		
		if (sensor1IRQ) {
		//if motion detected and automatic, calculate new countdown
			if(!isManual){
				setOn();
				maxCount = calculateNewCount();
				timer = MINUTE_TIMER;
				count = 0;
			}
			sensor1IRQ = false;
		}
		
		if(!isManual) {//decrement logic to turn off automatic lights
			if(isOn) {
				timer--;
				if(timer == 0) {
					count++;
					timer = MINUTE_TIMER;
					if(count == maxCount) {
						count = 0;
						setOff();
					}
				}
			}
		}
	}	
}

