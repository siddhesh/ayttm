#ifndef FILE_SELECT_H
#define FILE_SELECT_H

#ifdef __cplusplus
extern "C" {
#endif
void eb_do_file_selector(char *default_filename, char *window_title, 
		void (*callback)(char *filename, gpointer data), gpointer data);
#ifdef __cplusplus
}
#endif
#endif
