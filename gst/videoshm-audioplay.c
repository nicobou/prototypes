#include <gst/gst.h>
#include <signal.h>
#include <shmdata/base-writer.h>

GstElement *pipeline;
GstPad *main_pad = NULL;

void
leave(int sig) {
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);
    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));
    exit(sig);
}

static gboolean
bus_call (GstBus *bus,
          GstMessage *msg,
          gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
	g_print ("End of stream\n");
	/* if (!gst_element_seek(pipeline,  */
	/* 		      1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, */
	/* 		      GST_SEEK_TYPE_SET, 0, */
	/* 		      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))  */
	/*   g_print("Seek failed!\n"); */
	   g_main_loop_quit (loop); 
	break;

    case GST_MESSAGE_ERROR: {
	gchar *debug;
	GError *error;
      
	gst_message_parse_error (msg, &error, &debug);
	g_free (debug);
      
	g_printerr ("Error: %s\n", error->message);
	g_error_free (error);
      
	g_main_loop_quit (loop);
	break;
    }
    default:
	//g_print ("unknown message type \n");
	break;
    }
    return TRUE;
}

gboolean 
my_rewind (gpointer user_data)
{
  gst_element_seek (pipeline,
		    1.0,  
		    GST_FORMAT_TIME,  
		    (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), 
		    GST_SEEK_TYPE_SET,  
		    0.0 * GST_SECOND,  
		    GST_SEEK_TYPE_NONE,  
		    GST_CLOCK_TIME_NONE);  
  g_print ("loop !");
  return FALSE;
}


gboolean
event_probe_cb (GstPad *pad, GstEvent * event, gpointer user_data)
{
  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) 
    { 
      if (pad == main_pad)
	g_idle_add ((GSourceFunc) my_rewind, NULL);   
      return FALSE;
    }  

  if (GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_START || 
      GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_STOP /*||
						       GST_EVENT_TYPE (event) == GST_EVENT_LATENCY */) 
    {
      return FALSE;
    }

    return TRUE; 
  }

void
install_looping (const char*element_name)
{
  GstElement *v = gst_bin_get_by_name (GST_BIN(pipeline), element_name);
  GstPad *pad = gst_element_get_static_pad (v,"sink");
  if (main_pad == NULL)
    main_pad = pad;
  gst_pad_add_event_probe (pad, (GCallback) event_probe_cb, NULL);   
  gst_object_unref (pad);
}

void
install_shmdata (const char *element_name)
{
  GstElement *tee = gst_bin_get_by_name (GST_BIN(pipeline), element_name);
  shmdata_base_writer_t *writer = shmdata_base_writer_init ();
  gchar *path = g_strdup_printf ("/tmp/%s",element_name);
  if(shmdata_base_writer_set_path (writer,path) == SHMDATA_FILE_EXISTS)
    g_printerr ("**** The file exists, therefore a shmdata cannot be operated with this path.\n");	
  
  shmdata_base_writer_plug (writer, pipeline, tee);
  
  g_free (path);
  //g_print ("Now writing to the shared memory\n");
}

int
main (int argc,
      char *argv[])
{
    (void) signal(SIGINT,leave);
    
    if (argc != 2)
      {
	g_printerr ("usage : %s <dir_path>\n",argv[0]);
	return -1;
      }

    if (g_access (argv[1], R_OK) != 0)
      {
	g_printerr ("directory %s not readable\n", argv[1]); 
	return -1;
      }

    /* Initialisation */
    gst_init (&argc, &argv);

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    /* Create gstreamer elements */
    pipeline  = gst_pipeline_new (NULL);

    if (!pipeline) {
	g_printerr ("The pipeline could not be created. Exiting.\n");
	return -1;
    }


    GFile *dir = g_file_parse_name (argv[1]);
    char *uri = g_file_get_path (dir);
    g_print ("%s\n", uri);
    
    /* GstElement *fakesrc = gst_element_factory_make ("fakesrc",NULL); */
    /* GstElement *fakesink = gst_element_factory_make ("fakesink", NULL); */
    /* gst_bin_add_many (GST_BIN (pipeline), */
    /* 		      fakesrc, */
    /* 		      fakesink,	    */
    /* 		      NULL); */

    /* gst_element_link_many (fakesrc, */
    /* 			   fakesink,	    */
    /* 			   NULL); */
    
    


    GError *error = NULL;
    const char *pipe = g_strdup_printf ("uridecodebin  uri=file://%s/Lucy_0.mov ! \
                        ffmpegcolorspace name=v1 ! identity sync=true ! tee name=switcher_default_Lucy_0_video_0 ! fakesink \
                        uridecodebin  uri=file://%s/Lucy_90.mov ! \
                        ffmpegcolorspace name=v2 ! identity sync=true ! tee name=switcher_default_Lucy_90_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Lucy_180.mov ! \
                        ffmpegcolorspace name=v3 ! identity sync=true ! tee name=switcher_default_Lucy_180_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Lucy_270.mov ! \
                        ffmpegcolorspace name=v4 ! identity sync=true ! tee name=switcher_default_Lucy_270_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Manu_0.mov ! \
                        ffmpegcolorspace name=v5 ! identity sync=true ! tee name=switcher_default_Manu_0_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Manu_90.mov ! \
                        ffmpegcolorspace name=v6 ! identity sync=true ! tee name=switcher_default_Manu_90_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Manu_180.mov ! \
                        ffmpegcolorspace name=v7 ! identity sync=true ! tee name=switcher_default_Manu_180_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Manu_270.mov ! \
                        ffmpegcolorspace name=v8 ! identity sync=true ! tee name=switcher_default_Manu_270_video_0 ! fakesink\
                        uridecodebin  uri=file://%s/Manu_hardpan.wav ! \
                        identity name=a1 ! pulsesink \
                        uridecodebin  uri=file://%s/Lucy_hardpan.wav ! \
                        identity name=a2 ! pulsesink",
					uri, uri, uri, uri, uri, uri, uri, uri, uri, uri
					); 

    GstElement *gst_parse_to_bin_src_ = gst_parse_bin_from_description (pipe,
									 FALSE,
									&error);
    g_object_set (G_OBJECT (gst_parse_to_bin_src_), "async-handling",TRUE, NULL);
    if (error != NULL)
      {
	g_warning ("%s",error->message);
	g_error_free (error);
	return 1;
      }
    
    gst_bin_add (GST_BIN (pipeline), gst_parse_to_bin_src_);

     /* GstElement *queue =  gst_element_factory_make ("queue",NULL);  */
     /* GstElement *fakesink = gst_element_factory_make ("xvimagesink",NULL);   */
     /* gst_bin_add_many (GST_BIN (pipeline),   */
     /* 		      fakesink,   */
     /* 		      queue, NULL);   */
     /* gst_element_link_many (tee, queue,   */
     /* 			   fakesink,   */
     /* 			   NULL);  */
 

     install_looping ("v1"); 
     install_looping ("v2"); 
     install_looping ("v3"); 
     install_looping ("v4"); 
     install_looping ("v5"); 
     install_looping ("v6"); 
     install_looping ("v7"); 
     install_looping ("v8"); 
     install_looping ("a1"); 
     install_looping ("a2"); 
    
    install_shmdata ("switcher_default_Lucy_0_video_0");
    install_shmdata ("switcher_default_Lucy_90_video_0");
    install_shmdata ("switcher_default_Lucy_180_video_0");
    install_shmdata ("switcher_default_Lucy_270_video_0");
    install_shmdata ("switcher_default_Manu_0_video_0");
    install_shmdata ("switcher_default_Manu_90_video_0");
    install_shmdata ("switcher_default_Manu_180_video_0");
    install_shmdata ("switcher_default_Manu_270_video_0");

    /* message handler */
    GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* Set the pipeline to "playing" state*/
    g_print ("Now playing\n");
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Iterate */
    g_print ("Running...\n");
    g_main_loop_run (loop);

    /* Out of the main loop, clean up nicely */
    g_print ("Returned, stopping playback\n");
    gst_element_set_state (pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (pipeline));

    return 0;
}
