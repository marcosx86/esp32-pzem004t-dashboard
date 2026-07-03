#ifndef CST820_METRICS_H
#define CST820_METRICS_H

#include <Arduino.h>
#include "lvgl.h"

/*默认指标数据点*/
#define METRICS_POINTS_DEFAULT 4

/*ESPHome/Home Assistant等设备的metrics端点URL模板*/
const char* const METRICS_ENDPOINT = "/metrics";

/*定义指标数据结构*/
typedef struct {
    uint32_t timestamp;          /*时间戳（Unix epoch秒）*/
    double voltage;              /*电压 V */
    double current;              /*电流 A */
    double energy;               /*能量 Wh */
    double wattage;              /*功率 W */
} MetricsData;

/*初始化指标数据解析器，用于从/metrics端点读取传感器数据*/
// Fetch metrics from a full URL like "http://192.168.1.50/metrics".
// Returns true on success and fills `out`, false on failure.
bool fetch_metrics(const char* url, MetricsData &out);

#endif /*CST820_METRICS_H*/
