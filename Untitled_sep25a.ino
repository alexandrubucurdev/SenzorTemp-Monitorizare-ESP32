#include "thingProperties.h"
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <Adafruit_BMP280.h>    // Folosim BMP280
#include <Adafruit_GFX.h>       // Librarie pentru grafica
#include <Adafruit_SSD1306.h>   // Librarie pentru OLED
#include <esp_sleep.h>

// --- Definire Ecran OLED ---
#define SCREEN_WIDTH 128 // Latimea ecranului OLED (pixeli)
#define SCREEN_HEIGHT 64 // Inaltimea ecranului OLED (pixeli)
#define OLED_RESET     -1 // Pinul de Reset

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Obiecte Senzori ---
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_BMP280 bmp; // Obiectul pentru senzorul BMP280

// --- Variabile pentru medii si timere ---
float sumaTempLCD = 0.0;
float sumaHumLCD = 0.0;
int numarCitiriLCD = 0;

float sumaTempCloud = 0.0;
float sumaHumCloud = 0.0;
int numarCitiriCloud = 0;

// Variabile pentru a stoca valoarea curenta a presiunii
float presiuneCurenta = 0;
float ultimaPresiuneTrimisa = 0; // Pentru a verifica schimbarea

unsigned long timpSerialAnterior = 0;
unsigned long timpLcdAnterior = 0;
unsigned long timpCloudAnterior = 0;

const long intervalSerial = 1000; 
const long intervalLCD = 5000;    
const long intervalCloud = 10000; 


void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Initializare sistem...");

  Wire.begin(21, 22); // Specificam pinii SDA=21, SCL=22

  // --- Initializare OLED ---
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Adresa 0x3C confirmata
    Serial.println(F("Eroare SSD1306 (OLED)"));
    for(;;); 
  }
  display.clearDisplay();
  display.setTextSize(2); // Text mare
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Pornire...");
  display.display(); 
  delay(1500);

  // Senzor SHT31 (Primul)
  if (!sht31.begin(0x44)) {
    display.clearDisplay(); display.setCursor(0,0); display.setTextSize(2); display.println("SHT31 ERR"); display.display();
    Serial.println("Eroare la initializarea SHT31 (0x44)!");
    while (1) delay(1);
  }
  Serial.println("Senzor SHT31 initializat cu succes.");
  display.clearDisplay(); display.setCursor(0,0); display.setTextSize(2); display.println("SHT31 OK"); display.display();
  delay(1500);

  // Senzor BMP280 (Al doilea)
  Serial.println("Initializare BMP280...");
  delay(250);
  if (!bmp.begin(0x76)) { // Adresa 0x76 confirmata
    display.clearDisplay(); display.setCursor(0,0); display.setTextSize(2); display.println("BMP280 ERR"); display.display();
    Serial.println("Eroare la initializarea BMP280 (0x76)!");
    while (1) delay(1);
  }
  Serial.println("Senzor BMP280 initializat cu succes.");
  display.clearDisplay(); display.setCursor(0,0); display.setTextSize(2); display.println("BMP280 OK"); display.display();
  delay(1500);

  // Arduino IoT Cloud
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Conectat!");
  delay(1000);
  display.setCursor(0, 32);
  display.println("Astept...");
  delay(1000);
  display.display();
  Serial.println("Conectat la Cloud. Se asteapta citirile...");
  delay(1000);
}


void loop() {
  ArduinoCloud.update();
  unsigned long timpCurent = millis();

  // --- Bloc 1: Citire & Afisare Serial (ruleaza la fiecare 'intervalSerial' - 1s) ---
  if (timpCurent - timpSerialAnterior >= intervalSerial) {
    timpSerialAnterior = timpCurent;

    // Citim toti senzorii
    float temp_sht = sht31.readTemperature();
    float hum_sht  = sht31.readHumidity();
    float temp_bmp = bmp.readTemperature(); // Citim temp BMP doar pentru serial
    float pres_bmp = bmp.readPressure() / 100.0F;

    // Salvam presiunea curenta pentru a o afisa pe OLED
    if (!isnan(pres_bmp)) {
      presiuneCurenta = pres_bmp;
    }

    // Afisare pe Serial Monitor (cu temp BMP inclusa)
    Serial.print("--- Citire Curenta --- ");
    Serial.print("SHT31_T: ");
    if (!isnan(temp_sht)) Serial.print(temp_sht, 1); else Serial.print("Err");
    Serial.print(" | BMP_T: "); // Temperatura de la BMP ramane in serial
    if (!isnan(temp_bmp)) Serial.print(temp_bmp, 1); else Serial.print("Err");
    Serial.print(" | Dif: ");
    if (!isnan(temp_sht) && !isnan(temp_bmp)) Serial.print(temp_sht - temp_bmp, 1); else Serial.print("Err");
    Serial.println(" *C");
    
    // --- AICI ESTE MODIFICAREA ---
    // Trimitem presiunea la Cloud DOAR daca se schimba partea intreaga (cifra unitatilor)
    // Folosim (int) pentru a converti valoarea float in integer (taie zecimalele)
    if (!isnan(pres_bmp) && (int)pres_bmp != (int)ultimaPresiuneTrimisa) {
      presiune = pres_bmp; // Actualizam variabila de cloud
      ultimaPresiuneTrimisa = pres_bmp; // Salvam noua valoare ca referinta
      Serial.println(">>> PRESIUNE NOUA TRIMISA LA CLOUD: " + String(presiune, 1) + " hPa");
    }

    // Adaugam la suma pentru LCD (Doar Temp si Humiditate SHT31)
    if (!isnan(temp_sht) && !isnan(hum_sht)) {
      sumaTempLCD += temp_sht;
      sumaHumLCD += hum_sht;
      numarCitiriLCD++;
    }
    
    // Adaugam la suma pentru Cloud (T, H de la SHT31) - neschimbat
    if (!isnan(temp_sht) && !isnan(hum_sht)) {
      sumaTempCloud += temp_sht;
      sumaHumCloud += hum_sht;
      numarCitiriCloud++;
    }
  }

  // --- Bloc 2: Calcul Medie & Update OLED (ruleaza la fiecare 'intervalLCD' - 5s) ---
  if (timpCurent - timpLcdAnterior >= intervalLCD) {
    timpLcdAnterior = timpCurent;
    
    display.clearDisplay(); 
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2); // Text mare

    if (numarCitiriLCD > 0) {
      float mediaTemp = sumaTempLCD / numarCitiriLCD;
      float mediaHum = sumaHumLCD / numarCitiriLCD;

      // Afisam media pe OLED
      // Randul 1: T (SHT31) - Media
      display.setCursor(0, 0);
      display.print("T:");
      display.print(mediaTemp, 1);
      display.print(" C");

      // Randul 2: H (SHT31) - Media
      display.setCursor(0, 22); // 16px (text) + 6px (spatiu)
      display.print("U:"); // "Umid" e prea lung
      display.print(mediaHum, 0);
      display.print(" %");

      // Randul 3: P (BMP280) - Curenta (nu media)
      display.setCursor(0, 44); // 22 + 16 + 6
      display.print("P:");
      display.print(presiuneCurenta, 0); // Folosim valoarea stocata
      display.print("hPa");

    } else {
      display.setCursor(0, 0);
      display.print("OLED Err(5s)");
    }

    display.display(); // Trimitem buffer-ul completat catre ecran

    // Resetam sumele
    sumaTempLCD = 0.0;
    sumaHumLCD = 0.0;
    numarCitiriLCD = 0;
  }

  // --- Bloc 3: Calcul Medie & Update Cloud (ruleaza la fiecare 'intervalCloud' - 10s) ---
  // Acest bloc trimite DOAR Temp si Umiditate SHT31
  if (timpCurent - timpCloudAnterior >= intervalCloud) {
    timpCloudAnterior = timpCurent;

    if (numarCitiriCloud > 0) {
      float mediaTemp = sumaTempCloud / numarCitiriCloud;
      float mediaHum = sumaHumCloud / numarCitiriCloud;
      
      temperatura = mediaTemp; // Variabila cloud
      umiditate = mediaHum;  // Variabila cloud

      Serial.println("---------------------------------");
      Serial.print("TRIMITERE CLOUD (Media SHT31 / 10s): ");
      Serial.print(mediaTemp, 1);
      Serial.print(" *C, ");
      Serial.print(mediaHum, 1);
      Serial.println(" %");
      Serial.println("---------------------------------");

    } else {
      Serial.println("---------------------------------");
      Serial.println("EROARE Cloud: Nu s-au inregistrat citiri valide in ultimele 10s.");
      Serial.println("---------------------------------");
    }
    
    sumaTempCloud = 0.0;
    sumaHumCloud = 0.0;
    numarCitiriCloud = 0;
  }
}


void onPowerOffChange() {
  if (powerOff) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(2); // Text mare
    display.println("SYSTEM OFF");
    display.display();
    Serial.println("Oprire sistem prin comanda Cloud...");
    delay(500); 
    esp_deep_sleep_start();
  }
}