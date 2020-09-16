#include <Sodaq_R4X.h>
#include <Sodaq_wdt.h>
#include <Sodaq_LSM303AGR.h>
#include "sha256.h"

#define CONSOLE_STREAM   SerialUSB
#define MODEM_STREAM     Serial1

#define CURRENT_APN      "kpnthings2.m2m"
#define CURRENT_OPERATOR AUTOMATIC_OPERATOR
#define CURRENT_URAT     SODAQ_R4X_LTEM_URAT
#define CURRENT_MNO_PROFILE MNOProfiles::STANDARD_EUROPE

#define HTTP_HOST        "10.151.236.157"
#define HTTP_PORT        80
#define HTTP_QUERY       "/ingestion/m2m/senml/v1"

static Sodaq_R4X r4x;
static Sodaq_LSM303AGR AccMeter;
static Sodaq_SARA_R4XX_OnOff saraR4xxOnOff;
static bool isReady;
static bool isOff;

#ifndef NBIOT_BANDMASK
#define NBIOT_BANDMASK BAND_MASK_UNCHANGED
#endif

#define MAX_ALLOWED_BUFFER 512
#define STARTUP_DELAY 5000

unsigned long previousMillis = 0;
const long interval = 60000;

void setup()
{
  sodaq_wdt_safe_delay(STARTUP_DELAY);

  Wire.begin();

  AccMeter.rebootAccelerometer();
  delay(1000);

  // Enable the Accelerometer
  AccMeter.enableAccelerometer();

  while ((!CONSOLE_STREAM) && (millis() < 10000)) {
    // Wait max 10 sec for the CONSOLE_STREAM to open
  }

  CONSOLE_STREAM.begin(115200);
  MODEM_STREAM.begin(r4x.getDefaultBaudrate());

  r4x.setDiag(CONSOLE_STREAM);

}

void loop()
{
  r4x.init(&saraR4xxOnOff, MODEM_STREAM);

  isReady = r4x.connect(CURRENT_APN, CURRENT_URAT, CURRENT_MNO_PROFILE, CURRENT_OPERATOR, BAND_MASK_UNCHANGED, NBIOT_BANDMASK);
  CONSOLE_STREAM.println(isReady ? "Network connected" : "Network connection failed");

  CONSOLE_STREAM.println("Setup done");
  if (isReady) {
    postHTTP();
  }
  isOff = r4x.off();
  CONSOLE_STREAM.println(isOff ? "Modem off" : "Power off failed");
  sodaq_wdt_safe_delay(60000);
  
}

int8_t getBoardTemperature()
{
  int8_t temp = AccMeter.getTemperature();
  return temp;
}

String hashMessage(String toBeHashed) {
  uint8_t *hash;
  Sha256.init();
  Sha256.print(toBeHashed);
  uint8_t * result = Sha256.result();
  String token = "";
  for (int i = 0; i < 32; i++) {
    token = token + "0123456789abcdef"[result[i] >> 4];
    token = token + "0123456789abcdef"[result[i] & 0xf];
  }

  return token;
}

void postHTTP()
{

  char buffer[2048] = "/0";

  //Prepare message
  String message = "[{\"bn\":\"urn:dev:IMEI:356726104408422:\"},{\"n\": \"temperature\", \"v\":" + String(getBoardTemperature()) + ", \"u\": \"Cel\"}]";

  //Convert message string to Char Array
  char sendbuffer[message.length() + 1];
  message.toCharArray(sendbuffer, message.length() + 1);
  CONSOLE_STREAM.print("Body:");
  CONSOLE_STREAM.print(message);
  CONSOLE_STREAM.print("\n");

  //Prepare messageToken
  String sharedSecret = "72J3wD2bd49jnMI8S31j2L3vZldH3rSS";
  CONSOLE_STREAM.print("sharedSecret:\n");
  CONSOLE_STREAM.print(sharedSecret);
  CONSOLE_STREAM.print("\n");

  String toBeHashed = message + "\0" + sharedSecret;
  CONSOLE_STREAM.print("String to hash:\n");
  CONSOLE_STREAM.print(toBeHashed);
  CONSOLE_STREAM.print("\n");

  //Hash message
  String token = hashMessage(toBeHashed);

  CONSOLE_STREAM.print("Hash:\n");
  CONSOLE_STREAM.print(token);
  CONSOLE_STREAM.print("\n");

  //Convert token string to Char array
  char tokenBuffer[token.length() + 1];
  token.toCharArray(tokenBuffer, token.length() + 1);

  CONSOLE_STREAM.print("Sending message \n");

  r4x.httpClear();
  r4x.httpSetCustomHeader(0, "content-type", "application/json");
  r4x.httpSetCustomHeader(1 , "Things-Message-Token", tokenBuffer);
  r4x.httpSetCustomHeader(2 , "host", "m.m");
  char i =  r4x.httpRequest(HTTP_HOST, HTTP_PORT, HTTP_QUERY,
                            HttpRequestTypes::POST,
                            buffer,   sizeof(buffer),
                            sendbuffer, sizeof(sendbuffer));


  CONSOLE_STREAM.print("Read bytes: ");
  CONSOLE_STREAM.println(i);

  if (i > 0) {
    buffer[i] = 0;
    CONSOLE_STREAM.println("Buffer:");
    CONSOLE_STREAM.println(buffer);
  }
}
