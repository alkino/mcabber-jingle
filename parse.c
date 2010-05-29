#include <loudmouth/loudmouth.h>
#include <string.h>

#include "parse.h"
#include "jingle.h"


static JingleAction action_from_str(const gchar* string);


struct JingleActionStr {
	JingleAction  act;
	const gchar  *name;
} jingle_actions_str[] = {
  { JINGLE_CONTENT_ACCEPT,    "content-accept" },
  { JINGLE_CONTENT_ADD,       "content-add" },
  { JINGLE_CONTENT_MODIFY,    "content-modify" },
  { JINGLE_CONTENT_REJECT,    "content-reject" },
  { JINGLE_CONTENT_REMOVE,    "content-remove" },
  { JINGLE_DESCRIPTION_INFO,  "description-info" },
  { JINGLE_SECURITY_INFO,     "security-info" },
  { JINGLE_SESSION_ACCEPT,    "session-accept" },
  { JINGLE_SESSION_INFO,      "session-info" },
  { JINGLE_SESSION_INITIATE,  "session-initiate" },
  { JINGLE_SESSION_TERMINATE, "session-terminate" },
  { JINGLE_TRANSPORT_ACCEPT,  "transport-accept" },
  { JINGLE_TRANSPORT_INFO,    "transport-info" },
  { JINGLE_TRANSPORT_REJECT,  "transport-reject" },
  { JINGLE_TRANSPORT_REPLACE, "transport-replace" },
};

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
gint check_jingle(LmMessageNode *node, struct jingle_data *ij)
{
  int nb_reason = 0;
  LmMessageNode *child = NULL;
  gchar *actionstr;

  actionstr     = lm_message_node_get_attribute(node, "action");
  ij->initiator = lm_message_node_get_attribute(node, "initiator");
  ij->responder = lm_message_node_get_attribute(node, "responder");
  ij->sid       = lm_message_node_get_attribute(node, "sid");

  if (action == NULL || ij->sid == NULL)
    return PARSE_ERROR_REQUIRED; // those elements are required

  ij->action = action_from_str(actionstr);
  if (ij->action == JINGLE_UNKNOWN)
    return PARSE_ERROR_RESTRICTION;

  // check childs
  for (child = node->children; child; child = child->next) {
    if (!strcmp(child->name, "reason"))
      nb_reason++;
  }

  if (nb_reason > 1)
    return PARSE_ERROR_TOO_MANY_CHILDS;

  return PARSE_OK;
}

gint check_content(LmMessageNode* node, struct content_data* ic)
{
  if (!strcmp(ic->name, "content"))
    return PARSE_ERROR_NAME;

  ic->creator     = lm_message_node_get_attribute(node, "creator");
  ic->disposition = lm_message_node_get_attribute(node, "disposition");
  ic->name        = lm_message_node_get_attribute(node, "name");
  ic->senders     = lm_message_node_get_attribute(node, "senders");

  if (ic->disposition == NULL)
    ic->disposition = "session";

  if (ic->creator == NULL || ic->name == NULL)
    return PARSE_ERROR_REQUIRED;

  if (!str_in_array(ic->creator, jingle_content_creator))
    return PARSE_ERROR_RESTRICTION;
  if (!str_in_array(ic->senders, jingle_content_senders))
    ic->senders = NULL;

  return PARSE_OK;
}


/**
 * Find the jingle_action corresponding to a string
 */
static JingleAction action_from_str(const gchar* string)
{
  guint i, actstrlen = sizeof(jingle_actions_str)/sizeof(struct JingleActionStr);
  for (i = 0; i < actstrlen; i++)
    if (!g_strcmp0(jingle_actions_str[i].name, string))
      return jingle_actions_str[i].act;

  return JINGLE_UNKNOWN;
}
