/* -----------------------------------------------------------------------------
 * Component Name: Sensors
 * Author(s): Harald Magnusson
 * Purpose: Poll sensors and store the latest data. Provide an interface for
 *          other components to read the data.
 * -----------------------------------------------------------------------------
 */

#pragma once

/* The out_of_date flag shows if the available data is the latest (value: 0)
 * or if an error occured in the respective module while updating (value: 1).
 * If an error has occured, the data in the struct is the latest valid data.
 */

typedef struct{
    float lat, lon, alt;
    char out_of_date;
} gps_t;

typedef struct{
    double x, y, z;
    char out_of_date;
} gyro_t;

typedef struct{
    double az, alt_ang;
    char out_of_date;
} encoder_t;

/* initialise the sensors component */
int init_sensors( void );

/* fetch the latest gps data */
void get_gps(gps_t* gps);

/* fetch the latest encoder data */
void get_encoder(encoder_t* encoder);

/* fetch the latest gyro data */
void get_gyro(gyro_t* gyro);