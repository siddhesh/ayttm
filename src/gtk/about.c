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

#include "intl.h"

#include "dialog.h"

#include "pixmaps/ayttmlogo.xpm"


/* info about a developer */
typedef struct
{
	const char	*name;
	const char	*email;
	const char	*role;
} tDevInfo;


/* Ayttm developers */

#define	AYTTM_TEAM_SIZE	(5)

static tDevInfo sAyttmDevTeam[AYTTM_TEAM_SIZE] =
{
	{"Colin Leroy", 	"<colin@colino.net>", 		"Lead Developer"},
	{"Andy S. Maloney", 	"<a_s_maloney@yahoo.com>", 	"Code Monkey"},
	{"Philip S. Tellis", 	"<philip.tellis@iname.com>", 	"Yahoo!"},
	{"Edward L. Haletky", 	"<elh@astroarch.com>",		"Windows Port"},
	{"Tahir Hashmi", 	"<code_martial@softhome.net>",	"Manual, Doc."}
};

/* Everybuddy developers */

#define	EVERYBUDDY_TEAM_SIZE	(9)

static tDevInfo sEverybuddyDevTeam[EVERYBUDDY_TEAM_SIZE] =
{
	{"Torrey Searle", 	"<tsearle@antihe.ro>", 		"Creator"},
	{"Ben Rigas", 		"<ben@flygroup.org>", 		"Web Design & GUI Devel"},
	{"Jared Peterson", 	"<jared@tgflinux.com>",		"GUI Devel"},
	{"Alex Wheeler", 	"<awheeler@speakeasy.org>", 	"Jabber Devel & Much More :)"},
	{"Robert Lazzurs", 	"<lazzurs@lazzurs.myftp.org>", 	"Maintainer"},
	{"Meredydd", 		"<m_luff@mail.wincoll.ac.uk>", 	"MSN Devel"},
	{"Erik Inge Bolso", 	"<knan@mo.himolde.no>", 	"IRC Devel"},
	{"Colin Leroy", 	"<colin@colino.net>", 		"Various hacks, i18n, Ayttm fork"},
	{"Philip Tellis", 	"<philip.tellis@iname.com>", 	"Yahoo Devel"}
};

#define	HELP_TEAM_SIZE	(4)

static tDevInfo sHelpDevTeam[HELP_TEAM_SIZE] =
{
	{"Yann Marigo", 	"<yann@yannos.com>", 		"Art, Fr translation"},
	{"Toby A. Inkster", 	"<tobyink@goddamn.co.uk>", 	"Patches"},
	{"Ben Reser", 		"<ben@reser.org>", 		"Patches, MDK RPMs"},
	{"Lee Leahu", 		"<penguin365@dyweni.com>", 	"Patches"}
};

/* the one and only about window */
static GtkWidget *sAboutWindow = NULL;

/*
  Functions
*/

static void destroy_about()
{
	if ( sAboutWindow != NULL )
		gtk_widget_destroy( sAboutWindow );

	sAboutWindow = NULL;
}

static GtkTable	*sMakeDeveloperTable( int inNumDevelopers, const tDevInfo *inDevInfo )
{
	GtkTable	*table = GTK_TABLE( gtk_table_new( inNumDevelopers, 3, FALSE ) );
	GtkLabel	*label = NULL;
	int			i = 0;


	for (i = 0; i < inNumDevelopers; i++)
	{
		label = GTK_LABEL( gtk_label_new( inDevInfo[i].name ) );
		gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
		gtk_widget_show( GTK_WIDGET(label ) );
		gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, i, i+1, GTK_FILL, GTK_FILL, 10, 0 );

		label = GTK_LABEL( gtk_label_new( inDevInfo[i].email ) );
		gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
		gtk_widget_show( GTK_WIDGET( label ) );
		gtk_table_attach( table, GTK_WIDGET( label ), 1, 2, i, i+1, GTK_FILL, GTK_FILL, 10, 0 );

		label = GTK_LABEL( gtk_label_new( inDevInfo[i].role ) );
		gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
		gtk_widget_show( GTK_WIDGET( label ) );
		gtk_table_attach(table, GTK_WIDGET( label ), 2, 3, i, i+1, GTK_FILL, GTK_FILL, 10, 0 );
	}

	gtk_widget_show( GTK_WIDGET( table ) );
	
	return( table );
}

void	ay_ui_show_about(void *useless, void *null)
{
	GtkWidget	*logo = NULL;
	GtkWidget	*ok = NULL;
	GtkLabel	*label = NULL;
	GtkTable	*table = NULL;
	GtkWidget	*separator = NULL;
	GtkBox		*vbox = NULL, *vbox2 = NULL;
	GtkStyle	*style = NULL;
	GdkPixmap	*pm = NULL;
	GdkBitmap	*bm = NULL;
	GtkWidget	*scroll = NULL;
	const char	*versionStr = "Ayttm " VERSION "-" RELEASE "\n" __DATE__;


	if ( sAboutWindow != NULL )
	{
		gtk_widget_show(sAboutWindow);
		return;
	}

	/* create the window and set some attributes */
	sAboutWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(sAboutWindow);
	gtk_window_set_position(GTK_WINDOW(sAboutWindow), GTK_WIN_POS_MOUSE);
	gtk_window_set_policy( GTK_WINDOW(sAboutWindow), FALSE, FALSE, TRUE );
	gtk_window_set_title(GTK_WINDOW(sAboutWindow), _("About Ayttm"));
	eb_icon(sAboutWindow->window);
	gtk_container_border_width(GTK_CONTAINER(sAboutWindow), 5);

	vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
	gtk_widget_show(GTK_WIDGET(vbox));

	vbox2 = GTK_BOX(gtk_vbox_new(FALSE, 0));
	gtk_widget_show(GTK_WIDGET(vbox2));

	/* logo */
	style = gtk_widget_get_style(sAboutWindow);
	pm = gdk_pixmap_create_from_xpm_d(sAboutWindow->window, &bm,
                 &style->bg[GTK_STATE_NORMAL], (gchar **)ayttmlogo_xpm);
	logo = gtk_pixmap_new(pm, bm);
	gtk_box_pack_start(vbox, logo, FALSE, FALSE, 5);
	gtk_widget_show(logo);

	/* version */
	label = GTK_LABEL(gtk_label_new( versionStr ));
	gtk_widget_show( GTK_WIDGET(label) );
	gtk_box_pack_start(vbox, GTK_WIDGET(label), FALSE, FALSE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_start(vbox, separator, FALSE, FALSE, 5);

	/* scroll */
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scroll);
	
	/* text */
	label = GTK_LABEL(gtk_label_new( _("Ayttm is brought to you by:") ));
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show( GTK_WIDGET(label) );
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of Ayttm developers */
	table = sMakeDeveloperTable( AYTTM_TEAM_SIZE, sAyttmDevTeam );
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* text */
	label = GTK_LABEL(gtk_label_new( _("Ayttm is based on Everybuddy 0.4.3 which was brought to you by:") ));
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show( GTK_WIDGET(label) );
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of Everybuddy developers */
	table = sMakeDeveloperTable( EVERYBUDDY_TEAM_SIZE, sEverybuddyDevTeam );
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* text */
	label = GTK_LABEL(gtk_label_new( _("\"With a little help from\":") ));
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show( GTK_WIDGET(label) );
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* list of "Help" team */
	table = sMakeDeveloperTable( HELP_TEAM_SIZE, sHelpDevTeam );
	gtk_box_pack_start(vbox2, GTK_WIDGET(table), TRUE, TRUE, 5);

	gtk_scrolled_window_add_with_viewport(
			GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(vbox2));
	
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
	gtk_box_pack_start(vbox, GTK_WIDGET(scroll), TRUE, TRUE, 5);
	
	/* separator */
	separator = gtk_hseparator_new();
	gtk_widget_show( separator );
	gtk_box_pack_start(vbox2, separator, TRUE, TRUE, 5);

	/* text */
	label = GTK_LABEL(gtk_label_new( _("And a special thanks to Mark Spencer, initial creator of GAIM,\nfor all the tremendous support he has given.") ));
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show( GTK_WIDGET(label) );
	gtk_box_pack_start(vbox2, GTK_WIDGET(label), TRUE, TRUE, 5);

	/* close button */
	ok = gtk_button_new_with_label(_("Close"));
	gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
		                  GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(sAboutWindow));

	gtk_box_pack_start(vbox, ok, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(ok);
	gtk_widget_show(ok);
	
	gtk_signal_connect_object(GTK_OBJECT(sAboutWindow), "destroy",
                                  GTK_SIGNAL_FUNC(destroy_about), GTK_OBJECT(sAboutWindow));

	gtk_container_add(GTK_CONTAINER(sAboutWindow), GTK_WIDGET(vbox));

	gtk_widget_set_usize(sAboutWindow, -1, 400);
	
	gtk_widget_show(sAboutWindow);
	gtk_widget_grab_focus(ok);

}

