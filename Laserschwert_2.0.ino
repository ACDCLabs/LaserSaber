// I2Cdev and accelgyro6050 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "Laserschwert_2.h"
#include "I2Cdev.h"
// #include "MPU6050.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include "WT588D.h"
#include "Tlc5940.h"


// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

// Create the object representing the acceleration sensor
MPU6050 accelgyro;
// Create the object representing the soud module
WT588D mySoundChip(WT588D_RST, WT588D_CS, WT588D_SCL, WT588D_SDA, WT588D_BUSY);

// Speichert den Zustand (ein, aus, etc.. des Laserschwertes)
laserSchwertModus laserSchwertStatus;

// verschiedene Siganle für die Power LED
ledSignalModus ledSignal;

boolean laserschwertSollEin = false;

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t mpuIntEnabled;
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer


// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
  mpuInterrupt = true;
}


void setup() {

  Serial.begin(115200);
  // join I2C bus (I2Cdev library doesn't do this automatically)
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin();
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif

  accelgyro.initialize(); // initialize the acceleration sensor

  mySoundChip.begin(); // Initialize the sound chip

  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // load and configure the DMP
  Serial.println(F("Initializing DMP..."));
  devStatus = accelgyro.dmpInitialize();

  // gyro offsets as calibrated with MPU6050_Calibration
  accelgyro.setXAccelOffset(2265);
  accelgyro.setYAccelOffset(-545);
  accelgyro.setZAccelOffset(1616);
  accelgyro.setXGyroOffset(60);
  accelgyro.setYGyroOffset(-27);
  accelgyro.setZGyroOffset(11);

  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
    // turn on the DMP, now that it's ready
    Serial.println(F("Enabling DMP..."));
    accelgyro.setDMPEnabled(true);

    // enable Arduino interrupt detection
    Serial.println(F("Enabling interrupt detection (Arduino external interrupt 0)..."));
    attachInterrupt(0, dmpDataReady, RISING);
    mpuIntStatus = accelgyro.getIntStatus();

    // set our DMP Ready flag so the main loop() function knows it's okay to use it
    Serial.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;

    // get expected DMP packet size for later comparison
    packetSize = accelgyro.dmpGetFIFOPacketSize();
  } else {
    // ERROR!
    // 1 = initial memory load failed
    // 2 = DMP configuration updates failed
    // (if it's going to break, usually the code will be 1)
    Serial.print(F("DMP Initialization failed (code "));
    Serial.print(devStatus);
    Serial.println(F(")"));
  }


  /* Call Tlc.init() to setup the tlc.
  You can optionally pass an initial PWM value (0 - 4095) for all channels.*/
  Tlc.init();
  Serial.println("Tlc init");
  Tlc.clear();


  laserSchwertStatus = istAus;

  pinMode(SCHALTERPIN, INPUT); // Drucktaster zum ein und ausfahren
  pinMode(LEDPOWERPIN, OUTPUT); // Power On / Reday LED

  powerLED(aus);
  laserschwert(istAus, SCHWERT_VERZOEGERUNG);

}

void loop() {

  double acceleration, accelerationYZ;
  VectorInt16 accelVector;

  unsigned long millisBeiKnopfdruck, millisKnopfGedrueckt;

  // ================================================================
  // ===          Do all things if no interrupt occures           ===
  // ================================================================

  while (!mpuInterrupt ) {
    // Serial.println("In Loop");

    if (digitalRead(SCHALTERPIN) == 1)  { // Einschltknopf gedrückt
      millisBeiKnopfdruck = millis();
      while (digitalRead(SCHALTERPIN) == 1);// warten solange der Knopf gedrückt ist
      millisKnopfGedrueckt = millis() - millisBeiKnopfdruck; // wie langer wurde Knopf gedrückt? behandelt keinen überlauf
      Serial.print("Knopdruck=");
      Serial.println(millisKnopfGedrueckt);
      if (millisKnopfGedrueckt < MILLISKURZ) laserschwertSollEin = !laserschwertSollEin;
    }

    // Status des Laserschwertes ändern
    if (laserSchwertStatus != istAus && laserschwertSollEin == false) {
      laserSchwertStatus = einfahren;
    }
    else if (laserSchwertStatus == istAus && laserschwertSollEin == true) {
      laserSchwertStatus = ausfahren;
    }

    Serial.print("Status: ");
    Serial.print(laserSchwertStatus);
    Serial.print(" soll: ");
    Serial.println(laserschwertSollEin);

    switch (laserSchwertStatus) {

      case istAus:
        {
          powerLED(langsamBlinken);
        }
        break;

      case istEin:
        if ( ! mySoundChip.isBusy() ) {
          //mySoundChip.playSound(NUR_BRUMMEN);
          // delay(50);
          // Loop command sound chip to avoid breaks
        }
        // ledSignal = schnellBlinken;
        powerLED(schnellBlinken);
        break;

      case ausfahren:

        if ( ! mySoundChip.isBusy() ) {
          mySoundChip.playSound(SCHWERT_AUSFAHREN);
          delay(100);
        }
        laserSchwertStatus = laserschwert(ausfahren, SCHWERT_VERZOEGERUNG);
        Serial.println("Laserschwert ausfahren");
        break;

      case einfahren:

        //if ( ! mySoundChip.isBusy() ) {
        mySoundChip.playSound(SCHWERT_EINFAHREN);
        delay(100);
        // }
        laserSchwertStatus = laserschwert(einfahren, SCHWERT_VERZOEGERUNG);
        Serial.println("Laserschwert einfahren");
        break;

      case spezialGluehen:
        laserSchwertStatus = laserschwert(spezialGluehen, SCHWERT_VERZOEGERUNG);
        break;

      case ruhen:
        if ( ! mySoundChip.isBusy() ) {
          //mySoundChip.playSound(NUR_BRUMMEN);
          //delay(10);
          powerLED(langsamBlinken);
        }
        break;

      case bewegen:
        if ( ! mySoundChip.isBusy() ) {
          mySoundChip.playSound(BRUMMEN_SCHNELL);
          powerLED(langsamBlinken);
          delay(10);
        }
        break;

      case treffer:
        mySoundChip.playSound(SCHLAG_2);
        delay(10);
        break;

      default :
        {
          ;
        }
        break;

    }

  }


  // ================================================================
  // ===     Calculate acceleration if an interrupt came in       ===
  // ================================================================

  accelVector = accelgyroReadAcceleration();
  acceleration = sqrt(pow(accelVector.x, 2) + pow(accelVector.y, 2) + pow(accelVector.z, 2));
  accelerationYZ = sqrt(pow(accelVector.y, 2) + pow(accelVector.z, 2));

  if (false) {
    Serial.print("accel: ");
    Serial.print(acceleration);
    Serial.print("\t");
    Serial.print("accelYZ: ");
    Serial.println(accelerationYZ);
  }


  // if (accelerationYZ < 1000 ) laserSchwertStatus = ruhen;

  if (laserSchwertStatus == istAus )
    ;
  else {
    if ( accelerationYZ > 1500 && accelerationYZ < 7000 ) 
      laserSchwertStatus = bewegen;
    else
      laserSchwertStatus = ruhen;
      
    if (accelerationYZ > 7000 ) laserSchwertStatus = treffer;
  }
}

  laserSchwertModus laserschwert (laserSchwertModus modus, uint8_t Verzoegerung) {

    switch (modus) {

      case (ausfahren):
        for (int led = 0 ; led < 16; led++) {
          Tlc.set(led, 4095);
          delay(Verzoegerung);
          Tlc.update();
          Serial.println("ausfahren");
        }
        return (istEin);
        break; // macht nix


      case (einfahren):
        for (int led = 15 ; led >= 0; led--) {
          Tlc.set(led, 0);
          delay(Verzoegerung);
          Tlc.update();
          Serial.println("einfahren");
        }
        return (istAus);
        break; // macht nix

      case (spezialGluehen):
        Serial.println("Gluehen");
        for (int p = 0; p < 4096; p += 10) // heller
        {
          for (int l = 0; l < 16; l++)
          {
            Tlc.set(l, p);
          }
          Tlc.update();
          delay(5);
        }
        for (int p = 4095; p >= 0; p -= 10) // dunkler
        {
          for (int l = 0; l < 16; l++)
          {
            Tlc.set(l, p);
          }
          Tlc.update();
          delay(5);
        }
        return (spezialGluehen);
        break;

      default:
        Serial.println("default");
        return (modus);
        break;
    }
  }

  void powerLED(ledSignalModus modus) {

    const uint8_t zeitBlinkLang = 1000;
    const uint8_t zeitBlinkKurz = 50;
    static unsigned long zeitStart;
    static bool ledStatus;

    switch (modus) {

      case an:
        digitalWrite(LEDPOWERPIN, HIGH);
        break;

      case aus:
        digitalWrite(LEDPOWERPIN, LOW);

      case langsamBlinken:

        if (ledStatus == true) { // led ist an
          if ((millis() - zeitStart) < zeitBlinkLang ) {
            digitalWrite(LEDPOWERPIN, HIGH);
            ledStatus = true;
          }
          else {
            digitalWrite(LEDPOWERPIN, LOW);
            ledStatus = false;
            zeitStart = millis();
            // Serial.println("LED geht aus");
          }
        }
        else {  // led ist aus
          if ((millis() - zeitStart) < zeitBlinkLang ) {
            digitalWrite(LEDPOWERPIN, LOW);
            ledStatus = false;
          }
          else {
            digitalWrite(LEDPOWERPIN, HIGH);
            ledStatus = true;
            zeitStart = millis();
            // Serial.println("LED geht an");
          }
        }

        break;

      case schnellBlinken:
        break;

      case zweimalSchnell:
        break;

    }
  }

  VectorInt16 accelgyroReadAcceleration() {
    // orientation/motion vars
    Quaternion q;           // [w, x, y, z]         quaternion container
    VectorInt16 aa;         // [x, y, z]            accel sensor measurements
    VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
    VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
    VectorFloat gravity;    // [x, y, z]            gravity vector
    float euler[3];         // [psi, theta, phi]    Euler angle container
    float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector


    if (mpuInterrupt ) {
      // reset interrupt flag and get INT_STATUS byte
      mpuInterrupt = false;
      mpuIntStatus = accelgyro.getIntStatus();
      mpuIntEnabled = accelgyro.getIntEnabled();
      //Serial.print("MPU IntStat:");
      //Serial.print(mpuIntStatus,BIN);
      //Serial.print("MPU IntEna:");
      //Serial.println(mpuIntEnabled);
      // Serial.println(mpuIntEnabled,BIN);

      // get current FIFO count
      fifoCount = accelgyro.getFIFOCount();

      // check for overflow (this should never happen unless our code is too inefficient)
      if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        // reset so we can continue cleanly
        accelgyro.resetFIFO();
        Serial.println(F("FIFO overflow!"));

        // otherwise, check for DMP data ready interrupt (this should happen frequently)
      } else if (mpuIntStatus & 0x02) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = accelgyro.getFIFOCount();

        // read a packet from FIFO
        accelgyro.getFIFOBytes(fifoBuffer, packetSize);

        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;


        // display real acceleration, adjusted to remove gravity
        accelgyro.dmpGetQuaternion(&q, fifoBuffer);
        accelgyro.dmpGetAccel(&aa, fifoBuffer);
        accelgyro.dmpGetGravity(&gravity, &q);
        accelgyro.dmpGetLinearAccel(&aaReal, &aa, &gravity);

        if (false) {
          Serial.print("a_real\t");
          Serial.print(aaReal.x);
          Serial.print("\t");
          Serial.print(aaReal.y);
          Serial.print("\t");
          Serial.println(aaReal.z);
        }

      }
    }
    return aaReal;
  }


