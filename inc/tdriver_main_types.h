#ifndef TDRIVER_MAIN_TYPES_H
#define TDRIVER_MAIN_TYPES_H

#include <QString>

class QTreeWidgetItem;


// types meant to be used in other code

typedef void* ProcessKey;
typedef void* TestObjectKey;



// internal convenience type, not really intended to be used outside this header

typedef QTreeWidgetItem *TestObjectPtrType;



// convenience functions meant to hide static casts

static inline TestObjectPtrType testObjectKey2Ptr(TestObjectKey key) {
    return static_cast<TestObjectPtrType>(key);
}

static inline TestObjectKey ptr2TestObjectKey(TestObjectPtrType ptr) {
    return static_cast<TestObjectKey>(ptr);
}

static inline QString testObjectKey2Str(TestObjectKey key) {
    return QString::number((ulong)key);
}

static inline TestObjectKey str2TestObjectKey(const QString &str) {
    return ptr2TestObjectKey(reinterpret_cast<TestObjectPtrType >(str.toULong()));
}

#endif // TDRIVER_MAIN_TYPES_H
