/*
  WiFiEsp test: ClientTest
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI


// Including necessary libraries
#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>


// Wi-Fi credentials and server details
#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.14.61"
#define SERVER_PORT 5000
#define LOGID "KSH_ARD"
#define PASSWD "PASSWD"



// Pin definitions
#define CDS_PIN A0
#define BUTTON_PIN 2
#define LED_LAMP_PIN 3
#define DHTPIN 4
#define SERVO_PIN 5
#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define LED_BUILTIN_PIN 13

#define waterLed 12         // LED 디지털 12번 포트 연결 선언


#define waterAnalogPin A2   // 워터센서 analog port A2 연결 선언




// Constants for command and array sizes
#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11
bool timerIsrFlag = false;
boolean lastButton = LOW;     // 버튼의 이전 상태 저장
boolean currentButton = LOW;    // 버튼의 현재 상태 저장
boolean ledOn = false;      // LED의 현재 상태 (on/off)
boolean cdsFlag = false;
// Buffer for sending data
char sendBuf[CMD_SIZE];


// LCD display lines
char lcdLine1[17] = "Smart IoT By KSH";
char lcdLine2[17] = "WiFi Connecting!";



// Initialize sensor variables
int cds = 0;
unsigned int secCount;
unsigned int myservoTime = 0;
int mappedWaterValue=0;


// Variables for storing sensor data
char getSensorId[10];
int sensorTime;
float temp = 0.0;
float humi = 0.0;
bool updatTimeFlag = false;




// Structure for storing date and time
typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;



DATETIME dateTime = {0, 0, 0, 12, 0, 0};

// Create DHT object
DHT dht(DHTPIN, DHTTYPE);


// Create a SoftwareSerial object for communication with the ESP8266
SoftwareSerial wifiSerial(WIFIRX, WIFITX);

// Create a Wi-Fi client object
WiFiEspClient client;


// Create a LiquidCrystal object for the I2C LCD display
LiquidCrystal_I2C lcd(0x27, 16, 2);


// Create a Servo object
Servo myservo;



// Setup function
void setup() {




  pinMode(waterLed, OUTPUT); 

  // Initialize the LCD display
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);


  // Set pin modes for various components
  pinMode(CDS_PIN, INPUT);    // 조도 핀을 입력으로 설정 (생략 가능)
  pinMode(BUTTON_PIN, INPUT);    // 버튼 핀을 입력으로 설정 (생략 가능)
  pinMode(LED_LAMP_PIN, OUTPUT);    // LED 핀을 출력으로 설정
  pinMode(LED_BUILTIN_PIN, OUTPUT); //D13





// Initialize Serial communication for debugging
#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif

  // Setup Wi-Fi and server connection
  wifi_Setup();


  // Set up a timer to trigger the timerIsr function every 1000ms (1 second)
  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();



  // Attach the servo to its pin and initialize it at 0 degrees
  myservo.attach(SERVO_PIN);
  myservo.write(0);
  myservoTime = secCount;


  // Initialize the DHT sensor
  dht.begin();
}

void loop() {


  dowaterSensor();


  // Check if data is available from the server
  if (client.available()) {
    socketEvent();
  }
  // Execute code every second
  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;

      // Execute code every 5 seconds
    if (!(secCount % 5)) //5초에 한번씩 실행
    {
      // Read and process CDS sensor data
      cds = map(analogRead(CDS_PIN), 0, 1023, 0, 100);
      if ((cds >= 50) && cdsFlag)
      {
        // Send CDS data to the server when threshold is crossed
        cdsFlag = false;
        sprintf(sendBuf, "[%s]CDS@%d\n", LOGID, cds);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        //        digitalWrite(LED_BUILTIN_PIN, HIGH);     // LED 상태 변경
      } else if ((cds < 50) && !cdsFlag)
      // Send CDS data to the server when threshold is crossed
      {
        cdsFlag = true;
        sprintf(sendBuf, "[%s]CDS@%d\n", LOGID, cds);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        //        digitalWrite(LED_BUILTIN_PIN, LOW);     // LED 상태 변경
      }


      // Read humidity and temperature from DHT sensor
      humi = dht.readHumidity();
      temp = dht.readTemperature();
#ifdef DEBUG
      /*      Serial.print("Cds: ");
            Serial.print(cds);
            Serial.print(" Humidity: ");
            Serial.print(humi);
            Serial.print(" Temperature: ");
            Serial.println(temp);
      */
#endif


      // Check if the client is still connected
      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect();
      }
    }
    // Detach servo if it's attached and more than 2 seconds have passed
    if (myservo.attached() && myservoTime + 2 == secCount)
    {
      myservo.detach();
    }

    
    // Send sensor data if the sensorTime interval has passed
    if (sensorTime != 0 && !(secCount % sensorTime ))
    {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi,int(mappedWaterValue));
      /*      char tempStr[5];
            char humiStr[5];
            dtostrf(humi, 4, 1, humiStr);  //50.0 4:전체자리수,1:소수이하 자리수
            dtostrf(temp, 4, 1, tempStr);  //25.1
            sprintf(sendBuf,"[%s]SENSOR@%d@%s@%s\r\n",getSensorId,cdsValue,tempStr,humiStr);
      */
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
    }


    // Update the time display on the LCD
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec );
    lcdDisplay(0, 0, lcdLine1);
    
    
    
    // Send a request for time update if necessary
    if (updatTimeFlag)
    {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }


  
  }



  // Debounce the button and handle LED control
  currentButton = debounce(lastButton);   // 디바운싱된 버튼 상태 읽기
  if (lastButton == LOW && currentButton == HIGH)  // 버튼을 누르면...
  {
    ledOn = !ledOn;       // LED 상태 값 반전
    digitalWrite(LED_BUILTIN_PIN, ledOn);     // LED 상태 변경
    //    sprintf(sendBuf,"[%s]BUTTON@%s\n",LOGID,ledOn?"ON":"OFF");


    // Send LED state to the server
    sprintf(sendBuf, "[HM_CON]GAS%s\n", ledOn ? "ON" : "OFF");
    client.write(sendBuf, strlen(sendBuf));
    client.flush();
  }
  lastButton = currentButton;     // 이전 버튼 상태를 현재 버튼 상태로 설정

}


// Function to handle socket events
void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;



  // Clear sendBuf and read incoming data into recvBuf
  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif





  // Tokenize the received data into an array using delimiter "[@]"
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //[KSH_ARD]LED@ON : pArray[0] = "KSH_ARD", pArray[1] = "LED", pArray[2] = "ON"
  
    // Handle various commands based on the first token
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
        // Display command details on LCD if it fits
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }
  // Handle "New Connected" command
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  // Display server connection status
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    strcpy(lcdLine2, "Server Connected");
    lcdDisplay(0, 1, lcdLine2);
    updatTimeFlag = true;
    return ;
  }
   // Handle "Already logged" command
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  // Stop the client and reconnect to the server
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return ;
  }
  // Handle LED control command
  else if (!strcmp(pArray[1], "LED")) {
    // Change LED status based on command ("ON" or "OFF")
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_BUILTIN_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_BUILTIN_PIN, LOW);
    }
    // Prepare response and send it back to the server
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }
  
    // Handle lamp control command
   else if (!strcmp(pArray[1], "LAMP")) {
     // Change lamp status based on command ("ON" or "OFF")
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_LAMP_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_LAMP_PIN, LOW);
    }
    // Prepare response and send it back to the server
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }
  
    // Handle "GETSTATE LED" command
   else if (!strcmp(pArray[1], "GETSTATE")) {
    if (!strcmp(pArray[2], "LED")) {
      // Respond with the current LED state ("ON" or "OFF")
      sprintf(sendBuf, "[%s]LED@%s\n", pArray[0], digitalRead(LED_BUILTIN_PIN) ? "ON" : "OFF");
    }
  }



  
  // Handle SERVO control command
  else if (!strcmp(pArray[1], "SERVO"))
  {

    // Attach the servo and move it based on command ("ON" or "OFF")
    myservo.attach(SERVO_PIN);
    myservoTime = secCount;
    if (!strcmp(pArray[2], "ON"))
      myservo.write(180);
    else
      myservo.write(0);
      
    // Prepare response and send it back to the server
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);

  }





    // Handle GETSENSOR command
  else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
    if (pArray[2] != NULL) {
      // Set sensor time interval and ID if provided
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      // Respond with sensor data (CDS, temperature, humidity)
      sensorTime = 0;
      //sprintf(sendBuf, "[%s]%s@%d@%d@%d\n", pArray[0], pArray[1], cds, (int)temp, (int)humi);
      //sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi,int(mappedWaterValue));
      
      sprintf(sendBuf, "%d@%d@%d@%d\r\n", cds, (int)temp, (int)humi,int(mappedWaterValue));
      lcdDisplay(0, 1, sendBuf);
      
    
    }
  }
  // Handle GETTIME command
  else if(!strcmp(pArray[0],"GETTIME")) {  //GETTIME
  // Update the device's date and time
    dateTime.year = (pArray[1][0]-0x30) * 10 + pArray[1][1]-0x30 ;
    dateTime.month =  (pArray[1][3]-0x30) * 10 + pArray[1][4]-0x30 ;
    dateTime.day =  (pArray[1][6]-0x30) * 10 + pArray[1][7]-0x30 ;
    dateTime.hour = (pArray[1][9]-0x30) * 10 + pArray[1][10]-0x30 ;
    dateTime.min =  (pArray[1][12]-0x30) * 10 + pArray[1][13]-0x30 ;
    dateTime.sec =  (pArray[1][15]-0x30) * 10 + pArray[1][16]-0x30 ;
#ifdef DEBUG
// Debug print the updated time
    sprintf(sendBuf,"\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r",dateTime.year,dateTime.month,dateTime.day,dateTime.hour,dateTime.min,dateTime.sec );
    Serial.println(sendBuf);
#endif
    return;
  } 
  else
    return;
  // Send the response back to the server
  client.write(sendBuf, strlen(sendBuf));
  client.flush();

#ifdef DEBUG
// Debug print the sent response
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}

// Timer interrupt function
void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime);
}

// Function to calculate the clock
void clock_calc(DATETIME *dateTime)
{
  int ret = 0;
  dateTime->sec++;          // increment second

  if(dateTime->sec >= 60)                              // if second = 60, second = 0
  { 
      dateTime->sec = 0;
      dateTime->min++; 
             
      if(dateTime->min >= 60)                          // if minute = 60, minute = 0
      { 
          dateTime->min = 0;
          dateTime->hour++;                               // increment hour
          if(dateTime->hour == 24) 
          {
            dateTime->hour = 0;
            updatTimeFlag = true;
          }
       }
    }
}
// Wi-Fi setup function
void wifi_Setup() {
  wifiSerial.begin(19200);
  wifi_Init();
  server_Connect();
}

// Wi-Fi initialization function
// Wi-Fi initialization function
void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    }
    else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }

    // Display device information on the LCD
  sprintf(lcdLine1, "ID:%s", LOGID);
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}


// Function to connect to the server
int server_Connect()
{
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");// Send authentication data to server
  }
  else
  {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}


// Function to print Wi-Fi status information
void printWifiStatus()
{
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}



// Function to display text on the LCD
void lcdDisplay(int x, int y, char * str)
{
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}


// Function for button debouncing
boolean debounce(boolean last)
{
  boolean current = digitalRead(BUTTON_PIN);  // 버튼 상태 읽기
  if (last != current)      // 이전 상태와 현재 상태가 다르면...
  {
    delay(5);         // 5ms 대기
    current = digitalRead(BUTTON_PIN);  // 버튼 상태 다시 읽기
  }
  return current;       // 버튼의 현재 상태 반환
}




void dowaterSensor()
{
  int waterAnalogPinVal = analogRead(waterAnalogPin);   // analogPin 의 변화값(전류값)을 읽음
   // 센서 값의 범위를 변환 범위로 매핑
  mappedWaterValue = map(waterAnalogPinVal, 400, 800, 0, 700);
  
 
  if (waterAnalogPinVal > 100)                 // val 값이 100이 넘으면 (전류가 100이 넘으면)
  {                               
      digitalWrite(waterLed, HIGH);   // LED ON
  }
  else                           // val 값이 100이하면 (전류가 100이하면)
  {
      digitalWrite(waterLed, LOW);    // LED OFF
  }
  
  //Serial.println(mappedWaterValue);      // 시리얼모니터에 전류값 표시
  //delay (500);
}
