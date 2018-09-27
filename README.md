# Automated Lighting System
Source code for lighting system that interacts with and stores user history to adjust light duration. Built on NXP KL25z.

**[Demonstration Video](https://youtu.be/32dDoZ-XoAY)**
 
Desription of video has commentary with time-stamps.

Senior Project for undergraduate computer engineering degree.
****************************************************************************************************************************************
## System
The system runs on a NXP KL25Z microcontroller that is housed in a one-gang work box. Inside is also support circuity designed by Cenk Candan, which interface the microcontroller to the live lighting circuit, controlled by relay. On the faceplate are two switches to control the system, an LED to identify current operating state, and a motion sensor to aid in system operation objectives. This one gang box will replace an existing one gang switch housing and interface with the existing lighting circuit, allowing current to flow through the relay when appropriate and give power to the lighting.

The system has two modes of operation, manual and automatic, controlled by one of the switches. When in manual mode, the system operates as a typical light switch does. The other switch acts as the ON/OFF while in manual mode and has no function in automatic mode. A GPIO signal is sent to a transistor to manipulate the relay.

In automatic mode the basic operation is that the lights are enabled based on motion and automatically shut off after the default alloted time. The duration is refreshed on each new detection of motion. In addition to this, an algorithm is used to dynamically add time to the shut off timer based on user history (detailed below). If in automatic mode the faceplate LED is lit to signal as such, it is off when in manual mode.

## Algorithm | User History

The algorithm is implemented to add duration to the shut off timer based on user history. User history is stored for two weeks in a      circular array. The light status is polled (ON/OFF) and stored in each index sequentially every 10 minutes. This means the first 144 indicies represent the first 24 hours of history, and the array has 2016 indicies to accomidate 2 weeks. The array is populated via a system interrupt that triggers every 10 minutes to check the status flag.

## Algorithm | Processing

When motion is detected in automatic mode, a new shutoff duration is calculated. The algorithm performs manipulations of the current index location (which always points to the current time slot) to look at index locations representing the past. It looks at those indicies representing last week +- 1 hour, yesterday +- 1 hour, and the last hour today. Their data is then simply summed and added to the default timer and returned as the new shutoff duration. The current iteration adds 1 minute per presence-of-use data point to a default 2 minute timer, giving a maximum duration of 32 minutes.

## Program Flow | smartLighting.c

The program first initalizes some bit masks, timer vaules, global variables, and status flags. These are followed by initialization functions for GPIO and interrupts. They initialize settings for GPIO and GPIO interrupt functionality. Three helper functions are then defined. Calling them opens or closes the lighting circuit with the third toggling the status LED. **calculateNewCount()** is the function that perfroms the calculation and is the function described by the section directly above. It returns an int that is used later in a wrapper function to change the shutoff timer duration. It contains logic to accomidate the circular nature of the history array.

There is then **PORTA_IRQHandler()** which is the IRQ routine for the GPIO port. An interrupt is generated when the switches are flipped or motion is detected. This interrupt controls all manual funtionality and sets a status flag for the main program to proceed in automatic mode when appropriate. Another status flag is updated when motion is detected and handled by the main program. 

**SysTick_Handler()** is the automatic 10 minute interrupt. It is actually an interrupt that is triggered every half second, but performs its operation when 10 minutes is reached by a counter. It puts data reflecting the light status in the current time slot of the user histoy array, then increments the current index to point to the next time slot. This also accomidates for the array's circular nature.

The **main()** function is then reached. SystemCoreClockUpdate() sets the operating frequency to a known value. **Interrupt_Init()** is called. **Systick_Config(unsigned long)** sets the frequency of system interrupts with a parameter of core clock ticks to trigger. GPIO_Init() is called. The inital switch positions are then checked and status updated accordingly. The control loop is then entered operating in a polling fashion. 

If a sensor irq was triggered, handle it; if in manual when recieved, ignore and clear. If in automatic, service it by calculating a new shutoff duration in *maxCount* and reset the timer. In a sequential statment, begin/continue the timer decrement. The c variable *timer* represents a 1 minute duration. When depleted, 1 minute has elapsed and *count* is incremented and checked againts *maxCount* (2<->32). When *count* and *maxCount* are equal the automatic duration is reached and the lighting circuit is broken.

Edge cases in this approach can be ignored with state mismatch only occuring worst case for one control loop cycle and 2 IRQ services, inconsequential to the 2-32 minute countdown.

## Improvements

Removal of polling logic and full interrupt / low power implementation

Track change of user history so only calculate on motion AND user history change to reduce number of calculations

Further testing on weight of user history to adjust *maxCount* range / more recent data hold more weight

Dedicated power supply

Wireless user interface functionality

Remote access/tracking/features

Private functions where potentially necessary

Removal of stdbool.h for reduced code size

Custom PCB design with smaller microcontroller

Potential desync over time, RTC implementation, RTC also offers additional control

External (SD)storage for larger user history

Cut power to motion sensor when in manual mode instead of ignoring interrupt
