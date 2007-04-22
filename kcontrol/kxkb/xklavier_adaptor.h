#ifndef XKLAVIER_ADAPTOR_H
#define XKLAVIER_ADAPTOR_H
#include <X11/Xlib.h>

#include <QHash>


class XKlavierAdaptorPriv;

class XKlavierAdaptor {
public:
	XKlavierAdaptor();
	~XKlavierAdaptor();	
		
	void loadXkbConfig(Display* dpy, bool layoutsOnly);

	QHash<QString, QString> getModels();
	QHash<QString, QString> getLayouts();
	QHash<QString, XkbOption> getOptions();
	QHash<QString, XkbOptionGroup> getOptionGroups();
	QHash<QString, QStringList*> getVariants();
	
private:
	XKlavierAdaptorPriv* priv;
};
#endif

