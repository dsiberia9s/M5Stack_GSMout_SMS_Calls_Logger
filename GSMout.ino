#include <M5Stack.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include "FS.h"

#define RX_PIN  16
#define TX_PIN  17
#define RESET_PIN 5

WiFiUDP udp;
NTPClient ntp(udp, 7 * 3600);
WebServer web(80);

char * s2c(String s) {
  char * t_ = new char[s.length() + 1];
  strcpy(t_, s.c_str());
  char * t = t_;
  delete t_;
  return t;
}

String parseString(int idSeparator, char separator, String str) { 
  String output = "";
  int separatorCout = 0;
  for (int i = 0; i < str.length(); i++) {
    if ((char)str[i] == separator) {
      separatorCout++;
    } else {
      if (separatorCout == idSeparator) {
        output += (char)str[i];
      } else if (separatorCout > idSeparator) {
        break;
      }
    }
  }
  return output;
}

// сколько раз подстрока встречается в строке
int strstrcnt(char * t, char * w) {
  char * q = t;
  int n = 0;
  while ((q = strstr(q, w)) != NULL) {
   n++;
   q++;
  }
  return n;
}

// удаляет символ из строки
String rchar(String s, char c) {
  String t = "";
  for (int i = 0; i < s.length(); i++) {
    if (s[i] != c)
      t += (char)s[i];
  }
  return t;
}

void debug(String s) {
  M5.Lcd.fillScreen(0);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print(s);
}

char * WiFiAuto(int timeout = 5000) {
  timeout = (timeout < 1000) ? 1000 : timeout;
  File file;
  char * WiFiSSID;
  char * WiFiPswd;
  const char * path;
  bool r = false;
  WiFi.disconnect(true);
  delay(100);
  for (int i = 0; i < WiFi.scanNetworks(); i++) {
    File root = SPIFFS.open("/");
    file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String WiFiSSID_ = WiFi.SSID(i);
        WiFiSSID = new char[WiFiSSID_.length() + 1];
        strcpy(WiFiSSID, WiFiSSID_.c_str());
        path = file.name();
        if ((strstr(path, WiFiSSID) != NULL) && strstr(file.name(), ".wifi")) {
          r = true;
        }
        if (r) break;
      }
      file = root.openNextFile();
    }
    root.close();
    if (r) break;
  }
  if (r) {
    file = SPIFFS.open(path);
    String WiFiPswd_ = "";
    while (file.available()){
      char ch = file.read();
      if (ch == 0x0A) break;
      WiFiPswd_ += ch;
    }
    file.close();
    WiFiPswd = new char[WiFiPswd_.length() + 1];
    strcpy(WiFiPswd, WiFiPswd_.c_str());
    WiFi.begin((char *)WiFiSSID, (char *)WiFiPswd);
    delete WiFiPswd;
    int s = 250;
    for (int i = 0; i < timeout; i += s) {
      if (WiFi.status() == WL_CONNECTED) return WiFiSSID;
      delay(s);
    }
  }
  return NULL;
}

String AT(String s, unsigned long timeout = 10000) {
  String b;
  unsigned long p = millis();
  while (true) {
    if (millis() - p >= timeout) break;
    b = "";
    Serial2.print(s);
    delay(100);
    while (Serial2.available()) {
      b += (char)Serial2.read();
    }
    debug(b);
    if (strstr(s2c(s), "+CREG?")) {
      if (strstr(s2c(parseString(1, ',', b)), "1"))
        break;
    } else {
      if (strstr(s2c(b), "OK"))
        break;
    }
  }
  return b;
}

bool modemBegin(bool restart = false) {
  if (!restart) {
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  
    pinMode(RESET_PIN, OUTPUT);
    //return true;
  
    // аппаратная перезагрузка
    digitalWrite(RESET_PIN, LOW);
    delay(1000);
    digitalWrite(RESET_PIN, HIGH);
  
    delay(60000); // "прогрев" 
  }
  
  if (AT("AT\r") != "") // модем отвечает?
  if (AT("ATE0\r") != "") // ЭХО 1 – вкл (по умолчанию) / 0 – выкл
  if (AT("AT+CPAS\r", 60000) != "") // Информация о состояние модуля 0 – готов к работе 2 – неизвестно 3 – входящий звонок 4 – голосовое соединение
  if (AT("AT+CMGD=1,4\r") != "") // удалить все сообщения
  if (AT("AT+CSCB=1\r") != "") // Приём специальных сообщений 0 – разрешен (по умолчанию) 1 – запрещен
  if (AT("AT+CLIP=1\r") != "") // АОН 1 – включить 0 – выключить
  if (AT("AT+CMGF=1\r") != "") // Текстовый режим 1 – включить 0 – выключить
  if (AT("AT+CSCS=\"GSM\"\r") != "") // кодировка
  if (AT("AT+CNMI=2,2\r") != "") // разрешить индикацию содержимого SMS сообщений.
  if (AT("AT+CREG?\r", 60000) != "") // Тип регистрации сети Второй параметр: 0 – не зарегистрирован, поиска сети нет 1 – зарегистрирован, домашняя сеть 2 – не зарегистрирован, идёт поиск новой сети 3 – регистрация отклонена 4 – неизвестно 5 – роуминг
  return true;
  return false;
}

void modemRestart() {
  digitalWrite(RESET_PIN, LOW);
  delay(60000);
  modemBegin(true);
}

void reg(String number, String message = "") {
  debug(number + " : " + message);
  File file = SPIFFS.open(((message == "") ? "/calls.txt" : "/sms.txt"), FILE_APPEND);
  file.print(ntp.getEpochTime());
  file.print('\t');
  file.print(ntp.getFormattedTime());
  file.print('\t');
  if (message == "") {
    file.print(number);
  } else {
    file.print(number);
    file.print('\t');
    file.print(message);
  }
  file.print('\n');
  file.close();
}

String getReg(String path) {
  File file = SPIFFS.open(path.c_str());
  String t = "var data = `";
  if (file) {
    while (file.available()) {
      t += (char)file.read();
    }
  }
  t += "`;";
  file.close();
  String c_ = "";
  file = SPIFFS.open("/index.html");
  if (file) {
    while (file.available()) {
      c_ += (char)file.read();
    }
  }
  file.close();
  const char * c = c_.c_str();
  char * a_ = strstr(c, "///");
  int a = a_ - c;
  String h = "";
  for (int i = 0; i < c_.length(); i++) {
    if ((i != a) && (i != a + 1) && (i != a + 2)) {
      h += (char)c_[i];
    } else {
      h += t;
      t = "";
    }
  }
  return h;
}

void setup() {
  M5.begin();

  SPIFFS.begin(true);

  M5.Lcd.println("Searching Wi-Fi, please wait...");
  char * ssid = WiFiAuto(); 
  if (ssid) {
    M5.Lcd.println("Connectid to ");
    M5.Lcd.println(ssid);
    M5.Lcd.println(WiFi.localIP());
  } else {
    M5.Lcd.println("Can't connect to WiFi network");
  }

  M5.Lcd.println("Starting NTP, please wait...");
  ntp.begin();

  M5.Lcd.println("Starting WEB, please wait...");
  web.on("/sms", []() {
    web.send(200, "text/html", getReg("/sms.txt")); 
  });
  web.on("/calls", []() {
    web.send(200, "text/html", getReg("/calls.txt"));
  });
  web.onNotFound([]() {
    web.send(404, "text/html", "404");
  });
  web.begin();

  M5.Lcd.println("Starting modem, please wait...");
  if ( modemBegin() ) {
    M5.Lcd.println("Modem OK");
  } else {
    M5.Lcd.println("Modem FAIL");
    M5.Lcd.println("Modem Restarting");
    modemRestart();
  }
    
}

void loop() {
  unsigned long ntp_p = 0;
  unsigned long modem_p = 0;
  String modem_recived = "";
  while (true) {
    // ntp
    if (millis() - ntp_p >= 1000) {
      ntp.update();
      ntp_p = millis();
    }
  
    //web
    web.handleClient();
  
    // modem
    if (Serial2.available()) {
      modem_recived += (char)Serial2.read();
      modem_p = millis();
    } else {
      if (modem_recived == "") break;
      debug(modem_recived);
      int calls = strstrcnt((char *)modem_recived.c_str(), "+CLIP:");
      int sms = strstrcnt((char *)modem_recived.c_str(), "+CMT:");
      String z = "Calls: " + String(calls) + "\nSMS: " + sms;
      debug(modem_recived);
      modem_recived = rchar(modem_recived, '\r');
      for (int i = 0; i < strstrcnt((char *)modem_recived.c_str(), "\n"); i++) {
        String n = parseString(i, '\n', modem_recived);
        if (strstr(n.c_str(), "+CLIP:")) {
          String number = parseString(1, '\"', n);
          AT("ATH0\r"); // повесить трубку
          reg(number);
        } else if (strstr(n.c_str(), "+CMT:")) {
          String number = parseString(1, '\"', n);
          i++;
          String message = parseString(i, '\n', modem_recived);
          reg(number, message);
        } else if (strstr(n.c_str(), "NO CARRIER")) {
          //debug("No carrier. Modem restarting...");
          //modemRestart();
          //break;
        }
      }
      modem_recived = "";
    }
    
    
    delay(10);
  }
}
