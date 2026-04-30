#include <Arduino.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <SPI.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "EmonLib.h"

// ================= PROTÓTIPOS =================
void tela_erro();
void tela_aberto();
void tela_fechado();
void ler_temperatura();
void energia();
void ler_corrente();
void leitura_rfid(MFRC522 &dev);

// ================= PINOS =================
#define LED_BUILTIN 2

// RFID (VSPI)
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23
#define CS_A 5
#define RST_A 17

// OLED (HSPI)
#define OLED_CLK 14
#define OLED_MOSI 13
#define OLED_DC 16
#define OLED_RST 4
#define OLED_CS 27

// Relés
#define Rele_Ele_Ima 25
#define Rele_ArCondi 32
#define Rele_Energia 12

// Botão
#define Button 26

// ===== SENSOR DE CORRENTE =====
#define PINO_SCT 34

float correnteRMS = 0;
float potenciaAparente = 0;

unsigned long tempo_corrente = 0;
const unsigned long INTERVALO_CORRENTE = 1000;

const float tensaoRede = 127.0;

// ================= SPI =================
SPIClass spiRFID(VSPI);
SPIClass spiOLED(HSPI);

// ================= OBJETOS =================

// RFID
MFRC522DriverPinSimple ssA(CS_A);
MFRC522DriverSPI driverA(ssA, spiRFID);
MFRC522 mfrcA(driverA);

// OLED
Adafruit_SSD1306 display(128, 64, &spiOLED, OLED_DC, OLED_RST, OLED_CS);

// Sensor
Adafruit_BMP085 bmp;

// ===== EmonLib =====
EnergyMonitor SCT013;

// ================= VARIÁVEIS =================
String read_rfid;

String ok_rfid_1 = "c4666a31";
String ok_rfid_2 = "eaddd70";

bool acesso_liberado = false;
unsigned long tempo_aberto = 0;
const unsigned long TEMPO_PORTA_ABERTA = 10000;

unsigned long tempo_temp = 0;
const unsigned long INTERVALO_TEMP = 2000;

bool estadoEnergiaAnterior = false;
float temperaturaAtual = 0;

// ================= FUNÇÕES =================

void dump_byte_array(byte *buffer, byte bufferSize)
{
  read_rfid = "";
  for (byte i = 0; i < bufferSize; i++)
  {
    if (buffer[i] < 0x10)
      read_rfid += "0";
    read_rfid += String(buffer[i], HEX);
  }
}

// ===== OLED =====

void tela_fechado()
{
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("FECHADO");

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temperaturaAtual);
  display.println(" C");

  display.display();
}

void tela_aberto()
{
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("ABERTO");

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Temp: ");
  display.print(temperaturaAtual);
  display.println(" C");

  display.display();
}

void tela_erro()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Cartao invalido");

  display.display();
}

// ===== RFID =====

void leitura_rfid(MFRC522 &dev)
{
  if (!dev.PICC_IsNewCardPresent())
    return;
  if (!dev.PICC_ReadCardSerial())
    return;

  dump_byte_array(dev.uid.uidByte, dev.uid.size);

  Serial.print("\nUID: ");
  Serial.println(read_rfid);

  if (read_rfid == ok_rfid_1 || read_rfid == ok_rfid_2)
  {
    acesso_liberado = true;
    tempo_aberto = millis();

    digitalWrite(Rele_Ele_Ima, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.println("Acesso liberado");
  }
  else
  {
    Serial.println("Cartao invalido");
    tela_erro();
    delay(1500);
  }

  dev.PICC_HaltA();
  dev.PCD_StopCrypto1();
}

// ===== TEMPERATURA =====

void ler_temperatura()
{
  temperaturaAtual = bmp.readTemperature();

  Serial.print("Temp: ");
  Serial.print(temperaturaAtual);
  Serial.println(" C");

  digitalWrite(Rele_ArCondi, temperaturaAtual <= 21);
}

// ===== ENERGIA =====

void energia()
{
  bool estadoAtual = digitalRead(Button);

  if (estadoAtual != estadoEnergiaAnterior)
  {
    estadoEnergiaAnterior = estadoAtual;

    digitalWrite(Rele_Energia, estadoAtual);

    Serial.println(estadoAtual ? "Energia Ligada" : "Energia Desligada");
  }
}

// ===== CORRENTE (EmonLib) =====

void ler_corrente()
{
  double Irms = SCT013.calcIrms(1480);

  correnteRMS = Irms;
  potenciaAparente = Irms * tensaoRede;

  Serial.print("Corrente RMS: ");
  Serial.print(correnteRMS);
  Serial.println(" A");

  Serial.print("Potencia Aparente: ");
  Serial.print(potenciaAparente);
  Serial.println(" W\n");
}

// ================= SETUP =================

void setup()
{
  Serial.begin(115200);

  spiRFID.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  spiOLED.begin(OLED_CLK, -1, OLED_MOSI);

  if (!display.begin(SSD1306_SWITCHCAPVCC))
  {
    Serial.println("Erro OLED");
    while (1);
  }

  display.setTextColor(WHITE);

  if (!bmp.begin())
  {
    Serial.println("BMP180 nao encontrado!");
    while (1);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(Rele_Ele_Ima, OUTPUT);
  pinMode(Rele_ArCondi, OUTPUT);
  pinMode(Rele_Energia, OUTPUT);
  pinMode(Button, INPUT);

  pinMode(RST_A, OUTPUT);
  digitalWrite(RST_A, HIGH);
  mfrcA.PCD_Init();

  // ===== INICIALIZA SENSOR DE CORRENTE =====
  SCT013.current(PINO_SCT, 1.0);

  Serial.println("Sistema pronto");
}

// ================= LOOP =================

void loop()
{
  leitura_rfid(mfrcA);

  if (acesso_liberado && millis() - tempo_aberto >= TEMPO_PORTA_ABERTA)
  {
    acesso_liberado = false;
    digitalWrite(Rele_Ele_Ima, LOW);
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (millis() - tempo_temp >= INTERVALO_TEMP)
  {
    tempo_temp = millis();
    ler_temperatura();
  }

  if (millis() - tempo_corrente >= INTERVALO_CORRENTE)
  {
    tempo_corrente = millis();
    ler_corrente();
  }

  if (acesso_liberado)
    tela_aberto();
  else
    tela_fechado();

  energia();
}