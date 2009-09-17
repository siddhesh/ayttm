/*
 * Ayttm
 *
 * Code from the GTK+ Reference Manual: Migrating from GnomeUIInfo
 * (http://www.gtk.org/api/2.6/gtk/migrating-gnomeuiinfo.html)
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
 * icons.c
 *
 */

#include "gtk_globals.h"

static struct {
	char *filename;
	char *stock_id;
} ay_stock_icons[] = {
	{
	IMG_DIR "buddy.png", "ayttm_contact"}, {
	IMG_DIR "group.png", "ayttm_group"}, {
	IMG_DIR "away.png", "ayttm_away"}, {
	IMG_DIR "group-chat.png", "ayttm_group_chat"}, {
	IMG_DIR "smileys.png", "ayttm_smileys"}
};

static gint n_stock_icons = G_N_ELEMENTS(ay_stock_icons);

void ay_register_icons(void)
{
	GtkIconFactory *icon_factory;
	GtkIconSet *icon_set;
	GtkIconSource *icon_source;

	int i;

	icon_factory = gtk_icon_factory_new();

	for (i = 0; i < n_stock_icons; i++) {
		icon_set = gtk_icon_set_new();
		icon_source = gtk_icon_source_new();

		gtk_icon_source_set_filename(icon_source,
			ay_stock_icons[i].filename);
		gtk_icon_set_add_source(icon_set, icon_source);
		gtk_icon_source_free(icon_source);

		gtk_icon_factory_add(icon_factory, ay_stock_icons[i].stock_id,
			icon_set);
		gtk_icon_set_unref(icon_set);
	}

	gtk_icon_factory_add_default(icon_factory);
	g_object_unref(icon_factory);
}
