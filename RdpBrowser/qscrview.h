#ifndef QSCRVIEW_H
#define QSCRVIEW_H

#include <QWidget>
#include <QPainter>
#include "mainwindow.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSize>
#include <QTime>
#include "../RdpProxy/browserclient.h"

class MainWindow;
namespace Ui {
class QScrView;
}

class QScrView : public QWidget
{
    Q_OBJECT

public:
    explicit QScrView(MainWindow *parent = nullptr);
    ~QScrView() override;
protected:
    void  paintEvent(QPaintEvent *) override;
    void  mouseDoubleClickEvent(QMouseEvent *event) override;
    void  mouseMoveEvent(QMouseEvent *event) override;
    void  mousePressEvent(QMouseEvent *event) override;
    void  mouseReleaseEvent(QMouseEvent *event) override;
    void  keyPressEvent(QKeyEvent *event) override;
    void  keyReleaseEvent(QKeyEvent *event)override;

public slots:
    void timerEvent(QTimerEvent *event) override;

public:
    MainWindow *m_Parent;
    std::string MK_EVENTS;
    QSize sz;
    QImage image[4][4];

    int iGridX, iGridY;
     QRectF targetRect;
     QRectF sourceRect;
private:
    Ui::QScrView *ui;
};

#endif // QSCRVIEW_H
