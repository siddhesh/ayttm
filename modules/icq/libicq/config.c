/* Copyright (C) 1998 Sean Gabriel <gabriel@korsoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib.h>

#if defined( _WIN32 )
#include <direct.h>
#define mkdir( x, y ) _mkdir( x )
#define S_IRWXU 0
#else
#endif

#include "libicq.h"
#include "icqconfig.h"
#include "tcp.h"

extern int Verbose;

char icq_rc[100], contacts_rc[100];

int Get_Config_Info()
{
  char path[300];

#if 0
#warning "Crashes"
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Get_Config_Info");
#endif

#if defined( _WIN32 )
  strcpy( path, "C:" );
#else
  strcpy(path, getenv("HOME"));
#endif

  strcat(path, "/.icq");
  mkdir(path, S_IRWXU);

  strcpy(icq_rc, path);
  strcat(icq_rc, "/icq.rc");
  if(Verbose & ICQ_VERB_INFO)
    printf("ICQ.RC: %s\n", icq_rc);
  
  strcpy(contacts_rc, path);
  strcat(contacts_rc, "/contacts.rc");
  if(Verbose & ICQ_VERB_INFO)
    printf("CONTACTS.RC: %s\n", contacts_rc);

  if(!Read_ICQ_RC(icq_rc)) return 0;
  Read_Contacts_RC(contacts_rc);
  
  return 1;
}


int Read_ICQ_RC(char* filename)
{
  /* CfgVersion doesn't exist in older configuration files, but they should
    still be read fine.  It will add "Version 1" to them when saving them as
    Version 1 is considered the version with a version tag.  Enough of that. */

  int CfgVersion = 0;
  FILE* fp;
  char buf[100], c;

#if 0
#warning "Crashes!"
  ICQ_Debug(ICQ_VERB_INFO, "\nLIBICQ> Get_ICQ_RC");
#endif
  if(!(fp = fopen(filename, "rt")))
  {
    if(UIN == 0) return 0;
    
    set_status = STATUS_ONLINE;
    strcpy(server, "icq.mirabilis.com");
    remote_port = 4000;

    Write_ICQ_RC(filename);
    return 1;
  }
  
  while(!feof(fp))
  {
    c = fgetc(fp);
    if(c == '#')  /* Comment */
    {
      while(!feof(fp) && fgetc(fp) != '\n');
      continue;
    } 
    else if(c == '\n')  /* Blank line */
    { 
      continue;
    } 
    else 
    {
      ungetc(c, fp);
    }

    fscanf(fp, "%s ", buf);

    if(CfgVersion >= 0)
    {
      /* In all cases, check for the following: */
      if (!strcmp(buf, "Version")) fscanf(fp, "%d\n", &CfgVersion);
      else if(!strcmp(buf, "UIN")) fscanf(fp, "%d\n", &UIN);
      else if(!strcmp(buf, "Password")) fscanf(fp, "%s\n", passwd);
      else if(!strcmp(buf, "Status")) fscanf(fp, "%d\n", &set_status);
      else if(!strcmp(buf, "Server")) fscanf(fp, "%s\n", server);
      else if(!strcmp(buf, "Port")) fscanf(fp, "%d\n", &remote_port);
    }

    if(CfgVersion > 0)
    {  
      /* VER 1 This is the first version with the "Version" number */
      /* No features added with version 1 */
    }

    if(CfgVersion > 1)
    {  
      /* Put new config information for version 2 in here */
      /* if (!strcmp(buf, "Whatever")) ... */
    }
  }

  if(fclose(fp) != 0)
  {
    if(Verbose & ICQ_VERB_ERR)
      printf("\nfclose (%s) failed.\n", filename);
    return 0;
  }

  return 1;
}


void Write_ICQ_RC(char* filename)
{
  int CfgVersion = 1;  // Current version
  FILE* fp;

  ICQ_Debug(ICQ_VERB_INFO, "\nLIBICQ> Write_ICQ_RC");

  if(!(fp = fopen(filename, "wt")))
  {
	ICQ_Debug(ICQ_VERB_ERR, "LIBICQ> failed to open file.");
/*win32--#warning "FIXME: ... but we should return a value to know if we failed."*/
  return;
  }

  fprintf(fp, "# ICQ connection settings\n\n");
  fprintf(fp, "Version %d\n", CfgVersion);
  fprintf(fp, "UIN %d\n", UIN);
  fprintf(fp, "Password %s\n", passwd);
  fprintf(fp, "Status %d\n", set_status);
  fprintf(fp, "Server %s\n", server);
  fprintf(fp, "Port %d\n", remote_port);
  
  if (fclose (fp) != 0)
  {
    if(Verbose & ICQ_VERB_ERR)
      printf("\nfclose (%s) failed.\n", filename);
/*  Void Function ... but we should return a value to know if we failed.
    return PROBLEM!; */
  }
}
  

int Read_Contacts_RC(char* filename)
{
  FILE* fp;
  char buf[100], c;

  Num_Contacts = 0;

#if 0
#warning "Crashes!"
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Read_Contacts_RC");
#endif

  if(!(fp = fopen(filename, "rt"))) return 0;

  while(!feof(fp))
  {
    c = fgetc(fp);
    if(c == '#')
    {
      fgets(buf, 100, fp);
      continue;
    }
    else if(feof(fp))
      break;
    else if(c == '\n')
      continue;
    else
      ungetc(c, fp);

    fscanf(fp, "%d ", &Contacts[Num_Contacts].uin);
    fgets(buf, 100, fp);
    buf[strlen(buf) - 1] = '\0';
    strncpy(Contacts[Num_Contacts].nick, buf, 20);
    Contacts[Num_Contacts].status = STATUS_OFFLINE;
    Contacts[Num_Contacts].last_time = -1L;
    Contacts[Num_Contacts].current_ip = -1L;
    Contacts[Num_Contacts].tcp_sok = -1L;
    Contacts[Num_Contacts].tcp_port = 0;
    Contacts[Num_Contacts].tcp_status = TCP_NOT_CONNECTED;
    Contacts[Num_Contacts].chat_sok = -1L;
    Contacts[Num_Contacts].chat_port = 0;
    Contacts[Num_Contacts].chat_status = TCP_NOT_CONNECTED;
    Contacts[Num_Contacts].chat_active = FALSE;
    Contacts[Num_Contacts].chat_active2 = FALSE;
    Num_Contacts++;
  }
  
  if(fclose(fp) != 0)
  {
    if(Verbose & ICQ_VERB_ERR)
      printf("\nfclose (%s) failed.\n", filename);
    return 0;
  }
  
  return 1;
}


void Write_Contacts_RC(char* filename)
{
  FILE* fp;
  int x;
  
  ICQ_Debug(ICQ_VERB_INFO, "LIBICQ> Write_Contacts_RC");

  if(!(fp = fopen(filename, "wt"))) return;
  
  fprintf(fp, "# ICQ contact list\n\n");

  for(x = 0; x < Num_Contacts; x++)
    if((Contacts[x].uin > 0) && (strlen(Contacts[x].nick) > 0)
      && (Contacts[x].status != STATUS_NOT_IN_LIST))
      fprintf(fp, "%d %s\n", Contacts[x].uin, Contacts[x].nick);
  
  if(fclose (fp) != 0)
  {
    if(Verbose & ICQ_VERB_ERR)
      printf("\nfclose (%s) failed.\n", filename);
//  Void function ... but we should return a value to know if we failed
//    return 0;
  }
}
