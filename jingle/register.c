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

#include <jingle/register.h>
#include <jingle/jingle.h>


typedef struct {
  gchar *xmlns;
  JingleAppFuncs *funcs;
} AppHandlerEntry;

typedef struct {
  gchar *xmlns;
  JingleTransportFuncs *funcs;
} TransportHandlerEntry;


static AppHandlerEntry *jingle_find_app(const gchar *xmlns);
static TransportHandlerEntry *jingle_find_transport(const gchar *xmlns);


GSList *jingle_app_handlers = NULL;
GSList *jingle_transport_handlers = NULL;


void jingle_register_app(const gchar *xmlns, JingleAppFuncs *funcs)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_APP_PREFIX)) return;

  AppHandlerEntry *h = g_new(AppHandlerEntry, 1);

  h->xmlns  = g_strdup(xmlns);
  h->funcs  = funcs;

  jingle_app_handlers = g_slist_append(jingle_app_handlers, h);
}

void jingle_register_transport(const gchar *xmlns, JingleTransportFuncs *funcs)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_TRANSPORT_PREFIX)) return;

  TransportHandlerEntry *h = g_new(TransportHandlerEntry, 1);

  h->xmlns  = g_strdup(xmlns);
  h->funcs  = funcs;

  jingle_transport_handlers = g_slist_append(jingle_transport_handlers, h);
}

JingleAppFuncs *jingle_get_appfuncs(const gchar *xmlns)
{
  AppHandlerEntry *entry;
  return (entry = jingle_find_app(xmlns)) != NULL ? entry->funcs : NULL;
}

JingleTransportFuncs *jingle_get_transportfuncs(const gchar *xmlns)
{
  TransportHandlerEntry *entry;
  return (entry = jingle_find_transport(xmlns)) != NULL ? entry->funcs : NULL;
}

/**
 * This function should work with AppHandlerEntry and
 * TransportHandlerEntry as long as both start with xmlns.
 */
static gint jingle_entry_cmp(gconstpointer a, gconstpointer b)
{
  return g_strcmp0(((AppHandlerEntry *) a)->xmlns, (const gchar*) b);
}

static AppHandlerEntry *jingle_find_app(const gchar *xmlns)
{
  GSList *entry = g_slist_find_custom(jingle_app_handlers, xmlns,
                                      jingle_entry_cmp);
  return (AppHandlerEntry *) entry;
}

static TransportHandlerEntry *jingle_find_transport(const gchar *xmlns)
{
  GSList *entry = g_slist_find_custom(jingle_transport_handlers, xmlns,
                                      jingle_entry_cmp);
  return (TransportHandlerEntry *) entry;
}

static void jingle_free_app(AppHandlerEntry *entry)
{
  g_free(entry->xmlns);
  g_free(entry);
}

static void jingle_free_transport(TransportHandlerEntry *entry)
{
  g_free(entry->xmlns);
  g_free(entry);
}

void jingle_unregister_app(const gchar *xmlns)
{
  AppHandlerEntry *entry = jingle_find_app(xmlns);
  if (entry) {
    jingle_free_app(entry);
    jingle_app_handlers = g_slist_remove(jingle_app_handlers, entry);
  }
}

void jingle_unregister_transport(const gchar *xmlns)
{
  TransportHandlerEntry *entry = jingle_find_transport(xmlns);
  if (entry) {
    jingle_free_transport(entry);
    jingle_transport_handlers = g_slist_remove(jingle_app_handlers, entry);
  }
}
