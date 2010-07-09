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

  //http://www.imagemagick.org/api/constitute.php
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


#define ThrowWandException(wand) \
  { \
  char \
    *description; \
 \
  ExceptionType \
    severity; \
 \
  description=MagickGetException(wand,&severity); \
  (void) g_print("Magickwand exception %s %s %lu %s\n",GetMagickModule(),description); \
  description=(char *) MagickRelinquishMemory(description); \
}

void imagemagick_get_image(camera_parameters_t* camera_params)
{
  MagickBooleanType status;

  ClearMagickWand(camera_params->wand_data.raw_magick_wand);
  status=MagickConstituteImage(camera_params->wand_data.raw_magick_wand,camera_params->camera_frame.Width,camera_params->camera_frame.Height,"I",ShortPixel,camera_params->camera_frame.ImageBuffer);
  if (status == MagickFalse)
    ThrowWandException(camera_params->wand_data.magick_wand);
  //MagickCompositeImage (bg substract)

}

void imagemagick_process_image(camera_parameters_t* camera_params)
{
  MagickBooleanType status;

  camera_params->wand_data.magick_wand=DestroyMagickWand(camera_params->wand_data.magick_wand);
  camera_params->wand_data.magick_wand=CloneMagickWand(camera_params->wand_data.raw_magick_wand);
  MagickSetImageDepth(camera_params->wand_data.magick_wand,16);
  MagickLevelImage(camera_params->wand_data.magick_wand, 0, 1, 1<<((int)camera_params->sensorbits));
  //MagickLinearStretchImage(camera_params->wand_data.magick_wand,0,1<<((int)camera_params->sensorbits));
  //MagickLinearStretchImage(camera_params->wand_data.magick_wand,0,4096);

  //g_print("%d\n",1<<((int)camera_params->sensorbits));
  MagickBrightnessContrastImage(camera_params->wand_data.magick_wand,gtk_adjustment_get_value(camera_params->objects->Brightness_adj),gtk_adjustment_get_value(camera_params->objects->Contrast_adj));

  PixelWand *black;
  black = NewPixelWand();
  PixelSetColor(black, "black");
  status=MagickRotateImage( camera_params->wand_data.magick_wand, black, gtk_adjustment_get_value(camera_params->objects->soft_angle_adj));
  if (status == MagickFalse)
    ThrowWandException(camera_params->wand_data.magick_wand);
  DestroyPixelWand(black);

  // MagickAutoLevelImage(camera_params->wand_data.magick_wand);
  // MagickCropImage
  // MagickGammaImage
  // MagickGetImageChannelMean // MagickGetImageChannelMean(cutted_wand,AllChannels,&mean,&std);
  // MagickGetImageRegion
  // MagickMagnifyImage
  // MagickMinifyImage
  // MagickNormalizeImage
  // MagickScaleImage
}


void imagemagick_display_image(camera_parameters_t* camera_params)
{
  int width,height;
  width=MagickGetImageWidth(camera_params->wand_data.magick_wand);
  height=MagickGetImageHeight(camera_params->wand_data.magick_wand);

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


  MagickSetImageDepth(camera_params->wand_data.magick_wand,8);
  //g_print("New depth %lu\n", MagickGetImageDepth(camera_params->wand_data.magick_wand));

  //Write the image in the pixbuff
  int row, rowstride;
  rowstride=gdk_pixbuf_get_rowstride(camera_params->processed_image_pixbuff);
  //Line per line due to the rowstride
  for(row=0;row<height;row++)
  {
    MagickExportImagePixels(camera_params->wand_data.magick_wand,0,row,width,1,"RGB",CharPixel,pixels+row*rowstride);
  }



  //We force the image to be refreshed
  gtk_widget_queue_draw(camera_params->objects->processed_image);

}
