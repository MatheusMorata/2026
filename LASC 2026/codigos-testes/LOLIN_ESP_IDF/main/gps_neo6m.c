#include "gps_neo6m.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "esp_idf_version.h"

#define GPS_UART_BUF_SIZE 1024
#define NMEA_LINE_MAX     128

static uart_port_t s_uart = UART_NUM_1;
static char s_line[NMEA_LINE_MAX];
static int s_line_len = 0;
static gps_fix_t s_last_fix = {0};

static float parse_nmea_coord(const char *value, const char hemi) {
    if (!value || !*value) return 0.0f;

    float v = strtof(value, NULL);
    int deg = (int)(v / 100.0f);
    float min = v - (deg * 100.0f);
    float dec = deg + (min / 60.0f);

    if (hemi == 'S' || hemi == 'W') dec = -dec;
    return dec;
}

static void parse_gga(char *line) {
    char *fields[16] = {0};
    int count = 0;

    char *token = strtok(line, ",");
    while (token && count < 16) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }

    if (count < 8) return;

    const char *lat = fields[2];
    const char *ns = fields[3];
    const char *lon = fields[4];
    const char *ew = fields[5];
    const char *quality = fields[6];
    const char *sats = fields[7];

    if (!lat || !lon || !ns || !ew || !quality || !sats) return;
    if (quality[0] == '0') return;

    s_last_fix.latitude = parse_nmea_coord(lat, ns[0]);
    s_last_fix.longitude = parse_nmea_coord(lon, ew[0]);
    s_last_fix.sats = atoi(sats);
    s_last_fix.valid = true;
}

static void parse_rmc(char *line) {
    char *fields[16] = {0};
    int count = 0;

    char *token = strtok(line, ",");
    while (token && count < 16) {
        fields[count++] = token;
        token = strtok(NULL, ",");
    }

    if (count < 7) return;

    const char *status = fields[2];
    const char *lat = fields[3];
    const char *ns = fields[4];
    const char *lon = fields[5];
    const char *ew = fields[6];

    if (!status || !lat || !lon || !ns || !ew) return;
    if (status[0] != 'A') return;

    s_last_fix.latitude = parse_nmea_coord(lat, ns[0]);
    s_last_fix.longitude = parse_nmea_coord(lon, ew[0]);
    s_last_fix.valid = true;
}

static void process_line(const char *src) {
    char tmp[NMEA_LINE_MAX];
    strncpy(tmp, src, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    if (strncmp(tmp, "$GPGGA", 6) == 0 || strncmp(tmp, "$GNGGA", 6) == 0) {
        parse_gga(tmp);
    } else if (strncmp(tmp, "$GPRMC", 6) == 0 || strncmp(tmp, "$GNRMC", 6) == 0) {
        parse_rmc(tmp);
    }
}

esp_err_t gps_neo6m_init(uart_port_t uart_num, int tx_pin, int rx_pin, int baudrate) {
    s_uart = uart_num;

    uart_config_t cfg = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION_MAJOR >= 5
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    esp_err_t err = uart_driver_delete(s_uart);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = uart_driver_install(s_uart, GPS_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(s_uart, &cfg);
    if (err != ESP_OK) return err;

    err = uart_set_pin(s_uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    s_line_len = 0;
    s_last_fix.valid = false;
    s_last_fix.latitude = 0.0f;
    s_last_fix.longitude = 0.0f;
    s_last_fix.sats = 0;

    return ESP_OK;
}

esp_err_t gps_neo6m_poll(gps_fix_t *fix) {
    if (!fix) return ESP_ERR_INVALID_ARG;

    uint8_t buf[128];
    int n = uart_read_bytes(s_uart, buf, sizeof(buf), pdMS_TO_TICKS(20));

    for (int i = 0; i < n; i++) {
        char c = (char)buf[i];

        if (c == '\r') continue;
        if (c == '\n') {
            if (s_line_len > 6) {
                s_line[s_line_len] = '\0';
                process_line(s_line);
            }
            s_line_len = 0;
            continue;
        }

        if (isprint((unsigned char)c) && s_line_len < (NMEA_LINE_MAX - 1)) {
            s_line[s_line_len++] = c;
        }
    }

    *fix = s_last_fix;
    return s_last_fix.valid ? ESP_OK : ESP_ERR_NOT_FOUND;
}
