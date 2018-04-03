/**
 * @file
 * Header: Resize Catcher
 *
 * All REVOSIM code is released under the GNU General Public License.
 * See GNUv3License.txt files in the programme directory.
 *
 * All REVOSIM code is Copyright 2018 by Mark Sutton, Russell Garwood,
 * and Alan R.T. Spencer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version. This program is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY.
 */

#ifndef RESIZECATCHER_H
#define RESIZECATCHER_H


#include <QObject>
#include <QEvent>

class ResizeCatcher : public QObject
{
    Q_OBJECT
public:
    explicit ResizeCatcher(QObject *parent = 0);

signals:

public slots:

protected:
    bool eventFilter(QObject *obj, QEvent *event);

};

#endif // RESIZECATCHER_H
