

//#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include "CheapStepper.h"
#include <Stream.h>

ESP8266WebServer server(80);
const char ssid[] = "FAST_Ding";
const char password[] = "zuozuoyouyou";

const int motorPin1 = 16;//0;    // Blue   - In 1
const int motorPin2 = 5;//2;    // Pink   - In 2
const int motorPin3 = 4;    // Yellow - In 3
const int motorPin4 = 2;//5;    // Orange - In 4
// Red    - pin 5 (VCC)

const int motorPin5 = 14;//12;    // Blue   - In 1
const int motorPin6 = 12;//13;    // Pink   - In 2
const int motorPin7 = 13;//14;    // Yellow - In 3
const int motorPin8 = 15;//16;    // Orange - In 4

const int stepsPerRevolution = 4076;
const int s100mm2steps = int(100 / (3.14 * 40.5) * stepsPerRevolution*1.14);
const int s45degreemm2steps = int(80 / (8 * 40.5) * stepsPerRevolution*1.14);
const int defaultContinueRunSteps = 2;


// initialize the stepper library on pins 8 through 11 -> IN1, IN2, IN3, IN4
// as shown above, we need to exchange wire 2&3, which we do in the constructor
CheapStepper leftStepper(motorPin1, motorPin2, motorPin3, motorPin4);
CheapStepper rightStepper(motorPin5, motorPin6, motorPin7, motorPin8);

bool moveClockwise = true;
unsigned long moveStartTime = 0; // this will save the time (millis()) when we started each new move


bool ota_flag = true;
uint16_t time_elapsed = 0;


int steps;
int actionFlag = 0xFF;
int actionFlagOld;
enum { FW, BW, TL, TR, ST, STP };
void (*pFunction)(int);

// define functions for basic movement
// using 'for' to move steppers (pseudo)simultaniously

void goForward(long x) {
  leftStepper.newMove(1, x);
  rightStepper.newMove(0, x);
}

void goForward_nx100mm(int x) {
  long totalSteps = x * s100mm2steps;
  Serial.print("goForward_nx100mm..."); Serial.println(x);
  goForward(totalSteps);
}


void goBackward(long x) {
  leftStepper.newMove(0, x);
  rightStepper.newMove(1, x);
}

void goBackward_nx100mm(int x) {
  long totalSteps = x * s100mm2steps;
  Serial.print("goBackward_nx100mm..."); Serial.println(x);
  goBackward(totalSteps);
}

//turning left and right standing at the same place - with both motors
void turnLeft(long x) {
  leftStepper.newMove(0, x);
  rightStepper.newMove(0, x);
}

void turnLeft_nx45degree(int x) {
  long totalSteps = x * s45degreemm2steps;
  Serial.print("turnLeft..."); Serial.println(x);
  turnLeft(totalSteps);
}

void turnRight(long x) {
  leftStepper.newMove(1, x);
  rightStepper.newMove(1, x);
}

void turnRight_nx45degree(int x) {
  long totalSteps = x * s45degreemm2steps;
  Serial.print("turnRight..."); Serial.println(x);
  turnRight(totalSteps);
}


void tankStop( ) {
  leftStepper.stop();
  rightStepper.stop();
}

bool isFirstStepsAction;
int totalActionsofSteps = 0;
String actionString[10];
int num_x_Action = 0;

void divideActionString(String str)
{
  int p = 0, q = 0, k = 0, m = 0;
  for (int i = 0; i < str.length(); i++)
    if ((str[i]) == 'F' || (str[i]) == 'B' || (str[i]) == 'L' || (str[i]) == 'R')
    {
      q = p; p = i; k++;
      if (k == 1)
      {      
        actionString[m] = str.substring(q, p);
        k = 0; m++;
      }
    }
  actionString[m] = str.substring(p);

  totalActionsofSteps = m;
  isFirstStepsAction = 1;
  num_x_Action = 0;
  if(m!=0)actionFlag=STP;
}



void processStepsAction(String _str )
{ //actionFlag=STP;
  { steps =(_str.substring(1)).toInt();

    switch (_str[0])
    {
      case 'F':
        pFunction = goForward_nx100mm;
        server.send(200, "text/html", "steps...forward");
        break;
      case 'B':
        pFunction = goBackward_nx100mm;
        server.send(200, "text/html", "steps...backward");
        break;
      case 'L':
        pFunction = turnLeft_nx45degree;
        break;
      case 'R':
        pFunction = turnRight_nx45degree;
        break;
      default:
        break;
    }
  }
}


void enableWebServer()
{
  HTTPMethod getMethod = HTTP_GET;

  server.on("/restart", []() {
    server.send(200, "text/html", "restarting...");
    delay(1000);
    ESP.restart();
  });
  server.on("/setflag", []() {
    server.send(200, "text/html", "Setting flag...");
    ota_flag = true;
    time_elapsed = 0;
  });
  server.on("/forward", getMethod, []() {
    actionFlag = FW;
    server.send(200, "text/html", "forward");
  });
  server.on("/backward", getMethod, []() {
    actionFlag = BW;
    server.send(200, "text/html", "backward");
  });
  server.on("/turnleft", getMethod, []() {
    actionFlag = TL;
    server.send(200, "text/html", "turnleft");
  });
  server.on("/turnright", getMethod, []() {
    actionFlag = TR;
    server.send(200, "text/html", "turnright");
  });
  server.on("/stop", getMethod, []() {
    actionFlag = ST;
    server.send(200, "text/html", "stop");
  });

  server.on("/steps", getMethod, [] { String s_action = server.arg("action");   if (s_action != "") divideActionString(s_action);server.send(200, "text/html", "steps");});
  // server.on("/", getMethod, judgeAction);

  server.begin();
}

//void judgeAction()
//{
//String action = server.arg("action");
//int n_action=action.toInt();
//String s_steps=server.arg("steps");
// if (s_steps != "") steps=s_steps.toInt();
// else steps=0xFF;
//  if(n_action==1)    actionFlag=FW;
//  else if(n_action==2)  actionFlag=BW;
//  else if(n_action==3)  actionFlag=TL;
//  else if(n_action==4) actionFlag=TR;
//  else if(n_action==0)   actionFlag=ST;
//   }

void setup() {
  if (ota_flag)
  {
    uint16_t time_start = millis();
    while (time_elapsed < 15000)
    {
      ArduinoOTA.handle();
      time_elapsed = millis() - time_start;
      delay(10);
    }
    ota_flag = false;
  }
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  //Serial.begin(115200);
  Serial.println("Tank Start");

  //  WiFi.softAP(AP_NameChar, WiFiAPPSK);
  //  IPAddress myIP = WiFi.softAPIP();
  // leftStepper.setSpeed(60);rightStepper.setSpeed(60);
  enableWebServer();

}

void loop() {
  ArduinoOTA.handle();
  // put your main code here, to run repeatedly:
  server.handleClient();

  if (isFirstStepsAction)
  { processStepsAction(actionString[0]);
    isFirstStepsAction = 0;
    num_x_Action = 1;
  }

  if (actionFlag != actionFlagOld) {
    switch (actionFlag)
    {
      case FW:
        //if(steps!=0xFF) {goForward(steps);actionFlag=0xFF;}
        //else
        goForward(defaultContinueRunSteps);
        break;

      case BW:
        //if(steps!=0xFF) {goBack(steps);actionFlag=0xFF;}
        // else
        goBackward(defaultContinueRunSteps);
        break;
      case TL:
        turnLeft(defaultContinueRunSteps );
        break;

      case TR:
        turnRight(defaultContinueRunSteps );
        break;

      case ST:
        tankStop();
        break;

      case STP:
      if (num_x_Action==0) pFunction(steps);
          break;

      default:
        break;
    }
    actionFlagOld = actionFlag;
  }
  int stepsLeft_L = leftStepper.getStepsLeft();

  // if the current move is done...

  if (stepsLeft_L == 0) {
    switch (actionFlag)
    {
      case FW:
        //if(steps!=0xFF) {goForward(steps);actionFlag=0xFF;}
        goForward(defaultContinueRunSteps );
        break;

      case BW:
        // if(steps!=0xFF) {goBack(steps);actionFlag=0xFF;}
        goBackward(defaultContinueRunSteps);
        break;
      case TL:
        turnLeft(defaultContinueRunSteps);
        break;

      case TR:
        turnRight(defaultContinueRunSteps );
        break;

      case STP:
         if (num_x_Action <= totalActionsofSteps)
        {
          processStepsAction(actionString[num_x_Action]);
          pFunction(steps);
          num_x_Action++;
        }
        else actionFlag = ST;
        break;

      default:
        break;
    }
  }


  //    unsigned long timeTook = millis() - moveStartTime; // calculate time elapsed since move start
  //    Serial.print("move took (ms): "); Serial.print(timeTook);
  //    Serial.println(); Serial.println();
  //
  //    // let's start a new move in the reverse direction
  //
  //    moveClockwise = !moveClockwise; // reverse direction
  //    stepper.newMoveDegrees (moveClockwise, 180); // move 180 degrees from current position
  //    moveStartTime = millis(); // reset move start time




  delay(5);

  leftStepper.run();
  rightStepper.run();
}
