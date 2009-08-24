
/*
 * GtkTreeViewTooltip: Tooltips for rows of a tree view
 *
 * Copyright (C) 2007, Siddhesh Poyarekar <siddhesh.poyarekar@gmail.com>
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

#include "gtk_tree_view_tooltip.h"
#include <gtk/gtk.h>

/* Initialize the GtkTreeViewTooltipClass */
static void gtk_tree_view_tooltip_class_init(GtkTreeViewTooltipClass *tooltipclass)
{
	return;
}

/* Initialize a GtkTreeViewTooltip instance */
void gtk_tree_view_tooltip_init(GtkTreeViewTooltip *tooltip)
{
	tooltip->window			= NULL;
	tooltip->tip_icon		= NULL;
	tooltip->tip_title_label	= NULL;
	tooltip->tip_tiptext_label	= NULL;

	tooltip->tip_flag_column	= -1;
	tooltip->icon_column		= -1;
	tooltip->title_column		= -1;
	tooltip->tiptext_column		= -1;
	tooltip->source			=  0;

	tooltip->treeview	= NULL;
	tooltip->active_path	= NULL;
	tooltip->enable_tips	= FALSE;
}

/* Closes the tooltip window */
void gtk_tree_view_tooltip_reset(GtkTreeViewTooltip *tooltip)
{
	g_return_if_fail(tooltip!=NULL);

	if(tooltip->source) {
		g_source_remove(tooltip->source);
		g_free(tooltip->data);
		tooltip->source = 0;
	}

	if(tooltip->window && GTK_WIDGET_VISIBLE(tooltip->window))
		gtk_widget_destroy(tooltip->window);

	tooltip->window			= NULL;
	tooltip->tip_icon		= NULL;
	tooltip->tip_title_label	= NULL;
	tooltip->tip_tiptext_label	= NULL;

	tooltip->enable_tips		= FALSE;
}

/* Shows the tooltip window */
typedef struct {
	GtkTreeViewTooltip *tooltip;
	int x;
	int y;
} CoordData;

static gboolean tooltip_show_tip(void *d)
{
	GtkWidget *hbox = NULL;
	GtkWidget *vbox = NULL;
	GtkWidget *iconwid = NULL;
	CoordData *data = d;
	GtkTreeViewTooltip *tooltip = data->tooltip;
	int x = data->x;
	int y = data->y;

	g_free(data);

	/* We don't need to re-create it if it already exists, just update it in that case */
	if(!tooltip->window) {
		hbox = gtk_hbox_new(FALSE, 0);
		vbox = gtk_vbox_new(FALSE, 0);
	
		iconwid = gtk_image_new_from_pixbuf(tooltip->tip_icon);
		gtk_widget_show(iconwid);
	
		gtk_box_pack_start(GTK_BOX(vbox), tooltip->tip_title_label, TRUE, FALSE, 2);
		gtk_box_pack_end(GTK_BOX(vbox), tooltip->tip_tiptext_label, TRUE, FALSE, 2);
		
		gtk_widget_show(tooltip->tip_title_label);
		gtk_widget_show(tooltip->tip_tiptext_label);
		gtk_widget_show(vbox);
		
		gtk_box_pack_start(GTK_BOX(hbox), iconwid, FALSE, FALSE, 5);
		gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 5);
		gtk_widget_show(hbox);

		tooltip->window = gtk_window_new(GTK_WINDOW_POPUP);
		gtk_window_set_type_hint(GTK_WINDOW(tooltip->window), GDK_WINDOW_TYPE_HINT_TOOLTIP);
		gtk_container_add(GTK_CONTAINER(tooltip->window), hbox);
		gtk_container_set_border_width(GTK_CONTAINER(tooltip->window), 5);
		gtk_window_set_resizable(GTK_WINDOW(tooltip->window), FALSE);
	}
	gtk_window_move(GTK_WINDOW(tooltip->window), x, y);

	gtk_widget_show(tooltip->window);

	tooltip->source = 0;

	return FALSE;
}

/* All the funny stuff happens here... showing and hiding of the tooltip and all that */
static void treeview_tooltip_event_handler(GtkTreeView *treeview, GdkEvent *event, gpointer data)
{
	GtkTreeViewTooltip *tooltip = GTK_TREE_VIEW_TOOLTIP(data);
	
	if((tooltip->icon_column == -1 && tooltip->title_column == -1 && tooltip->tiptext_column == -1))
		return;
	
	switch(event->type) {
	case GDK_LEAVE_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_PROXIMITY_IN:
	case GDK_SCROLL:
	case GDK_DRAG_MOTION:
		gtk_tree_view_tooltip_reset(tooltip);
		return;
	case GDK_MOTION_NOTIFY:
	{	
		GtkTreePath *path;
		gchar *title = NULL;
		GdkPixbuf *icon = NULL;
		gchar *tiptext = NULL;
		GdkRectangle rect;
		int x, y, wx, wy;
		gboolean tips_enabled;
		CoordData *cdata = NULL;
		
		GtkTreeIter iter;
		GtkTreeModel *model = gtk_tree_view_get_model(treeview);
		GdkEventMotion *eventmotion = (GdkEventMotion *)event;
		
		/* Are we over a valid row at all? */
		if(!gtk_tree_view_get_path_at_pos(treeview,
						  eventmotion->x, eventmotion->y,
						  &path, NULL,
						  NULL, NULL))
		{
			return;
		}
		
		/* Have we moved on to the next row? */
		if(tooltip->active_path &&
		   !gtk_tree_path_compare(tooltip->active_path, path))
		{
			return;
		}
		
		gtk_tree_view_tooltip_reset(tooltip);

		gtk_tree_path_free(tooltip->active_path);
		tooltip->active_path = gtk_tree_path_copy(path);
		gtk_tree_path_free(path);
		
		gtk_tree_model_get_iter(model, &iter, tooltip->active_path);
		if(tooltip->tip_flag_column!=-1) {
			gtk_tree_model_get(model, &iter,
					   tooltip->tip_flag_column, &tips_enabled, -1);
			
			if(!tips_enabled)
				return;
		}
		if(tooltip->icon_column!=-1) {
			gtk_tree_model_get(model, &iter,
					   tooltip->icon_column, &icon, -1);
			tooltip->tip_icon = icon;
		}
		if(tooltip->title_column!=-1) {
			gchar *markup;

			gtk_tree_model_get(model, &iter,
					   tooltip->title_column, &title, -1);
			
			if(!tooltip->tip_title_label)
				tooltip->tip_title_label = gtk_label_new(NULL);
			
			markup = g_markup_printf_escaped("<span size=\'large\'><b>%s</b></span>", title);
			gtk_label_set_markup(GTK_LABEL(tooltip->tip_title_label), markup);
		}
		if(tooltip->tiptext_column!=-1) {
			gtk_tree_model_get(model, &iter,
					   tooltip->tiptext_column, &tiptext, -1);
			
			if(!tooltip->tip_tiptext_label) {
				tooltip->tip_tiptext_label = gtk_label_new(NULL);
				gtk_label_set_line_wrap(GTK_LABEL(tooltip->tip_tiptext_label), TRUE);
			}
			
			gtk_label_set_markup(GTK_LABEL(tooltip->tip_tiptext_label), tiptext);
		}
		
		/* Now to get the coordinates of the popup */
		gtk_tree_view_get_cell_area(treeview, tooltip->active_path, NULL, &rect);
		gtk_tree_view_tree_to_widget_coords(treeview, rect.x, rect.y, &wx, &wy);
		
		y = eventmotion->y_root + rect.height - eventmotion->y + rect.y;
		x = eventmotion->x_root - eventmotion->x + rect.width/2;

		cdata = g_new0(CoordData, 1);
		cdata->tooltip = tooltip;
		cdata->x = x;
		cdata->y = y;

		tooltip->data = cdata;
		
		tooltip->source = g_timeout_add(1000, tooltip_show_tip, cdata);
		return;
	}

	default:
		return;
	}
}

/* Unset the tooltip */
static void treeview_tooltip_unmap(GtkWidget *widget, gpointer data)
{
	GtkTreeViewTooltip *tooltip = GTK_TREE_VIEW_TOOLTIP(data);
	gtk_tree_view_tooltip_reset(tooltip);
}


/********* Public Functions **********/

GType gtk_tree_view_tooltip_get_type (void)
{
	static GType treeview_tooltip_type = 0;

	if( !treeview_tooltip_type ) {
		static const GTypeInfo treeview_tooltip_info = 
		{
			sizeof(GtkTreeViewTooltipClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_tree_view_tooltip_class_init,
			NULL,
			NULL,
			sizeof(GtkTreeViewTooltip),
			0,
			(GInstanceInitFunc) gtk_tree_view_tooltip_init
		};

		treeview_tooltip_type = g_type_register_static(G_TYPE_OBJECT,
								"GtkTreeViewTooltip",
								&treeview_tooltip_info,
								0);
	}

	return treeview_tooltip_type;
}



GtkTreeViewTooltip *gtk_tree_view_tooltip_new(void)
{
	return GTK_TREE_VIEW_TOOLTIP( g_object_new(GTK_TREE_VIEW_TOOLTIP_TYPE, NULL) );
}



GtkTreeViewTooltip *gtk_tree_view_tooltip_new_for_treeview(GtkTreeView *treeview)
{
	GtkTreeViewTooltip *tooltip = gtk_tree_view_tooltip_new();
	gtk_tree_view_tooltip_set_treeview(tooltip, treeview);

	return tooltip;
}



void gtk_tree_view_tooltip_set_treeview(GtkTreeViewTooltip *tooltip, GtkTreeView *treeview)
{
	g_return_if_fail(tooltip!=NULL);

	tooltip->treeview = treeview;

	if(!treeview) 
		return;

	g_signal_connect_after(treeview, "event-after", G_CALLBACK(treeview_tooltip_event_handler), tooltip);
	g_signal_connect(treeview, "unmap", G_CALLBACK(treeview_tooltip_unmap), tooltip);
	g_signal_connect(treeview, "unrealize", G_CALLBACK(treeview_tooltip_unmap), tooltip);
	g_signal_connect(treeview, "destroy", G_CALLBACK(treeview_tooltip_unmap), tooltip);
}



void gtk_tree_view_tooltip_set_tip_columns(GtkTreeViewTooltip *tooltip, gint tip_flag_col, gint icon_col,
		gint title_col, gint tiptext_col)
{
	tooltip->tip_flag_column = tip_flag_col;
	tooltip->icon_column = icon_col;
	tooltip->title_column = title_col;
	tooltip->tiptext_column = tiptext_col;
}



gint gtk_tree_view_tooltip_get_tip_icon_column(GtkTreeViewTooltip *tooltip)
{
	return tooltip->icon_column;
}



gint gtk_tree_view_tooltip_get_tip_title_column(GtkTreeViewTooltip *tooltip)
{
	return tooltip->title_column;
}



gint gtk_tree_view_tooltip_get_tip_text_column(GtkTreeViewTooltip *tooltip)
{
	return tooltip->tiptext_column;
}



gint gtk_tree_view_tooltip_get_tip_flag_column(GtkTreeViewTooltip *tooltip)
{
	return tooltip->tip_flag_column;
}

