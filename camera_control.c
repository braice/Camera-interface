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

void camera_reset_roi(camera_parameters_t* camera_params);
void camera_new_image(camera_parameters_t* camera_params);

long start_time;


char *PvAPIerror_to_str(tPvErr error)
{
  switch(error)
  {
  case ePvErrSuccess:
    return "No Error";
  case ePvErrCameraFault:
    return "Unexpected camera fault";
  case ePvErrInternalFault:
    return "Unexpected fault in PvApi or driver";
  case ePvErrBadHandle:
    return "Camera handle is invalid";
  case ePvErrBadParameter:
    return "Bad parameter to API call";
  case ePvErrBadSequence:
    return "Sequence of API calls is incorrect";
  case ePvErrNotFound:
    return "Camera or attribute not found";
  case ePvErrAccessDenied:
    return "Camera cannot be opened in the specified mode";
  case ePvErrUnplugged:
    return "Camera was unplugged";
  case ePvErrInvalidSetup:
    return "Setup is invalid (an attribute is invalid)";
  case ePvErrResources:
    return "System/network resources or memory not available";
  case ePvErrBandwidth:
    return "1394 bandwidth not available";
  case ePvErrQueueFull:
    return "Too many frames on queue";
  case ePvErrBufferTooSmall:
    return "Frame buffer is too small";
  case ePvErrCancelled:
    return "Frame cancelled by user";
  case ePvErrDataLost:
    return "The data for the frame was lost";
  case ePvErrDataMissing:
    return "Some data in the frame is missing";
  case ePvErrTimeout:
    return "Timeout during wait";
  case ePvErrOutOfRange:
    return "Attribute value is out of the expected range";
  case ePvErrWrongType:
    return "Attribute is not this type (wrong access function)";
  case ePvErrForbidden:
    return "Attribute write forbidden at this time";
  case ePvErrUnavailable:
    return "Attribute is not available at this time";
  case ePvErrFirewall:
    return "A firewall is blocking the traffic (Windows only)";
  case __ePvErr_force_32: //To make the compiler happy
    return "";
  }
  return "Unknown error";
}


int camera_init(camera_parameters_t* camera_params, long int UniqueId , char *DisplayName)
{
  int ret;
  gchar *msg; 

  ret=PvCameraOpen(UniqueId,ePvAccessMaster,&camera_params->camera_handler);
  if(ret!=ePvErrSuccess)
  {
    g_warning("Error while opening the camera : %s",PvAPIerror_to_str(ret));
    add_to_statusbar(camera_params, 1, "Camera error : %s",PvAPIerror_to_str(ret));
    usleep(1000000); //some waiting 
    return 1;
  }
  /****************** Camera information getting ************/
  unsigned long gainmin, gainmax,min,max;
  //ret=PvAttrStringGet(camera_params->camera_handler,"DeviceModelName",ModelName,100,NULL);
  PvAttrUint32Get(camera_params->camera_handler,"SensorBits",&camera_params->sensorbits);
  gtk_adjustment_set_upper(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_value(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  PvAttrUint32Get(camera_params->camera_handler,"SensorWidth",&camera_params->sensorwidth);
  PvAttrUint32Get(camera_params->camera_handler,"SensorHeight",&camera_params->sensorheight);
  PvAttrRangeUint32(camera_params->camera_handler,"GainValue",&gainmin,&gainmax);
  //We write the info in the text buffer
  gdk_threads_enter();
  msg = g_strdup_printf ("Camera : %s\nSensor %ldx%ld %ld bits\nGain %lddB to %lddB", DisplayName,camera_params->sensorwidth,camera_params->sensorheight,camera_params->sensorbits,gainmin,gainmax);
  gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
  g_free (msg);
  gdk_threads_leave();
  //we set the min/max for the adjustments
  gtk_adjustment_set_lower(camera_params->objects->Exp_adj_gain,gainmin);
  gtk_adjustment_set_upper(camera_params->objects->Exp_adj_gain,gainmax);
  gtk_adjustment_set_value(camera_params->objects->Exp_adj_gain,gainmin);
  PvAttrRangeUint32(camera_params->camera_handler,"ExposureValue",&min,&max);
  gtk_adjustment_set_lower(camera_params->objects->Exp_adj_time,min/1000+1);
  gtk_adjustment_set_upper(camera_params->objects->Exp_adj_time,max/1000);
  gtk_adjustment_set_value(camera_params->objects->Exp_adj_time,min/1000+1);
  camera_reset_roi(camera_params);
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
  PvAttrRangeUint32(camera_params->camera_handler,"AcquisitionFrameCount",&min,&max);
  gtk_adjustment_set_lower(camera_params->objects->Trig_nbframes_adj,min);
  gtk_adjustment_set_upper(camera_params->objects->Trig_nbframes_adj,max);
  gtk_adjustment_set_value(camera_params->objects->Trig_nbframes_adj,min);
  tPvFloat32 fmin,fmax;
  PvAttrRangeFloat32(camera_params->camera_handler,"FrameRate",&fmin,&fmax);
  gtk_adjustment_set_lower(camera_params->objects->Trig_framerate_adj,fmin);
  gtk_adjustment_set_upper(camera_params->objects->Trig_framerate_adj,fmax);
  gtk_adjustment_set_value(camera_params->objects->Trig_framerate_adj,fmin);
  
  /********************* Camera INIT *******************/
  //Now we adjust the packet size
  if(!(ret=PvCaptureAdjustPacketSize(camera_params->camera_handler,9000)))
  {
    unsigned long Size;
    PvAttrUint32Get(camera_params->camera_handler,"PacketSize",&Size);
    PvAttrUint32Set(camera_params->camera_handler,"PacketSize",Size);
    PvAttrUint32Get(camera_params->camera_handler,"PacketSize",&Size);
    gdk_threads_enter();
    msg = g_strdup_printf ("\nPacket size: %lu",Size);
    gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
    g_free (msg);
    gdk_threads_leave();
  }
  else 
    g_print("Error while setting the packet size: %s\n",PvAPIerror_to_str(ret));
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

#if 0
  char truc[30];
  int i;
  for( i=0;i<30;i++)
    truc[i]='\0';
  PvAttrEnumGet(camera_params->camera_handler, "AcqStartTriggerMode",truc, 30, NULL);
  g_print("AcqStartTriggerMode %s\n",truc);
  PvAttrEnumGet(camera_params->camera_handler, "AcqEndTriggerMode", truc, 30, NULL);
  g_print("AcqEndTriggerMode %s\n",truc);
#endif


  return 0;
}



void camera_update_roi(camera_parameters_t* camera_params)
{
  if(camera_params->roi_hard_active)
  {
    PvAttrUint32Set(camera_params->camera_handler,"Height",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_height));
    PvAttrUint32Set(camera_params->camera_handler,"RegionX",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_x));
    PvAttrUint32Set(camera_params->camera_handler,"RegionY",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_y));
    PvAttrUint32Set(camera_params->camera_handler,"Width",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_width));
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
  int ret;
  g_print("camera_stop_grabbing\n");
  ret=PvCommandRun(camera_params->camera_handler,"AcquisitionStop");
  if(ret)
    add_to_statusbar(camera_params, 0, "Error while stopping the acquisition %s",PvAPIerror_to_str(ret));
  PvCaptureEnd(camera_params->camera_handler);
  PvCaptureQueueClear(camera_params->camera_handler);
  //We free the image buffer
  free(camera_params->camera_frame.ImageBuffer);
  camera_params->grabbing_images=0;
  if(!ret)
    add_to_statusbar(camera_params, 0, "Acquisition Stopped");
}

void camera_set_triggering(camera_parameters_t* camera_params)
{
  int ret=0;
  //We set the trigger soure
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->ext1_trig))==TRUE)
  {
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerMode","SyncIn1");
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerEvent","EdgeRising");
  }
  else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->ext2_trig))==TRUE)
  {
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerMode","SyncIn2");
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerEvent","EdgeRising");
  }
  else   if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->freerun_trig))==TRUE)
  {
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerMode","Freerun");
  }
  else   if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->framerate_trig))==TRUE)
  {
    PvAttrEnumSet(camera_params->camera_handler,"FrameStartTriggerMode","FixedRate");
    ret=PvAttrFloat32Set(camera_params->camera_handler,"FrameRate",gtk_adjustment_get_value(camera_params->objects->Trig_framerate_adj));
    if(ret)
      g_print("Error %s\n",PvAPIerror_to_str(ret));
  }
  else
  {
    g_warning("bug in the trigger back to freerun\n");
    PvAttrEnumSet(camera_params->camera_handler, "FrameStartTriggerMode","Freerun");
  }



  //We set the framecount/acquisition mode
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->trig_cont))==TRUE)
    PvAttrEnumSet(camera_params->camera_handler, "AcquisitionMode", "Continuous");
  else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->trig_single))==TRUE)
    PvAttrEnumSet(camera_params->camera_handler, "AcquisitionMode", "SingleFrame");
  else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->trig_mult))==TRUE)
  {
    PvAttrEnumSet(camera_params->camera_handler, "AcquisitionMode", "MultiFrame");
    PvAttrUint32Set(camera_params->camera_handler,"AcquisitionFrameCount",(int)gtk_adjustment_get_value(camera_params->objects->Trig_nbframes_adj));
  }
  else
  {
    g_warning("bug in the trigger back to continuous\n");
    PvAttrEnumSet(camera_params->camera_handler, "AcquisitionMode", "Continuous");
  }

}

void camera_set_exposure(camera_parameters_t* camera_params)
{
  //Set the exposure time
  PvAttrUint32Set(camera_params->camera_handler,"ExposureValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_time)*1000);
  //Set the gain
  PvAttrUint32Set(camera_params->camera_handler,"GainValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_gain));

}

void camera_reset_roi(camera_parameters_t* camera_params)
{
  unsigned long min,max;
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
}


void FrameDoneCB(tPvFrame* pFrame)
{
  camera_parameters_t* camera_params;
  camera_params=(camera_parameters_t *)pFrame->Context[0];
  if(pFrame->Status == ePvErrSuccess)
    camera_new_image(camera_params);
  else
    {
      if(camera_params->camera_frame.Status!=ePvErrCancelled)
      {
	//Error on the frame, we queue another
	add_to_statusbar(camera_params, 1, "Frame error : %s after frame number %ld",PvAPIerror_to_str(camera_params->camera_frame.Status),camera_params->image_number);
	PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,FrameDoneCB);
      }
    }
}


void camera_start_grabbing(camera_parameters_t* camera_params)
{
  g_print("camera_start_grabbing\n");
  camera_set_exposure(camera_params);
  camera_set_triggering(camera_params);
  //set the ROI
  camera_update_roi(camera_params);
  
  PvCaptureStart(camera_params->camera_handler);

  //We initialize the frame buffer
  int imageSize;
  //2 bits per pixel
  imageSize=camera_params->sensorwidth*camera_params->sensorheight*2;
  camera_params->camera_frame.ImageBufferSize=imageSize;
  camera_params->camera_frame.ImageBuffer=calloc(imageSize,sizeof(char)*2);
  camera_params->camera_frame.AncillaryBufferSize=0;
  camera_params->camera_frame.Context[0]=camera_params;
  PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,FrameDoneCB);

  PvCommandRun(camera_params->camera_handler, "AcquisitionStart");
  camera_params->grabbing_images=1;
  add_to_statusbar(camera_params, 1, "Acquisition Started");

}

void camera_new_image(camera_parameters_t* camera_params)
{
  gchar *msg;
  int width,height;
  width=camera_params->camera_frame.Width;
  height=camera_params->camera_frame.Height;

  gdk_threads_enter();
  camera_params->image_number++;



  //note : in case of redim, unref the pixbuf, create a new one and associate it with the image
  //todo check the size of the pixbuf versus the size of the camera buffer
  //If we didn't set a pixbuf yet, we do it
  if(camera_params->raw_image_pixbuff==NULL)
  {
    g_print("New pixbuff\n");
    camera_params->raw_image_pixbuff=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    //We associate this new pixbuf with the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(camera_params->objects->raw_image),camera_params->raw_image_pixbuff);
    g_object_ref(camera_params->raw_image_pixbuff);
  }
  else if((width!=gdk_pixbuf_get_width(camera_params->raw_image_pixbuff))||
	  (height!=gdk_pixbuf_get_height(camera_params->raw_image_pixbuff)))
  {
    g_print("pixbuff Resize\n");
    //The size of the frame is different from the size of the pixbuff
    //we dereference the pixbuff so the garbage colletor can work
    g_object_unref(camera_params->raw_image_pixbuff);
    //clear the image
    gtk_image_clear(GTK_IMAGE(camera_params->objects->raw_image));
    //create the new pixbuff
    camera_params->raw_image_pixbuff=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    //We associate this new pixbuf with the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(camera_params->objects->raw_image),camera_params->raw_image_pixbuff);
    g_object_ref(camera_params->raw_image_pixbuff);
  }
  /********************** copy the pixels *************************/
  guchar *pixels;
  pixels=gdk_pixbuf_get_pixels(camera_params->raw_image_pixbuff);

  //convert black and white into RGB
  char pixel;
  int row, col, pos, rowstride;
  long double mean;
  mean=0;
  rowstride=gdk_pixbuf_get_rowstride(camera_params->raw_image_pixbuff);
  //Todo binning tests
  for(row=0;row<height;row++)
  {
    for(col=0;col<width;col++)
    {
      pos=row*width+col;
      //RED
      mean+=((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)+1]<<8)+((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)]);
      pixel=(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)+1])<<4)&0xF0) +(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)])>>4)&0x0F);
      pixels[0+3*(col)+row*rowstride]=pixel;
      //GREEN
      pixels[1+3*(col)+row*rowstride]=pixel;
      //BLUE
      pixels[2+3*(col)+row*rowstride]=pixel;
    }
  }

  //We store the corner of the ROI
  camera_params->roi_hard_X_lastimage=camera_params->camera_frame.RegionX;
  camera_params->roi_hard_Y_lastimage=camera_params->camera_frame.RegionY;
  
  /******************* Filling the text buffer and chart ********************/
  mean/=(height*width);
  msg = g_strdup_printf ("Size: %dx%d \tMean %.3Lf\tNumber: %ld", width, height, mean, camera_params->image_number);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->image_number)),msg,-1);
  g_free (msg);
  //We add the new value to the array
  struct timeval tv;
  gdouble time, time_usec;
  gettimeofday (&tv, (struct timezone *) NULL);
  time_usec=tv.tv_usec;
  time_usec/=1000000;
  time = tv.tv_sec-start_time+time_usec;
  
  GtkTreeIter iter;
  gtk_list_store_append (camera_params->objects->statistics_list, &iter);
  gtk_list_store_set (camera_params->objects->statistics_list, &iter,
		      0, (gint)camera_params->image_number,
		      1, time,
		      2, (gdouble)mean,
		      -1);

  //if we asked for autoscrolling the chart, we do it
  if(camera_params->autoscroll_chart)
  {
    /* With NULL as iter, we get the number of toplevel nodes. */
    gint rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(camera_params->objects->statistics_list), NULL);
    GtkTreePath *path;
    /* Now get a path from the index. */
    path = gtk_tree_path_new_from_indices(rows - 1, -1);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW (camera_params->objects->stats_treeview), path, NULL, TRUE, 0.0, 0.0); 
    /* Drop the path, we're done with it. */
    gtk_tree_path_free(path);
  }
  /********************** mean bar ***************************/
  gdouble fraction;
  fraction=(mean-gtk_adjustment_get_value(camera_params->objects->min_meanbar))/(gtk_adjustment_get_value(camera_params->objects->max_meanbar)-gtk_adjustment_get_value(camera_params->objects->min_meanbar));
  fraction=fraction<0?0:fraction;
  fraction=fraction>1?1:fraction;
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(camera_params->objects->mean_bar),fraction);

  /********************* ImageMagick *************************/

  //http://www.imagemagick.org/api/constitute.php
  //http://www.imagemagick.org/api/magick-wand.php
  //http://www.imagemagick.org/api/magick-wand.php#NewMagickWandFromImage
  //http://blog.gmane.org/gmane.comp.video.image-magick.devel/month=20060301/page=1
  //http://studio.imagemagick.org/pipermail/magick-developers/2002-October/001077.html
  //http://studio.imagemagick.org/pipermail/magick-developers/2002-October/001083.html

  //camera_params->wand_data.image=ConstituteImage(width,height,"I",ShortPixel,camera_params->camera_frame.ImageBuffer,camera_params->wand_data.exception);
  





  /******************* End of ImageMagick *******************/


  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->raw_image);
  gdk_threads_leave();
  //ready for the next one
  PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,FrameDoneCB);


}


void *camera_thread_func(void* arg)
{
  camera_parameters_t *camera_params;
  camera_params= (camera_parameters_t *) arg;
  struct timeval tv;

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
    add_to_statusbar(camera_params, 1, "Failed to initialise the Camera API. Error %s", PvAPIerror_to_str(ret));
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


	add_to_statusbar(camera_params, 1, "We detected %ld Reachable camera%c (%ld total)",cameraRle,cameraRle>1 ? 's':' ',cameraNum);
	usleep(300000); //just to see the message
	if(cameraNum)
	{
	  struct in_addr addr;
          tPvIpSettings Conf;
          tPvErr lErr;
            
	  if((lErr = PvCameraIpSettingsGet(cameraList[0].UniqueId,&Conf)) == ePvErrSuccess)
          {
            addr.s_addr = Conf.CurrentIpAddress;
	    add_to_statusbar(camera_params, 1, "Camera: %s. Serial %s. Unique ID = % 8ld IP@ = %15s [%s]", cameraList[0].DisplayName,cameraList[0].SerialString, cameraList[0].UniqueId, inet_ntoa(addr), cameraList[0].PermittedAccess & ePvAccessMaster ? "available" : "in use");

	    //We init the camera
	    if(camera_init(camera_params,cameraList[0].UniqueId,cameraList[0].DisplayName))
	      continue;
          }
	}
	else
	  usleep(500000); //some waiting 
      }
      usleep(500000); //some waiting 
    } //While camera unconnected
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
    //We are getting images //only waiting because we are using the callback
    else if((camera_params->grabbing_images==1) && (camera_params->grab_images==1))
    {
      usleep(100000); //some waiting 
    }
    else //we are not getting image and we don't want to, we wait for the user to press the button
      usleep(100000); //some waiting 

  }
  g_print("Camera thread stopping\n");

  /************ At this point the thread has been asked to shut down **********************/
  if(camera_params->grabbing_images==1)
       camera_stop_grabbing(camera_params);
  g_print("Camera thread stopping 2\n");

  if(camera_params->camera_connected)
    PvCameraClose(camera_params->camera_handler);
  // uninit the API
  g_print("Camera thread stopping 3\n");
  PvUnInitialize();
  gettimeofday (&tv, (struct timezone *) NULL);

  g_print("Camera thread stopped after %ld seconds\n", tv.tv_sec-start_time);

  return NULL;
}
