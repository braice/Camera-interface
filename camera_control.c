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
#include <stdlib.h>

#define MAX_CAMERA_LIST 20

void camera_update_roi(camera_parameters_t* camera_params)
{
  g_print("update ROI");
  if(camera_params->roi_hard_active)
  {
    PvAttrUint32Set(camera_params->camera_handler,"Height",camera_params->roi_hard_height);
    PvAttrUint32Set(camera_params->camera_handler,"RegionX",camera_params->roi_hard_x);
    PvAttrUint32Set(camera_params->camera_handler,"RegionY",camera_params->roi_hard_y);
    PvAttrUint32Set(camera_params->camera_handler,"Width",camera_params->roi_hard_width);
  }
  else
  {
    unsigned long min,max;
    PvAttrRangeUint32(camera_params->camera_handler,"Height",&min,&max);
    PvAttrUint32Set(camera_params->camera_handler,"Height",max);
    PvAttrRangeUint32(camera_params->camera_handler,"RegionX",&min,&max);
    PvAttrUint32Set(camera_params->camera_handler,"RegionX",min);
    PvAttrRangeUint32(camera_params->camera_handler,"RegionY",&min,&max);
    PvAttrUint32Set(camera_params->camera_handler,"RegionY",min);
    PvAttrRangeUint32(camera_params->camera_handler,"Width",&min,&max);
    PvAttrUint32Set(camera_params->camera_handler,"Width",max);
  }
}

void camera_stop_grabbing(camera_parameters_t* camera_params)
{
  g_print("camera_stop_grabbing\n");
  PvCommandRun(camera_params->camera_handler,"AcquisitionStop");
  PvCaptureQueueClear(camera_params->camera_handler);
  PvCaptureEnd(camera_params->camera_handler);
  //We free the image buffer
  free(camera_params->camera_frame.ImageBuffer);
  camera_params->grabbing_images=0;
  gchar *msg; 
  gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
  msg = g_strdup_printf ("Acquisition Stopped");
  gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
  g_free (msg);

}


void camera_start_grabbing(camera_parameters_t* camera_params)
{
  g_print("camera_start_grabbing\n");
  
  //Set the exposure time
  PvAttrUint32Set(camera_params->camera_handler,"ExposureValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_time)*1000);
  //Set the gain
  PvAttrUint32Set(camera_params->camera_handler,"GainValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_gain));
  
  PvCaptureStart(camera_params->camera_handler);
  PvAttrEnumSet(camera_params->camera_handler, "AcquisitionMode", "Continuous");
  PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerMode","Freerun");

  //We initialize the frame buffer
  int imageSize;
  //2 bits per pixel
  imageSize=camera_params->sensorwidth*camera_params->sensorheight*2;
  camera_params->camera_frame.ImageBufferSize=imageSize;
  camera_params->camera_frame.ImageBuffer=calloc(imageSize,sizeof(char)*2);
  camera_params->camera_frame.AncillaryBufferSize=0;
  PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,NULL);

  PvCommandRun(camera_params->camera_handler, "AcquisitionStart");
  camera_params->grabbing_images=1;
  gchar *msg;
  gdk_threads_enter();
  gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
  msg = g_strdup_printf ("Acquisition Started");
  gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
  g_free (msg);
  gdk_threads_leave();

}

void camera_new_image(camera_parameters_t* camera_params)
{

  gdk_threads_enter();
  g_print("New Image\n");
  //note : in case of redim, unref the pixbuf, create a new one and associate it with the image
  //todo check the size of the pixbuf versus the size of the camera buffer
  //If we didn't set a pixbuf yet, we do it
  if(camera_params->raw_image_pixbuff==NULL)
  {
    //GdkPixbuf*  gdk_pixbuf_new(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height);
    camera_params->raw_image_pixbuff=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, camera_params->sensorwidth, camera_params->sensorheight);
    //    camera_params->raw_image_pixbuff=
    //gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 16, camera_params->sensorwidth, camera_params->sensorheight);
    //We associate this new pixbuf with the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(camera_params->objects->raw_image),camera_params->raw_image_pixbuff);
    g_object_ref(camera_params->raw_image_pixbuff);
  }
  //copy the pixels
  guchar *pixels;
  pixels=gdk_pixbuf_get_pixels(camera_params->raw_image_pixbuff);

  //convert black and white into RGB
  char pixel;
  int row, col, pos;
  //Todo use the buffer width
  for(row=0;row<camera_params->sensorheight;row++)
  {
    for(col=0;col<camera_params->sensorwidth;col++)
    {
      pos=row*camera_params->sensorwidth+col;
      //RED
      pixel=(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)+1])<<4)&0xF0) +(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)])>>4)&0x0F);
      pixels[0+3*(pos)]=pixel;
      //GREEN
      pixels[1+3*(pos)]=pixel;
      //BLUE
      pixels[2+3*(pos)]=pixel;
    }
  }

  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->raw_image);
  //ready for the next one
  PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,NULL);
  gdk_threads_leave();

}


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
    //We loop until the camera is connected
    while((!camera_params->camera_thread_shutdown)&&(!camera_params->camera_connected ))
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
	    msg = g_strdup_printf ("Camera : %s", cameraList[0].DisplayName);
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
	    unsigned long gainmin, gainmax, time_min, time_max,min,max;
	    //ret=PvAttrStringGet(camera_params->camera_handler,"DeviceModelName",ModelName,100,NULL);
	    PvAttrUint32Get(camera_params->camera_handler,"SensorBits",&camera_params->sensorbits);
	    PvAttrUint32Get(camera_params->camera_handler,"SensorWidth",&camera_params->sensorwidth);
	    PvAttrUint32Get(camera_params->camera_handler,"SensorHeight",&camera_params->sensorheight);
	    PvAttrRangeUint32(camera_params->camera_handler,"GainValue",&gainmin,&gainmax);
	    PvAttrRangeUint32(camera_params->camera_handler,"ExposureValue",&time_min,&time_max);
	    //We write the info in the text buffer
	    msg = g_strdup_printf ("Camera : %s\nSensor %ldx%ld %ld bits\nGain %lddB to %lddB", cameraList[0].DisplayName,camera_params->sensorwidth,camera_params->sensorheight,camera_params->sensorbits,gainmin,gainmax);
	    gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
	    g_free (msg);
	    //we set the min/max for the adjustments
	    gtk_adjustment_set_lower(camera_params->objects->Exp_adj_gain,gainmin);
	    gtk_adjustment_set_upper(camera_params->objects->Exp_adj_gain,gainmax);
	    gtk_adjustment_set_value(camera_params->objects->Exp_adj_gain,gainmin);
	    gtk_adjustment_set_lower(camera_params->objects->Exp_adj_time,time_min/1000+1);
	    gtk_adjustment_set_upper(camera_params->objects->Exp_adj_time,time_max/1000);
	    gtk_adjustment_set_value(camera_params->objects->Exp_adj_time,time_min/1000+1);
	    PvAttrRangeUint32(camera_params->camera_handler,"Height",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_height,min);
	    gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_height,max);
	    gtk_adjustment_set_value(camera_params->objects->ROI_adjust_height,max);
	    PvAttrRangeUint32(camera_params->camera_handler,"RegionX",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_x,min);
	    gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_x,max);
	    gtk_adjustment_set_value(camera_params->objects->ROI_adjust_x,min);
	    PvAttrRangeUint32(camera_params->camera_handler,"RegionY",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_y,min);
	    gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_y,max);
	    gtk_adjustment_set_value(camera_params->objects->ROI_adjust_y,min);
	    PvAttrRangeUint32(camera_params->camera_handler,"Width",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_width,min);
	    gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_width,max);
	    gtk_adjustment_set_value(camera_params->objects->ROI_adjust_width,max);
	    PvAttrRangeUint32(camera_params->camera_handler,"BinningX",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->Bin_X_adj,min);
	    gtk_adjustment_set_upper(camera_params->objects->Bin_X_adj,max);
	    gtk_adjustment_set_value(camera_params->objects->Bin_X_adj,min);
	    PvAttrRangeUint32(camera_params->camera_handler,"BinningY",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->Bin_Y_adj,min);
	    gtk_adjustment_set_upper(camera_params->objects->Bin_Y_adj,max);
	    gtk_adjustment_set_value(camera_params->objects->Bin_Y_adj,min);
	    PvAttrRangeUint32(camera_params->camera_handler,"StreamBytesPerSecond",&min,&max);
	    gtk_adjustment_set_lower(camera_params->objects->Bytes_per_sec_adj,min);
            gtk_adjustment_set_upper(camera_params->objects->Bytes_per_sec_adj,max);
	    if((min<12000000) && (12000000<max))
	      gtk_adjustment_set_value(camera_params->objects->Bytes_per_sec_adj,12000000);
	    else
	      gtk_adjustment_set_value(camera_params->objects->Bytes_per_sec_adj,min);
	    /********************* Camera INIT *******************/
	    //Now we adjust the packet size
	    if(!PvCaptureAdjustPacketSize(camera_params->camera_handler,9000))
	      {
		unsigned long Size;
		PvAttrUint32Get(camera_params->camera_handler,"PacketSize",&Size);   
		msg = g_strdup_printf ("\nPacket size: %lu",Size);
		gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
		g_free (msg);

	      }
	    //We adjudst the bandwith control mode
	    if(PvAttrStringSet(camera_params->camera_handler,"BandwidthCtrlMode","StreamBytesPerSecond"))
	      g_print("Error while setting the Bandwidth control mode\n");
	    //We set the gain to manual
	    if(PvAttrStringSet(camera_params->camera_handler,"GainMode","Manual"))
	      g_print("Error while setting the gain mode\n");
	    //Exposure mode manual
	    if(PvAttrStringSet(camera_params->camera_handler,"ExposureMode","Manual"))
	      g_print("Error while setting the exposure mode\n");
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
    //*******************end of camera init *************************

    //If we are getting image and we stopped, we stop the camera
    if((camera_params->grabbing_images==1) && (camera_params->grab_images==0))
    {
       camera_stop_grabbing(camera_params);
    }

    //If we are NOT getting images and we want to, we init the grabbing
    else if((camera_params->grabbing_images==0) && (camera_params->grab_images==1))
    {
       camera_start_grabbing(camera_params);
    }

    else if((camera_params->grabbing_images==1) && (camera_params->grab_images==1))
    {
      ret=PvCaptureWaitForFrameDone(camera_params->camera_handler, &camera_params->camera_frame, FRAME_WAIT_TIMEOUT);
      if((ret==ePvErrSuccess)&&(camera_params->camera_frame.Status==ePvErrSuccess))
	camera_new_image(camera_params);
      if((ret==ePvErrSuccess)&&(camera_params->camera_frame.Status!=ePvErrSuccess))
      {
	//Error on the frame, we restart the Grabbing
	g_print("Frame error");
	camera_stop_grabbing(camera_params);
	camera_start_grabbing(camera_params);
      }
    }
    else
      usleep(100000); //some waiting 

  }


  if(camera_params->grabbing_images==1)
       camera_stop_grabbing(camera_params);

  if(camera_params->camera_connected)
    PvCameraClose(camera_params->camera_handler);
  // uninit the API
  PvUnInitialize();
  gettimeofday (&tv, (struct timezone *) NULL);

  g_print("Camera thread stopped after %ld seconds\n", tv.tv_sec-start_time);

  return NULL;
}
