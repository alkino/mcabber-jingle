/*
 * register.c
 *
 * Copyrigth (C) 2010 Nicolas Cornu <nicolas.cornu@ensi-bourges.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <glib.h>

#include <mcabber/logprint.h>

#include "jingle.h"

GHashTable *hk_jingle_apps_handler_hash = NULL;
GHashTable *hk_jingle_transports_handler_hash = NULL;

gchar* jingle_register_apps(const gchar* namespace)
{
  gchar* hookname = NULL;

  if (!hk_jingle_apps_handler_hash) {
    hk_jingle_apps_handler_hash = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, &g_free);
    if (!hk_jingle_apps_handler_hash) {
      scr_log_print(LPRINT_LOGNORM, "Couldn't create hook hash table for jingle apps!");
      return NULL;
    }
  }

  hookname = g_strdup_printf("%s%s", "hook_jingle_apps_", namespace);

  g_hash_table_insert(hk_jingle_apps_handler_hash, g_strdup(namespace), hookname);

  return hookname;
}

gchar* jingle_register_transports(const gchar* namespace) {
  gchar* hookname = NULL;

  if (!hk_jingle_transports_handler_hash) {
    hk_jingle_transports_handler_hash = g_hash_table_new_full(&g_str_hash, &g_str_equal, &g_free, &g_free);
    if (!hk_jingle_transports_handler_hash) {
      scr_log_print(LPRINT_LOGNORM, "Couldn't create hook hash table for jingle transports!");
      return NULL;
    }
  }

  hookname = g_strdup_printf("%s%s", "hook_jingle_transports_", namespace);

  g_hash_table_insert(hk_jingle_transports_handler_hash, g_strdup(namespace), hookname);

  return hookname;
}
