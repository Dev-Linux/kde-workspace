/*
 * main.cpp
 *
 * Copyright (c) 1999 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <kcmodule.h>
#include "memory.h"
 
/* we have to include the info.cpp-file, to get the DEFINES about possible properties.
   example: we need the "define INFO_CPU_AVAILABLE" */
#include "info.cpp"
#include "info.h" 


extern "C"
{

  KCModule *create_cpu(QWidget *parent, const char *name)
  { 
#ifdef INFO_CPU_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_CPU);
#else
    return 0;
#endif
  }

  KCModule *create_irq(QWidget *parent, const char *name)
  { 
#ifdef INFO_IRQ_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_IRQ);
#else
    return 0;
#endif
  }

  KCModule *create_pci(QWidget *parent, const char *name)
  { 
#ifdef INFO_PCI_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_PCI);
#else
    return 0;
#endif
  }

  KCModule *create_dma(QWidget *parent, const char *name)
  { 
#ifdef INFO_DMA_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_DMA);
#else
    return 0;
#endif
  }

  KCModule *create_ioports(QWidget *parent, const char *name)
  { 
#ifdef INFO_IOPORTS_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_IO_Ports);
#else
    return 0;
#endif
  }

  KCModule *create_sound(QWidget *parent, const char *name)
  { 
#ifdef INFO_SOUND_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_Sound);
#else
    return 0;
#endif
  }

  KCModule *create_scsi(QWidget *parent, const char *name)
  { 
#ifdef INFO_SCSI_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_SCSI);
#else
    return 0;
#endif
  }

  KCModule *create_devices(QWidget *parent, const char *name)
  { 
#ifdef INFO_DEVICES_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_Devices);
#else
    return 0;
#endif
  }

  KCModule *create_partitions(QWidget *parent, const char *name)
  { 
#ifdef INFO_PARTITIONS_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_Partitions);
#else
    return 0;
#endif
  }

  KCModule *create_xserver(QWidget *parent, const char *name)
  { 
#ifdef INFO_XSERVER_AVAILABLE
    return new KInfoListWidget(parent, name, GetInfo_XServer_and_Video);
#else
    return 0;
#endif
  }

  KCModule *create_memory(QWidget *parent, const char *name)
  { 
    return new KMemoryWidget(parent, name);
  }

}

