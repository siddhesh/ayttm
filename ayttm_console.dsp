# Microsoft Developer Studio Project File - Name="ayttm_console" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ayttm_console - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ayttm_console.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ayttm_console.mak"\
 CFG="ayttm_console - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ayttm_console - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "ayttm_console - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ayttm_console - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "ayttm_console - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "everybud"
# PROP BASE Intermediate_Dir "everybud"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "everybud"
# PROP Intermediate_Dir "everybud"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib glib-1.3.lib gtk-1.3.lib gdk-1.3.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "ayttm_console - Win32 Release"
# Name "ayttm_console - Win32 Debug"
# Begin Source File

SOURCE=.\src\account.c
# End Source File
# Begin Source File

SOURCE=.\src\account.h
# End Source File
# Begin Source File

SOURCE=.\src\add_contact_window.c
# End Source File
# Begin Source File

SOURCE=.\src\add_contact_window.h
# End Source File
# Begin Source File

SOURCE=.\src\aim.c
# End Source File
# Begin Source File

SOURCE=.\src\aim.h
# End Source File
# Begin Source File

SOURCE=.\src\chat_window.c
# End Source File
# Begin Source File

SOURCE=.\src\chat_window.h
# End Source File
# Begin Source File

SOURCE=.\src\contact.h
# End Source File
# Begin Source File

SOURCE=.\src\contactlist.c
# End Source File
# Begin Source File

SOURCE=.\src\editcontacts.c
# End Source File
# Begin Source File

SOURCE=.\src\editcontacts.h
# End Source File
# Begin Source File

SOURCE=.\src\externs.h
# End Source File
# Begin Source File

SOURCE=.\src\globals.h
# End Source File
# Begin Source File

SOURCE=.\src\icq.c
# End Source File
# Begin Source File

SOURCE=.\src\icq.h
# End Source File
# Begin Source File

SOURCE=.\src\main.c
# End Source File
# Begin Source File

SOURCE=.\src\msn.h
# End Source File
# Begin Source File

SOURCE=.\src\service.c
# End Source File
# Begin Source File

SOURCE=.\src\service.h
# End Source File
# Begin Source File

SOURCE=.\src\status.c
# End Source File
# Begin Source File

SOURCE=.\src\status.h
# End Source File
# Begin Source File

SOURCE=.\src\util.c
# End Source File
# Begin Source File

SOURCE=.\src\util.h
# End Source File
# Begin Source File

SOURCE=.\src\yahoo.h
# End Source File
# End Target
# End Project
