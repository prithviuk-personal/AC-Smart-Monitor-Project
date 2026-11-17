#define BLYNK_TEMPLATE_ID "TMPL2FevEsPoZ"
#define BLYNK_TEMPLATE_NAME "UART ESP"
#define BLYNK_AUTH_TOKEN "nVn8xo1H2wtIKs78lHjPSDezl-okEUXe"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "Get it while its hotspot";
char pass[] = "parum123";

#define RXD2 16
#define TXD2 17

// Virtual pins
#define VPIN_RAW    V0   // full UART line (text / terminal widget)
#define VPIN_VRMS   V1   // Vrms in volts   (e.g. 14.03)
#define VPIN_IPEAK  V2   // Ipeak in mA     (e.g. 754)
#define VPIN_POWER  V3   // Power in watts  (e.g. 7.43)

String rxLine = "";

// Finds `label` in `line` and parses the number straight after it.
// "Vrms: 14.03 V | ..." with label "Vrms:" -> 14.03
// toFloat() skips leading spaces and stops at the first non-numeric char,
// so the trailing " V", " mA", " W" units are ignored automatically.
bool parseField(const String &line, const char *label, float &out) {
  int idx = line.indexOf(label);
  if (idx < 0) return false;
  out = line.substring(idx + strlen(label)).toFloat();
  return true;
}

void handleLine(const String &line) {
  Serial.println(line);
  Blynk.virtualWrite(VPIN_RAW, line);

  // Only measurement lines get graphed; boot banner and ERROR lines are skipped.
  if (!line.startsWith("Vrms:")) return;

  float vrms, ipeak, power;
  if (parseField(line, "Vrms:",  vrms)  &&
      parseField(line, "Ipeak:", ipeak) &&
      parseField(line, "Power:", power)) {

    Blynk.virtualWrite(VPIN_VRMS,  vrms);
    Blynk.virtualWrite(VPIN_IPEAK, (int)ipeak);   // already whole mA
    Blynk.virtualWrite(VPIN_POWER, power);

    Serial.printf("  -> V=%.2f  I=%d mA  P=%.2f\n", vrms, (int)ipeak, power);
  }
}

void setup() {
  Serial.begin(115200);                          // USB debug
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);   // UART from ATmega
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();

  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      rxLine.trim();
      if (rxLine.length() > 0) handleLine(rxLine);
      rxLine = "";
    } else if (c != '\r') {
      if (rxLine.length() < 96) rxLine += c;   // full line is ~55 chars, 64 was cutting it close
    }
  }
}
