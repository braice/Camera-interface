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

#define ETHERNET_PACKET_SIZE 1341

/* Convenience macros for obtaining objects from UI file */
#define CH_GET_OBJECT( builder, name, type, data ) \
    camera_params.objects->name = type( gtk_builder_get_object( builder, #name ) )
#define CH_GET_WIDGET( builder, name, data ) \
    CH_GET_OBJECT( builder, name, GTK_WIDGET, data )

typedef struct gui_objects_t{
  GtkWidget *main_window;
  GtkWidget *main_status_bar;
}gui_objects_t;

typedef struct camera_parameters_t{
  //Put this variable to 1 to shutdown the thread for the camera
  int camera_thread_shutdown;
  pthread_t camera_thread;
  int camera_connected;
  //The image number
  int image_number;
  //Are we grabbing images ?
  int grab_images;
  //Exposure information
  int exp_time; //time in milliseconds
  int exp_gain; //camera gain
  //Hardware region of interest information
  int roi_hard_active;
  int roi_hard_x;
  int roi_hard_width;
  int roi_hard_y;
  int roi_hard_height;
  //The GUI objects
  gui_objects_t *objects;
}camera_parameters_t;


void *camera_thread_func(void* arg);


#endif
