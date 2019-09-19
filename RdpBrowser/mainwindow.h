#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProgressBar>
#include <QLineEdit>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QUdpSocket>
#include <QDateTime>
#include <QVector>
#include "qscrview.h"

#include "../RdpProxy/browserclient.h"
#include <vector>
using namespace std;

#define SEC_TIMER_PERIOD        1000    // 1s
#define MSG_TIMER_PERIOD        5000    // 5s

#define BROWSER_LISTING_MAGIC   "SERVICE:"

#define FILED_DELIMETER			";"
#define HTTP_CRLF_MARK          "\r\n"
#define HTTP_HEADER_END         "\r\n\r\n"

// treeview type
#define DRV_TYPE_STRING         "DRV"
#define DIR_TYPE_STRING         "DIR"
#define FIL_TYPE_STRING         "FIL"
#define IMAGE_PATH              "temp\\"
// treeview column structure
#define NAME_FLD_COLUMN         0
#define TYPE_FLD_COLUMN         1
#define SIZE_FLD_COLUMN         2
#define DATE_FLD_COLUMN         3
#define ATTR_FLD_COLUMN         4
//Image Grid Count
#define nGridX          4
#define nGridY          4

class QScrView;

namespace Ui {
class MainWindow;
}



class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QString path, QWidget *parent = 0);
    ~MainWindow();
public:
    QScrView *scrview;

public:
    void start_timer();
    void stop_timer();
    void sendControlPacket(uint16_t type, const char* raw_data, size_t raw_size);
    // transfer
    QString getPathname(QTreeWidgetItem* item);
    void listDiskDrives(QString drives, bool flag);
    void listRemoteDirs(QString dirInfo, bool flag);
    //void uploadData(QTreeWidgetItem* localItem, QTreeWidgetItem* remoteItem);
    //void downloadData(QTreeWidgetItem* localItem, QTreeWidgetItem* remoteItem);
    void saveFileData(QString localPath, QString remotePath, int offset, std::string file_data);
    void sendFileData(QString localPath, QString remotePath, int offset);
    void saveImageFile(std::string image_data , int , int );

private slots:
    void timerEvent(QTimerEvent *event);
    void proxyReadSlot();
    void processFinished();
    // left panel
    void monitorSlot();
    void connectSlot();
    // transfer
    void uploadSlot();
    void downloadSlot();
    void localSlot(QTreeWidgetItem*,int);
    void remoteSlot(QTreeWidgetItem*,int);

    // screen
    void on_tabWidget_tabBarClicked(int index);

private:
    Ui::MainWindow *        ui;
    QString                 m_app_path;
    QProcess*               m_ext_process;
    int                     m_sec_timer = 0;
    QMap<int, QString>      m_service_map; // sock: info
    QString                 m_read_buff;    
    QUdpSocket *            m_b2p_sock;
    QUdpSocket *            m_p2b_sock;
    int                     m_svc_sock;

public:
    void ProcessPacket(SERVICE_CONTENTS * service_contents);

    bool                    m_isCapturing;
    QByteArray              m_imgData;
    vector<char>            m_packet_buffer;
    int                     m_originalX;
    int                     m_originalY;

};


#endif // MAINWINDOW_H
