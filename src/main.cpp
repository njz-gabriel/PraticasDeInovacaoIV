#include <Arduino.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <SPI.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= PINOS =================
#define LED_BUILTIN 2

// SPI (compartilhado)
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23

// RFID
#define CS_A 5
#define RST_A 17

// OLED SPI
#define OLED_DC 16
#define OLED_RST 4
#define OLED_CS 27

// Relés
#define Rele_Ele_Ima 14
#define Rele_ArCondi 13
#define Rele_Energia 12

// Botão
#define Button 26

// ================= OBJETOS =================

// --- RFID ---

MFRC522DriverPinSimple ssA(CS_A);
MFRC522DriverSPI driverA(ssA, SPI);
MFRC522 mfrcA(driverA);

// --- OLED ---

// O construtor está correto usando &SPI
Adafruit_SSD1306 display(128, 64, &SPI, OLED_DC, OLED_RST, OLED_CS);

// Sensor
Adafruit_BMP085 bmp;

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
// UID → string
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
  digitalWrite(CS_A, HIGH);
  digitalWrite(OLED_CS, LOW);
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
  digitalWrite(OLED_CS, HIGH);
}

void tela_aberto()

{

  digitalWrite(CS_A, HIGH);

  digitalWrite(OLED_CS, LOW);

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

  digitalWrite(OLED_CS, HIGH);
}

void tela_erro()

{

  digitalWrite(CS_A, HIGH);

  digitalWrite(OLED_CS, LOW);

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 20);

  display.println("Cartao invalido");

  display.display();

  digitalWrite(OLED_CS, HIGH);
}

// ===== RFID =====

void leitura_rfid(MFRC522 &dev)

{

  digitalWrite(OLED_CS, HIGH);

  digitalWrite(CS_A, LOW);

  if (!dev.PICC_IsNewCardPresent())

  {

    digitalWrite(CS_A, HIGH);

    return;
  }

  if (!dev.PICC_ReadCardSerial())

  {

    digitalWrite(CS_A, HIGH);

    return;
  }

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

  digitalWrite(CS_A, HIGH);
}

// ===== TEMPERATURA =====

void ler_temperatura()

{

  temperaturaAtual = bmp.readTemperature();

  Serial.print("Temp: ");

  Serial.print(temperaturaAtual);

  Serial.println(" C");

  if (temperaturaAtual <= 21)

    digitalWrite(Rele_ArCondi, HIGH);

  else

    digitalWrite(Rele_ArCondi, LOW);
}

// ===== ENERGIA =====

void energia()

{

  bool estadoAtual = digitalRead(Button);

  if (estadoAtual != estadoEnergiaAnterior)

  {

    estadoEnergiaAnterior = estadoAtual;

    if (estadoAtual == HIGH)

    {

      Serial.println("Energia Ligada");

      digitalWrite(Rele_Energia, HIGH);
    }

    else

    {

      Serial.println("Energia Desligada");

      digitalWrite(Rele_Energia, LOW);
    }
  }
}

// ================= SETUP =================

void setup()

{

  Serial.begin(115200);

  // Inicialize o SPI apenas uma vez

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  // CS controle (garanta que comecem em HIGH para evitar conflitos)

  pinMode(CS_A, OUTPUT);

  pinMode(OLED_CS, OUTPUT);

  digitalWrite(CS_A, HIGH);

  digitalWrite(OLED_CS, HIGH);

  // Inicialize o OLED

  if (!display.begin(SSD1306_SWITCHCAPVCC))

  {

    Serial.println("Erro OLED");

    while (1)

      ;
  }

  display.clearDisplay();

  display.setTextColor(WHITE);

  display.display(); // Importante chamar para limpar o buffer inicial

  // BMP180

  if (!bmp.begin())

  {

    Serial.println("BMP180 nao encontrado!");

    while (1)

      ;
  }

  // Pinos

  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(Rele_Ele_Ima, OUTPUT);

  pinMode(Rele_ArCondi, OUTPUT);

  pinMode(Rele_Energia, OUTPUT);

  pinMode(Button, INPUT);

  // RFID

  pinMode(RST_A, OUTPUT);

  digitalWrite(RST_A, HIGH);

  // No RFID, remova o spi.begin extra, mantenha apenas o mfrcA.PCD_Init()

  mfrcA.PCD_Init();

  Serial.println("Sistema pronto");
}

// ================= LOOP =================

void loop()

{

  leitura_rfid(mfrcA);

  // Tempo porta

  if (acesso_liberado && (millis() - tempo_aberto >= TEMPO_PORTA_ABERTA))

  {

    acesso_liberado = false;

    digitalWrite(Rele_Ele_Ima, LOW);

    digitalWrite(LED_BUILTIN, LOW);
  }

  // Temperatura

  if (millis() - tempo_temp >= INTERVALO_TEMP)

  {

    tempo_temp = millis();

    ler_temperatura();
  }

  // Display

  if (acesso_liberado)

    tela_aberto();

  else

    tela_fechado();

  energia();
}