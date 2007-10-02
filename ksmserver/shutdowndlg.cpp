/*****************************************************************
ksmserver - the KDE session management server

Copyright 2000 Matthias Ettrich <ettrich@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

******************************************************************/

#include <config-workspace.h>
#include <config-X11.h>

#include "shutdowndlg.h"
#include "plasma/svg.h"

#include <QApplication>
#include <QCursor>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QMenu>
#include <QStyle>
#include <QTimer>
#include <QSvgRenderer>
#include <QPaintEvent>
#include <QPaintEngine>

#include <kdebug.h>
#include <kdialog.h>
#include <kglobalsettings.h>
#include <kiconloader.h>
#include <klocale.h>
#include <qimageblitz.h>
#include <QPixmap>
#include <kseparator.h>
#include <kstandardguiitem.h>
#include <kuser.h>
#include <solid/control/powermanager.h>

#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdlib.h>
#include <dmctl.h>

#include <X11/Xlib.h>
#ifdef HAVE_XRENDER
#  include <X11/extensions/Xrender.h>
#endif

#include "shutdowndlg.moc"
#include <QX11Info>
#include <QDesktopWidget>
#include <QTimeLine>

#define GLOW_WIDTH 3

KSMShutdownFeedback * KSMShutdownFeedback::s_pSelf = 0L;

KSMShutdownFeedback::KSMShutdownFeedback()
 : QWidget( 0L, Qt::Popup ),
   m_currentY( 0 )
{
    setObjectName( "feedbackwidget" );
    setAttribute( Qt::WA_NoSystemBackground );
    setAttribute( Qt::WA_PaintOnScreen );
    setGeometry( QApplication::desktop()->geometry() );
    m_pixmap.resize( size() );
    m_pixmap.fill( Qt::transparent );
    QTimer::singleShot( 10, this, SLOT( slotPaintEffect() ) );
}


void KSMShutdownFeedback::paintEvent( QPaintEvent* )
{
    if ( m_currentY >= height() ) {
        QPainter painter( this );
        painter.drawPixmap( 0, 0, m_pixmap );
        return;
    }

    QPainter painter( this );
    painter.drawPixmap( 0, 0, m_pixmap, 0, 0, width(), m_currentY );
}

void KSMShutdownFeedback::slotPaintEffect()
{
    if ( m_currentY >= height() )
        return;

    QPixmap pixmap( width(), 10 );

    // grabWindow() is broken in Qt4 (because QPixmap is 32bpp).
    // Work around this bug by using Xrender to grab the screenshot, since
    // XRenderComposite() will convert the pixmap format automatically.
#ifdef HAVE_XRENDER
    if ( pixmap.paintEngine()->hasFeature( QPaintEngine::PorterDuff ) )
    {
        Display *dpy = QX11Info::display();
        Window root = DefaultRootWindow( dpy );
        XRenderPictureAttributes attr;
        attr.subwindow_mode = IncludeInferiors;
        XRenderPictFormat *format = XRenderFindVisualFormat( dpy, DefaultVisual( dpy, 0 ) );
        Picture rootPict = XRenderCreatePicture( dpy, root, format, CPSubwindowMode, &attr );
        XRenderComposite( dpy, PictOpSrc, rootPict, None, pixmap.x11PictureHandle(), 0, m_currentY,
                          0, 0, 0, 0, pixmap.width(), pixmap.height() );
        XRenderFreePicture( dpy, rootPict );
    } else
#endif
        pixmap = QPixmap::grabWindow( QX11Info::appRootWindow(), 0, m_currentY,
                                      pixmap.width(), pixmap.height() );

    QImage image = pixmap.toImage();
    Blitz::intensity( image, -0.4 );
    Blitz::grayscale( image );

    QPainter painter( &m_pixmap );
    painter.drawImage( 0, m_currentY, image );
    painter.end();

    m_currentY += pixmap.height();
    update( 0, 0, width(), m_currentY );

    QTimer::singleShot( 1, this, SLOT( slotPaintEffect() ) );
}

////////////

KSMPushButton::KSMPushButton( const QString &text, QWidget *parent )
 : QPushButton( parent ),  m_highlight( false ), m_text( text ), m_popupMenu(0), m_popupTimer(0),
   m_glowOpacity( 0.0 )
{
    setAttribute(Qt::WA_Hover, true);
    m_text = text;
    init();
}

void KSMPushButton::init()
{
    setMinimumSize( 85 + GLOW_WIDTH, 85 + GLOW_WIDTH );
    connect( this, SIGNAL(pressed()), SLOT(slotPressed()) );
    connect( this, SIGNAL(released()), SLOT(slotReleased()) );

    m_glowSvg = new Plasma::Svg( "dialogs/shutdowndlgbuttonglow", this );
    connect( m_glowSvg, SIGNAL(repaintNeeded()), this, SLOT(update()) );

    m_glowTimeLine = new QTimeLine( 150, this );
    connect( m_glowTimeLine, SIGNAL(valueChanged(qreal)),
            this, SLOT(animateGlow(qreal)) );

    QFont fnt;
    fnt.setPixelSize( 13 );
    fnt.setBold( true );
    // Calculate the width of the text when splitted on two lines and
    // properly resize the button.
    if( QFontMetrics(fnt).width( m_text ) > width()-4-(2*GLOW_WIDTH) ||
          2 * QFontMetrics(fnt).lineSpacing() > height()-54-(2*GLOW_WIDTH) ) {
        int w, h;
        int i = m_text.length()/2;
        int fac = 1;
        int diff = 1;
        while( i && i < m_text.length() && m_text[i] != ' ' ) {
            i = i + (diff * fac);
            fac *= -1;
            ++diff;
        }
        QString upper = m_text.left( i );
        QString lower = m_text.right( m_text.length() - i );
#ifdef __GNUC__
#warning "Which one of the following two is correct?"
#endif
        w = qMax( QFontMetrics(fnt).width( upper ) + 6, QFontMetrics(fnt).width( lower ) + 6 );
        w = qMax( w, width() );
        h = qMax( height(), 2 * QFontMetrics( fnt ).lineSpacing() + 52 + GLOW_WIDTH );
        if( w > width() || h > height()) {
            setMinimumSize( w, h );
            updateGeometry();
        }
    }
}

void KSMPushButton::paintEvent( QPaintEvent * e )
{
    QPainter p( this );
    p.setClipRect( e->rect() );
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    QPen pen;
    p.setBrush( QColor(255,255,255,40) );
    pen.setColor(QColor(150,150,150,200));
    pen.setWidth(1);
    p.setPen( pen );
    QFont fnt;
    fnt.setPixelSize( 13 );
    fnt.setBold( true );
    p.setFont( fnt );

    if( m_glowOpacity > 0 ) {
        p.save();
        p.setOpacity( m_glowOpacity );
        m_glowSvg->paint( &p, 0, 0 );
        p.restore();
    }

    p.setRenderHints( QPainter::Antialiasing, false);
    p.drawRect( QRect( GLOW_WIDTH, GLOW_WIDTH, width()-(2*GLOW_WIDTH), height()-(2*GLOW_WIDTH) ) );
    p.drawPixmap( width()/2 - 16, 14, m_pixmap );

    p.save();
    p.translate( 0, 50 );
    p.setPen( QPen( QColor( Qt::black ) ) );
    p.drawText( 0, 0, width(), height()-50, Qt::AlignHCenter|Qt::AlignVCenter|Qt::TextWordWrap|Qt::TextShowMnemonic, m_text );
    p.restore();

    if( m_popupMenu ) {
        p.save();
        p.setBrush( Qt::black );
        pen.setColor(QColor(Qt::black));
        p.setPen( pen );
        QPoint points[3] = {
            QPoint( width()-10-GLOW_WIDTH, height()-7-GLOW_WIDTH ),
            QPoint( width()-4-GLOW_WIDTH, height()-7-GLOW_WIDTH ),
            QPoint( width()-7-GLOW_WIDTH, height()-4-GLOW_WIDTH ) };
        p.drawPolygon( points, 3 );
        p.restore();
    }

    if( hasFocus() ) {
        pen.setBrush( QColor( 50, 50, 50) );
        pen.setStyle( Qt::DotLine );
        p.setPen( pen );
        p.drawRect( QRect( 2+GLOW_WIDTH, 2+GLOW_WIDTH, width()-4-(2*GLOW_WIDTH), height()-4-(2*GLOW_WIDTH) ) );
    }
}

void KSMPushButton::resizeEvent(QResizeEvent *e)
{
    m_glowSvg->resize( e->size() );
    QPushButton::resizeEvent( e );
}

void KSMPushButton::animateGlow( qreal value )
{
    m_glowOpacity = value;
    update();
}

void KSMPushButton::setPixmap( const QPixmap &p )
{
    m_pixmap = p;
    if( m_pixmap.size().width() != 32 || m_pixmap.size().height () != 32 )
        m_pixmap = m_pixmap.scaled( 32, 32 );
    update();
}

void KSMPushButton::setPopupMenu( QMenu *m )
{
    m_popupMenu = m;
    if( !m_popupTimer ) {
        m_popupTimer = new QTimer( this );
        connect( m_popupTimer, SIGNAL(timeout()), this, SLOT(slotTimeout()));
    }
}

void KSMPushButton::slotPressed()
{
    if( m_popupTimer )
        m_popupTimer->start( QApplication::startDragTime() );
}

void KSMPushButton::slotReleased()
{
    if( m_popupTimer )
        m_popupTimer->stop();
}

void KSMPushButton::slotTimeout()
{
    m_popupTimer->stop();
    if( m_popupMenu ) {
        m_popupMenu->popup( mapToGlobal(rect().bottomLeft()) );
        m_highlight = false;
        update();
    }
}

bool KSMPushButton::event( QEvent *e )
{
    if( e->type() == QEvent::HoverEnter )
    {
        m_highlight = true;
        m_glowTimeLine->setDirection( QTimeLine::Forward );
        m_glowTimeLine->start();
        update();
        return true;
    }
    else if( e->type() == QEvent::HoverLeave )
    {
        m_highlight = false;
        m_glowTimeLine->setDirection( QTimeLine::Backward );
        m_glowTimeLine->start();
        update();
        return true;
    }
    else
        return QPushButton::event( e );
}

//////

KSMShutdownDlg::KSMShutdownDlg( QWidget* parent,
                                bool maysd, KWorkSpace::ShutdownType sdtype )
  : QDialog( parent, Qt::Popup )
    // this is a WType_Popup on purpose. Do not change that! Not
    // having a popup here has severe side effects.
{
    m_svg = new Plasma::Svg( "dialogs/shutdowndlg", this);
    connect( m_svg, SIGNAL(repaintNeeded()), this, SLOT(update()) );
    setModal( true );
    resize(420, 180);
    KDialog::centerOnScreen(this);

    int space = maysd ? 10 : 80;
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setMargin( 0 );
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing( space );

    QFont fnt;
    fnt.setBold( true );
    fnt.setPixelSize( 20 );

    KUser userInformation;
    QString logoutMsg;
    QString userName = userInformation.fullName();
    QString loginName = userInformation.loginName();

    if (userName.isEmpty()) {
        logoutMsg = i18n( "End Session for %1", loginName );
    } else {
        logoutMsg = i18n( "End Session for %1 (%2)", userName, loginName );
    }

    QLabel *topLabel = new QLabel( this );
    // FIXME Made the color picked from the user's one
    topLabel->setText( "<font color='#eeeeec'>" + logoutMsg + "</font>" );
    topLabel->setFixedHeight( 30 );
    topLabel->setSizePolicy( QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed ) );
    topLabel->setAlignment( Qt::AlignCenter );
    topLabel->setFont( fnt );
    topLabel->setMargin( 5 );

    mainLayout->addWidget( topLabel, 0 );
    mainLayout->addStretch();
    mainLayout->addLayout( btnLayout );
    mainLayout->addSpacing( 5 );

    KSMPushButton* btnLogout = new KSMPushButton( i18n("Logout"), this );
    btnLogout->setPixmap( KIconLoader::global()->loadIcon( "edit-undo", KIconLoader::NoGroup, 32 ) );
    btnLogout->setFocus();
    connect(btnLogout, SIGNAL(clicked()), SLOT(slotLogout()));
    btnLayout->addWidget( btnLogout, 0 );

    if (maysd) {
        // Shutdown
        KSMPushButton* btnHalt = new KSMPushButton( i18n("Turn Off Computer"), this );
        btnHalt->setPixmap( KIconLoader::global()->loadIcon( "application-exit", KIconLoader::NoGroup, 32 ) );
        btnLayout->addWidget( btnHalt, 0 );
        connect(btnHalt, SIGNAL(clicked()), SLOT(slotHalt()));
        if ( sdtype == KWorkSpace::ShutdownTypeHalt )
            btnHalt->setFocus();

        QMenu *shutdownMenu = new QMenu( btnHalt );
        connect( shutdownMenu, SIGNAL(activated(int)), SLOT(slotSuspend(int)) );
        btnHalt->setPopupMenu( shutdownMenu );
        Solid::Control::PowerManager::SuspendMethods spdMethods = Solid::Control::PowerManager::supportedSuspendMethods();
        if( spdMethods & Solid::Control::PowerManager::Standby )
          shutdownMenu->insertItem( i18n("Standby"), Solid::Control::PowerManager::Standby );
        if( spdMethods & Solid::Control::PowerManager::ToRam )
          shutdownMenu->insertItem( i18n("Suspend to RAM"), Solid::Control::PowerManager::ToRam );
        if( spdMethods & Solid::Control::PowerManager::ToDisk )
          shutdownMenu->insertItem( i18n("Suspend to Disk"), Solid::Control::PowerManager::ToDisk );

        // Reboot
        KSMPushButton* btnReboot = new KSMPushButton( i18n("Restart Computer"), this );
        btnReboot->setPixmap( KIconLoader::global()->loadIcon( "view-refresh", KIconLoader::NoGroup, 32 ) );
        connect(btnReboot, SIGNAL(clicked()), SLOT(slotReboot()));
        btnLayout->addWidget( btnReboot, 0 );
        if ( sdtype == KWorkSpace::ShutdownTypeReboot )
            btnReboot->setFocus();

        int def, cur;
        if ( DM().bootOptions( rebootOptions, def, cur ) ) {
        if ( cur == -1 )
            cur = def;

        QMenu *rebootMenu = new QMenu( btnReboot );
        connect( rebootMenu, SIGNAL(activated(int)), SLOT(slotReboot(int)) );
        btnReboot->setPopupMenu( rebootMenu );

        int index = 0;
        for (QStringList::ConstIterator it = rebootOptions.begin(); it != rebootOptions.end(); ++it, ++index)
            {
            QString label = (*it);
            label=label.replace('&',"&&");
            if (index == cur) {
                rebootMenu->insertItem( label + i18nc("current option in boot loader", " (current)"), index );
            }
            else {
                rebootMenu->insertItem( label, index );
            }
        }
        }
    }

    KSMPushButton* btnBack = new KSMPushButton( i18n("Cancel"), this );
    btnBack->setPixmap( KIconLoader::global()->loadIcon( "dialog-cancel", KIconLoader::NoGroup, 32 ) );
    btnLayout->addWidget( btnBack, 0 );
    connect(btnBack, SIGNAL(clicked()), SLOT(reject()));

    btnLayout->insertSpacing( 0, 10 );
    btnLayout->insertSpacing( -1, 10 );
    btnLayout->insertStretch( 0, 1 );
    btnLayout->insertStretch( -1, 1 );
    setLayout( mainLayout );
}

void KSMShutdownDlg::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    p.setClipRect(e->rect());
    p.save();
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(rect(), Qt::transparent);
    p.restore();

    m_svg->paint( &p, 0, 0 );
}

void KSMShutdownDlg::resizeEvent(QResizeEvent *e)
{
    m_svg->resize( e->size() );
    QDialog::resizeEvent( e );
}

void KSMShutdownDlg::slotLogout()
{
    m_shutdownType = KWorkSpace::ShutdownTypeNone;
    accept();
}

void KSMShutdownDlg::slotReboot()
{
    // no boot option selected -> current
    m_bootOption.clear();
    m_shutdownType = KWorkSpace::ShutdownTypeReboot;
    accept();
}

void KSMShutdownDlg::slotReboot(int opt)
{
    if (int(rebootOptions.size()) > opt)
        m_bootOption = rebootOptions[opt];
    m_shutdownType = KWorkSpace::ShutdownTypeReboot;
    accept();
}


void KSMShutdownDlg::slotHalt()
{
    m_bootOption.clear();
    m_shutdownType = KWorkSpace::ShutdownTypeHalt;
    accept();
}


void KSMShutdownDlg::slotSuspend(int method)
{
    m_bootOption.clear();
    Solid::Control::PowerManager::SuspendMethod spdMethod = static_cast<Solid::Control::PowerManager::SuspendMethod>(method);
    Solid::Control::PowerManager::suspend( spdMethod );
    reject();
}

bool KSMShutdownDlg::confirmShutdown( bool maysd, KWorkSpace::ShutdownType& sdtype, QString& bootOption )
{
    KSMShutdownDlg* l = new KSMShutdownDlg( 0,
                                            //KSMShutdownFeedback::self(),
                                            maysd, sdtype );
    bool result = l->exec();
    sdtype = l->m_shutdownType;
    bootOption = l->m_bootOption;

    delete l;

    return result;
}
