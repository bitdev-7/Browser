#include "qscrview.h"
#include "ui_qscrview.h"
#include <QDateTime>

QScrView::QScrView(MainWindow *parent) :
    m_Parent(parent),
    ui(new Ui::QScrView)
{
    ui->setupUi(this);
    MK_EVENTS.clear();
    startTimer(200);
}

QScrView::~QScrView()
{
    delete ui;
}
void QScrView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    int i,j;
    sz = this->size();

    for (i = 0 ; i < nGridX; i ++) {
        for (j = 0 ; j < nGridY ; j ++) {
            QRectF target;
            QRectF source;
            target.setRect(sz.width() / nGridX * i , sz.height() / nGridY * j , sz.width() / nGridX , sz.height() / nGridY);
            source.setRect(0,0,image[i][j].width(),image[i][j].height());
            painter.drawImage(target, image[i][j], source);
        }
    }

}
void QScrView::timerEvent(QTimerEvent *event)
{
    if (!m_Parent->m_isCapturing) {
        return;
    }
    if (!MK_EVENTS.empty()) {
        m_Parent->sendControlPacket(PACKET_TYPE_MUS_CTL , MK_EVENTS.c_str(),MK_EVENTS.size());
        MK_EVENTS.clear();
    }
}
void QScrView::mouseMoveEvent(QMouseEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    sz = this->size();
    MK_EVENTS += "WM_MM;";
    MK_EVENTS += std::to_string(event->x()) + ";"  +
                 std::to_string(event->y()) + ";"  +
                 std::to_string(sz.width()) + ";"  +
                 std::to_string(sz.height())  + ";\0";
}
void QScrView::mousePressEvent(QMouseEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    sz = this->size();
    if (event->button() == Qt::LeftButton) {
        MK_EVENTS += "WM_LBD;";
    }
    if (event->button() == Qt::RightButton) {
        MK_EVENTS += "WM_RBD;";
    }
    if (event->button() == Qt::MidButton) {
        MK_EVENTS += "WM_MBD;";
    }
    MK_EVENTS += std::to_string(event->x()) + ";"  +
                 std::to_string(event->y()) + ";"  +
                 std::to_string(sz.width()) + ";"  +
                 std::to_string(sz.height())  + ";\0";

}
void QScrView::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    sz = this->size();
    if (event->button() == Qt::LeftButton) {
        MK_EVENTS += "WM_LBU;";
    }
    if (event->button() == Qt::RightButton) {
        MK_EVENTS += "WM_RBU;";
    }
    if (event->button() == Qt::MidButton) {
        MK_EVENTS += "WM_MBU;";
    }
    MK_EVENTS += std::to_string(event->x()) + ";"  +
                 std::to_string(event->y()) + ";"  +
                 std::to_string(sz.width()) + ";"  +
                 std::to_string(sz.height())  + ";\0";


}
void QScrView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    sz = this->size();

    if (event->button() == Qt::LeftButton) {
        MK_EVENTS += "WM_LBK;";
    }
    if (event->button() == Qt::RightButton) {
        MK_EVENTS += "WM_RBK;";
    }
    if (event->button() == Qt::MidButton) {
        MK_EVENTS += "WM_MBK;";
    }
    MK_EVENTS += std::to_string(event->x()) + ";"  +
                 std::to_string(event->y()) + ";"  +
                 std::to_string(sz.width()) + ";"  +
                 std::to_string(sz.height())  + ";\0";

}
void QScrView::keyPressEvent(QKeyEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    MK_EVENTS += "WM_KD;";
    MK_EVENTS += std::to_string(event->nativeVirtualKey()) +";" + std::to_string(event->nativeScanCode()) +  ";\0";


}
void QScrView::keyReleaseEvent(QKeyEvent *event) {
    if (!m_Parent->m_isCapturing)
        return;

    MK_EVENTS += "WM_KU;";
    MK_EVENTS += std::to_string(event->nativeVirtualKey()) +";" + std::to_string(event->nativeScanCode()) +  ";\0";

}
