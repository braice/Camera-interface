/*
 * Program to control the GigE camera Header file
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


#ifndef _CAMERA_H
#define _CAMERA_H


// We have a thread for controlling the camera
#include <pthread.h>
#include <gtk/gtk.h>
#include <PvApi.h>
#include <PvRegIo.h>
#include <wand/MagickWand.h>

#define FRAME_WAIT_TIMEOUT 100

/* Convenience macros for obtaining objects from UI file */
#define CH_GET_OBJECT( builder, name, type, data ) \
    camera_params.objects->name = type( gtk_builder_get_object( builder, #name ) )
#define CH_GET_WIDGET( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_WIDGET, data )
#define CH_GET_ADJUSTMENT( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_ADJUSTMENT, data )
#define CH_GET_LIST_STORE( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_LIST_STORE, data )

typedef struct gui_objects_t{
  GtkWidget *main_window;
  GtkWidget *main_status_bar;
  GtkWidget *camera_text;
  GtkWidget *image_number;
  GtkWidget *raw_image;
  GtkWidget *processed_image;
  GtkWidget *ext_trig;
  GtkWidget *trig_mult;
  GtkWidget *trig_single;
  GtkWidget *trig_cont;
  GtkWidget *freerun_trig;
  GtkWidget *ext1_trig;
  GtkWidget *ext2_trig;
  GtkWidget *framerate_trig;
  GtkWidget *directorychooserdialog;
  GtkWidget *imagesavedialog;
  GtkWidget *listsavedialog;
  GtkWidget *no_image_dialog;
  GtkWidget *ROI_confirm_dialog;
  GtkWidget *ROI_confirm_dialog_text;
  GtkWidget *stats_treeview;
  GtkWidget *acq_toggle;
  GtkWidget *mean_bar;
  GtkWidget *soft_rotate_image;
  GtkWidget *soft_autolevel;
  GtkWidget *soft_autogamma;
  GtkWidget *soft_magn_x2;
  GtkWidget *soft_magn_x4;
  GtkWidget *soft_magn_x8;
  GtkWidget *soft_cut_img;
  GtkWidget *soft_image_text;
  GtkWidget *soft_display_mean_roi1;
  GtkWidget *soft_compute_mean_roi1;
  GtkWidget *soft_display_mean_roi2;
  GtkWidget *soft_compute_mean_roi2;
  GtkWidget *soft_background_info_text;
  GtkWidget *soft_background_button;
  GtkWidget *raw_autosave;
  GtkWidget *processed_autosave;
  GtkAdjustment *Exp_adj_gain;
  GtkAdjustment *Exp_adj_time;
  GtkAdjustment *ROI_adjust_x;
  GtkAdjustment *ROI_adjust_y;
  GtkAdjustment *ROI_adjust_width;
  GtkAdjustment *ROI_adjust_height;
  GtkAdjustment *Bin_X_adj;
  GtkAdjustment *Bin_Y_adj;
  GtkAdjustment *Bytes_per_sec_adj;
  GtkAdjustment *Trig_nbframes_adj;
  GtkAdjustment *Trig_framerate_adj;
  GtkAdjustment *max_meanbar;
  GtkAdjustment *min_meanbar;
  GtkAdjustment *soft_brightness_adj;
  GtkAdjustment *soft_contrast_adj;
  GtkAdjustment *soft_angle_adj;
  GtkAdjustment *soft_level_min_adj;
  GtkAdjustment *soft_level_max_adj;
  GtkAdjustment *ROI_soft_adjust_width;
  GtkAdjustment *ROI_soft_adjust_height;
  GtkAdjustment *ROI_soft_adjust_x;
  GtkAdjustment *ROI_soft_adjust_y;
  GtkAdjustment *ROI_soft_mean_adjust_x1;
  GtkAdjustment *ROI_soft_mean_adjust_width1;
  GtkAdjustment *ROI_soft_mean_adjust_y1;
  GtkAdjustment *ROI_soft_mean_adjust_height1;
  GtkAdjustment *ROI_soft_mean_adjust_x2;
  GtkAdjustment *ROI_soft_mean_adjust_width2;
  GtkAdjustment *ROI_soft_mean_adjust_y2;
  GtkAdjustment *ROI_soft_mean_adjust_height2;
  GtkListStore *statistics_list;
}gui_objects_t;

#define DIR_CHOOSING_RAW 1
#define DIR_CHOOSING_PROCESSED 2
typedef struct magickwand_data_t{
  MagickWand *processed_magick_wand;  //The magickwand data
  MagickWand *processed_magick_wand_old;  //The magickwand data
  int processed_img_ok;
  int processed_img_old_ok;
  MagickWand *display_magick_wand;  //The magickwand data
  MagickWand *raw_magick_wand;  //The magickwand data
  MagickWand *raw_magick_wand_old;  //The magickwand data
  int raw_img_ok;
  int raw_img_old_ok;
  MagickWand *background_wand;
  MagickWand *saving_wand;
  char *raw_directory;
  char *processed_directory;
  int dirchoosing;
}magickwand_data_t;

#define ROI_CLICK_NONE 0
#define ROI_CLICK_CORNER1 1
#define ROI_CLICK_CORNER2 2

typedef struct camera_parameters_t{
  //Camera handler
  tPvHandle camera_handler;
  tPvFrame camera_frame;
  //PixBuff
  GdkPixbuf *raw_image_pixbuff; 
  GdkPixbuf *processed_image_pixbuff; 
  //sensor info
  unsigned long sensorbits;
  unsigned long sensorwidth;
  unsigned long sensorheight;
  //Put this variable to 1 to shutdown the thread for the camera
  int camera_thread_shutdown;
  pthread_t camera_thread;
  int camera_connected;
  //The image number
  long int image_number;
  //Are we grabbing images ?
  int grab_images;
  int grabbing_images;
  //Exposure information
  int exp_time; //time in milliseconds
  int exp_gain; //camera gain
  //Hardware region of interest information
  int roi_hard_active;
  int roi_hard_corners[2][2]; //The corners of the selected ROI
  int roi_hard_clicking; //Are we selecting the ROI on the image
  int roi_hard_X_lastimage;
  int roi_hard_Y_lastimage;
  //Binning
  int binning_x;
  int binning_y;
  //Do we autoscroll the chart
  int autoscroll_chart;
  //Is the iterator for the chart ok ?
  int list_store_iter_ok;
  GtkTreeIter list_iter;
  //Background
  int background_set;
  //The magick wand/imagemagick information
  magickwand_data_t wand_data;
  //The GUI objects
  gui_objects_t *objects;
}camera_parameters_t;

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


void *camera_thread_func(void* arg);
void camera_update_roi(camera_parameters_t* );
void camera_set_exposure(camera_parameters_t* camera_params);
void camera_set_triggering(camera_parameters_t* camera_params);
void camera_reset_roi(camera_parameters_t* camera_params);
void add_to_statusbar(camera_parameters_t *camera_params, int enter_threads, const char *psz_format, ...);

void imagemagick_get_image(camera_parameters_t* camera_params);
void imagemagick_process_image(camera_parameters_t* camera_params, int threads_enter);
void imagemagick_display_image(camera_parameters_t* camera_params);
void update_soft_val(camera_parameters_t* camera_params);
void imagemagick_set_bg(camera_parameters_t* camera_params);

#endif
