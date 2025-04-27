# esp_ping

Aplicativo simples para teste da comunicação WiFi com microcontroladores ESP32. Foi testado com o ESP32-C3, mas deve funcionar com outros modelos que tenham WiFi.

## Compilação

O aplicativo foi desenvolvido com o IDF v5.4.1. Certifique-se que a configuração do IDF (`menuconfig` corresponde ao microcontrolador e placa usados);

Crie um arquivo `secrets.h`, no diretório `main`, com o seguinte conteudo:

```
#define AP_ESSID  "ESSID da sua rede"
#define AP_PASSWD "senha da sua rede"
static char ip_target[] = "192.168.15.1"; // ip de quem receberá os pings
```

# Uso

Após ligar/reiniciar, o programa aguarda 5 segundos, informa (via serial) as informações sobre o microcontrolador e tenta se conectar à rede WiFi,

Tendo sucesso na conexão, a cada 10 minutos realiza um teste de comunicação, enviando 100 "pings" (pedidos de ICMP echo). Os pings são espaçados de 500ms e a resposta é aguardada por 400ms. Ao final do teste é apresentado um resumo informando o RSSI (intensidade do sinal) quantos pacotes foram transmitidos e recebidos e o tempo total.
