/*
  Aplicativo simples para teste da comunicação WiFi
  Gerado a partir do exemplo ICMP Echo da Espressif (https://github.com/espressif/esp-idf/tree/v5.4.1/examples/protocols/icmp_echo)
  Daniel Quadros, abril/25
*/

#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <nvs_flash.h>
#include <ping/ping_sock.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>

#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>

/*
  crie o arquivo secrets.h no mesmo diretório deste fonte, com o seguinte conteudo:

  #define AP_ESSID  "ESSID da sua rede"
  #define AP_PASSWD "senha da sua rede"
  static char ip_target[] = "192.168.15.1"; // ip de quem receberá os pings

*/
#include "secrets.h"

// Handle para geração dos pings
static esp_ping_handle_t ping;

// Semáforo para aguardar final dos pings
static SemaphoreHandle_t sem_ping = NULL;

// Semáforo para aguardar final da conexão à rede
static SemaphoreHandle_t sem_got_ip = NULL;

// Mostra informações sobre o microcontrolador
void chip_info() {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("Este e um %s com %d core(s), %s%s%s%s, ",
         CONFIG_IDF_TARGET,
         chip_info.cores,
         (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
         (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
         (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
         (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("silicon revision v%d.%d, ", major_rev, minor_rev);
  if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
      printf("Nao conseguiu obter o tamanho da Flash\n");
      return;
  }

  // CHIP_FEATURE_EMB_FLASH não implementado no ESP32-C3?
  //printf("%" PRIu32 "MB flash %s\n", flash_size / (uint32_t)(1024 * 1024),
  //       (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "interna" : "externa");
  printf("%" PRIu32 "MB flash\n", flash_size / (uint32_t)(1024 * 1024));
}


// Trata os eventos de rede (WiFi e TCP/IP)
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  if (event_id == WIFI_EVENT_STA_START) {
      printf("Conectando WiFi....\n");
  } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      printf("WiFi conectado\n");
  } else if (event_id == IP_EVENT_STA_GOT_IP) {
      // Obteve um IP, pode iniciar comunicação
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      printf("IP: " IPSTR "\n", IP2STR(&event->ip_info.ip));
      if (sem_got_ip) {
          xSemaphoreGive(sem_got_ip);
      }
  }
}

// Dispara a conexão à rede WiFi usada para teste
void wifi_connection() {
  esp_netif_init();

  sem_got_ip = xSemaphoreCreateBinary();

  esp_event_loop_create_default();

  esp_netif_create_default_wifi_sta();
  wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_initiation);

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
  wifi_config_t my_config = {
      .sta = {
          .ssid = AP_ESSID,
          .password = AP_PASSWD,
          .bssid_set = 0
      }
  };
  esp_wifi_set_config(ESP_IF_WIFI_STA, &my_config);

  esp_wifi_start();
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_connect();
  printf ("Disparada conexão\n");

  xSemaphoreTake(sem_got_ip, portMAX_DELAY);
}


// Callback de ping bem sucedido
static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    printf("%ld bytes from %s icmp_seq=%d ttl=%d time=%ld ms\n",
           recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

// Callback de timeout na resposta do ping
static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4), seqno);
}

// Callback de fim do teste de ping
static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;
    wifi_ap_record_t ap_info;

    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
      printf ("RSSI = %d  ", ap_info.rssi);
    }

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    printf("%ld packets transmitted, %ld received, time %ldms\n\n", transmitted, received, total_time_ms);

    xSemaphoreGive(sem_ping);
}


void ping_init() {
  ip_addr_t target_addr;
  ip4addr_aton(ip_target, &target_addr.u_addr.ip4);
  target_addr.type = IPADDR_TYPE_V4;

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
  ping_config.target_addr = target_addr;
  ping_config.interval_ms = 500;
  ping_config.timeout_ms = 400;
  ping_config.count = 100;

  /* set callback functions */
  esp_ping_callbacks_t cbs;
  cbs.on_ping_success = test_on_ping_success;
  cbs.on_ping_timeout = test_on_ping_timeout;
  cbs.on_ping_end = test_on_ping_end;
  cbs.cb_args = NULL;

  esp_ping_new_session(&ping_config, &cbs, &ping);

  sem_ping = xSemaphoreCreateBinary();
}

void app_main(void)
{
  // Dá um tempo para conectar o terminal
  vTaskDelay(5000 / portTICK_PERIOD_MS);

  printf("\nTeste de WiFi - PING\n");
  nvs_flash_init();
  chip_info();
  wifi_connection();

  ping_init();

  while (1) {
      // Faz o teste de pings
    esp_ping_start(ping);
    xSemaphoreTake(sem_ping, portMAX_DELAY);   

    // Dá um tempo antes de repetir
    vTaskDelay(60000 / portTICK_PERIOD_MS);
  }
}