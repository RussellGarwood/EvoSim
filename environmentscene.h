/**
 * @file
 * Header: Environment Scene
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

#ifndef ENVIRONMENTSCENE_H
#define ENVIRONMENTSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QString>
#include <QList>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include "fossilrecord.h"

class MainWindow;

class EnvironmentScene : public QGraphicsScene
{
public:
        EnvironmentScene();
        MainWindow *mw;
        void DrawLocations(QList <FossilRecord *> frlist, bool show);
        int button;
        int grabbed;

protected:
     void mousePressEvent(QGraphicsSceneMouseEvent *event);
     void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
     void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
     QList<QGraphicsLineItem *> HorizontalLineList;
     QList<QGraphicsLineItem *> VerticalLineList;
     QList<QGraphicsSimpleTextItem *> LabelsList;
     void DoMouse(int x, int y);
 private slots:
     void ScreenUpdate();
};

#endif // ENVIRONMENTSCENE_H
