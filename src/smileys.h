#ifndef __SMILEYS_H_
#define __SMILEYS_H_

#include "llist.h"
#include "chat_window.h"
#include "chat_room.h"

struct protocol_smiley_struct
{
  char text[16]; // :-), :), ;-), etc
  char name[64]; // this goes into the <smiley> tag for the gtkhtml stuff
};

typedef struct protocol_smiley_struct protocol_smiley;


struct smiley_struct
{
  char name[64];
  gchar ** pixmap; // from an xpm file, you know the drill...
};

typedef struct smiley_struct smiley;

#if defined(__MINGW32__) && defined(__IN_PLUGIN__)
__declspec(dllimport) LList *smileys;
#else
extern LList *smileys;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void init_smileys(void);
gchar * eb_smilify(gchar * text, LList * protocol_smileys);

LList * eb_default_smileys(void);

LList * add_smiley(LList * list, char * name, gchar ** data);

LList * add_protocol_smiley(LList * list, char * text, char * name);

/* someone figure out how to do this with LList * const */
LList * eb_smileys(void);

smiley * get_smiley_by_name(char * name);

typedef struct _smiley_callback_data smiley_callback_data;

struct _smiley_callback_data
{
	eb_chat_room   *c_room;
	chat_window    *c_window;
};

void show_smileys_cb (smiley_callback_data *data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // ifndef _SMILEYS_
