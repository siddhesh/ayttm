/*
 * Ayttm 
 *
 * Copyright (C) 2003, the Ayttm team
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

#include <stddef.h>
#include <gtk/gtk.h>

#include "plugin_api.h"
#include "activity_bar.h"

/*
 * NOTE: We do not use gtk--
 */

/*
 * callback prototypes - these are in C because they're used by gtk
 */
#ifdef __cplusplus
extern "C" {
#endif
static void	s_cancel_clicked(GtkButton * btn, gpointer data);
static gint	s_delete_event(GtkWidget *widget, GdkEvent *event, gpointer null);
static int	s_update_activity(void * data);
#ifdef __cplusplus
}
#endif

/**
 * The activity_bar_pack class - this contains the label, progress bar and
 * cancel button.  It calls the cancel callback when cancelled and handles
 * updating of the label and progress bar.
 * update_activity() must be called periodically
 */
class ay_activity_bar_pack {
	private:
	GtkWidget *m_lbl, *m_hbox, *m_pbar, *m_cbtn;
	activity_bar_cancel_callback m_cancel_callback;
	void *m_userdata;
	int m_tag;
	unsigned long m_size;
	
	public:
	ay_activity_bar_pack(const char *label,
		activity_bar_cancel_callback cancel_callback, void *userdata, 
		unsigned long size, int mode);
	void update_label(const char *label);
	void update_progress(const unsigned long done);
	void update_activity( void );
	void cancel( void );
	int get_tag( void ) const;
	~ay_activity_bar_pack( void );

	friend class ay_activity_window;
};

/**
 * The activity_window class - this contains a list of all progress bars.
 * you can add/remove progress bars from it.  It handles the timer that
 * updates all existing progress bars.
 * The window is created and displayed when the first progress bar is
 * added, and deleted when the last one is removed.
 *
 * @warning Do NOT create more than one instance of this class.
 * One instance is created static in this file.  When it goes out of
 * scope, all destructors will be called.  This should take care of
 * dynamically allocated memory.
 */
class ay_activity_window {
	private:
	static LList *s_packs;
	static GtkWidget *s_window, *s_vbox;
	static int s_last_tag;
	static int s_timeout;

	void create_window( void );
	void show( void );

	public:
	int add_pack(ay_activity_bar_pack *abp);
	void remove_pack(ay_activity_bar_pack *abp);
	ay_activity_bar_pack * get_pack_by_tag(int tag) const;
	void close_window( void );
	~ay_activity_window( void );
};
/* initialise static variables */
LList *ay_activity_window::s_packs=NULL;
GtkWidget *ay_activity_window::s_window=NULL;
GtkWidget *ay_activity_window::s_vbox=NULL;
int ay_activity_window::s_last_tag=0;
int ay_activity_window::s_timeout=0;

/* Create the only window that will exist */
static ay_activity_window aw;

/*
 * methods for ay_activity_bar_pack
 */
ay_activity_bar_pack::ay_activity_bar_pack(const char *label,
		activity_bar_cancel_callback cancel_callback, void *userdata, 
		unsigned long size, int mode)
:	m_cancel_callback( cancel_callback ),
	m_userdata( userdata ),
	m_size( size )
{
	m_lbl = gtk_label_new(label);
	gtk_label_set_justify(GTK_LABEL(m_lbl), GTK_JUSTIFY_LEFT);
	gtk_widget_show(m_lbl);

	m_hbox = gtk_hbox_new(FALSE, 0);

	m_pbar = gtk_progress_bar_new();
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(m_pbar), 
			GTK_PROGRESS_LEFT_TO_RIGHT );
	gtk_progress_set_activity_mode(GTK_PROGRESS(m_pbar), mode);
	gtk_box_pack_start(GTK_BOX(m_hbox), m_pbar, TRUE, TRUE, 10);
	gtk_widget_show(m_pbar);

	m_cbtn = gtk_button_new_with_label(_("Cancel"));
	if(cancel_callback) {
		gtk_signal_connect(GTK_OBJECT(m_cbtn), "clicked",
			GTK_SIGNAL_FUNC(s_cancel_clicked), this);
	} else {
		gtk_widget_set_sensitive(m_cbtn, FALSE);
	}
	
	gtk_box_pack_start(GTK_BOX(m_hbox), m_cbtn, FALSE, FALSE, 10);
	gtk_widget_show(m_cbtn);

	gtk_widget_show(m_hbox);

	m_tag = aw.add_pack(this);
}

void ay_activity_bar_pack::update_label(const char * label)
{
	gtk_label_set_text(GTK_LABEL(m_lbl), label);
}

void ay_activity_bar_pack::update_progress(const unsigned long done)
{
	gtk_progress_set_percentage(GTK_PROGRESS(m_pbar), ((float)done)/((float)m_size));
}

void ay_activity_bar_pack::update_activity( void )
{
	float new_val = gtk_progress_get_value(GTK_PROGRESS(m_pbar)) + 3;
	GtkAdjustment *adj = GTK_PROGRESS(m_pbar)->adjustment;
	if (m_size > 0)
		return;
	
	if (new_val > adj->upper)
		new_val = adj->lower;

	gtk_progress_set_value(GTK_PROGRESS(m_pbar), new_val);
}

void ay_activity_bar_pack::cancel( void )
{
	if(m_cancel_callback)
		m_cancel_callback(m_userdata);
}

int ay_activity_bar_pack::get_tag( void ) const
{
	return m_tag;
}

ay_activity_bar_pack::~ay_activity_bar_pack()
{
	gtk_widget_hide(m_hbox);
	gtk_widget_hide(m_lbl);

	gtk_widget_destroy(m_hbox);
	gtk_widget_destroy(m_lbl);

	aw.remove_pack(this);
}
/*
 * End ay_activity_bar_pack methods
 */

/*
 * Start ay_activity_window methods
 */
void ay_activity_window::create_window( void )
{
	s_window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect(GTK_OBJECT(s_window), "delete_event", 
			GTK_SIGNAL_FUNC(s_delete_event), NULL);
	gtk_window_set_policy(GTK_WINDOW(s_window), FALSE, TRUE, TRUE);
	gtk_window_set_position(GTK_WINDOW(s_window), GTK_WIN_POS_MOUSE);
	s_vbox = gtk_vbox_new(TRUE, 0);

	gtk_container_add(GTK_CONTAINER(s_window), s_vbox);

	s_timeout = eb_timeout_add(50, s_update_activity, &s_packs);
}

void ay_activity_window::show( void )
{
	gtk_widget_show(s_vbox);
	gtk_widget_show(s_window);
}

int ay_activity_window::add_pack(ay_activity_bar_pack *abp)
{
	if(!s_window)
		create_window();

	s_packs = l_list_prepend(s_packs, abp);
	gtk_box_pack_start(GTK_BOX(s_vbox), abp->m_lbl, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(s_vbox), abp->m_hbox, TRUE, TRUE, 0);

	if(! GTK_WIDGET_VISIBLE(GTK_WIDGET(s_window)) )
		show();

	return ++s_last_tag;
}

void ay_activity_window::remove_pack(ay_activity_bar_pack *abp)
{
	s_packs = l_list_remove(s_packs, abp);

	if(abp->m_tag == s_last_tag)
		s_last_tag--;

	if(!s_packs)
		close_window();
}

ay_activity_bar_pack * ay_activity_window::get_pack_by_tag(int tag) const
{
	LList *l;
	for(l = s_packs; l; l = l_list_next(l)) {
		ay_activity_bar_pack *abp = (ay_activity_bar_pack *)l->data;
		if(abp->m_tag == tag) {
			return abp;
		}
	}

	return NULL;
}

void ay_activity_window::close_window( void )
{
	if(s_timeout) {
		eb_timeout_remove(s_timeout);
		s_timeout = 0;
	}

	if(s_window)
		gtk_widget_hide(s_window);

	while(s_packs) {
		ay_activity_bar_pack *abp = (ay_activity_bar_pack *)s_packs->data;
		abp->cancel();
		s_packs = l_list_remove(s_packs, abp);
		delete abp;
	}

	if(s_window) {
		gtk_widget_destroy(s_window);
		s_window = s_vbox = NULL;
	}
}

/**
 * Destroy the activity window and all data associated with it
 * @note This will call cancel callbacks for all pending bars
 */ 
ay_activity_window::~ay_activity_window( void )
{
	close_window();
}
/*
 * End ay_activity_window_methods
 */

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Callbacks
 */
static void s_cancel_clicked(GtkButton * btn, gpointer data)
{
	ay_activity_bar_pack *abp = (ay_activity_bar_pack *)data;

	/* btn == NULL means owner closed it with bar_remove */
	if(btn && abp)
		abp->cancel();
	if (abp)
		delete abp;
}

static gint s_delete_event(GtkWidget *widget, GdkEvent *event, gpointer null)
{
	aw.close_window();
	return FALSE;
}

static int s_update_activity(void * data)
{
	LList *l = *((LList **)data);
	for(; l; l = l_list_next(l)) {
		ay_activity_bar_pack *abp = (ay_activity_bar_pack *)l->data;
		abp->update_activity();
	}

	return 1;
}

/*
 * Start C interface
 */

/**
 * Update the label associated with an activity bar
 * @param	tag		a tag that identifies the activity bar
 * @param	label	the text to be displayed on the label
 */
void ay_activity_bar_update_label(int tag, const char *label)
{
	ay_activity_bar_pack *abp = aw.get_pack_by_tag(tag);
	if(abp)
		abp->update_label(label);
}

/**
 * Update the label associated with an activity bar
 * @param	tag		a tag that identifies the activity bar
 * @param	label	the text to be displayed on the label
 */
void ay_progress_bar_update_progress(int tag, const unsigned long done)
{
	ay_activity_bar_pack *abp = aw.get_pack_by_tag(tag);
	if(abp)
		abp->update_progress(done);
}

/**
 * Remove an existing activity bar.  Destroy the window if this is the last
 * @param	tag		a tag that identifies the activity bar
 */
void ay_activity_bar_remove(int tag)
{
	ay_activity_bar_pack *abp = aw.get_pack_by_tag(tag);
	if(tag && abp)
		s_cancel_clicked(NULL, abp);
}

/**
 * Add a new activity bar, create the window if this is the first
 * @param	label				the text to display against this activity bar
 * @param	cancel_callback		a function to be called if the user clicks cancel
 * @param	userdata			the callback data to be passed to cancel_callback
 *
 * @return	a tag to identify this activity bar
 */
int ay_activity_bar_add(const char *label, 
		activity_bar_cancel_callback cancel_callback, void *userdata)
{
	ay_activity_bar_pack *abp = 
		new ay_activity_bar_pack(label, cancel_callback, userdata, 0, TRUE);

	return abp->get_tag();
}

/**
 * Add a new progress bar, create the window if this is the first
 * @param	label				the text to display against this activity bar
 * @param	cancel_callback			a function to be called if the user clicks cancel
 * @param	userdata			the callback data to be passed to cancel_callback
 *
 * @return	a tag to identify this activity bar
 */
int ay_progress_bar_add(const char *label, unsigned long size, 
		activity_bar_cancel_callback cancel_callback, void *userdata)
{
	ay_activity_bar_pack *abp = 
		new ay_activity_bar_pack(label, cancel_callback, userdata, size, FALSE);

	return abp->get_tag();
}

#ifdef __cplusplus
}
#endif
