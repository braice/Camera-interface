/*
 * Program to control the GigE camera. Imagemagick part
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
#include <unistd.h>

  //http://www.imagemagick.org/api/magick-wand.php
  //http://www.imagemagick.org/api/magick-wand.php#NewMagickWandFromImage
  //http://blog.gmane.org/gmane.comp.video.image-magick.devel/month=20060301/page=1
  //http://studio.imagemagick.org/pipermail/magick-developers/2002-October/001077.html
  //http://studio.imagemagick.org/pipermail/magick-developers/2002-October/001083.html

  //GetExceptionInfo(camera_params->wand_data.exception);
  //camera_params->wand_data.image=ConstituteImage(width,height,"I",ShortPixel,camera_params->camera_frame.ImageBuffer,camera_params->wand_data.exception);
  //if (camera_params->wand_data.exception->severity != UndefinedException) {
  //g_print("Exception imagemagick %s\n", camera_params->wand_data.exception->reason );
  //}
  // if (camera_params->wand_data.image == (Image *) NULL) {
  // g_print( "ConstitureImage failed\n");
  //}


void update_soft_val(camera_parameters_t* camera_params)
{
  camera_params->processed_image_needs_update=1;
}

void imagemagick_get_image(camera_parameters_t* camera_params)
{
  MagickBooleanType status;


  //We lock the mutex
  pthread_mutex_lock(&camera_params->wand_data.raw_img_mutex);
  camera_params->wand_data.raw_img_ok=0;

  ClearMagickWand(camera_params->wand_data.raw_magick_wand);
  status=MagickConstituteImage(camera_params->wand_data.raw_magick_wand,camera_params->camera_frame.Width,camera_params->camera_frame.Height,"I",ShortPixel,camera_params->camera_frame.ImageBuffer);
  if(status == MagickFalse)
  {
    ThrowWandException(camera_params->wand_data.raw_magick_wand);
  }
  else
  {
    camera_params->wand_data.raw_img_ok=1;
    /********************* Image autosaving ***************/
    gchar *msg;
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->raw_autosave))==TRUE)
    {
      if(camera_params->wand_data.raw_directory)
      {
	MagickWand *tempwand;
	tempwand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
	//we scale the image on 16 bits before saving
	imagemagick_levelimage(tempwand, 0, 1<<((int)camera_params->sensorbits));
	gchar *filename;
	struct timeval tv;
	gettimeofday(&tv, (struct timezone *) NULL);
	filename=g_strdup_printf("%s/image%ld.%.0f.png",camera_params->wand_data.raw_directory,tv.tv_sec,((double)tv.tv_usec)/10000);
	
	status=MagickSetImageFormat(tempwand,"PNG");
	if (status == MagickFalse)
	  ThrowWandException(tempwand);
	status=MagickSetImageCompression(tempwand,NoCompression);
	if (status == MagickFalse)
	  ThrowWandException(tempwand);
	status=MagickSetImageCompressionQuality(tempwand,0);
	if (status == MagickFalse)
	  ThrowWandException(tempwand);
	status=MagickWriteImage(tempwand,filename);    
	if (status == MagickFalse)
	  ThrowWandException(tempwand);
	DestroyMagickWand(tempwand);
	msg=g_strdup_printf("Image saved to %s",filename);
	g_free (filename);
      }
      else
      {
	msg=g_strdup_printf("Directory not choosen");
      }
    }
    else
      msg=g_strdup_printf("No autosaving");
    gdk_threads_enter();
    gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->raw_text)),msg,-1);
    gdk_threads_leave();
    g_free(msg);
  }
  //We unlock the mutex
  pthread_mutex_unlock(&camera_params->wand_data.raw_img_mutex);


}

MagickBooleanType imagemagick_draw_roi(MagickWand *wand,char* color, int x, int y, int width, int height)
{
  MagickBooleanType status;
  PixelWand *p_color;
  p_color = NewPixelWand();
  PixelSetColor(p_color, color);
  PixelWand *none;
  none = NewPixelWand();
  PixelSetColor(none, "none");

  DrawingWand *drawing;
  drawing=NewDrawingWand();
  DrawSetViewbox(drawing,0,0,MagickGetImageWidth(wand),MagickGetImageHeight(wand));
  DrawSetFillColor(drawing, none);
  DrawSetStrokeColor(drawing, p_color);
  DrawSetStrokeWidth(drawing, 1);
  DrawRectangle(drawing,x,y,x+width,y+height);
  //put the drawing onto the image
  status=MagickDrawImage(wand, drawing);
  DestroyDrawingWand(drawing);
  DestroyPixelWand(none);
  DestroyPixelWand(p_color);
  return status;

}



MagickBooleanType imagemagick_levelimage(MagickWand *wand,int black, int white)
{


  Image *image;
  PixelPacket *pixels;
  int x,y,width, height;
  int value;
  int max=(1<<16)-1; //replace by quantumrange
  
  if(white==black)
    white++;

  double scale;
  scale=(double)max/((double)white-(double)black);
  //g_print("Scale %f\n",scale);
  /*
  int scale;
  scale=max/(white-black);
  g_print("Scale %d\n",scale);
  */
  width = MagickGetImageWidth(wand);
  height = MagickGetImageHeight(wand);
  image=GetImageFromMagickWand(wand);
  pixels=GetAuthenticPixels(image,0,0,width,height, NULL);
  for (y=0; y < height; y++)
  for (x=0; x < width; x++)
    {
      if(pixels->red<black)
	value=0;
      else if(pixels->red>white)
	value=max;
      else
	value=(pixels->red-black)*scale; 
      
      pixels->red=value;
      pixels->green=value;
      pixels->blue=value;
      pixels++;
    }

  SyncAuthenticPixels(image,NULL);
  return 0;

}

void imagemagick_process_image(camera_parameters_t* camera_params)
{
  MagickBooleanType status;
  struct timeval tv_before,tv_after;
  gdouble time_bg,time_level,time_magnify,time_rot,time_cut,time_mean,time_list;
  gchar *msg_bg;
  gdouble fraction_roi1;
  gdouble fraction_roi2;
  gdouble fraction_tot;
  fraction_roi1=-1;
  fraction_roi2=-1;
  fraction_tot=-1;


  msg_bg=NULL;

  time_bg=0;time_level=0;time_magnify=0;time_rot=0;time_cut=0;time_mean=0;time_list=0;
  //We save the old one

  if((camera_params->wand_data.raw_img_ok==0))
  {
    add_to_statusbar(camera_params, 1, "Image processing : No frame to process");
    return;
  }
  //We lock the mutex
  pthread_mutex_lock(&camera_params->wand_data.processed_img_mutex);
  camera_params->wand_data.processed_img_ok=0;
  camera_params->wand_data.processed_magick_wand=DestroyMagickWand(camera_params->wand_data.processed_magick_wand);
  //We copy the raw image
  pthread_mutex_lock(&camera_params->wand_data.raw_img_mutex);
  camera_params->wand_data.processed_magick_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
  pthread_mutex_unlock(&camera_params->wand_data.raw_img_mutex);
  MagickSetImageDepth(camera_params->wand_data.processed_magick_wand,16);

  /************** Background substraction *****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_background_button))==TRUE)
  {
    if(camera_params->background_set)
    {
      pthread_mutex_lock(&camera_params->wand_data.background_img_mutex);
      //We check if the sizes are compatible
      if((MagickGetImageWidth(camera_params->wand_data.background_wand)!=MagickGetImageWidth(camera_params->wand_data.processed_magick_wand))||
	 (MagickGetImageHeight(camera_params->wand_data.background_wand)!=MagickGetImageHeight(camera_params->wand_data.processed_magick_wand)))
      {
	msg_bg = g_strdup_printf ("!!! Incompatible sizes\nBg size: %ldx%ld",
			       MagickGetImageWidth(camera_params->wand_data.background_wand),
			       MagickGetImageHeight(camera_params->wand_data.background_wand));

      }
      else
      {
	//We do the substraction
	//see http://www.simplesystems.org/RMagick/doc/constants.html#CompositeOperator
	//http://www.imagemagick.org/api/magick-image.php#MagickCompositeImage
	//MinusCompositeOp,
	gettimeofday (&tv_before, (struct timezone *) NULL);
	status=MagickCompositeImage(camera_params->wand_data.processed_magick_wand,
			     camera_params->wand_data.background_wand,
			     DifferenceCompositeOp,
			     0,0);
	if (status == MagickFalse)
	  ThrowWandException(camera_params->wand_data.processed_magick_wand);
	gettimeofday (&tv_after, (struct timezone *) NULL);
	time_bg = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;

      }
      pthread_mutex_unlock(&camera_params->wand_data.background_img_mutex);
    }
    else
    {
      msg_bg = g_strdup_printf ("!!! Background NOT set");
    }
  }


  /********* Level ****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_autolevel))==FALSE)
  {
    gettimeofday (&tv_before, (struct timezone *) NULL);
    //MagickLevelImage(camera_params->wand_data.processed_magick_wand, gtk_adjustment_get_value(camera_params->objects->soft_level_min_adj), 1, gtk_adjustment_get_value(camera_params->objects->soft_level_max_adj));
    imagemagick_levelimage(camera_params->wand_data.processed_magick_wand, gtk_adjustment_get_value(camera_params->objects->soft_level_min_adj), gtk_adjustment_get_value(camera_params->objects->soft_level_max_adj));
    gettimeofday (&tv_after, (struct timezone *) NULL);
    time_level = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;
  }


  /**************** Auto level and auto gamma ***************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_autolevel))==TRUE)
  {
    gettimeofday (&tv_before, (struct timezone *) NULL);
    MagickAutoLevelImage(camera_params->wand_data.processed_magick_wand);
    gettimeofday (&tv_after, (struct timezone *) NULL);
    time_level = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;
  }
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_autogamma))==TRUE)
  {
    MagickAutoGammaImage(camera_params->wand_data.processed_magick_wand);
  }


  /*************** Software magnification ******************/
  // MagickSetImageInterpolateMethod
  int n;
  n=1;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_magn_x2))==TRUE)
    n=2;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_magn_x4))==TRUE)
    n=4;
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_magn_x8))==TRUE)
    n=8;
  if(n!=1)
  {
    //see http://www.dylanbeattie.net/magick/filters/result.html
    //default filter for magnifyimage : CubicFilter
    gettimeofday (&tv_before, (struct timezone *) NULL);
    MagickResizeImage(camera_params->wand_data.processed_magick_wand,
		      n*MagickGetImageWidth(camera_params->wand_data.processed_magick_wand),
		      n*MagickGetImageHeight(camera_params->wand_data.processed_magick_wand),
		      PointFilter,0);
    gettimeofday (&tv_after, (struct timezone *) NULL);
    time_magnify = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;
  }


  /**************** Image rotation ****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_rotate_image))==TRUE)
  {
    gettimeofday (&tv_before, (struct timezone *) NULL);
    PixelWand *black;
    black = NewPixelWand();
    PixelSetColor(black, "black");
    status=MagickRotateImage( camera_params->wand_data.processed_magick_wand, black, gtk_adjustment_get_value(camera_params->objects->soft_angle_adj));
    if (status == MagickFalse)
      ThrowWandException(camera_params->wand_data.processed_magick_wand);
    DestroyPixelWand(black);
    gettimeofday (&tv_after, (struct timezone *) NULL);
    time_rot = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;
  }


  /************* Software ROI *********************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_cut_img))==TRUE)
  {
    gettimeofday (&tv_before, (struct timezone *) NULL);
    MagickCropImage( camera_params->wand_data.processed_magick_wand,
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_width),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_height),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_x),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_y));
    //We set the image page to avoid disturbing other functions below
    MagickSetImagePage(camera_params->wand_data.processed_magick_wand,
		       gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_width),
		       gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_height),
		       0,0);
    gettimeofday (&tv_after, (struct timezone *) NULL);
    time_cut = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;
  }


  // MagickGammaImage
  // MagickMinifyImage
  // MagickScaleImage

  //processing done
  camera_params->wand_data.processed_img_ok=1;

  //We copy the real image for display (for keeping the other one for saving)
  pthread_mutex_lock(&camera_params->wand_data.display_img_mutex);
  camera_params->wand_data.display_magick_wand=DestroyMagickWand(camera_params->wand_data.display_magick_wand);
  camera_params->wand_data.display_magick_wand=CloneMagickWand(camera_params->wand_data.processed_magick_wand);



  /******************** Compute the mean over a ROI and display the ROI **********************/
  double mean_roi1,std_roi1;
  gettimeofday (&tv_before, (struct timezone *) NULL);
 
  mean_roi1=0;
  std_roi1=0;
  if((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_display_mean_roi1))==TRUE)||
     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_compute_mean_roi1))==TRUE))
  {
    int roi1_x, roi1_y, roi1_width, roi1_height;
    roi1_x = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_x1);
    roi1_y = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_y1);
    roi1_width = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_width1);
    roi1_height = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_height1);
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_display_mean_roi1))==TRUE)
    {
      status=imagemagick_draw_roi(camera_params->wand_data.display_magick_wand,"red",roi1_x, roi1_y, roi1_width, roi1_height);
      if (status == MagickFalse)
	ThrowWandException(camera_params->wand_data.display_magick_wand);
    }
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_compute_mean_roi1))==TRUE)
    {
      MagickWand *temp_wand;
      temp_wand = MagickGetImageRegion(camera_params->wand_data.processed_magick_wand, roi1_width,roi1_height,roi1_x,roi1_y);
      status=MagickGetImageChannelMean(temp_wand,AllChannels,&mean_roi1,&std_roi1);
      if (status == MagickFalse)
	ThrowWandException(camera_params->wand_data.display_magick_wand);
      DestroyMagickWand(temp_wand);
      /********************** mean bar ***************************/
      fraction_roi1=(mean_roi1-gtk_adjustment_get_value(camera_params->objects->processed_mean_roi1_min_adj))/(gtk_adjustment_get_value(camera_params->objects->processed_mean_roi1_max_adj)-gtk_adjustment_get_value(camera_params->objects->processed_mean_roi1_min_adj));
      fraction_roi1=fraction_roi1<0?0:fraction_roi1;
      fraction_roi1=fraction_roi1>1?1:fraction_roi1;
    }
  }

  double mean_roi2,std_roi2;
 
  mean_roi2=0;
  std_roi2=0;
  if((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_display_mean_roi2))==TRUE)||
     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_compute_mean_roi2))==TRUE))
  {
    int roi2_x, roi2_y, roi2_width, roi2_height;
    roi2_x = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_x2);
    roi2_y = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_y2);
    roi2_width = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_width2);
    roi2_height = gtk_adjustment_get_value(camera_params->objects->ROI_soft_mean_adjust_height2);
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_display_mean_roi2))==TRUE)
    {
      status=imagemagick_draw_roi(camera_params->wand_data.display_magick_wand, "green", roi2_x, roi2_y, roi2_width, roi2_height);
      if (status == MagickFalse)
	ThrowWandException(camera_params->wand_data.display_magick_wand);
    }
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_compute_mean_roi2))==TRUE)
    {
      MagickWand *temp_wand;
      temp_wand = MagickGetImageRegion(camera_params->wand_data.processed_magick_wand, roi2_width,roi2_height,roi2_x,roi2_y);
      status=MagickGetImageChannelMean(temp_wand,AllChannels,&mean_roi2,&std_roi2);
      if (status == MagickFalse)
	ThrowWandException(camera_params->wand_data.display_magick_wand);
      DestroyMagickWand(temp_wand);
      /********************** mean bar ***************************/
      fraction_roi2=(mean_roi2-gtk_adjustment_get_value(camera_params->objects->processed_mean_roi2_min_adj))/(gtk_adjustment_get_value(camera_params->objects->processed_mean_roi2_max_adj)-gtk_adjustment_get_value(camera_params->objects->processed_mean_roi2_min_adj));
      fraction_roi2=fraction_roi2<0?0:fraction_roi2;
      fraction_roi2=fraction_roi2>1?1:fraction_roi2;
    }
  }

  /******************** Compute the overall mean  **********************/
  double mean,std;
  long int img_width,img_height;
  img_width = MagickGetImageWidth(camera_params->wand_data.processed_magick_wand);
  img_height = MagickGetImageHeight(camera_params->wand_data.processed_magick_wand);
  MagickGetImageChannelMean(camera_params->wand_data.processed_magick_wand,AllChannels,&mean,&std);
  /********************** mean bar ***************************/
  fraction_tot=(mean-gtk_adjustment_get_value(camera_params->objects->processed_mean_min_adj))/(gtk_adjustment_get_value(camera_params->objects->processed_mean_max_adj)-gtk_adjustment_get_value(camera_params->objects->processed_mean_min_adj));
  fraction_tot=fraction_tot<0?0:fraction_tot;
  fraction_tot=fraction_tot>1?1:fraction_tot;
  gettimeofday (&tv_after, (struct timezone *) NULL);
  time_mean = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;


  //We unlock the mutex
  pthread_mutex_unlock(&camera_params->wand_data.display_img_mutex);
  pthread_mutex_unlock(&camera_params->wand_data.processed_img_mutex);


  /******************** Fill the treeview     **********************/
  gettimeofday (&tv_before, (struct timezone *) NULL);
  gtk_list_store_insert_with_values(camera_params->objects->statistics_list, NULL,
				    camera_params->list_store_rows,
				    0, (gint)camera_params->image_number,
				    1, (gint)camera_params->image_acq_number,
				    2, camera_params->image_time,
				    3, (gdouble) camera_params->raw_image_mean,
				    4, (gdouble) mean,
				    5, (gdouble) mean_roi1,
				    6, (gdouble) mean_roi2,
				    -1);
  camera_params->list_store_rows++;

  if (camera_params->list_file != NULL)
    {
      fprintf(camera_params->list_file,"%d\t%d\t%f\t%f\t%f\t%f\t%f\n",
	      (gint)camera_params->image_number,
	      (gint)camera_params->image_acq_number,
	      camera_params->image_time,
	      (gdouble)camera_params->raw_image_mean,
	      (gdouble)mean,
	      (gdouble)mean_roi1,
	      (gdouble)mean_roi2);
      fflush(camera_params->list_file);
    }


  //if we asked for autoscrolling the chart, we do it
  if(camera_params->autoscroll_chart)
  {
    GtkTreePath *path;
    /* Now get a path from the index. */
    path = gtk_tree_path_new_from_indices(camera_params->list_store_rows - 1, -1);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW (camera_params->objects->stats_treeview), path, NULL, TRUE, 0.0, 0.0); 
    /* Drop the path, we're done with it. */
    gtk_tree_path_free(path);
  }
  gettimeofday (&tv_after, (struct timezone *) NULL);
  time_list = (tv_after.tv_sec-tv_before.tv_sec)*1000000+tv_after.tv_usec-tv_before.tv_usec;

  /******************** Display image data   **********************/

  gchar *msg,*msg2;
  
  msg = g_strdup_printf ("Size: %ldx%ld\tMean %.3lf\tStd: %.3lf\nMean1 %.3lf\tStd1: %.3lf\tMean2 %.3lf\tStd2: %.3lf",
			 img_width,
			 img_height,
			 mean, std,
			 mean_roi1, std_roi1,
			 mean_roi2, std_roi2);

  msg2 = g_strdup_printf ("Timings (ms):\nBackground\n   %.0f\nLevelling\n   %.0f\nMagnify\n   %.0f\nRotation\n   %.0f\nCut\n   %.0f\nMean\n   %.0f\nList\n   %.0f\n",
			  time_bg/1000,time_level/1000,time_magnify/1000,time_rot/1000,time_cut/1000,time_mean/1000,time_list/1000);
  gdk_threads_enter();
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_image_text)),msg,-1);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_timing_text)),msg2,-1);
  if(msg_bg)
    gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_background_info_text)),msg,-1);
 
  if(fraction_roi1>=0)
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(camera_params->objects->processed_mean_roi1_bar),fraction_roi1);
  if(fraction_roi2>=0)
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(camera_params->objects->processed_mean_roi2_bar),fraction_roi2);
  if(fraction_tot>=0)
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(camera_params->objects->processed_mean_bar),fraction_tot);


  gdk_threads_leave();
  g_free (msg);
  g_free (msg2);
  if(msg_bg)  
    g_free (msg_bg);


  
}


void imagemagick_set_bg(camera_parameters_t* camera_params)
{
  pthread_mutex_lock(&camera_params->wand_data.background_img_mutex);

  if(camera_params->background_set)
    camera_params->wand_data.background_wand=DestroyMagickWand(camera_params->wand_data.background_wand);
  camera_params->wand_data.background_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
  camera_params->background_set=1;
  gchar *msg;
  msg = g_strdup_printf ("Bg set, Size: %ldx%ld\nimage %ld",
			 MagickGetImageWidth(camera_params->wand_data.background_wand),
			 MagickGetImageHeight(camera_params->wand_data.background_wand),
			 camera_params->image_number);
  pthread_mutex_unlock(&camera_params->wand_data.background_img_mutex);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_background_info_text)),msg,-1);
  g_free (msg);
}

void imagemagick_display_image(camera_parameters_t* camera_params)
{
  int width,height;
  pthread_mutex_lock(&camera_params->wand_data.display_img_mutex);
  width=MagickGetImageWidth(camera_params->wand_data.display_magick_wand);
  height=MagickGetImageHeight(camera_params->wand_data.display_magick_wand);

  //If we didn't set a pixbuf yet, we do it
  if(camera_params->processed_image_pixbuff==NULL)
  {
    g_print("New pixbuff %d %d\n", width, height);
    camera_params->processed_image_pixbuff=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    //We associate this new pixbuf with the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(camera_params->objects->processed_image),camera_params->processed_image_pixbuff);
    g_object_ref(camera_params->processed_image_pixbuff);
  }
  else if((width!=gdk_pixbuf_get_width(camera_params->processed_image_pixbuff))||
	  (height!=gdk_pixbuf_get_height(camera_params->processed_image_pixbuff)))
  {
    g_print("pixbuff Resize %d %d\n", width, height);
    //The size of the frame is different from the size of the pixbuff
    //we dereference the pixbuff so the garbage colletor can work
    g_object_unref(camera_params->processed_image_pixbuff);
    //clear the image
    gtk_image_clear(GTK_IMAGE(camera_params->objects->processed_image));
    //create the new pixbuff
    camera_params->processed_image_pixbuff=
      gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    //We associate this new pixbuf with the image
    gtk_image_set_from_pixbuf(GTK_IMAGE(camera_params->objects->processed_image),camera_params->processed_image_pixbuff);
    g_object_ref(camera_params->processed_image_pixbuff);
  }
  /********************** copy the pixels *************************/
  guchar *pixels;
  pixels=gdk_pixbuf_get_pixels(camera_params->processed_image_pixbuff);


  MagickSetImageDepth(camera_params->wand_data.display_magick_wand,8);
  //g_print("New depth %lu\n", MagickGetImageDepth(camera_params->wand_data.display_magick_wand));

  //Write the image in the pixbuff
  int row, rowstride;
  rowstride=gdk_pixbuf_get_rowstride(camera_params->processed_image_pixbuff);
  //Line per line due to the rowstride
  for(row=0;row<height;row++)
  {
    MagickExportImagePixels(camera_params->wand_data.display_magick_wand,0,row,width,1,"RGB",CharPixel,pixels+row*rowstride);
  }


  pthread_mutex_unlock(&camera_params->wand_data.display_img_mutex);

  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->processed_image);

}
