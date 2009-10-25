/*

Copyright 1988, 1998  The Open Group
Copyright 2003 Oswald Buddenhagen <ossi@kde.org>

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the copyright holder.

*/

/*
 * xdm - display manager daemon
 * Author: Keith Packard, MIT X Consortium
 *
 * generate authorization keys
 * for MIT-MAGIC-COOKIE-1 type authorization
 */

#include "dm.h"
#include "dm_auth.h"

#define AUTH_DATA_LEN 16 /* bytes of authorization data */

Xauth *
mitGetAuth( unsigned short namelen, const char *name )
{
	Xauth *new;
	new = Malloc( sizeof(Xauth) );

	if (!new)
		return 0;
	new->family = FamilyWild;
	new->address_length = 0;
	new->address = 0;
	new->number_length = 0;
	new->number = 0;

	new->data = Malloc( AUTH_DATA_LEN );
	if (!new->data) {
		free( new );
		return 0;
	}
	new->name = Malloc( namelen );
	if (!new->name) {
		free( new->data );
		free( new );
		return 0;
	}
	memmove( new->name, name, namelen );
	new->name_length = namelen;
	if (!generateAuthData( new->data, AUTH_DATA_LEN )) {
		free( new->name );
		free( new->data );
		free( new );
		return 0;
	}
	new->data_length = AUTH_DATA_LEN;
	return new;
}
