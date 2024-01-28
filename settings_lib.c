// (new) settings library
#include "revk.h"
#ifndef  CONFIG_REVK_OLD_SETTINGS
static const char __attribute__((unused)) * TAG = "Settings";

extern revk_settings_t revk_settings[];

static nvs_handle nvs[2] = { -1, -1 };

char *__malloc_like __result_use_check
strdup (const char *s)
{
   int l = strlen (s);
   char *o = mallocspi (l + 1);
   if (!o)
      return NULL;
   memcpy (o, s, l + 1);
   return o;
}

char *__malloc_like __result_use_check
strndup (const char *s, size_t l)
{
   int l2 = strlen (s);
   if (l2 < l)
      l = l2;
   char *o = mallocspi (l + 1);
   if (!o)
      return NULL;
   memcpy (o, s, l);
   o[l] = 0;
   return o;
}

static const char *
nvs_get (revk_settings_t * s, const char *tag, int index)
{                               // Getting NVS
   if (s->array && index >= s->array)
      return "Array overflow";
   size_t len = 0;
   esp_err_t err;
   if (!s->ptr)
   {                            // Bit
      uint8_t data = 0;
      if ((err = nvs_get_u8 (nvs[s->revk], tag, &data)))
         return "Cannot load bit";
      if (data)
         ((uint8_t *) & revk_settings_bits)[s->bit / 8] |= (1 << (s->bit & 7));
      else
         ((uint8_t *) & revk_settings_bits)[s->bit / 8] &= ~(1 << (s->bit & 7));
      return NULL;
   }
   if (s->blob)
   {
      void **p = s->ptr + (s->size ? : sizeof (void *)) * index;
      if (nvs_get_blob (nvs[s->revk], tag, NULL, &len))
         return "Cannot get blob len";
      len += sizeof (revk_settings_blob_t);
      revk_settings_blob_t *d = mallocspi (len);
      if (!d)
         return "malloc";
      if (nvs_get_blob (nvs[s->revk], tag, d->data, &len))
         return "Cannot load blob";
      d->len = len - sizeof (revk_settings_blob_t);
      free (*p);
      *p = &d;
      return NULL;
   }
   if (!s->size)
   {                            /* String */
      void **p = s->ptr + (s->size ? : sizeof (void *)) * index;
      if (nvs_get_str (nvs[s->revk], tag, NULL, &len))
         return "Cannot get string len";
      if (!len)
         return "Bad string len";
      char *data = mallocspi (len);
      if (!data)
         return "malloc";
      if (nvs_get_str (nvs[s->revk], tag, data, &len))
         return "Cannot load string";
      free (*p);
      *p = data;
      return NULL;
   }
   void *data = s->ptr;
   uint64_t temp;
   if (!data)
      data = &temp;
   if (s->sign)
   {
      if ((s->size == 8 && (err = nvs_get_i64 (nvs[s->revk], tag, data))) ||    //
          (s->size == 4 && (err = nvs_get_i32 (nvs[s->revk], tag, data))) ||    //
          (s->size == 2 && (err = nvs_get_i16 (nvs[s->revk], tag, data))) ||    //
          (s->size == 1 && (err = nvs_get_i8 (nvs[s->revk], tag, data))))
         return "Cannot load number (signed)";
      return NULL;
   }
   if ((s->size == 8 && (err = nvs_get_u64 (nvs[s->revk], tag, data))) ||       //
       (s->size == 4 && (err = nvs_get_u32 (nvs[s->revk], tag, data))) ||       //
       (s->size == 2 && (err = nvs_get_u16 (nvs[s->revk], tag, data))) ||       //
       (s->size == 1 && (err = nvs_get_u8 (nvs[s->revk], tag, data))))
      return "Cannot load number (unsigned)";
   return NULL;
}

static const char *
parse_numeric (revk_settings_t * s, void **pp, const char **dp, const char *e)
{                               // Single numeric parse to memory, advance memory and source
   if (!s || !dp || !pp)
      return "NULL";
   if (!s->size)
      return "Not numeric";
   const char *err = NULL;
   const char *d = *dp;
   void *p = *pp;
   while (d && d < e && *d == ' ')
      d++;
   if (!d || d >= e)
   {                            // Empty
      memset (p, 0, s->size);
   } else
   {                            // Value
      uint64_t v = 0,
         f = 0;
      char sign = 0;
      int bits = s->size * 8;
      if (s->set)
         f |= (1ULL << (--bits));
      int top = bits - 1;
      const char *b = s->bitfield;
      void scan (void)
      {                         // Scan for bitfields
         while (d < e)
         {
            int l = 1;
            while (d + l < e && (d[l] & 0xC0) == 0x80)
               l++;
            int bit = top;
            const char *q;
            for (q = b; *q; q++)
               if (!(f & (1ULL << bit)) && !memcmp (q, d, l))   // Allows for duplicates in bitfield
               {
                  f |= (1ULL << bit);
                  d += l;
                  break;
               } else if (*q != ' ' && !((*q & 0xC0) == 0x80))
                  bit--;
            if (!*q)
               break;           // Not found
         }
      }
      if (b)
      {                         // Bit fields
         for (const char *q = b; *q; q++)
            if (*q != ' ' && !((*q & 0xC0) == 0x80))
               bits--;
         if (!err && bits < 0)
            err = "Too many bitfields";
         scan ();
      }
      // Numeric value
      if (d < e && *d == '-')
         sign = *d++;
      if (!err && bits && (d >= e || !isdigit ((int) *d)))
         err = "No number found";
      void add (char c)
      {
         uint64_t wrap = v;
         v = v * 10 + (c + -'0');
         if (!err && v < wrap)
            err = "Number too large";
      }
      while (!err && d < e && isdigit ((int) *d))
         add (*d++);
      if (!err && s->decimal)
      {
         int q = s->decimal;
         if (d < e && *d == '.')
         {
            d++;
            while (!err && d < e && isdigit ((int) *d) && q && q--)
               add (*d++);
            if (!err && d < e && isdigit ((int) *d))
               err = "Too many decimal places";
         }
         while (!err && q--)
            add ('0');
      }
      if (b)
         scan ();
      if (!err && sign && !s->sign)
         err = "Negative not allowed";
      if (!err && bits < 64 && v >= (1ULL << bits))
         err = "Number too big";
      if (sign)
         v = -v;
      if (bits < 64)
         v = ((v & ((1ULL << bits) - 1)) | f);
      if (!err)
      {
         if (s->size == 1)
            *((uint8_t *) p) = v;
         else if (s->size == 2)
            *((uint16_t *) p) = v;
         else if (s->size == 4)
            *((uint32_t *) p) = v;
         else if (s->size == 8)
            *((uint64_t *) p) = v;
         else
            err = "Bad number size";
      }
   }
   while (d && d < e && *d == ' ')
      d++;
   if (d && d < e && *d == ',')
      d++;
   while (d && d < e && *d == ' ')
      d++;
   p += (s->size ? : sizeof (void *));
   *pp = p;
   *dp = d;
   return err;
}

static const char *
load_default (revk_settings_t * s)
{
   const char *err = NULL;
   int a = s->array;
   const char *d = s->def;
   const char *e = NULL;
   if (d)
   {
      e = d + strlen (d);
      if (s->dq && e > d + 1 && *d == '"' && e[-1] == '"')
      {
         d++;
         e--;
      }
      if (d == e)
         d = e = NULL;
   }
   if (d && s->hex)
   {
      // TODO
   }
   if (d && s->base64)
   {
      // TODO
   }
   if (s->ptr)
   {                            // value
      if (s->size)
      {                         // Value
         void *p = s->ptr;
         if (a && s->size == 1 && a && (s->hex || s->base64))
         {                      // Fixed string
            if (d)
            {
               // TODO
            } else
               memset (p, 0, a);
         } else
         {                      // Value(s)
            if (d)
            {
               err = parse_numeric (s, &p, &d, e);
               if (a)
                  while (!err && --a)
                     err = parse_numeric (s, &p, &d, e);
               if (!err && d < e)
                  err = "Extra data on end of number";
            } else
               memset (p, 0, s->size * (a ? : 1));
         }
      } else
      {                         // Pointer
         void **p = (void **) s->ptr;
         if (s->blob)
         {                      // Blob
            if (d)
            {
               // TODO
            } else
            {                   // Empty blob
               free (*p);
               *p = calloc (1, 2);
               if (a)
                  while (--a)
                  {
                     free (*++p);
                     *p = calloc (1, 2);
                  }
            }
         } else
         {                      // String
            free (*p);
            if (d)
               *p = strndup (d, (int) (e - d));
            else
               *p = strdup ("");
            if (a)
               while (--a)
               {
                  free (*++p);
                  *p = strdup ("");
               }
         }
      }
   }

   else
   {                            // bitfield
      if (d && (*d == '1' || *d == 't'))
         ((uint8_t *) & revk_settings_bits)[s->bit / 8] |= (1 << (s->bit & 7));
      else
         ((uint8_t *) & revk_settings_bits)[s->bit / 8] &= ~(1 << (s->bit & 7));
   }
   if (err)
      ESP_LOGE (TAG, "%s %s", s->name, err);
   return err;
}

void
revk_settings_load (const char *tag, const char *appname)
{                               // Scan NVS to load values to settings
   for (revk_settings_t * s = revk_settings; s->len1; s++)
      load_default (s);
   // Scan
   for (int revk = 0; revk < 2; revk++)
   {
      nvs_open_from_partition (revk ? tag : "nvs", revk ? tag : appname, NVS_READWRITE, &nvs[revk]);
      // TODO Make list of deletions...
      // TODO loading values
      nvs_iterator_t i = NULL;
      if (!nvs_entry_find (revk ? tag : "nvs", revk ? tag : appname, NVS_TYPE_ANY, &i))
      {
         do
         {
            nvs_entry_info_t info = { 0 };
            if (!nvs_entry_info (i, &info))
            {
               int l = strlen (info.key);
               revk_settings_t *s;
               const char *err = NULL;
               for (s = revk_settings; s->len1; s++)
               {
                  if (!s->array && s->len == l && !memcmp (s->name, info.key, l))
                  {             // Exact match
                     err = nvs_get (s, info.key, 0);
                     break;
                  } else if (s->array && s->len + 1 == l && !memcmp (s->name, info.key, s->len) && info.key[s->len] >= 0x80)
                  {             // Array match, new
                     err = nvs_get (s, info.key, info.key[s->len] - 0x80);
                     break;
                  } else if (s->array && s->len < l && !memcmp (s->name, info.key, s->len) && isdigit ((int) info.key[s->len]))
                  {             // Array match, old
                     err = nvs_get (s, info.key, atoi (info.key + s->len) - 1);
                     break;
                  }
               }
               if (!s->len1)
                  err = "Not found";
               if (err)
                  ESP_LOGE (tag, "NVS %s Failed %s", info.key, err);
               // TODO delete?
            }
         }
         while (!nvs_entry_next (&i));
      }
      nvs_release_iterator (i);
   }
}

const char *
revk_setting_dump (void)
{
   return "TODO";
}

const char *
revk_setting (jo_t j)
{
   return "TODO";
}

void
revk_settings_commit (void)
{
   REVK_ERR_CHECK (nvs_commit (nvs[0]));
   REVK_ERR_CHECK (nvs_commit (nvs[1]));
}

#endif
