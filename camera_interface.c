/*
 * Program to control the GigE camera, interface part
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

/* For later




 */


 
#include <gtk/gtk.h>
#include <errno.h>
#include "camera.h"

camera_parameters_t camera_params={
  .camera_thread_shutdown = 0,
  .camera_connected = 0,
  .image_number = 0,
  .grab_images = 0,
  .exp_time = 1,
  .exp_gain = 1,
  .roi_hard_active = 0,
};

int
main( int    argc,
      char **argv )
{
    GtkBuilder *builder;
    GtkWidget  *window;
    GError     *error = NULL;
    int ret;
    /* Init GTK+ */
    gtk_init( &argc, &argv );
 
    /* Create new GtkBuilder object */
    builder = gtk_builder_new();
    /* Load UI from file. If error occurs, report it and quit application.
     * Replace "tut.glade" with your saved project. */
    if( ! gtk_builder_add_from_file( builder, "camera_interface.glade", &error ) )
    {
        g_warning( "%s", error->message );
        g_free( error );
        return( 1 );
    }
 
    /* Get main window pointer from UI */
    window = GTK_WIDGET( gtk_builder_get_object( builder, "window1" ) );
 
    /* Connect signals */
    gtk_builder_connect_signals( builder, NULL );
 
    /* Destroy builder, since we don't need it anymore */
    g_object_unref( G_OBJECT( builder ) );
 
    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( window );

    /* Start the camera thread */
    ret=pthread_create(&(camera_params.camera_thread), NULL, camera_thread_func, &camera_params);
    if(ret){
        g_warning( "Thread creation error %d", ret );
        return( 1 ); 
    }
 
    /* Start main loop */
    gtk_main();

    return( 0 );
}


static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
  /* If you return FALSE in the "delete-event" signal handler,
   * GTK will emit the "destroy" signal. Returning TRUE means
   * you don't want the window to be destroyed.
   * This is useful for popping up 'are you sure you want to quit?'
   * type dialogs. */

  g_print ("delete event occurred\n");
   

  /* Change TRUE to FALSE and the main window will be destroyed with
   * a "delete-event". */

  return TRUE;
}


/* Another callback */
static void destroy( GtkWidget *widget,
                     gpointer   data )
{

  //We kill the camera thread
  camera_params.camera_thread_shutdown=1;
  pthread_join(camera_params.camera_thread, NULL);
 
  gtk_main_quit ();
}
