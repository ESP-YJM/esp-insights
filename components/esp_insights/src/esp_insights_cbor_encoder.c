// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <stdint.h>
#include <esp_log.h>
#include <esp_diagnostics.h>
#include <cbor.h>
#include <esp_timer.h>
#if CONFIG_DIAG_COREDUMP_ENABLE
#include <esp_core_dump.h>
#endif
#include <rtc_store.h>
#include <esp_diagnostics_metrics.h>
#include <esp_diagnostics_variables.h>
#include <soc/soc_memory_layout.h>

static CborEncoder s_encoder, s_result_map, s_diag_map, s_diag_data_map;
static CborEncoder s_meta_encoder, s_meta_result_map, s_diag_meta_map, s_diag_meta_data_map;

void esp_insights_cbor_encode_diag_begin(void *data, size_t data_size, const char *version, const char *sha256)
{
    cbor_encoder_init(&s_encoder, data, data_size, 0);
    cbor_encoder_create_map(&s_encoder, &s_result_map, 1);
    cbor_encode_text_stringz(&s_result_map, "diag");
    cbor_encoder_create_map(&s_result_map, &s_diag_map, CborIndefiniteLength);

    cbor_encode_text_stringz(&s_diag_map, "ver");
    cbor_encode_text_stringz(&s_diag_map, version);

    cbor_encode_text_stringz(&s_diag_map, "ts");
    cbor_encode_uint(&s_diag_map, esp_diag_timestamp_get());

    cbor_encode_text_stringz(&s_diag_map, "sha256");
    cbor_encode_text_stringz(&s_diag_map, sha256);
}

size_t esp_insights_cbor_encode_diag_end(void *data)
{
    cbor_encoder_close_container(&s_result_map, &s_diag_map);
    cbor_encoder_close_container(&s_encoder, &s_result_map);
    return cbor_encoder_get_buffer_size(&s_encoder, data);
}

void esp_insights_cbor_encode_diag_data_begin(void)
{
    cbor_encode_text_stringz(&s_diag_map, "data");
    cbor_encoder_create_map(&s_diag_map, &s_diag_data_map, CborIndefiniteLength);
}

void esp_insights_cbor_encode_diag_data_end(void)
{
    cbor_encoder_close_container(&s_diag_map, &s_diag_data_map);
}

#if CONFIG_DIAG_COREDUMP_ENABLE
void esp_insights_cbor_encode_diag_crash(esp_core_dump_summary_t *summary)
{
    uint8_t i;
    CborEncoder crash_map, val_list, bt_list;

    cbor_encode_text_stringz(&s_diag_data_map, "crash");
    cbor_encoder_create_map(&s_diag_data_map, &crash_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&crash_map, "ver");
    cbor_encode_uint(&crash_map, summary->core_dump_version);
    cbor_encode_text_stringz(&crash_map, "sha256");
    cbor_encode_text_stringz(&crash_map, (char *)summary->app_elf_sha256);
    cbor_encode_text_stringz(&crash_map, "task");
    cbor_encode_text_stringz(&crash_map, summary->exc_task);

    cbor_encode_text_stringz(&crash_map, "exc_val");
    cbor_encoder_create_array(&crash_map, &val_list, CborIndefiniteLength);
    cbor_encode_uint(&val_list, summary->exc_pc);
#if CONFIG_IDF_TARGET_ARCH_RISCV
    cbor_encode_uint(&val_list, summary->ex_info.mcause);
    cbor_encode_uint(&val_list, summary->ex_info.mtval);

    cbor_encoder_close_container(&crash_map, &val_list);

    cbor_encode_text_stringz(&crash_map, "stackdump");
    cbor_encoder_create_array(&crash_map, &bt_list, CborIndefiniteLength);

    // Interpret stackdump as an array of uint32_t
    uint32_t *s_dump =(uint32_t *)&summary->exc_bt_info.stackdump[0];

    for (i = 0; i < (summary->exc_bt_info.dump_size / 4); i++) {
        cbor_encode_uint(&bt_list, s_dump[i]);
    }
    cbor_encoder_close_container(&crash_map, &bt_list);

    // stackdump is treated as an array of uint32_t hence converting no. of bytes to no. of words
    cbor_encode_text_stringz(&crash_map, "dump_size");
    cbor_encode_uint(&crash_map, summary->exc_bt_info.dump_size / 4);

    CborEncoder reg_list;
    cbor_encode_text_stringz(&crash_map, "mstatus");
    cbor_encode_uint(&crash_map, summary->ex_info.mstatus);
    cbor_encode_text_stringz(&crash_map, "mtvec");
    cbor_encode_uint(&crash_map, summary->ex_info.mtvec);
    cbor_encode_text_stringz(&crash_map, "ra");
    cbor_encode_uint(&crash_map, summary->ex_info.ra);
    cbor_encode_text_stringz(&crash_map, "sp");
    cbor_encode_uint(&crash_map, summary->ex_info.sp);
    cbor_encode_text_stringz(&crash_map, "a_reg");
    cbor_encoder_create_array(&crash_map, &reg_list, CborIndefiniteLength);
    for (i = 0; i < 8; i++) {
        cbor_encode_uint(&reg_list, summary->ex_info.exc_a[i]);
    }
    cbor_encoder_close_container(&crash_map, &reg_list);
#else /* IDF_TARGET_ARCH_XTENSA */
    cbor_encode_uint(&val_list, summary->ex_info.exc_cause);
    cbor_encode_uint(&val_list, summary->ex_info.exc_vaddr);

    cbor_encoder_close_container(&crash_map, &val_list);
    cbor_encode_text_stringz(&crash_map, "bt");
    cbor_encoder_create_array(&crash_map, &bt_list, CborIndefiniteLength);
    for (i = 0; i < summary->exc_bt_info.depth; i++) {
        cbor_encode_uint(&bt_list, summary->exc_bt_info.bt[i]);
    }
    cbor_encoder_close_container(&crash_map, &bt_list);

    cbor_encode_text_stringz(&crash_map, "bt_corrupt");
    cbor_encode_boolean(&crash_map, summary->exc_bt_info.corrupted);

    CborEncoder epcx_list, reg_list;
    cbor_encode_text_stringz(&crash_map, "a_reg");
    cbor_encoder_create_array(&crash_map, &reg_list, CborIndefiniteLength);
    for (i = 0; i < 16; i++) {
        cbor_encode_uint(&reg_list, summary->ex_info.exc_a[i]);
    }
    cbor_encoder_close_container(&crash_map, &reg_list);

    cbor_encode_text_stringz(&crash_map, "epcx");
    cbor_encoder_create_array(&crash_map, &epcx_list, CborIndefiniteLength);
    for (i = 0; i < EPCx_REGISTER_COUNT; i++) {
        if (summary->ex_info.epcx_reg_bits & (1 << i)) {
            cbor_encode_uint(&epcx_list, summary->ex_info.epcx[i]);
        }
    }
    cbor_encoder_close_container(&crash_map, &epcx_list);
#endif /* CONFIG_IDF_TARGET_ARCH_RISCV */
    cbor_encoder_close_container(&s_diag_data_map, &crash_map);
}
#endif /* CONFIG_DIAG_COREDUMP_ENABLE */

void esp_insights_cbor_encode_diag_boot_info(esp_diag_device_info_t *device_info)
{
    CborEncoder boot_map;
    cbor_encode_text_stringz(&s_diag_data_map, "boot");
    cbor_encoder_create_map(&s_diag_data_map, &boot_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&boot_map, "ts");
    cbor_encode_uint(&boot_map, esp_diag_timestamp_get() - esp_timer_get_time());
    cbor_encode_text_stringz(&boot_map, "chip");
    cbor_encode_uint(&boot_map, device_info->chip_model);
    cbor_encode_text_stringz(&boot_map, "chip_rev");
    cbor_encode_uint(&boot_map, device_info->chip_rev);
    cbor_encode_text_stringz(&boot_map, "reason");
    cbor_encode_uint(&boot_map, device_info->reset_reason);
    cbor_encode_text_stringz(&boot_map, "proj");
    cbor_encode_text_stringz(&boot_map, device_info->project_name);
    cbor_encode_text_stringz(&boot_map, "app_ver");
    cbor_encode_text_stringz(&boot_map, device_info->app_version);
    cbor_encoder_close_container(&s_diag_data_map, &boot_map);
}

static void encode_msg_args(CborEncoder *element, uint8_t *args, uint8_t args_len)
{
#ifdef CONFIG_DIAG_LOG_MSG_ARG_FORMAT_TLV
    int d;
    float f;
    unsigned int u;
    uint8_t type, len, i = 0;
    CborEncoder arg_list;

    cbor_encoder_create_array(element, &arg_list, CborIndefiniteLength);
    if (!args || !args_len) {
        cbor_encoder_close_container(element, &arg_list);
        return;
    }

    while (i < args_len) {
        type = args[i++];
        len = args[i++];
        switch(type) {
            case ARG_TYPE_STRING:
                cbor_encode_text_string(&arg_list, (char *)(args + i), len);
                break;

            case ARG_TYPE_FLOAT:
                memcpy(&f, args + i, len);
                cbor_encode_float(&arg_list, f);
                break;

            case ARG_TYPE_INT:
            case ARG_TYPE_HEX:
                memcpy(&d, args + i, len);
                if (d < 0) {
                    cbor_encode_negative_int(&arg_list, -d);
                } else {
                    cbor_encode_int(&arg_list, d);
                }
                break;

            case ARG_TYPE_UINT:
            case ARG_TYPE_POINTER:
                memcpy(&u, args + i, len);
                cbor_encode_uint(&arg_list, u);
                break;

            case ARG_TYPE_CHAR:
                cbor_encode_simple_value(&arg_list, args[i]);
                break;
        }
        i += len;
    }
    cbor_encoder_close_container(element, &arg_list);
#else
    cbor_encode_text_stringz(element, (char *)args);
#endif /* CONFIG_DIAG_LOG_MSG_ARG_FORMAT_TLV */
}

static void encode_log_element(CborEncoder *list, esp_diag_log_data_t *log)
{
    CborEncoder element;
    cbor_encoder_create_map(list, &element, CborIndefiniteLength);
    cbor_encode_text_stringz(&element, "ts");
    cbor_encode_uint(&element, log->timestamp);
    cbor_encode_text_stringz(&element, "tag");
    cbor_encode_text_stringz(&element, log->tag);
    cbor_encode_text_stringz(&element, "pc");
    cbor_encode_uint(&element, log->pc);
    cbor_encode_text_stringz(&element, "ro");
    cbor_encode_uint(&element, (uint32_t)log->msg_ptr);
    cbor_encode_text_stringz(&element, "av");
    encode_msg_args(&element, log->msg_args, log->msg_args_len);
    if (strlen(log->task_name) > 0) {
        cbor_encode_text_stringz(&element, "task");
        cbor_encode_text_stringz(&element, log->task_name);
    }
    cbor_encoder_close_container(list, &element);
}

static void encode_log_list(CborEncoder *map, esp_diag_log_type_t type,
                              const char *key, const uint8_t *data, size_t size)
{
    int i = 0, len = 0;
    CborEncoder list;
    esp_diag_log_data_t *log = NULL;
    cbor_encode_text_stringz(map, key);
    cbor_encoder_create_array(map, &list, CborIndefiniteLength);
    while (size > 0) {
        if (data[i] == type) {
            log = (esp_diag_log_data_t *)&data[i];
            encode_log_element(&list, log);
        }
        len = sizeof(esp_diag_log_data_t);
        i += len;
        size -= len;
    }
    cbor_encoder_close_container(map, &list);
}

/* The TinyCBOR library does not support DOM (Document Object Model)-like API.
 * So, we need to traverse through the entire data to encode every type of log.
 */
void esp_insights_cbor_encode_diag_logs(const uint8_t *data, size_t size)
{
    CborEncoder log_map;
    cbor_encode_text_stringz(&s_diag_data_map, "traces");
    cbor_encoder_create_map(&s_diag_data_map, &log_map, CborIndefiniteLength);
    encode_log_list(&log_map, ESP_DIAG_LOG_TYPE_ERROR, "errors", data, size);
    encode_log_list(&log_map, ESP_DIAG_LOG_TYPE_WARNING, "warnings", data, size);
    encode_log_list(&log_map, ESP_DIAG_LOG_TYPE_EVENT, "events", data, size);
    cbor_encoder_close_container(&s_diag_data_map, &log_map);
}

#if (CONFIG_DIAG_ENABLE_METRICS || CONFIG_DIAG_ENABLE_VARIABLES)
// {"n":<key>, "v": <value>, "t": <ts> }
static void encode_str_data_pt(CborEncoder *map, const uint8_t *data)
{
    esp_diag_str_data_pt_t *m_data = (esp_diag_str_data_pt_t *)data;
    cbor_encode_text_stringz(map, "n");
    cbor_encode_text_stringz(map, m_data->key);
    cbor_encode_text_stringz(map, "v");
    cbor_encode_text_stringz(map, m_data->value.str);
    cbor_encode_text_stringz(map, "t");
    cbor_encode_uint(map, m_data->ts);
}

static void encode_data_pt(CborEncoder *map, const uint8_t *data)
{
    esp_diag_data_pt_t *m_data = (esp_diag_data_pt_t *)data;
    cbor_encode_text_stringz(map, "n");
    cbor_encode_text_stringz(map, m_data->key);
    cbor_encode_text_stringz(map, "v");
    switch (m_data->data_type) {
        case ESP_DIAG_DATA_TYPE_BOOL:
            cbor_encode_boolean(map, m_data->value.b);
            break;
        case ESP_DIAG_DATA_TYPE_INT:
            if (m_data->value.i < 0) {
                cbor_encode_negative_int(map, -(m_data->value.i));
            } else {
                cbor_encode_int(map, m_data->value.i);
            }
            break;
        case ESP_DIAG_DATA_TYPE_UINT:
            cbor_encode_uint(map, m_data->value.u);
            break;
        case ESP_DIAG_DATA_TYPE_FLOAT:
            cbor_encode_double(map, m_data->value.d);
            break;
        case ESP_DIAG_DATA_TYPE_IPv4:
            cbor_encode_byte_string(map, (uint8_t *)&m_data->value.ipv4, sizeof(m_data->value.ipv4));
            break;
        case ESP_DIAG_DATA_TYPE_MAC:
            cbor_encode_byte_string(map, &m_data->value.mac[0], sizeof(m_data->value.mac));
            break;
        default:
            break;
    }
    cbor_encode_text_stringz(map, "t");
    cbor_encode_uint(map, m_data->ts);
}

static void encode_data_points(const uint8_t *data, size_t size, const char *key, uint16_t type)
{
    assert(key);
    size_t i = 0;
    CborEncoder array, map;
    rtc_store_non_critical_data_hdr_t header;
    esp_diag_data_type_t data_type;

    if (!data) {
        return;
    }
    cbor_encode_text_stringz(&s_diag_data_map, key);
    cbor_encoder_create_array(&s_diag_data_map, &array, CborIndefiniteLength);
    while (size > 0) {
        memset(&header, 0, sizeof(header));
        memcpy(&header, data + i, sizeof(header));
        if (!header.dg || !esp_ptr_in_drom(header.dg) || !header.len) {
            break;
        }
        if ((uint16_t)(data[i + sizeof(header)]) == type) {
            cbor_encoder_create_map(&array, &map, CborIndefiniteLength);
            data_type = data[i + sizeof(header) + sizeof(uint16_t)];
            if (data_type == ESP_DIAG_DATA_TYPE_STR) {
                encode_str_data_pt(&map, data + i + sizeof(header));
            } else {
                encode_data_pt(&map, data + i + sizeof(header));
            }
            cbor_encoder_close_container(&array, &map);
        }
        size -= (sizeof(header) + header.len);
        i += (sizeof(header) + header.len);
    }
    cbor_encoder_close_container(&s_diag_data_map, &array);
}
#endif /* (CONFIG_DIAG_ENABLE_METRICS || CONFIG_DIAG_ENABLE_VARIABLES) */

#if CONFIG_DIAG_ENABLE_METRICS
void esp_insights_cbor_encode_diag_metrics(const uint8_t *data, size_t size)
{
    encode_data_points(data, size, "metrics", ESP_DIAG_DATA_PT_METRICS);
}
#endif /* CONFIG_DIAG_ENABLE_METRICS */

#if CONFIG_DIAG_ENABLE_VARIABLES
void esp_insights_cbor_encode_diag_variables(const uint8_t *data, size_t size)
{
    encode_data_points(data, size, "params", ESP_DIAG_DATA_PT_VARIABLE);
}
#endif /* CONFIG_DIAG_ENABLE_VARIABLES */

/* Below are the helpers to encode esp insights meta data */

void esp_insights_cbor_encode_meta_begin(void *data, size_t data_size, const char *version, const char *sha256)
{
    cbor_encoder_init(&s_meta_encoder, data, data_size, 0);
    cbor_encoder_create_map(&s_meta_encoder, &s_meta_result_map, 1);
    cbor_encode_text_stringz(&s_meta_result_map, "diagmeta");
    cbor_encoder_create_map(&s_meta_result_map, &s_diag_meta_map, CborIndefiniteLength);

    cbor_encode_text_stringz(&s_diag_meta_map, "ver");
    cbor_encode_text_stringz(&s_diag_meta_map, version);

    cbor_encode_text_stringz(&s_diag_meta_map, "ts");
    cbor_encode_uint(&s_diag_meta_map, esp_diag_timestamp_get());

    cbor_encode_text_stringz(&s_diag_meta_map, "sha256");
    cbor_encode_text_stringz(&s_diag_meta_map, sha256);
}

size_t esp_insights_cbor_encode_meta_end(void *data)
{
    cbor_encoder_close_container(&s_meta_result_map, &s_diag_meta_map);
    cbor_encoder_close_container(&s_meta_encoder, &s_meta_result_map);
    return cbor_encoder_get_buffer_size(&s_meta_encoder, data);
}

void esp_insights_cbor_encode_meta_data_begin(void)
{
    cbor_encode_text_stringz(&s_diag_meta_map, "data");
    cbor_encoder_create_map(&s_diag_meta_map, &s_diag_meta_data_map, CborIndefiniteLength);
}

void esp_insights_cbor_encode_meta_data_end(void)
{
    cbor_encoder_close_container(&s_diag_meta_map, &s_diag_meta_data_map);
}

#if CONFIG_DIAG_ENABLE_METRICS
static void encode_metrics_meta_element(CborEncoder *map, const esp_diag_metrics_meta_t *metrics)
{
    CborEncoder id_map;
    cbor_encode_text_stringz(map, metrics->key);
    cbor_encoder_create_map(map, &id_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&id_map, "label");
    cbor_encode_text_stringz(&id_map, metrics->label);
    cbor_encode_text_stringz(&id_map, "path");
    cbor_encode_text_stringz(&id_map, metrics->path);
    cbor_encode_text_stringz(&id_map, "data_type");
    cbor_encode_uint(&id_map, metrics->type);
    cbor_encoder_close_container(map, &id_map);
}

void esp_insights_cbor_encode_meta_metrics(const esp_diag_metrics_meta_t *metrics, uint32_t metrics_len)
{
    if (!metrics || !metrics_len) {
        return;
    }
    uint32_t i;
    CborEncoder map;
    cbor_encode_text_stringz(&s_diag_meta_data_map, "metrics");
    cbor_encoder_create_map(&s_diag_meta_data_map, &map, CborIndefiniteLength);
    for (i = 0; i < metrics_len; i++) {
        encode_metrics_meta_element(&map, (metrics + i));
    }
    cbor_encoder_close_container(&s_diag_meta_data_map, &map);
}
#endif /* CONFIG_DIAG_ENABLE_METRICS */

#if CONFIG_DIAG_ENABLE_VARIABLES
static void encode_variable_meta_element(CborEncoder *map, const esp_diag_variable_meta_t *variable)
{
    CborEncoder id_map;
    cbor_encode_text_stringz(map, variable->key);
    cbor_encoder_create_map(map, &id_map, CborIndefiniteLength);
    cbor_encode_text_stringz(&id_map, "label");
    cbor_encode_text_stringz(&id_map, variable->label);
    cbor_encode_text_stringz(&id_map, "path");
    cbor_encode_text_stringz(&id_map, variable->path);
    cbor_encode_text_stringz(&id_map, "data_type");
    cbor_encode_uint(&id_map, variable->type);
    cbor_encoder_close_container(map, &id_map);
}

void esp_insights_cbor_encode_meta_variables(const esp_diag_variable_meta_t *variables, uint32_t variables_len)
{
    if (!variables || !variables_len) {
        return;
    }
    uint32_t j;
    CborEncoder map;
    cbor_encode_text_stringz(&s_diag_meta_data_map, "params");
    cbor_encoder_create_map(&s_diag_meta_data_map, &map, CborIndefiniteLength);
    for (j = 0; j < variables_len; j++) {
        encode_variable_meta_element(&map, (variables + j));
    }
    cbor_encoder_close_container(&s_diag_meta_data_map, &map);
}
#endif /* CONFIG_DIAG_ENABLE_VARIABLES */
