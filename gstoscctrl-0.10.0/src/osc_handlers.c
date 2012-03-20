/*
 * Copyright (C) 2011-2012 Nicolas Bouillot (http://www.nicolasbouillot.net)
 * Copyright (C) 2011      Marcio Tomiyoshi <mmt@ime.usp.br>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

#include "osc_handlers.h"

GST_DEBUG_CATEGORY_STATIC (gst_control_from_osc_handlers_debug);
#define GST_CAT_DEFAULT gst_control_from_osc_handlers_debug

/* osc handlers */

void
osc_error (int num, const char *msg, const char *path)
{
  GST_ERROR ("liblo server error '%d' in path '%s': '%s'\n", num, path, msg);
  fflush (stdout);
}

// Performs a seek
int
osc_locate_handler (const char *path, const char *types,
		    lo_arg ** argv, int argc, void *data, void *user_data)
{
  gdouble time = argv[0]->d;

  GST_LOG ("osc_locate_handler. Time '%f'\n", time);

  GstOscctrl *filter = GST_OSCCTRL (user_data);

  gint64 pos = GST_SECOND * time;
  gst_element_seek (GST_ELEMENT_PARENT (filter),
		    1.0,
		    GST_FORMAT_TIME,
		    GST_SEEK_FLAG_KEY_UNIT
		    | GST_SEEK_FLAG_SEGMENT
		    | GST_SEEK_FLAG_FLUSH,
		    GST_SEEK_TYPE_SET, pos, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

  return 0;
}

// Get the value of a property of an element.
int
osc_get_helper (GstOscctrl * filter,
		const gchar * name, const gchar * property,
		const gchar * address, const gchar * port)
{
  GstElement *target_elem;
  GParamSpec *elem_prop;

  GST_INFO("Looking for property '%s' on element '%s'...", property,	
	   name);							

  lo_address reply = lo_address_new (address, port);
  lo_message message = lo_message_new ();
									
  target_elem =								
    gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(filter)), name);     
									
  if (target_elem == NULL) {						
    GST_WARNING("Element '%s' not found!", name);                       
    lo_send (reply, "/get/reply", "sssss", "getting",
	     "error","no element with that name",name, property);
    return 1;
  }									
									
  elem_prop = g_object_class_find_property(				
					   G_OBJECT_GET_CLASS(G_OBJECT(target_elem)), 
					   property);			
									
  if (elem_prop == NULL) {						
    GST_WARNING("Property '%s' not found!", property);                  
    lo_send (reply, "/get/reply", "sssss", "getting",
	     "error","no property with that name",name, property);
    return 1;

  }									
									
  GST_INFO("Element '%s' and property '%s' found!");			


  create_message (message, target_elem, elem_prop);

  GST_INFO ("Sending message '/get/reply' to '%s'", lo_address_get_url (reply));
  lo_send_message (reply, "/get/reply", message);

  lo_address_free (reply);

  return 0;
}

int
osc_get_handler (const char *path, const char *types,
		 lo_arg ** argv, int argc, void *data, void *user_data)
{
  GstOscctrl *filter = GST_OSCCTRL (user_data);
  const gchar *name = (const gchar *) argv[0];
  const gchar *property = (const gchar *) argv[1];
  const gchar *address = (const gchar *) argv[2];
  const gchar *port = (const gchar *) argv[3];

  GST_LOG
    ("osc_get_handler. Name: '%s' Property: '%s' Address: '%s' Port: '%s'",
     name, property, address, port);

  return osc_get_helper (filter, name, property, address, port);
}



// Pauses the pipeline
int
osc_pause_handler (const char *path, const char *types,
		   lo_arg ** argv, int argc, void *data, void *user_data)
{
  GST_LOG ("osc_pause_handler");

  GstOscctrl *filter = GST_OSCCTRL (user_data);

  gst_element_set_state ((GST_ELEMENT_PARENT (filter)), GST_STATE_PAUSED);

  return 0;
}

// Plays the pipeline
int
osc_play_handler (const char *path, const char *types,
		  lo_arg ** argv, int argc, void *data, void *user_data)
{
  GST_LOG ("osc_play_handler");

  GstOscctrl *filter = GST_OSCCTRL (user_data);

  gst_element_set_state ((GST_ELEMENT_PARENT (filter)), GST_STATE_PLAYING);

  return 0;
}

// Creates a message and send it to every element subscribed to this property.
void
send_reply (GObject * gobject, GParamSpec * pspec, gpointer user_data)
{
  GstOscctrl *filter = GST_OSCCTRL (user_data);

  gchar *name = gst_element_get_name (gobject);
  const gchar *property = g_param_spec_get_name (pspec);
  GstElement *target_elem = GST_ELEMENT (gobject);

  GST_LOG ("send_reply. Name: '%s' Property: '%s'", name, property);

  gchar *key;
  key = g_strconcat (name, " ", property, NULL);
  GST_INFO ("Looking for key '%s' on hash table", key);
  GList *next = g_hash_table_lookup (filter->subscribers, key);

  lo_message message = lo_message_new ();
  create_message (message, target_elem, pspec);

  while (next != NULL) {
    GST_INFO ("Sending message '/subscribe/reply' to '%s'",
	      lo_address_get_url (next->data));
    lo_send_message (next->data, "/subscribe/reply", message);
    next = g_list_next (next);
  }

  g_free (name);
  g_free (key);
}

// Subscribes an address to receive OSC messages every time a property is
// modified.
int
osc_subscribe_helper (GstOscctrl * filter,
		      const gchar * name, const gchar * property,
		      const gchar * address, const gchar * port)
{
  GstElement *target_elem;
  GParamSpec *elem_prop;
  
  GST_INFO("Looking for property '%s' on element '%s'...", property,	
	   name);							

  lo_address new_subscriber = lo_address_new (address, port);
    
  target_elem =								
    gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(filter)), name);     
  
  if (target_elem == NULL) {						
    GST_WARNING("Element '%s' not found!", name);                       
    lo_send (new_subscriber, "/subscribe/reply", "sssss", "subscribing",
	     "error","no element with that name",name, property);
    return 1;
  }									
  
  elem_prop = g_object_class_find_property(				
					   G_OBJECT_GET_CLASS(G_OBJECT(target_elem)), 
					   property);			
  
  if (elem_prop == NULL) {						
    GST_WARNING("Property '%s' not found!", property);                  
    lo_send (new_subscriber, "/subscribe/reply", "sssss", "subscribing",
	     "error","no property with that name",name, property);
    return 1;
  }									
  
  GST_INFO("Element '%s' and property '%s' found!");			
  

  gchar *key;
  key = g_strconcat (name, " ", property, NULL);
  gpointer value = NULL;
  GST_INFO ("Looking for key '%s' on hash table", key);
  gboolean key_found = g_hash_table_lookup_extended (filter->subscribers, key,
						     NULL, &value);
  if (key_found == FALSE) {
    GST_INFO ("New key '%s' added to hash table\n", key);
    gchar *signal;
    signal = g_strconcat ("notify::", property, NULL);
    GST_INFO ("Connecting '%s' signal '%s' to send_reply callback", name,
	      signal);
    g_signal_connect (target_elem, signal, G_CALLBACK (send_reply), filter);
    g_free (signal);
  }

  GST_INFO ("Adding '%s' to subscribers list and updating hash table",
	    lo_address_get_url (new_subscriber));
  value = g_list_prepend (value, new_subscriber);
  g_hash_table_insert (filter->subscribers, key, value);

  GST_INFO ("Sending message '/subscribe/reply' to '%s'",
	    lo_address_get_url (new_subscriber));
  lo_send (new_subscriber, "/subscribe/reply", "sss", "subscribed",
	   name, property);

  //now calling get to notify the subscriber 
  osc_get_helper (filter, name, property, address, port);


  return 0;
}

int
osc_subscribe_handler (const char *path, const char *types,
		       lo_arg ** argv, int argc, void *data, void *user_data)
{
  GstOscctrl *filter = GST_OSCCTRL (user_data);
  const gchar *name = (const gchar *) argv[0];
  const gchar *property = (const gchar *) argv[1];
  const gchar *address = (const gchar *) argv[2];
  const gchar *port = (const gchar *) argv[3];

  GST_LOG
    ("osc_subscribe_handler. Name: '%s' Property: '%s' Address: '%s' Port: '%s'",
     name, property, address, port);

  return osc_subscribe_helper (filter, name, property, address, port);
}

// Unsubscribes a computer from receiving updates.
int
osc_unsubscribe_helper (GstOscctrl * filter,
			const gchar * name, const gchar * property,
			const gchar * address, const gchar * port)
{
  gchar *key;
  key = g_strconcat (name, " ", property, NULL);
  lo_address unsubscriber = lo_address_new (address, port);

  GST_INFO ("Looking for key '%s' on hash table", key);
  GList *head = g_hash_table_lookup (filter->subscribers, key);

  GST_INFO ("Looking for '%s' in the subscribers list",
	    lo_address_get_url (unsubscriber));
  GList *element = g_list_find_custom (head, lo_address_get_url (unsubscriber),
				       compare_lo_address);

  if (element == NULL || head == NULL) {
    GST_WARNING ("'%s' is not subscribed to property '%s' from element '%s'.\n",
		 lo_address_get_url (unsubscriber), name, property);
    lo_send (unsubscriber, "/unsubscribe/reply", "sssss", "unsubscribing",
	     "warning", "not subscribed", name, property);
    return 1;
  }

  GST_INFO ("Deleting '%s' from subscribers list and updating hash table",
	    lo_address_get_url (unsubscriber));
  head = g_list_delete_link (head, element);
  g_hash_table_insert (filter->subscribers, key, head);

  GST_INFO ("Sending message '/unsubscribe/reply' to '%s'",
	    lo_address_get_url (unsubscriber));
  lo_send (unsubscriber, "/unsubscribe/reply", "sss", "unsubscribed",
	   name, property);
  lo_address_free (unsubscriber);

  return 0;
}

int
osc_unsubscribe_handler (const char *path, const char *types,
			 lo_arg ** argv, int argc, void *data, void *user_data)
{
  GstOscctrl *filter = GST_OSCCTRL (user_data);
  const gchar *name = (const gchar *) argv[0];
  const gchar *property = (const gchar *) argv[1];
  const gchar *address = (const gchar *) argv[2];
  const gchar *port = (const gchar *) argv[3];

  GST_LOG ("osc_unsubscribe_handler. Name: '%s' Property: '%s' Address: '%s' \
            Port: '%s'", name, property, address, port);

  return osc_unsubscribe_helper (filter, name, property, address, port);
}


// Sets a property of a certain element.
int
osc_set_helper (GstOscctrl * filter,
		const gchar * name, const gchar * property,
		lo_arg * value, const gchar type)
{
  GstElement *target_elem;
  GParamSpec *elem_prop;

  GST_INFO("Looking for property '%s' on element '%s'...", property,	
	   name);							
									
  target_elem =								
    gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(filter)), name);     
									
  if (target_elem == NULL) {						
    GST_WARNING("Element '%s' not found!", name);   
    return 1;
  }									
									
  elem_prop = g_object_class_find_property(				
					   G_OBJECT_GET_CLASS(G_OBJECT(target_elem)), 
					   property);			
									
  if (elem_prop == NULL) {						
    GST_WARNING("Property '%s' not found!", property);                  
    return 1;
  }									
									
  GST_INFO("Element '%s' and property '%s' found!");			

  GValue gv = { 0 };
  gv = osc_value_to_g_value (type, value);

  gchar *value_string = g_strdup_value_contents (&gv);
  GST_LOG ("Setting property '%s' to value '%s'", property, value_string);
  g_free (value_string);
  g_object_set_property (G_OBJECT (target_elem), property, &gv);

  return 0;
}

int
osc_set_handler (const char *path, const char *types,
		 lo_arg ** argv, int argc, void *data, void *user_data)
{
  if (argc < 3)
    GST_WARNING ("osc_set_handler called with less than 3 arguments");
  return 1;

  GstOscctrl *filter = GST_OSCCTRL (user_data);
  const gchar *name = (const gchar *) argv[0];
  const gchar *property = (const gchar *) argv[1];
  lo_arg *value = argv[2];
  const gchar type = types[2];

  GST_LOG ("osc_set_handler. Name: '%s' Property: '%s' Type: '%c'",
	   name, property, type);

  return osc_set_helper (filter, name, property, value, type);
}

// Sets the property of an element to a certain value after 'time' seconds.
int
osc_controller_helper (GstOscctrl * filter,
		       const gchar * name, const gchar * property,
		       lo_arg * value, const gchar type, const gdouble time)
{
  GstElement *target_elem;
  GParamSpec *elem_prop;

  GST_INFO("Looking for property '%s' on element '%s'...", property,	
	   name);							
									
  target_elem =								
    gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(filter)), name);     
									
  if (target_elem == NULL) {						
    GST_WARNING("Element '%s' not found!", name);                       
    return 1;
  }									
									
  elem_prop = g_object_class_find_property(				
					   G_OBJECT_GET_CLASS(G_OBJECT(target_elem)), 
					   property);			
									
  if (elem_prop == NULL) {						
    GST_WARNING("Property '%s' not found!", property);                  
    return 1;
  }									
									
  GST_INFO("Element '%s' and property '%s' found!");			

  if (filter->controller != NULL)
    g_object_unref (G_OBJECT (filter->controller));

  GST_LOG ("Creating GstController and GstInterpolationControlSource");
  filter->controller = gst_controller_new (G_OBJECT (target_elem), property,
					   NULL);
  filter->control_source = gst_interpolation_control_source_new ();

  GST_LOG ("Setting interpolation mode to: '%d'", filter->interpolate_mode);
  gst_interpolation_control_source_set_interpolation_mode
    (filter->control_source, filter->interpolate_mode);
  gst_controller_set_control_source (filter->controller, property,
				     GST_CONTROL_SOURCE (filter->control_source));

  GST_LOG ("Creating GValues. Property type: '%s'. OSC type: '%c'",
	   G_PARAM_SPEC_TYPE_NAME (elem_prop), type);
  GValue original_value = { 0 };
  GValue new_target_value = { 0 };
  GValue new_value = { 0 };
  g_value_init (&original_value, G_PARAM_SPEC_VALUE_TYPE (elem_prop));
  g_value_init (&new_value, G_PARAM_SPEC_VALUE_TYPE (elem_prop));
  new_target_value = osc_value_to_g_value (type, value);

  GstFormat format = GST_FORMAT_TIME;
  gint64 cur_time = 0;

  g_object_get_property (G_OBJECT (target_elem), property, &original_value);

  gst_element_query_position (GST_ELEMENT_PARENT (filter), &format, &cur_time);
  gst_interpolation_control_source_set (filter->control_source,
					cur_time + GST_SECOND * 0, &original_value);

  g_value_transform (&new_target_value, &new_value);
  gchar *value_string = g_strdup_value_contents (&new_value);
  GST_LOG ("Setting property '%s' to value '%s' in '%f' seconds",
	   property, value_string, time);
  g_free (value_string);
  gst_interpolation_control_source_set (filter->control_source,
					cur_time + GST_SECOND * time, &new_value);

  g_object_unref (filter->control_source);

  return 0;
}

int
osc_controller_handler (const char *path, const char *types,
			lo_arg ** argv, int argc, void *data, void *user_data)
{
  if (argc < 4)
    
    return 1;

  GstOscctrl *filter = GST_OSCCTRL (user_data);
  const gchar *name = (const gchar *) argv[0];
  const gchar *property = (const gchar *) argv[1];
  lo_arg *value = argv[2];
  const gchar type = types[2];
  gdouble time = argv[3]->d;

  GST_LOG ("osc_controller_handler. Name: '%s' Property: '%s' Type: '%c' \
            Time: '%d'", name, property, type, time);

  return osc_controller_helper (filter, name, property, value, type, time);
}

/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
int
osc_handler (const char *path, const char *types,
	     lo_arg ** argv, int argc, void *data, void *user_data)
{
  char *function_name;
  const gchar *name;
  const gchar *property;
  const gchar *address;
  const gchar *port;
  GstOscctrl *filter = GST_OSCCTRL (user_data);

  function_name = strtok ((char *) path, "/");
  name = strtok (NULL, "/");
  property = strtok (NULL, "/");

  GST_LOG ("Trying to parse alternate path. Method: '%s' Element name: '%s' \
            Property: '%s'.", function_name, name, property);

  if (name != NULL && property != NULL) {
    if (strcmp (function_name, "get") == 0) {
      address = (const gchar *) argv[0];
      port = (const gchar *) argv[1];
      GST_LOG ("'%s' message received. Address: '%s' Port: '%s'",
	       function_name, address, port);
      return osc_get_helper (filter, name, property, address, port);
    } else if (strcmp (function_name, "set") == 0) {
      lo_arg *value = argv[0];
      const gchar type = types[0];
      GST_LOG ("'%s' message received. Value type: '%c'", function_name, type);
      return osc_set_helper (filter, name, property, value, type);
    } else if (strcmp (function_name, "subscribe") == 0) {
      address = (const gchar *) argv[0];
      port = (const gchar *) argv[1];
      GST_LOG ("'%s' message received. Address: '%s' Port: '%s'",
	       function_name, address, port);
      return osc_subscribe_helper (filter, name, property, address, port);
    } else if (strcmp (function_name, "unsubscribe") == 0) {
      address = (const gchar *) argv[0];
      port = (const gchar *) argv[1];
      GST_LOG ("'%s' message received. Address: '%s' Port: '%s'",
	       function_name, address, port);
      return osc_unsubscribe_helper (filter, name, property, address, port);
    } else if (strcmp (function_name, "control") == 0) {
      lo_arg *value = argv[0];
      const gchar type = types[0];
      const gdouble time = argv[1]->d;
      GST_LOG ("'%s' message received. Value type: '%c' Time: '%d'",
	       function_name, type, time);
      return osc_controller_helper (filter, name, property, value, type, time);
    }
  } else {
    GST_WARNING ("Missing element and/or property name");
  }

  GST_WARNING ("Unknown OSC message received.");

  //this handles message well formed but without already specified handler
  return 1;
}

void
register_osc_handlers (GstOscctrl * filter)
{
  GST_INFO ("Registering OSC handlers");
  lo_server_thread_add_method (filter->osc_server, 
			       "/set", 
			       NULL,
			       osc_set_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/get",
			       "ssss",
			       osc_get_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/locate", 
			       "d",
			       osc_locate_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server,
			       "/pause", 
			       "",
			       osc_pause_handler,
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/play", "",
			       osc_play_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/subscribe", 
			       "ssss",
			       osc_subscribe_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/unsubscribe", 
			       "ssss",
			       osc_unsubscribe_handler, 
			       (void *) filter);
  lo_server_thread_add_method (filter->osc_server, 
			       "/control", 
			       NULL,
			       osc_controller_handler, 
			       (void *) filter);
  /* add method that will match any path and args */
  lo_server_thread_add_method (filter->osc_server, 
			       NULL, 
			       NULL,
			       osc_handler, 
			       (void *) filter);
}

void
osc_handlers_debug_init ()
{
  GST_DEBUG_CATEGORY_INIT (gst_control_from_osc_handlers_debug,
			   "oscctrl", 0, "Template oscctrl");
}
