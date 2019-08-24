/*
Copyright 2019 Alexander Sowitzki.

GNU Affero General Public License version 3 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://opensource.org/licenses/AGPL-3.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <esp_log.h>
#include <driver/rmt.h>
#include <stddef.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#define RMT_CHANNEL RMT_CHANNEL_1
#define MAXIMUM_CHANNELS 2048
#define UART_NUM UART_NUM_0

static const char* TAG = "SK6812";

static esp_err_t rmt_init() {
  rmt_config_t rmt_cfg = {
      .rmt_mode = RMT_MODE_TX,
      .clk_div = 8,
      .gpio_num = GPIO_NUM_13, // Must be smaller than 33
      .channel = RMT_CHANNEL,
      .mem_block_num = 1,
      .tx_config = {
          .loop_en = false,
          .carrier_freq_hz = 100,
          .carrier_duty_percent = 50,
          .carrier_level = RMT_CARRIER_LEVEL_LOW,
          .carrier_en = false,
          .idle_level = RMT_IDLE_LEVEL_LOW,
          .idle_output_en = true,
      }
  };

  esp_err_t ret = rmt_config(&rmt_cfg);
  if (ret == ESP_OK) {
      ret = rmt_driver_install(rmt_cfg.channel, 0, 0);
  }
  return ret;
}

static void sk6812(void *arg) {
    // Buffer all channel settings here
    uint8_t *led_strip_buf = malloc(MAXIMUM_CHANNELS);
    // Create a RMT item for all channels (1 channel bit gets stretched into 8 RMT items)
    rmt_item32_t *rmt_items = (rmt_item32_t*) malloc(sizeof(rmt_item32_t) * MAXIMUM_CHANNELS * 8);
    for (int i=0; i < MAXIMUM_CHANNELS * 8; i++) {
      rmt_items[i].level0 = 1;
      rmt_items[i].level1 = 0;
    }

    while (1) {
        // Number of channels the master wants us to send
        uint16_t expected_length;
        // First two bytes from the master contain the expected length, read that.
        uint8_t* expected_length_array = (uint8_t*) &expected_length;
        while (uart_read_bytes(UART_NUM, &expected_length_array[0], 1, portMAX_DELAY) != 1) {}
        while (uart_read_bytes(UART_NUM, &expected_length_array[1], 1, portMAX_DELAY) != 1) {}

        if (expected_length >= MAXIMUM_CHANNELS) {
          ESP_LOGE(TAG, "Expecting to many channels. May be: %d, is %d", MAXIMUM_CHANNELS, expected_length);
          continue;
        }

        // Read in all channels
        uint16_t length_left = expected_length;
        uint16_t offset = 0;
        while (length_left != 0) {
          int read = uart_read_bytes(UART_NUM, &led_strip_buf[offset], length_left, 100000 / portTICK_RATE_MS);
          offset += read;
          length_left -= read;
        }

        // Set RMT items according to channel values
        uint32_t rmt_items_index = 0;
        for (uint32_t led_index = 0; led_index < expected_length; led_index++) {
            for (uint8_t bit = 8; bit != 0; bit--) {
                uint8_t bit_set = (led_strip_buf[led_index] >> (bit - 1)) & 1;
                rmt_items[rmt_items_index].duration0 = bit_set ? 6 : 3;
                rmt_items[rmt_items_index].duration1 = bit_set ? 6 : 9;
                rmt_items_index++;
           }
        }

        // Write RMT items
        esp_err_t ret = rmt_write_items(RMT_CHANNEL, rmt_items, rmt_items_index, false);
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "RMT error: %s", esp_err_to_name(ret));
        }
    }
    vTaskDelete(NULL);
}

void app_main() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, MAXIMUM_CHANNELS, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(rmt_init());
    xTaskCreate(sk6812, "sk6812", 1024 * 2, (void *)0, 10, NULL);
}
