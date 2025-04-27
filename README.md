# esp_ping

Aplicativo simples para teste da comunicação WiFi com microcontroladores ESP32. Foi testado com o ESP32-C3, mas deve funcionar com outros modelos que tenham WiFi.

## Compilação

O aplicativo foi desenvolvido com o IDF v5.4.1. Certifique-se que a configuração do IDF (`menuconfig` corresponde ao microcontrolador e placa usados);

Atualize o arquivo `secrets.h`, no diretório `main`, com os dados da rede WiFi e o endereço IP de quem receberá os pings.

# Uso

Após ligar/reiniciar, o programa aguarda 5 segundos, informa (via serial) as informações sobre o microcontrolador e tenta se conectar à rede WiFi,

Tendo sucesso na conexão, a cada 10 minutos realiza um teste de comunicação, enviando 100 "pings" (pedidos de ICMP echo). Os pings são espaçados de 500ms e a resposta é aguardada por 400ms. Ao final do teste é apresentado um resumo informando o quantos pacotes foram transmitidos e recebidos, os tempos mínimo, máximo e médio de resposta e a intensidade (RSSI) média do sinal.

