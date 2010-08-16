#ifndef __JINGLEFT_H__
#define __JINGLEFT_H__ 1

/**
 * \file filetransfer.h
 * \brief The main file of app-module to the file transfer XEP 234
 * \author Nicolas Cornu
 * \version 0.1
 */
 
#define NS_JINGLE_APP_FT      "urn:xmpp:jingle:apps:file-transfer:1"
#define NS_JINGLE_APP_FT_INFO "urn:xmpp:jingle:apps:file-transfer:info:1"
#define NS_SI_FT              "http://jabber.org/protocol/si/profile/file-transfer"
#define JINGLE_FT_SIZE_READ 2048

/**
 * \enum JingleFTType
 * \brief type of the content
 */
typedef enum {
  JINGLE_FT_OFFER,
  JINGLE_FT_REQUEST,
} JingleFTType;

/**
 * \enum JingleFTDirection
 * \brief Direction of the file
 */
typedef enum {
  JINGLE_FT_INCOMING, /*!< We download the file */
  JINGLE_FT_OUTGOING /*!< We upload the file */
} JingleFTDirection;

/**
 * \enum JingleFTState
 * \brief state of the file
 */
typedef enum {
  JINGLE_FT_PENDING, /*!< The file isn't yet accepted by the both parts */
  JINGLE_FT_STARTING, /*!< The file is transfering */
  JINGLE_FT_ENDING, /*!< The file has been transfered */
  JINGLE_FT_REJECT, /*!< The transfer has been refused */
  JINGLE_FT_ERROR /*!< And error occured during the transfer */
} JingleFTState;

/**
 * \struct JingleFT
 * \brief represent the file transfer himself
 */
typedef struct {
  /**
   * the last modification of the file, optional 
   */
  time_t date;

  /**
   * MD5 hash of the file, optional 
   */
  gchar *hash;

  /**
   * the name of the file that the sender wishes to send
   */
  gchar *name;

  /**
   * the size, in bytes, of the data to be sent 
   */
  guint64 size;

  /**
   * Data already send/receive 
   */
  guint64 transmit;
  
  /**
   * descriptor to the output file
   */
  GIOChannel *outfile;
  
  /**
   * Is it an offer or a request ?
   */
  JingleFTType type;
  
  /**
   * Is it if the file is incoming or outgoing
   */
  JingleFTDirection dir;
  
  /**
   * The state of the file (PENDING, STARTING, ...) 
   */
  JingleFTState state;
  
  /**
   * A little description of the transfer
   */
  gchar *desc;
  
  /**
   * Where we compute the hash
   */
  GChecksum *md5;
  
} JingleFT;

/**
 * \struct JingleFTInfo
 * \brief contain only a link to the gconstpointer
 *
 * We keep localy a list of all the files transfered to print infos
 *
 */
typedef struct {
  /**
   * An index. Not use now, but will in the futur to cancel a transfer.
   */
  int index;
  
  /**
   * The link to the JingleFT in the session.
   */
  JingleFT *jft;
} JingleFTInfo;
#endif
