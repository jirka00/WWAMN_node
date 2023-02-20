#include "max1730x.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "mqtt.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "cJSON.h"

extern esp_mqtt_client_handle_t client;

char buffer[50];

static const char *TAG = "JSON";

void JSON_Print(cJSON *element)
{
    if (element->type == cJSON_Invalid)
        ESP_LOGI(TAG, "cJSON_Invalid");
    if (element->type == cJSON_False)
        ESP_LOGI(TAG, "cJSON_False");
    if (element->type == cJSON_True)
        ESP_LOGI(TAG, "cJSON_True");
    if (element->type == cJSON_NULL)
        ESP_LOGI(TAG, "cJSON_NULL");
    if (element->type == cJSON_Number)
        ESP_LOGI(TAG, "cJSON_Number int=%d double=%f", element->valueint, element->valuedouble);
    if (element->type == cJSON_String)
        ESP_LOGI(TAG, "cJSON_String string=%s", element->valuestring);
    if (element->type == cJSON_Array)
        ESP_LOGI(TAG, "cJSON_Array");
    if (element->type == cJSON_Object)
    {
        ESP_LOGI(TAG, "cJSON_Object");
        cJSON *child_element = NULL;
        cJSON_ArrayForEach(child_element, element)
        {
            JSON_Print(child_element);
        }
    }
    if (element->type == cJSON_Raw)
        ESP_LOGI(TAG, "cJSON_Raw");
}

union Data
{
    uint8_t Bytes[2];
    struct
    {
        uint8_t Low_byte;
        uint8_t High_byte;
    };
    int16_t Value;
} Data;

void max1730x::startTaskImpl(void *_this)
{
    static_cast<max1730x *>(_this)->BmsTask();
}

void max1730x::BmsTask(void)
{
    while (1)
    {
        // printf("BMS Task\n");
        // fflush(stdout);
        this->GetInfo();
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

max1730x::max1730x(i2c_port_t i2c)
{
    i2c_port = i2c;
    esp_efuse_mac_get_default(mac_base);
    sprintf(mac_str, MACSTR, MAC2STR(mac_base));
};

void max1730x::GetInfo()
{
    BmsRegs.AvgCurrent = ReadBmsReg(MAX1730X_AVGCURRENT);
    BmsRegs.Current = ReadBmsReg(MAX1730X_CURRENT);
    BmsRegs.Voltage = ReadBmsReg(MAX1730X_VCELL);
    BmsRegs.AvgVoltage = ReadBmsReg(MAX1730X_AVGVCELL);
    BmsRegs.RepFullCap = ReadBmsReg(MAX1730X_FULLCAPREP);
    BmsRegs.RepSOC = ReadBmsReg(MAX1730X_REPSOC);
    BmsRegs.RepCap = ReadBmsReg(MAX1730X_REPCAP);
    BmsRegs.TTE = ReadBmsReg(MAX1730X_TTE);
    BmsRegs.TTF = ReadBmsReg(MAX1730X_TTF);
    BmsRegs.QH = ReadBmsReg(MAX1730X_QH);
    BmsRegs.Temp = ReadBmsReg(MAX1730X_TEMP);
    BmsRegs.DieTemp = ReadBmsReg(MAX1730X_DIETEMP);
    // printf("Vcell: %i \n", (int)(BmsRegs.Voltage * 0.078125));
    // printf("AvgVcell: %i \n", (int)(BmsRegs.AvgVoltage * 0.078125));
    // printf("AvgCurr: %i \n", (int)(BmsRegs.AvgCurrent * 156.25));
    // printf("Curr: %i \n", (int)(BmsRegs.Current * 156.25));
    sprintf(buffer, "%i", (int)(BmsRegs.Voltage * 0.078125));
    //esp_mqtt_client_publish(client, "BMS2/Voltage_mV", buffer, 0, 0, 0);
    strcpy(topic_str, mac_str);
    strcat(topic_str, "");

    //ESP_LOGI(TAG, "Serialize.....");
    cJSON *root;
    root = cJSON_CreateArray();

    cJSON *element, *element2;


    element = cJSON_CreateObject();
    cJSON_AddStringToObject(element, "measurement", mac_str);
    element2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(element2, "Voltage_mV", (int)(BmsRegs.Voltage * 0.078125));
    cJSON_AddNumberToObject(element2, "Current_mA", (int)(BmsRegs.Current * 156.25));
    cJSON_AddNumberToObject(element2, "AvgVoltage_mV", (int)(BmsRegs.AvgVoltage * 0.078125));
    cJSON_AddNumberToObject(element2, "AvgCurrent_mA", (int)(BmsRegs.AvgCurrent * 156.25));
    cJSON_AddNumberToObject(element2, "RepFullCap_mAh", (int)(BmsRegs.RepFullCap * 0.5));
    cJSON_AddNumberToObject(element2, "RepSOC_m%", (int)(BmsRegs.RepSOC * 3.90625));
    cJSON_AddNumberToObject(element2, "RepCap_mAh", (int)(BmsRegs.RepCap * 0.5));
    cJSON_AddNumberToObject(element2, "TTE_s", (int)(BmsRegs.TTE * 5.625));
    cJSON_AddNumberToObject(element2, "TTF_s", (int)(BmsRegs.TTF * 5.625));
    cJSON_AddNumberToObject(element2, "QH_mAh", (int)(BmsRegs.QH * 0.5));
    cJSON_AddNumberToObject(element2, "Temp_m°C", (int)(BmsRegs.Temp * 3.90625));
    cJSON_AddNumberToObject(element2, "DieTemp_m°C", (int)(BmsRegs.DieTemp * 3.90625));

    cJSON_AddItemToObject(element, "fields", element2);

    element2 = cJSON_CreateObject();
    cJSON_AddStringToObject(element2, "MAC", mac_str);
    cJSON_AddItemToObject(element, "tags", element2);

    cJSON_AddItemToArray(root, element);


    char *my_json_string = cJSON_Print(root);
    //ESP_LOGI(TAG, "my_json_string\n%s", my_json_string);

    esp_mqtt_client_publish(client, topic_str, my_json_string, 0, 0, 0);

    cJSON_Delete(root);
    // Buffers returned by cJSON_Print must be freed by the caller.
    // Please use the proper API (cJSON_free) rather than directly calling stdlib free.
    cJSON_free(my_json_string);

    //esp_mqtt_client_publish(client, topic_str, buffer, 0, 0, 0);

}

int16_t max1730x::ReadBmsReg(uint8_t reg_addr)
{
    i2c_master_write_read_device(i2c_port, MAX_1730x_addr_low, &reg_addr, 1, (uint8_t *)&(Data.Bytes), 2, 50 / portTICK_RATE_MS);

    //    printf("BMS reg read value: %i \n", (int)(Data.Value * 156.25));
    //    printf("BMS reg read valueV: %i \n", (int)(Data.Value * 0.078125));
    return Data.Value;
}

void max1730x::begin()
{
    xTaskCreate(this->startTaskImpl, "BMS_task", 10240, this, 5, NULL);
}
