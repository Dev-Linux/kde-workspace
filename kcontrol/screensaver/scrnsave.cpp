//-----------------------------------------------------------------------------
//
// KDE Display screen saver setup module
//
// Copyright (c)  Martin R. Jones 1996,1999,2002
//
// Converted to a kcc module by Matthias Hoelzer 1997
//


#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <qbuttongroup.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qslider.h>
#include <qlayout.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qheader.h>

#include <kapplication.h>
#include <kdebug.h>
#include <kprocess.h>
#include <knuminput.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#include <dcopclient.h>
#include <kservicegroup.h>
#include <kgenericfactory.h>
#include <kwin.h>
#include <kdialog.h>

#include <X11/Xlib.h>

#include "scrnsave.h"

// X11 headers
#undef Above
#undef Below
#undef None

template class QPtrList<SaverConfig>;

const uint widgetEventMask =                 // X event mask
(uint)(
       ExposureMask |
       PropertyChangeMask |
       StructureNotifyMask
      );

//===========================================================================
// DLL Interface for kcontrol
typedef KGenericFactory<KScreenSaver, QWidget > KSSFactory;
K_EXPORT_COMPONENT_FACTORY (kcm_screensaver, KSSFactory("kcmscreensaver") )


static QString findExe(const QString &exe) {
    QString result = locate("exe", exe);
    if (result.isEmpty())
        result = KStandardDirs::findExe(exe);
    return result;
}

//===========================================================================
//
//
SaverConfig::SaverConfig()
{
}

bool SaverConfig::read(QString file)
{
    KDesktopFile config(file, true);
    mExec = config.readPathEntry("Exec");
    mName = config.readEntry("Name");
    mCategory = i18n("Screen saver category", // Must be same in Makefile.am
                     config.readEntry("X-KDE-Category").utf8());

    if (config.hasActionGroup("Setup"))
    {
      config.setActionGroup("Setup");
      mSetup = config.readPathEntry("Exec");
    }

    if (config.hasActionGroup("InWindow"))
    {
      config.setActionGroup("InWindow");
      mSaver = config.readPathEntry("Exec");
    }

    int indx = file.findRev('/');
    if (indx >= 0) {
        mFile = file.mid(indx+1);
    }

    return !mSaver.isEmpty();
}

//===========================================================================
//
int SaverList::compareItems(QPtrCollection::Item item1, QPtrCollection::Item item2)
{
    SaverConfig *s1 = (SaverConfig *)item1;
    SaverConfig *s2 = (SaverConfig *)item2;

    return s1->name().localeAwareCompare(s2->name());
}

//===========================================================================
//
TestWin::TestWin()
    : QXEmbed(0, 0, WStyle_Customize | WStyle_NoBorder | WX11BypassWM )
{
    setFocusPolicy(StrongFocus);
    KWin::setState( winId(), NET::StaysOnTop );
}

//===========================================================================
//

KScreenSaver::KScreenSaver(QWidget *parent, const char *name, const QStringList&)
    : KCModule(KSSFactory::instance(), parent, name)
{
    mSetupProc = 0;
    mPreviewProc = 0;
    mTestWin = 0;
    mTestProc = 0;
    mPrevSelected = -2;
    mMonitor = 0;
    mTesting = false;

    // Add non-KDE path
    KGlobal::dirs()->addResourceType("scrsav",
                                     KGlobal::dirs()->kde_default("apps") +
                                     "apps/ScreenSavers/");

    // Add KDE specific screensaver path
    QString relPath="System/ScreenSavers/";
    KServiceGroup::Ptr servGroup = KServiceGroup::baseGroup( "screensavers" );
    if (servGroup)
    {
      relPath=servGroup->relPath();
      kdDebug() << "relPath=" << relPath << endl;
    }

    KGlobal::dirs()->addResourceType("scrsav",
                                     KGlobal::dirs()->kde_default("apps") +
                                     relPath);

    readSettings();

    mSetupProc = new KProcess;
    connect(mSetupProc, SIGNAL(processExited(KProcess *)),
            this, SLOT(slotSetupDone(KProcess *)));

    mPreviewProc = new KProcess;
    connect(mPreviewProc, SIGNAL(processExited(KProcess *)),
            this, SLOT(slotPreviewExited(KProcess *)));

    QBoxLayout *topLayout = new QHBoxLayout(this, 0, KDialog::spacingHint());

    // left column
    QBoxLayout *vLayout = new QVBoxLayout(topLayout, KDialog::spacingHint());

    mSaverGroup = new QGroupBox(i18n("Screen Saver"), this );
    mSaverGroup->setColumnLayout( 0, Qt::Horizontal );
    vLayout->addWidget(mSaverGroup);
    QBoxLayout *groupLayout = new QVBoxLayout( mSaverGroup->layout(),
        KDialog::spacingHint() );

    mSaverListView = new QListView( mSaverGroup );
    mSaverListView->addColumn("");
    mSaverListView->header()->hide();
    mSelected = -1;
    groupLayout->addWidget( mSaverListView, 10 );
    connect( mSaverListView, SIGNAL(doubleClicked ( QListViewItem *)), this, SLOT( slotSetup()));
    QWhatsThis::add( mSaverListView, i18n("This is a list of the available"
      " screen savers. Select the one you want to use.") );

    QBoxLayout* hlay = new QHBoxLayout(groupLayout, KDialog::spacingHint());
    mSetupBt = new QPushButton( i18n("&Setup..."), mSaverGroup );
    connect( mSetupBt, SIGNAL( clicked() ), SLOT( slotSetup() ) );
    mSetupBt->setEnabled(false);
    hlay->addWidget( mSetupBt );
    QWhatsThis::add( mSetupBt, i18n("If the screen saver you selected has"
      " customizable features, you can set them up by clicking this button.") );

    mTestBt = new QPushButton( i18n("&Test"), mSaverGroup );
    connect( mTestBt, SIGNAL( clicked() ), SLOT( slotTest() ) );
    mTestBt->setEnabled(false);
    hlay->addWidget( mTestBt );
    QWhatsThis::add( mTestBt, i18n("You can try out the screen saver by clicking"
      " this button. (Also, the preview image shows you what the screen saver"
      " will look like.)") );

    // right column
    vLayout = new QVBoxLayout(topLayout, KDialog::spacingHint());

    mMonitorLabel = new QLabel( this );
    mMonitorLabel->setAlignment( AlignCenter );
    mMonitorLabel->setPixmap( QPixmap(locate("data",
                         "kcontrol/pics/monitor.png")));
    vLayout->addWidget(mMonitorLabel, 0);
    QWhatsThis::add( mMonitorLabel, i18n("Here you can see a preview of the selected screen saver.") );

    mSettingsGroup = new QGroupBox( i18n("Settings"), this );
    mSettingsGroup->setColumnLayout( 0, Qt::Vertical );
    vLayout->addWidget( mSettingsGroup );
    groupLayout = new QVBoxLayout( mSettingsGroup->layout(),
        KDialog::spacingHint() );


    mEnabledCheckBox = new QCheckBox(i18n("Start screen saver a&utomatically"), mSettingsGroup);
    mEnabledCheckBox->setChecked(mEnabled);
    QWhatsThis::add( mEnabledCheckBox, i18n("When you check this option, the selected screen saver will be started"
                                            " automatically after a certain number of minutes of inactivity."
                                            " This timeout period can be set using the spinbox below.") );
    connect(mEnabledCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotEnable(bool)));
    groupLayout->addWidget(mEnabledCheckBox);

    QBoxLayout *hbox = new QHBoxLayout();
    groupLayout->addLayout(hbox);
    hbox->addSpacing(30);
    mActivateLbl = new QLabel(i18n("After:"), mSettingsGroup);
    mActivateLbl->setEnabled(mEnabled);
    hbox->addWidget(mActivateLbl);
    mWaitEdit = new QSpinBox(mSettingsGroup);
    mWaitEdit->setSteps(1, 10);
    mWaitEdit->setRange(1, INT_MAX);
    mWaitEdit->setSuffix(i18n(" minutes"));
    mWaitEdit->setSpecialValueText(i18n("1 minute"));
    mWaitEdit->setValue(mTimeout/60);
    mWaitEdit->setEnabled(mEnabled);
    connect(mWaitEdit, SIGNAL(valueChanged(int)), SLOT(slotTimeoutChanged(int)));
    mActivateLbl->setBuddy(mWaitEdit);
    hbox->addWidget(mWaitEdit);
    hbox->addStretch(1);
    QString wtstr = i18n("Choose the period of inactivity"
      " after which the screen saver should start."
      " To prevent the screen saver from automatically starting, uncheck the above option.");
    QWhatsThis::add( mActivateLbl, wtstr );
    QWhatsThis::add( mWaitEdit, wtstr );

    mDPMSDependentCheckBox = new QCheckBox(i18n("Make the screen saver aware of power management"), mSettingsGroup);
    mDPMSDependentCheckBox->setChecked( mDPMS );
    connect( mDPMSDependentCheckBox, SIGNAL( toggled( bool ) ),
	this, SLOT( slotDPMS( bool ) ) );
    groupLayout->addWidget(mDPMSDependentCheckBox);
    QWhatsThis::add( mDPMSDependentCheckBox, i18n("Enable this option if you want to disable the screen saver while watching TV or movies.") );

    mLockCheckBox = new QCheckBox( i18n("&Require password to stop screen saver"), mSettingsGroup );
    mLockCheckBox->setChecked( mLock );
    connect( mLockCheckBox, SIGNAL( toggled( bool ) ),
         this, SLOT( slotLock( bool ) ) );
    groupLayout->addWidget(mLockCheckBox);
    QWhatsThis::add( mLockCheckBox, i18n("If you check this option, the display"
      " will be locked when the screen saver starts. To restore the display,"
      " enter your account password at the prompt.") );

    QGridLayout *gl = new QGridLayout(groupLayout, 2, 4);
    gl->setColStretch( 2, 10 );

    QLabel* lbl = new QLabel(i18n("&Priority:"), mSettingsGroup);
    gl->addWidget(lbl, 0, 0);

    mPrioritySlider = new QSlider(QSlider::Horizontal, mSettingsGroup);
    mPrioritySlider->setRange(0, 19);
    mPrioritySlider->setSteps(1, 5);
    mPrioritySlider->setTickmarks(QSlider::Below);
    mPrioritySlider->setValue(19 - mPriority);
    connect(mPrioritySlider, SIGNAL( valueChanged(int)),
        SLOT(slotPriorityChanged(int)));
    lbl->setBuddy(mPrioritySlider);
    gl->addMultiCellWidget(mPrioritySlider, 0, 0, 1, 3);
    QWhatsThis::add( mPrioritySlider, i18n("Use this slider to change the"
      " processing priority for the screen saver over other jobs that are"
      " being executed in the background. For a processor-intensive screen"
      " saver, setting a higher priority may make the display smoother at"
      " the expense of other jobs.") );

#ifndef HAVE_SETPRIORITY
    lbl->setEnabled(false);
    mPrioritySlider->setEnabled(false);
#endif

    lbl = new QLabel(i18n("Low Priority", "Low"), mSettingsGroup);
    gl->addWidget(lbl, 1, 1);

#ifndef HAVE_SETPRIORITY
    lbl->setEnabled(false);
#endif

    lbl = new QLabel(i18n("High Priority", "High"), mSettingsGroup);
    gl->addWidget(lbl, 1, 3);

#ifndef HAVE_SETPRIORITY
    lbl->setEnabled(false);
#endif

    QGroupBox *mAutoLockGroup = new QGroupBox( i18n("Autolock"), this );
    mAutoLockGroup->setColumnLayout( 0, Qt::Vertical );
    vLayout->addWidget( mAutoLockGroup );
    groupLayout = new QVBoxLayout( mAutoLockGroup->layout(), 10 );

    m_topLeftCorner = new QCheckBox( i18n("Top-Left corner"), mAutoLockGroup );
    groupLayout->addWidget( m_topLeftCorner);
    m_topLeftCorner->setChecked(mTopLeftCorner);
    connect( m_topLeftCorner, SIGNAL( toggled( bool ) ),
         this, SLOT( slotChangeTopLeftCorner( bool ) ) );



    m_topRightCorner = new QCheckBox( i18n("Top-Right corner"), mAutoLockGroup );
    groupLayout->addWidget( m_topRightCorner);
    m_topRightCorner->setChecked(mTopRightCorner);
    connect( m_topRightCorner, SIGNAL( toggled( bool ) ),
         this, SLOT( slotChangeTopRightCorner( bool ) ) );


    m_bottomLeftCorner = new QCheckBox( i18n("Bottom-Left corner"), mAutoLockGroup );
    groupLayout->addWidget( m_bottomLeftCorner);
    m_bottomLeftCorner->setChecked(mBottomLeftCorner);
    connect( m_bottomLeftCorner, SIGNAL( toggled( bool ) ),
         this, SLOT( slotChangeBottomLeftCorner( bool ) ) );


    m_bottomRightCorner = new QCheckBox( i18n("Bottom-Right corner"), mAutoLockGroup );
    groupLayout->addWidget( m_bottomRightCorner);
    m_bottomRightCorner->setChecked(mBottomRightCorner);
    connect( m_bottomRightCorner, SIGNAL( toggled( bool ) ),
         this, SLOT( slotChangeBottomRightCorner( bool ) ) );



    //groupLayout->addStretch(1);

    vLayout->addStretch();

    if (mImmutable)
    {
       setButtons(buttons() & ~Default);
       mSettingsGroup->setEnabled(false);
       mSaverGroup->setEnabled(false);
    }

    // finding the savers can take some time, so defer loading until
    // we've started up.
    mNumLoaded = 0;
    mLoadTimer = new QTimer( this );
    connect( mLoadTimer, SIGNAL(timeout()), SLOT(findSavers()) );
    mLoadTimer->start( 100 );
    mChanged = false;
    setChanged(false);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::resizeEvent( QResizeEvent * )
{

  if (mMonitor)
    {
      mMonitor->setGeometry( (mMonitorLabel->width()-200)/2+23,
                 (mMonitorLabel->height()-186)/2+14, 151, 115 );
    }
}

//---------------------------------------------------------------------------
//
void KScreenSaver::mousePressEvent( QMouseEvent *)
{
    if ( mTesting )
	slotStopTest();
}

//---------------------------------------------------------------------------
//
void KScreenSaver::keyPressEvent( QKeyEvent *)
{
    if ( mTesting )
	slotStopTest();
}

//---------------------------------------------------------------------------
//
int KScreenSaver::buttons()
{
    return KCModule::Help | KCModule::Default | KCModule::Apply;
}

//---------------------------------------------------------------------------
//
KScreenSaver::~KScreenSaver()
{
    if (mPreviewProc)
    {
        if (mPreviewProc->isRunning())
        {
            int pid = mPreviewProc->pid();
            mPreviewProc->kill( );
            waitpid(pid, (int *) 0,0);
        }
        delete mPreviewProc;
    }

    delete mTestProc;
    delete mSetupProc;
    delete mTestWin;
}

//---------------------------------------------------------------------------
//
void KScreenSaver::load()
{
    readSettings();

//with the following line, the Test and Setup buttons are not enabled correctly
//if no saver was selected, the "Reset" and the "Enable screensaver", it is only called when starting and when pressing reset, aleXXX
//    mSelected = -1;
    int i = 0;
    QListViewItem *selectedItem = 0;
    for (SaverConfig* saver = mSaverList.first(); saver != 0; saver = mSaverList.next()) {
        if (saver->file() == mSaver)
        {
            selectedItem = mSaverListView->findItem ( saver->name(), 0 );
            if (selectedItem) {
                mSelected = i;
                break;
            }
        }
        i++;
    }
    if ( selectedItem )
    {
      mSaverListView->setSelected( selectedItem, true );
      mSaverListView->setCurrentItem( selectedItem );
      slotScreenSaver( selectedItem );
    }

    updateValues();
    mChanged = false;
    setChanged(false);
}

//------------------------------------------------------------After---------------
//
void KScreenSaver::readSettings()
{
    KConfig *config = new KConfig( "kdesktoprc");

    mImmutable = config->groupIsImmutable("ScreenSaver");

    config->setGroup( "ScreenSaver" );

    mEnabled = config->readBoolEntry("Enabled", false);
    mTimeout = config->readNumEntry("Timeout", 300);
    mDPMS = config->readBoolEntry("DPMS-dependent", false);
    mLock = config->readBoolEntry("Lock", false);
    mPriority = config->readNumEntry("Priority", 19);
    mSaver = config->readEntry("Saver");

    if (mPriority < 0) mPriority = 0;
    if (mPriority > 19) mPriority = 19;
    if (mTimeout < 60) mTimeout = 60;

    mTopLeftCorner = config->readBoolEntry("LockCornerTopLeft", false);
    mTopRightCorner = config->readBoolEntry("LockCornerTopRight", false) ;
    mBottomLeftCorner = config->readBoolEntry("LockCornerBottomLeft", false);
    mBottomRightCorner = config->readBoolEntry("LockCornerBottomRight", false);


    mChanged = false;
    delete config;
}

//---------------------------------------------------------------------------
//
void KScreenSaver::updateValues()
{
    if (mEnabled)
    {
        mWaitEdit->setValue(mTimeout/60);
    }
    else
    {
        mWaitEdit->setValue(0);
    }

    mLockCheckBox->setChecked(mLock);
    mDPMSDependentCheckBox->setChecked(mDPMS);
    mPrioritySlider->setValue(19-mPriority);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::defaults()
{
    if (mImmutable) return;

    slotScreenSaver( 0 );

    QListViewItem *item = mSaverListView->firstChild();
    if (item) {
        mSaverListView->setSelected( item, true );
        mSaverListView->setCurrentItem( item );
        mSaverListView->ensureItemVisible( item );
    }
    slotTimeoutChanged( 5 );
    slotPriorityChanged( 0 );
    slotDPMS( false );
    slotLock( false );
    updateValues();


    setChanged(true);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::save()
{
    if ( !mChanged )
        return;

    KConfig *config = new KConfig( "kdesktoprc");
    config->setGroup( "ScreenSaver" );

    config->writeEntry("Enabled", mEnabled);
    config->writeEntry("Timeout", mTimeout);
    config->writeEntry("DPMS-dependent", mDPMS);
    config->writeEntry("Lock", mLock);
    config->writeEntry("Priority", mPriority);
    config->writeEntry("LockCornerTopLeft", m_topLeftCorner->isChecked());
    config->writeEntry("LockCornerBottomLeft", m_bottomLeftCorner->isChecked());
    config->writeEntry("LockCornerTopRight", m_topRightCorner->isChecked());
    config->writeEntry("LockCornerBottomRight", m_bottomRightCorner->isChecked());


    if ( !mSaver.isEmpty() )
        config->writeEntry("Saver", mSaver);
    config->sync();
    delete config;

    // TODO (GJ): When you changed anything, these two lines will give a segfault
    // on exit. I don't know why yet.

    DCOPClient *client = kapp->dcopClient();
    client->send("kdesktop", "KScreensaverIface", "configure()", "");

    mChanged = false;
    setChanged(false);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::findSavers()
{
    if ( !mNumLoaded ) {
        mSaverFileList = KGlobal::dirs()->findAllResources("scrsav",
                            "*.desktop", false, true);
        new QListViewItem ( mSaverListView, i18n("Loading...") );
        if ( mSaverFileList.isEmpty() )
            mLoadTimer->stop();
        else
            mLoadTimer->start( 50 );
    }

    for ( int i = 0; i < 5 &&
            (unsigned)mNumLoaded < mSaverFileList.count();
            i++, mNumLoaded++ ) {
        QString file = mSaverFileList[mNumLoaded];
        SaverConfig *saver = new SaverConfig;
        if (saver->read(file)) {
            mSaverList.append(saver);
        } else
            delete saver;
    }

    if ( (unsigned)mNumLoaded == mSaverFileList.count() ) {
        QListViewItem *selectedItem = 0;
        int categoryCount = 0;
        int indx = 0;

        mLoadTimer->stop();
        delete mLoadTimer;
        mSaverList.sort();

        mSelected = -1;
        mSaverListView->clear();
        for ( SaverConfig *s = mSaverList.first(); s != 0; s = mSaverList.next())
        {
            QListViewItem *item;
            if (s->category().isEmpty())
                item = new QListViewItem ( mSaverListView, s->name(), "2" + s->name() );
            else
            {
                QListViewItem *categoryItem = mSaverListView->findItem( s->category(), 0 );
                if ( !categoryItem ) {
                    categoryItem = new QListViewItem ( mSaverListView, s->category(), "1" + s->category() );
                    categoryItem->setPixmap ( 0, SmallIcon ( "kscreensaver" ) );
                }
                item = new QListViewItem ( categoryItem, s->name(), s->name() );
                categoryCount++;
            }
            if (s->file() == mSaver) {
                mSelected = indx;
                selectedItem = item;
            }
            indx++;
        }

        // Delete categories with only one item
        QListViewItemIterator it ( mSaverListView );
        for ( ; it.current(); it++ )
            if ( it.current()->childCount() == 1 ) {
               QListViewItem *item = it.current()->firstChild();
               it.current()->takeItem( item );
               mSaverListView->insertItem ( item );
               delete it.current();
               categoryCount--;
            }

        mSaverListView->setRootIsDecorated ( categoryCount > 0 );
        mSaverListView->setSorting ( 1 );

        if ( mSelected > -1 )
        {
            mSaverListView->setSelected(selectedItem, true);
            mSaverListView->setCurrentItem(selectedItem);
            mSaverListView->ensureItemVisible(selectedItem);
            mSetupBt->setEnabled(!mSaverList.at(mSelected)->setup().isEmpty());
            mTestBt->setEnabled(!mSaverList.at(mSelected)->setup().isEmpty());
        }

        connect( mSaverListView, SIGNAL( clicked( QListViewItem * ) ),
                 this, SLOT( slotScreenSaver( QListViewItem * ) ) );

        setMonitor();
    }
}

//---------------------------------------------------------------------------
//
void KScreenSaver::setMonitor()
{
    if (mPreviewProc->isRunning())
    // CC: this will automatically cause a "slotPreviewExited"
    // when the viewer exits
    mPreviewProc->kill();
    else
    slotPreviewExited(mPreviewProc);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotPreviewExited(KProcess *)
{
    // Ugly hack to prevent continual respawning of savers that crash
    if (mSelected == mPrevSelected)
        return;

    if ( mSaverList.isEmpty() ) // safety check
        return;

    // Some xscreensaver hacks do something nasty to the window that
    // requires a new one to be created (or proper investigation of the
    // problem).
    delete mMonitor;

    mMonitor = new KSSMonitor(mMonitorLabel);
    mMonitor->setBackgroundColor(black);
    mMonitor->setGeometry((mMonitorLabel->width()-200)/2+23,
                          (mMonitorLabel->height()-186)/2+14, 151, 115);
    mMonitor->show();
    // So that hacks can XSelectInput ButtonPressMask
    XSelectInput(qt_xdisplay(), mMonitor->winId(), widgetEventMask );

    if (mSelected >= 0) {
        mPreviewProc->clearArguments();

        QString saver = mSaverList.at(mSelected)->saver();
        QTextStream ts(&saver, IO_ReadOnly);

        QString word;
        ts >> word;
        QString path = findExe(word);

        if (!path.isEmpty())
        {
            (*mPreviewProc) << path;

            while (!ts.atEnd())
            {
                ts >> word;
                if (word == "%w")
                {
                    word = word.setNum(mMonitor->winId());
                }
                (*mPreviewProc) << word;
            }

            mPreviewProc->start();
        }
    }

    mPrevSelected = mSelected;
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotEnable(bool e)
{
    mEnabled = e;
    mActivateLbl->setEnabled( e );
    mWaitEdit->setEnabled( e );
    mChanged = true;
    setChanged(true);
}


//---------------------------------------------------------------------------
//
void KScreenSaver::slotScreenSaver(QListViewItem *item)
{
    if (!item)
      return;

    int i = 0, indx = -1;
    for (SaverConfig* saver = mSaverList.first(); saver != 0; saver = mSaverList.next()) {
        if (saver->name() == item->text (0))
        {
            indx = i;
            break;
        }
        i++;
    }
    if (indx == -1) {
        mSelected = -1;
        return;
    }

    bool bChanged = (indx != mSelected);

    if (!mSetupProc->isRunning())
        mSetupBt->setEnabled(!mSaverList.at(indx)->setup().isEmpty());
    mTestBt->setEnabled(true);
    mSaver = mSaverList.at(indx)->file();

    mSelected = indx;
    setMonitor();
    if (bChanged)
    {
       mChanged = true;
       setChanged(true);
    }
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotSetup()
{
    if ( mSelected < 0 )
    return;

    if (mSetupProc->isRunning())
    return;

    mSetupProc->clearArguments();

    QString saver = mSaverList.at(mSelected)->setup();
    if( saver.isEmpty())
        return;
    QTextStream ts(&saver, IO_ReadOnly);

    QString word;
    ts >> word;
    bool kxsconfig = word == "kxsconfig";
    QString path = findExe(word);

    if (!path.isEmpty())
    {
        (*mSetupProc) << path;

        // Add caption and icon to about dialog
        if (!kxsconfig) {
            word = "-caption";
            (*mSetupProc) << word;
            word = mSaverList.at(mSelected)->name();
            (*mSetupProc) << word;
            word = "-icon";
            (*mSetupProc) << word;
            word = "kscreensaver";
            (*mSetupProc) << word;
        }

        while (!ts.atEnd())
        {
            ts >> word;
            (*mSetupProc) << word;
        }

        // Pass translated name to kxsconfig
        if (kxsconfig) {
          word = mSaverList.at(mSelected)->name();
          (*mSetupProc) << word;
        }

        mSetupBt->setEnabled( false );
        kapp->flushX();

        mSetupProc->start();
    }
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotTest()
{
    if ( mSelected == -1 )
        return;

    if (!mTestProc) {
        mTestProc = new KProcess;
    }

    mTestProc->clearArguments();
    QString saver = mSaverList.at(mSelected)->saver();
    QTextStream ts(&saver, IO_ReadOnly);

    QString word;
    ts >> word;
    QString path = findExe(word);

    if (!path.isEmpty())
    {
        (*mTestProc) << path;

        if (!mTestWin)
        {
            mTestWin = new TestWin();
            mTestWin->setBackgroundMode(QWidget::NoBackground);
            mTestWin->setGeometry(0, 0, kapp->desktop()->width(),
                                    kapp->desktop()->height());
        }

        mTestWin->show();
        mTestWin->raise();
        mTestWin->setFocus();
	// So that hacks can XSelectInput ButtonPressMask
	XSelectInput(qt_xdisplay(), mTestWin->winId(), widgetEventMask );

	grabMouse();
	grabKeyboard();

        mTestBt->setEnabled( FALSE );
	mPreviewProc->kill();

        while (!ts.atEnd())
        {
            ts >> word;
            if (word == "%w")
            {
                word = word.setNum(mTestWin->winId());
            }
            (*mTestProc) << word;
        }

	mTesting = true;
        mTestProc->start(KProcess::NotifyOnExit);
    }
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotStopTest()
{
    if (mTestProc->isRunning()) {
        mTestProc->kill();
    }
    releaseMouse();
    releaseKeyboard();
    mTestWin->hide();
    mTestBt->setEnabled(true);
    mPrevSelected = -1;
    setMonitor();
    mTesting = false;
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotTimeoutChanged(int to )
{
    mTimeout = to * 60;
    mChanged = true;
    setChanged(true);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotDPMS( bool d )
{
    mDPMS = d;
    mChanged = true;
    setChanged(true);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotLock( bool l )
{
    mLock = l;
    mChanged = true;
    setChanged(true);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotPriorityChanged( int val )
{
    if (val == mPriority)
    return;

    mPriority = 19 - val;
    if (mPriority > 19)
    mPriority = 19;
    else if (mPriority < 0)
    mPriority = 0;

    mChanged = true;
    setChanged(true);
}

//---------------------------------------------------------------------------
//
void KScreenSaver::slotSetupDone(KProcess *)
{
    mPrevSelected = -1;  // see ugly hack in slotPreviewExited()
    setMonitor();
    mSetupBt->setEnabled( true );
    setChanged(true);
}

void KScreenSaver::slotChangeBottomRightCorner( bool b)
{
    mBottomRightCorner = b;
    mChanged = true;
    emit changed(true);
}

void KScreenSaver::slotChangeBottomLeftCorner( bool b)
{
    mBottomLeftCorner = b;
    mChanged = true;
    emit changed(true);
}

void KScreenSaver::slotChangeTopRightCorner( bool b)
{
    mTopRightCorner = b;
    mChanged = true;
    emit changed(true);
}

void KScreenSaver::slotChangeTopLeftCorner( bool b)
{
    mTopLeftCorner = b;
    mChanged = true;
    emit changed(true);
}


//---------------------------------------------------------------------------
//
QString KScreenSaver::quickHelp() const
{
    return i18n("<h1>Screen Saver</h1> This module allows you to enable and"
       " configure a screen saver. Note that you can enable a screen saver"
       " even if you have power saving features enabled for your display.<p>"
       " Besides providing an endless variety of entertainment and"
       " preventing monitor burn-in, a screen saver also gives you a simple"
       " way to lock your display if you are going to leave it unattended"
       " for a while. If you want the screen saver to lock the screen, make sure you enable"
       " the \"Require password\" feature of the screen saver. If you don't, you can still"
       " explicitly lock the screen using the desktop's \"Lock Screen\" action.");
}

#include "scrnsave.moc"
