#ifndef BQ27426_H
#define BQ27426_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// BQ27426 I2C Address (7-bit address)
#define BQ27426_I2C_ADDRESS 0x55

// I2C Configuration
#define BQ27426_I2C_MASTER_NUM     I2C_NUM_0
#define BQ27426_I2C_MASTER_FREQ_HZ 100000
#define BQ27426_I2C_MASTER_TIMEOUT_MS 1000

// Standard Commands (from reference manual Table 5-1)
#define BQ27426_CONTROL             0x00  // Control()
#define BQ27426_TEMPERATURE         0x02  // Temperature()
#define BQ27426_VOLTAGE             0x04  // Voltage()
#define BQ27426_FLAGS               0x06  // Flags()
#define BQ27426_NOMINAL_CAPACITY    0x08  // NominalAvailableCapacity()
#define BQ27426_FULL_CAPACITY       0x0A  // FullAvailableCapacity()
#define BQ27426_REMAINING_CAPACITY  0x0C  // RemainingCapacity()
#define BQ27426_FULL_CHARGE_CAP     0x0E  // FullChargeCapacity()
#define BQ27426_AVERAGE_CURRENT     0x10  // AverageCurrent()
#define BQ27426_AVERAGE_POWER       0x18  // AveragePower()
#define BQ27426_STATE_OF_CHARGE     0x1C  // StateOfCharge()
#define BQ27426_INTERNAL_TEMP       0x1E  // InternalTemperature()
#define BQ27426_STATE_OF_HEALTH     0x20  // StateOfHealth()

// Control Subcommands (from reference manual Table 5-2)
#define BQ27426_CONTROL_STATUS      0x0000
#define BQ27426_DEVICE_TYPE         0x0001
#define BQ27426_FW_VERSION          0x0002
#define BQ27426_DM_CODE             0x0004
#define BQ27426_CHEM_ID             0x0008
#define BQ27426_BAT_INSERT          0x000C
#define BQ27426_BAT_REMOVE          0x000D
#define BQ27426_SET_CFGUPDATE       0x0013
#define BQ27426_SHUTDOWN_ENABLE     0x001B
#define BQ27426_SHUTDOWN            0x001C
#define BQ27426_SEALED              0x0020
#define BQ27426_PULSE_SOC_INT       0x0023
#define BQ27426_RESET               0x0041
#define BQ27426_SOFT_RESET          0x0042

// Flags Register Bit Definitions (from reference manual Table 5-4)
#define BQ27426_FLAG_OT             0x8000  // Over Temperature
#define BQ27426_FLAG_UT             0x4000  // Under Temperature
#define BQ27426_FLAG_FC             0x0200  // Full Charge
#define BQ27426_FLAG_CHG            0x0100  // Charge allowed
#define BQ27426_FLAG_OCVTAKEN       0x0080  // OCV measurement taken
#define BQ27426_FLAG_ITPOR          0x0020  // POR occurred
#define BQ27426_FLAG_CFGUPMODE      0x0010  // Config update mode
#define BQ27426_FLAG_BAT_DET        0x0008  // Battery detected
#define BQ27426_FLAG_SOC1           0x0004  // SOC1 threshold reached
#define BQ27426_FLAG_SOCF           0x0002  // SOCF threshold reached
#define BQ27426_FLAG_DSG            0x0001  // Discharging detected

// Control Status Bit Definitions (from reference manual Table 5-3)
#define BQ27426_CTRL_SHUTDOWNEN     0x8000
#define BQ27426_CTRL_WDRESET        0x4000
#define BQ27426_CTRL_SS             0x2000  // Sealed State
#define BQ27426_CTRL_CALMODE        0x1000
#define BQ27426_CTRL_CCA            0x0800
#define BQ27426_CTRL_BCA            0x0400
#define BQ27426_CTRL_QMAX_UP        0x0200
#define BQ27426_CTRL_RES_UP         0x0100
#define BQ27426_CTRL_INITCOMP       0x0080  // Initialization Complete
#define BQ27426_CTRL_SLEEP          0x0010
#define BQ27426_CTRL_LDMD           0x0008
#define BQ27426_CTRL_RUP_DIS        0x0004
#define BQ27426_CTRL_VOK            0x0002

// Error codes
typedef enum {
    BQ27426_OK = 0,
    BQ27426_ERR_I2C_INIT,
    BQ27426_ERR_I2C_COMM,
    BQ27426_ERR_DEVICE_NOT_FOUND,
    BQ27426_ERR_NOT_INITIALIZED,
    BQ27426_ERR_TIMEOUT
} bq27426_err_t;

typedef struct {
    // Basic measurements
    uint16_t voltage;              // mV
    int16_t averageCurrent;        // mA (signed)
    int16_t averagePower;          // mW (signed)
    uint16_t temperature;          // 0.1°K
    uint16_t internalTemperature;  // 0.1°K
    
    // Capacity information
    uint16_t stateOfCharge;        // %
    uint16_t remainingCapacity;    // mAh
    uint16_t fullChargeCapacity;   // mAh
    uint16_t fullAvailableCapacity; // mAh
    uint16_t nominalAvailableCapacity; // mAh
    uint16_t stateOfHealth;        // %
    
    // Status information
    uint16_t flags;
    uint16_t controlStatus;
    uint16_t deviceType;
    uint16_t firmwareVersion;
    uint16_t chemicalID;
    
    // Temperature in Celsius (calculated)
    float temperatureC;
    float internalTemperatureC;
    
    // Status flags (decoded)
    bool overTemperature;
    bool underTemperature;
    bool fullCharge;
    bool chargeAllowed;
    bool batteryDetected;
    bool discharging;
    bool initComplete;
    bool sealedState;
} bq27426_data_t;

class BQ27426 {
private:
    bool initialized;
    i2c_port_t i2c_port;
    static const char* TAG;
    
    // Low-level I2C functions
    esp_err_t i2c_master_read_register(uint8_t reg, uint8_t* data, size_t len);
    esp_err_t i2c_master_write_register(uint8_t reg, uint16_t value);
    esp_err_t i2c_master_write_control_command(uint16_t command);
    
    // Helper functions
    uint16_t read_register(uint8_t reg);
    bool write_register(uint8_t reg, uint16_t value);
    bool write_control_command(uint16_t command);
    void decode_flags(uint16_t flags, bq27426_data_t &data);
    void decode_control_status(uint16_t status, bq27426_data_t &data);
    
public:
    BQ27426();
    ~BQ27426();
    
    // Initialization
    bq27426_err_t begin(int sda_pin = 39, int scl_pin = 49, uint32_t frequency = 100000);
    bool is_connected();
    void deinit();
    
    // Data reading functions
    bq27426_err_t read_all_data(bq27426_data_t &data);
    void print_state_data();
    void print_state_data(const bq27426_data_t &data);
    
    // Individual parameter reading
    uint16_t read_voltage();           // Returns voltage in mV
    int16_t read_average_current();    // Returns current in mA
    int16_t read_average_power();      // Returns power in mW
    uint16_t read_temperature();       // Returns temperature in 0.1°K
    uint16_t read_internal_temperature(); // Returns internal temp in 0.1°K
    uint16_t read_state_of_charge();   // Returns SOC in %
    uint16_t read_remaining_capacity(); // Returns capacity in mAh
    uint16_t read_full_charge_capacity(); // Returns capacity in mAh
    uint16_t read_flags();             // Returns flags register
    uint16_t read_control_status();    // Returns control status
    uint16_t read_device_type();       // Returns device type
    uint16_t read_firmware_version();  // Returns firmware version
    uint16_t read_chemical_id();       // Returns chemical ID
    
    // Control functions
    bool execute_control_command(uint16_t command);
    bool battery_insert();
    bool battery_remove();
    bool reset();
    bool soft_reset();
    
    // Utility functions
    float kelvin_to_celsius(uint16_t kelvin);
    const char* decode_flags_to_string(uint16_t flags);
};

#endif // BQ27426_H