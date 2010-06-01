#ifndef __ERROR_H__
#define __ERROR_H__

void jingle_error_iq(LmMessage *m, const gchar *errtype,
    const gchar *cond, const gchar *jinglecond);

void jingle_error_bad_request(LmMessage *m);

#endif
