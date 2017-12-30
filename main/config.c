#include "config.h"
#include <esp_err.h>
#include <esp_log.h>
#include <esp_spiffs.h>
#include <cJSON.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Constants */
static const char *TAG = "Config";
static cJSON *config = NULL;

/* BLE Configuration*/
cJSON *json_find_in_array(cJSON *arr, const char *item)
{
    cJSON *cur;

    if (!arr)
        return NULL;

    for (cur = arr->child; cur; cur = cur->next)
    {
        if (cJSON_IsString(cur) && !strcmp(item, cur->valuestring))
            return cur;
    }

    return NULL;
}

uint8_t config_ble_should_connect(const char *mac)
{
    cJSON *ble = cJSON_GetObjectItemCaseSensitive(config, "ble");
    cJSON *whitelist = cJSON_GetObjectItemCaseSensitive(ble, "whitelist");
    cJSON *blacklist = cJSON_GetObjectItemCaseSensitive(ble, "blacklist");
    uint8_t action = whitelist ? 1 : 0;
    cJSON *list = whitelist ? : blacklist;

    /* No list was defined, accept all */
    if (!list)
        return 1;

    return json_find_in_array(list, mac) ? action : !action;
}

/* MQTT Configuration*/
const char *config_mqtt_server_get(const char *param_name)
{
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(config, "mqtt");
    cJSON *server = cJSON_GetObjectItemCaseSensitive(mqtt, "server");
    cJSON *param = cJSON_GetObjectItemCaseSensitive(server, param_name);

    if (cJSON_IsString(param))
        return param->valuestring;

    return NULL;
}

const char *config_mqtt_host_get(void)
{
    return config_mqtt_server_get("host");
}

uint16_t config_mqtt_port_get(void)
{
    return 1883;
}

const char *config_mqtt_client_id_get(void)
{
    return config_mqtt_server_get("client_id");
}

const char *config_mqtt_username_get(void)
{
    return config_mqtt_server_get("username");
}

const char *config_mqtt_password_get(void)
{
    return config_mqtt_server_get("password");
}

uint8_t config_mqtt_qos_get(void)
{
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(config, "mqtt");
    cJSON *publish = cJSON_GetObjectItemCaseSensitive(mqtt, "publish");
    cJSON *qos = cJSON_GetObjectItemCaseSensitive(publish, "qos");

    if (cJSON_IsNumber(qos))
        return qos->valuedouble;

    return 0;
}

uint8_t config_mqtt_retained_get(void)
{
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(config, "mqtt");
    cJSON *publish = cJSON_GetObjectItemCaseSensitive(mqtt, "publish");
    cJSON *retain = cJSON_GetObjectItemCaseSensitive(publish, "retain");

    return cJSON_IsTrue(retain);
}

const char *config_mqtt_topics_get(const char *param_name, const char *def)
{
    cJSON *mqtt = cJSON_GetObjectItemCaseSensitive(config, "mqtt");
    cJSON *topics = cJSON_GetObjectItemCaseSensitive(mqtt, "topics");
    cJSON *param = cJSON_GetObjectItemCaseSensitive(topics, param_name);

    if (cJSON_IsString(param))
        return param->valuestring;

    return def;
}

const char *config_mqtt_get_suffix_get(void)
{
    return config_mqtt_topics_get("get_suffix", "/Get");
}

const char *config_mqtt_set_suffix_get(void)
{
    return config_mqtt_topics_get("set_suffix", "/Set");
}

/* WiFi Configuration */
const char *config_wifi_ssid_get(void)
{
    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(config, "wifi");
    cJSON *ssid = cJSON_GetObjectItemCaseSensitive(wifi, "ssid");

    if (cJSON_IsString(ssid))
        return ssid->valuestring;

    return NULL;
}

const char *config_wifi_password_get(void)
{
    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(config, "wifi");
    cJSON *password = cJSON_GetObjectItemCaseSensitive(wifi, "password");

    if (cJSON_IsString(password))
        return password->valuestring;

    return NULL;
}

static char *read_file(const char *path)
{
    int fd, len;
    struct stat st;
    char *buf;

    if (stat(path, &st))
        return NULL;

    if ((fd = open(path, O_RDONLY)) < 0)
        return NULL;

    if (!(buf = malloc(st.st_size + 1)))
        return NULL;

    len = read(fd, buf, 1024);
    close(fd);

    if (len < 0)
    {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

static cJSON *load_json(const char *path)
{
    char *str = read_file(path);
    cJSON *json;

    if (!str)
        return NULL;

    json = cJSON_Parse(str);

    free(str);
    return json;
}

int config_initialize(void)
{
    ESP_LOGI(TAG, "Initializing configuration");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));

    /* Load config.json from SPIFFS */
    if (!(config = load_json("/spiffs/config.json")))
        return -1;

    return 0;
}