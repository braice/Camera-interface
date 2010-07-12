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
  //If there is no image to be processed we return
  if((camera_params->wand_data.raw_img_ok==0)&&(camera_params->wand_data.raw_img_old_ok==0))
    return;
  //imagemagick_process_image(camera_params,0);
  //imagemagick_display_image(camera_params);
}

void imagemagick_get_image(camera_parameters_t* camera_params)
{
  MagickBooleanType status;


  //We save the old one if we already have an image
  if(camera_params->wand_data.raw_img_ok==1)
  {
    //NOTE : USE MUTEX
    camera_params->wand_data.raw_img_old_ok=0;
    camera_params->wand_data.raw_magick_wand_old=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
    camera_params->wand_data.raw_img_old_ok=1;
  }

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
  }

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


void imagemagick_process_image(camera_parameters_t* camera_params, int threads_enter)
{
  MagickBooleanType status;
  //We save the old one
  if((camera_params->wand_data.raw_img_ok==0)&&(camera_params->wand_data.raw_img_old_ok==0))
  {
    add_to_statusbar(camera_params, 1, "Image processing : No frame to process");
    return;
  }
  camera_params->wand_data.processed_img_old_ok=0;
  camera_params->wand_data.processed_magick_wand_old=CloneMagickWand(camera_params->wand_data.processed_magick_wand);
  camera_params->wand_data.processed_img_old_ok=1;
  camera_params->wand_data.processed_img_ok=0;
  camera_params->wand_data.processed_magick_wand=DestroyMagickWand(camera_params->wand_data.processed_magick_wand);
  //If the new image is not ok, we process the old one
  if(camera_params->wand_data.raw_img_ok==1)
    camera_params->wand_data.processed_magick_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
  else
    camera_params->wand_data.processed_magick_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand_old);
  MagickSetImageDepth(camera_params->wand_data.processed_magick_wand,16);

  /************** Background substraction *****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_background_button))==TRUE)
  {
    if(camera_params->background_set)
    {
      //We check if the sizes are compatible
      if((MagickGetImageWidth(camera_params->wand_data.background_wand)!=MagickGetImageWidth(camera_params->wand_data.processed_magick_wand))||
	 (MagickGetImageHeight(camera_params->wand_data.background_wand)!=MagickGetImageHeight(camera_params->wand_data.processed_magick_wand)))
      {
	gchar *msg;
	if(threads_enter)
	  gdk_threads_enter();
	msg = g_strdup_printf ("!!! Incompatible sizes\nBg size: %ldx%ld",
			       MagickGetImageWidth(camera_params->wand_data.background_wand),
			       MagickGetImageHeight(camera_params->wand_data.background_wand));
	gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_background_info_text)),msg,-1);
	g_free (msg);
	if(threads_enter)
	  gdk_threads_leave();

      }
      else
      {
	//We do the substraction
	//see http://www.simplesystems.org/RMagick/doc/constants.html#CompositeOperator
	//http://www.imagemagick.org/api/magick-image.php#MagickCompositeImage
	status=MagickCompositeImage(camera_params->wand_data.processed_magick_wand,
			     camera_params->wand_data.background_wand,
			     MinusCompositeOp,
			     0,0);
	if (status == MagickFalse)
	  ThrowWandException(camera_params->wand_data.processed_magick_wand);

      }
    }else{
      gchar *msg;
      if(threads_enter)
	gdk_threads_enter();
      msg = g_strdup_printf ("!!! Background NOT set");
      gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_background_info_text)),msg,-1);
      g_free (msg);
      if(threads_enter)
	gdk_threads_leave();
    }
  }


  /********* Level and brightness/contrast ****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_autolevel))==FALSE)
  {
    MagickLevelImage(camera_params->wand_data.processed_magick_wand, gtk_adjustment_get_value(camera_params->objects->soft_level_min_adj), 1, gtk_adjustment_get_value(camera_params->objects->soft_level_max_adj));

    MagickBrightnessContrastImage(camera_params->wand_data.processed_magick_wand,gtk_adjustment_get_value(camera_params->objects->soft_brightness_adj),gtk_adjustment_get_value(camera_params->objects->soft_contrast_adj));
  }


  /**************** Auto level and auto gamma ***************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_autolevel))==TRUE)
  {
    MagickAutoLevelImage(camera_params->wand_data.processed_magick_wand);
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
    MagickResizeImage(camera_params->wand_data.processed_magick_wand,
		      n*MagickGetImageWidth(camera_params->wand_data.processed_magick_wand),
		      n*MagickGetImageHeight(camera_params->wand_data.processed_magick_wand),
		      PointFilter,0);
  }
  /**************** Image rotation ****************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_rotate_image))==TRUE)
  {
    PixelWand *black;
    black = NewPixelWand();
    PixelSetColor(black, "black");
    status=MagickRotateImage( camera_params->wand_data.processed_magick_wand, black, gtk_adjustment_get_value(camera_params->objects->soft_angle_adj));
    if (status == MagickFalse)
      ThrowWandException(camera_params->wand_data.processed_magick_wand);
    DestroyPixelWand(black);
  }

  /************* Software ROI *********************/
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params->objects->soft_cut_img))==TRUE)
  {
    MagickCropImage( camera_params->wand_data.processed_magick_wand,
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_width),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_height),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_x),
                     gtk_adjustment_get_value(camera_params->objects->ROI_soft_adjust_y));
  }


  // MagickGammaImage
  // MagickMinifyImage
  // MagickScaleImage

  //processing done
  camera_params->wand_data.processed_img_ok=1;

  //We copy the real image for display (for keeping the other one for saving)
  camera_params->wand_data.display_magick_wand=DestroyMagickWand(camera_params->wand_data.display_magick_wand);
  camera_params->wand_data.display_magick_wand=CloneMagickWand(camera_params->wand_data.processed_magick_wand);


  /******************** Compute the mean over a ROI and display the ROI **********************/
  double mean_roi1,std_roi1;
 
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
    }
  }
  
  /******************** Compute the overall mean  **********************/
  double mean,std;
  MagickGetImageChannelMean(camera_params->wand_data.processed_magick_wand,AllChannels,&mean,&std);

  /******************** Fill the treeview     **********************/
  if(camera_params->list_store_iter_ok)
  {
    if(threads_enter)
      gdk_threads_enter();
    gtk_list_store_set(camera_params->objects->statistics_list, &camera_params->list_iter,
		       3,(gdouble) mean,
		       4,(gdouble) mean_roi1,
		       5,(gdouble) mean_roi2,
		       -1);
    if(threads_enter)
      gdk_threads_leave();
  }
  /******************** Display image data   **********************/

  gchar *msg;
  
  msg = g_strdup_printf ("Size: %ldx%ld\tMean %.3lf\tStd: %.3lf\nMean1 %.3lf\tStd1: %.3lf\tMean2 %.3lf\tStd2: %.3lf",
			 MagickGetImageWidth(camera_params->wand_data.processed_magick_wand),
			 MagickGetImageHeight(camera_params->wand_data.processed_magick_wand),
			 mean, std,
			 mean_roi1, std_roi1,
			 mean_roi2, std_roi2);
  if(threads_enter)
    gdk_threads_enter();
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_image_text)),msg,-1);
  if(threads_enter)
    gdk_threads_leave();
  g_free (msg);
  

}


void imagemagick_set_bg(camera_parameters_t* camera_params)
{
  if(camera_params->background_set)
    camera_params->wand_data.background_wand=DestroyMagickWand(camera_params->wand_data.background_wand);
  camera_params->wand_data.background_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
  camera_params->background_set=1;
  gchar *msg;
  msg = g_strdup_printf ("Bg set, Size: %ldx%ld",
			 MagickGetImageWidth(camera_params->wand_data.background_wand),
			 MagickGetImageHeight(camera_params->wand_data.background_wand));
  gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params->objects->soft_background_info_text)),msg,-1);
  g_free (msg);
}

void imagemagick_display_image(camera_parameters_t* camera_params)
{
  int width,height;

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



  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->processed_image);

}
