# GroundStation Ceres-1 (Receptor) - ESP-IDF

Este projeto implementa a estacao de solo (receptor) do Ceres-1 com recepcao LoRa, exibicao de telemetria e envio de comandos para o computador de bordo (transmissor).

Tambem foi feita a compatibilizacao com a versao mais nova do transmissor, que envia telemetria em formato texto rotulado.

## 1. Visao Geral

A GroundStation recebe pacotes LoRa vindos do OBC e:

- Decodifica telemetria.
- Exibe dados no serial monitor.
- Recebe imagens em chunks.
- Mostra mensagens de debug do OBC.
- Envia comandos uplink para o OBC.

## 2. Arquitetura de Tasks

### TaskRX

Responsavel por recepcao e processamento de pacotes LoRa.

- Inicia o radio em modo recepcao continua.
- Processa telemetria em dois modos:
  - Modo novo (texto rotulado): frame iniciando com `Tempo:`.
  - Modo legado (binario): payload `struct respost` com `struct sensorsData`.
- Processa imagens fragmentadas (`TYPE_IMAGE`) com controle de ordem e timeout.
- Processa mensagens de debug (`TYPE_DEBUG`).

### TaskTX

Responsavel por comando uplink via LoRa.

- Le entradas da UART0 (serial monitor).
- Suporta dois modos de comando:
  - Legado numerico: `1..5`.
  - Texto livre por linha: qualquer string finalizada com Enter.
- Encapsula comando em pacote `TYPE_COMMAND` e transmite via LoRa.

## 3. Protocolo Atual no Receptor

O receptor suporta simultaneamente:

### 3.1 Modo texto (novo transmissor)

Quando o frame recebido comeca com `Tempo:`, ele e tratado como telemetria textual e impresso diretamente.

Formato esperado:

`Tempo:Temperatura:Umidade:Altitude:Pressao:Latitude:Longitude:Sats:Roll:Pitch:Yaw:TempBat1:TempBat2:Tensao:Corrente:NumPacotes:RSSI:TamPacote`

Exemplo:

`Tempo:123:Temperatura:24.51:Umidade:56.20:Altitude:812.10:Pressao:920.43:Latitude:-22.123456:Longitude:-47.987654:Sats:8:Roll:1.05:Pitch:-0.77:Yaw:12.33:TempBat1:31.25:TempBat2:30.87:Tensao:12.18:Corrente:0.84:NumPacotes:245:RSSI:-98:TamPacote:248`

### 3.2 Modo legado (frame com header)

Header atual:

- `START_BYTE = 0x7E`
- `type`
- `src`
- `dst`

Tipos usados:

- `TYPE_RESPOST = 0x04`
- `TYPE_IMAGE = 0x10`
- `TYPE_DEBUG = 0x20`
- `TYPE_COMMAND = 0x30`

No `TYPE_RESPOST`:

- Primeiro tenta parse binario (`struct respost`).
- Se parse binario falhar, tenta fallback para texto rotulado no payload.

## 4. Recepcao de Imagem

Implementado buffer de imagem com:

- Tamanho maximo de buffer: 46000 bytes.
- Chunks com indice e total.
- Verificacao de sequencia esperada.
- Timeout de montagem (descarta imagem incompleta).
- Saida delimitada no serial:
  - `IMAGE_BEGIN`
  - `SIZE:<bytes>`
  - dump binario
  - `IMAGE_END`

## 5. Comandos Uplink Implementados

### 5.1 Comando numerico legado

Se usuario digitar apenas `1`, `2`, `3`, `4` ou `5` e pressionar Enter:

- Envia 1 byte de comando no payload de `TYPE_COMMAND`.

### 5.2 Comando textual

Se usuario digitar qualquer texto e pressionar Enter:

- Envia a linha como payload textual de `TYPE_COMMAND`.
- Limite atual: 96 caracteres por linha.

Mensagem de status no boot da TaskTX:

`TX pronto. Digite 1..5 para comandos legados ou texto + Enter para comando livre.`

## 6. Estrutura do Projeto

Arquivos principais do receptor:

- `main/main.c`
- `main/task_rx.c`
- `main/task_rx.h`
- `main/task_tx.c`
- `main/task_tx.h`
- `main/protocol.h`
- `main/lora_sx1278.c`
- `main/lora_sx1278.h`
- `main/CMakeLists.txt`
- `CMakeLists.txt`

## 7. LoRa Driver (Estado Atual)

O modulo `lora_sx1278.c` esta em modo wrapper/mock para integracao inicial:

- Inicializacao de SPI e pinos.
- Stubs para `lora_start_receive`, `lora_transmit` e `lora_check_rx`.

Para voo/operacao real, recomenda-se integrar driver completo SX1278 com:

- TX real
- RX real
- RSSI/SNR reais
- IRQ/FIFO e controle de estados

## 8. Build e Flash

No terminal ESP-IDF (ambiente exportado):

1. `idf.py fullclean`
2. `idf.py reconfigure`
3. `idf.py build`
4. `idf.py -p <PORTA_SERIAL> flash monitor`

Se houver problema de timestamp (ex.: OneDrive/clock skew), normalize os timestamps e rode novamente.

## 9. Testes Recomendados

1. Teste de telemetria nova (texto puro iniciando com `Tempo:`).
2. Teste de telemetria legado binario (`TYPE_RESPOST` com `struct respost`).
3. Teste de fallback: `TYPE_RESPOST` com payload texto rotulado.
4. Teste de comando uplink numerico (`1..5`).
5. Teste de comando uplink textual (linha completa + Enter).
6. Teste de imagem com perda de chunk e validacao de timeout.

## 10. Compatibilidade com o Transmissor Atual

Esta versao da GroundStation ficou compativel com o transmissor modular descrito para o OBC Ceres-1, principalmente nos pontos:

- Telemetria textual padronizada com labels.
- Campos de energia vindos da placa de suprimento dentro da string de telemetria.
- Uplink de comandos em formato flexivel (numerico legado e texto).
- Convivencia com protocolo legado sem quebra de retrocompatibilidade.

## 11. Proximos Passos

- Padronizar comandos textuais com namespace (`SUP:`, `ATT:`, `MIS:`).
- Adicionar ACK/NACK textual ou binario para comandos uplink.
- Implementar parser estruturado da string `Tempo:...` para validar campos no receptor.
- Integrar driver LoRa completo para telemetria real de RSSI/SNR e RX/TX robusto.
- Considerar CRC mais robusto para enlaces criticos (CRC-8/CRC-16).
