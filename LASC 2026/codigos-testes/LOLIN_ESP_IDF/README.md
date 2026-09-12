# Computador de Bordo (Transmissor) - ESP-IDF

Este projeto implementa o computador de bordo (mestre) com:

- Leitura modular de sensores locais:
  - MPU9250 (I2C)
  - BMP280 (I2C)
  - DHT22 (single-wire)
  - GPS NEO-6M V2 (UART)
- Telemetria formatada em string para uplink LoRa
- Barramento serial em cadeia (mestre -> suprimento -> controle -> missao)
- Coleta de dados de energia da placa de suprimento por protocolo de pacotes

## 1. Arquitetura de Tasks

- TaskSensores:
  - Le dados dos sensores locais.
  - Preenche estrutura sensorsData_t com campos ambientais e atitude.
  - Envia para fila de telemetria.

- TaskTelemetria:
  - Recebe dados da fila.
  - Consulta placa de suprimento via serial em cadeia (corrente, tensao, tempBat1, tempBat2).
  - Monta payload no formato padrao definido.
  - Atualiza metadados de pacote (NumPacotes, RSSI, TamPacote).
  - Envia payload via interface LoRa.

- TaskEEPROM:
  - Reservada para persistencia futura.

## 2. Formato de Telemetria

A string e enviada com a ordem exata:

Tempo:Temperatura:Umidade:Altitude:Pressao:Latitude:Longitude:Sats:Roll:Pitch:Yaw:TempBat1:TempBat2:Tensao:Corrente:NumPacotes:RSSI:TamPacote

Exemplo real de payload:

Tempo:123:Temperatura:24.51:Umidade:56.20:Altitude:812.10:Pressao:920.43:Latitude:-22.123456:Longitude:-47.987654:Sats:8:Roll:1.05:Pitch:-0.77:Yaw:12.33:TempBat1:31.25:TempBat2:30.87:Tensao:12.18:Corrente:0.84:NumPacotes:245:RSSI:-98:TamPacote:248

## 3. Pinagem Atual

### 3.1 Sensores locais

- I2C (MPU9250 e BMP280):
  - SDA = GPIO21
  - SCL = GPIO22
- DHT22:
  - DATA = GPIO4
- GPS NEO-6M V2 (UART1):
  - TX (ESP32 -> GPS) = GPIO17
  - RX (ESP32 <- GPS) = GPIO16
  - Baudrate = 9600

### 3.2 Barramento serial em cadeia entre placas

- UART2 do mestre:
  - TX = GPIO27
  - RX = GPIO25
  - Baudrate = 115200

A topologia prevista em hardware e:

Mestre (OBC) -> Placa de Suprimento -> Placa de Controle de Atitude -> Placa de Missao

Cada escravo deve:

- Consumir pacotes cujo destino (dst) seja seu ID.
- Repassar pacotes nao destinados a ele para a proxima placa da cadeia.

## 4. Protocolo de Pacotes do Barramento em Cadeia

Arquivo-base: main/serial_chain_master.c

Frame:

- SOF: 0x7E
- dst: 1 byte (destino)
- src: 1 byte (origem)
- cmd: 1 byte (comando)
- len: 1 byte (tamanho payload)
- payload: 0..96 bytes
- crc: XOR de dst, src, cmd, len e payload

IDs utilizados:

- Mestre: 0x01
- Suprimento: 0x10
- Controle de atitude: 0x20
- Missao: 0x30

## 5. Logica dos Modulos de Escravos no Mestre

Observacao: neste repositorio foi implementada apenas a logica do mestre (como solicitado).

### 5.1 Suprimento

Arquivo: main/supply_slave.c

- Requisicao:
  - cmd = 0x01 para CHAIN_SUPPLY_ID
- Resposta esperada:
  - cmd = 0x81
  - payload texto: TB1=<float>;TB2=<float>;V=<float>;I=<float>
- Campos lidos:
  - TempBat1, TempBat2, Tensao, Corrente

### 5.2 Controle de Atitude

Arquivo: main/attitude_slave.c

- Comando de execucao:
  - cmd = 0x10 para CHAIN_ATTITUDE_ID
- ACK esperado:
  - cmd = 0x90
- API:
  - attitude_slave_send_command("...", timeout_ms)

### 5.3 Missao

Arquivo: main/mission_slave.c

- Comando de execucao:
  - cmd = 0x10 para CHAIN_MISSION_ID
- ACK esperado:
  - cmd = 0x90
- API:
  - mission_slave_send_command("...", timeout_ms)

## 6. Modulos de Sensores

- main/mpu9250.c(.h)
  - Inicializacao via WHO_AM_I e configuracao basica de acel/gyro.
  - Leitura de ax/ay/az, gx/gy/gz e temperatura interna.
  - Estimativa de roll/pitch/yaw por filtro complementar simples.

- main/bmp280.c(.h)
  - Leitura de calibração e compensacao de temperatura/pressao.
  - Calculo de altitude pela atmosfera padrao.

- main/dht22.c(.h)
  - Leitura por temporizacao de pulsos e validacao de checksum.

- main/gps_neo6m.c(.h)
  - Leitura UART e parser NMEA para GGA/RMC.
  - Atualizacao de latitude, longitude e numero de satelites.

## 7. Arquivos Principais Alterados

- main/task_sensores.c
- main/task_telemetria.c
- main/main.c
- main/obc_types.h
- main/CMakeLists.txt
- main/lora_spi.c
- main/lora_spi.h

Novos:

- main/serial_chain_master.c
- main/serial_chain_master.h
- main/supply_slave.c
- main/supply_slave.h
- main/attitude_slave.c
- main/attitude_slave.h
- main/mission_slave.c
- main/mission_slave.h
- main/mpu9250.c
- main/mpu9250.h
- main/bmp280.c
- main/bmp280.h
- main/dht22.c
- main/dht22.h
- main/gps_neo6m.c
- main/gps_neo6m.h

## 8. Como compilar

No terminal ESP-IDF (com ambiente exportado):

1. idf.py fullclean
2. idf.py reconfigure
3. idf.py build

Se houver loop de reconfiguracao por timestamp (OneDrive/clock skew), normalize timestamps dos arquivos de projeto e rode novamente.

## 9. Proximos passos recomendados

- Implementar no firmware das placas escravas o repasse de quadros nao destinados.
- Implementar ACK/NACK com codigos de erro no payload.
- Adicionar timeout e retentativa por comando no mestre.
- Trocar stub de LoRa por driver SX127x/SX126x completo (TX/RX/RSSI real).
- Adicionar CRC mais robusto (CRC-8/CRC-16) se necessario.
