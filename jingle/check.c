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

#include <mcabber/utils.h>

#include <jingle/check.h>
#include <jingle/jingle.h>
#include <jingle/register.h>

static JingleContent *check_content(LmMessageNode *node, GError **err);
gint index_in_array(const gchar *str, const gchar **array);


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
 * @param message The incoming message
 * @param node    The <jingle> node in message
 * @param jn      The JingleNode to populate
 * @param err     A GError* to store the error
 * @return        TRUE if successfull, FALSE if an error happened
 * 
 * Populate a JingleNode struct from a <jingle> element.
 * Check if the element is in compliance with the XEP.
 */
gboolean check_jingle(LmMessage *message, LmMessageNode *node,
                      JingleNode *jn, GError **err)
{
  //gint nb_reason = 0;
  const gchar *actionstr;

  actionstr     = lm_message_node_get_attribute(node, "action");
  jn->initiator = lm_message_node_get_attribute(node, "initiator");
  jn->responder = lm_message_node_get_attribute(node, "responder");
  jn->sid       = lm_message_node_get_attribute(node, "sid");

  if (actionstr == NULL || jn->sid == NULL) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "an attribute of the jingle element is missing");
    return FALSE;
  }

  if (jn->initiator != NULL && check_jid_syntax(jn->initiator)) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the initiator attribute in invalid (not a jid)");
    return FALSE;
  }

  jn->action = jingle_action_from_str(actionstr);
  if (jn->action == JINGLE_UNKNOWN_ACTION) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                "the action attribute is invalid");
    return FALSE;
  }

  /*for (child = node->children; child; child = child->next) {
    if (!g_strcmp0(child->name, "reason"))
      nb_reason++;
  }

  if (nb_reason > 1) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADELEM,
                "too many reason elements");
    return FALSE;
  }*/

  jn->message = lm_message_ref(message);
  jn->node    = node;

  return TRUE;
}

static JingleContent *check_content(LmMessageNode *node, GError **err)
{
  JingleContent *cn = g_new0(JingleContent, 1);
  const gchar *creatorstr, *sendersstr;
  gint tmp, tmp2;

  creatorstr      = lm_message_node_get_attribute(node, "creator");
  cn->disposition = lm_message_node_get_attribute(node, "disposition");
  cn->name        = lm_message_node_get_attribute(node, "name");
  sendersstr      = lm_message_node_get_attribute(node, "senders");

  if (cn->name == NULL) {
    g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                "the name attribute of the content element is missing");
    g_free(cn);
    return NULL;
  }
  
  if (creatorstr == NULL) {
    cn->creator = JINGLE_CREATOR_INITIATOR;
  } else {
    tmp = index_in_array(creatorstr, jingle_content_creator);
    if(tmp < 0) {
      g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                  "the creator attribute is invalid");
    }
    cn->creator = (JingleCreator)tmp;
  }
  
  if (sendersstr == NULL) {
      cn->senders = JINGLE_SENDERS_BOTH;
  } else {
    tmp2 = index_in_array(sendersstr, jingle_content_senders);
    if (tmp2 < 0) {
      g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_BADVALUE,
                  "the senders attribute is invalid");
      g_free(cn);
      return NULL;
    }
    cn->senders = (JingleSenders)tmp2;
  }
    
  cn->description = lm_message_node_get_child(node, "description");
  cn->transport   = lm_message_node_get_child(node, "transport");
  if (cn->description == NULL || cn->transport == NULL) {
     g_set_error(err, JINGLE_CHECK_ERROR, JINGLE_CHECK_ERROR_MISSING,
                 "a child element of content is missing");
     g_free(cn);
     return NULL;
   }

  return cn;
}

/**
 * @param jn  The JingleNode to populate
 * @param err A GError* to store the error
 * @return    TRUE if successfull, FALSE if an error happened
 * 
 * Check all <content> element and add them to the JingleNode::content linked list.
 */
gboolean check_contents(JingleNode *jn, GError **err)
{
  LmMessageNode *child = NULL;
  JingleContent *cn;

  for (child = jn->node->children; child; child = child->next) {
    if (!g_strcmp0(child->name, "content")) {
      cn = check_content(child, err);
      if(cn == NULL) {
        if(jn->content != NULL) {
          g_slist_foreach(jn->content, (GFunc)g_free, NULL);
          g_slist_free(jn->content);
        }
        return FALSE;
      }
      jn->content = g_slist_append(jn->content, cn);
    }
  }
  return TRUE;
}

gint index_in_array(const gchar *str, const gchar **array)
{
  gint i;
  for (i = 0; array[i]; i++) {
    if (!g_strcmp0(array[i], str)) {
      return i;
    }
  }
  return -1;
}

GQuark jingle_check_error_quark(void)
{
  return g_quark_from_string("JINGLE_CHECK_ERROR");
}
