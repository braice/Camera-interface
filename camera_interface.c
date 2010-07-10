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
#include <string.h>
#include "camera.h"

//http://sourceforge.net/projects/gapcmon/files/


camera_parameters_t camera_params={
  .image_number = 0,
  .camera_thread_shutdown = 0,
  .camera_connected = 0,
  .grab_images = 0,
  .exp_time = 1,
  .exp_gain = 1,
  .roi_hard_active = 0,
  .roi_hard_clicking = ROI_CLICK_NONE,
  .roi_hard_X_lastimage = 0,
  .roi_hard_Y_lastimage = 0,
  .autoscroll_chart = 0,
  .list_store_iter_ok = 0,
  .background_set = 0,
};

// MAIN IS AT THE END

void add_to_statusbar(camera_parameters_t *camera_params, int enter_threads, const char *psz_format, ...)
{
  va_list args;
  gchar *msg;
  va_start( args, psz_format );
  if(enter_threads)
    gdk_threads_enter();
  gtk_statusbar_pop (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0);
  msg =   g_strdup_vprintf(psz_format,args);
  gtk_statusbar_push (GTK_STATUSBAR(camera_params->objects->main_status_bar), 0, msg);
  g_free (msg);
  if(enter_threads)
    gdk_threads_leave();
  va_end( args );

}






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

//Callback for the ROI reset is clicked
G_MODULE_EXPORT void cb_ROI_reset_clicked(GtkButton *button)
{
  camera_reset_roi(&camera_params);
  camera_update_roi(&camera_params);
}



G_MODULE_EXPORT void cb_select_ROI_clicked(GtkButton *button)
{
  //message if we are already using a ROI
  //  if(camera_params.roi_hard_active==1)
  //;
  //else
    camera_params.roi_hard_clicking=ROI_CLICK_CORNER1;
}

//Callback when the user click on the image
G_MODULE_EXPORT void cb_Image_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  if(event->type==GDK_BUTTON_PRESS)
  {
    if(camera_params.roi_hard_clicking==ROI_CLICK_CORNER1)
    {
      camera_params.roi_hard_corners[0][0]=(int)event->x;
      camera_params.roi_hard_corners[0][1]=(int)event->y;
      camera_params.roi_hard_clicking=ROI_CLICK_CORNER2;
    }
    else if(camera_params.roi_hard_clicking==ROI_CLICK_CORNER2)
    {
      if(((int)event->x) < (camera_params.roi_hard_corners[0][0]))
      {
	camera_params.roi_hard_corners[1][0]=camera_params.roi_hard_corners[0][0];
	camera_params.roi_hard_corners[0][0]=(int)event->x;
      }
      else
	camera_params.roi_hard_corners[1][0]=(int)event->x;
      if(((int)event->y) < (camera_params.roi_hard_corners[0][1]))
      {
	camera_params.roi_hard_corners[1][1]=camera_params.roi_hard_corners[0][1];
	camera_params.roi_hard_corners[0][1]=(int)event->y;
      }
      else
	camera_params.roi_hard_corners[1][1]=(int)event->y;
      camera_params.roi_hard_clicking=ROI_CLICK_NONE;
      gtk_widget_show( camera_params.objects->ROI_confirm_dialog );
      gchar *msg;
      //if there is already an ROI used, we have to sum the values with the current camera ROI 
      if((camera_params.image_number>0)&&((camera_params.roi_hard_X_lastimage !=0) || (camera_params.roi_hard_Y_lastimage != 0)))
	{
	  camera_params.roi_hard_corners[0][0]+=camera_params.roi_hard_X_lastimage;
	  camera_params.roi_hard_corners[1][0]+=camera_params.roi_hard_X_lastimage;
	  camera_params.roi_hard_corners[0][1]+=camera_params.roi_hard_Y_lastimage;
	  camera_params.roi_hard_corners[1][1]+=camera_params.roi_hard_Y_lastimage;
	}
      msg = g_strdup_printf ("Start X: %d, Width: %d\nStart Y: %d, Height: %d",
			     camera_params.roi_hard_corners[0][0],
			     camera_params.roi_hard_corners[1][0]-camera_params.roi_hard_corners[0][0],
			     camera_params.roi_hard_corners[0][1],
			     camera_params.roi_hard_corners[1][1]-camera_params.roi_hard_corners[0][1]);
      gtk_text_buffer_set_text(gtk_text_view_get_buffer (GTK_TEXT_VIEW (camera_params.objects->ROI_confirm_dialog_text)),msg,-1);
      g_free (msg);
    }
  }
}

//Callbak when the user confirmed the selected ROI
G_MODULE_EXPORT void cb_dialog_valid_ROI_clicked(GtkButton *button)
{
  gtk_adjustment_set_value(camera_params.objects->ROI_adjust_x,camera_params.roi_hard_corners[0][0]);
  gtk_adjustment_set_value(camera_params.objects->ROI_adjust_width,camera_params.roi_hard_corners[1][0]-camera_params.roi_hard_corners[0][0]);
  gtk_adjustment_set_value(camera_params.objects->ROI_adjust_y,camera_params.roi_hard_corners[0][1]);
  gtk_adjustment_set_value(camera_params.objects->ROI_adjust_height,camera_params.roi_hard_corners[1][1]-camera_params.roi_hard_corners[0][1]);
  if(camera_params.roi_hard_active)
    camera_update_roi(&camera_params);

  gtk_widget_hide( camera_params.objects->ROI_confirm_dialog );
}

G_MODULE_EXPORT void cb_dialog_cancel_ROI_clicked(GtkButton *button)
{
  gtk_widget_hide( camera_params.objects->ROI_confirm_dialog );
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

//Callback for the acquisition button is toggled
G_MODULE_EXPORT void cb_Acquire_toggled(GtkToggleButton *togglebutton,gpointer   data )
{

  if(gtk_toggle_button_get_active(togglebutton)==TRUE)
    camera_params.grab_images=1;
  else
    camera_params.grab_images=0;

}

//Triggering callbacks
G_MODULE_EXPORT void cb_trig_toggled(GtkToggleButton *togglebutton,gpointer   data )
{
  camera_set_triggering(&camera_params);
}

G_MODULE_EXPORT void cb_trig_changed( GtkEditable *editable, gpointer   data )
{
  camera_set_triggering(&camera_params);
}


//Callback for the autoscroll button is toggled
G_MODULE_EXPORT void cb_autoscroll_toggled(GtkToggleButton *togglebutton,gpointer   data )
{

  if(gtk_toggle_button_get_active(togglebutton)==TRUE)
    camera_params.autoscroll_chart=1;
  else
    camera_params.autoscroll_chart=0;
}


//Saving the actual image
G_MODULE_EXPORT void cb_save_clicked(GtkButton *button)
{
  //Check if there is an image
  if(camera_params.image_number==0)
    gtk_widget_show( camera_params.objects->no_image_dialog );
  else
    gtk_widget_show( camera_params.objects->imagesavedialog );

}

G_MODULE_EXPORT void cb_noimg_dialog_close(GtkButton *button)
{
  gtk_widget_hide( camera_params.objects->no_image_dialog );
}

G_MODULE_EXPORT void cb_image_save_cancel_clicked(GtkButton *button)
{
  //destroymagickwand
  gtk_widget_hide( camera_params.objects->imagesavedialog );
}

G_MODULE_EXPORT void cb_image_save_ok_clicked(GtkButton *button)
{
  char *filename;
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (camera_params.objects->imagesavedialog));
  g_print("Filename %s\n",filename);
  g_free (filename);
  //CloneMagickWand
  //MagickLinearStretchImage(camera_params->wand_data.magick_wand,0,1<<((int)camera_params.sensorbits));
  //MagickSetImageFormat
  //MagickWriteImage
  //http://library.gnome.org/devel/gdk-pixbuf/unstable/gdk-pixbuf3-file-saving.html#gdk-pixbuf-save
  // MagickSetImageCompression
  // MagickSetImageFormat
  // MagickWriteImage
  gtk_widget_hide( camera_params.objects->imagesavedialog );
}


//Directory chooser
G_MODULE_EXPORT void cb_choose_dir_clicked(GtkButton *button)
{
  gtk_widget_show( camera_params.objects->directorychooserdialog );
}

G_MODULE_EXPORT void cb_directory_chooser_cancel_clicked(GtkButton *button)
{
  gtk_widget_hide( camera_params.objects->directorychooserdialog );
}

G_MODULE_EXPORT void cb_directory_chooser_open_clicked(GtkButton *button)
{
  char *filename;
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (camera_params.objects->directorychooserdialog));
  g_print("Directory %s\n",filename);
  g_free (filename);

  gtk_widget_hide( camera_params.objects->directorychooserdialog );
}


//List store reset
G_MODULE_EXPORT void cb_list_reset_clicked(GtkButton *button)
{
  gtk_list_store_clear ( camera_params.objects->statistics_list );
  camera_params.list_store_iter_ok=0;
}


//Saving the actual list
G_MODULE_EXPORT void cb_save_list_clicked(GtkButton *button)
{
  gtk_widget_show( camera_params.objects->listsavedialog );

}

G_MODULE_EXPORT void cb_list_save_cancel_clicked(GtkButton *button)
{
  gtk_widget_hide( camera_params.objects->listsavedialog );
}

//Saving the list to a file
G_MODULE_EXPORT void cb_list_save_ok_clicked(GtkButton *button)
{
  char *filename;
  FILE *file;
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (camera_params.objects->listsavedialog));
  g_print("Filename %s\n",filename);
  GtkTreeIter iter;
  //Check if the list is empty
  if(gtk_tree_model_get_iter_first (GTK_TREE_MODEL(camera_params.objects->statistics_list),&iter)==TRUE)
  {

    //we get the data from the list
    gdouble time,mean, mean_soft, mean_soft_roi1, mean_soft_roi2;
    gint image_number;
    int row=0;
    file = fopen (filename, "w");
    if (file == NULL)
    {
      gtk_widget_hide( camera_params.objects->listsavedialog );
      add_to_statusbar(&camera_params, 0, "Error openning %s : %s", filename,strerror (errno));
      g_free (filename);
      return;
    }

    fprintf(file,"#image_number\ttime\tmean\tmean_soft\tmean_soft_roi1\tmean_soft_roi2\n");
    do{
      gtk_tree_model_get (GTK_TREE_MODEL(camera_params.objects->statistics_list),&iter,
			  0, &image_number,
			  1, &time,
			  2, &mean,
			  3, &mean_soft,
			  4, &mean_soft_roi1,
			  4, &mean_soft_roi2,
			  -1);
      fprintf(file,"%d\t%f\t%f\t%f\t%f\t%f\n",image_number, time, mean, mean_soft, mean_soft_roi1, mean_soft_roi2);
      row++;
    }while(TRUE==gtk_tree_model_iter_next (GTK_TREE_MODEL(camera_params.objects->statistics_list),&iter));
    fclose (file);
  }
  else
  {
      add_to_statusbar(&camera_params, 0, "Empty list, cannot save");
  }
  g_free (filename);
  gtk_widget_hide( camera_params.objects->listsavedialog );
}

/***************** Software image processing ***********/

G_MODULE_EXPORT void cb_soft_level_reset_clicked(GtkButton *button)
{
  gtk_adjustment_set_value(camera_params.objects->soft_level_max_adj,1<<((int)camera_params.sensorbits));
  gtk_adjustment_set_value(camera_params.objects->soft_level_min_adj,0);
  update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_brightcont_reset_clicked(GtkButton *button)
{
  gtk_adjustment_set_value(camera_params.objects->soft_brightness_adj,0);
  gtk_adjustment_set_value(camera_params.objects->soft_contrast_adj,0);
  update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_editable_changed( GtkEditable *editable, gpointer   data )
{
    update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_ROI_editable_changed( GtkEditable *editable, gpointer   data )
{
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params.objects->soft_cut_img))==TRUE)
    update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_ROI_mean1_editable_changed( GtkEditable *editable, gpointer   data )
{
  if((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params.objects->soft_display_mean_roi1))==TRUE)||
     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params.objects->soft_compute_mean_roi1))==TRUE))
    update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_ROI_mean2_editable_changed( GtkEditable *editable, gpointer   data )
{
  if((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params.objects->soft_display_mean_roi2))==TRUE)||
     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(camera_params.objects->soft_compute_mean_roi2))==TRUE))
    update_soft_val(&camera_params);
}

G_MODULE_EXPORT void cb_soft_togglebutton_toggled(GtkToggleButton *togglebutton,gpointer   data )
{
    update_soft_val(&camera_params);
}

/*** Background ****/

G_MODULE_EXPORT void  cb_soft_set_bg_clicked(GtkButton *button)
{
  if(camera_params.image_number>0)
    imagemagick_set_bg(&camera_params);
}

int
main( int    argc,
      char **argv )
{
    GtkBuilder *builder;
    GError     *error = NULL;
    int ret;

    /* MagickWand Init */
    MagickWandGenesis();
    camera_params.wand_data.display_magick_wand=NewMagickWand();
    camera_params.wand_data.processed_magick_wand=NewMagickWand();
    camera_params.wand_data.raw_magick_wand=NewMagickWand();

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
    GW( processed_image );
    GW( ext_trig );
    GW( trig_mult );
    GW( trig_single );
    GW( trig_cont );
    GW( freerun_trig );
    GW( ext1_trig );
    GW( ext2_trig );
    GW( framerate_trig );
    GW( directorychooserdialog );
    GW( imagesavedialog );
    GW( listsavedialog );
    GW( no_image_dialog );
    GW( ROI_confirm_dialog );
    GW( ROI_confirm_dialog_text );
    GW( stats_treeview );
    GW( acq_toggle );
    GW( mean_bar );
    GW( soft_rotate_image );
    GW( soft_autolevel );
    GW( soft_autogamma );
    GW( soft_magn_x2 );
    GW( soft_magn_x4 );
    GW( soft_magn_x8 );
    GW( soft_cut_img );
    GW( soft_image_text );
    GW( soft_display_mean_roi1 );
    GW( soft_compute_mean_roi1 );
    GW( soft_display_mean_roi2 );
    GW( soft_compute_mean_roi2 );
    GW( soft_background_info_text );
    GW( soft_background_button );
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
    GA( Trig_nbframes_adj);
    GA( Trig_framerate_adj);
    GA( min_meanbar );
    GA( max_meanbar );
    GA( soft_brightness_adj );
    GA( soft_contrast_adj );
    GA( soft_angle_adj );
    GA( soft_level_min_adj );
    GA( soft_level_max_adj );
    GA( ROI_soft_adjust_width );
    GA( ROI_soft_adjust_height );
    GA( ROI_soft_adjust_x );
    GA( ROI_soft_adjust_y );
    GA( ROI_soft_mean_adjust_x1 );
    GA( ROI_soft_mean_adjust_width1 );
    GA( ROI_soft_mean_adjust_y1 );
    GA( ROI_soft_mean_adjust_height1 );
    GA( ROI_soft_mean_adjust_x2 );
    GA( ROI_soft_mean_adjust_width2 );
    GA( ROI_soft_mean_adjust_y2 );
    GA( ROI_soft_mean_adjust_height2 );
#undef GA
#define GL( name ) CH_GET_LIST_STORE( builder, name, data )
    GL( statistics_list );
#undef GL

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


    if(camera_params.background_set)
      DestroyMagickWand(camera_params.wand_data.background_wand);
    DestroyMagickWand(camera_params.wand_data.raw_magick_wand);
    DestroyMagickWand(camera_params.wand_data.processed_magick_wand);
    DestroyMagickWand(camera_params.wand_data.display_magick_wand);
    MagickWandTerminus();

    return( 0 );
}
