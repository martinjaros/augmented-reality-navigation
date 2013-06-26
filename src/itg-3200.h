/**
 * @file
 * @brief       ITG-3200 definitions
 * @author      Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details at
 * <http://www.gnu.org/licenses>
 *
 * @section DESCRIPTION
 * Here are defines specific for the InvenSense ITG-3200 3-axis gyroscope.
 */

#ifndef ITG3200_H
#define ITG3200_H

/**
 * @brief Register 0 – Who Am I
 */
#define ITG3200_REG_WHO_AM_I        0x00

/**
 * @brief Register 21 – Sample Rate Divider
 */
#define ITG3200_REG_SMPLRT_DIV      0x15

/**
 * @brief Register 22 – DLPF, Full Scale
 */
#define ITG3200_REG_DLPF_FS         0x16

/**
 * @brief Register 23 – Interrupt Configuration
 */
#define ITG3200_REG_INT_CFG         0x17

/**
 * @brief Register 26 – Interrupt Status
 */
#define ITG3200_REG_INT_STATUS      0x1A

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_TEMP_OUT_H      0x1B

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_TEMP_OUT_L      0x1C

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_XOUT_H     0x1D

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_XOUT_L     0x1E

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_YOUT_H     0x1F

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_YOUT_L     0x20

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_ZOUT_H     0x21

/**
 * @brief Registers 27 to 34 – Sensor Registers
 */
#define ITG3200_REG_GYRO_ZOUT_L     0x22

/**
 * @brief Register 62 – Power Management
 */
#define ITG3200_REG_PWR_MGM         0x3E

/**
 * @brief ITG-3200 I2C slave address (AD0 low)
 */
#define ITG3200_I2C_ADDR0           0x68

/**
 * @brief ITG-3200 I2C slave address (AD0 high)
 */
#define ITG3200_I2C_ADDR1           0x69

#endif /* ITG3200_H */
