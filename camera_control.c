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
#include <arpa/inet.h>

#include <PvApi.h>

#define MAX_CAMERA_LIST 20

void *camera_thread_func(void* arg)
{
  camera_parameters_t *camera_params;
  camera_params= (camera_parameters_t *) arg;
  struct timeval tv;
  long start_time;

  tPvCameraInfo   cameraList[MAX_CAMERA_LIST];
  unsigned long   cameraNum = 0;
  unsigned long   cameraRle;

  int ret;
  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  start_time = tv.tv_sec;
  
  g_print("Camera thread started\n");
  if((ret=PvInitialize()))
  {
    g_warning("failed to initialise the Camera API. Error %d\n", ret);
    return NULL;
  }

  while(!camera_params->camera_thread_shutdown) 
  {
    //We loop until the camera is connected and we asked for images
    while((!camera_params->camera_thread_shutdown)&&(!camera_params->grab_images || !camera_params->camera_connected ))
    {
      //If the camera is not connected, we search for it
      if(!camera_params->camera_connected )
      {
	// first, get list of reachable cameras.
	cameraNum = PvCameraList(cameraList,MAX_CAMERA_LIST,NULL);

	// keep how many cameras listed are reachable
	cameraRle = cameraNum;

	// then we append the list of unreachable cameras.
        if (cameraNum < MAX_CAMERA_LIST)
        {
            cameraNum += PvCameraListUnreachable(&cameraList[cameraNum],
                                                 MAX_CAMERA_LIST-cameraNum,
                                                 NULL);
        }


	g_print("We detected %ld Reachable camera%c (%ld total)\n",cameraRle,cameraRle>1 ? 's':' ',cameraNum);
	if(cameraNum)
	{
	  camera_params->camera_connected=1;
	  struct in_addr addr;
          tPvIpSettings Conf;
          tPvErr lErr;
            
	  if((lErr = PvCameraIpSettingsGet(cameraList[0].UniqueId,&Conf)) == ePvErrSuccess)
          {
            addr.s_addr = Conf.CurrentIpAddress;
	    g_print("First Camera\n");
            g_print("%s - %8s - Unique ID = % 8ld IP@ = %15s [%s]\n",cameraList[0].SerialString,
                                                                    cameraList[0].DisplayName,
                                                                    cameraList[0].UniqueId,
                                                                    inet_ntoa(addr),
                                                                    cameraList[0].PermittedAccess & ePvAccessMaster ? "available" : "in use");
          }
	}
	else
	  usleep(500000); //some waiting 

      }
      usleep(500000); //some waiting 
    }

  }

  // uninit the API
  PvUnInitialize();
  gettimeofday (&tv, (struct timezone *) NULL);

  g_print("Camera thread stopped after %ld seconds\n", tv.tv_sec-start_time);

  return NULL;
}
