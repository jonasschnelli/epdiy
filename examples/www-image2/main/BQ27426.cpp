#include "BQ27426.h"
#include <string.h>
#include <stdio.h>

const char* BQ27426::TAG = "BQ27426";

BQ27426::BQ27426() : initialized(false), i2c_port(BQ27426_I2C_MASTER_NUM) {
}

BQ27426::~BQ27426() {
    if (initialized) {
        deinit();
    }
}

bq27426_err_t BQ27426::begin(int sda_pin, int scl_pin, uint32_t frequency) {
    // Configure I2C master
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)sda_pin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)scl_pin;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = frequency;
    conf.clk_flags = 0;
    
    esp_err_t ret = i2c_param_config(i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return BQ27426_ERR_I2C_INIT;
    }
    
    ret = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return BQ27426_ERR_I2C_INIT;
    }
    
    // Give the chip time to start up
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Check if device is connected by reading device type
    uint16_t deviceType = read_device_type();
    if (deviceType == 0x0426) {  // Expected device type for BQ27426
        initialized = true;
        ESP_LOGI(TAG, "BQ27426 initialized successfully");
        return BQ27426_OK;
    } else {
        ESP_LOGE(TAG, "BQ27426 initialization failed. Expected device type 0x0426, got 0x%04X", deviceType);
        i2c_driver_delete(i2c_port);
        return BQ27426_ERR_DEVICE_NOT_FOUND;
    }
}

void BQ27426::deinit() {
    if (initialized) {
        i2c_driver_delete(i2c_port);
        initialized = false;
    }
}

bool BQ27426::is_connected() {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27426_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(BQ27426_I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK);
}

esp_err_t BQ27426::i2c_master_read_register(uint8_t reg, uint8_t* data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27426_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    
    // Read data
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27426_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(BQ27426_I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

esp_err_t BQ27426::i2c_master_write_register(uint8_t reg, uint16_t value) {
    uint8_t data[3] = {reg, (uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27426_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 3, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(i2c_port, cmd, pdMS_TO_TICKS(BQ27426_I2C_MASTER_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

uint16_t BQ27426::read_register(uint8_t reg) {
    uint8_t data[2];
    esp_err_t ret = i2c_master_read_register(reg, data, 2);
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(ret));
        return 0xFFFF;
    }
    
    // BQ27426 returns LSB first, then MSB
    return (data[1] << 8) | data[0];
}

bool BQ27426::write_register(uint8_t reg, uint16_t value) {
    esp_err_t ret = i2c_master_write_register(reg, value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool BQ27426::write_control_command(uint16_t command) {
    return write_register(BQ27426_CONTROL, command);
}

bool BQ27426::execute_control_command(uint16_t command) {
    if (!write_control_command(command)) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2)); // Wait for command to execute
    return true;
}

// Individual reading functions
uint16_t BQ27426::read_voltage() {
    return read_register(BQ27426_VOLTAGE);
}

int16_t BQ27426::read_average_current() {
    return (int16_t)read_register(BQ27426_AVERAGE_CURRENT);
}

int16_t BQ27426::read_average_power() {
    return (int16_t)read_register(BQ27426_AVERAGE_POWER);
}

uint16_t BQ27426::read_temperature() {
    return read_register(BQ27426_TEMPERATURE);
}

uint16_t BQ27426::read_internal_temperature() {
    return read_register(BQ27426_INTERNAL_TEMP);
}

uint16_t BQ27426::read_state_of_charge() {
    return read_register(BQ27426_STATE_OF_CHARGE);
}

uint16_t BQ27426::read_remaining_capacity() {
    return read_register(BQ27426_REMAINING_CAPACITY);
}

uint16_t BQ27426::read_full_charge_capacity() {
    return read_register(BQ27426_FULL_CHARGE_CAP);
}

uint16_t BQ27426::read_flags() {
    return read_register(BQ27426_FLAGS);
}

uint16_t BQ27426::read_control_status() {
    write_control_command(BQ27426_CONTROL_STATUS);
    vTaskDelay(pdMS_TO_TICKS(2));
    return read_register(BQ27426_CONTROL);
}

uint16_t BQ27426::read_device_type() {
    write_control_command(BQ27426_DEVICE_TYPE);
    vTaskDelay(pdMS_TO_TICKS(2));
    return read_register(BQ27426_CONTROL);
}

uint16_t BQ27426::read_firmware_version() {
    write_control_command(BQ27426_FW_VERSION);
    vTaskDelay(pdMS_TO_TICKS(2));
    return read_register(BQ27426_CONTROL);
}

uint16_t BQ27426::read_chemical_id() {
    write_control_command(BQ27426_CHEM_ID);
    vTaskDelay(pdMS_TO_TICKS(2));
    return read_register(BQ27426_CONTROL);
}

bq27426_err_t BQ27426::read_all_data(bq27426_data_t &data) {
    if (!initialized) {
        return BQ27426_ERR_NOT_INITIALIZED;
    }
    
    // Read basic measurements
    data.voltage = read_voltage();
    data.averageCurrent = read_average_current();
    data.averagePower = read_average_power();
    data.temperature = read_temperature();
    data.internalTemperature = read_internal_temperature();
    
    // Read capacity information
    data.stateOfCharge = read_state_of_charge();
    data.remainingCapacity = read_remaining_capacity();
    data.fullChargeCapacity = read_full_charge_capacity();
    data.fullAvailableCapacity = read_register(BQ27426_FULL_CAPACITY);
    data.nominalAvailableCapacity = read_register(BQ27426_NOMINAL_CAPACITY);
    data.stateOfHealth = read_register(BQ27426_STATE_OF_HEALTH);
    
    // Read status information
    data.flags = read_flags();
    data.controlStatus = read_control_status();
    data.deviceType = read_device_type();
    data.firmwareVersion = read_firmware_version();
    data.chemicalID = read_chemical_id();
    
    // Calculate temperatures in Celsius
    data.temperatureC = kelvin_to_celsius(data.temperature);
    data.internalTemperatureC = kelvin_to_celsius(data.internalTemperature);
    
    // Decode flags and status
    decode_flags(data.flags, data);
    decode_control_status(data.controlStatus, data);
    
    return BQ27426_OK;
}

void BQ27426::decode_flags(uint16_t flags, bq27426_data_t &data) {
    data.overTemperature = (flags & BQ27426_FLAG_OT) != 0;
    data.underTemperature = (flags & BQ27426_FLAG_UT) != 0;
    data.fullCharge = (flags & BQ27426_FLAG_FC) != 0;
    data.chargeAllowed = (flags & BQ27426_FLAG_CHG) != 0;
    data.batteryDetected = (flags & BQ27426_FLAG_BAT_DET) != 0;
    data.discharging = (flags & BQ27426_FLAG_DSG) != 0;
}

void BQ27426::decode_control_status(uint16_t status, bq27426_data_t &data) {
    data.initComplete = (status & BQ27426_CTRL_INITCOMP) != 0;
    data.sealedState = (status & BQ27426_CTRL_SS) != 0;
}

float BQ27426::kelvin_to_celsius(uint16_t kelvin) {
    return (kelvin / 10.0f) - 273.15f;
}

void BQ27426::print_state_data() {
    bq27426_data_t data;
    if (read_all_data(data) == BQ27426_OK) {
        print_state_data(data);
    } else {
        ESP_LOGE(TAG, "Failed to read BQ27426 data");
    }
}

void BQ27426::print_state_data(const bq27426_data_t &data) {
    ESP_LOGI(TAG, "=== BQ27426 Fuel Gauge State ===");
    
    // Device Information
    ESP_LOGI(TAG, "Device Type: 0x%04X", data.deviceType);
    ESP_LOGI(TAG, "Firmware Version: 0x%04X", data.firmwareVersion);
    ESP_LOGI(TAG, "Chemical ID: 0x%04X", data.chemicalID);
    
    // Basic Measurements
    ESP_LOGI(TAG, "Voltage: %d mV (%.3f V)", data.voltage, data.voltage / 1000.0f);
    ESP_LOGI(TAG, "Average Current: %d mA", data.averageCurrent);
    ESP_LOGI(TAG, "Average Power: %d mW", data.averagePower);
    ESP_LOGI(TAG, "Temperature: %.1f°C", data.temperatureC);
    ESP_LOGI(TAG, "Internal Temperature: %.1f°C", data.internalTemperatureC);
    
    // Capacity Information
    ESP_LOGI(TAG, "State of Charge: %d%%", data.stateOfCharge);
    ESP_LOGI(TAG, "State of Health: %d%%", data.stateOfHealth);
    ESP_LOGI(TAG, "Remaining Capacity: %d mAh", data.remainingCapacity);
    ESP_LOGI(TAG, "Full Charge Capacity: %d mAh", data.fullChargeCapacity);
    ESP_LOGI(TAG, "Full Available Capacity: %d mAh", data.fullAvailableCapacity);
    ESP_LOGI(TAG, "Nominal Available Capacity: %d mAh", data.nominalAvailableCapacity);
    
    // Status Flags
    ESP_LOGI(TAG, "--- Status Flags ---");
    ESP_LOGI(TAG, "Battery Detected: %s", data.batteryDetected ? "YES" : "NO");
    ESP_LOGI(TAG, "Initialization Complete: %s", data.initComplete ? "YES" : "NO");
    ESP_LOGI(TAG, "Sealed State: %s", data.sealedState ? "YES" : "NO");
    ESP_LOGI(TAG, "Charging Allowed: %s", data.chargeAllowed ? "YES" : "NO");
    ESP_LOGI(TAG, "Full Charge: %s", data.fullCharge ? "YES" : "NO");
    ESP_LOGI(TAG, "Discharging: %s", data.discharging ? "YES" : "NO");
    ESP_LOGI(TAG, "Over Temperature: %s", data.overTemperature ? "YES" : "NO");
    ESP_LOGI(TAG, "Under Temperature: %s", data.underTemperature ? "YES" : "NO");
    
    // Raw Register Values
    ESP_LOGI(TAG, "--- Raw Register Values ---");
    ESP_LOGI(TAG, "Flags: 0x%04X", data.flags);
    ESP_LOGI(TAG, "Control Status: 0x%04X", data.controlStatus);
    ESP_LOGI(TAG, "================================");
}

// Control functions
bool BQ27426::battery_insert() {
    return execute_control_command(BQ27426_BAT_INSERT);
}

bool BQ27426::battery_remove() {
    return execute_control_command(BQ27426_BAT_REMOVE);
}

bool BQ27426::reset() {
    return execute_control_command(BQ27426_RESET);
}

bool BQ27426::soft_reset() {
    return execute_control_command(BQ27426_SOFT_RESET);
}

const char* BQ27426::decode_flags_to_string(uint16_t flags) {
    static char flagStr[256];
    flagStr[0] = '\0';
    
    if (flags & BQ27426_FLAG_OT) strcat(flagStr, "OT ");
    if (flags & BQ27426_FLAG_UT) strcat(flagStr, "UT ");
    if (flags & BQ27426_FLAG_FC) strcat(flagStr, "FC ");
    if (flags & BQ27426_FLAG_CHG) strcat(flagStr, "CHG ");
    if (flags & BQ27426_FLAG_OCVTAKEN) strcat(flagStr, "OCVTAKEN ");
    if (flags & BQ27426_FLAG_ITPOR) strcat(flagStr, "ITPOR ");
    if (flags & BQ27426_FLAG_CFGUPMODE) strcat(flagStr, "CFGUPMODE ");
    if (flags & BQ27426_FLAG_BAT_DET) strcat(flagStr, "BAT_DET ");
    if (flags & BQ27426_FLAG_SOC1) strcat(flagStr, "SOC1 ");
    if (flags & BQ27426_FLAG_SOCF) strcat(flagStr, "SOCF ");
    if (flags & BQ27426_FLAG_DSG) strcat(flagStr, "DSG ");
    
    return flagStr;
}