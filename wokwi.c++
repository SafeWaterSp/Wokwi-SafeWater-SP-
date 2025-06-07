#include <Wire.h>   // Necessário para comunicação I2C com o RTC
#include "RTClib.h" // Biblioteca para o módulo RTC DS3231
#include <EEPROM.h> // Biblioteca para salvar dados na memória EEPROM do Arduino

// --- CONFIGURAÇÕES DOS PINOS E CONSTANTES ---
const int trigPin = 9;         // Pino Trigger do sensor ultrassônico
const int echoPin = 10;        // Pino Echo do sensor ultrassônico
const int ledPin = 7;          // Pino para o LED de alerta (conectado à protoboard)
const int buzzerPin = 8;       // Pino para o Buzzer
const float alturaSensor = 100.0; // Altura do sensor em cm (do sensor até a base, por exemplo)

// --- CONFIGURAÇÃO EEPROM ---
int endereco = 0;              // Endereço inicial para escrita na EEPROM

// --- CONFIGURAÇÃO RTC ---
RTC_DS3231 rtc;                 // Objeto para o módulo RTC DS3231

// --- VARIÁVEIS DE ESTADO ---
bool enchenteAtiva = false;    // Indica se há uma enchente detectada
float distancia;               // Distância lida pelo sensor ultrassônico
float nivelAgua;               // Nível de água calculado
DateTime inicioEnchente;       // Armazena a data e hora de início da enchente

// Variáveis para controle de tempo (não bloqueante)
unsigned long ultimoMonitoramento = 0; // Para controle de tempo da leitura do sensor
unsigned long ultimoPiscarLed = 0;     // Para controle de tempo do piscar do LED
unsigned long ultimoToqueBuzzer = 0;   // Para controle de tempo do buzzer
bool buzzerLigado = false;             // Estado atual do buzzer

// Constantes de tempo
const long intervaloMonitoramento = 10000; // Intervalo de 10 segundos para leitura do sensor
const long intervaloPiscarLed = 500;       // Intervalo de 0.5 segundos para o LED piscar
const long duracaoToqueBuzzer = 5000;      // Buzzer toca por 5 segundos
const long intervaloRepeticaoBuzzer = 15000; // Buzzer repete a cada 15 segundos

// --- DECLARAÇÃO DE FUNÇÕES ---
void salvarInicio(DateTime tempo, float nivel);
void salvarFim(DateTime tempo, float nivel, int duracaoMin);
float medirDistancia();
void exibirDataHora(String texto, DateTime tempo);

void setup() {
  Serial.begin(9600); // Inicia a comunicação serial

  // Configura os pinos do sensor ultrassônico
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  // Configura os pinos do LED e do Buzzer
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT); // Configura o pino do buzzer como saída

  // Inicializa o módulo RTC
  if (!rtc.begin()) {
    Serial.println("❌ RTC não encontrado. Verifique as conexões.");
    while (1); // Trava o programa se o RTC não for encontrado
  }

  // Verifica se o RTC perdeu energia e ajusta a hora
  if (rtc.lostPower()) {
    Serial.println("⚠️ RTC sem energia, ajustando para hora atual do computador...");
    // A data e hora atual será 2025-06-06 21:25:20
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Ajusta a hora usando a data/hora da compilação
  }

  Serial.println("🚀 Sistema Iniciado");

  // --- VERIFICAÇÃO INICIAL AO LIGAR O SISTEMA ---
  // Mede a distância e calcula o nível de água logo no início
  distancia = medirDistancia();
  nivelAgua = alturaSensor - distancia;
  if (nivelAgua < 0) nivelAgua = 0; // Garante que o nível não seja negativo

  // Se o nível de água já estiver acima do limite na inicialização
  if (nivelAgua >= 30) {
    enchenteAtiva = true;
    inicioEnchente = rtc.now(); // Registra o início da enchente

    Serial.println("⚠️ ENCHENTE DETECTADA NO INÍCIO!");
    exibirDataHora("🕒 Início da Enchente: ", inicioEnchente);

    salvarInicio(inicioEnchente, nivelAgua); // Salva os dados de início na EEPROM
  }
}

void loop() {
  unsigned long tempoAtual = millis();

  // Lógica para piscar o LED se houver enchente (não bloqueante)
  if (enchenteAtiva) {
    if (tempoAtual - ultimoPiscarLed >= intervaloPiscarLed) {
      ultimoPiscarLed = tempoAtual;
      digitalWrite(ledPin, !digitalRead(ledPin)); // Inverte o estado do LED
    }

    // Lógica para o buzzer tocar (não bloqueante)
    if (!buzzerLigado && (tempoAtual - ultimoToqueBuzzer >= intervaloRepeticaoBuzzer)) {
      digitalWrite(buzzerPin, HIGH); // Liga o buzzer
      buzzerLigado = true;
      ultimoToqueBuzzer = tempoAtual; // Reinicia o timer do toque
    }

    if (buzzerLigado && (tempoAtual - ultimoToqueBuzzer >= duracaoToqueBuzzer)) {
      digitalWrite(buzzerPin, LOW); // Desliga o buzzer após a duração
      buzzerLigado = false;
      // Não atualiza ultimoToqueBuzzer aqui, ele será atualizado quando for a hora de tocar novamente
    }
  } else {
    // Garante que o LED e o Buzzer estejam desligados quando não há enchente
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
    buzzerLigado = false; // Reseta o estado do buzzer
    ultimoToqueBuzzer = tempoAtual; // Reinicia o timer do buzzer para quando a enchente começar
  }


  // Atualiza o monitoramento da distância a cada 'intervaloMonitoramento' segundos
  if (tempoAtual - ultimoMonitoramento >= intervaloMonitoramento) {
    ultimoMonitoramento = tempoAtual; // Reinicia o timer

    // --- LEITURA DA DISTÂNCIA E NÍVEL DE ÁGUA ---
    distancia = medirDistancia();
    nivelAgua = alturaSensor - distancia;
    if (nivelAgua < 0) nivelAgua = 0; // Garante que o nível não seja negativo

    DateTime agora = rtc.now(); // Obtém a data e hora atual do RTC

    // --- LÓGICA DE DETECÇÃO DE ENCHENTE ---
    // Se o nível de água atingir 30cm ou mais e a enchente não estiver ativa
    if (nivelAgua >= 30 && !enchenteAtiva) {
      enchenteAtiva = true;
      inicioEnchente = agora; // Registra a hora de início da enchente

      Serial.println("⚠️ ENCHENTE DETECTADA!");
      exibirDataHora("🕒 Início da Enchente: ", inicioEnchente);

      salvarInicio(agora, nivelAgua); // Salva os dados de início na EEPROM
      ultimoToqueBuzzer = tempoAtual - intervaloRepeticaoBuzzer; // Garante que o buzzer toque imediatamente ao iniciar a enchente
    }

    // --- LÓGICA DE FIM DA ENCHENTE ---
    // Se o nível de água cair abaixo de 30cm e a enchente estiver ativa
    if (nivelAgua < 30 && enchenteAtiva) {
      enchenteAtiva = false; // Sinaliza que a enchente terminou

      TimeSpan duracao = agora - inicioEnchente; // Calcula a duração da enchente
      int duracaoMin = duracao.totalseconds() / 60; // Converte para minutos

      Serial.println("✔️ ENCHENTE TERMINOU.");
      exibirDataHora("🕒 Fim da Enchente: ", agora);
      Serial.print("⏳ Duração da enchente: ");
      Serial.print(duracaoMin);
      Serial.println(" minutos.");

      salvarFim(agora, nivelAgua, duracaoMin); // Salva os dados de fim na EEPROM
      digitalWrite(buzzerPin, LOW); // Garante que o buzzer seja desligado
      buzzerLigado = false; // Reseta o estado do buzzer
    }

    // Exibe o nível de água atual se a enchente estiver ativa
    if (enchenteAtiva) {
      Serial.print("🌊 Monitorando enchente | Nível de água: ");
      Serial.print(nivelAgua);
      Serial.println(" cm");
    }
  }
}

// --- FUNÇÃO PARA MEDIR DISTÂNCIA COM O SENSOR ULTRASSÔNICO ---
float medirDistancia() {
  // Garante que o Trigger esteja LOW antes de iniciar
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Envia um pulso HIGH de 10 microssegundos para o Trigger
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Mede a duração do pulso HIGH no Echo (tempo que o som levou para ir e voltar)
  // O timeout de 30000 microssegundos (30ms) evita que a função trave se não houver eco
  long duracao = pulseIn(echoPin, HIGH, 30000);

  // Calcula a distância em cm
  // Velocidade do som no ar ~0.034 cm/microsegundo
  // Dividido por 2 porque o som vai e volta
  float distanciaCm = duracao * 0.034 / 2;
  return distanciaCm;
}

// --- FUNÇÃO PARA SALVAR OS DADOS DE INÍCIO DA ENCHENTE NA EEPROM ---
void salvarInicio(DateTime tempo, float nivel) {
  // Salva ano (apenas os últimos 2 dígitos), mês, dia, hora, minuto, segundo e nível de água
  EEPROM.update(endereco++, tempo.year() - 2000); // Salva o ano como byte (ex: 2025 -> 25)
  EEPROM.update(endereco++, tempo.month());
  EEPROM.update(endereco++, tempo.day());
  EEPROM.update(endereco++, tempo.hour());
  EEPROM.update(endereco++, tempo.minute());
  EEPROM.update(endereco++, tempo.second());
  EEPROM.update(endereco++, (byte)nivel); // Salva o nível como byte
}

// --- FUNÇÃO PARA SALVAR OS DADOS DE FIM DA ENCHENTE NA EEPROM ---
void salvarFim(DateTime tempo, float nivel, int duracaoMin) {
  // Salva ano (apenas os últimos 2 dígitos), mês, dia, hora, minuto, segundo e nível de água
  EEPROM.update(endereco++, tempo.year() - 2000); // Salva o ano como byte (ex: 2025 -> 25)
  EEPROM.update(endereco++, tempo.month());
  EEPROM.update(endereco++, tempo.day());
  EEPROM.update(endereco++, tempo.hour());
  EEPROM.update(endereco++, tempo.minute());
  EEPROM.update(endereco++, tempo.second());
  EEPROM.update(endereco++, (byte)nivel); // Salva o nível como byte

  // Salva a duração em minutos (pode precisar de 2 bytes se for muito longa)
  EEPROM.update(endereco++, (duracaoMin >> 8) & 0xFF); // Salva o byte mais significativo
  EEPROM.update(endereco++, duracaoMin & 0xFF);        // Salva o byte menos significativo
}

// --- FUNÇÃO PARA EXIBIR DATA E HORA NO MONITOR SERIAL ---
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
