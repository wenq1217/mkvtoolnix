/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id: gui.cpp 3373 2006-11-19 17:41:46Z mosu $

   A Qt GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __RIGHTCLICK_TREE_WIDGET_H
#define __RIGHTCLICK_TREE_WIDGET_H

#include "config.h"

#include <QTreeWidget>

class rightclick_tree_widget: public QTreeWidget {
  Q_OBJECT;

public slots:

public:
  rightclick_tree_widget(QWidget *parent = NULL);

protected:
  virtual void mousePressEvent(QMouseEvent *event);
};

#endif // __RIGHTCLICK_TREE_WIDGET_H