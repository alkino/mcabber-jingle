#ifndef __JINGLE_CHECK_H__
#define __JINGLE_CHECK_H__ 1

#include <glib.h>
#include <loudmouth/loudmouth.h>

#include <jingle/jingle.h>

GSList* get_sorted_resources(const gchar *jid);
void free_gslist_resources(GSList *reslist);
#endif
