#include <gio/gio.h>

//http://developer.gnome.org/gio/2.26/GFile.html#g-file-monitor-directory


static void
file_system_monitor_change (GFileMonitor *      monitor,
			    GFile *             file,
			    GFile *             other_file,
			    GFileMonitorEvent   type,
			    gpointer userdata)
{

    char *filename = g_file_get_path (file);
    
  switch (type)
    {
      case G_FILE_MONITOR_EVENT_CREATED:
	  g_print ("G_FILE_MONITOR_EVENT_CREATED: %s\n",filename);
	  break;
      case G_FILE_MONITOR_EVENT_DELETED:
	  g_print ("G_FILE_MONITOR_EVENT_DELETED: %s\n",filename);
	  break;
      /* case G_FILE_MONITOR_EVENT_CHANGED: */
      /* 	  g_print ("G_FILE_MONITOR_EVENT_CHANGED\n"); */
      /* 	  break; */
      /* case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: */
      /* 	  g_print ("G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED\n"); */
      /* 	  break; */
      /* case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: */
      /* 	  g_print ("G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT\n"); */
      /* 	  break; */
      /* case G_FILE_MONITOR_EVENT_PRE_UNMOUNT: */
      /* 	  g_print ("G_FILE_MONITOR_EVENT_PRE_UNMOUNT\n"); */
      /* 	  break; */
      /* case G_FILE_MONITOR_EVENT_UNMOUNTED: */
      /* 	  g_print ("G_FILE_MONITOR_EVENT_UNMOUNTED\n"); */
      /* 	  break; */
      default:
        break;
    }
  
  g_free (filename);

}



int
main (int   argc,
      char *argv[])
{

    if (argc != 2) {
	g_printerr ("Usage: %s <file name>\n", argv[0]);
	return -1;
    }
    GMainLoop *loop;

    g_type_init();
    loop = g_main_loop_new (NULL, FALSE);
    
    GFile *dir = g_file_new_for_path (argv[1]);
    
    GFileMonitor* dirMonitor = g_file_monitor_directory (dir,  
     							 G_FILE_MONITOR_NONE, 
     							 NULL,/*GCancellable*/ 
     							 NULL/*GError*/); 
    
    g_signal_connect (dirMonitor, 
     		      "changed", 
     		      G_CALLBACK (file_system_monitor_change), 
     		      NULL); 


    g_main_loop_run (loop);

    return 0;
}
