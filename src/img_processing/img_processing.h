/* -----------------------------------------------------------------------------
 * Component Name: Img Processing
 * Author(s): 
 * Purpose: Process and store images along with housekeeping data, as well as
 *          sending it to ground.
 * -----------------------------------------------------------------------------
 */

#pragma once

/* initialise the img processing component */
int init_img_processing(void* args);

/* enqueue an image with meta data in the queue to be processed */
void queue_image( void );
