#ifndef RZGLOBAL_H
#define RZGLOBAL_H

#include <QObject>
#include <QString>

#define itemsof(_container_) (sizeof(_container_)/sizeof(0[_container_]))

template<class T> inline T rzobject_cast(QObject *o)
{
#if defined(QT_DEBUG) && 0
    if (!qobject_cast<T>(o)) {
        if (!o) {
            qFatal("rzobject_cast failed cause arg=NULL");
        } else {
            T target_obj;
            qFatal("rzobject_cast failed: got object '%s' class '%s' instead of '%s'",
                    qPrintable(o->objectName()), o->metaObject()->className(),
                    target_obj->staticMetaObject.className());
        }
        Q_ASSERT(!"rzobject_cast fail");
    }
#endif
    return static_cast<T>(o);
}

#endif // RZGLOBAL_H
