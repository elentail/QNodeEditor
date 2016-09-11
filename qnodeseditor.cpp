/* Copyright (c) 2012, STANISLAW ADASZEWSKI
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STANISLAW ADASZEWSKI nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL STANISLAW ADASZEWSKI BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "qnodeseditor.h"

#include <QGraphicsScene>

#include <QEvent>
#include <QKeyEvent>

#include <QGraphicsSceneMouseEvent>
#include <QDebug>

#include "qneport.h"
#include "qneconnection.h"
#include "qneblock.h"

QNodesEditor::QNodesEditor(QObject *parent) :
    QObject(parent)
{
	conn = 0;
}

void QNodesEditor::install(QGraphicsScene *s)
{
	s->installEventFilter(this);
	scene = s;
}

QGraphicsItem* QNodesEditor::itemAt(const QPointF &pos)
{
	QList<QGraphicsItem*> items = scene->items(QRectF(pos - QPointF(1,1), QSize(3,3)));

	foreach(QGraphicsItem *item, items)
		if (item->type() > QGraphicsItem::UserType)
			return item;

	return 0;
}

bool QNodesEditor::eventFilter(QObject *o, QEvent *e)
{
	QGraphicsSceneMouseEvent *me = (QGraphicsSceneMouseEvent*) e;

	switch ((int) e->type())
	{
	case QEvent::GraphicsSceneMousePress:
	{

		switch ((int) me->button())
		{
		case Qt::LeftButton:
		{
			QGraphicsItem *item = itemAt(me->scenePos());


            //마우스 이벤트 위치에서 item 을 선택
			if (item && item->type() == QNEPort::Type)
			{
                conn = new QNEConnection(0);
                scene->addItem(conn);
				conn->setPort1((QNEPort*) item);
				conn->setPos1(item->scenePos());
				conn->setPos2(me->scenePos());
				conn->updatePath();

                e->accept();
				return true;
			} else if (item && item->type() == QNEBlock::Type)
			{
                /*if (selBlock)
                    selBlock->setSelected(true);*/
                selBlock = (QNEBlock*) item;
                e->accept();
			}
            else
            {
                selBlock = 0;
                e->accept();
            }
			break;
		}
		case Qt::RightButton:
		{
			QGraphicsItem *item = itemAt(me->scenePos());
            if (item && (item->type() == QNEConnection::Type))
				delete item;

			// if (selBlock == (QNEBlock*) item)
				// selBlock = 0;
            e->accept();
			break;
		}
		}
	}
	case QEvent::GraphicsSceneMouseMove:
	{
		if (conn)
		{
			conn->setPos2(me->scenePos());
			conn->updatePath();
			return true;
		}
		break;
	}
	case QEvent::GraphicsSceneMouseRelease:
	{
		if (conn && me->button() == Qt::LeftButton)
		{
			QGraphicsItem *item = itemAt(me->scenePos());
			if (item && item->type() == QNEPort::Type)
			{
				QNEPort *port1 = conn->port1();
				QNEPort *port2 = (QNEPort*) item;

				if (port1->block() != port2->block() && port1->isOutput() != port2->isOutput() && !port1->isConnected(port2))
				{
					conn->setPos2(port2->scenePos());
					conn->setPort2(port2);
					conn->updatePath();
					conn = 0;
					return true;
				}
			}

			delete conn;
			conn = 0;
			return true;
		}
		break;
	}

    // zoom in / out 이벤트
    case QEvent::GraphicsSceneWheel:
    {
        QGraphicsSceneWheelEvent *se = static_cast<QGraphicsSceneWheelEvent *>(e);
        const int degrees = se->delta()/8;
        int steps = degrees / 15;

        qDebug() << "STEP" << steps;

        //double scaleFactor = 1.0; //How fast we zoom

        const qreal minFactor = 1.0;
        const qreal maxFactor = 10.0;

        if(steps > 0)
        {
            qDebug() << "+ : " << steps ;
        }
        else
        {
            qDebug() << "- : " << steps ;
        }
        se->accept();
        break;
    }

    // delete 이벤트
    case QEvent::KeyPress:
    {
        QKeyEvent *ke = (QKeyEvent*) e;
        if(ke->key() == Qt::Key_Delete)
        {
            qDebug() << ke->key();
            if (selBlock)
                delete selBlock;
            selBlock = 0;
            //if (selBlock == (QNEBlock*) item)
            //    selBlock = 0;
        }
        break;


    }

	}
	return QObject::eventFilter(o, e);
}

void QNodesEditor::save(QDataStream &ds)
{
	foreach(QGraphicsItem *item, scene->items())
		if (item->type() == QNEBlock::Type)
		{
			ds << item->type();
			((QNEBlock*) item)->save(ds);
		}

	foreach(QGraphicsItem *item, scene->items())
		if (item->type() == QNEConnection::Type)
		{
			ds << item->type();
			((QNEConnection*) item)->save(ds);
		}
}

void QNodesEditor::load(QDataStream &ds)
{
	scene->clear();

	QMap<quint64, QNEPort*> portMap;

	while (!ds.atEnd())
	{
		int type;
		ds >> type;
		if (type == QNEBlock::Type)
		{
            QNEBlock *block = new QNEBlock(0);
            scene->addItem(block);
			block->load(ds, portMap);
		} else if (type == QNEConnection::Type)
		{
            QNEConnection *conn = new QNEConnection(0);
            scene->addItem(conn);
			conn->load(ds, portMap);
		}
    }
}

// Recipe 실행을 위해 추가된 함
void QNodesEditor::execute()
{
    foreach(QGraphicsItem *item, scene->items())
    {
        if (item->type() == QNEBlock::Type)
        {
            foreach(QGraphicsItem *port_, item->childItems()) {
                if (port_->type() != QNEPort::Type)
                    continue;

                QNEPort *port = (QNEPort*) port_;
                if (port->isOutput())
                    //qDebug()<< "output : " << port->getName();
                    qDebug() << "[OUTPUT] = " << port->getName() << "\n";
                else
                    //qDebug()<< port->getName();
                    qDebug() << "[INPUT ] = " << port->getName() << "\n";
            }
        }
    }
}
