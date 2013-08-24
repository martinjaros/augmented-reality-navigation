/*
 * Inertial measurement unit - calibration utilities
 *
 * Copyright (C) 2013 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
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
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <sys/timerfd.h>

#include "debug.h"
#include "imu.h"
#include "imu-driver.h"

/* I/O buffer size */
#define BUFFER_SIZE     128

/* Number of calibration steps */
#define CALIB_STEPS     30

/* Default measurement relative weights */
#define MAG_WEIGHT      0.2
#define ACC_WEIGHT      0.2

int imu_calib_load(struct imucalib *calib, const char *source)
{
    DEBUG("imu_calib_load()");
    assert(calib != NULL);
    assert(source != NULL);

    FILE *f;
    if((f = fopen(source, "r")) == NULL)
    {
        WARN("Failed to open `%s`", source);
        return 0;
    }

    bzero(calib, sizeof(struct imucalib));
    char buf[BUFFER_SIZE];
    while(fgets(buf, BUFFER_SIZE, f) != NULL)
    {
        char *str = buf;

        // Skip empty lines and '#' comments
        while(*str == ' ') str++;
        if((*str == '\n') || (*str == '#')) continue;

        // Parse line
        if(sscanf(str, "gyro_offset_x = %lf", &calib->gyro_offset_x) != 1)
        if(sscanf(str, "gyro_offset_y = %lf", &calib->gyro_offset_y) != 1)
        if(sscanf(str, "gyro_offset_z = %lf", &calib->gyro_offset_z) != 1)
        if(sscanf(str, "mag_deviation_x = %lf", &calib->mag_deviation_x) != 1)
        if(sscanf(str, "mag_deviation_y = %lf", &calib->mag_deviation_y) != 1)
        if(sscanf(str, "mag_deviation_z = %lf", &calib->mag_deviation_z) != 1)
        if(sscanf(str, "mag_declination = %lf", &calib->mag_declination) != 1)
        if(sscanf(str, "mag_inclination = %lf", &calib->mag_inclination) != 1)
        if(sscanf(str, "mag_weight = %lf", &calib->mag_weight) != 1)
        if(sscanf(str, "acc_weight = %lf", &calib->acc_weight) != 1)
        {
            WARN("Parse error");
            return 0;
        }
    }

    INFO("Calibration(1/2) gyro_offset_x = %lf, gyro_offset_y = %lf, gyro_offset_z = %lf, mag_deviation_x = %lf, mag_deviation_y = %lf, mag_deviation_z = %lf",
         calib->gyro_offset_x, calib->gyro_offset_y, calib->gyro_offset_z, calib->mag_deviation_x, calib->mag_deviation_y, calib->mag_deviation_z);
    INFO("Calibration(2/2) mag_declination = %lf, mag_inclination = %lf, mag_weight = %lf, acc_weight = %lf",
         calib->mag_declination, calib->mag_inclination, calib->mag_weight, calib->acc_weight);
    return 1;
}

int imu_calib_save(const struct imucalib *calib, const char *dest)
{
    DEBUG("imu_calib_save()");
    assert(calib != NULL);
    assert(dest != NULL);

    FILE *f;
    if((f = fopen(dest, "w")) == NULL)
    {
        WARN("Failed to open `%s`", dest);
        return 0;
    }

    fprintf(f, "# Gyroscope zero offset (radian per second)\n"
            "gyro_offset_x = %lf\n"
            "gyro_offset_y = %lf\n"
            "gyro_offset_z = %lf\n"
            "\n# Compass deviation (microtesla)\n"
            "mag_deviation_x = %lf\n"
            "mag_deviation_y = %lf\n"
            "mag_deviation_z = %lf\n"
            "\n# Compass declination and inclination (radian)\n"
            "mag_declination = %lf\n"
            "mag_inclination = %lf\n"
            "\n# Relative weight of the compass and accelerometer measurements\n"
            "mag_weight = %lf\n"
            "acc_weight = %lf\n",
            calib->gyro_offset_x, calib->gyro_offset_y, calib->gyro_offset_z, calib->mag_deviation_x, calib->mag_deviation_y, calib->mag_deviation_z,
            calib->mag_declination, calib->mag_inclination, calib->mag_weight, calib->acc_weight);
    fclose(f);
    return 1;
}

int imu_calib_generate(struct imucalib *calib, const char *devname)
{
    DEBUG("imu_calib_generate()");
    assert(calib != NULL);

    int i, fd;
    struct driver_data data;
    double gyro[3] = { 0, 0, 0 }, mag[3] = { 0, 0, 0 };
    bzero(calib, sizeof(struct imucalib));
    calib->mag_weight = MAG_WEIGHT;
    calib->acc_weight = ACC_WEIGHT;

    // Open device
    if((fd = open(devname, O_RDWR)) == -1)
    {
        WARN("Failed to open `%s`", devname);
        return 0;
    }

    // Discard first measurement
    if(!driver_read(fd, &data)) goto error;

    printf("Starting calibration, point device to true north and press enter");
    getchar();
    for(i = 0; i < CALIB_STEPS; i++)
    {
        if(!driver_read(fd, &data)) goto error;
        gyro[0] += data.gyro[0];
        gyro[1] += data.gyro[1];
        gyro[2] += data.gyro[2];
        mag[0] += data.mag[0];
        mag[1] += data.mag[1];
        mag[2] += data.mag[2];
    }
    calib->gyro_offset_x = gyro[0] / CALIB_STEPS;
    calib->gyro_offset_y = gyro[1] / CALIB_STEPS;
    calib->gyro_offset_z = gyro[2] / CALIB_STEPS;
    mag[0] /= CALIB_STEPS;
    mag[1] /= CALIB_STEPS;
    mag[2] /= CALIB_STEPS;
    calib->mag_deviation_x = sqrt(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);
    calib->mag_declination = acos(mag[0] / sqrt(mag[0]*mag[0] + mag[1]*mag[1]));
    calib->mag_inclination = M_PI_2 - (mag[2] / calib->mag_deviation_x);

    printf("Point device to east and press enter");
    getchar();
    mag[0] = 0; mag[1] = 0; mag[2] = 0;
    for(i = 0; i < CALIB_STEPS; i++)
    {
        if(!driver_read(fd, &data)) goto error;
        mag[0] += data.mag[0];
        mag[1] += data.mag[1];
        mag[2] += data.mag[2];
    }
    mag[0] /= CALIB_STEPS;
    mag[1] /= CALIB_STEPS;
    mag[2] /= CALIB_STEPS;
    calib->mag_deviation_y = sqrt(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);

    printf("Point device to ground and press enter");
    getchar();
    mag[0] = 0; mag[1] = 0; mag[2] = 0;
    for(i = 0; i < CALIB_STEPS; i++)
    {
        if(!driver_read(fd, &data)) goto error;
        mag[0] += data.mag[0];
        mag[1] += data.mag[1];
        mag[2] += data.mag[2];
    }
    mag[0] /= CALIB_STEPS;
    mag[1] /= CALIB_STEPS;
    mag[2] /= CALIB_STEPS;
    calib->mag_deviation_y = sqrt(mag[0]*mag[0] + mag[1]*mag[1] + mag[2]*mag[2]);
    double avg = (calib->mag_deviation_x + calib->mag_deviation_y + calib->mag_deviation_z) / 3.0;
    calib->mag_deviation_x -= avg;
    calib->mag_deviation_y -= avg;
    calib->mag_deviation_z -= avg;
    printf("Calibration is finished\n");

    INFO("Calibration(1/2) gyro_offset_x = %lf, gyro_offset_y = %lf, gyro_offset_z = %lf, mag_deviation_x = %lf, mag_deviation_y = %lf, mag_deviation_z = %lf",
         calib->gyro_offset_x, calib->gyro_offset_y, calib->gyro_offset_z, calib->mag_deviation_x, calib->mag_deviation_y, calib->mag_deviation_z);
    INFO("Calibration(2/2) mag_declination = %lf, mag_inclination = %lf, mag_weight = %lf, acc_weight = %lf",
         calib->mag_declination, calib->mag_inclination, calib->mag_weight, calib->acc_weight);

    close(fd);
    return 1;

error:
    WARN("Failed to read from driver");
    close(fd);
    return 0;
}
