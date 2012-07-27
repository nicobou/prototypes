#include <glib-object.h>
#include <unistd.h>

#define TEST_POINTER   ((gpointer) 0x49)

static volatile gboolean stopping = FALSE;


gboolean
invoke_closure(GClosure *closure)
{
  GValue params[3] = { G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;
      
  g_value_init (&result_value, G_TYPE_BOOLEAN);
      

  g_value_init (&params[0], G_TYPE_UINT); 
  g_value_set_uint (&params[0], (gint)333); 
      
  g_value_init (&params[1], G_TYPE_INT);  
  g_value_set_int (&params[1], (gint)444);  
      
  g_value_init (&params[2], G_TYPE_INT);  
  g_value_set_int (&params[2], (gint)555);  
      
      
  g_closure_invoke (closure, &result_value, 3, params, NULL); 


  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]); 
  g_value_unset (&params[1]); 
  g_value_unset (&params[2]); 
  if (result) 
    g_print ("result true\n");
  else
    g_print ("result false\n");
}



static gpointer 
thread1_main (gpointer data) 
{ 
  GClosure *closure = data; 
  while (!stopping) 
    { 
      invoke_closure (closure); 
    } 
  return NULL; 
} 

static gpointer 
thread2_main (gpointer data) 
{ 
  GClosure *closure = data; 
  while (!stopping) 
    { 
      invoke_closure (closure); 
    } 
  return NULL; 
} 

gboolean 
test_signal_handler (guint a, gint b, gint c, gpointer data) 
{ 
   
  g_print ("test_signal_handler %d, %d, %d, %p\n", a, b, c, data);
    
  return TRUE;
} 

static void
destroy_data (gpointer  data,
              GClosure *closure)
{
  g_print ("destroy data\n");
}

int
main (int    argc,
      char **argv)
{
  GThread *thread1, *thread2;
  GClosure *closure;
  guint i;

  g_print ("START: %s\n", argv[0]);
  //g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | g_log_set_always_fatal (G_LOG_FATAL_MASK));
  g_type_init ();

  closure = g_cclosure_new (G_CALLBACK (test_signal_handler), TEST_POINTER, destroy_data);
  g_closure_set_marshal  (closure,g_cclosure_marshal_generic);

  stopping = FALSE;

  thread1 = g_thread_create (thread1_main, closure, TRUE, NULL); 
  thread2 = g_thread_create (thread2_main, closure, TRUE, NULL); 

  for (i = 0; i < 3; i++)
    {
      invoke_closure (closure); 
    }

  stopping = TRUE;
  g_print ("\nstopping\n");

  /* wait for thread shutdown */
  g_thread_join (thread1); 
  g_thread_join (thread2); 


  g_print ("stopped\n");

  return 0;
}
