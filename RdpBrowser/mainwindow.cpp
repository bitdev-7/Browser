#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../RdpProxy/browserclient.h"

MainWindow::MainWindow(QString path, QWidget *parent)
     :QMainWindow(parent) ,
      ui(new Ui::MainWindow),
      m_app_path(path),
      m_svc_sock(0),
      m_isCapturing(false),
      m_originalX(0),
      m_originalY(0)
{
    ui->setupUi(this);    
    // load local drives
    TCHAR szDrives[MAX_PATH] = { 0 };
    DWORD szLength = GetLogicalDriveStrings(MAX_PATH, szDrives);
    if (szLength) {
        std::wstring wlist(szDrives, szLength);
        listDiskDrives(QString::fromStdWString(wlist), false);
    }
    ///////////////////////////////////////////////////////////

    scrview = new QScrView(this);
    ui->tabWidget->removeTab(1);
    ui->tabWidget->addTab(scrview,"Screen Monitor");
    ui->tabWidget->setTabIcon(1,QIcon(":/res/monitor.png"));

    ////////////////////////////////////////////////////////////
    m_originalX = m_originalY = 0;
    m_imgData.clear();
    m_ext_process = new QProcess(this);
    connect(m_ext_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished()));
    m_b2p_sock = new QUdpSocket(this);
    m_p2b_sock = new QUdpSocket(this);
    if (m_p2b_sock->bind(QHostAddress::LocalHost, LOCAL_IPC_P2B_PORT))
        connect(m_p2b_sock, SIGNAL(readyRead()), this, SLOT(proxyReadSlot()));
    else
        qDebug() << "Proxy binding failed";
}

MainWindow::~MainWindow()
{

    stop_timer();

    m_ext_process->kill();
    m_ext_process->waitForFinished();      

    delete m_b2p_sock;
    delete m_p2b_sock;
    delete ui;
}

void MainWindow::sendControlPacket(uint16_t type, const char* raw_data, size_t raw_size)
{
    if (m_svc_sock > 0) {
        int buf_len = BROWSER_PACKET_OVERHEAD + raw_size;
        const char *buffer = (const char*)malloc(buf_len);
        memset((void*)buffer, 0, buf_len);
        BROWSER_PACKET * header = (BROWSER_PACKET*)buffer;
        header->service_sock = m_svc_sock;
        header->service_request.service_data.packet_size = (uint32_t)raw_size;
        header->service_request.service_data.packet_type = type;
        if (raw_size > 0 && raw_data) memcpy((void*)(buffer + BROWSER_PACKET_OVERHEAD), raw_data, raw_size);
        buf_len = m_b2p_sock->writeDatagram(buffer, buf_len, QHostAddress::LocalHost, LOCAL_IPC_B2P_PORT);
        if (buf_len == -1) qDebug() << "Request send failed" << buf_len << type;
        //else qDebug() << "Request sent" << buf_len << type;
        free((void*)buffer);
    }
    else qDebug() << "Invalid service socket";
}

void MainWindow::ProcessPacket(SERVICE_CONTENTS * service_contents)
{
    char* pBody = (char *)service_contents + SERVICE_CONTENTS_OVERHEAD;
    //qDebug() << "read from proxy" << data.size() << buffer;
//   SERVICE_CONTENTS *service_contents = reinterpret_cast<SERVICE_CONTENTS*>(buffer);
   std::string service_buffer = std::string(pBody, service_contents->packet_size);
   if (service_contents->packet_type == PACKET_TYPE_SVC_ADD) {
       QString service_info = QString::fromStdString(service_buffer);
       if (!service_info.isEmpty()) {
           // parse service descriptor
           int sock = service_info.section(":", 0, 0).toInt();
           QString node = service_info.section(":", 1);
           bool duplicated = false;
           for (auto it : m_service_map.keys()) {
               if (m_service_map.value(it) == node) {
                   duplicated = true;
                   // update key
                   m_service_map.remove(it);
                   m_service_map.insert(sock, node);
                   break;
               }
           }
           if (!duplicated) {
               QString name = node.section(":", 0, 0);
               QString addr = node.section(":", 1, 1);
               statusBar()->showMessage("Add service " + node, MSG_TIMER_PERIOD);
               m_service_map.insert(sock, node);
               int row = ui->tableWidget->rowCount();
               ui->tableWidget->insertRow(row);
               ui->tableWidget->setItem(row, 0, new QTableWidgetItem(name));
               ui->tableWidget->setItem(row, 1, new QTableWidgetItem(addr));
           }
       }
   }
   else if (service_contents->packet_type == PACKET_TYPE_SVC_DEL) {
       QString service_info = QString::fromStdString(service_buffer);
       if (!service_info.isEmpty()) {
           // parse service descriptor
           int sock = service_info.section(":", 0, 0).toInt();
           QString node = service_info.section(":", 1);
           if (m_service_map.find(sock) != m_service_map.end()) {
               statusBar()->showMessage("Del service " + node, MSG_TIMER_PERIOD);
               m_service_map.remove(sock);
               QString name = node.section(":", 0, 0);
               QString addr = node.section(":", 1, 1);
               for (int row = 0; row < ui->tableWidget->rowCount(); row++) {
                   if (ui->tableWidget->item(row, 0)->text() == name && ui->tableWidget->item(row, 1)->text() == addr) {
                       ui->tableWidget->removeRow(row);
                       break;
                   }
               }
           }
           m_svc_sock = 0;
           ui->treeWidget_remote->clear();
       }
   }
   else if (service_contents->packet_type == PACKET_TYPE_DRV_LST) {
       QString service_info = QString::fromStdString(service_buffer);
      // qDebug() << "Remote Drive List" << service_info;
       listDiskDrives(service_info, true);
   }
   else if (service_contents->packet_type == PACKET_TYPE_FLD_LST) {
       QString service_info = QString::fromStdString(service_buffer);
      // qDebug() << "Remote Folder List" << service_info;
       listRemoteDirs(service_info, true);
   }
   else if (service_contents->packet_type == PACKET_TYPE_FLD_CRT) {
       QString service_info = QString::fromStdString(service_buffer);
//       qDebug() << "Remote Folder Create" << service_info;
       listRemoteDirs(service_info, false);
   }
   else if (service_contents->packet_type == PACKET_TYPE_FIL_PUT) {
       QString service_info = QString::fromStdString(service_buffer);
       // local_path;remote_path;offset
       QString localPath = service_info.section(FILED_DELIMETER, 0, 0);
       QString remotePath = service_info.section(FILED_DELIMETER, 1, 1);
       int offset = service_info.section(FILED_DELIMETER, 2).toInt();
    //   qDebug() << "Remote File PUT" << localPath << remotePath << offset;
       sendFileData(localPath, remotePath, offset);
   }
   else if (service_contents->packet_type == PACKET_TYPE_FIL_GET) {
       //local_path;remote_path;offset;size\r\n{file data}
       size_t pos = service_buffer.find(HTTP_CRLF_MARK);
       std::string file_data = service_buffer.substr(pos + 2);
       QString service_info = QString::fromStdString(service_buffer.substr(0, pos));
       QString localPath = service_info.section(FILED_DELIMETER, 0, 0);
       QString remotePath = service_info.section(FILED_DELIMETER, 1, 1);
       int offset = service_info.section(FILED_DELIMETER, 2, 2).toInt();
       int size = service_info.section(FILED_DELIMETER, 3).toInt();
       if (file_data.size() >= size) {
           qDebug() << "Remote File GET" << localPath << remotePath << offset << size;
           saveFileData(localPath, remotePath, offset, file_data);
       }
       else qDebug() << "Remote File GET" << localPath << remotePath << offset << size << "<>" << file_data.size();
   }
   else if (service_contents->packet_type == PACKET_TYPE_SCR_GET) {
       size_t start_pos = 0;
       size_t current_pos = 0;
       current_pos = service_buffer.find(FILED_DELIMETER);
       int iGridX = atoi(service_buffer.substr(start_pos,current_pos).c_str());
       start_pos = current_pos + 1;
       current_pos = service_buffer.find(FILED_DELIMETER,start_pos);
       int iGridY = atoi(service_buffer.substr(start_pos,current_pos).c_str());
       start_pos = current_pos + 1;
       std::string file_data = service_buffer.substr(start_pos);
       saveImageFile(file_data, iGridX , iGridY);
   }
}
void MainWindow::proxyReadSlot()
{
    while (m_p2b_sock->hasPendingDatagrams()) {
        // connection successed
        stop_timer();

        qint64 bytes_to_read = m_p2b_sock->pendingDatagramSize();
        QByteArray data;
        data.resize(bytes_to_read);

        if (m_p2b_sock->readDatagram(data.data(), bytes_to_read, nullptr, nullptr) != -1) {


            qDebug() << "read from proxy" << data.size();//<< buffer;

            // add to recv buffer
            m_packet_buffer.insert(m_packet_buffer.end(), data.data(), data.data() + bytes_to_read);

            SERVICE_CONTENTS *service_contents ;

            if (m_packet_buffer.size() < SERVICE_CONTENTS_OVERHEAD) {
                continue;
            }

            service_contents = reinterpret_cast<SERVICE_CONTENTS*>(m_packet_buffer.data());

            qDebug() << "type=" << service_contents->packet_type << ", packet size = " << service_contents->packet_size << endl ;
            size_t total_packet_len = service_contents->packet_size + SERVICE_CONTENTS_OVERHEAD;
            if (total_packet_len <= m_packet_buffer.size())
            {

                ProcessPacket(service_contents);

                // remove from recv_buffer
                m_packet_buffer.erase(m_packet_buffer.begin(), m_packet_buffer.begin() + (int)total_packet_len);
            }
        }
    }
}

void MainWindow::processFinished()
{
    stop_timer();
    int exitCode = m_ext_process->exitCode();
    statusBar()->showMessage("Local Proxy disconnected " + QString::number(exitCode), MSG_TIMER_PERIOD);
    ui->tableWidget->setRowCount(0);
    ui->tableWidget->clearContents();
    ui->treeWidget_remote->clear();
    ui->pushButton_connect->setText("Connect");
    qApp->quit();
}

// handle timer events
void MainWindow::timerEvent(QTimerEvent *event)
{
    // progressbar
    if (event->timerId() == m_sec_timer) {
        int val = ui->progressBar->value();
        if (val < ui->progressBar->maximum()) ui->progressBar->setValue(val + 1);
        else ui->progressBar->setValue(0);
    }
}

void MainWindow::start_timer()
{
    ui->progressBar->setValue(0);
    if (m_sec_timer == 0) m_sec_timer = startTimer(SEC_TIMER_PERIOD,Qt::PreciseTimer);
}

void MainWindow::stop_timer()
{
    ui->progressBar->setValue(0);
    if (m_sec_timer > 0) { killTimer(m_sec_timer); m_sec_timer = 0; }
}

void MainWindow::connectSlot()
{
    start_timer();
    if (m_ext_process->state() == QProcess::Running) {
        m_ext_process->kill();
        m_ext_process->waitForFinished();
    }
    m_ext_process->start(m_app_path + "/RdpProxy.exe", QStringList() << ui->lineEdit_addr->text() << ui->lineEdit_port->text());
    if (!m_ext_process->waitForStarted()) {
        statusBar()->showMessage(m_app_path + "/RdpProxy.exe is not started", MSG_TIMER_PERIOD);
        stop_timer();
        ui->pushButton_connect->setText("Connect");
    }
    else ui->pushButton_connect->setText("ReConnect");
}

void MainWindow::monitorSlot()
{
    QList<QTableWidgetItem*> list = ui->tableWidget->selectedItems();
    if (list.count() == 0) {
        statusBar()->showMessage("Remote Service is not selected", MSG_TIMER_PERIOD);
        return;
    }
    int row = list[0]->row();
    QString name = ui->tableWidget->item(row, 0)->text();
    QString addr = ui->tableWidget->item(row, 1)->text();
    // if a service is already connected, send screen capture off command
//    if (m_svc_sock > 0) {
//        sendControlPacket(PACKET_TYPE_SCR_OFF, NULL, 0);
//        isCapturing = false;
//        m_svc_sock = 0;
//    }
    if (m_isCapturing) {
         sendControlPacket(PACKET_TYPE_SCR_OFF, nullptr, 0);
         m_isCapturing = false;
         return;
    }
    for (auto it: m_service_map.keys()) {
        if (m_service_map.value(it) == name + ":" + addr) {
            m_svc_sock = it;
            break;
        }
    }
    statusBar()->showMessage("Connecting to " + name + " " + addr, MSG_TIMER_PERIOD);
    if (ui->tabWidget->currentIndex() == 0) sendControlPacket(PACKET_TYPE_DRV_LST, nullptr, 0);
    else {
        m_isCapturing = true;
        sendControlPacket(PACKET_TYPE_SCR_GET, nullptr, 0);

    }
}

void MainWindow::uploadSlot()
{
    QList<QTreeWidgetItem*> local = ui->treeWidget_local->selectedItems();
    QList<QTreeWidgetItem*> remote = ui->treeWidget_remote->selectedItems();
    if (local.empty() || remote.empty()) statusBar()->showMessage("Select proper source or destination for uploading", MSG_TIMER_PERIOD);
    else {
        QTreeWidgetItem* localItem = local[0];
        QTreeWidgetItem* remoteItem = remote[0];
        QString localType = localItem->text(TYPE_FLD_COLUMN);
        if (localType == FIL_TYPE_STRING) {
            QString remoteType = remoteItem->text(TYPE_FLD_COLUMN);
            QString localPath = getPathname(localItem);
            QString remotePath = getPathname(remoteItem);
            QString localFile = localPath.section("\\", -1, -1);
            if (remoteType == FIL_TYPE_STRING) {
                if (QMessageBox::question(this, "File Overwrite", "Will you overwrite " + remotePath + "?") != 0) return;
            }
            else if (remoteType == DIR_TYPE_STRING) remotePath.append("\\" + localFile);
            else remotePath.append(localFile);
            qDebug() << "Upload" << localPath << "to" << remotePath;
            sendFileData(localPath, remotePath, 0);
        }
        else QMessageBox::warning(this, "Folder upload", "Folder uploading is not allowed");
    }
}

void MainWindow::downloadSlot()
{
    QList<QTreeWidgetItem*> local = ui->treeWidget_local->selectedItems();
    QList<QTreeWidgetItem*> remote = ui->treeWidget_remote->selectedItems();
    if (local.empty() || remote.empty()) statusBar()->showMessage("Select proper source or destination for downloading", MSG_TIMER_PERIOD);
    else {
        QTreeWidgetItem* localItem = local[0];
        QTreeWidgetItem* remoteItem = remote[0];
        QString remoteType = remoteItem->text(TYPE_FLD_COLUMN);
        if (remoteType == FIL_TYPE_STRING) {
            QString localType = localItem->text(TYPE_FLD_COLUMN);
            QString localPath = getPathname(localItem);
            QString remotePath = getPathname(remoteItem);
            QString remoteFile = remotePath.section("\\", -1, -1);
            if (localType == FIL_TYPE_STRING) {
                if (QMessageBox::question(this, "File Overwrite", "Will you overwrite " + localPath + "?") == 0) {
                    remove(localPath.toStdString().c_str());
                }
                else return;
            }
            else if (localType == DIR_TYPE_STRING) localPath.append("\\" + remoteFile);
            else localPath.append(remoteFile);
            qDebug() << "Download" << remotePath << "to" <<  localPath;
            // local_path;remote_path;offset
            std::string file_info = localPath.toStdString() + FILED_DELIMETER + remotePath.toStdString() + FILED_DELIMETER + std::string("0");
            sendControlPacket(PACKET_TYPE_FIL_GET, file_info.c_str(), file_info.size());
        }
        else QMessageBox::warning(this, "Folder download", "Folder downloading is not allowed");
    }
}

void MainWindow::on_tabWidget_tabBarClicked(int index)
{
   ui->tabWidget->currentWidget()->setFocus();
}
