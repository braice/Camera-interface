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
  gchar *msg;
  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  start_time = tv.tv_sec;
  
  g_print("Camera thread started\n");
  if((ret=PvInitialize()))
  {
    g_warning("failed to initialise the Camera API. Error %d\n", ret);
    msg = g_strdup_printf ("Failed to initialise the Camera API. Error %d", ret);
    gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
    gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
    g_free (msg);

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
            cameraNum += PvCameraListUnreachable(&cameraList[cameraNum], MAX_CAMERA_LIST-cameraNum, NULL);


	g_print("We detected %ld Reachable camera%c (%ld total)\n",cameraRle,cameraRle>1 ? 's':' ',cameraNum);
	if(cameraNum)
	{
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
	    gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
	    msg = g_strdup_printf ("Camera found : %s", cameraList[0].DisplayName);
	    gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
	    g_free (msg);
	    //We open the handler for the camera
	    ret=PvCameraOpen(cameraList[0].UniqueId,ePvAccessMaster,&camera_params->camera_handler);
	    if(ret!=ePvErrSuccess)
	    {
	      g_warning("Error while opening the camera : %d",ret);
	      gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
	      switch(ret)
	      {
	        case ePvErrAccessDenied:
	          msg = g_strdup_printf ("Camera error : Access denied (another apliation is using it ?)");
	          break;
	        case ePvErrNotFound:
	          msg = g_strdup_printf ("Camera error : Camera not found (camera unplugged ?)");
	          break;
  	        default:
	          msg = g_strdup_printf ("Camera error : Unknown error");
	      }
	      gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
	      g_free (msg);
	      usleep(1000000); //some waiting 
	      continue;
	    }
	    /****************** Camera information getting ************/
	    unsigned long sensorbits, sensorwidth,sensorheight, gainmin, gainmax;
	    //ret=PvAttrStringGet(camera_params->camera_handler,"DeviceModelName",ModelName,100,NULL);
	    PvAttrUint32Get(camera_params->camera_handler,"SensorBits",&sensorbits);
	    PvAttrUint32Get(camera_params->camera_handler,"SensorWidth",&sensorwidth); //todo : adjust the rolling buttons with these max values
	    PvAttrUint32Get(camera_params->camera_handler,"SensorHeight",&sensorheight);
	    PvAttrRangeUint32(camera_params->camera_handler,"GainValue",&gainmin,&gainmax);
	    gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
	    msg = g_strdup_printf ("Sensor information : %ldx%ld %ld bits. Gain Min: %ld Max: %ld",sensorwidth,sensorheight,sensorbits,gainmin,gainmax); //todo : put this in the camera info text
	    gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
	    g_free (msg);
	    /********************* Camera INIT *******************/
	    //Now we adjust the packet size
	    if(!PvCaptureAdjustPacketSize(camera_params->camera_handler,9000))
	      {
		unsigned long Size;
		PvAttrUint32Get(camera_params->camera_handler,"PacketSize",&Size);   
		g_print("the best packet size is %lu bytes\n",Size);
	      }
	    //We set the gain to manual
	    if(PvAttrStringSet(camera_params->camera_handler,"GainMode","Manual"))
	      g_print("Error while setting the gain mode\n");
	    //Default gain 0
	    PvAttrUint32Set(camera_params->camera_handler,"GainValue",0); //note, get the value of the spinbutton / reset this values when grabbing is selected
	    //Exposure mode manual
	    if(PvAttrStringSet(camera_params->camera_handler,"ExposureMode","Manual"))
	      g_print("Error while setting the exposure mode\n");
	    //Default Exposure 100ms
	    PvAttrUint32Set(camera_params->camera_handler,"ExposureValue",100000);
	    //PixelFormat Monochrome 16 bits
	    if(PvAttrStringSet(camera_params->camera_handler,"PixelFormat","Mono16"))
	      g_print("Error while setting the PixelFormat\n");

	    //We have a good camera handler, we set the camera as being connected
	    camera_params->camera_connected=1;
	    /********************* End of Camera INIT *******************/
          }
	}
	else
	  usleep(500000); //some waiting 

	//See snap example

      }
      usleep(500000); //some waiting 
    }

  }

  if(camera_params->camera_connected)
    PvCameraClose(camera_params->camera_handler);
  // uninit the API
  PvUnInitialize();
  gettimeofday (&tv, (struct timezone *) NULL);

  g_print("Camera thread stopped after %ld seconds\n", tv.tv_sec-start_time);

  return NULL;
}
