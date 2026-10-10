#include <Wire.h>
#include <EEPROM.h>
#include <RTClib.h>
#include <U8x8lib.h>
#include <SoftwareSerial.h>
#include <Adafruit_Fingerprint.h>

#define FINGER_RX_PIN 2
#define FINGER_TX_PIN 3

// ESP32 communication
#define ESP_TX_PIN 10

#define OLED_ADDRESS_FIRST 0x3C
#define OLED_ADDRESS_SECOND 0x3D

#define SET_RTC_ON_UPLOAD false
#define MAX_FINGERPRINT_ID 127

U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);

SoftwareSerial fingerSerial(FINGER_RX_PIN, FINGER_TX_PIN);
Adafruit_Fingerprint finger(&fingerSerial);

// D10 -> ESP32 GPIO16
SoftwareSerial espSerial(-1, ESP_TX_PIN);

RTC_DS3231 rtc;


// --------------------------------------------------
// STUDENT DATABASE
// Fingerprint Template ID | Student ID | Student Name
// --------------------------------------------------

const struct Person {
  uint16_t fingerprintID;
  const char *studentID;
  const char *name;
} people[] = {

  {2,  "02", "Abhin"},
  {3,  "03", "Abishek"},
  {4,  "04", "Ajith"},
  {5,  "05", "Akash"},
  {6,  "06", "Anoop"},
  {21, "21", "Chandra"},
  {24, "24", "Dharineesh"},
  {27, "27", "Dinesh"},
  {29, "29", "Farseen"},
  {47, "47", "Jamshid"}

};

const uint8_t PEOPLE_COUNT =
  sizeof(people) / sizeof(people[0]);

unsigned long lastClockPaint = 0;


// --------------------------------------------------
// I2C
// --------------------------------------------------

bool i2cResponds(uint8_t address)
{
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}


// --------------------------------------------------
// STUDENT LOOKUP
// --------------------------------------------------

const Person *findPerson(uint16_t id)
{
  for (uint8_t i = 0; i < PEOPLE_COUNT; i++)
  {
    if (people[i].fingerprintID == id)
    {
      return &people[i];
    }
  }

  return nullptr;
}


// --------------------------------------------------
// EEPROM ATTENDANCE
// --------------------------------------------------

uint16_t dayNumber(const DateTime &now)
{
  return (uint16_t)(now.unixtime() / 86400UL);
}

int eepromAddress(uint16_t id)
{
  return (id - 1) * 2;
}

uint16_t lastDayFor(uint16_t id)
{
  uint16_t value;

  EEPROM.get(eepromAddress(id), value);

  return value;
}

void saveDay(uint16_t id, uint16_t value)
{
  EEPROM.put(eepromAddress(id), value);
}


// --------------------------------------------------
// OLED DISPLAY
// --------------------------------------------------

void clearLine(uint8_t row)
{
  oled.drawString(0, row, "                ");
}


// Center a string on the 16-character OLED
void centerText(const char *text, uint8_t row)
{
  uint8_t length = strlen(text);

  if (length > 16)
    length = 16;

  uint8_t column = (16 - length) / 2;

  clearLine(row);

  oled.drawString(column, row, text);
}


// Display four centered lines
void showCenteredScreen(
  const char *line1,
  const char *line2 = "",
  const char *line3 = "",
  const char *line4 = "")
{
  oled.clear();

  centerText(line1, 0);
  centerText(line2, 2);
  centerText(line3, 4);
  centerText(line4, 6);
}


// --------------------------------------------------
// TIME / DATE
// --------------------------------------------------

void formatTime12(
  const DateTime &now,
  char *output,
  size_t size)
{
  uint8_t hour = now.hour() % 12;

  if (hour == 0)
    hour = 12;

  snprintf(
    output,
    size,
    "%02u:%02u:%02u %s",
    hour,
    now.minute(),
    now.second(),
    now.hour() < 12 ? "AM" : "PM"
  );
}


const char *weekdayName(uint8_t day)
{
  const char *days[] =
  {
    "SUNDAY",
    "MONDAY",
    "TUESDAY",
    "WEDNESDAY",
    "THURSDAY",
    "FRIDAY",
    "SATURDAY"
  };

  return days[day];
}


// --------------------------------------------------
// STANDBY SCREEN
// --------------------------------------------------

void showClock(bool redraw = false)
{
  DateTime now = rtc.now();

  char date[9];
  char time[13];
  char day[11];

  // DD/MM/YY
  snprintf(
    date,
    sizeof(date),
    "%02d/%02d/%02d",
    now.day(),
    now.month(),
    now.year() % 100
  );

  formatTime12(
    now,
    time,
    sizeof(time)
  );

  snprintf(
    day,
    sizeof(day),
    "%s",
    weekdayName(now.dayOfTheWeek())
  );

  if (redraw)
  {
    showCenteredScreen(
      "ATTENDANCE",
      time,
      date,
      day
    );
  }
  else
  {
    centerText("ATTENDANCE", 0);
    centerText(time, 2);
    centerText(date, 4);
    centerText(day, 6);
  }
}


// --------------------------------------------------
// SERIAL EVENT
// --------------------------------------------------

void sendEvent(
  const char *studentID,
  const char *name,
  const DateTime &now,
  const char *status)
{
  char date[9];
  char time12[13];
  char time24[9];

  snprintf(
    date,
    sizeof(date),
    "%02d/%02d/%02d",
    now.day(),
    now.month(),
    now.year() % 100
  );

  formatTime12(
    now,
    time12,
    sizeof(time12)
  );

  snprintf(
    time24,
    sizeof(time24),
    "%02d:%02d:%02d",
    now.hour(),
    now.minute(),
    now.second()
  );


  // Send to ESP32 through D10
  espSerial.print("ATTEND,");
  espSerial.print(studentID);
  espSerial.print(",");
  espSerial.print(name);
  espSerial.print(",");
  espSerial.print(date);
  espSerial.print(",");
  espSerial.print(weekdayName(now.dayOfTheWeek()));
  espSerial.print(",");
  espSerial.print(time12);
  espSerial.print(",");
  espSerial.print(time24);
  espSerial.print(",");
  espSerial.println(status);


  // Also show the event on USB Serial Monitor
  Serial.print("ESP32 EVENT: ATTEND,");
  Serial.print(studentID);
  Serial.print(",");
  Serial.print(name);
  Serial.print(",");
  Serial.print(date);
  Serial.print(",");
  Serial.print(weekdayName(now.dayOfTheWeek()));
  Serial.print(",");
  Serial.print(time12);
  Serial.print(",");
  Serial.print(time24);
  Serial.print(",");
  Serial.println(status);
}


// --------------------------------------------------
// FINGERPRINT
// --------------------------------------------------

// -1 = no finger
// -2 = scan error
// -3 = fingerprint not enrolled
// >=0 = recognized template ID

int findFingerprint()
{
  uint8_t result;

  result = finger.getImage();

  if (result == FINGERPRINT_NOFINGER)
    return -1;

  if (result != FINGERPRINT_OK)
    return -2;

  result = finger.image2Tz();

  if (result != FINGERPRINT_OK)
    return -2;

  result = finger.fingerFastSearch();

  if (result == FINGERPRINT_NOTFOUND)
    return -3;

  if (result != FINGERPRINT_OK)
    return -2;

  return finger.fingerID;
}


// --------------------------------------------------
// WAIT FOR FINGER REMOVAL
// --------------------------------------------------

void waitForFingerRemoval()
{
  while (finger.getImage() != FINGERPRINT_NOFINGER)
  {
    delay(50);
  }
}


// --------------------------------------------------
// RETURN TO STANDBY
// --------------------------------------------------

void returnToStandby()
{
  showClock(true);
  lastClockPaint = millis();
}


// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup()
{
  // USB Serial Monitor
  Serial.begin(115200);

  // ESP32 communication through D10
  espSerial.begin(9600);

  Wire.begin();


  // Find OLED address
  uint8_t oledAddress =
    i2cResponds(OLED_ADDRESS_FIRST)
      ? OLED_ADDRESS_FIRST
      : OLED_ADDRESS_SECOND;

  if (!i2cResponds(oledAddress))
  {
    while (true)
    {
      delay(100);
    }
  }

  oled.setI2CAddress(oledAddress << 1);

  oled.begin();

  oled.setFont(
    u8x8_font_chroma48medium8_r
  );

  showCenteredScreen(
    "ATTENDANCE",
    "INITIALIZING"
  );

  delay(700);


  // ------------------------------------------------
  // RTC
  // ------------------------------------------------

  if (!rtc.begin())
  {
    showCenteredScreen(
      "RTC ERROR",
      "CHECK CONNECTION"
    );

    while (true)
    {
      delay(100);
    }
  }

  if (SET_RTC_ON_UPLOAD || rtc.lostPower())
  {
    rtc.adjust(
      DateTime(
        F(__DATE__),
        F(__TIME__)
      )
    );
  }


  // ------------------------------------------------
  // FINGERPRINT SENSOR
  // ------------------------------------------------

  showCenteredScreen(
    "ATTENDANCE",
    "SYSTEM READY"
  );

  finger.begin(57600);

  if (!finger.verifyPassword())
  {
    showCenteredScreen(
      "SENSOR ERROR",
      "CHECK R307"
    );

    while (true)
    {
      delay(100);
    }
  }


  // ------------------------------------------------
  // SYSTEM READY
  // ------------------------------------------------

  Serial.println(
    "COLLEGE ATTENDANCE SYSTEM READY"
  );

  Serial.println(
    "Registered students:"
  );

  Serial.println(
    "ID 2  = 02 = Abhin"
  );

  Serial.println(
    "ID 3  = 03 = Abishek"
  );

  Serial.println(
    "ID 4  = 04 = Ajith"
  );

  Serial.println(
    "ID 5  = 05 = Akash"
  );

  Serial.println(
    "ID 6  = 06 = Anoop"
  );

  Serial.println(
    "ID 21 = 21 = Chandra"
  );

  Serial.println(
    "ID 24 = 24 = Dharineesh"
  );

  Serial.println(
    "ID 27 = 27 = Dinesh"
  );

  Serial.println(
    "ID 29 = 29 = Farseen"
  );

  Serial.println(
    "ID 47 = 47 = Jamshid"
  );

  Serial.println(
    "ESP32 UART: D10 -> GPIO16 @ 9600"
  );

  returnToStandby();
}


// --------------------------------------------------
// MAIN LOOP
// --------------------------------------------------

void loop()
{
  // Update clock every second
  if (millis() - lastClockPaint >= 1000)
  {
    lastClockPaint = millis();

    showClock(false);
  }


  int id = findFingerprint();


  // No finger
  if (id == -1)
  {
    return;
  }


  // Scan error
  if (id == -2)
  {
    showCenteredScreen(
      "SCAN ERROR",
      "PLEASE RETRY"
    );

    Serial.println("SCAN_ERROR");

    delay(1500);

    returnToStandby();

    return;
  }


  // Fingerprint not enrolled
  if (id == -3)
  {
    showCenteredScreen(
      "NOT REGISTERED",
      "FINGERPRINT",
      "CONTACT ADMIN"
    );

    Serial.println(
      "FINGERPRINT_NOT_ENROLLED"
    );

    delay(2200);

    waitForFingerRemoval();

    returnToStandby();

    return;
  }


  // ------------------------------------------------
  // Fingerprint is enrolled.
  // Check whether it is assigned to a student.
  // ------------------------------------------------

  const Person *person =
    findPerson((uint16_t)id);


  // Enrolled fingerprint but no student assignment
  if (person == nullptr ||
      id > MAX_FINGERPRINT_ID)
  {
    char idLine[17];

    snprintf(
      idLine,
      sizeof(idLine),
      "TEMPLATE ID: %d",
      id
    );

    showCenteredScreen(
      "ID NOT ASSIGNED",
      idLine,
      "CONTACT ADMIN"
    );

    Serial.print(
      "ID_NOT_ASSIGNED,"
    );

    Serial.println(id);

    delay(2500);

    waitForFingerRemoval();

    returnToStandby();

    return;
  }


  // ------------------------------------------------
  // STUDENT FOUND
  // ------------------------------------------------

  DateTime now = rtc.now();

  bool duplicate =
    lastDayFor((uint16_t)id)
    == dayNumber(now);


  // First attendance of the day
  if (!duplicate)
  {
    saveDay(
      (uint16_t)id,
      dayNumber(now)
    );
  }


  char studentLine[17];
  char dateLine[9];
  char timeLine[13];


  // Student ID + name
  snprintf(
    studentLine,
    sizeof(studentLine),
    "ID:%s %s",
    person->studentID,
    person->name
  );


  // DD/MM/YY
  snprintf(
    dateLine,
    sizeof(dateLine),
    "%02d/%02d/%02d",
    now.day(),
    now.month(),
    now.year() % 100
  );


  // 12-hour time
  formatTime12(
    now,
    timeLine,
    sizeof(timeLine)
  );


  // ------------------------------------------------
  // ATTENDANCE RESULT
  // ------------------------------------------------

  if (duplicate)
  {
    showCenteredScreen(
      "ALREADY MARKED",
      studentLine,
      dateLine,
      timeLine
    );
  }
  else
  {
    showCenteredScreen(
      "ATTENDANCE SAVED",
      studentLine,
      dateLine,
      timeLine
    );
  }


  // Send attendance event
  sendEvent(
    person->studentID,
    person->name,
    now,
    duplicate
      ? "ALREADY_REGISTERED"
      : "RECORDED"
  );


  // Keep result visible
  delay(2500);


  // Wait until finger is removed
  waitForFingerRemoval();


  // Return to standby
  returnToStandby();
}
