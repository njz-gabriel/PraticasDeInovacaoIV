#include <Arduino.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ================= PINOS =================
#define LED_BUILTIN 2

// SPI (RFID)
#define SCK_PIN 18
#define MISO_PIN 19
#define MOSI_PIN 23

#define CS_A 5
#define RST_A 17

// I2C (LCD)
#define SDA_PIN 21
#define SCL_PIN 22

// Reles
#define Rele_Ele_Ima 4
#define Rele_ArCondi 13

// DHT
#define DHTPIN 15
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ================= OBJETOS =================
SPIClass spi(VSPI);

// RFID
MFRC522DriverPinSimple ssA(CS_A);
MFRC522DriverSPI driverA(ssA, spi);
MFRC522 mfrcA(driverA);

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= VARIÁVEIS =================
String read_rfid;

String ok_rfid_1 = "c4666a31";
String ok_rfid_2 = "eaddd70";

bool acesso_liberado = false;
unsigned long tempo_aberto = 0;
const unsigned long TEMPO_PORTA_ABERTA = 10000; // 10s

// ================= FUNÇÕES =================

// Converte UID para string
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

// LCD - estado fechado
void mostrar_fechado()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Quarto fechado");
  lcd.setCursor(0,1);
  lcd.print(dht.readTemperature());
}

// LCD - erro
void mostrar_erro()
{
  lcd.setCursor(0, 1);
  lcd.print("Cartao errado ");
  lcd.setCursor(0,1);
  lcd.print(dht.readTemperature());
}

// LCD - acesso liberado
void mostrar_acesso()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Quarto aberto");
  lcd.setCursor(0, 1);
  lcd.print("Acesso OK");
}

// Leitura RFID
void leitura_rfid(MFRC522 &dev, const char *label)
{
  if (!dev.PICC_IsNewCardPresent())
    return;
  if (!dev.PICC_ReadCardSerial())
    return;

  dump_byte_array(dev.uid.uidByte, dev.uid.size);

  Serial.print("\n[");
  Serial.print(label);
  Serial.print("] UID: ");
  Serial.println(read_rfid);

  if (read_rfid == ok_rfid_1 || read_rfid == ok_rfid_2)
  {
    acesso_liberado = true;
    tempo_aberto = millis();
    digitalWrite(Rele_Ele_Ima, HIGH);

    digitalWrite(LED_BUILTIN, HIGH);
    mostrar_acesso();
  }
  else
  {
    mostrar_erro();
  }

  dev.PICC_HaltA();
  dev.PCD_StopCrypto1();
}

// Medir temperatura e umidade
void ler_dht()
{
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade))
  {
    Serial.println("Erro ao ler DHT");
    return;
  }

  Serial.print("Temp: ");
  Serial.print(temperatura);
  Serial.print(" °C | Umidade: ");
  Serial.print(umidade);
  Serial.println(" %");
  if(temperatura <= 25){
    digitalWrite(Rele_ArCondi, HIGH);
  } else{
    digitalWrite(Rele_ArCondi,LOW);
  }
}

// ================= SETUP =================
void setup()
{
  dht.begin();
  Serial.begin(115200);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();

  // Estado inicial
  mostrar_fechado();

  // LED
  pinMode(LED_BUILTIN, OUTPUT);

  // Rele
  pinMode(Rele_Ele_Ima, OUTPUT);
  pinMode(Rele_ArCondi, OUTPUT);

  // RFID
  pinMode(RST_A, OUTPUT);
  digitalWrite(RST_A, HIGH);

  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
  mfrcA.PCD_Init();

  MFRC522Debug::PCD_DumpVersionToSerial(mfrcA, Serial);

  Serial.println("Sistema pronto");
}

// ================= LOOP =================
void loop()
{
  leitura_rfid(mfrcA, "RFID");

  // Controle de tempo (10s aberto)
  if (acesso_liberado && (millis() - tempo_aberto >= TEMPO_PORTA_ABERTA))
  {
    acesso_liberado = false;
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(Rele_Ele_Ima, LOW);
    mostrar_fechado();
  }
  ler_dht();
  delay(2000);
}