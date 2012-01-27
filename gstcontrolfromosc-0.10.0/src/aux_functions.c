#include "aux_functions.h"

GST_DEBUG_CATEGORY_STATIC (gst_control_from_osc_aux_debug);
#define GST_CAT_DEFAULT gst_control_from_osc_aux_debug

#define BUILD_REPLY(osctype, varname) \
        g_object_get(G_OBJECT(target_elem), property, &varname, NULL); \
        lo_message_add(message, osctype, name, property, varname); \
        break;

// Sets the appropriate type on the 'message' that will be sent according to the
// property being read. 'message' is modified by this function.
void
create_message (lo_message message, GstElement * target_elem,
    GParamSpec * elem_prop)
{
  gint intvalue;
  guint uintvalue;
  glong glongvalue;
  gulong gulongvalue;
  gint64 gint64value;
  guint64 guint64value;
  gfloat gfloatvalue;
  gdouble gdoublevalue;
  gchar gcharvalue;
  guchar gucharvalue;
  gchar *gstringvalue;
  gboolean gboolvalue;

  gchar *name = gst_element_get_name (target_elem);
  const gchar *property = g_param_spec_get_name (elem_prop);

  switch (elem_prop->value_type) {
    case G_TYPE_INT:
      BUILD_REPLY ("ssi", intvalue);
    case G_TYPE_UINT:
      BUILD_REPLY ("ssi", uintvalue);
    case G_TYPE_LONG:
      BUILD_REPLY ("ssi", glongvalue);
    case G_TYPE_ULONG:
      BUILD_REPLY ("ssi", gulongvalue);
    case G_TYPE_INT64:
      BUILD_REPLY ("ssh", gint64value);
    case G_TYPE_UINT64:
      BUILD_REPLY ("ssh", guint64value);
    case G_TYPE_FLOAT:
      BUILD_REPLY ("ssf", gfloatvalue);
    case G_TYPE_DOUBLE:
      BUILD_REPLY ("ssd", gdoublevalue);
    case G_TYPE_CHAR:
      BUILD_REPLY ("ssc", gcharvalue);
    case G_TYPE_UCHAR:
      BUILD_REPLY ("ssc", gucharvalue);
    case G_TYPE_STRING:
      BUILD_REPLY ("sss", gstringvalue);
    case G_TYPE_BOOLEAN:
      g_object_get (G_OBJECT (target_elem), property, &gboolvalue, NULL);
      /* if (gboolvalue) */
      /*   lo_message_add (message, "ssT", name, property); */
      /* else */
      /*   lo_message_add (message, "ssF", name, property); */
      //this is because of OSC library that does not handle the boolean type
      if (gboolvalue)
	lo_message_add (message, "sss", name, property, "true");
      else
	lo_message_add (message, "sss", name, property, "false");
      break;
    default:
      GST_WARNING ("Unknown parameter type %s\n",
          g_type_name (elem_prop->value_type));
      break;
  }

  g_free (name);
}

gint
compare_lo_address (gconstpointer a, gconstpointer b)
{
  return g_strcmp0 (lo_address_get_url ((lo_address *) a), b);
}

// Maps OSC types into GValue types
GValue
osc_value_to_g_value (gchar type, gpointer value)
{
  GValue ret = { 0 };

  switch (type) {
    case 'i':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%d'",
          type, g_type_name (G_TYPE_INT), *(gint *) value);
      g_value_init (&ret, G_TYPE_INT);
      g_value_set_int (&ret, *(gint *) value);
      break;
    case 'f':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%f'",
          type, g_type_name (G_TYPE_FLOAT), *(gfloat *) value);
      g_value_init (&ret, G_TYPE_FLOAT);
      g_value_set_float (&ret, *(gfloat *) value);
      break;
    case 's':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%s'",
          type, g_type_name (G_TYPE_STRING), (gchar *) value);
      g_value_init (&ret, G_TYPE_STRING);
      g_value_set_string (&ret, (gchar *) value);
      break;
    case 'h':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%"
          G_GINT64_FORMAT "'",
          type, g_type_name (G_TYPE_INT64), *(gint64 *) value);
      g_value_init (&ret, G_TYPE_INT64);
      g_value_set_int64 (&ret, *(gint64 *) value);
      break;
    case 'd':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%lf'",
          type, g_type_name (G_TYPE_DOUBLE), *(gdouble *) value);
      g_value_init (&ret, G_TYPE_DOUBLE);
      g_value_set_double (&ret, *(gdouble *) value);
      break;
    case 'c':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value '%c'",
          type, g_type_name (G_TYPE_CHAR), *(gchar *) value);
      g_value_init (&ret, G_TYPE_CHAR);
      g_value_set_char (&ret, *(gchar *) value);
      break;
    case 'T':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value 'TRUE'",
          type, g_type_name (G_TYPE_BOOLEAN));
      g_value_init (&ret, G_TYPE_BOOLEAN);
      g_value_set_boolean (&ret, TRUE);
      break;
    case 'F':
      GST_LOG ("OSC type '%c' maps to GType '%s' with value 'FALSE'",
          type, g_type_name (G_TYPE_BOOLEAN));
      g_value_init (&ret, G_TYPE_BOOLEAN);
      g_value_set_boolean (&ret, FALSE);
      break;
    default:
      GST_WARNING ("Type '%c' not supported\n", type);
      g_value_init (&ret, G_TYPE_INVALID);
      break;
  }
  return ret;
}

// Functions that perform transformation between string GValues and numeric
// GValues.

#define TRANSFORM_FUNC(func_name, gvalfunc, ctype, convfunc)                \
static void                                                                 \
value_transform_##func_name (const GValue *src_value,                       \
                             GValue       *dest_value)                      \
{                                                                           \
  const gchar *str = g_value_get_string(src_value);                         \
  ctype c_value = convfunc;                                                 \
  g_value_set_##gvalfunc(dest_value, c_value);                              \
}

TRANSFORM_FUNC (str_int, int, gint, g_ascii_strtoll (str, NULL, 10));
TRANSFORM_FUNC (str_long, long, glong, g_ascii_strtoll (str, NULL, 10));
TRANSFORM_FUNC (str_int64, int64, gint64, g_ascii_strtoll (str, NULL, 10));
TRANSFORM_FUNC (str_uint, uint, guint, g_ascii_strtoull (str, NULL, 10));
TRANSFORM_FUNC (str_ulong, ulong, gulong, g_ascii_strtoull (str, NULL, 10));
TRANSFORM_FUNC (str_uint64, uint64, guint64, g_ascii_strtoull (str, NULL, 10));
TRANSFORM_FUNC (str_float, float, gfloat, g_ascii_strtod (str, NULL));
TRANSFORM_FUNC (str_double, double, gdouble, g_ascii_strtod (str, NULL));
TRANSFORM_FUNC (str_char, char, gchar, str[0]);
TRANSFORM_FUNC (str_uchar, uchar, guchar, str[0]);

static void
value_transform_str_bool (const GValue * src_value, GValue * dest_value)
{
  const gchar *str = g_value_get_string (src_value);
  gboolean b = FALSE;

  if (g_ascii_strcasecmp (str, "true") == 0)
    b = TRUE;

  g_value_set_boolean (dest_value, b);
}

void
register_gvalue_transforms ()
{
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT,
      value_transform_str_int);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_LONG,
      value_transform_str_long);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT64,
      value_transform_str_int64);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT,
      value_transform_str_uint);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ULONG,
      value_transform_str_ulong);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT64,
      value_transform_str_uint64);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_FLOAT,
      value_transform_str_float);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DOUBLE,
      value_transform_str_double);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_CHAR,
      value_transform_str_char);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UCHAR,
      value_transform_str_uchar);
  g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN,
      value_transform_str_bool);
}

void
aux_functions_debug_init ()
{
  GST_DEBUG_CATEGORY_INIT (gst_control_from_osc_aux_debug, "controlfromosc",
      0, "Template controlfromosc");
}
