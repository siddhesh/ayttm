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

#include <gtk/gtk.h>
#include "llist.h"
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
static void cancel_clicked(GtkButton * btn, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer null);
static int update_activity(void * data);
#ifdef __cplusplus
}
#endif

/*
 * The activity_bar_pack class - this contains the label, progress bar and
 * cancel button.  It calls the cancel callback when cancelled and handles
 * updating of the label and progress bar.
 * update_activity() must be called periodically
 */
class ay_activity_bar_pack {
	private:
	GtkWidget *lbl, *hbox, *pbar, *cbtn;
	activity_bar_cancel_callback cancel_callback;
	void *userdata;
	int tag;

	public:
	ay_activity_bar_pack(const char *label,
		activity_bar_cancel_callback cancel_callback, void *userdata);
	void update_label(const char *label);
	void update_activity( void );
	void cancel( void );
	int get_tag( void );
	~ay_activity_bar_pack( void );

	friend class ay_activity_window;
};

/*
 * The activity_window class - this contains a list of all progress bars.
 * you can add/remove progress bars from it.  It handles the timer that
 * updates all existing progress bars.
 * The window is created and displayed when the first progress bar is
 * added, and deleted when the last one is removed.
 * Do NOT create more than one instance of this class.
 * One instance is created static in this file.  When it goes out of
 * scope, all destructors will be called.  This should take care of
 * dynamically allocated memory.
 */
class ay_activity_window {
	private:
	static LList *packs;
	static GtkWidget *window, *vbox;
	static int last_tag;
	static int timeout;

	void create_window( void );
	void show( void );

	public:
	int add_pack(ay_activity_bar_pack *abp);
	void remove_pack(ay_activity_bar_pack *abp);
	ay_activity_bar_pack * get_pack_by_tag(int tag);
	void close_window( void );
	~ay_activity_window( void );
};
/* initialise static variables */
LList *ay_activity_window::packs=NULL;
GtkWidget *ay_activity_window::window=NULL;
GtkWidget *ay_activity_window::vbox=NULL;
int ay_activity_window::last_tag=0;
int ay_activity_window::timeout=0;

/* Create the only window that will exist */
static ay_activity_window aw;

/*
 * methods for ay_activity_bar_pack
 */
ay_activity_bar_pack::ay_activity_bar_pack(const char *label,
		activity_bar_cancel_callback cancel_callback, void *userdata)
{
	this->cancel_callback = cancel_callback;
	this->userdata = userdata;

	this->lbl = gtk_label_new(label);
	gtk_label_set_justify(GTK_LABEL(this->lbl), GTK_JUSTIFY_LEFT);
	gtk_widget_show(this->lbl);

	this->hbox = gtk_hbox_new(FALSE, 0);

	this->pbar = gtk_progress_bar_new();
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pbar), 
			GTK_PROGRESS_LEFT_TO_RIGHT );
	gtk_progress_set_activity_mode(GTK_PROGRESS(pbar), TRUE);
	gtk_box_pack_start(GTK_BOX(this->hbox), this->pbar, TRUE, TRUE, 10);
	gtk_widget_show(this->pbar);

	if(cancel_callback) {
		this->cbtn = gtk_button_new_with_label(_("Cancel"));
		gtk_signal_connect(GTK_OBJECT(this->cbtn), "clicked",
				GTK_SIGNAL_FUNC(cancel_clicked), this);
		gtk_box_pack_start(GTK_BOX(this->hbox), this->cbtn, FALSE, FALSE, 10);
		gtk_widget_show(this->cbtn);
	} else {
		this->cbtn = NULL;
	}

	gtk_widget_show(this->hbox);

	this->tag = aw.add_pack(this);
}

void ay_activity_bar_pack::update_label(const char * label)
{
	gtk_label_set_text(GTK_LABEL(lbl), label);
}

void ay_activity_bar_pack::update_activity( void )
{
	float new_val = gtk_progress_get_value(GTK_PROGRESS(pbar)) + 3;
	GtkAdjustment *adj = GTK_PROGRESS(pbar)->adjustment;

	if (new_val > adj->upper)
		new_val = adj->lower;

	gtk_progress_set_value(GTK_PROGRESS(pbar), new_val);
}

void ay_activity_bar_pack::cancel( void )
{
	if(cancel_callback)
		cancel_callback(userdata);
}

int ay_activity_bar_pack::get_tag( void )
{
	return tag;
}

ay_activity_bar_pack::~ay_activity_bar_pack()
{
	gtk_widget_hide(hbox);
	gtk_widget_hide(lbl);

	gtk_widget_destroy(hbox);
	gtk_widget_destroy(lbl);

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
	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event", 
			GTK_SIGNAL_FUNC(delete_event), NULL);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
	vbox = gtk_vbox_new(TRUE, 0);

	gtk_container_add(GTK_CONTAINER(window), vbox);

	timeout = eb_timeout_add(50, update_activity, &packs);
}

void ay_activity_window::show( void )
{
	gtk_widget_show(vbox);
	gtk_widget_show(window);
}

int ay_activity_window::add_pack(ay_activity_bar_pack *abp)
{
	if(!window)
		create_window();

	packs = l_list_prepend(packs, abp);
	gtk_box_pack_start(GTK_BOX(vbox), abp->lbl, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), abp->hbox, TRUE, TRUE, 0);

	if(! GTK_WIDGET_VISIBLE(GTK_WIDGET(window)) )
		show();

	return ++last_tag;
}

void ay_activity_window::remove_pack(ay_activity_bar_pack *abp)
{
	packs = l_list_remove(packs, abp);

	if(abp->tag == last_tag)
		last_tag--;

	if(!packs)
		close_window();
}

ay_activity_bar_pack * ay_activity_window::get_pack_by_tag(int tag)
{
	LList *l;
	for(l = packs; l; l = l_list_next(packs)) {
		ay_activity_bar_pack *abp = (ay_activity_bar_pack *)l->data;
		if(abp->tag == tag) {
			return abp;
		}
	}

	return NULL;
}

void ay_activity_window::close_window( void )
{
	if(timeout) {
		eb_timeout_remove(timeout);
		timeout = 0;
	}

	if(window) {
		gtk_widget_hide(window);
		gtk_widget_destroy(window);
		window = vbox = NULL;
	}

	while(packs) {
		ay_activity_bar_pack *abp = (ay_activity_bar_pack *)packs->data;
		abp->cancel();
		delete abp;
	}
}

/*
 * destroy the activity window and all data associated with it
 * will call cancel callbacks for all pending bars
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
static void cancel_clicked(GtkButton * btn, gpointer data)
{
	ay_activity_bar_pack *abp = (ay_activity_bar_pack *)data;

	/* btn == NULL means owner closed it with bar_remove */
	if(btn)
		abp->cancel();
	delete abp;
}

static gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer null)
{
	aw.close_window();
	return FALSE;
}

static int update_activity(void * data)
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

/*
 * Update the label associated with an activity bar
 * @tag: a tag that identifies the activity bar
 * @label: the text to be displayed on the label
 */
void ay_activity_bar_update_label(int tag, const char *label)
{
	ay_activity_bar_pack *abp = aw.get_pack_by_tag(tag);
	if(abp)
		abp->update_label(label);
}

/*
 * Remove an existing activity bar.  Destroy the window if this is the last
 * @tag: a tag that identifies the activity bar
 */
void ay_activity_bar_remove(int tag)
{
	ay_activity_bar_pack *abp = aw.get_pack_by_tag(tag);
	if(abp)
		cancel_clicked(NULL, abp);
}

/*
 * Add a new activity bar, create the window if this is the first
 * @label: The text to display against this activity bar
 * @cancel_callback: a function to be called if the user clicks cancel
 * @userdata: the callback data to be passed to cancel_callback
 *
 * Returns: a tag to identify this activity bar
 */
int ay_activity_bar_add(const char *label, 
		activity_bar_cancel_callback cancel_callback, void *userdata)
{
	ay_activity_bar_pack *abp = 
		new ay_activity_bar_pack(label, cancel_callback, userdata);

	return abp->get_tag();
}

#ifdef __cplusplus
}
#endif
