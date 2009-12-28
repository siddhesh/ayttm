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

#include "about.h"

#include "ui_about_window.h"

/* Ayttm developers */

#define	AYTTM_TEAM_SIZE	(7)

static tDeveloperInfo sAyttmDevTeam[AYTTM_TEAM_SIZE] = {
	{"Colin Leroy", "<colin@colino.net>", "Lead Developer"},
	{"Andy S. Maloney", "<a_s_maloney@yahoo.com>", "Code Monkey"},
	{"Philip S. Tellis", "<philip.tellis@gmail.com>", "Yahoo!"},
	{"Edward L. Haletky", "<elh@astroarch.com>", "Windows Port"},
	{"Tahir Hashmi", "<code_martial@softhome.net>", "Documentation"},
	{"Siddhesh Poyarekar", "<siddhesh.poyarekar@gmail.com>",
			"Gtk2 Port, IRC, MSN"},
	{"Piotr Stefaniak", "<2134918@gmail.com>", "UI, IRC and Jabber fixes"}
};

/* Kudos */

#define	KUDOS_SIZE	(6)

static tDeveloperInfo sKudosPeople[KUDOS_SIZE] = {
	{"Yann Marigo", "<yann@yannos.com>", "Art, Fr translation"},
	{"Ben Reser", "<ben@reser.org>", "Patches, MDK RPMs"},
	{"Lee Leahu", "<penguin365@dyweni.com>", "Patches"},
	{"Richard Ellis", "<rellis9@yahoo.com>", "Patches"},
	{"Paul Rhodes", "<withnail@users.sf.net>", "Patches"},
	{"Nicolas Peninguy", "<npml@free-anatole.net>", "Patches"}
};

/* Everybuddy developers */

#define	EVERYBUDDY_TEAM_SIZE	(11)

static tDeveloperInfo sEverybuddyDevTeam[EVERYBUDDY_TEAM_SIZE] = {
	{"Torrey Searle", "<tsearle@antihe.ro>", "Creator"},
	{"Ben Rigas", "<ben@flygroup.org>", "Web Design & GUI Devel"},
	{"Jared Peterson", "<jared@tgflinux.com>", "GUI Devel"},
	{"Alex Wheeler", "<awheeler@speakeasy.org>",
			"Jabber Devel & Much More"},
	{"Robert Lazzurs", "<lazzurs@lazzurs.myftp.org>", "Maintainer"},
	{"Meredydd", "<m_luff@mail.wincoll.ac.uk>", "MSN Devel"},
	{"Erik Inge Bolso", "<knan@mo.himolde.no>", "IRC Devel"},
	{"Colin Leroy", "<colin@colino.net>", "Various hacks, i18n"},
	{"Philip Tellis", "<philip.tellis@iname.com>", "Yahoo!"},
	{"Toby A. Inkster", "<tobyink@goddamn.co.uk>", "Patches"},
	{"Mark Spencer", "<no email>", "Special Thanks"}
};

void ay_show_about(void)
{
	tAboutInfo the_info;

	the_info.m_version = PACKAGE_STRING "-" RELEASE "\n" __DATE__;

	the_info.m_ay_developers = sAyttmDevTeam;
	the_info.m_num_ay_developers = AYTTM_TEAM_SIZE;

	the_info.m_ay_kudos = sKudosPeople;
	the_info.m_num_ay_kudos = KUDOS_SIZE;

	the_info.m_eb_developers = sEverybuddyDevTeam;
	the_info.m_num_eb_developers = EVERYBUDDY_TEAM_SIZE;

	ay_ui_about_window_create(&the_info);
}
