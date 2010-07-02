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


 
#include <errno.h>
#include "camera.h"

camera_parameters_t camera_params={
  .image_number = 0,
  .camera_thread_shutdown = 0,
  .camera_connected = 0,
  .image_number = 0,
  .grab_images = 0,
  .exp_time = 1,
  .exp_gain = 1,
  .roi_hard_active = 0,
};

// MAIN IS AT THE END

G_MODULE_EXPORT gboolean cb_delete_event( GtkWidget *widget,
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

  return FALSE;
}


/* Another callback */
G_MODULE_EXPORT void cb_destroy( GtkWidget *widget,
                     gpointer   data )
{

  g_print ("destroy event occurred shutting down the thread\n");
  //We kill the camera thread
  camera_params.camera_thread_shutdown=1;
  pthread_join(camera_params.camera_thread, NULL);
 
  gtk_main_quit ();
}


//Callback for the ROI values changed
G_MODULE_EXPORT void cb_ROI_changed( GtkEditable *editable, gpointer   data )
{

  camera_params.roi_hard_x=(int)gtk_adjustment_get_value(camera_params.objects->ROI_adjust_x);
  camera_params.roi_hard_y=(int)gtk_adjustment_get_value(camera_params.objects->ROI_adjust_y);
  camera_params.roi_hard_width=(int)gtk_adjustment_get_value(camera_params.objects->ROI_adjust_width);
  camera_params.roi_hard_height=(int)gtk_adjustment_get_value(camera_params.objects->ROI_adjust_height);
  if(camera_params.roi_hard_active)
    camera_update_roi(&camera_params);
}

//Callback for the ROI button is toggled
G_MODULE_EXPORT void cb_ROI_toggled(GtkToggleButton *togglebutton,gpointer   data )
{

  if(gtk_toggle_button_get_active(togglebutton)==TRUE)
    camera_params.roi_hard_active=1;
  else
    camera_params.roi_hard_active=0;

  camera_update_roi(&camera_params);
}


//Callback for the bytespersecond values changed
G_MODULE_EXPORT void cb_BytesPerSecond_changed( GtkEditable *editable, gpointer   data )
{
  //We set the new value on the camera
  PvAttrUint32Set(camera_params.camera_handler,"StreamBytesPerSecond",(int)gtk_adjustment_get_value(camera_params.objects->Bytes_per_sec_adj));
}


//Callback for the bytespersecond values changed
G_MODULE_EXPORT void cb_Binning_changed( GtkEditable *editable, gpointer   data )
{

  camera_params.binning_x=(int)gtk_adjustment_get_value(camera_params.objects->Bin_X_adj);
  camera_params.binning_y=(int)gtk_adjustment_get_value(camera_params.objects->Bin_Y_adj);
  PvAttrUint32Set(camera_params.camera_handler,"BinningX",camera_params.binning_x);
  PvAttrUint32Set(camera_params.camera_handler,"BinningY",camera_params.binning_y);
}

//Callback for the bytespersecond values changed
G_MODULE_EXPORT void cb_Eposure_changed( GtkEditable *editable, gpointer   data )
{
  camera_set_exposure(&camera_params);
}

//Callback for the ROI button is toggled
G_MODULE_EXPORT void cb_Acquire_toggled(GtkToggleButton *togglebutton,gpointer   data )
{

  g_print("cb_Acquire_toggled\n");

  if(gtk_toggle_button_get_active(togglebutton)==TRUE)
    camera_params.grab_images=1;
  else
    camera_params.grab_images=0;

  

}


//Callback when the user click on the image
G_MODULE_EXPORT void cb_Image_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  g_print("Th\n");
  if(event->type==GDK_BUTTON_PRESS)
  {
    g_print("The image has been clicked at %f %f\n",event->x,event->y);
  }

}

//Callback when the user click on the image
G_MODULE_EXPORT void cb_Image_button_released(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  g_print("Th\n");
  if(event->type==GDK_BUTTON_RELEASE)
  {
    g_print("The image has been released at %f %f\n",event->x,event->y);
  }

}


int
main( int    argc,
      char **argv )
{
    GtkBuilder *builder;
    GError     *error = NULL;
    int ret;

    /* We are using a threaded program we must say it to gtk */
    if( ! g_thread_supported() )
      g_thread_init(NULL);
    gdk_threads_init();

    /* Obtain gtk's global lock */
    gdk_threads_enter();

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
 
    /* Allocate data structure */
    camera_params.objects = g_slice_new( gui_objects_t );

 
    /* Get objects from UI */
#define GW( name ) CH_GET_WIDGET( builder, name, data )
    GW( main_window );
    GW( main_status_bar );
    GW( camera_text );
    GW( image_number );
    GW( raw_image );
#undef GW
    /* Get adjustments objects from UI */
#define GA( name ) CH_GET_ADJUSTMENT( builder, name, data )
    GA( Exp_adj_gain );
    GA( Exp_adj_time );
    GA( ROI_adjust_x );
    GA( ROI_adjust_y );
    GA( ROI_adjust_width );
    GA( ROI_adjust_height );
    GA( Bin_X_adj );
    GA( Bin_Y_adj );
    GA( Bytes_per_sec_adj );
#undef GA


    /* Connect signals */
    gtk_builder_connect_signals( builder, NULL );
 
    /* Destroy builder, since we don't need it anymore */
    g_object_unref( G_OBJECT( builder ) );
 
    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( camera_params.objects->main_window );

    /* Start the camera thread */
    ret=pthread_create(&(camera_params.camera_thread), NULL, camera_thread_func, &camera_params);
    if(ret){
        g_warning( "Thread creation error %d", ret );
        return( 1 ); 
    }
 
    /* Start main loop */
    gtk_main();

    /* Release gtk's global lock */
    gdk_threads_leave();

    /* We derease the pixbuf reference counter */
    if(camera_params.raw_image_pixbuff!=NULL)
      g_object_unref(camera_params.raw_image_pixbuff);

    /* Free any allocated data */
    g_slice_free( gui_objects_t, camera_params.objects );


    return( 0 );
}
