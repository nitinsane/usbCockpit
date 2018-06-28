

#include <SoftwareSerial.h>

#define POT_PIN A0
#define SWITCH_PIN  9
#define LED_PIN 8


#undef DEBUG

#define NUM_BUTTONS  40
#define NUM_AXES  8        // 8 axes, X, Y, Z, etc

typedef struct joyReport_t {
    uint8_t reportID;
    struct {
    int16_t axis[NUM_AXES];
    uint8_t button[(NUM_BUTTONS+7)/8]; // 8 buttons per byte
    } data;
} joyReport_t;

joyReport_t joyReport = { 0, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0} };

typedef struct hostReport_t {
  uint8_t reportID;
  struct {
  uint8_t button[(NUM_BUTTONS+7)/8]; // 8 buttons per byte
  int16_t axis[NUM_AXES];
  } data;
} hostReport_t;

hostReport_t hostReport = { 0, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}};


SoftwareSerial mySerial (2, 3);

void setup() 
{
    Serial.begin(115200);
    mySerial.begin (9600);

  // init LED pin for output
  pinMode (LED_PIN, OUTPUT);

  // init switch pin for input
  pinMode (SWITCH_PIN, INPUT_PULLUP);

    
    delay(200);
}


void processHostReport (struct hostReport_t* hostReport)
{
	mySerial.print ("Report ID = ");
        mySerial.println (hostReport->reportID, HEX);
	mySerial.print ("Axis ==> ");
	for (int i = 0; i < NUM_AXES; i++) {
		mySerial.print (hostReport->axis [i]);
                mySerial.print ("  ");
        }

	mySerial.println ("");
        mySerial.print ("buttons ==> ");

	for (int i = 0; i < (NUM_BUTTONS+7)/8;i++) {
                mySerial.print (hostReport->button [i]);
                mySerial.print ("  ");
        }

	mySerial.println ("");

}

// Send an HID report to the USB interface
void sendJoyReport(struct joyReport_t *report)
{
#ifndef DEBUG
    Serial.write((uint8_t *)report, sizeof(joyReport_t));
#else
    // dump human readable output for debugging
    for (uint8_t ind=0; ind<NUM_AXES; ind++) {
  Serial.print("axis[");
  Serial.print(ind);
  Serial.print("]= ");
  Serial.print(report->axis[ind]);
  Serial.print(" ");
    }
    Serial.println();
    for (uint8_t ind=0; ind<NUM_BUTTONS/8; ind++) {
  Serial.print("button[");
  Serial.print(ind);
  Serial.print("]= ");
  Serial.print(report->button[ind], HEX);
  Serial.print(" ");
    }
    Serial.println();
#endif
}

// turn a button on
void setButton(joyReport_t *joy, uint8_t button)
{
    uint8_t index = button/8;
    uint8_t bit = button - 8*index;

    joy->button[index] |= 1 << bit;
}

// turn a button off
void clearButton(joyReport_t *joy, uint8_t button)
{
    uint8_t index = button/8;
    uint8_t bit = button - 8*index;

    joy->button[index] &= ~(1 << bit);
}

#define AP_BUTTON 0
#define FD_BUTTON 1
#define AT_BUTTON 2
#define LNAV_BUTTON 3
#define VNAV_BUTTON 4
#define FLCH_BUTTON 5
#define HDG_HOLD_BUTTON 6
#define VS_HOLD_BUTTON 7
#define ALT_HOLD_BUTTON 8
#define LOC_BUTTON 9
#define APP_BUTTON 10
#define AT_DISENGAGE_BUTTON 11



#define ALT_HOLD_INDEX 1


uint8_t button=0; // current button
bool press = true;  // turn buttons on?

/* Turn each button on in sequence 1 - 40, then off 1 - 40
 * add values to each axis each loop
 */
void loop() 
{
#if 0
    // Turn on a different button each time
    if (press) {
  setButton(&joyReport, button);
    } else {
  clearButton(&joyReport, button);
    }

    /* Move all of the axes */
    for (uint8_t ind=0; ind<8; ind++) {
  joyReport.axis[ind] += 10 * (ind+1);
    }

    sendJoyReport(&joyReport);

    button++;
    if (button >= 40) {
       button = 0;
       press = !press;
    }

#endif

  if (! digitalRead (SWITCH_PIN))
    setButton (&joyReport, FD_BUTTON);
  else
    clearButton (&joyReport, FD_BUTTON);

  joyReport.axis[ALT_HOLD_INDEX] = analogRead (POT_PIN);

  sendJoyReport (&joyReport);

    static unsigned int hostReadCounter = 0;
    char* ptr = (char *)&hostReport;
    
    while (Serial.available ()) {
      *ptr++ = Serial.read ();
      
      if (++hostReadCounter == sizeof (hostReport)) {
        ptr = (char *)&hostReport;
        hostReadCounter = 0;
        
        processHostReport (&hostReport);
        
      }
    }
  
    delay(100);

  // testing only.
  // heartbeat LED
  static uint32_t heartbeatCtr = 0;
  
  if (++heartbeatCtr == 10) {
    heartbeatCtr = 0;
    digitalWrite (LED_PIN, !digitalRead (LED_PIN));
  }
}

//////////////////////////////////////////////////////////////////////
class DigitalPin
{
  private:
    int _pin;
    
  public:
    DigitalPin (int pin) : _pin (pin) {}
    
    int getPin () const
    {
      return _pin;
    }
    
    boolean getState () const
    {
      return digitalRead (_pin);
    }

    bool isLow () const
    {
	return getState () == LOW;
    }

    bool isHigh () const
    {
	return getState () == HIGH;
    }
    
    void high ()
    {
      digitalWrite (_pin, true);
    }
    
    void low ()
    {
      digitalWrite (_pin, false);
    }
      
};

class DigitalOutputPin : public DigitalPin
{
  public:
    DigitalOutputPin (int pin) : DigitalPin (pin)
    {
      pinMode (getPin (), OUTPUT);
    }
};

class DigitalInputPin : public DigitalPin
{
  public:
    DigitalInputPin (int pin, boolean pullUp = true) : DigitalPin (pin)
    {
      pinMode (getPin (), pullUp ? INPUT_PULLUP : INPUT);
    }
    
};

class MCP23017
{
   private:
	class MCPDigitalPin : public DigitalPin
	{
	};

	class MCPDigitalInputPin : public DigitalInputPin
	{
	};

	class MCPDigitalOutputPin : public DigitalOutputPin
	{
	};

   private:
	DigitalPin _pin [16];

   public:
	MCP23017 (int i2cAddress)
	{
	}

	DigitalPin& getPin (uint32_t index)
	{
		return _pin [index];
	}

};

class AnalogPin
{
};


class LED : public DigitalOutputPin
{
  public:
    LED (int pin, boolean on = false) : DigitalOutputPin (pin)
    {
      if (on)
        high ();
      else
        low ();
    }
    
    void on ()
    {
      high ();
    }
    
    void off ()
    {
      low ();
    }
    
    boolean isOn () const
    {
      return getState () == true;
    }
    
    boolean isOff () const
    {
      return getState () == false;
    }
};

class Button : public DigitalInputPin
{
  public:
    Button (int pin, boolean pullUp = true) : DigitalInputPin (pin, pullUp)
    {
    }
};

class LEDButtoncombo
{
};

class Throttle
{
};

class Aileron
{
};

class Elevator
{
};

class Rudder
{
};

class ModeControlPanel
{
};

class usbCockpit
{
  private:
    ModeControlPanel _mcp;
    Rudder _rudder;
    Aileron _aileron;
    Elevator _elevator;
    
};
