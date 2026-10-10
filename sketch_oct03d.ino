#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Firebase helper libraries
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// ==================================================
// WIFI
// ==================================================

#define WIFI_SSID       "realme 11x 5G"
#define WIFI_PASSWORD   "123123123"

// ==================================================
// FIREBASE
// ==================================================

#define API_KEY \
"AIzaSyCdjUZ7bG5MrmEYL2ApthGSEs8vMMcbFxA"

#define DATABASE_URL \
"https://biometric-attendance-sys-78b5e-default-rtdb.firebaseio.com/"

// Firebase Authentication account
#define USER_EMAIL       "akashvijai42@gmail.com"
#define USER_PASSWORD    "akashvijai2008@"


// ==================================================
// ESP32 UART
// Uno D10 -> ESP32 GPIO16
// ==================================================

#define UNO_RX 16

HardwareSerial UnoSerial(2);


// ==================================================
// FIREBASE OBJECTS
// ==================================================

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ==================================================
// SETUP
// ==================================================

void setup()
{
  Serial.begin(115200);

  // Uno communication
  UnoSerial.begin(
    9600,
    SERIAL_8N1,
    UNO_RX,
    -1
  );

  Serial.println();
  Serial.println("================================");
  Serial.println(" BIOMETRIC ATTENDANCE ESP32");
  Serial.println("================================");


  // ------------------------------------------------
  // WIFI
  // ------------------------------------------------

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


  // ------------------------------------------------
  // FIREBASE
  // ------------------------------------------------

  config.api_key = API_KEY;

  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback =
    tokenStatusCallback;

  Firebase.begin(
    &config,
    &auth
  );

  Firebase.reconnectWiFi(true);

  Serial.println("Connecting to Firebase...");

  unsigned long startTime = millis();

  while (!Firebase.ready() &&
         millis() - startTime < 15000)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();

  if (Firebase.ready())
  {
    Serial.println("Firebase Connected");
  }
  else
  {
    Serial.println("Firebase Connection Failed");
  }


  // ------------------------------------------------
  // READY
  // ------------------------------------------------

  Serial.println("--------------------------------");
  Serial.println("SYSTEM READY");
  Serial.println("Waiting for attendance...");
  Serial.println("--------------------------------");
}


// ==================================================
// SEND ATTENDANCE TO FIREBASE
// ==================================================

void sendAttendance(String data)
{
  /*
     Expected format:

     ATTEND,
     05,
     Akash,
     03/10/26,
     SATURDAY,
     09:25:41 PM,
     21:25:41,
     RECORDED
  */

  data.trim();

  if (!data.startsWith("ATTEND,"))
  {
    Serial.println("Invalid attendance data");
    return;
  }


  // ------------------------------------------------
  // Remove ATTEND,
  // ------------------------------------------------

  data.remove(0, 7);


  // ------------------------------------------------
  // Parse fields
  // ------------------------------------------------

  String fields[7];

  int fieldIndex = 0;
  int start = 0;

  for (int i = 0; i <= data.length(); i++)
  {
    if (i == data.length() ||
        data.charAt(i) == ',')
    {
      if (fieldIndex < 7)
      {
        fields[fieldIndex] =
          data.substring(start, i);
      }

      fieldIndex++;
      start = i + 1;
    }
  }


  if (fieldIndex != 7)
  {
    Serial.println("Invalid attendance format");
    return;
  }


  String studentID = fields[0];
  String name      = fields[1];
  String date      = fields[2];
  String day       = fields[3];
  String time12    = fields[4];
  String time24    = fields[5];
  String status    = fields[6];


  // ------------------------------------------------
  // Display received information
  // ------------------------------------------------

  Serial.println();
  Serial.println("========== ATTENDANCE ==========");

  Serial.print("Student ID : ");
  Serial.println(studentID);

  Serial.print("Name       : ");
  Serial.println(name);

  Serial.print("Date       : ");
  Serial.println(date);

  Serial.print("Day        : ");
  Serial.println(day);

  Serial.print("Time       : ");
  Serial.println(time12);

  Serial.print("Status     : ");
  Serial.println(status);


  // ------------------------------------------------
  // Firebase check
  // ------------------------------------------------

  if (!Firebase.ready())
  {
    Serial.println("Firebase not ready");
    Serial.println("Attendance NOT uploaded");
    return;
  }


  // ------------------------------------------------
  // Create Firebase record
  // ------------------------------------------------

  FirebaseJson json;

  json.set(
    "studentID",
    studentID
  );

  json.set(
    "name",
    name
  );

  json.set(
    "date",
    date
  );

  json.set(
    "day",
    day
  );

  json.set(
    "time",
    time12
  );

  json.set(
    "time24",
    time24
  );

  json.set(
    "status",
    status
  );


  // ------------------------------------------------
  // Push new attendance record
  // ------------------------------------------------

  if (Firebase.RTDB.pushJSON(
        &fbdo,
        "/attendance",
        &json))
  {
    Serial.println();
    Serial.println("Firebase Upload: SUCCESS");

    Serial.print("Record ID: ");
    Serial.println(
      fbdo.pushName()
    );
  }
  else
  {
    Serial.println();
    Serial.println("Firebase Upload: FAILED");

    Serial.println(
      fbdo.errorReason()
    );
  }

  Serial.println("================================");
}


// ==================================================
// MAIN LOOP
// ==================================================

void loop()
{
  // ------------------------------------------------
  // Read data from Arduino Uno
  // ------------------------------------------------

  if (UnoSerial.available())
  {
    String data =
      UnoSerial.readStringUntil('\n');

    data.trim();

    if (data.length() > 0)
    {
      Serial.print("UNO DATA: ");
      Serial.println(data);

      sendAttendance(data);
    }
  }


  // ------------------------------------------------
  // Maintain WiFi
  // ------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected");
    WiFi.reconnect();
  }
}
