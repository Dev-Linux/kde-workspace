//-----------------------------------------------------------------------------
// $Id$
// KDE screen saver
//-----------------------------------------------------------------------------

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <kapp.h>

#include "kscreensave.h"

void kForceLocker()
{
	char *buffer=0;

	sprintf(buffer, "%s/.kss.pid", getenv("HOME"));

	FILE *fp;

	if ( (fp = fopen( buffer, "r" ) ) != NULL )
	{
		// a screen saver is running - just tell it to lock
		int pid;

		fscanf( fp, "%d", &pid );
		fclose( fp );
		kill( pid, SIGUSR1 );
	}
	else
	{
	    sprintf(buffer, "%s/kblankscrn.kss", KApplication::kde_bindir().data());
	    
	    if ( fork() == 0 )
		{
		    execlp( buffer, buffer, "-test", "-lock", 0 );
		    
                    // if we make it here then try again using default path
		    execlp("kblankscrn.kss","kblankscrn.kss","-test","-lock",0);
		    
                    // uh oh - failed
		    fprintf( stderr, "Could not invoke kblankscrn.kss in $PATH or"
                             " %s/bin\n" , KApplication::kde_bindir().data());
		    exit (1);
		}
	}
}
