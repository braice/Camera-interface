/*
 * Program to control the GigE camera. Camera communication part
 *
 * Copyright (C) 2010 Brice Dubost 
 *
 * Copyright notice:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *                                                                                                                                         
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 *
 */


#include "camera.h"
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>

void *camera_thread_func(void* arg)
{
  camera_parameters_t *camera_params;
  camera_params= (camera_parameters_t *) arg;
  struct timeval tv;
  long start_time;




  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  start_time = tv.tv_sec;
  
  printf("Camera thread started\n");

  while(!camera_params->camera_thread_shutdown) 
  {
    while((!camera_params->camera_thread_shutdown)&&(!camera_params->grab_images || !camera_params->camera_connected ))
    {
      usleep(500000); //some waiting 
    }

  }

  gettimeofday (&tv, (struct timezone *) NULL);

  
  printf("Camera thread stopped after %ld seconds\n", tv.tv_sec-start_time);

  return NULL;
}
