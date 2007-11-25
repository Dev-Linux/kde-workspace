/*
 *   Copyright (C) 2007 Luboš Luňák <l.lunak@kde.org>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "pixmap.h"
#include "splash.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>

static void usage( char* name )
    {
    fprintf( stderr, "Usage: %s <theme> [--test] [--pid]\n", name );
    exit( 1 );
    }

int main( int argc, char* argv[] )
    {
    if( argc != 2 && argc != 3 && argc != 4 )
        usage( argv[ 0 ] );
    bool test = false;
    bool print_pid = false;
    for( int i = 2; // 1 is the theme
         i < argc;
         ++i )
        {
        if( strcmp( argv[ i ], "--test" ) == 0 )
            test = true;
        else if( strcmp( argv[ i ], "--pid" ) == 0 )
            print_pid = true;
        else
            usage( argv[ 0 ] );
        }
    char* theme = argv[ 1 ];
    int pipes[ 2 ];
    if( pipe( pipes ) < 0 )
        {
        perror( "pipe()" );
        abort();
        }
    int parent_pipe = -1;
    pid_t pid = fork();
    if( pid < 0 )
        {
        perror( "fork()" );
        abort();
        }
    if( pid == 0 )
        { // child
        close( pipes[ 0 ] );
        parent_pipe = pipes[ 1 ];
        close( 0 ); // close stdin,stdout,stderr, otherwise startkde will block
        close( 1 );
        close( 2 );
        }
    else
        { // parent
        close( pipes[ 1 ] );
        char buf;
        while( read( pipes[ 0 ], &buf, 1 ) < 0
            && ( errno == EINTR || errno == EAGAIN || errno == ECHILD ))
            ;
        if( print_pid )
            printf( "%d\n", pid );
        return 0;
        }
    if( !openDisplay())
        return 2;
    if( test )
        XSynchronize( qt_xdisplay(), True );
    runSplash( theme, test, parent_pipe );
    closeDisplay();
    }
