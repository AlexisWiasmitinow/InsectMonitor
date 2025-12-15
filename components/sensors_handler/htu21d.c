/*
 * HTU21D Component
 *
 * esp-idf component to interface with HTU21D humidity and temperature sensor
 * by TE Connectivity (http://www.te.com/usa-en/product-CAT-HSC0004.html)
 *
 * Luca Dentella, www.lucadentella.it
 */
 
 
// Component header file
#include "htu21d.h"

// Error library
#include "esp_err.h"
 
// FreeRTOS (for delay)
#include "freertos/task.h"

// Logging
#include "esp_log.h"


#define HTU21D_DELAY_MS					500

static const char *TAG = "HTU21D_driver";

static uint16_t read_value(uint8_t command);
static bool is_crc_valid(uint16_t value, uint8_t crc);

static i2c_port_t port = I2C_NUM_0;

int htu21d_init(i2c_port_t _port) {
	
	esp_err_t ret;

	if ((_port == I2C_NUM_0) || (_port == I2C_NUM_1)) {
		port = _port;
	} else {
		return HTU21D_ERR_INVALID_ARG;
	}

	// verify if a sensor is present
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
    i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK)
		return HTU21D_ERR_NOTFOUND;
	
	return HTU21D_ERR_OK;
}

float htu21d_read_temperature() {

	// get the raw value from the sensor
	uint16_t raw_temperature = read_value(TRIGGER_TEMP_MEASURE_NOHOLD);
	if(raw_temperature == 0) return -1000;
	// ESP_LOGE(TAG,"Raw temperature %d\r\n", raw_temperature);
	
	// return the real value, formula in datasheet
	return (raw_temperature * 175.72 / 65536.0) - 46.85;
}

float htu21d_read_humidity() {

	// get the raw value from the sensor
	uint16_t raw_humidity = read_value(TRIGGER_HUMD_MEASURE_NOHOLD);
	if(raw_humidity == 0) return -1000;
	// ESP_LOGE(TAG,"Raw humidity %d\r\n", raw_humidity);
	
	// return the real value, formula in datasheet
	return (raw_humidity * 125.0 / 65536.0) - 6.0;
}

uint8_t htu21d_get_resolution() {

	uint8_t reg_value = ht21d_read_user_register();
	return reg_value & 0b10000001;
}

int htu21d_set_resolution(uint8_t resolution) {
	
	// get the actual resolution
	uint8_t reg_value = ht21d_read_user_register();
	reg_value &= 0b10000001;
	
	// update the register value with the new resolution
	resolution &= 0b10000001;
	reg_value |= resolution;
	
	return ht21d_write_user_register(reg_value);
}

int htu21d_soft_reset() {
	
	esp_err_t ret;

	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, SOFT_RESET, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	
	switch(ret) {
		
		case ESP_ERR_INVALID_ARG:
			return HTU21D_ERR_INVALID_ARG;
			
		case ESP_FAIL:
			return HTU21D_ERR_FAIL;
		
		case ESP_ERR_INVALID_STATE:
			return HTU21D_ERR_INVALID_STATE;
		
		case ESP_ERR_TIMEOUT:
			return HTU21D_ERR_TIMEOUT;
	}
	return HTU21D_ERR_OK;
}

uint8_t ht21d_read_user_register() {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, READ_USER_REG, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	// receive the answer
	uint8_t reg_value;
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, &reg_value, 0x01);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	return reg_value;
}

int ht21d_write_user_register(uint8_t value) {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, WRITE_USER_REG, true);
	i2c_master_write_byte(cmd, value, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	
	switch(ret) {
		
		case ESP_ERR_INVALID_ARG:
			return HTU21D_ERR_INVALID_ARG;
			
		case ESP_FAIL:
			return HTU21D_ERR_FAIL;
		
		case ESP_ERR_INVALID_STATE:
			return HTU21D_ERR_INVALID_STATE;
		
		case ESP_ERR_TIMEOUT:
			return HTU21D_ERR_TIMEOUT;
	}
	return HTU21D_ERR_OK;
}

static uint16_t read_value(uint8_t command) {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, command, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	// wait for the sensor (100ms)
	vTaskDelay(pdMS_TO_TICKS(HTU21D_DELAY_MS / 5));
	
	// receive the answer
	uint8_t msb, lsb, crc;
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, &msb, 0x00);
	i2c_master_read_byte(cmd, &lsb, 0x00);
	i2c_master_read_byte(cmd, &crc, 0x01);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(HTU21D_DELAY_MS));
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	uint16_t raw_value = ((uint16_t) msb << 8) | (uint16_t) lsb;
	if(!is_crc_valid(raw_value, crc)) {
        ESP_LOGE(TAG,"CRC invalid\r\n");
    }
	return raw_value & 0xFFFC;
}

// verify the CRC, algorithm in the datasheet (see comments below)
static bool is_crc_valid(uint16_t value, uint8_t crc) {
	
	// line the bits representing the input in a row (first data, then crc)
	uint32_t row = (uint32_t)value << 8;
	row |= crc;
	
	// polynomial = x^8 + x^5 + x^4 + 1 
	// padded with zeroes corresponding to the bit length of the CRC
	uint32_t divisor = (uint32_t)0x988000;
	
	for (int i = 0 ; i < 16 ; i++) {
		
		// if the input bit above the leftmost divisor bit is 1, 
		// the divisor is XORed into the input
		if (row & (uint32_t)1 << (23 - i)) row ^= divisor;
		
		// the divisor is then shifted one bit to the right
		divisor >>= 1;
	}
	
	// the remainder should equal zero if there are no detectable errors
	return (row == 0);
}

