/* -------------------------------------------------------------

   historyitem.cpp (part of Klipper - Cut & paste history for KDE)

   (C) Esben Mose Hansen <kde@mosehansen.dk>

   Licensed under the GNU GPL Version 2

 ------------------------------------------------------------- */

#include <qmime.h>
#include <qdragobject.h>
#include <qstring.h>
#include <qpixmap.h>

#include <kdebug.h>

#include "historyitem.h"
#include "historystringitem.h"
#include "historyimageitem.h"

HistoryItem::HistoryItem() {

}

HistoryItem::~HistoryItem() {

}

HistoryItem* HistoryItem::create( const QMimeSource& aSource )
{
#if 0
    int i=0;
    while ( const char* f = aSource.format( i++ ) ) {
        kdDebug() << "format(" << i <<"): " << f << endl;
    }
#endif
    if ( QTextDrag::canDecode( &aSource ) ) {
        QString text;
        QTextDrag::decode( &aSource, text );
        return text.isNull() ? 0 : new HistoryStringItem( text );
    } else if ( QImageDrag::canDecode( &aSource ) ) {
        QPixmap image;
        QImageDrag::decode( &aSource, image );
        return image.isNull() ? 0 : new HistoryImageItem( image );
    }

    return 0; // Failed.

}

HistoryItem* HistoryItem::create( QDataStream& aSource ) {
    if ( aSource.atEnd() ) {
        return 0;
    }
    QString type;
    aSource >> type;
    if ( type == "string" ) {
        QString text;
        aSource >> text;
        return new HistoryStringItem( text );
    }
    if ( type == "image" ) {
        QPixmap image;
        aSource >> image;
        return new HistoryImageItem( image );
    }
    kdWarning() << "Failed to restore history item: Unknown type \"" << type << "\"" << endl;
    return 0;
}

