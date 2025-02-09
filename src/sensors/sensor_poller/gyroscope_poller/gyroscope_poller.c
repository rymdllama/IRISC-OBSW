/* -----------------------------------------------------------------------------
 * Component Name: Gyroscope Poller
 * Parent Component: Sensor Poller
 * Author(s): Harald Magnusson
 * Purpose: Poll the gyroscopes for the current angular motion of the telescope.
 * -----------------------------------------------------------------------------
 */

#define _GNU_SOURCE

#include <ftd2xx.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "global_utils.h"
#include "sensors.h"
#include "gyroscope.h"
#include "gpio.h"
#include "mode.h"

#define SERIAL_NUM "FT2GZ6PG"
#define DATAGRAM_IDENTIFIER 0x94
#define DATAGRAM_SIZE 27
#define FTDI_BAUDRATE 921600

static void* thread_func(void* args);
static void active_m(void);

static FT_HANDLE fd;
static FILE* gyro_log;

pthread_mutex_t mutex_cond_gyro = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_gyro = PTHREAD_COND_INITIALIZER;

int init_gyroscope_poller(void* args){

    /* set up log file */
    char log_fn[100];

    strcpy(log_fn, get_top_dir());
    strcat(log_fn, "output/logs/gyro.log");

    gyro_log = fopen(log_fn, "a");
    if(gyro_log == NULL){
        logging(ERROR, "Gyro",
            "Failed to open gyro log file, (%s)",
            strerror(errno));
        return errno;
    }

    /* gpio pin is used for trigger to poll the gyroscope */
    int ret = gpio_export(GYRO_TRIG_PIN);
    if(ret != SUCCESS){
        return ret;
    }

    ret = gpio_direction(GYRO_TRIG_PIN, OUT);
    if(ret != SUCCESS){
        return ret;
    }

    gpio_write(GYRO_TRIG_PIN, HIGH);

    /* ftdi setup:
     * baud rate = FTDI_BAUDRATE
     * message time out = 4 ms
     * lowest latency setting (2 ms)
     */
    FT_STATUS stat;

    stat = FT_OpenEx(SERIAL_NUM, FT_OPEN_BY_SERIAL_NUMBER, &fd);
    if(stat != FT_OK){
        logging(ERROR, "GYRO",
                "Failed to initiate UART, error: %d", stat);
        return FAILURE;
    }

    stat = FT_SetBaudRate(fd, FTDI_BAUDRATE);
    if(stat != FT_OK){
        logging(ERROR, "GYRO",
                "Failed to set baudrate for UART, error: %d", stat);
        return FAILURE;
    }

    stat = FT_SetTimeouts(fd, 4, 4);
    if(stat != FT_OK){
        logging(ERROR, "GYRO",
                "Failed to set timeout for UART, error: %d", stat);
        return FAILURE;
    }

    stat = FT_SetLatencyTimer(fd, 2);
    if(stat != FT_OK){
        logging(ERROR, "GYRO",
                "Failed to set latency timer for UART, error: %d", stat);
        return FAILURE;
    }

    return create_thread("gyro_poller", thread_func, 40);
}

static void* thread_func(void* args){

    pthread_mutex_lock(&mutex_cond_gyro);

    struct timespec wake_time;
    int ret;

    ret = FT_Purge(fd, FT_PURGE_RX);
    if(ret != FT_OK){
        logging(WARN, "GYRO",
                "Failed to purge UART receive buffer: %d", ret);
    }

    while(1){

        pthread_cond_wait(&cond_gyro, &mutex_cond_gyro);

        clock_gettime(CLOCK_MONOTONIC, &wake_time);

        while(get_mode() != RESET){
            active_m();

            wake_time.tv_nsec += GYRO_SAMPLE_TIME;
            if(wake_time.tv_nsec >= 1000000000){
                wake_time.tv_sec++;
                wake_time.tv_nsec -= 1000000000;
            }
            clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wake_time, NULL);
        }
    }

    return NULL;
}

static gyro_t gyro;
static unsigned char data[DATAGRAM_SIZE];
static unsigned int bytes_read, bytes_available;
static double rate[3];
static int ret;

static void active_m(void){

    /* create trigger pulse */
    gpio_write(GYRO_TRIG_PIN, LOW);
    usleep(1);
    gpio_write(GYRO_TRIG_PIN, HIGH);

    /* wait for UART conversion */
    usleep(2000);

    /* find start of datagram */
    do{
        FT_Read(fd, &data[0], 1, &bytes_read);
    } while(data[0] != DATAGRAM_IDENTIFIER);

    /* wait until full datagram is available */
    do{
        FT_GetQueueStatus(fd, &bytes_available);
        usleep(1);
    } while(bytes_available < DATAGRAM_SIZE-1);

    /* read full datagram */
    ret = FT_Read(fd, &data[1], DATAGRAM_SIZE-1, &bytes_read);
    if(ret != FT_OK){
        logging(WARN, "Gyro", "Reading datagram failed, "
                "error: %d", ret);
        return;
    }

    /* check for datagram termination */
    if(     data[DATAGRAM_SIZE-2] != '\r' ||
            data[DATAGRAM_SIZE-1] != '\n'){

        logging(WARN, "Gyro", "Incorrect datagram received");
        return;
    }

    /* check gyroscope data quality */
    if(data[10]){
        logging(WARN, "Gyro",
                "Bad gyroscope data quality, status byte: %2x", data[10]);
        gyro_out_of_date();
        return;
    }

    /* calculate gyroscope data */
    for(int ii=0; ii<3; ++ii){
        unsigned char buffer[4];
        buffer[0] = data[3+3*ii];
        buffer[1] = data[2+3*ii];
        buffer[2] = data[1+3*ii];
        buffer[3] = (data[1+3*ii] & 0x80) ? 0xFF : 0x00;
        rate[ii] = (double)*(int32_t*)buffer / 16384.f;
    }

    /* save data in protected object */
    gyro.x = rate[0];
    gyro.y = rate[1];
    gyro.z = rate[2];

    set_gyro(&gyro);

    double temp = NAN;
    if(data[17] == 0){
        unsigned char buffer[2];
        buffer[0] = data[12];
        buffer[1] = data[11];
        temp = (double)*(int16_t*)buffer / 256.f;

        set_gyro_temp(temp);
    }
    else{
        logging(WARN, "Gyro", "Bad gyroscope temperature data quality: %2x",
                data[17]);
    }

    logging_csv(gyro_log, "%+011.6lf,%+011.6lf,%+011.6lf,%+011.6lf",
            gyro.x, gyro.y, gyro.z, temp);

    #if GYRO_DEBUG
        logging(DEBUG, "Gyro",
                "x: %+09.4lf\ty: %+09.4lf\tz: %+09.4lf\ttemp: %+09.4lf",
                gyro.x, gyro.y, gyro.z, temp);
    #endif
}
