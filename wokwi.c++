#include <Wire.h>
#include "RTClib.h"
#include <EEPROM.h>

// === CONFIGURAÇÕES DO SENSOR ===
const int trigPin = 9;
const int echoPin = 10;
const float alturaSensor = 100.0; // Altura do sensor em cm

// === EEPROM ===
int endereco = 0;

// === RTC ===
RTC_DS3231 rtc;

// === VARIÁVEIS ===
bool enchenteAtiva = false;
float distancia;
float nivelAgua;
DateTime inicioEnchente;

// ==== Declarações ====
void salvarInicio(DateTime tempo, float nivel);
void salvarFim(DateTime tempo, float nivel, int duracaoMin);
float medirDistancia();
void exibirDataHora(String texto, DateTime tempo);

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  if (!rtc.begin()) {
    Serial.println("❌ RTC não encontrado.");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("⚠️ RTC sem energia, ajustando para hora atual do computador...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("🚀 Sistema Iniciado");

  // Verificar estado inicial para enchente ativa
  distancia = medirDistancia();
  nivelAgua = alturaSensor - distancia;
  if (nivelAgua < 0) nivelAgua = 0;

  if (nivelAgua >= 30) {
    enchenteAtiva = true;
    inicioEnchente = rtc.now();

    Serial.println("⚠️ ENCHENTE DETECTADA NO INÍCIO!");
    exibirDataHora("🕒 Início da Enchente: ", inicioEnchente);

    salvarInicio(inicioEnchente, nivelAgua);
  }
}

void loop() {
  // Leitura da distância e nível
  distancia = medirDistancia();
  nivelAgua = alturaSensor - distancia;
  if (nivelAgua < 0) nivelAgua = 0;

  DateTime agora = rtc.now();

  if (nivelAgua >= 30 && !enchenteAtiva) {
    enchenteAtiva = true;
    inicioEnchente = agora;

    Serial.println("⚠️ ENCHENTE DETECTADA!");
    exibirDataHora("🕒 Início da Enchente: ", inicioEnchente);

    salvarInicio(agora, nivelAgua);
  }

  if (nivelAgua < 30 && enchenteAtiva) {
    enchenteAtiva = false;

    TimeSpan duracao = agora - inicioEnchente;
    int duracaoMin = duracao.totalseconds() / 60;

    Serial.println("✔️ ENCHENTE TERMINOU.");
    exibirDataHora("🕒 Fim da Enchente: ", agora);
    Serial.print("⏳ Duração da enchente: ");
    Serial.print(duracaoMin);
    Serial.println(" minutos.");

    salvarFim(agora, nivelAgua, duracaoMin);
  }

  if (enchenteAtiva) {
    Serial.print("🌊 Monitorando enchente | Nível de água: ");
    Serial.print(nivelAgua);
    Serial.println(" cm");
  }

  delay(30000); // Aguarda 30 segundos
}

// ==== Função para medir distância ====
float medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracao = pulseIn(echoPin, HIGH, 30000); // timeout de 30ms
  float distanciaCm = duracao * 0.034 / 2;
  return distanciaCm;
}

// ==== Função para salvar início ====
void salvarInicio(DateTime tempo, float nivel) {
  EEPROM.update(endereco++, tempo.year() - 2000);
  EEPROM.update(endereco++, tempo.month());
  EEPROM.update(endereco++, tempo.day());
  EEPROM.update(endereco++, tempo.hour());
  EEPROM.update(endereco++, tempo.minute());
  EEPROM.update(endereco++, tempo.second());
  EEPROM.update(endereco++, (byte)nivel);
}

// ==== Função para salvar fim ====
void salvarFim(DateTime tempo, float nivel, int duracaoMin) {
  EEPROM.update(endereco++, tempo.year() - 2000);
  EEPROM.update(endereco++, tempo.month());
  EEPROM.update(endereco++, tempo.day());
  EEPROM.update(endereco++, tempo.hour());
  EEPROM.update(endereco++, tempo.minute());
  EEPROM.update(endereco++, tempo.second());
  EEPROM.update(endereco++, (byte)nivel);

  EEPROM.update(endereco++, (duracaoMin >> 8) & 0xFF); // byte alto
  EEPROM.update(endereco++, duracaoMin & 0xFF);        // byte baixo
}

// ==== Função para exibir data e hora ====
void exibirDataHora(String texto, DateTime tempo) {
  Serial.print(texto);
  Serial.print(tempo.day());
  Serial.print("/");
  Serial.print(tempo.month());
  Serial.print("/");
  Serial.print(tempo.year());
  Serial.print(" ");
  Serial.print(tempo.hour());
  Serial.print(":");
  Serial.print(tempo.minute());
  Serial.print(":");
  Serial.println(tempo.second());
}
 