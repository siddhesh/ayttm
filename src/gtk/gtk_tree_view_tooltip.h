
/*
 * GtkTreeViewTooltip: Tooltips for rows of a tree view
 *
 * Copyright (C) 2007, Siddhesh Poyarekar <siddhesh.poyarekar@gmail.com>
 *
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

#ifndef __GTK_TREE_VIEW_TOOLTIP__
#define __GTK_TREE_VIEW_TOOLTIP__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkwidget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS
#define GTK_TREE_VIEW_TOOLTIP_TYPE		(gtk_tree_view_tooltip_get_type ())
#define GTK_TREE_VIEW_TOOLTIP(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TREE_VIEW_TOOLTIP_TYPE, GtkTreeViewTooltip))
#define GTK_TREE_VIEW_TOOLTIP_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST ((cls), GTK_TREE_VIEW_TOOLTIP_TYPE, GtkTreeViewTooltipClass))
#define GTK_IS_TREE_VIEW_TOOLTIP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TREE_VIEW_TOOLTIP_TYPE))
#define GTK_IS_TREE_VIEW_TOOLTIP_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE ((cls), GTK_TREE_VIEW_TOOLTIP_TYPE))
typedef struct _GtkTreeViewTooltip GtkTreeViewTooltip;
typedef struct _GtkTreeViewTooltipClass GtkTreeViewTooltipClass;

struct _GtkTreeViewTooltip {
	GObject parent;

	GtkWidget *window;
	GdkPixbuf *tip_icon;
	GtkWidget *tip_title_label;
	GtkWidget *tip_tiptext_label;

	GtkTreeView *treeview;
	GtkTreePath *active_path;

	gint tip_flag_column;
	gint icon_column;
	gint title_column;
	gint tiptext_column;
	gboolean enable_tips;

	gint source;
	gpointer data;
};

struct _GtkTreeViewTooltipClass {
	GObjectClass parent_class;
};

/*
 * Get the type number of GtkTreeViewTooltip as registered with GLib
 */
GType gtk_tree_view_tooltip_get_type();

/*
 * Create a new GtkTreeViewTooltip
 */
GtkTreeViewTooltip *gtk_tree_view_tooltip_new(void);

/*
 * Create a new GtkTreeViewTooltip for a given tree view object
 * 
 * Arguments:
 * @ treeview:	The GtkTreeView to which the newly created tooltip must attach
 */
GtkTreeViewTooltip *gtk_tree_view_tooltip_new_for_treeview(GtkTreeView
	*treeview);

/*
 * Attach the tooltip object to a tree view
 *
 * Arguments
 * @ tooltip:	The GtkTreeViewTooltip object
 * @ treeview:	The GtkTreeView object to which tooltip must attach
 */
void gtk_tree_view_tooltip_set_treeview(GtkTreeViewTooltip *tooltip,
	GtkTreeView *treeview);

/*
 * Set the columns on the tree model from which the tooltip should read its data.
 * Set whichever column you don't want as -1
 *
 * Arguments:
 * @ tooltip:		The concerned tooltip
 * @ tip_flag_col: 	Column number for a boolean specifier which denotes whether the row should have tooltips
 * @ icon_col: 		Column number for the icon pixbuf
 * @ title_col:		Column number for the bold title of the tip
 * @ tiptext_col:	Column number for the detailed tip
 */
void gtk_tree_view_tooltip_set_tip_columns(GtkTreeViewTooltip *tooltip,
	gint tip_flag_col, gint icon_col, gint title_col, gint tiptext_col);

/*
 * Returns the column number for the tip flag
 */
gint gtk_tree_view_tooltip_get_tip_flag_column(GtkTreeViewTooltip *tooltip);

/*
 * Returns the column number for the tip icon
 */
gint gtk_tree_view_tooltip_get_tip_icon_column(GtkTreeViewTooltip *tooltip);

/*
 * Returns the column number for the tip title
 */
gint gtk_tree_view_tooltip_get_tip_title_column(GtkTreeViewTooltip *tooltip);

/*
 * Returns the column number for the tip text
 */
gint gtk_tree_view_tooltip_get_tip_text_column(GtkTreeViewTooltip *tooltip);

G_END_DECLS
#endif				/*End __GTK_TREE_VIEW_TOOLTIP__ */
