/*
 * Program to control the GigE camera. Camera communication part
 *
 * Copyright (C) 2010 Brice Dubost & Benjamin Szymanski
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

#ifdef FAKE_ANDORLIB
#include "fake_andorlib.h"
#else
#include "atmcdLXd.h"
#endif

#define MAX_CAMERA_LIST 20

void camera_reset_roi(camera_parameters_t* camera_params);
void camera_new_image(camera_parameters_t* camera_params);



char *AndorErrorToStr(int error)
{
  switch(error)
  {
  case DRV_SUCCESS:
    return "No Error";
  case DRV_GENERAL_ERRORS:
    return "DRV_GENERAL_ERRORS";
  case DRV_VXDNOTINSTALLED:
    return "DRV_VXDNOTINSTALLED";
  case DRV_INIERROR:
    return "DRV_INIERROR : Unable to loard DETECTOR.INI";
  case DRV_COFERROR:
    return "DRV_COFERROR : Unable to load *.COF";
  case DRV_FLEXERROR:
    return "DRV_FLEXERROR : Unableto load *.RBF";
  case DRV_ERROR_ACK:
    return "DRV_ERROR_ACK : Unable to communicate with card";
  case DRV_ERROR_FILELOAD:
    return "DRV_ERROR_FILELOAD : file *.COF or *.RBF";
  case DRV_ERROR_PAGELOCK:
    return "DRV_ERROR_PAGELOCK : Unable to acquire lock on requested memory";
  case DRV_USBERROR:
    return "DRV_USBERROR";
  case DRV_ERROR_NOCAMERA:
    return "DRV_ERROR_NOCAMERA";
  case DRV_NOT_AVAILABLE:
    return "DRV_NOT_AVAILABLE";
  case DRV_NOT_INITIALIZED:
    return "DRV_NOT_INITIALIZED";
  case DRV_ACQUIRING:
    return "DRV_ACQUIRING";
  case DRV_INVALID_FILTER:
    return "DRV_INVALID_FILTER";
  case DRV_BINNING_ERROR:
    return "DRV_BINNING_ERROR";
  case DRV_P1INVALID:
    return "DRV_P1INVALID";
  case DRV_P2INVALID:
    return "DRV_P2INVALID";
  case DRV_P3INVALID:
    return "DRV_P3INVALID";
  case DRV_P4INVALID:
    return "DRV_P4INVALID";
  }
  return "Unknown error";
}

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

int camera_Andor_init(camera_parameters_t* camera_params, long andor_num)
{
  //We hide useless buttons
  gtk_widget_hide((camera_params->objects->ext2_trig));
  gtk_widget_hide((camera_params->objects->framerate_trig));
  gtk_widget_hide((camera_params->objects->trig_framerate));
  gtk_widget_hide((camera_params->objects->net_expander));

  g_print("Camera Andor init\n");
  int ret;  	      
  GetCameraHandle(andor_num,&camera_params->andor_handler);
  SetCurrentCamera(camera_params->andor_handler);
  add_to_statusbar(camera_params, 1, "Initialzing Andor Camera...");
  ret = Initialize("/usr/local/etc/andor");
  if(ret!=DRV_SUCCESS)
    {
      add_to_statusbar(camera_params, 1, "An error occured while initializing the Andor Camera. Error : %d %s",
		       ret,AndorErrorToStr(ret));
      g_warning("An error occured while initializing the Andor Camera. Error : %d %s",
		ret,AndorErrorToStr(ret));
      sleep(1);
      return 1;
    }
  else
    {
      add_to_statusbar(camera_params, 1, "Camera Andor almost initialzed.");
      g_print("Camera Andor almost initialzed.");
      sleep(2);
      add_to_statusbar(camera_params, 1, "Camera Andor initialzed.");
      g_print("Camera Andor initialzed.");
    }
  
  gchar *msg; 
  /****************** Camera information getting ************/
  AndorCapabilities caps;
  caps.ulSize = sizeof(AndorCapabilities);
  GetCapabilities(&caps);
  g_print("Capabilities\nulSetFunctions 0x%lx AC_SETFUNCTION_PREAMPGAIN 0x%x\nulGetFunctions 0x%lx\nulEMGainCapability 0x%lx AC_EMGAIN_12BIT 0x%x\n", caps.ulSetFunctions, AC_SETFUNCTION_PREAMPGAIN, caps.ulGetFunctions, caps.ulEMGainCapability, AC_EMGAIN_12BIT);
  g_print("Capabilities ulSetFunctions & AC_SETFUNCTION_PREAMPGAIN 0x%lx\n", caps.ulSetFunctions & AC_SETFUNCTION_PREAMPGAIN);
  unsigned long min;
  int gainmin, gainmax;
  gdk_threads_enter();
  //We set theses ROI to a minimal value
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_width1,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_width2,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_height1,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_height2,1);
  //We fix the things which depends on the sensor depth
  if(caps.ulPixelMode & AC_PIXELMODE_32BIT)
    camera_params->sensorbits = 32;
  else if(caps.ulPixelMode & AC_PIXELMODE_16BIT)
    camera_params->sensorbits = 16;
  else if(caps.ulPixelMode & AC_PIXELMODE_14BIT)
    camera_params->sensorbits = 14;
  else if(caps.ulPixelMode & AC_PIXELMODE_8BIT)
    camera_params->sensorbits = 8;
  gtk_adjustment_set_upper(camera_params->objects->min_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_value(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->soft_level_min_adj,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->soft_level_max_adj,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_value(camera_params->objects->soft_level_max_adj,1<<((int)camera_params->sensorbits));
  int width, height;
  GetDetector(&width,&height);
  camera_params->sensorwidth = width;
  camera_params->sensorheight = height;
  GetEMGainRange(&gainmin,&gainmax);
  gainmin=0;
  //We write the info in the text buffer
  msg = g_strdup_printf ("Camera : Andor\nSensor %ldx%ld %ld bits\n EM Gain %d to %d", camera_params->sensorwidth,camera_params->sensorheight,camera_params->sensorbits,gainmin,gainmax);
  gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
  g_free (msg);

  //we set the min/max for the adjustments
  gtk_adjustment_set_lower(camera_params->objects->Exp_adj_gain,gainmin);
  gtk_adjustment_set_upper(camera_params->objects->Exp_adj_gain,gainmax);
  gtk_adjustment_set_value(camera_params->objects->Exp_adj_gain,gainmin);
  min = 10; //we can't ask the camera min and max exposure time, we fix them.
  float f_max;
  GetMaximumExposure(&f_max);
  gtk_adjustment_set_lower(camera_params->objects->Exp_adj_time,min);
  gtk_adjustment_set_upper(camera_params->objects->Exp_adj_time,f_max*1000);
  gtk_adjustment_set_value(camera_params->objects->Exp_adj_time,100);
  camera_reset_roi(camera_params);
  min = 1;
  int i_max;
  GetMaximumBinning(4,0,&i_max);
  gtk_adjustment_set_lower(camera_params->objects->Bin_X_adj,min);
  gtk_adjustment_set_upper(camera_params->objects->Bin_X_adj,i_max);
  gtk_adjustment_set_value(camera_params->objects->Bin_X_adj,min);
  GetMaximumBinning(4,1,&i_max);  
  gtk_adjustment_set_lower(camera_params->objects->Bin_Y_adj,min);
  gtk_adjustment_set_upper(camera_params->objects->Bin_Y_adj,i_max);
  gtk_adjustment_set_value(camera_params->objects->Bin_Y_adj,min);
  gtk_adjustment_set_lower(camera_params->objects->Bytes_per_sec_adj,1);
  gtk_adjustment_set_upper(camera_params->objects->Bytes_per_sec_adj,1);
  gtk_adjustment_set_lower(camera_params->objects->Trig_nbframes_adj,1);
  gtk_adjustment_set_upper(camera_params->objects->Trig_nbframes_adj,1);
  gtk_adjustment_set_value(camera_params->objects->Trig_nbframes_adj,1);
  gtk_adjustment_set_lower(camera_params->objects->Trig_framerate_adj,1.);
  gtk_adjustment_set_upper(camera_params->objects->Trig_framerate_adj,1.);
  gtk_adjustment_set_value(camera_params->objects->Trig_framerate_adj,1.);
  gdk_threads_leave();
  
  /********************* Camera INIT *******************/
  //We set the read mode to Image
  SetReadMode(4);
  //We set the acquisition mode to Single Scan
  SetAcquisitionMode(1);
  // Dummy shutter
  SetShutter(1,0,50,50);
  // Set initial exposure time
  SetExposureTime(0.1);
  // We would like a full image
  SetImage(1,1,1,camera_params->sensorwidth,1,camera_params->sensorheight);
  //We have a good camera handler, we set the camera as being connected
  camera_params->camera_connected=1;
  /********************* End of Camera INIT *******************/

  g_print("End of Camera inint\n");

  return 0;
}

int camera_init(camera_parameters_t* camera_params, long int UniqueId , char *DisplayName)
{
  int ret;
  gchar *msg; 
  g_print("Camera init\n");
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
  gdk_threads_enter();
  //We set theses ROI to a minimal value
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_width1,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_width2,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_height1,1);
  gtk_adjustment_set_value(camera_params->objects->ROI_soft_mean_adjust_height2,1);
  //We fix the things which depends on the sensor depth
  PvAttrUint32Get(camera_params->camera_handler,"SensorBits",&camera_params->sensorbits);
  gtk_adjustment_set_upper(camera_params->objects->min_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_value(camera_params->objects->max_meanbar,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->soft_level_min_adj,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_upper(camera_params->objects->soft_level_max_adj,1<<((int)camera_params->sensorbits));
  gtk_adjustment_set_value(camera_params->objects->soft_level_max_adj,1<<((int)camera_params->sensorbits));
  PvAttrUint32Get(camera_params->camera_handler,"SensorWidth",&camera_params->sensorwidth);
  PvAttrUint32Get(camera_params->camera_handler,"SensorHeight",&camera_params->sensorheight);
  PvAttrRangeUint32(camera_params->camera_handler,"GainValue",&gainmin,&gainmax);
  //We write the info in the text buffer
  msg = g_strdup_printf ("Camera : %s\nSensor %ldx%ld %ld bits\nGain %lddB to %lddB", DisplayName,camera_params->sensorwidth,camera_params->sensorheight,camera_params->sensorbits,gainmin,gainmax);
  gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->camera_text)),msg,-1);
  g_free (msg);

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
  gdk_threads_leave();
  
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

  g_print("End of Camera inint\n");

  return 0;
}



void camera_update_binning(camera_parameters_t* camera_params)
{
  camera_params->binning_x=(int)gtk_adjustment_get_value(camera_params->objects->Bin_X_adj);
  camera_params->binning_y=(int)gtk_adjustment_get_value(camera_params->objects->Bin_Y_adj);
  //We don't update when it's 0
  if(!camera_params->binning_x || !camera_params->binning_y)
    return;
  if(camera_params->type == CAMERA_GIGE)
    {
      PvAttrUint32Set(camera_params->camera_handler,"BinningX",camera_params->binning_x);
      PvAttrUint32Set(camera_params->camera_handler,"BinningY",camera_params->binning_y);
    }

}

void camera_update_roi(camera_parameters_t* camera_params)
{
  if(camera_params->roi_hard_active)
  {
    if(camera_params->type == CAMERA_GIGE)
      {
	PvAttrUint32Set(camera_params->camera_handler,"Height",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_height));
	PvAttrUint32Set(camera_params->camera_handler,"RegionX",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_x));
	PvAttrUint32Set(camera_params->camera_handler,"RegionY",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_y));
	PvAttrUint32Set(camera_params->camera_handler,"Width",(int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_width));
      }
    else if(camera_params->type == CAMERA_ANDOR)
      {
	add_to_statusbar(camera_params, 0, "No Hardware ROI on Andor camera (SDK bug)");
      }
    camera_params->roi_hard_width = (int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_width);
    camera_params->roi_hard_height = (int)gtk_adjustment_get_value(camera_params->objects->ROI_adjust_height);
      
  }
  else
    {
      if(camera_params->type == CAMERA_GIGE)
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
      else if(camera_params->type == CAMERA_ANDOR)
	{
	  //Nothing to do since we don't set the ROI with this camera
	}
    }
}

void camera_stop_grabbing(camera_parameters_t* camera_params)
{
  int ret=0;
  g_print("camera_stop_grabbing\n");
  if(camera_params->type == CAMERA_GIGE)
    {
      ret=PvCommandRun(camera_params->camera_handler,"AcquisitionStop");
      if(ret)
	{
	  g_print("Error while stopping the acquisition %s\n",PvAPIerror_to_str(ret));
	  add_to_statusbar(camera_params, 0, "Error while stopping the acquisition %s",PvAPIerror_to_str(ret));
	}
      PvCaptureEnd(camera_params->camera_handler);
      PvCaptureQueueClear(camera_params->camera_handler);
    }
  else if(camera_params->type == CAMERA_ANDOR)
    {
      AbortAcquisition();
    }
  //We free the image buffer
  free(camera_params->camera_frame.ImageBuffer);
  camera_params->grabbing_images=0;
  
  if(!ret)
    {
      g_print("Acquisition Stopped\n");
      add_to_statusbar(camera_params, 0, "Acquisition Stopped");
    }
  camera_params->image_acq_number=0;
}

void camera_set_triggering(camera_parameters_t* camera_params)
{
  int ret=0;
  if(camera_params->type == CAMERA_GIGE)
    {
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
  else if(camera_params->type == CAMERA_ANDOR)
    {
      int ret;
      if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->ext1_trig))==TRUE)
	ret = SetTriggerMode(1);
      else
	ret = SetTriggerMode(0);
      if(ret!=DRV_SUCCESS)
	{
	  g_warning("Andor set triggering error : %d %s", ret, AndorErrorToStr(ret));
	  add_to_statusbar(camera_params, 1, "Andor set triggering error : %d %s", ret, AndorErrorToStr(ret));
	}
    }
}

void camera_set_exposure(camera_parameters_t* camera_params)
{
  if(camera_params->type == CAMERA_GIGE)
    {
      //Set the exposure time
      PvAttrUint32Set(camera_params->camera_handler,"ExposureValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_time)*1000);
      //Set the gain
      PvAttrUint32Set(camera_params->camera_handler,"GainValue",(int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_gain));
    }
  else if(camera_params->type == CAMERA_ANDOR)
    {
      float time;
      int ret;
      int status;
      GetStatus(&status);
      if(status==DRV_ACQUIRING)
	{
	  return;
	}
      
      time = gtk_adjustment_get_value(camera_params->objects->Exp_adj_time)/1000.;
      ret = SetExposureTime(time);
      if(ret!=DRV_SUCCESS)
	{ 
	  g_warning( "Andor : set exposure time. Error : %d %s", ret, AndorErrorToStr(ret));
	}
      ret = SetEMCCDGain((int)gtk_adjustment_get_value(camera_params->objects->Exp_adj_gain));
      if(ret!=DRV_SUCCESS)
	{ 
	  g_warning( "Andor : set EMCCD Gain. Error : %d %s", ret, AndorErrorToStr(ret));
	}
    }
  
}

void camera_reset_roi(camera_parameters_t* camera_params)
{
  if(camera_params->type == CAMERA_GIGE)
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
  else if(camera_params->type == CAMERA_ANDOR)
    {
      int width, height;
      GetDetector(&width,&height);
      gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_height,1);
      gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_height,height);
      gtk_adjustment_set_value(camera_params->objects->ROI_adjust_height,height);
      gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_x,0);
      gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_x,height-1);
      gtk_adjustment_set_value(camera_params->objects->ROI_adjust_x,0);
      gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_y,0);
      gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_y,width-1);
      gtk_adjustment_set_value(camera_params->objects->ROI_adjust_y,0);
      gtk_adjustment_set_lower(camera_params->objects->ROI_adjust_width,1);
      gtk_adjustment_set_upper(camera_params->objects->ROI_adjust_width,width);
      gtk_adjustment_set_value(camera_params->objects->ROI_adjust_width,width);
    }
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


void camera_update_andor_binning(camera_parameters_t* camera_params)
{
  int ret;
  camera_params->binning_x=(int)gtk_adjustment_get_value(camera_params->objects->Bin_X_adj);
  camera_params->binning_y=(int)gtk_adjustment_get_value(camera_params->objects->Bin_Y_adj);
  //We don't update when it's 0
  if(camera_params->binning_x && camera_params->binning_y)
    {
      ret=SetImage(camera_params->binning_x,camera_params->binning_y,1,camera_params->sensorwidth,1,camera_params->sensorheight);
      if(ret!=DRV_SUCCESS)
	{ 
	  add_to_statusbar(camera_params,1, "The camera does not support the following binning : X %d Y %d. Back to no binning. Error %d %s",
			   camera_params->binning_x,camera_params->binning_y,
			   ret, AndorErrorToStr(ret));
	  ret=SetImage(1,1,1,(int)camera_params->sensorwidth,1,(int)camera_params->sensorheight);
	}
      else
	{
	  camera_params->camera_frame.Width = camera_params->sensorwidth/camera_params->binning_x;
	  camera_params->camera_frame.Height = camera_params->sensorheight/camera_params->binning_y;
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
  
  if(camera_params->type == CAMERA_GIGE)
    {
      PvCaptureStart(camera_params->camera_handler);
    }
  
  //We initialize the frame buffer
  int imageSize;
  //2 bits per pixel
  imageSize=camera_params->sensorwidth*camera_params->sensorheight*2;
  camera_params->camera_frame.ImageBufferSize=imageSize;
  camera_params->camera_frame.ImageBuffer=calloc(imageSize,sizeof(char)*2);
  camera_params->camera_frame.AncillaryBufferSize=0;
  camera_params->camera_frame.Context[0]=camera_params;
  
  if(camera_params->type == CAMERA_GIGE)
    {
      PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,FrameDoneCB);
      
      PvCommandRun(camera_params->camera_handler, "AcquisitionStart");
    }
  else if(camera_params->type == CAMERA_ANDOR)
    {
      int ret;
      camera_params->camera_frame.Width = camera_params->sensorwidth;
      camera_params->camera_frame.Height = camera_params->sensorheight;
      camera_update_andor_binning(camera_params);
      ret = StartAcquisition();
      if(ret != DRV_SUCCESS)
	{
	  add_to_statusbar(camera_params, 1, "An error occured while Starting the acquisition. Error : %d %s",
			   ret,AndorErrorToStr(ret));
	  return;
	}
    }
  camera_params->grabbing_images=1;
  add_to_statusbar(camera_params, 1, "Acquisition Started");

}

void camera_new_image(camera_parameters_t* camera_params)
{
  gchar *msg;
  int width,height;
  width=camera_params->camera_frame.Width;
  height=camera_params->camera_frame.Height;

  struct timeval tv;
  gettimeofday (&tv, (struct timezone *) NULL);
  camera_params->image_time = tv.tv_sec-camera_params->start_time+((double)tv.tv_usec)/1000000;

  gdk_threads_enter();
    
  camera_params->image_number++;
  camera_params->image_acq_number++;



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
  camera_params->raw_image_mean=0;
  rowstride=gdk_pixbuf_get_rowstride(camera_params->raw_image_pixbuff);
  //Todo binning tests
  int shift_up,shift_down;
  shift_up = 16 - ((int)camera_params->sensorbits);
  shift_down = 8 - shift_up;
  for(row=0;row<height;row++)
  {
    for(col=0;col<width;col++)
    {
      pos=row*width+col;
      camera_params->raw_image_mean+=((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)+1]<<8)+((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)]);
      //Levelling
      pixel=(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)+1])<<shift_up)) +(((((guchar *)camera_params->camera_frame.ImageBuffer)[2*(pos)])>>shift_down));
      //RED
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
  camera_params->raw_image_mean/=(height*width);
  msg = g_strdup_printf ("Size: %dx%d \tMean %.3Lf\tNumber: %ld\tNumber for current Acquisiton: %ld", width, height, camera_params->raw_image_mean, camera_params->image_number, camera_params->image_acq_number);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->image_number)),msg,-1);
  g_free (msg);
  //We add the new value to the array
  

  /********************** mean bar ***************************/
  gdouble fraction;
  fraction=(camera_params->raw_image_mean-gtk_adjustment_get_value(camera_params->objects->min_meanbar))/(gtk_adjustment_get_value(camera_params->objects->max_meanbar)-gtk_adjustment_get_value(camera_params->objects->min_meanbar));
  fraction=fraction<0?0:fraction;
  fraction=fraction>1?1:fraction;
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(camera_params->objects->mean_bar),fraction);
  gdk_threads_leave();


  /********************* ImageMagick *************************/
  imagemagick_get_image(camera_params);
  imagemagick_process_image(camera_params);
  gdk_threads_enter();

  imagemagick_display_image(camera_params);



  /******************* End of ImageMagick *******************/

  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->raw_image);
  gdk_threads_leave();

  //ready for the next one
  if(camera_params->type == CAMERA_GIGE)
    PvCaptureQueueFrame(camera_params->camera_handler,&camera_params->camera_frame,FrameDoneCB);
  else if(camera_params->type == CAMERA_ANDOR)
    {
      int ret;
      camera_params->camera_frame.Width = camera_params->sensorwidth;
      camera_params->camera_frame.Height = camera_params->sensorheight;
      camera_update_andor_binning(camera_params);
      ret = StartAcquisition();
      if(ret != DRV_SUCCESS)
	{
	  add_to_statusbar(camera_params, 1, "An error occured while Starting the acquisition. Error : %d %s",
			   ret,AndorErrorToStr(ret));
	  return;
	}
    }
}


void andor_poll(camera_parameters_t* camera_params)
{
  int ret;
  static int old_status = -1;
  static int status;
  GetStatus(&status);
  if(old_status == DRV_ACQUIRING && status!=DRV_ACQUIRING)
    {
      	add_to_statusbar(camera_params, 1, "Frame Done");
	ret = GetAcquiredData16(camera_params->camera_frame.ImageBuffer,
				  camera_params->camera_frame.Width*camera_params->camera_frame.Height);
	if(ret!=DRV_SUCCESS)
	  { 
	    add_to_statusbar(camera_params, 1, "An error occured while acquiring frame. Error : %d %s",
			     ret, AndorErrorToStr(ret));
	    g_warning("An error occured while acquiring frame. Error : %d %s", ret,
		      AndorErrorToStr(ret));
	  }
	else
	  {
	    //We update the exposure and triggerring parameters
	    camera_set_exposure(camera_params);
	    camera_set_triggering(camera_params);
	    camera_new_image(camera_params);
	  }
    }
  old_status = status;
}


void *camera_thread_func(void* arg)
{
  camera_parameters_t *camera_params;
  camera_params= (camera_parameters_t *) arg;
  struct timeval tv;

  tPvCameraInfoEx   cameraList[MAX_CAMERA_LIST];
  unsigned long   cameraNum = 0;
  long cameraAndorNum = 0;
  long cameraTotalNum = 0;
  unsigned long   cameraRle;

  int ret;
  //We record the starting time
  gettimeofday (&tv, (struct timezone *) NULL);
  camera_params->start_time = tv.tv_sec;
  
  g_print("Camera thread started\n");
  if((ret=PvInitialize()))
  {
    add_to_statusbar(camera_params, 1, "Failed to initialise the Camera API. Error %s", PvAPIerror_to_str(ret));
    return NULL;
  }
  sleep(3);
  while(!camera_params->camera_thread_shutdown) 
  {
    //We loop until the camera is connected
    while((!camera_params->camera_thread_shutdown)&&(!camera_params->camera_connected ))
    {
      //If the camera is not connected, we search for it
      if(!camera_params->camera_connected )
      {
	// first, get list of reachable cameras.
	cameraNum = PvCameraListEx(cameraList,MAX_CAMERA_LIST,NULL,(sizeof(tPvCameraInfoEx)));
	
	ret = GetAvailableCameras(&cameraAndorNum);
	if(ret!=DRV_SUCCESS)
	  {
	    cameraAndorNum = 0; 
	    add_to_statusbar(camera_params, 1, "An error occurs while obtening the number of available cameras. Error : %s",
			     AndorErrorToStr(ret));
	  }
	
	// keep how many cameras listed are reachable
	cameraRle = cameraNum;

	// then we append the list of unreachable cameras.
        if (cameraNum < MAX_CAMERA_LIST)
	  cameraNum += PvCameraListUnreachableEx(&cameraList[cameraNum], MAX_CAMERA_LIST-cameraNum, NULL,(sizeof(tPvCameraInfoEx)));


	add_to_statusbar(camera_params, 1, "We detected %ld GigE Reachable camera%c (%ld GigE total) and %ld Andor camera%c",
			 cameraRle,cameraRle>1 ? 's':' ',cameraNum, cameraAndorNum, cameraAndorNum>1 ? 's':' ' );
	usleep(300000); //just to see the message
	
	cameraTotalNum = cameraNum + cameraAndorNum;

	if(cameraTotalNum == 1 || (camera_params->choosen_camera > 0))
	{
	  struct in_addr addr;
          tPvIpSettings Conf;
          tPvErr lErr;
	  int camera;
	  if(cameraTotalNum==1)
	    camera=0;
	  else
	    camera = camera_params->choosen_camera-1;
	  
	  if(camera<cameraNum) // So the camera is a GigE
	    {
	      if((lErr = PvCameraIpSettingsGet(cameraList[camera].UniqueId,&Conf)) == ePvErrSuccess)
		{
		  addr.s_addr = Conf.CurrentIpAddress;
		  add_to_statusbar(camera_params, 1, "Camera: %s Model %s  Part %s. Serial %s. Unique ID = % 8ld IP@ = %15s [%s]",
				   cameraList[camera].CameraName,
				   cameraList[camera].ModelName,
				   cameraList[camera].PartNumber,
				   cameraList[camera].SerialNumber,
				   cameraList[camera].UniqueId,
				   inet_ntoa(addr), 
				   cameraList[camera].PermittedAccess & ePvAccessMaster ? "available" : "in use");
		  
		  //We init the camera
		  camera_params->type = CAMERA_GIGE;
		  if(camera_init(camera_params,cameraList[camera].UniqueId,cameraList[camera].CameraName))
		    continue;
		}
	    }
	  else // It's an Andor Camera
	    {
	      camera_params->type = CAMERA_ANDOR;
	      if(camera_Andor_init(camera_params, camera - cameraNum))
		continue;
	    }

	}
	else if((cameraNum > 1) && (camera_params->choosen_camera==0)) //There is several camera connected and the user didn't choosed any
	{
	  //We populate the list of the available cameras for the combo box
	  camera_params->choosen_camera=-1; //set it to -1 to remember that we opened the dialog
	  gdk_threads_enter();
	  gtk_list_store_clear ( camera_params->objects->camera_list );
	  int i_cam;
	  for(i_cam=0; i_cam < cameraNum; i_cam++)
	    {
	      gchar *msg;
	      msg = g_strdup_printf ("Number: %d   Name: %s   Model: %s   Part: %s",
			       i_cam+1,
			       cameraList[i_cam].CameraName,
			       cameraList[i_cam].ModelName,
			       cameraList[i_cam].PartNumber);
	      gtk_list_store_insert_with_values(camera_params->objects->camera_list, NULL,
						i_cam,
						0, (gchararray)(msg),
						-1);
	      g_free (msg);

	    }
	  for(i_cam=0; i_cam < cameraAndorNum; i_cam++)
	    {
	      gchar *msg;
	      //We have to do a selectCamera and getCapabilites to get more information.
	      msg = g_strdup_printf ("Number: %ld   Name: Andor Camera %d",
			       i_cam + 1 + cameraNum,
			       i_cam + 1);
	      gtk_list_store_insert_with_values(camera_params->objects->camera_list, NULL,
						i_cam+cameraNum,
						0, (gchararray)(msg),
						-1);
	      g_free (msg);

	    }
	  //We select a camera
	  gtk_combo_box_set_active (GTK_COMBO_BOX(camera_params->objects->camera_list_combo),0);
	  //Several cameras detected, we show the dialog
	  gtk_widget_show( camera_params->objects->camera_choose_win );
	  gdk_threads_leave();  
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
      if(camera_params->type == CAMERA_GIGE)
	usleep(100000); //some waiting
      else if(camera_params->type == CAMERA_ANDOR)
	{
	  andor_poll(camera_params);
	  usleep(10000);
	}
      if(camera_params->processed_image_needs_update)
	{
	  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_dont_update_current))==TRUE)
	    continue;
	  //If there is no image to be processed we return
	  if(camera_params->wand_data.raw_img_ok==0)
	    continue;
	  //If we are already processing an image we quit
	  if(camera_params->wand_data.processed_img_ok==0)
	    continue;
	  imagemagick_process_image(camera_params);
	  gdk_threads_enter();
	  imagemagick_display_image(camera_params);
	  gdk_threads_leave();
	  camera_params->processed_image_needs_update=0;
	}

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

  g_print("Camera thread stopped after %ld seconds\n", tv.tv_sec-camera_params->start_time);

  return NULL;
}
