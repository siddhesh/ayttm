/*
 * Ayttm
 * Copyright (C) 2003, the Ayttm team
 * 
 * Ayttm is derivative of Everybuddy
 * Copyright (C) 1999-2002, Torrey Searle <tsearle@uci.edu>
 *
 * code orriginally derrived from 
 *
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <glib.h>

#ifdef __MINGW32__
#include <windows.h>
#else
#include <sys/wait.h>
#include <sys/signal.h>
#endif /* __MINGW32__ */

#include "globals.h"
#include "sound.h"
#include "prefs.h"
#include "plugin_api.h"

int clean_pid(void * dummy);	/* util.h */

#ifdef ESD_SOUND
#include <esd.h>
#endif	/* ARTS_SOUND */

#ifdef ARTS_SOUND
#include <string.h>
#include <artsc.h>
#include <audiofile.h>

#define BUFFERED_FRAME_COUNT 20000

/* This is for when arts_init(); arts_free(); arts_init(); does not work
 * with GlobalComm=Arts::X11GlobalComm in ~/.mcoprc
 * This is KDE bug #34541
 * Chris Halls <chris.halls@nikocity.de>
 */
#define ARTS_FREE_IS_BROKEN

#endif	/* ARTS_SOUND */

static int (*play_soundfile)(gchar *soundfile);
static void (*shutdown_sounddrv)();

static int play_audio(gchar *soundfile)
{
	int fd;
	struct stat info;	
	char * buf;
	char * audio_device;

	fd = open(soundfile,O_RDONLY);
	if (fd <= 0)
	{
		eb_debug(DBG_CORE, "Cannot open file %s\n", soundfile);
		return FALSE;
	}
	fstat(fd, &info);
	buf = alloca(info.st_size);
	read(fd,buf,24);
	read(fd,buf,info.st_size-24);
	close(fd);
	eb_debug(DBG_CORE, "File is %ld bytes\n", info.st_size);

	audio_device = getenv("AUDIODEV");
	if (audio_device == NULL) {
		/* OK, we're not running on a SunRay */
		audio_device = "/dev/audio";
	}

	eb_debug(DBG_CORE, "sending to %s\n", audio_device);

	fd = open(audio_device, O_WRONLY | O_EXCL);
	if (fd < 0)
		return FALSE;
	write(fd, buf, info.st_size-24);
	close(fd);

	return TRUE;
}

static int can_play_audio()
{
        /* FIXME check for write access and such. */
	/* do not change from return 1 for WIN32 */
        return 1;

}

/*
** This routine converts from linear to ulaw
**
** Craig Reese: IDA/Supercomputing Research Center
** Joe Campbell: Department of Defense
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) "A New Digital Technique for Implementation of Any
**     Continuous PCM Companding Law," Villeret, Michel,
**     et al. 1973 IEEE Int. Conf. on Communications, Vol 1,
**     1973, pg. 11.12-11.17
** 3) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: Signed 16 bit linear sample
** Output: 8 bit ulaw sample
*/

#define ZEROTRAP    /* turn on the trap as per the MIL-STD */
#define BIAS 0x84   /* define the add-in bias for 16 bit samples */
#define CLIP 32635

static unsigned char linear2ulaw(int sample)
{
	static int exp_lut[256] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,
                	     4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
                	     5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                	     5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                	     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                	     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                	     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                	     6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                	     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
	int sign, exponent, mantissa;
	unsigned char ulawbyte;

	/* Get the sample into sign-magnitude. */
	sign = (sample >> 8) & 0x80;		/* set aside the sign */
	if (sign != 0) 
		sample = -sample;		/* get magnitude */
	if (sample > CLIP) 
		sample = CLIP;		/* clip the magnitude */

	/* Convert from 16 bit linear to ulaw. */
	sample = sample + BIAS;
	exponent = exp_lut[(sample >> 7) & 0xFF];
	mantissa = (sample >> (exponent + 3)) & 0x0F;
	ulawbyte = ~(sign | (exponent << 4) | mantissa);
#ifdef ZEROTRAP
	if (ulawbyte == 0) 
		ulawbyte = 0x02;	/* optional CCITT trap */
#endif

	return(ulawbyte);
}

#if HAVE_LIBAUDIOFILE

/*
** This is only a declaration of the _af_ulaw2linear function.
** It is defined in libaudiofile.
**
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
*/
int _af_ulaw2linear (unsigned char ulawbyte);

#else

/*
** This routine converts from ulaw to 16 bit linear.
**
** Craig Reese: IDA/Supercomputing Research Center
** 29 September 1989
**
** References:
** 1) CCITT Recommendation G.711  (very difficult to follow)
** 2) MIL-STD-188-113,"Interoperability and Performance Standards
**     for Analog-to_Digital Conversion Techniques,"
**     17 February 1987
**
** Input: 8 bit ulaw sample
** Output: signed 16 bit linear sample
** Z-note -- this is from libaudiofile.  Thanks guys!
*/
static int _af_ulaw2linear (unsigned char ulawbyte)
{
	static int exp_lut[8] = {0,132,396,924,1980,4092,8316,16764};
	int sign, exponent, mantissa, sample;

	ulawbyte = ~ulawbyte;
	sign = (ulawbyte & 0x80);
	exponent = (ulawbyte >> 4) & 0x07;
	mantissa = ulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent + 3));
	if (sign != 0) sample = -sample;
		return(sample);
}

#endif	/* HAVE_LIBAUDIOFILE */

#ifdef ESD_SOUND

static int can_play_esd()
{
	return TRUE;
}

static int play_esd_file(gchar *soundfile)
{
	int esd_stat;
	esd_stat = esd_play_file(NULL, soundfile, 1);
	if (!esd_stat) {
		if (errno == ENOENT) {
	    		eb_debug(DBG_CORE, "Ayttm: File not found - file = \"%s\"\n", soundfile);
		} else {
		    	eb_debug(DBG_CORE, "Ayttm: Esd play file failed with code %d\n", errno);
		}
	}	
	return esd_stat;
}

#else

static int can_play_esd()
{
	return FALSE;
}

static int play_esd_file(gchar *soundfile)
{
	return FALSE;
}

#endif	/* ESD_SOUND */

// *** ARTS Support ***
// Comments/Questions -> sebastian.held@gmx.de
#ifdef ARTS_SOUND

static int test_arts()
{
	int err;
	if ((err = arts_init()) != 0) {
		eb_debug(DBG_CORE, "WARNING (NO ERROR!):\ncan_play_arts(): arts_init() failed: %s\nIs artsd running?\n", arts_error_text( err ));
		return FALSE;
	}
	return TRUE;
}

static int can_play_arts()
{
    
#ifndef ARTS_FREE_IS_BROKEN
	return test_arts();
#else
	pid_t childpid; 
	int childstatus;
	int ret = -1;

	childpid = fork();

	if(childpid==0)
		/* Child - do test and exit with result */
		exit(test_arts() ? 0 : 1);
    	else if(childpid==-1) {
		fprintf(stderr,"test_arts(): Failed at fork(), errno=%d\n", errno);
		return FALSE;
	} else {
		if( (ret=waitpid(-1, &childstatus, 0) == -1)) {
			fprintf(stderr,"test_arts(): Failed at wait(), errno=%d\n", errno);
			perror("waitpid");
			return FALSE;
		}

		if( WEXITSTATUS(childstatus)==0 ) {
		    return TRUE;
		}
    	}
	return FALSE;	
#endif
}

static void arts_shutdown()
{
	arts_free();
}

static int play_arts_file(gchar *soundfile)
{
	AFfilehandle fd;
	AFframecount frameCount, count;
	arts_stream_t artsStream;
	int frameSize, channelCount, sampleFormat, sampleWidth, err, susperr;
	double sampleRate;
	char * buf;

#ifdef ARTS_FREE_IS_BROKEN
	/* We have to init every time */
	if ((err = arts_init()) != 0) {
		eb_debug(DBG_CORE, "WARNING (NO ERROR!):\ncan_play_arts(): arts_init() failed: %s\nIs artsd running?\n", arts_error_text( err ));
		return FALSE;
	}
#endif
    
	fd = afOpenFile( soundfile, "r", NULL );
	if (fd == AF_NULL_FILEHANDLE) {
		eb_debug(DBG_CORE, "play_arts_file(): Cannot open file %s\n", soundfile);
		return FALSE;
	}
	frameCount = afGetFrameCount( fd, AF_DEFAULT_TRACK );
	frameSize = afGetFrameSize( fd, AF_DEFAULT_TRACK, 1 );
	channelCount = afGetChannels( fd, AF_DEFAULT_TRACK );
	sampleRate = afGetRate( fd, AF_DEFAULT_TRACK );
	afGetSampleFormat( fd, AF_DEFAULT_TRACK, &sampleFormat, &sampleWidth );
	eb_debug(DBG_CORE, "play_arts_file(): frameCount: %d\nframeSize: %i\nchannelCount: %i\nsampleRate: %.2f\n", (int) frameCount, frameSize, channelCount, sampleRate );

	artsStream = arts_play_stream( sampleRate, sampleWidth, channelCount, "EVERYBUDDY" );
	buf = (char *) malloc( BUFFERED_FRAME_COUNT * frameSize );
	count = afReadFrames( fd, AF_DEFAULT_TRACK, buf, BUFFERED_FRAME_COUNT );
	do {
		err = arts_write( artsStream, buf, count * frameSize );
		eb_debug(DBG_CORE, "count = %d\n", (int) count);
		eb_debug(DBG_CORE, "play_arts_file(): written bytes in artsStream: %i\n", err );
		if(err < 0)
			eb_debug(DBG_CORE, "arts_write error: %s\n", arts_error_text( err ) );
	} while ((count = afReadFrames( fd, AF_DEFAULT_TRACK, buf, BUFFERED_FRAME_COUNT)) && (err >= 0));
	free( buf );
	arts_close_stream( artsStream );

	afCloseFile( fd );
	susperr = arts_suspend();
	if(susperr < 0)
	        eb_debug(DBG_CORE, "arts_suspend error: %s\n", arts_error_text( err ) );
    return (err>=0);
}
#else
static int can_play_arts()
{
	return FALSE;
}

static void arts_shutdown()
{
}

static int play_arts_file(gchar *soundfile)
{
	return FALSE;
}
#endif	/* ARTS_SOUND */

static char* auscale(char* infile, char*outfile, float scaling)
{
#ifndef __MINGW32__
	FILE *fd_in = NULL;
	FILE *fd_out = NULL;
	guint32 header_size = 0;
	size_t bytes_read = 0;
	size_t io_bytes = 0;

	typedef struct
	{
		/* NOTE: THE GUINT32 VALUES BELOW ARE IN THE FILE AS BIG-ENDIAN FORMAT !!!! */
		guchar  magic[4];  /* magic number '.snd' */
		guint32 dataloc;    /* offset to start of data */
		guint32 datasize;   /* num bytes of data */
		guint32 dataformat; /* data format code */
		guint32 samplerate; /* sampling rate */
		guint32 channelcount; /* num of channels */
		guchar  info;         /* null terminated id string (variable length) */
	} SNDStruct;

	SNDStruct auheader; /* first 28 bytes of .au header */
	const unsigned int	mapLen = 256;
	unsigned int		x = 0;
	guchar	buf;
	guchar	buf2[mapLen];
	guchar	map[mapLen]; /* 8-bit mapping */


	if (scaling == 0.0)
		return(infile);  /* save time */

	if ((fd_in = fopen(infile, "rb")) == NULL) {
		fprintf(stderr,"Failed opening infile %s, errno=%d\n",infile, errno);
		return(infile);
	}

	/* see if this is .au file, if not then get out */
	if ((io_bytes = fread((guchar *)&auheader, sizeof(guchar), sizeof(auheader), fd_in)) != 28) {
		fclose(fd_in);
		fprintf(stderr,"Auscale - header read failed\n");
		return(infile);
	}

	if (strncmp(auheader.magic,".snd",4)) {
		eb_debug(DBG_CORE, "Not .au file,file type=%X%X%X%X\n",auheader.magic[0],auheader.magic[1],auheader.magic[2],auheader.magic[3]);
		fclose(fd_in);
		return(infile);
	}

	x = GUINT32_FROM_BE(auheader.dataformat);

	if (x!=1) /* only want ulaw-8 format files */ {
		eb_debug(DBG_CORE, "Not .au file,file type=%X%X%X%X\n",auheader.magic[0],auheader.magic[1],auheader.magic[2],auheader.magic[3]);
		fclose(fd_in);
		return(infile);
	}

	header_size= GUINT32_FROM_BE(auheader.dataloc); /* get offset to start of data */
	/* file is right type, reset to start */
	rewind(fd_in);

	if ((fd_out = fopen(outfile, "w+b")) == NULL) {
		fprintf(stderr,"Failed opening outfile %s, errno=%d\n",outfile, errno);
		fclose(fd_in);
		return(infile);
	}

	eb_debug(DBG_CORE, "Scaling = %f dB\n",scaling);
	scaling = pow(10.0,scaling/20.0);

	    /* Build mapping */
	for (x=0; x<mapLen ; x++)
		map[x]=linear2ulaw(scaling*_af_ulaw2linear(x));
  
	/* Shift the .au header 
	 * copy the .au header 
	 * Note: There are unusual situations when the 'info' field is VERY
	 * large and the start of data can therefore be larger than buf2.
	 * The following 'while' statement takes care of that problem. */
	while(header_size >= (guint32)sizeof(buf2)) {
		if ((io_bytes = fread(buf2, sizeof(guchar), sizeof(buf2), fd_in)) == sizeof(buf2)) {
			if ((io_bytes = fwrite(buf2, sizeof(guchar), sizeof(buf2), fd_out)) != sizeof(buf2)) {
				eb_debug(DBG_CORE, "error copying au file");
				fclose(fd_out);
				fclose(fd_in);
				return (infile);
			}
		} else {
			eb_debug(DBG_CORE, "error copying au file");
			fclose(fd_in);
			fclose(fd_out);
			return (infile);
		}

		/* calc remaining amount of header */
		header_size -= (guint32)sizeof(buf2);
	}

	/* copy rest of header (or in most cases - all of it) */
	if ((io_bytes = fread(buf2, sizeof(guchar), header_size, fd_in)) == header_size) {
		if ((io_bytes = fwrite(buf2, sizeof(guchar), header_size, fd_out)) != header_size) {
			eb_debug(DBG_CORE, "error copying au file");
			fclose(fd_out);
			fclose(fd_in);
			return (infile);
		}
	} else {
		eb_debug(DBG_CORE, "error copying au file");
		fclose(fd_in);
		fclose(fd_out);
		return(infile);
	}

	/* Write the mapped data out */
	while((bytes_read = fread( &buf, sizeof(guchar), 1, fd_in)) > 0)
		fwrite(map+buf, sizeof(guchar), 1, fd_out);

	fclose(fd_in);
	fclose(fd_out);
#endif

	return outfile;
}



/* file_ok 
 *
 * This routine test to see if file can be found and accessed
 * It issues an error message if the file open fails
 * Returns:
 * FALSE - If file open fails
 * TRUE - If file open succeeds
 *
 * MIZHI 04162001
 * I modified this function to use fstat, much cleaner...
 */

static gboolean file_ok(gchar *soundfile)
{
	gchar message[1024];
    
	if (access(soundfile, R_OK) == -1) {
      /* find and report the error */
		switch (errno) {
		case ENOENT:
			g_snprintf(message, 1024, "Sound file \'%s\', Not Found", soundfile);
			break;
		case EACCES:
			g_snprintf(message, 1024, "Sound file \'%s\', Access Not Allowed", soundfile);
			break;
		default:
			g_snprintf(message, 1024, "Error in opening sound file \'%s\', error code = %d", soundfile, errno);
		}
		printf(message);
		return FALSE;
	}
	return TRUE;
}


void playsoundfile(gchar *soundfile)
{
    
#ifdef _WIN32
	if(!play_soundfile)
        return;

	if (!file_ok(soundfile)) 
		return;

	PlaySound(soundfile,NULL,SND_FILENAME|SND_SYNC);
	return;
#endif
#ifndef __MINGW32__
	gint pid;

	if(!play_soundfile)
		return;

	if (!file_ok(soundfile)) 
		return;

	pid = fork();
	if (pid < 0) {
	    fprintf(stderr, "in playsoundfile(), fork failed!\n");
	    return;
	} else if (pid == 0) {
		/* we're the child process, play the sound... */
		if(fGetLocalPref("SoundVolume") != 0.0) {
			gchar tmp_soundfile[9] = "ebXXXXXX";
			
			if (mkstemp(tmp_soundfile) == -1) /* create name for auscale */ {
				fprintf(stderr, "in playsoundfile(), mkstemp failed!\n");
				return;
			}

			soundfile = auscale(soundfile,tmp_soundfile,fGetLocalPref("SoundVolume"));

			play_soundfile(soundfile);

			if (unlink(tmp_soundfile))
				fprintf(stderr, "in playsoundfile(), unlink failed!\n");
		} else {
			play_soundfile(soundfile);
		}

		_exit(0);
	} else {
	    /* We're the parent process... */
	    eb_timeout_add(100, clean_pid, NULL);
	}
#endif 
}



void play_sound(int sound)
{
	if ( iGetLocalPref("do_no_sound_when_away") && is_away )
		return;
		
	switch(sound)  {
	case BUDDY_AWAY:
		playsoundfile(cGetLocalPref("BuddyAwayFilename"));
		break;
	case BUDDY_ARRIVE:
		playsoundfile(cGetLocalPref("BuddyArriveFilename"));
		break;
	case BUDDY_LEAVE:
		playsoundfile(cGetLocalPref("BuddyLeaveFilename"));
		break;
	case SEND:
		playsoundfile(cGetLocalPref("SendFilename"));
		break;
	case RECEIVE:
		playsoundfile(cGetLocalPref("ReceiveFilename"));
		break;
	case FIRSTMSG:
		playsoundfile(cGetLocalPref("FirstMsgFilename"));
		break;
	}
}

void sound_init()
{
	if(can_play_arts()) {
		eb_debug(DBG_CORE,"Using arts sound\n");
		play_soundfile = play_arts_file;
		shutdown_sounddrv = arts_shutdown;
	}
	else if(can_play_esd()) {
		eb_debug(DBG_CORE,"Using esd sound\n");
		play_soundfile = play_esd_file;
		shutdown_sounddrv = NULL;
	}
	/* handle sound direct to /dev/audio */
	else if (can_play_audio()) {
		eb_debug(DBG_CORE,"Using raw audio\n");
		play_soundfile = play_audio;
		shutdown_sounddrv = NULL;
	}
	else {
		eb_debug(DBG_CORE,"Sound not available\n");
		play_soundfile = NULL;
		shutdown_sounddrv = NULL;
	}
}

void sound_shutdown()
{
	if(shutdown_sounddrv != NULL)
		shutdown_sounddrv();
}
