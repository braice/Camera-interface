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

#define ETHERNET_PACKET_SIZE 1341
#define FRAME_WAIT_TIMEOUT 100

/* Convenience macros for obtaining objects from UI file */
#define CH_GET_OBJECT( builder, name, type, data ) \
    camera_params.objects->name = type( gtk_builder_get_object( builder, #name ) )
#define CH_GET_WIDGET( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_WIDGET, data )
#define CH_GET_ADJUSTMENT( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_ADJUSTMENT, data )

typedef struct gui_objects_t{
  GtkWidget *main_window;
  GtkWidget *main_status_bar;
  GtkWidget *camera_text;
  GtkWidget *image_number;
  GtkWidget *raw_image;
  GtkWidget *ext_trig;
  GtkWidget *trig_mult;
  GtkWidget *trig_single;
  GtkWidget *trig_cont;
  GtkWidget *freerun_trig;
  GtkWidget *ext1_trig;
  GtkWidget *ext2_trig;
  GtkWidget *framerate_trig;
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
}gui_objects_t;

typedef struct camera_parameters_t{
  //Camera handler
  tPvHandle camera_handler;
  tPvFrame camera_frame;
  //PixBuff
  GdkPixbuf *raw_image_pixbuff; 
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
  int roi_hard_x;
  int roi_hard_width;
  int roi_hard_y;
  int roi_hard_height;
  //Binning
  int binning_x;
  int binning_y;
  //The GUI objects
  gui_objects_t *objects;
}camera_parameters_t;


void *camera_thread_func(void* arg);
void camera_update_roi(camera_parameters_t* );
void camera_set_exposure(camera_parameters_t* camera_params);
void camera_set_triggering(camera_parameters_t* camera_params);


#endif
