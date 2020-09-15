
/*
  Copyright (c) 2019, SODAQ
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors
  may be used to endorse or promote products derived from this software without
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#include <Sodaq_R4X.h>
#include "sha256.h"


#define CONSOLE_STREAM   SerialUSB
#define MODEM_STREAM     Serial1


#define CURRENT_APN      "internet.m2m"
#define CURRENT_OPERATOR AUTOMATIC_OPERATOR
#define CURRENT_URAT     SODAQ_R4X_LTEM_URAT
#define CURRENT_MNO_PROFILE MNOProfiles::STANDARD_EUROPE

#define HTTP_HOST        "webhook.site"
#define HTTP_PORT        80
#define HTTP_QUERY       "/42f32f7e-f826-4553-8ec6-27db77d44aff"

static Sodaq_R4X r4x;
static Sodaq_SARA_R4XX_OnOff saraR4xxOnOff;
static bool isReady;

#ifndef NBIOT_BANDMASK
#define NBIOT_BANDMASK BAND_MASK_UNCHANGED
#endif

#define MAX_ALLOWED_BUFFER 512

void setup()
{
  while ((!CONSOLE_STREAM) && (millis() < 10000)) {
    // Wait max 10 sec for the CONSOLE_STREAM to open
  }

  CONSOLE_STREAM.begin(115200);
  MODEM_STREAM.begin(r4x.getDefaultBaudrate());

  r4x.setDiag(CONSOLE_STREAM);
  r4x.init(&saraR4xxOnOff, MODEM_STREAM);

  isReady = r4x.connect(CURRENT_APN, CURRENT_URAT, CURRENT_MNO_PROFILE, CURRENT_OPERATOR, BAND_MASK_UNCHANGED, NBIOT_BANDMASK);
  CONSOLE_STREAM.println(isReady ? "Network connected" : "Network connection failed");

  CONSOLE_STREAM.println("Setup done");
  if (isReady) {
    postHTTP();
  }
}

void loop()
{
  if (CONSOLE_STREAM.available()) {
    int i = CONSOLE_STREAM.read();
    CONSOLE_STREAM.write(i);
    MODEM_STREAM.write(i);
  }

  if (MODEM_STREAM.available()) {
    CONSOLE_STREAM.write(MODEM_STREAM.read());
  }
}

void postHTTP()
{
  char buffer[2048];
  //Prepare send buffer
  String message = "{\"bn\":\"urn:dev:IMEI:356726104408422:\"},{\"n\": \"temperature\", \"v\": 20.5, \"u\": \"Cel\"}, (number record)"; 
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
  
  String toBeHashed = message + sharedSecret;
  CONSOLE_STREAM.print("Sring to hash:\n");
  CONSOLE_STREAM.print(toBeHashed);  
  CONSOLE_STREAM.print("\n");  
  uint8_t *hash;
  Sha256.init();
  Sha256.print(toBeHashed);
  uint8_t * result = Sha256.result();
  String token = "";
  CONSOLE_STREAM.print("Hash:\n");

  for (int i = 0; i < 32; i++) {
          token = token + "0123456789abcdef"[result[i] >> 4];
          token = token + "0123456789abcdef"[result[i] & 0xf];
  }
  CONSOLE_STREAM.print(token);
  CONSOLE_STREAM.print("\n");  
  
  char tokenBuffer[token.length() + 1];
  token.toCharArray(tokenBuffer, token.length() + 1);   
  
  r4x.httpClear();
  r4x.httpSetCustomHeader(0, "content-type", "application/json");
  r4x.httpSetCustomHeader(1 , "Things-Message-Token", tokenBuffer);  
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
