/*
 * check.c
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
#include <loudmouth/loudmouth.h>

#include "check.h"
#include "jingle.h"


const gchar *jingle_content_creator[] = {
  "initiator",
  "responder",
  NULL
};

const gchar *jingle_content_senders[] = {
  "both",
  "initiator",
  "none",
  "responder",
  NULL
};


/**
 * Populate a jingle_data struct from a <jingle> element.
 * Check if the element is in compliance with the XEP.
 */
gboolean check_jingle(LmMessageNode *node, JingleData *jd, GError **err)
{
  gint nb_reason = 0;
  LmMessageNode *child = NULL;
  const gchar *actionstr;

  actionstr     = lm_message_node_get_attribute(node, "action");
  jd->initiator = lm_message_node_get_attribute(node, "initiator");
  jd->responder = lm_message_node_get_attribute(node, "responder");
  jd->sid       = lm_message_node_get_attribute(node, "sid");

  if (actionstr == NULL || jd->sid == NULL) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the jingle element is missing");
    return FALSE;
  }

  jd->action = jingle_action_from_str(actionstr);
  if (jd->action == JINGLE_UNKNOWN_ACTION) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the action attribute is invalid");
    return FALSE;
  }

  // check childs
  for (child = node->children; child; child = child->next) {
    if (!g_strcmp0(child->name, "reason"))
      nb_reason++;
  }

  if (nb_reason > 1) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADELEM,
                "too many reason elements");
    return FALSE;
  }

  return TRUE;
}

GQuark jingle_check_error_quark()
{
  return g_quark_from_string("JINGLE_CHECK_ERROR");
}
