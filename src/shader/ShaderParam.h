// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef SHADER_PARAM_H_INCLUDED
#define SHADER_PARAM_H_INCLUDED

#include <QMap>
#include <QString>
#include <QVariant>
#include <QByteArray>

/// Representation of a shader "parameter" (uniform variable or attribute)
struct ShaderParam
{
    enum Type {
        Float,
        Int,
        Vec3
    };

    Type type;             ///< Variable type
    QByteArray name;       ///< Name of the variable in the shader
    QVariant defaultValue; ///< Default value
    QMap<QString,QString> kvPairs; ///< name,value pairs with additional metadata
    int ordering;          ///< Ordering in source file

    QString uiName() const
    {
        return kvPairs.value("uiname", name);
    }

    double getDouble(QString name, double defaultVal) const
    {
        if (!kvPairs.contains(name))
            return defaultVal;
        bool convOk = false;
        double val = kvPairs[name].toDouble(&convOk);
        if (!convOk)
            return defaultVal;
        return val;
    }

    int getInt(QString name, int defaultVal) const
    {
        if (!kvPairs.contains(name))
            return defaultVal;
        bool convOk = false;
        double val = kvPairs[name].toInt(&convOk);
        if (!convOk)
            return defaultVal;
        return val;
    }

    ShaderParam(Type type=Float, QByteArray name="",
                QVariant defaultValue = QVariant())
        : type(type), name(name), defaultValue(defaultValue), ordering(0) {}
};


inline bool operator==(const ShaderParam& p1, const ShaderParam& p2)
{
    return p1.name == p2.name && p1.type == p2.type &&
        p1.defaultValue == p2.defaultValue &&
        p1.kvPairs == p2.kvPairs && p1.ordering == p2.ordering;
}


// Operator for ordering in QMap
inline bool operator<(const ShaderParam& p1, const ShaderParam& p2)
{
    if (p1.name != p2.name)
        return p1.name < p2.name;
    return p1.type < p2.type;
}

#endif // SHADER_PARAM_H_INCLUDED
