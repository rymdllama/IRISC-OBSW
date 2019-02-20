/* -----------------------------------------------------------------------------
 * Component Name: Mode
 * Author(s): Harald Magnusson
 * Purpose: Store the current state of the software.
 *
 * -----------------------------------------------------------------------------
 */

#include <pthread.h>
#include <stdio.h>

#include "global_utils.h"
#include "mode.h"

static pthread_mutex_t mutex_mode;
static enum mode_t mode;

int init_mode( void ){

    mode = normal;

    int res = pthread_mutex_init( &mutex_mode, NULL );
    if( res ){
        fprintf(stderr,
            "The initialisation of the mutex failed with code %d.\n",
            res);
        return FAILURE;
    }

    return SUCCESS;
}

void set_mode( char ch ){

    pthread_mutex_lock( &mutex_mode );
    mode = ch;
    pthread_mutex_unlock( &mutex_mode );

}

char get_mode( void ){

    char ch;

    pthread_mutex_lock( &mutex_mode );
    ch = mode;
    pthread_mutex_unlock( &mutex_mode );

    return ch;
}