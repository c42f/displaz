// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef QT_UTIL_H_INCLUDED
#define QT_UTIL_H_INCLUDED

#include <QByteArray>
#include <QString>

/// Print QByteArray to std stream as raw data
inline std::ostream& operator<<(std::ostream& out, const QByteArray& s)
{
    out.write(s.constData(), s.size());
    return out;
}

/// Print QString to std stream as utf8
inline std::ostream& operator<<(std::ostream& out, const QString& s)
{
    out << s.toUtf8();
    return out;
}


#endif // QT_UTIL_H_INCLUDED
