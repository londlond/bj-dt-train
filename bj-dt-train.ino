 #include <UIPEthernet.h>

byte mac[] = { 0x54, 0x34, 0x41, 0x3f, 0x4d, 0x31 };

IPAddress ip(192, 168, 0, 111);
EthernetServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("WEB SERVER ");
  Serial.println();
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  EthernetClient client = server.available();
  if (client) {
    Serial.println("-> New Connection");
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      Serial.println("connected");
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        if (c == '\n' && currentLineIsBlank) {
          client.println("HTTP/1.1 200 OK"); //send new page
          client.println("Content-Type: text/html");
          client.println();
          client.println(F("<html><head></head><body>"));
          client.println(F("<h1>M-Control</h1>"));
          client.println(F("<h3>Current Config</h3>"));
          client.println(F("<table><thead><tr><th>Config</th><th>Value</th><th>Description</th></tr></thead><tbody><tr><td>Range Normal</td><td>RANGE_NORMAL</td><td>if &lt; than, trigger<br></td></tr><tr><td>Range Normal Back</td><td>RANGE_NORMAL_BACK</td><td>if &gt; than, next trigger possible</td></tr><tr><td>Range DT</td><td>RANGE_DT</td><td>if &lt; than, DT counted</td></tr><tr><td>DT Hold</td><td>DT_HOLD</td><td>but only if hold long enough</td></tr><tr><td>Precum Steps</td><td>STEPS_PRECUM</td><td>Settings for Stepper, simulates precum</td></tr><tr><td>Precum Trigger</td><td>PRECUM_TRIGGER</td><td>Detections needed to trigger<br></td></tr><tr><td>Cum Normal<br></td><td>CUM_NORMAL</td><td>Normal detections needed to trigger</td></tr><tr><td>Cum Steps</td><td>STEPS_CUM</td><td>Settings for Stepper, simulates ejak</td></tr><tr><td>Cum Steps DT</td><td>STEPS_CUM_DT</td><td>DT detections needed to trigger</td></tr></tbody></table>"));
          break;  
        }
      }
    }
  }
  delay(1);
  client.stop();
  Serial.println("client disconnected");
}
