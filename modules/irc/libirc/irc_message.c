
/*
 * IRC protocol support for Ayttm
 *
 * Copyright (C) 2008, Siddhesh Poyarekar and the Ayttm Team
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

#include <string.h>
#include "libirc.h"
#include "irc_replies.h"

/* 
 * RFC1459 and RFC2812 defines max message size
 * including \n == 512 bytes, but being tolerant of
 * violations is nice nevertheless. 
 */
#define MAX_MESSAGE_LEN 512*2

#define CB_EXEC(cb,func)  if(cb->func) cb->func

int get_command_num(char *command)
{
	if (command && *command) {
		if (!strncmp(command, "NOTICE", 6))
			return IRC_CMD_NOTICE;
		else if (!strncmp(command, "QUIT", 4))
			return IRC_CMD_QUIT;
		else if (!strncmp(command, "JOIN", 4))
			return IRC_CMD_JOIN;
		else if (!strncmp(command, "PART", 4))
			return IRC_CMD_PART;
		else if (!strncmp(command, "MODE", 4))
			return IRC_CMD_MODE;
		else if (!strncmp(command, "INVITE", 6))
			return IRC_CMD_INVITE;
		else if (!strncmp(command, "KICK", 4))
			return IRC_CMD_KICK;
		else if (!strncmp(command, "NICK", 4))
			return IRC_CMD_NICK;
		else if (!strncmp(command, "PRIVMSG", 7))
			return IRC_CMD_PRIVMSG;
		else if (!strncmp(command, "KILL", 4))
			return IRC_CMD_KILL;
		else if (!strncmp(command, "PING", 4))
			return IRC_CMD_PING;
		else if (!strncmp(command, "ERROR", 5))
			return IRC_CMD_ERROR;
	}

	return 0;
}

/* 
 * Any message in IRC is of the form: 
 *      [:<prefix>] SPACE <command> SPACE <parameters> CRLF 
 * We simply split the message into prefix, command and parameters 
 * and then, based on commands, perform actions
 */
void irc_message_parse(char *incoming, irc_account *ia)
{
	int message_len;
	char *prefix_offset = NULL;
	char *user_offset = NULL;
	char *nick_offset = NULL;
	char *host_offset = NULL;
	char *servername_offset = NULL;
	char *command_offset = NULL;
	char *message_offset = NULL;
	char *params_offset = NULL;
	char *next_param_offset = NULL;

	int command_num = 0;

	irc_param_list *params = NULL;
	irc_message_prefix *prefix =
		(irc_message_prefix *)calloc(1, sizeof(irc_message_prefix));

	message_len = strlen(incoming);

	if (message_len > MAX_MESSAGE_LEN - 1)
		return;

	/* 
	 * First, get the trailing message out of the way, 
	 * then we extract everything else 
	 */
	if ((message_offset = strstr(incoming, " :"))) {
		*message_offset = '\0';
		message_offset += 2;
	}

	if (incoming[0] == ':') {
		/* separate the command from the prefix */
		command_offset = strchr(incoming, ' ');

		/* Malformed message. send a rejection */
		if (!command_offset) {
			CB_EXEC(ia->callbacks,
				irc_warning) (_
				("Malformed message from server"), ia->data);

			return;
		}

		*command_offset = '\0';
		command_offset++;

		/* We have a prefix... so parse it. */
		prefix_offset = &incoming[1];

		user_offset = strchr(prefix_offset, '!');
		if (user_offset) {
			*user_offset = '\0';
			user_offset++;

			host_offset = strchr(user_offset, '@');
		} else
			host_offset = strchr(prefix_offset, '@');

		if (host_offset) {
			*host_offset = '\0';
			host_offset++;
		}

		/* 
		 * If there's no user and nick, we assume it is a nickname 
		 * unless the name contains a dot 
		 */
		if (!user_offset && !host_offset && strchr(prefix_offset, '.'))
			servername_offset = prefix_offset;
		else
			nick_offset = prefix_offset;

		/* We've found the command, so now we can store the prefix data. */
		if (user_offset)
			prefix->user = strdup(user_offset);
		if (host_offset)
			prefix->hostname = strdup(host_offset);
		if (servername_offset)
			prefix->servername = strdup(servername_offset);
		if (nick_offset)
			prefix->nick = strdup(nick_offset);
	} else
		command_offset = incoming;

	params_offset = strchr(command_offset, ' ');
	if (params_offset) {
		*params_offset = '\0';
		params_offset++;
	}

	while (params_offset
		&& (next_param_offset = strchr(params_offset, ' '))) {
		*next_param_offset = '\0';
		params = irc_param_list_add(params, params_offset);
		params_offset = next_param_offset + 1;
	}

	if (params_offset)
		params = irc_param_list_add(params, params_offset);

	/* Add the trailing message as the last parameter */
	if (message_offset)
		params = irc_param_list_add(params, message_offset);

	/* 
	 * Done, now lets execute the command.
	 * If prefix offset is present, use atoi() to get command number.
	 * Use get_command_num() if atoi() fails or if there is no prefix_offset.
	 */
	if (!prefix_offset || !(command_num = atoi(command_offset)))
		command_num = get_command_num(command_offset);

	switch (command_num) {
		/* This is actually supposed to be a server-to-server ERROR, 
		 * but some servers like to send it to clients as well 
		 */
	case IRC_CMD_ERROR:
		CB_EXEC(ia->callbacks, irc_error) (irc_param_list_get_at(params,
				0), ia->data);
		break;
	case IRC_CMD_NOTICE:
		irc_process_notice(irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1), prefix, ia);
		break;
	case IRC_CMD_QUIT:
		CB_EXEC(ia->callbacks,
			buddy_quit) (irc_param_list_get_at(params, 0), prefix,
			ia);
		break;
	case IRC_CMD_JOIN:
		CB_EXEC(ia->callbacks,
			buddy_join) (irc_param_list_get_at(params, 0), prefix,
			ia);
		break;
	case IRC_CMD_PART:
		CB_EXEC(ia->callbacks,
			buddy_part) (irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1), prefix, ia);
		break;
	case IRC_CMD_MODE:
		break;
	case IRC_CMD_INVITE:
		CB_EXEC(ia->callbacks,
			got_invite) (irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1), prefix, ia);
		break;
	case IRC_CMD_KICK:
		CB_EXEC(ia->callbacks, got_kick) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case IRC_CMD_NICK:
		CB_EXEC(ia->callbacks,
			nick_change) (irc_param_list_get_at(params, 0), prefix,
			ia);
		break;
	case IRC_CMD_PRIVMSG:
		irc_process_privmsg(irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1), prefix, ia);
		break;
	case IRC_CMD_KILL:
		break;
	case IRC_CMD_PING:
		CB_EXEC(ia->callbacks, got_ping) (irc_param_list_get_at(params,
				0), ia);
		break;
	case RPL_WELCOME:
		CB_EXEC(ia->callbacks,
			got_welcome) (irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1), prefix, ia);
		break;
	case RPL_YOURHOST:
		CB_EXEC(ia->callbacks,
			got_yourhost) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case RPL_CREATED:
		CB_EXEC(ia->callbacks,
			got_created) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case RPL_MYINFO:
		ia->usermodes = irc_param_list_get_at(params, 3);
		ia->channelmodes = irc_param_list_get_at(params, 4);
		ia->servername = irc_param_list_get_at(params, 1);
		CB_EXEC(ia->callbacks,
			got_myinfo) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_BOUNCE:	/* Not implemented yet since this single message seems to do multiple unrelated things */
		break;
	case RPL_AWAY:
		CB_EXEC(ia->callbacks, got_away) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 2), prefix,
			ia);
		break;
	case RPL_USERHOST:
		break;
	case RPL_ISON:
		CB_EXEC(ia->callbacks, got_ison) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_UNAWAY:
		CB_EXEC(ia->callbacks,
			got_unaway) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case RPL_NOWAWAY:
		CB_EXEC(ia->callbacks,
			got_nowaway) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case RPL_WHOISUSER:
		CB_EXEC(ia->callbacks,
			got_whoisuser) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3),
			irc_param_list_get_at(params, 5), prefix, ia);
		break;
	case RPL_WHOISSERVER:
		CB_EXEC(ia->callbacks,
			got_whoisserver) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case RPL_WHOISOPERATOR:
		CB_EXEC(ia->callbacks,
			got_whoisop) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_WHOISIDLE:
		CB_EXEC(ia->callbacks,
			got_whoisidle) (irc_param_list_get_at(params, 1),
			atoi(irc_param_list_get_at(params, 2)),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case RPL_ENDOFWHOIS:
		CB_EXEC(ia->callbacks,
			got_endofwhois) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_WHOISCHANNELS:
		break;
	case RPL_WHOWASUSER:
		CB_EXEC(ia->callbacks,
			got_whowasuser) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3),
			irc_param_list_get_at(params, 5), prefix, ia);
		break;
	case RPL_ENDOFWHOWAS:
		CB_EXEC(ia->callbacks,
			got_endofwhowas) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_LISTSTART:
		/* RFC 2812 says that this is obsolete */
		break;
	case RPL_LIST:
		CB_EXEC(ia->callbacks,
			got_channel_list) (irc_param_list_get_at(params, 0),
			irc_param_list_get_at(params, 1),
			atoi(irc_param_list_get_at(params, 2)),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case RPL_LISTEND:
		CB_EXEC(ia->callbacks,
			got_channel_listend) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case RPL_CHANNELMODEIS:
		break;
	case RPL_NOTOPIC:
		CB_EXEC(ia->callbacks,
			got_notopic) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_TOPIC:
		CB_EXEC(ia->callbacks, got_topic) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 2), prefix,
			ia);
		break;
	case RPL_TOPICSETBY:
		CB_EXEC(ia->callbacks,
			got_topicsetby) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case RPL_INVITING:
		CB_EXEC(ia->callbacks, inviting) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 2), prefix,
			ia);
		break;
	case RPL_SUMMONING:
		CB_EXEC(ia->callbacks, summoning) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 2), prefix,
			ia);
		break;
	case RPL_VERSION:
		{
			char *debuglevel;

			debuglevel =
				strrchr(irc_param_list_get_at(params, 1), '.');
			*debuglevel = '\0';
			debuglevel++;

			CB_EXEC(ia->callbacks,
				got_version) (irc_param_list_get_at(params, 1),
				debuglevel, irc_param_list_get_at(params, 2),
				irc_param_list_get_at(params, 3), prefix, ia);
			break;
		}
	case RPL_WHOREPLY:
		break;
	case RPL_ENDOFWHO:
		CB_EXEC(ia->callbacks,
			got_endofwho) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_NAMEREPLY:
		{
			irc_name_list *list =
				irc_gen_name_list(irc_param_list_get_at(params,
					3));

			CB_EXEC(ia->callbacks, got_namelist) (list,
				irc_param_list_get_at(params, 2), prefix, ia);
			break;
		}
	case RPL_ENDOFNAMES:
		CB_EXEC(ia->callbacks,
			got_endofnames) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case RPL_LINKS:
		break;
	case RPL_ENDOFLINKS:
		break;
	case RPL_BANLIST:
		break;
	case RPL_ENDOFBANLIST:
		break;
	case RPL_INFO:
		CB_EXEC(ia->callbacks, got_info) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_ENDOFINFO:
		CB_EXEC(ia->callbacks, got_info) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_MOTDSTART:
		{
			char *motd = NULL;
			char *server = NULL;

			server = strchr(irc_param_list_get_at(params, 1), ' ');
			server++;
			motd = strchr(server, ' ');
			*motd = '\0';
			motd++;
			CB_EXEC(ia->callbacks, motd_start) (server,
				motd, prefix, ia);
			break;
		}
	case RPL_MOTD:
		{
			char *motd =
				strchr(irc_param_list_get_at(params, 1), ' ');
			*motd = '\0';
			motd++;
			CB_EXEC(ia->callbacks, got_motd) (motd, prefix, ia);
			break;
		}
	case RPL_ENDOFMOTD:
		CB_EXEC(ia->callbacks, endofmotd) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_YOUREOPER:
		CB_EXEC(ia->callbacks, youreoper) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_REHASHING:
		break;
	case RPL_TIME:
		CB_EXEC(ia->callbacks, got_time) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case RPL_USERSSTART:
		break;
	case RPL_USERS:
		break;
	case RPL_ENDOFUSERS:
		break;
	case RPL_NOUSERS:
		break;
	case RPL_TRACELINK:
		break;
	case RPL_TRACECONNECTING:
		break;
	case RPL_TRACEHANDSHAKE:
		break;
	case RPL_TRACEUNKNOWN:
		break;
	case RPL_TRACEOPERATOR:
		break;
	case RPL_TRACEUSER:
		break;
	case RPL_TRACESERVER:
		break;
	case RPL_TRACENEWTYPE:
		break;
	case RPL_TRACELOG:
		break;
	case RPL_STATSLINKINFO:
		break;
	case RPL_STATSCOMMANDS:
		break;
	case RPL_STATSCLINE:
		break;
	case RPL_STATSNLINE:
		break;
	case RPL_STATSILINE:
		break;
	case RPL_STATSKLINE:
		break;
	case RPL_STATSYLINE:
		break;
	case RPL_ENDOFSTATS:
		break;
	case RPL_STATSLLINE:
		break;
	case RPL_STATSUPTIME:
		break;
	case RPL_STATSOLINE:
		break;
	case RPL_STATSHLINE:
		break;
	case RPL_UMODEIS:
		break;
	case RPL_LUSERCLIENT:
		break;
	case RPL_LUSEROP:
		break;
	case RPL_LUSERUNKNOWN:
		break;
	case RPL_LUSERCHANNELS:
		break;
	case RPL_LUSERME:
		break;
	case RPL_ADMINME:
		break;
	case RPL_ADMINLOC1:
		break;
	case RPL_ADMINLOC2:
		break;
	case RPL_ADMINEMAIL:
		break;
	case ERR_NOSUCHNICK:
		CB_EXEC(ia->callbacks,
			irc_no_such_nick) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NOSUCHSERVER:
		CB_EXEC(ia->callbacks,
			irc_no_such_server) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NOSUCHCHANNEL:
		CB_EXEC(ia->callbacks,
			irc_no_such_channel) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_CANNOTSENDTOCHAN:
		CB_EXEC(ia->callbacks,
			irc_cant_sendto_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_TOOMANYCHANNELS:
		CB_EXEC(ia->callbacks,
			irc_too_many_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_WASNOSUCHNICK:
		CB_EXEC(ia->callbacks,
			irc_was_no_such_nick) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_TOOMANYTARGETS:
		CB_EXEC(ia->callbacks,
			irc_too_many_targets) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NOORIGIN:
		CB_EXEC(ia->callbacks,
			irc_no_origin) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NORECIPIENT:
		CB_EXEC(ia->callbacks,
			irc_no_recipient) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NOTEXTTOSEND:
		CB_EXEC(ia->callbacks,
			irc_no_text_to_send) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NOTOPLEVEL:
		CB_EXEC(ia->callbacks,
			irc_no_toplevel) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_WILDTOPLEVEL:
		CB_EXEC(ia->callbacks,
			irc_wild_toplevel) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_UNKNOWNCOMMAND:
		CB_EXEC(ia->callbacks,
			irc_unknown_cmd) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NOMOTD:
		CB_EXEC(ia->callbacks,
			irc_no_motd) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case ERR_NOADMININFO:
		CB_EXEC(ia->callbacks,
			irc_no_admininfo) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_FILEERROR:
		CB_EXEC(ia->callbacks,
			irc_file_error) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NONICKNAMEGIVEN:
		CB_EXEC(ia->callbacks,
			irc_no_nick_given) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_ERRONEUSNICKNAME:
		CB_EXEC(ia->callbacks,
			irc_erroneous_nick) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NICKNAMEINUSE:
		CB_EXEC(ia->callbacks,
			irc_nick_in_use) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NICKCOLLISION:
		CB_EXEC(ia->callbacks,
			irc_nick_collision) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_USERNOTINCHANNEL:
		CB_EXEC(ia->callbacks,
			irc_user_not_in_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case ERR_NOTONCHANNEL:
		CB_EXEC(ia->callbacks,
			irc_not_on_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_USERONCHANNEL:
		CB_EXEC(ia->callbacks,
			irc_on_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2),
			irc_param_list_get_at(params, 3), prefix, ia);
		break;
	case ERR_NOLOGIN:
		CB_EXEC(ia->callbacks,
			irc_nologin) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_SUMMONDISABLED:
		CB_EXEC(ia->callbacks,
			irc_summon_disabled) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_USERSDISABLED:
		CB_EXEC(ia->callbacks,
			irc_user_disabled) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NOTREGISTERED:
		CB_EXEC(ia->callbacks,
			irc_not_registered) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NEEDMOREPARAMS:
		CB_EXEC(ia->callbacks,
			irc_need_more_params) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_ALREADYREGISTRED:
		CB_EXEC(ia->callbacks,
			irc_already_registered) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case ERR_NOPERMFORHOST:
		CB_EXEC(ia->callbacks,
			irc_no_perm_for_host) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_PASSWDMISMATCH:
		CB_EXEC(ia->callbacks,
			irc_password_mismatch) (irc_param_list_get_at(params,
				1), prefix, ia);
		break;
	case ERR_YOUREBANNEDCREEP:
		CB_EXEC(ia->callbacks,
			irc_banned) (irc_param_list_get_at(params, 1), prefix,
			ia);
		break;
	case ERR_KEYSET:
		CB_EXEC(ia->callbacks,
			irc_chnl_key_set) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_CHANNELISFULL:
		CB_EXEC(ia->callbacks,
			irc_channel_full) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_UNKNOWNMODE:
		CB_EXEC(ia->callbacks,
			irc_unknown_mode) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_INVITEONLYCHAN:
		CB_EXEC(ia->callbacks,
			irc_invite_only) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_BANNEDFROMCHAN:
		CB_EXEC(ia->callbacks,
			irc_banned_from_chnl) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_BADCHANNELKEY:
		CB_EXEC(ia->callbacks,
			irc_bad_chnl_key) (irc_param_list_get_at(params, 1),
			irc_param_list_get_at(params, 2), prefix, ia);
		break;
	case ERR_NOPRIVILEGES:
		CB_EXEC(ia->callbacks,
			irc_no_privileges) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_CHANOPRIVSNEEDED:
		CB_EXEC(ia->callbacks,
			irc_chnlop_privs_needed) (irc_param_list_get_at(params,
				1), irc_param_list_get_at(params, 2), prefix,
			ia);
		break;
	case ERR_CANTKILLSERVER:
		CB_EXEC(ia->callbacks,
			irc_cant_kill_server) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_NOOPERHOST:
		CB_EXEC(ia->callbacks,
			irc_no_o_per_host) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_UMODEUNKNOWNFLAG:
		CB_EXEC(ia->callbacks,
			irc_mode_unknown) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
	case ERR_USERSDONTMATCH:
		CB_EXEC(ia->callbacks,
			irc_user_dont_match) (irc_param_list_get_at(params, 1),
			prefix, ia);
		break;
		/* Reserved Numerics... */
	case RPL_NONE:
		break;
	case RPL_TRACECLASS:
		break;
	case RPL_STATSQLINE:
		break;
	case RPL_SERVICEINFO:
		break;
	case RPL_ENDOFSERVICES:
		break;
	case RPL_SERVICE:
		break;
	case RPL_SERVLIST:
		break;
	case RPL_SERVLISTEND:
		break;
	case RPL_WHOISCHANOP:
		break;
	case RPL_KILLDONE:
		break;
	case RPL_CLOSING:
		break;
	case RPL_CLOSEEND:
		break;
	case RPL_INFOSTART:
		break;
	case RPL_MYPORTIS:
		break;
	case ERR_YOUWILLBEBANNED:
		break;
	case ERR_BADCHANMASK:
		break;
	case ERR_NOSERVICEHOST:
		break;
	default:
		break;
	}

	if (prefix) {
		if (prefix->user)
			free(prefix->user);
		if (prefix->hostname)
			free(prefix->hostname);
		if (prefix->servername)
			free(prefix->servername);
		if (prefix->nick)
			free(prefix->nick);
		free(prefix);
		prefix = NULL;
	}

	irc_param_list_free(params);
}

int irc_get_command_string(char *out, const char *recipient, char *command,
	char *params, irc_account *ia)
{
	if (!strcasecmp(command, "ME")) {
		snprintf(out, BUF_LEN, "PRIVMSG %s :\001ACTION %s\001\n",
			recipient, params);
		return IRC_ECHO_ACTION;
	}

	if (!strcasecmp(command, "LEAVE")) {
		strcpy(command, "PART");

		return IRC_NOECHO;
	}

	if (!strcasecmp(command, "J")) {
		snprintf(out, BUF_LEN, "JOIN %s\n", params);
		return IRC_NOECHO;
	}

	if (!strcasecmp(command, "JOIN")
		|| !strcasecmp(command, "PART")
		|| !strcasecmp(command, "AWAY")
		|| !strcasecmp(command, "UNAWAY")
		|| !strcasecmp(command, "WHOIS")
		|| !strcasecmp(command, "NICK")
		|| !strcasecmp(command, "JOIN")) {
		snprintf(out, BUF_LEN, "%s %s\n", command, params);
		return IRC_NOECHO;
	}

	if (!strcasecmp(command, "QUIT")) {
		out[0] = '\0';

		CB_EXEC(ia->callbacks, client_quit) (ia);
		return IRC_NOECHO;
	}

	if (!strcasecmp(command, "MSG")) {
		char *msg = strchr(params, ' ');

		if (msg) {
			*msg = '\0';
			msg++;
		}

		snprintf(out, BUF_LEN, "PRIVMSG %s :%s\n", params,
			(msg ? msg : ""));

		/* restore the space in params since client might need it */
		if (msg) {
			*(msg - 1) = ' ';
		}

		return IRC_NOECHO;
	}

	return IRC_NOECHO;
}
