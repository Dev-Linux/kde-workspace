/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kdesu.
 * Copyright (C) 1999,2000 Geert Jansen <jansen@kde.org>
 */

#ifndef __Passwords_h_included__
#define __Passwords_h_included__

class QButtonGroup;
class QCheckBox;

class KConfig;
class KIntNumInput;
class KAboutData;

#include <kcmodule.h>

class KPasswordConfig : public KCModule
{
    Q_OBJECT

public:
    KPasswordConfig(QWidget *parent=0, const char *name=0, const QStringList &list=QStringList());
    ~KPasswordConfig();

    virtual void load();
    virtual void save();
    virtual void defaults();
    const KAboutData* aboutData() const;

    void apply();
    int buttons();
    QString quickHelp() const;


private slots:

    void slotEchoMode(int);
    void slotKeep(bool);
    void configChanged(){emit changed(true);}
private:
    QButtonGroup *m_EMGroup;
    QCheckBox *m_KeepBut;
    KIntNumInput *m_TimeoutEdit;
    KConfig *m_pConfig;

    int m_Echo, m_Timeout;
    bool m_bKeep;
};

#endif
