#include <loudmouth/loudmouth.h>
#include <string.h>

#include "parse.h"

int parse_jingle (LmMessageNode *node, struct info_jingle *ij)
{
  int nb_reason = 0;
  LmMessageNode *child = NULL;
  
  if(!strcmp(ij->name, "jingle"))
    return PARSE_ERROR_NAME;

  ij->action    = attrcpy(lm_message_node_get_attribute(node, "action"));
  ij->initiator = attrcpy(lm_message_node_get_attribute(node, "initiator"));
  ij->responder = attrcpy(lm_message_node_get_attribute(node, "responder"));
  ij->sid       = attrcpy(lm_message_node_get_attribute(node, "sid"));
   
  // check required
  if(ij->action == NULL || ij->sid == NULL)
    return PARSE_ERROR_REQUIRED;
    
  // check restrictions
 if(!check_restriction(ij->action, {"content-accept", "content-add",
  "content-modify", "content-reject", "content-remove", "description-info",
  "security-info", "session-accept", "session-info", "session-initiate",
  "session-terminate", "transport-accept", "transport-info", "transport-reject",
  "transport-replace", NULL}))
   return PARSE_ERROR_RESTRICTION;
  
  // check childs
  for(child = node->children; child; child = child->next) {
    if(!strcmp(child->name, "reason"))
      nb_reason++;
  }
  
  if(reason > 1)
    return PARSE_ERROR_TOO_MANY_CHILDS;
    
  return PARSE_OK;
}


void free_jingle(struct info_jingle *ij)
{
  free(ij->action);
  free(ij->initiator);
  free(ij->responder);
  free(ij->sid);
}


int parse_content(LmMessageNode* node, struct info_content* ic)
{
  if(!strcmp(ic->name, "content"))
    return PARSE_ERROR_NAME;

  ic->creator     = attrcpy(lm_message_node_get_attribute(node, "creator"));
  ic->disposition = attrcpy(lm_message_node_get_attribute(node, "disposition"));
  ic->name        = attrcpy(lm_message_node_get_attribute(node, "name"));
  ic->senders     = attrcpy(lm_message_node_get_attribute(node, "senders"));

  // Put default if none
  if(ic->disposition == NULL)
    ic->disposition = attrcpy("session");

  // check required
  if(ic->creator == NULL || ic->name == NULL)
    return PARSE_ERROR_REQUIRED;

  // check restrictions
  if(!check_restriction(ic->creator, {"initiator", "responder", NULL}))
    return PARSE_ERROR_RESTRICTION;
  if(!check_restriction(ic->senders, {"both", "initiator", "none", "responder", NULL}))
    ic->senders = NULL; // because it's optional
    
  return PARSE_OK;
}


void free_content(struct info_content *ic)
{
  free(ic->creator);
  free(ic->disposition);
  free(ic->name);
  free(ic->senders);
}


int check_restriction(const char* name, const char** values)
{
  const char* value;
  int found = 0;
  value = values[0];
  while(value && !found) {
    if(!strcmp(name, value))
      found = 1;
    value++;
  }
  return found;
}


char* attrcpy(const char* attr)
{
  char *tmp = NULL;
  if(attr != NULL) {
    tmp = (char*) malloc((strlen(attr)+1) * sizeof(char));
    strcpy(tmp, attr);
  }
  return tmp;
}
