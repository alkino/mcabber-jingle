st/*
 * disco.c
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

#include "config.h"
 
#include <glib.h>
#include <loudmouth/loudmouth.h>

#include <mcabber/roster.h>

static gint compare_resources(gconstpointer a, gconstpointer b)
{
  return buddy_getresourceprio(a)-buddy_getresourceprio(b);
}

GSList* get_sorted_resources(const gchar *jid)
{
  roster *rs;
  
  GSList *reslist = buddy_search_jid(jid);
  
  if (reslist == NULL)
    return NULL;
  
  rs = (roster*)reslist->data;

  reslist = buddy_getresources(rs);
  
  reslist = g_slist_sort(reslist, compare_resources);
  
  return reslist;
}

void free_gslist_resources(GSList *reslist)
{
  GSList *el;
  roster *rs;
  
  for (el=reslist; el; el=el->next) {
    rs=(roster*)el->data;
    free_roster_user_data(rs);
  }
  g_slist_free(reslist);
}


