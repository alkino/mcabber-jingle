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
	JingleAppHandler handler;
	gpointer data;
} AppHandlerEntry;

typedef struct {
	gchar *xmlns;
	JingleTransportHandler handler;
	gpointer data;
} TransportHandlerEntry;


GSList *jingle_app_handlers = NULL;
GSList *jingle_transport_handlers = NULL;


gboolean jingle_register_app(const gchar *xmlns,
                             JingleAppHandler func,
                             gpointer data)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_APP_PREFIX)) return FALSE;

  AppHandlerEntry *h = g_new(AppHandlerEntry, 1);

  h->xmlns   = g_strdup(xmlns);
  h->handler = func;
  h->data    = data;

  jingle_app_handlers = g_slist_append(jingle_app_handlers, h);

  return TRUE;
}

gboolean jingle_register_transport(const gchar *xmlns,
                                   JingleTransportHandler func,
                                   gpointer data)
{
  if (!g_str_has_prefix(xmlns, NS_JINGLE_TRANSPORT_PREFIX)) return FALSE;

  TransportHandlerEntry *h = g_new(TransportHandlerEntry, 1);

  h->xmlns   = g_strdup(xmlns);
  h->handler = func;
  h->data    = data;

  jingle_transport_handlers = g_slist_append(jingle_transport_handlers, h);

  return TRUE;
}

static AppHandlerEntry *jingle_find_app(const gchar *xmlns)
{
  GSList *tmpl;
  AppHandlerEntry *entry;
  
  for (tmpl = jingle_app_handlers; tmpl; tmpl = tmpl->next) {
    entry = (AppHandlerEntry *)tmpl->data;
    if (!g_strcmp0(entry->xmlns, xmlns))
      return entry;
  }
  return NULL;
}

static TransportHandlerEntry *jingle_find_transport(const gchar *xmlns)
{
  GSList *tmpl;
  TransportHandlerEntry *entry;
  
  for (tmpl = jingle_transport_handlers; tmpl; tmpl = tmpl->next) {
    entry = (TransportHandlerEntry *)tmpl->data;
    if (!g_strcmp0(entry->xmlns, xmlns))
      return entry;
  }
  return NULL;
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
