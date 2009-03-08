/*
 * Ayttm
 *
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * main.c
 * yipee
 */

#include "intl.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <locale.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <sys/un.h>
#include <sys/socket.h>
#endif

#ifdef _WIN32
#include <direct.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "status.h"
#include "plugin.h"
#include "globals.h"
#include "util.h"
#include "sound.h"
#include "smileys.h"
#include "console_session.h"
#include "crash.h"
#include "prefs.h"
#include "messages.h"
#include "edit_local_accounts.h"
#include "spellcheck.h"
#include "externs.h"

#include "ayttm_tray.h"
#include "icons.h"

#include <pixmaps/ayttm.xpm>

/* globals */
char geometry[256];

#ifdef CRASH_DIALOG
gchar *startup_dir = NULL;
gchar *argv0 = NULL;
static int crash;
static gchar *crash_param = NULL;
#endif

static void eb_cli_ver ()
{
	printf(PACKAGE_STRING "-" RELEASE "\n");
	printf("Copyright (C) 2003 The Ayttm team\n");
	printf("Ayttm comes with NO WARRANTY, to the extent permitted"
	       " by law.\n");
	printf("You may redistribute copies of Ayttm under the terms of the\n");
	printf("GNU General Public License.  For more information about these\n");
	printf("matters, see the file named COPYING.\n");
	printf("\nAyttm is brought to you by (in no order): \n");
	printf(" Colin Leroy, Andy Maloney, Philip Tellis, Edward Haletky,\n");
	printf(" and Tahir Hashmi.\n");
	printf("\nFor more information on Ayttm, visit the website at\n");
	printf("         http://ayttm.sf.net/\n");
	return;
}

struct option_help
{
	const char   s_opt;
	const char * l_opt;
	const char * help;
};

static const struct option_help options [] =
{
	{'h', "help",       "Display this help and exit"},
	{'v', "version",    "Output version information and exit"},
	{'g', "geometry",   "Set position of main window"},
	{'d', "config-dir", "Specify configuration directory"},
	{'D', "disable-server",    "Disable console message server"},
	{'c', "contact",    "Specify contact to send a message to via console server"},
	{'m', "message",    "Specify the message to send via console server"},
	{' ', NULL,        "-c and -m must be used toegether"},
	{'u', "url",        "pass an url to a module"},
	{'\0', NULL,         NULL}
};



static void eb_cli_help (const char * cmd)
{
	unsigned int i = 0;

	printf("Usage: %s [OPTION]...\n", cmd);
	printf("An integrated instant messenger program.\n");
	printf("Supports AIM, ICQ, MSN, Yahoo!, Jabber, and IRC.\n\n");

#if defined (HAVE_GETOPT) || defined (HAVE_GETOPT_LONG)

	for (i = 0; options [i].help != NULL; i++) {
#ifdef HAVE_GETOPT_LONG

		printf ("  %c%c  %2s%15s  %s\n",
		  (options [i].s_opt == ' ' ? ' ' : '-'),
		  options [i].s_opt,
		  (options [i].l_opt == NULL ? "  " : "--"),
		  options [i].l_opt,
		  options [i].help);

#else /* HAVE_GETOPT_LONG */

  /* Leaves HAVE_GETOPT */
		printf ("  %c%c  %s\n",
		  (options [i].s_opt == ' ' ? ' ' : '-'),
		  options [i].s_opt,
		  options [i].help);

#endif /* HAVE_GETOPT_LONG */
	}

#endif /* defined (HAVE_GETOPT) || defined (HAVE_GETOPT_LONG) */
}


/* Global variable, referenced in globals.h */
char config_dir[1024] = "";


static void start_login(gboolean new)
{
	/* Setting the default icon for un-iconed windows */
	GdkPixbuf *default_icon = gdk_pixbuf_new_from_xpm_data((const char **)ayttm_xpm);
	gtk_window_set_default_icon(default_icon);

	ay_load_tray_icon(default_icon);
//	eb_status_window();

   	if (new)
		ay_edit_local_accounts();
	else
		eb_sign_on_startup() ;
}
 
int main(int argc, char *argv[])
{

	int c = 0;
	int sock;
	int len;
#ifndef __MINGW32__
	struct sockaddr_un local;
	int disable_console_server = FALSE;
#else
	struct sockaddr_in local;
	int disable_console_server = TRUE;
#endif
	char message[4096] = "";
	char contact[256] = "";
	char url[4096] = "";
	gchar buff[1024];
	struct stat stat_buf;
	gboolean accounts_success = FALSE;

	pid_t pid;
	const char * cmd = argv [0];

	srand(time(NULL));
	setlocale(LC_ALL, "");
	bindtextdomain (PACKAGE, LOCALEDIR);
	textdomain (PACKAGE);

	geometry[0] = 0;

/* Setup default value for config_dir */
#if defined( _WIN32 )
	{
		HKEY appKey;
		DWORD dwType, dwSize;
		char appDir[1024];
		dwType = REG_SZ;
		dwSize = 1024;
		RegOpenKey(HKEY_CURRENT_USER,
	"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
		&appKey);
		RegQueryValueEx(appKey, "AppData", NULL, &dwType, 
			&appDir[0], &dwSize);
		RegCloseKey(appKey);
		g_snprintf(config_dir, 1024, "%s\\Ayttm\\", appDir);
	}
#else
	gtk_set_locale ();
	g_snprintf(config_dir, 1024, "%s/.ayttm/",getenv("HOME"));
#endif

	g_thread_init(NULL);

#if defined ( HAVE_GETOPT ) || defined ( HAVE_GETOPT_LONG )
	while (1) {
#ifdef HAVE_GETOPT_LONG
		static struct option long_options[] = 
		{
	    	{"version", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
	    	{"geometry", required_argument, NULL, 'g'},
	    	{"activate-goad-server", required_argument, NULL, 'a'},
	    	{"disable-server", no_argument, NULL, 'D'},
	    	{"contact", required_argument, NULL, 'c'},
	    	{"message", required_argument, NULL, 'm'},
	    	{"config-dir", required_argument, NULL, 'd'},
	    	{"url", required_argument, NULL, 'u'},
#ifdef CRASH_DIALOG
	    	{"crash", required_argument, NULL, 'C'},
#endif
	    	{0, 0, 0, 0}
		};
    
		int option_index = 0;

		c = getopt_long(argc, argv, "vhg:a:d:Dc:m:u:C:", long_options, &option_index);
#else /* HAVE_GETOPT_LONG */
/* Either had getopt_long or getopt or both so getopt should be here */
		c = getopt (argc, argv, "vhg:a:d:Dc:m:u:C:");
#endif
		/* Detect the end of the options. */

		if (c == -1)
			break;

		switch (c) {
		    case 'v':           /* version information */
		        eb_cli_ver ();
		        exit(0);
			break;

		    case 'h':           /* help information */
		    case '?':
  		        eb_cli_help (cmd);
			exit (0);
		        break;

		    case 'g':
			strncpy(geometry, optarg, sizeof(geometry));
			break;

		    case ':':
			printf("Try 'ayttm --help' for more information.\n");
			exit(0);

		    case 'a':
			printf("hi\n");
			break;

		    case 'D':
			printf("Disabling console message server.\n");
			disable_console_server = TRUE;
			break;

		    case 'c':
			strncpy(contact, optarg, sizeof(contact));
			break;

		    case 'm':
			strncpy(message, optarg, sizeof(message));
			break;

		    case 'u':
			strncpy(url, optarg, sizeof(url));
			break;

#ifdef CRASH_DIALOG
		    case 'C':
			crash = 1;    
			crash_param = strdup(optarg);
			break;
#endif			
		    case 'd':
			strncpy(config_dir, optarg, sizeof(config_dir));
			/*Make sure we have directory delimiter */
#if defined( _WIN32 )
			if (config_dir[strlen(config_dir)-1]!='\\')
				strcat(config_dir, "\\");
#else
			if (config_dir[strlen(config_dir)-1]!='/')
				strcat(config_dir, "/");
#endif
			if (stat(config_dir, &stat_buf)==-1)
			{
				perror(config_dir);
				exit(errno);
			}
			if (!S_ISDIR(stat_buf.st_mode))
			{
				printf("config-dir %s is not a directory!\n", config_dir);
				exit(1);
			}
			break;
		}
	} 
    
#ifdef CRASH_DIALOG
	startup_dir = g_get_current_dir();
	argv0 = g_strdup(argv[0]);
	if (crash) {
		gtk_set_locale();
		gtk_init(&argc, &argv);
		ayttm_prefs_read();
		crash_main(crash_param);
		return 0;
	}
	crash_install_handlers();
#endif

#endif

	if ( (strlen(message) > 0 && strlen(contact) > 0)
		|| strlen(url) > 0) {
#ifndef __MINGW32__
		struct sockaddr_un remote;
#else
		struct sockaddr_in remote;
#endif
		short length;
		int ret;

#ifndef __MINGW32__
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		strncpy(remote.sun_path, config_dir, sizeof(remote.sun_path));
		strncat(remote.sun_path, "eb_socket",
			sizeof(remote.sun_path)-strlen(remote.sun_path));
		remote.sun_family = AF_UNIX;
		len = strlen(remote.sun_path) + sizeof(remote.sun_family) +1;
#else
		sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
		if (connect(sock, (struct sockaddr*)&remote, len) == -1 ) {
			perror("connect");
			exit(1);
		}
		if (strlen(url) > 0) {
			length = strlen("URL-ayttm")+1;
			write(sock, &length, sizeof(short));
			write(sock, "URL-ayttm", length);
			length = strlen(url)+1;
			write(sock, &length, sizeof(short));
			write(sock, url, length);
		} else {
			length = strlen(contact)+1;
			write(sock, &length, sizeof(short));
			write(sock, contact, length);
			length = strlen(message)+1;
			write(sock, &length, sizeof(short));
			write(sock, message, length);
		}
		read(sock, &ret, sizeof(int));
		close(sock);
		exit(ret);
	}

	sound_init();

	gtk_set_locale();
	gtk_init(&argc, &argv);
	
/*	g_snprintf(buff, 1024, "%s",config_dir);*/

	/* Mizhi 04-04-2001 */
	g_snprintf(buff, 1024, "%s.lock", config_dir);
	if ((pid = create_lock_file(buff)) > 0) {
#ifndef __MINGW32__
		struct sockaddr_un remote;
#else
		struct sockaddr_in remote;
#endif
		short length;
		int ret;
#ifndef __MINGW32__
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		strncpy(remote.sun_path, config_dir, sizeof(remote.sun_path));
		strncat(remote.sun_path, "eb_socket",
			sizeof(remote.sun_path)-strlen(remote.sun_path));
		remote.sun_family = AF_UNIX;
		len = strlen(remote.sun_path) + sizeof(remote.sun_family) +1;
#else
		sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
		if (connect(sock, (struct sockaddr*)&remote, len) == -1 ) {
			perror("connect");
			g_snprintf(buff, 1024, _("Ayttm is already running (pid=%d), and the remote-control UNIX socket cannot be connected."), pid);
			/* TODO make sure this one is modal */
			ay_do_error( _("Already Logged On"), buff );
			exit(1);
		}
		length = strlen("focus-ayttm")+1;
		write(sock, &length, sizeof(short));
		write(sock, "focus-ayttm", length);
		read(sock, &ret, sizeof(int));
		close(sock);
		exit(ret);
	}

	gtk_set_locale ();
	g_snprintf(buff, 1024, "%s",config_dir);

	mkdir(buff, 0700);
	g_snprintf(buff, 1024, "%slogs",config_dir);
	mkdir(buff, 0700);

	if (!disable_console_server) {
#ifndef __MINGW32__
		sock = socket(AF_UNIX, SOCK_STREAM, 0);
		strncpy(local.sun_path, config_dir, sizeof(local.sun_path));
		strncat(local.sun_path, "eb_socket",
			sizeof(local.sun_path)-strlen(local.sun_path));
		unlink(local.sun_path);
		local.sun_family = AF_UNIX;
		len = strlen(local.sun_path) + sizeof(local.sun_family) +1;
#else
		sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
		if (bind(sock, (struct sockaddr *)&local, len) == -1) {
			perror("Unable to handle console connections.  bind");
			exit(1);
		}
		if (listen(sock, 5) == -1) {
			perror("Unable to handle console connections. listen");
			exit(1);
		}
		eb_input_add(sock, EB_INPUT_READ, console_session_init, NULL);
	}

	/* Initalize the menus that are available through the plugin_api */
	init_menus();
	
	/* initialise outgoing_message_filters with the pre/post filter marker */
	outgoing_message_filters = l_list_append(outgoing_message_filters, NULL);

	ayttm_prefs_init();

	ayttm_prefs_read();
	ay_spell_check_reload();

	/* Load all the modules in the module_path preference, details can be found in the preferences module tab */
	init_smileys();
	
	load_modules();

	ay_register_icons ();

	accounts_success = load_accounts();
	//if (accounts_success)
	load_contacts();

	start_login(!accounts_success);

/*	if(!iGetLocalPref("usercount_window_seen"))
		show_wnd_usercount();*/

	gtk_main();

	clean_up_dummies();

	write_contact_list();
	
	/*
	 * Moved unload_modules() call to window delete event in status.c
	 */
	unload_modules();  // Need to unload what we load

	eb_debug(DBG_CORE, "Shutting sound down\n");
	sound_shutdown();
	eb_debug(DBG_CORE, "Removing lock file\n");
	g_snprintf(buff, 1024, "%s.lock", config_dir);
#ifndef __MINGW32__
	if (!disable_console_server)
		unlink(local.sun_path);
        delete_lock_file(buff);
	eb_debug(DBG_CORE, "Removed lock file\n");
#endif

	return 0;
}
