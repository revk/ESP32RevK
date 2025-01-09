// Home Assistant support / config librrary
#include "revk.h"
#ifdef	CONFIG_REVK_HALIB
#include "halib.h"

const char *
ha_config_opts (const char *config, ha_config_t h)
{
   if (!h.id)
      return "No name";
   char *topic;
   if (asprintf (&topic, "%s/%s/%s-%s/config", topicha, config, revk_id, h.id) < 0)
      return "malloc fail";
   char *hastatus = revk_topic (topicstate, NULL, NULL);
   char *hacmd = revk_topic (topiccommand, NULL, NULL);
   jo_t j = jo_object_alloc ();
   void addpath (const char *tag, const char *base, const char *path)
   {                            // Allow path. NULL is base, /suffix is after base, non / is absolute path
      if (!path)
         jo_string (j, tag, base);
      else if (*path == '/')
         jo_stringf (j, tag, "%s%s", base, path);
      else
         jo_string (j, tag, path);
   }
   jo_stringf (j, "unique_id", "%s-%s", revk_id, h.id);
   jo_object (j, "dev");
   jo_array (j, "ids");
   jo_string (j, NULL, revk_id);
   jo_close (j);
   jo_string (j, "name", hostname);
   jo_string (j, "mdl", appname);
   jo_string (j, "sw", revk_version);
   jo_string (j, "mf", "www.me.uk");
   jo_close (j);
   if (h.icon)
      jo_string (j, "icon", h.icon);
   if (h.type)
      jo_string (j, "dev_cla", h.type);
   if (h.name)
      jo_string (j, "name", h.name);
   if (h.stat)
   {
      addpath ("stat_t", hastatus, h.stat);
      jo_stringf (j, "val_tpl", "{{value_json.%s}}", h.field ? : h.id);
   }
   if (h.cmd)
      addpath ("cmd_t", hacmd, h.cmd);
   if (!strcmp (config, "sensor"))
   {                            // Sensor
      if (h.unit)
         jo_string (j, "unit_of_meas", h.unit);
   } else
      (!strcmp (config, "light"))
   {
      jo_string (j, "schema", "json");
   }
   // Availability
   jo_string (j, "avty_t", hastatus);
   jo_string (j, "avty_tpl", "{{value_json.up}}");
   jo_bool (j, "pl_avail", 1);
   jo_bool (j, "pl_not_avail", 0);
   free (hastatus);
   free (hacmd);
   revk_mqtt_send (NULL, 1, topic, h.delete ? NULL : &j);
   free (topic);
   return NULL;
}

#endif
