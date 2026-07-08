//Inclusão de bibliotecas de pinos
#include "pinos_placa_v1.h"
#include <SoftwareSerial.h>

// ===========================================
// --- Comunicação Serial com Placa Mestre ---
// ===========================================
#define RX_PIN 12    // Pino RX (recebe dados da placa mestre)
#define TX_PIN 4     // Pino TX (envia dados para placa mestre)

// Cria objeto SoftwareSerial para comunicação com a placa mestre
SoftwareSerial controleSerial(RX_PIN, TX_PIN); 

// Endereço deste dispositivo na comunicação serial
#define MEU_ENDERECO 3  // Endereço do Controle de Mecanismos

// ===========================================
// --- Modo de Teste de Comunicação ---
// ===========================================
#define MODO_TESTE_COMUNICACAO 0  // 0 = modo normal, 1 = modo teste
#define SERIAL_DEBUG_ENABLE 1

//===========================================
// --- Constantes para Painel e Antenas  ---
// ===========================================
#define TEMPO_ACIONAMENTO_PAINEL 5000
#define TIMEOUT_ANTENA 10000

String bufferSerial = "";
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 5000;

// ===========================================
// --- FUNÇÕES AUXILIARES ---
// ===========================================

String limparString(String str) {
  str.trim();
  String resultado = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if ((c >= 32 && c <= 126) || c == '\n' || c == '\r') {
      resultado += c;
    }
  }
  resultado.trim();
  return resultado;
}

void enviarResposta(String dados) {
  String resposta = String(MEU_ENDERECO) + ":" + dados;
  controleSerial.println(resposta);
  
  if (SERIAL_DEBUG_ENABLE) {
    Serial.print(">>> Enviando: ");
    Serial.println(resposta);
  }
}

void ativaPinoPorTempo(int pino1, int pino2, unsigned long tempoAtivo) {
  digitalWrite(pino1, HIGH);
  digitalWrite(pino2, HIGH);
  delay(tempoAtivo);
  digitalWrite(pino1, LOW);
  digitalWrite(pino2, LOW);    
}

int lerEntradaAnalogicaComoDigital(uint8_t pinoAnalogico, int limiar = 512) {
  int valor = analogRead(pinoAnalogico);
  return (valor > limiar) ? HIGH : LOW;
}

bool aguardaValorChave(int pinoChave, int valorEsperado, unsigned long tempoTimeout) {
  unsigned long inicio = millis();
  while (millis() - inicio < tempoTimeout) {
    int valorAtual = lerEntradaAnalogicaComoDigital(pinoChave);
    if (valorAtual == valorEsperado) {
      return true;
    }
  }
  return false;
}

// Controle de motor em malha aberta (PWM direto)
// valorPwm de -255 a 255
void defineVelocidade(int valorPwm) {
  valorPwm = constrain(valorPwm, -255, 255);

  if (valorPwm == 0) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
  } else if (valorPwm > 0) {
    analogWrite(IN1, valorPwm);
    digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);
    analogWrite(IN2, abs(valorPwm));
  }
}

// ===========================================
// --- MODO DE TESTE DE COMUNICAÇÃO ---
// ===========================================
#if MODO_TESTE_COMUNICACAO == 1

void setup() {
  Serial.begin(9600);
  controleSerial.begin(9600);
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  
  Serial.println("\n\n=== MODO TESTE COMUNICACAO ===");
  Serial.println("Controle de Mecanismos - Teste Serial");
  Serial.print("RX pino: "); Serial.println(RX_PIN);
  Serial.print("TX pino: "); Serial.println(TX_PIN);
  Serial.print("Endereço: "); Serial.println(MEU_ENDERECO);
  Serial.println("================================\n");
  
  delay(1000);
  enviarResposta("MODO TESTE - Pronto");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      controleSerial.println(cmd);
    }
  }
  
  if (controleSerial.available()) {
    String msg = controleSerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      String resposta = "ECO:" + msg;
      enviarResposta(resposta);
    }
  }
  
  if (millis() - lastHeartbeat > heartbeatInterval) {
    lastHeartbeat = millis();
    enviarResposta("HEARTBEAT");
  }
}

#else

// ===========================================
// --- MODO NORMAL DE OPERAÇÃO ---
// ===========================================

void setup() {
  Serial.begin(9600);
  controleSerial.begin(9600);
  
  delay(500);
  
  Serial.println("\n=== CONTROLE DE MECANISMOS DO SATÉLITE ===");
  Serial.print("RX: "); Serial.println(RX_PIN);
  Serial.print("TX: "); Serial.println(TX_PIN);
  Serial.print("Endereco: "); Serial.println(MEU_ENDERECO);
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  
  // Configuração dos Pinos de Motor (Controle Principal)
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  
  // Configuração dos Pinos de Mecanismos (Painéis e Antenas)
  pinMode(PAINEL_IN1, OUTPUT);
  pinMode(PAINEL_IN2, OUTPUT);
  pinMode(PAINEL_IN3, OUTPUT);
  pinMode(PAINEL_IN4, OUTPUT);
  pinMode(ANTIN1, OUTPUT);
  pinMode(ANTIN2, OUTPUT);
  
  delay(1000);
  
  Serial.println("Sistema de mecanismos inicializado!");
  Serial.println("Aguardando comandos do Computador de Bordo...\n");
  
  enviarResposta("Controle de Mecanismos inicializado");
}

void loop() {
  // === LEITURA SERIAL DA PLACA MESTRE ===
  while (controleSerial.available()) {
    char c = controleSerial.read();
    
    if (c == '\n' || c == '\r') {
      if (bufferSerial.length() > 0) {
        String msg = limparString(bufferSerial);
        
        if (msg.length() > 0) {
          if (SERIAL_DEBUG_ENABLE) {
            Serial.print("RX: '"); Serial.print(msg); Serial.println("'");
          }
          
          int idx = msg.indexOf(':');
          
          if (idx > 0) {
            String addrStr = msg.substring(0, idx);
            int addr = addrStr.toInt();
            String cmd = msg.substring(idx + 1);
            cmd.trim();
            
            if (addr == MEU_ENDERECO || addr == 0) {
              processarComando(cmd);
            }
          } else {
            processarComando(msg);
          }
        }
        bufferSerial = "";
      }
    } else {
      bufferSerial += c;
    }
  }
}

void processarComando(String comando) {
  comando = limparString(comando);
  
  if (SERIAL_DEBUG_ENABLE) {
    Serial.print("Processando Comando: '"); Serial.print(comando); Serial.println("'");
  }
  
  if (comando.startsWith("REQ:")) {
    // Telemetria simples: responde que está operacional
    enviarResposta("STATUS:OPERACIONAL");
    return;
  }
  
  if (comando == "0") {
    enviarResposta("Pronto para operacao");
    defineVelocidade(0);
  }
  else if (comando == "5") {
    enviarResposta("Motores principais desligados");
    defineVelocidade(0);
  }
  else if (comando == "6") {
    enviarResposta("Abrindo Paineis Solares");
    digitalWrite(LED, HIGH);
    digitalWrite(PAINEL_IN4, LOW);
    digitalWrite(PAINEL_IN2, LOW);
    ativaPinoPorTempo(PAINEL_IN3, PAINEL_IN1, TEMPO_ACIONAMENTO_PAINEL);
    digitalWrite(PAINEL_IN3, LOW);
    digitalWrite(PAINEL_IN1, LOW);
    digitalWrite(LED, LOW);
    enviarResposta("Paineis abertos com sucesso");
  }
  else if (comando == "7") {
    enviarResposta("Abrindo Antena");
    digitalWrite(ANTIN1, HIGH);
    digitalWrite(ANTIN2, LOW);
    
    // Aguarda o switch de fim de curso ser acionado
    if (aguardaValorChave(SWANT1, HIGH, TIMEOUT_ANTENA)) {
      digitalWrite(ANTIN1, LOW);
      digitalWrite(ANTIN2, LOW);
      enviarResposta("Antena aberta com sucesso");
    } else {
      enviarResposta("Erro: timeout antena ao abrir");
      digitalWrite(ANTIN1, LOW);
      digitalWrite(ANTIN2, LOW);
    }
  }
  else if (comando == "8") {
    enviarResposta("Fechando Antena");
    digitalWrite(LED, HIGH);
    digitalWrite(ANTIN1, LOW);
    digitalWrite(ANTIN2, HIGH);
    
    // Aguarda o switch de fim de curso oposto
    if (aguardaValorChave(SWANT2, HIGH, TIMEOUT_ANTENA)) {
      digitalWrite(ANTIN1, LOW);
      digitalWrite(ANTIN2, LOW);
      enviarResposta("Antena fechada com sucesso");
    } else {
      enviarResposta("Erro: timeout antena ao fechar");
      digitalWrite(ANTIN1, LOW);
      digitalWrite(ANTIN2, LOW);
    }
    digitalWrite(LED, LOW);
  }
  else if (comando == "10") {
    enviarResposta("EMERGENCIA - Parando todos os mecanismos");
    defineVelocidade(0);
    digitalWrite(PAINEL_IN1, LOW);
    digitalWrite(PAINEL_IN2, LOW);
    digitalWrite(PAINEL_IN3, LOW);
    digitalWrite(PAINEL_IN4, LOW);
    digitalWrite(ANTIN1, LOW);
    digitalWrite(ANTIN2, LOW);
  }
  else if (comando == "11") {
    enviarResposta("Testando paineis solares");
    digitalWrite(PAINEL_IN4, LOW);
    digitalWrite(PAINEL_IN2, LOW);
    ativaPinoPorTempo(PAINEL_IN3, PAINEL_IN1, 1000);
    delay(500);
    ativaPinoPorTempo(PAINEL_IN4, PAINEL_IN2, 1000);
    enviarResposta("Teste de paineis concluido");
  }
  else if (comando == "13") {
    enviarResposta("Testando atuador principal (malha aberta)");
    defineVelocidade(150); // PWM positivo
    delay(2000);
    defineVelocidade(-150); // PWM negativo
    delay(2000);
    defineVelocidade(0); // Parar
    enviarResposta("Teste do atuador concluido");
  }
  // Comandos antigos referentes a sensores/PID desativados para evitar falhas de comunicação
  else if (comando == "1" || comando.startsWith("2:") || comando == "3" || comando == "4" || comando == "9" || comando.startsWith("PID:")) {
    enviarResposta("AVISO: Comando desativado (Sistema operando apenas como mecanismo)");
  }
  else if (comando == "14") {
    enviarResposta("STATUS: MECANISMOS PRONTOS");
  }
  else {
    enviarResposta("ERRO: Comando invalido - " + comando);
  }
}

#endif // MODO_TESTE_COMUNICACAO