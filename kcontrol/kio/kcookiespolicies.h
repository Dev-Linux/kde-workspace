/*
* kcookiespolicies.h - Cookie policy configuration
*
* First version of cookies configuration by Waldo Bastian <bastian@kde.org>
* This dialog box created by David Faure <faure@kde.org>
* Re-written by Dawit Alemayehu <adawit@kde.org>
*
*/

#ifndef __KCOOKIESPOLICIES_H
#define __KCOOKIESPOLICIES_H

#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qbuttongroup.h>
#include <qgroupbox.h>
#include <qmap.h>
#include <kcmodule.h>


class DCOPClient;
class QStringList;
class KListView;
class QListViewItem;

class KCookiesPolicies : public KCModule
{
    Q_OBJECT

public:
    KCookiesPolicies(QWidget *parent = 0L, const char *name = 0L);
    ~KCookiesPolicies();

    virtual void load();
    virtual void save();
    virtual void defaults();
    virtual QString quickHelp();

protected slots:
    void changeCookiesEnabled();
    void addPressed();
    void changePressed();
    void deletePressed();
    void importPressed();
    void exportPressed();
    void changed();

private:
    void updateDomainList(const QStringList &list);

    // Global Policy Cookies enabled
    QGroupBox*    gb_global;
    QButtonGroup* bg_default;
    QCheckBox*    cb_enableCookies;
    QRadioButton* rb_gbPolicyAccept;
    QRadioButton* rb_gbPolicyAsk;
    QRadioButton* rb_gbPolicyReject;

    // Domain specific cookie policies
    QGroupBox*    gb_domainSpecific;
    KListView*   lb_domainPolicy;
    QPushButton*  pb_domPolicyAdd;
    QPushButton*  pb_domPolicyDelete;
    QPushButton*  pb_domPolicyChange;
    QPushButton*  pb_domPolicyImport;
    QPushButton*  pb_domPolicyExport;

    QMap<QListViewItem*, const char *> domainPolicy;
};

#endif // __KCOOKIESPOLICIES_H
