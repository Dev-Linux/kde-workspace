/*  $Id$

    Linux-specific Information about the Hardware.

    written 1998 by Helge Deller (helge.deller@ruhr-uni-bochum.de)

    To do (maybe?):
    - include Information about XFree86 and/or Accelerated X 
	(needs to change configure-skript, to see, if Header-files are available !)
    - maybe also include information about the video-framebuffer devices (newer Kernels >= 2.1.100)
    - rewrite detection-routines (maybe not to use the /proc-fs)
    - more & better sound-information
 
    /dev/sndstat support added: 1998-12-08 Duncan Haldane (f.d.m.haldane@cwix.com)
    $Log$
    Revision 1.11  2000/01/08 21:35:39  waba
    WABA: Applied patch from Graham TerMarsch <gtermars@home.com>
    (Adding status of AlsaSound driver)

    Revision 1.10  1999/12/03 03:32:39  deller
    - cleaned up Memory-Widget
    - much more information on X-Server (more will follow)
    - I tried to use better error-messages.

    Revision 1.9  1999/07/28 09:02:11  deller

    now the linux-specific information looks nicer again, after dmuell removed
    the ktablistbox-usage.

    Revision 1.8  1999/07/27 22:34:51  dmuell
    KTabListBox -> QListView.
    Will probably break compilation if platform != linux. Please tell me
    about problems, so I can fix it.

    Revision 1.7  1999/07/26 00:36:19  deller

    * partition-info should now work for systems without fstab.h but with
      mntent.h too... (Idea by Greg Lee, but I used an other implementation!).


*/

#include <unistd.h>
#include <syscall.h>
#include <stdio.h>
#include <sys/stat.h>
#include <linux/kernel.h>
#include <ctype.h>

#ifdef HAVE_FSTAB_H		// some Linux-versions don't have fstab.h !
#  include <fstab.h>
#  include <sys/statfs.h>
#  define INFO_PARTITIONS_FULL_INFO	// show complete info !
#elif defined HAVE_MNTENT_H	// but maybe they have mntent.h ?
# include <mntent.h>
# include <sys/vfs.h>
#  define INFO_PARTITIONS_FULL_INFO	// show complete info !
#else
#  undef INFO_PARTITIONS_FULL_INFO	// no partitions-info !
#endif


#include <kapp.h>

#define INFO_CPU_AVAILABLE
#define INFO_CPU "/proc/cpuinfo"

#define INFO_IRQ_AVAILABLE
#define INFO_IRQ "/proc/interrupts"

#define INFO_DMA_AVAILABLE
#define INFO_DMA "/proc/dma"

#define INFO_PCI_AVAILABLE
#define INFO_PCI "/proc/pci"

#define INFO_IOPORTS_AVAILABLE
#define INFO_IOPORTS "/proc/ioports"

#define INFO_SOUND_AVAILABLE
#define INFO_DEV_SNDSTAT "/dev/sndstat"
#define INFO_SOUND "/proc/sound" 
#define INFO_ASOUND "/proc/asound/sndstat"

#define INFO_DEVICES_AVAILABLE
#define INFO_DEVICES "/proc/devices"
#define INFO_MISC "/proc/misc"

#define INFO_SCSI_AVAILABLE
#define INFO_SCSI "/proc/scsi/scsi"

#define INFO_PARTITIONS_AVAILABLE
#define INFO_PARTITIONS "/proc/partitions"
#define INFO_MOUNTED_PARTITIONS "/etc/mtab"	// on Linux...

#define INFO_XSERVER_AVAILABLE


#define MAXCOLUMNWIDTH 600

bool GetInfo_ReadfromFile( QListView *lbox, const char *FileName, 
			    char splitchar, 
			    QListViewItem *lastitem = 0,
			    QListViewItem **newlastitem = 0 )
{
  char buf[512];
  bool added = false;

  QFile *file = new QFile(FileName);

  if (!file->exists()) {
    delete file; 
    return false;
  }

  if (!file->open(IO_ReadOnly)) {
    delete file; 
    /*   *GetInfo_ErrorString = 
	i18n("You do not have read-access for the file %1 !\nPlease ask your system-administrator for advice !")
	.arg(FileName);
    */
    return false;
  }

  while (file->readLine(buf,sizeof(buf)-1) > 0) {
      if (strlen(buf)) {
          char *p=buf;
          if (splitchar!=0)    /* remove leading spaces between ':' and the following text */
              while (*p) {
                  if (!isgraph(*p))
                      *p = ' ';
                  if (*p==splitchar) {
                      *p++ = ' ';
                      while (*p==' ')
                          ++p;
                      *(--p) = splitchar;
                      ++p;
                  }
                  else
                      ++p;
              }
          else {
              while (*p) {
                  if (!isgraph(*p))
                      *p = ' ';
                  ++p;
              }
          }

          QString s1(buf);
          QString s2 = s1.mid(s1.find(splitchar)+1);
          
          s1.truncate(s1.find(splitchar));
          if(!(s1.isEmpty() || s2.isEmpty()))
              lastitem = new QListViewItem(lbox, lastitem, s1, s2);
	      added = true;
      }
  }
  
  file->close();
  delete file;
  if (newlastitem)
      *newlastitem = lastitem;
  return added;
}




bool GetInfo_CPU( QListView *lBox )
{
  lBox->addColumn(i18n("Information"),150 );
  lBox->addColumn(i18n("Value") );
  return GetInfo_ReadfromFile( lBox, INFO_CPU, ':' );
}


bool GetInfo_IRQ( QListView *lBox )
{
  lBox->setFont(KGlobal::fixedFont());
  return GetInfo_ReadfromFile( lBox, INFO_IRQ, 0 );
}

bool GetInfo_DMA( QListView *lBox )
{
  lBox->addColumn(i18n("DMA-Channel"),100 );
  lBox->addColumn(i18n("used by") );
  return GetInfo_ReadfromFile( lBox, INFO_DMA, ':' );
}

bool GetInfo_PCI( QListView *lBox )
{
  sorting_allowed = false;	// no sorting by user !
  return GetInfo_ReadfromFile( lBox, INFO_PCI, 0 );
}

bool GetInfo_IO_Ports( QListView *lBox )
{
  lBox->addColumn(i18n("I/O-Range"),100 );
  lBox->addColumn(i18n("used by") );
  return GetInfo_ReadfromFile( lBox, INFO_IOPORTS, ':' );
}

bool GetInfo_Sound( QListView *lBox )
{
  sorting_allowed = false;	// no sorting by user !
  if ( GetInfo_ReadfromFile( lBox, INFO_DEV_SNDSTAT, 0 )) 
    return true;
  else if ( GetInfo_ReadfromFile( lBox, INFO_SOUND, 0 ))
    return true;
  else
    return GetInfo_ReadfromFile( lBox, INFO_ASOUND, 0 );
}

bool GetInfo_Devices( QListView *lBox )
{  
  QListViewItem* lastitem = 0;
  sorting_allowed = false;	// no sorting by user !
  GetInfo_ReadfromFile( lBox, INFO_DEVICES, 0, lastitem, &lastitem );
  lastitem = new QListViewItem(lBox, lastitem, "" ); // add empty line..
  // don't use i18n() for "Misc devices", because all other info is english too!
  lastitem = new QListViewItem(lBox, lastitem, QString("Misc devices:"));
  GetInfo_ReadfromFile( lBox, INFO_MISC, 0, lastitem, &lastitem );
  return true;
}

bool GetInfo_SCSI( QListView *lBox )
{
  return GetInfo_ReadfromFile( lBox, INFO_SCSI, 0 );
}



#ifndef INFO_PARTITIONS_FULL_INFO

bool GetInfo_Partitions( QListView *lBox )
{
  return GetInfo_ReadfromFile( lBox, INFO_PARTITIONS, 0 );
}

#else // INFO_PARTITIONS_FULL_INFO

// Some Ideas taken from garbazo from his source in info_fbsd.cpp

#if SIZEOF_LONG > 4	
#define LONG_TYPE	unsigned long 
#else
/* On 32-bit systems we would get an overflow in unsigned int for
   drives bigger than 4GB. Let's use the ugly type double ! */
/* I thought about using long long, but it's not always available ! */
#define LONG_TYPE	double
#endif

bool GetInfo_Partitions (QListView *lbox)
{
	#define NUMCOLS 6
	QString 	Title[NUMCOLS];
	QStringList	Mounted_Partitions;
	bool		found_in_List;
	int 		n;

#ifdef HAVE_FSTAB_H	
	struct fstab 	*fstab_ent;
# define FS_NAME	fstab_ent->fs_spec	// device-name
# define FS_FILE	fstab_ent->fs_file	// mount-point
# define FS_TYPE	fstab_ent->fs_vfstype	// fs-type
# define FS_MNTOPS 	fstab_ent->fs_mntops	// mount-options
#else
	struct mntent 	*mnt_ent;
	FILE		*fp;
# define FS_NAME	mnt_ent->mnt_fsname	// device-name
# define FS_FILE	mnt_ent->mnt_dir	// mount-point
# define FS_TYPE	mnt_ent->mnt_type	// fs-type
# define FS_MNTOPS 	mnt_ent->mnt_opts	// mount-options
#endif

 	struct statfs 	sfs;
	LONG_TYPE	total, avail;
	QString		str;
	QString 	MB(i18n("MB"));	// "MB" = "Mega-Byte"
	

#ifdef HAVE_FSTAB_H	
	if (setfsent() != 0) // Try to open fstab
	    return false;
#else
	if (!(fp=setmntent("/etc/fstab","r")))
 	    return false;
#endif

	// read the list of already mounted file-systems..
	QFile *file = new QFile(INFO_MOUNTED_PARTITIONS);
	if (file->open(IO_ReadOnly)) {
        while (file->readLine(str,1024) > 0) {
            if (str.length()) {
                int p = str.find(' ');	// find first space.
                if (p) str.remove(p,1024);	// erase all chars including space.
                Mounted_Partitions.append(str);
            }
        }
	    file->close();
	}
    delete file;

	// create the header-tables
	MB = QString(" ") + MB;
	Title[0] = i18n("Device");
	Title[1] = i18n("Mount Point");
	Title[2] = i18n("FS Type");
	Title[3] = i18n("Total Size");
	Title[4] = i18n("Free Size");
	Title[5] = i18n("Mount Options");

	for (n=0; n<NUMCOLS; ++n) 
        lbox->addColumn(Title[n]);

	// loop through all partitions...
#ifdef HAVE_FSTAB_H	
	while ((fstab_ent=getfsent())!=NULL)
#else
	while ((mnt_ent=getmntent(fp))!=NULL)
#endif
	{
		total = avail = 0;	// initialize size..
		found_in_List = (Mounted_Partitions.contains(FS_NAME)>0);
		if (found_in_List && statfs(FS_FILE,&sfs)==0) {
    		    total = ((LONG_TYPE)sfs.f_blocks) * sfs.f_bsize;
		    avail = (getuid() ? sfs.f_bavail : sfs.f_bfree) 
				* ((LONG_TYPE) sfs.f_bsize);
		};
		/*
		if (stat(fstab_ent->fs_file,&st)!=0)
			total = 0;
		if (!S_ISDIR(st.st_mode))
			total = 0;
		*/
		if (total)
            new QListViewItem(lbox, QString(FS_NAME) + "  ", QString(FS_FILE) + "  ",
                              QString(FS_TYPE) + "  ",
                              Value((int)(((total/1024)+512)/1024),6) + MB,
                              Value((int)(((avail/1024)+512)/1024),6) + MB,
                              QString(FS_MNTOPS));
		else
            new QListViewItem(lbox, QString(FS_NAME), QString(FS_FILE),
                              QString(FS_TYPE), " ", " ", 
                              QString(FS_MNTOPS));
	}

#ifdef HAVE_FSTAB_H	
	endfsent();  // close fstab..
#else
	endmntent(fp);  // close fstab..
#endif

    sorting_allowed = true;	// sorting by user allowed !
    lbox->setSorting(1);
    
    return true;
}
#endif	 // INFO_PARTITIONS_FULL_INFO




bool GetInfo_XServer_and_Video( QListView *lBox )
{
  return GetInfo_XServer_Generic( lBox );
}
