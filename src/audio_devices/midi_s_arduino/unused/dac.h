/*
    Driver for combined 17-bit PWM via Timers 0 and 1.
    https://github.com/DLehenbauer/arduino-midi-sound-module
    
    The ciruit combines the 8-bit PWM channels of each timer with a 1:256 resistor ratio
    to create two 16-bit PWM channels.  The synth outputs 8 voices to each 16-bit PWM channel,
    which are then combined in hardware for a total of 16 voices at 17-bit resolution.

    Splitting the voices between the PWM channels provides necessary headroom when mixing
    the voices and performing the final combination in hardware saves CPU cycles.

    Another benefit of the dual channel approach is that we can configure the PWMs to cancel
    each other's carrier waves when the signed output is resting at zero.

    Connection to Arduino Uno:
    
                    1M (1%)                  10uf*
        pin 5 >----^v^v^------o--------o------|(----> audio out
                              |        |
                   3.9k (1%)  |       === 3.3nf**
        pin 6 >----^v^v^------o        |
                              |       gnd
                              |
                              |
                    1M (1%)   |
        pin 9 >----^v^v^------o
                              |
                   3.9k (1%)  |
       pin 10 >----^v^v^------'

                                                                                                         
     * Note: A/C coupling capacitor typically optional.  (Negative is on audio out side.)
     ** Note: RC filtering capacitor can be adjusted to taste:
     
                8kHz      10kHz      30kHz
     2.2nf ~=  -0.7db    -1.1db     -5.6db
     3.3nf ~=  -1.5db    -2.2db     -8.4db
     4.7nf ~=  -2.7db    -3.6db    -11.1db

    Excellent reference:
    http://www.openmusiclabs.com/learning/digital/pwm-dac.1.html
    http://www.openmusiclabs.com/learning/digital/pwm-dac/dual-pwm-circuits/index.html
*/

#ifndef DAC_H_
#define DAC_H_

class Dac final {
  public:
  static void setup() {
    // Halt timers 0 and 1
    GTCCR = _BV(TSM) | _BV(PSRSYNC);
    
    // Note: The standard Arduino core registers a TIMER0_OVF_vect ISR to keep a count of passing
    //       time for 'millis()' and 'delay()'.
    //
    //       We need to disable interrupts for timer 0, or the Arduino library's ISR interrupt will
    //       starve our other work once we increase the timer's frequency for PWM.
    TIMSK0 = 0;                                         // Disable Timer 0 interrupts 
    TIMSK1 = 0;                                         // Disable Timer 1 interrupts
      
    // Setup Timer0 for PWM
    TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM01) | _BV(WGM00);   // Fast PWM (non-inverting), Top 0xFF
    TCCR0B = _BV(CS10);                                             // Prescale None
    DDRD |= _BV(DDD5) | _BV(DDD6);                                  // Output PWM to DDD5 / DDD6

    // Setup Timer1 for PWM
    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);    // Toggle OC1A/OC1B on Compare Match, Fast PWM (non-inverting)
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);       // Fast PWM, Top ICR1H/L, Prescale None
    ICR1H = 0;
    ICR1L = 0xFF;                                       // Top = 255 (8-bit PWM per output), 62.5khz carrier frequency
    DDRB |= _BV(DDB1) | _BV(DDB2);                      // Output PWM to DDB1 / DDB2

    // Synchronize Timers
    TCNT0 = 0x80;                                       // Set timer 0 and 1 counters so that they are 180 degrees out
    TCNT1H = TCNT1L = 0;                                // of phase, canceling the 62.5kHz carrier wave when amplitude is 0.

    GTCCR = 0;                                          // Resume timers
  }
  
  static void set0to7(int16_t out) {
    uint16_t u = static_cast<uint16_t>(out) + 0x8000;
    OCR0A = u >> 8;
    OCR0B = u & 0xFF;
  }
  
  static void set8toF(int16_t out) {
    uint16_t u = static_cast<uint16_t>(out) + 0x8000;
    OCR1B = u >> 8;
    OCR1A = u & 0xFF;
  }
};

#endif /* DAC_H_ */
