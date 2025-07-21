
#include "tps65185.h"
#include "pca9555.h"
#include "epd_board.h"
#include "esp_err.h"
#include "esp_log.h"

#include <driver/i2c.h>
#include <stdint.h>

static const int EPDIY_TPS_ADDR = 0x68;

static uint8_t i2c_master_read_slave(i2c_port_t i2c_num, int reg) {
    uint8_t r_data[1];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (EPDIY_TPS_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    ESP_ERROR_CHECK(i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (EPDIY_TPS_ADDR << 1) | I2C_MASTER_READ, true);
    /*
        if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, I2C_MASTER_ACK);
    }
    */
    i2c_master_read_byte(cmd, r_data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ESP_ERROR_CHECK(i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);

    return r_data[0];
}


void tps65185_debug_status(i2c_port_t i2c_num) {
    printf("\n=== TPS65185 Debug Status ===\n");
    
    // Read REVID register first to confirm communication
    uint8_t revid = i2c_master_read_slave(i2c_num, 0x10);
    printf("REVID: 0x%02X ", revid);
    switch(revid) {
        case 0x45: printf("(TPS65185 1p0)\n"); break;
        case 0x55: printf("(TPS65185 1p1)\n"); break;
        case 0x65: printf("(TPS65185 1p2)\n"); break;
        case 0x66: printf("(TPS651851 1p0)\n"); break;
        default: printf("(Unknown version)\n"); break;
    }
    
    // Read ENABLE register (0x01)
    uint8_t enable = i2c_master_read_slave(i2c_num, 0x01);
    printf("ENABLE (0x01): 0x%02X\n", enable);
    printf("  ACTIVE: %s\n", (enable & 0x80) ? "YES" : "NO");
    printf("  STANDBY: %s\n", (enable & 0x40) ? "YES" : "NO");
    printf("  V3P3_EN: %s\n", (enable & 0x20) ? "ON" : "OFF");
    printf("  VCOM_EN: %s\n", (enable & 0x10) ? "ON" : "OFF");
    printf("  VDDH_EN: %s\n", (enable & 0x08) ? "ON" : "OFF");
    printf("  VPOS_EN: %s\n", (enable & 0x04) ? "ON" : "OFF");
    printf("  VEE_EN: %s\n", (enable & 0x02) ? "ON" : "OFF");
    printf("  VNEG_EN: %s\n", (enable & 0x01) ? "ON" : "OFF");
    
    // Read Power Good Status (0x0F)
    uint8_t pg_status = i2c_master_read_slave(i2c_num, 0x0F);
    printf("POWER GOOD (0x0F): 0x%02X\n", pg_status);
    printf("  VB_PG: %s\n", (pg_status & 0x80) ? "GOOD" : "BAD");
    printf("  VDDH_PG: %s\n", (pg_status & 0x40) ? "GOOD" : "BAD");
    printf("  VN_PG: %s\n", (pg_status & 0x20) ? "GOOD" : "BAD");
    printf("  VPOS_PG: %s\n", (pg_status & 0x10) ? "GOOD" : "BAD");
    printf("  VEE_PG: %s\n", (pg_status & 0x08) ? "GOOD" : "BAD");
    printf("  VNEG_PG: %s\n", (pg_status & 0x02) ? "GOOD" : "BAD");
    
    // Read Interrupt Status 1 (0x07)
    uint8_t int1 = i2c_master_read_slave(i2c_num, 0x07);
    printf("INT1 (0x07): 0x%02X\n", int1);
    if (int1 & 0x80) printf("  DTX: Temperature change detected\n");
    if (int1 & 0x40) printf("  TSD: Thermal shutdown\n");
    if (int1 & 0x20) printf("  HOT: Thermal warning\n");
    if (int1 & 0x10) printf("  TMST_HOT: Thermistor hot\n");
    if (int1 & 0x08) printf("  TMST_COLD: Thermistor cold\n");
    if (int1 & 0x04) printf("  UVLO: Undervoltage lockout\n");
    if (int1 & 0x02) printf("  ACQC: VCOM acquisition complete\n");
    if (int1 & 0x01) printf("  PRGC: VCOM programming complete\n");
    
    // Read Interrupt Status 2 (0x08)
    uint8_t int2 = i2c_master_read_slave(i2c_num, 0x08);
    printf("INT2 (0x08): 0x%02X\n", int2);
    if (int2 & 0x80) printf("  VB_UV: Positive boost undervoltage\n");
    if (int2 & 0x40) printf("  VDDH_UV: VDDH undervoltage\n");
    if (int2 & 0x20) printf("  VN_UV: Inverting buck-boost undervoltage\n");
    if (int2 & 0x10) printf("  VPOS_UV: VPOS undervoltage\n");
    if (int2 & 0x08) printf("  VEE_UV: VEE undervoltage\n");
    if (int2 & 0x04) printf("  VCOMF: VCOM fault\n");
    if (int2 & 0x02) printf("  VNEG_UV: VNEG undervoltage\n");
    if (int2 & 0x01) printf("  EOC: ADC conversion complete\n");
    
    // Read Voltage Adjustment (0x02)
    uint8_t vadj = i2c_master_read_slave(i2c_num, 0x02);
    printf("VADJ (0x02): 0x%02X\n", vadj);
    uint8_t vset = vadj & 0x07;
    switch(vset) {
        case 0x03: printf("  VPOS/VNEG: ±15.000V\n"); break;
        case 0x04: printf("  VPOS/VNEG: ±14.750V\n"); break;
        case 0x05: printf("  VPOS/VNEG: ±14.500V\n"); break;
        case 0x06: printf("  VPOS/VNEG: ±14.250V\n"); break;
        default: printf("  VPOS/VNEG: Invalid setting\n"); break;
    }
    
    // Read VCOM settings (0x03 and 0x04)
    uint8_t vcom1 = i2c_master_read_slave(i2c_num, 0x03);
    uint8_t vcom2 = i2c_master_read_slave(i2c_num, 0x04);
    uint16_t vcom_val = ((vcom2 & 0x01) << 8) | vcom1;
    float vcom_voltage = vcom_val * -0.01f; // -10mV per LSB
    printf("VCOM1 (0x03): 0x%02X\n", vcom1);
    printf("VCOM2 (0x04): 0x%02X\n", vcom2);
    printf("  VCOM voltage: %.2fV\n", vcom_voltage);
    printf("  ACQ bit: %s\n", (vcom2 & 0x80) ? "SET" : "CLEAR");
    printf("  PROG bit: %s\n", (vcom2 & 0x40) ? "SET" : "CLEAR");
    printf("  HiZ bit: %s\n", (vcom2 & 0x20) ? "SET" : "CLEAR");
    uint8_t avg = (vcom2 >> 3) & 0x03;
    printf("  AVG: %dx measurements\n", (1 << avg));
    
    // Read Temperature Value (0x00)
    uint8_t temp_val = i2c_master_read_slave(i2c_num, 0x00);
    printf("TMST_VALUE (0x00): 0x%02X\n", temp_val);
    int8_t temp_c = (int8_t)temp_val; // Signed temperature
    printf("  Temperature: %d°C\n", temp_c);
    
    // Read Thermistor Config (0x0D)
    uint8_t tmst1 = i2c_master_read_slave(i2c_num, 0x0D);
    printf("TMST1 (0x0D): 0x%02X\n", tmst1);
    printf("  READ_THERM: %s\n", (tmst1 & 0x80) ? "ACTIVE" : "IDLE");
    printf("  CONV_END: %s\n", (tmst1 & 0x20) ? "DONE" : "BUSY");
    uint8_t dt = tmst1 & 0x03;
    printf("  DT threshold: %d°C\n", dt + 2);
    
    // Read Thermistor Hot/Cold thresholds (0x0E)
    uint8_t tmst2 = i2c_master_read_slave(i2c_num, 0x0E);
    printf("TMST2 (0x0E): 0x%02X\n", tmst2);
    uint8_t cold_thresh = (tmst2 >> 4) & 0x0F;
    uint8_t hot_thresh = tmst2 & 0x0F;
    printf("  COLD threshold: %d°C\n", (int8_t)(cold_thresh - 7));
    printf("  HOT threshold: %d°C\n", hot_thresh + 42);
    
    // Read Power-up sequence (0x09)
    uint8_t upseq0 = i2c_master_read_slave(i2c_num, 0x09);
    printf("UPSEQ0 (0x09): 0x%02X\n", upseq0);
    printf("  VDDH_UP: STROBE%d\n", ((upseq0 >> 6) & 0x03) + 1);
    printf("  VPOS_UP: STROBE%d\n", ((upseq0 >> 4) & 0x03) + 1);
    printf("  VEE_UP: STROBE%d\n", ((upseq0 >> 2) & 0x03) + 1);
    printf("  VNEG_UP: STROBE%d\n", (upseq0 & 0x03) + 1);
    
    // Read Power-up delays (0x0A)
    uint8_t upseq1 = i2c_master_read_slave(i2c_num, 0x0A);
    printf("UPSEQ1 (0x0A): 0x%02X\n", upseq1);
    printf("  UDLY4: %dms\n", (((upseq1 >> 6) & 0x03) + 1) * 3);
    printf("  UDLY3: %dms\n", (((upseq1 >> 4) & 0x03) + 1) * 3);
    printf("  UDLY2: %dms\n", (((upseq1 >> 2) & 0x03) + 1) * 3);
    printf("  UDLY1: %dms\n", ((upseq1 & 0x03) + 1) * 3);
    
    printf("=== End Debug Status ===\n\n");
}


static esp_err_t i2c_master_write_slave(
    i2c_port_t i2c_num, uint8_t ctrl, uint8_t* data_wr, size_t size
) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (EPDIY_TPS_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, ctrl, true);

    i2c_master_write(cmd, data_wr, size, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t tps_write_register(i2c_port_t port, int reg, uint8_t value) {
    uint8_t w_data[1];
    esp_err_t err;

    w_data[0] = value;

    err = i2c_master_write_slave(port, reg, w_data, 1);
    return err;
}

uint8_t tps_read_register(i2c_port_t i2c_num, int reg) {
    return i2c_master_read_slave(i2c_num, reg);
}

void tps_set_vcom(i2c_port_t i2c_num, unsigned vcom_mV) {
    unsigned val = vcom_mV / 10;
    ESP_ERROR_CHECK(tps_write_register(i2c_num, 4, (val & 0x100) >> 8));
    ESP_ERROR_CHECK(tps_write_register(i2c_num, 3, val & 0xFF));
}

int8_t tps_read_thermistor(i2c_port_t i2c_num) {
    tps_write_register(i2c_num, TPS_REG_TMST1, 0x80);
    int tries = 0;
    while (true) {
        uint8_t val = tps_read_register(i2c_num, TPS_REG_TMST1);
        // temperature conversion done
        if (val & 0x20) {
            break;
        }
        tries++;

        if (tries >= 100) {
            ESP_LOGE("epdiy", "thermistor read timeout!");
            break;
        }
    }
    return (int8_t)tps_read_register(i2c_num, TPS_REG_TMST_VALUE);
}

void tps_vcom_kickback() {
    printf("VCOM Kickback test\n");
    // pull the WAKEUP pin and the PWRUP pin high to enable all output rails.
    epd_current_board()->measure_vcom(epd_ctrl_state());
    // set the HiZ bit in the VCOM2 register (BIT 5) 0x20
    // this puts the VCOM pin in a high-impedance state.
    // bit 3 & 4 Number of acquisitions that is averaged to a single kick-back V. measurement
    tps_write_register(I2C_NUM_0, 4, 0x38);
    vTaskDelay(1);

    uint8_t int1reg = tps_read_register(I2C_NUM_0, TPS_REG_INT1);
    uint8_t vcomreg = tps_read_register(I2C_NUM_0, TPS_REG_VCOM2);
}

void tps_vcom_kickback_start() {
    uint8_t int1reg = tps_read_register(I2C_NUM_0, TPS_REG_INT1);
    // set the ACQ bit in the VCOM2 register to 1 (BIT 7)
    tps_write_register(I2C_NUM_0, TPS_REG_VCOM2, 0xA0);
}

unsigned tps_vcom_kickback_rdy() {
    uint8_t int1reg = tps_read_register(I2C_NUM_0, TPS_REG_INT1);

    if (int1reg == 0x02) {
        uint8_t lsb = tps_read_register(I2C_NUM_0, 3);
        uint8_t msb = tps_read_register(I2C_NUM_0, 4);
        int u16Value = (lsb | (msb << 8)) & 0x1ff;
        ESP_LOGI("vcom", "raw value:%d temperature:%d C", u16Value, tps_read_thermistor(I2C_NUM_0));
        return u16Value * 10;
    } else {
        return 0;
    }
}