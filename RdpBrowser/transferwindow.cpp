#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../RdpProxy/browserclient.h"

QString MainWindow::getPathname(QTreeWidgetItem* item)
{
    QString path;
    for (QTreeWidgetItem * parent = item; parent != NULL; parent = parent->parent()){
        if (path.isEmpty()) path = parent->text(NAME_FLD_COLUMN);
        else path = parent->text(NAME_FLD_COLUMN) + "\\" + path;
    }
    path.replace("\\\\", "\\");
    return path;
}

void MainWindow::localSlot(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;    
    QString path = getPathname(item);
    //qDebug() << "Local" << path;
    QString type = item->text(TYPE_FLD_COLUMN);
    if (type == FIL_TYPE_STRING) {
        // try open file
        QProcess::startDetached("explorer.exe " + path);
    }
    else {
        for (int i = item->childCount() - 1; i >= 0 ; i-- ) item->removeChild(item->child(i));
        QDir dir(path);
        QFileInfoList fileinfo_list = dir.entryInfoList(QStringList(), QDir::AllDirs|QDir::Files|QDir::NoDotAndDotDot, QDir::Type|QDir::Name|QDir::DirsFirst);
        for (int i = 0; i < fileinfo_list.size(); i++) {
            QFileInfo file_info = fileinfo_list[i];
            QString name = file_info.fileName();
            QTreeWidgetItem *node = new QTreeWidgetItem;
            node->setText(NAME_FLD_COLUMN, name);
            if (file_info.isDir()) {
                node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/folder.png"));
                node->setText(TYPE_FLD_COLUMN, DIR_TYPE_STRING);
                node->setExpanded( true );
            }
            else {
                node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/file.png"));
                node->setText(TYPE_FLD_COLUMN, FIL_TYPE_STRING);
                node->setExpanded( false );
            }
            node->setText(SIZE_FLD_COLUMN, QString::number(file_info.size()));
            node->setText(DATE_FLD_COLUMN, file_info.lastModified().toString("yyyy/MM/dd hh:ss:mm"));
            QString attr;
            (file_info.isReadable()) ? attr.append("r"):attr.append("-");
            (file_info.isWritable()) ? attr.append("w"):attr.append("-");
            (file_info.isExecutable()) ? attr.append("x"):attr.append("-");
            node->setText(ATTR_FLD_COLUMN, attr);
            item->addChild(node);
        }
        if (item->childCount() > 0) item->setExpanded(true);
    }
    ui->treeWidget_local->header()->resizeSection(NAME_FLD_COLUMN, ui->treeWidget_local->geometry().width()/2);
    ui->treeWidget_local->sortItems(TYPE_FLD_COLUMN, Qt::AscendingOrder);
}

void MainWindow::remoteSlot(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) return;
    QString path = getPathname(item);
    //qDebug() << "Remote" << path;
    QString type = item->text(TYPE_FLD_COLUMN);
    if (type == FIL_TYPE_STRING) return;
    sendControlPacket(PACKET_TYPE_FLD_LST, path.toStdString().c_str(), path.length());    
    ui->treeWidget_remote->header()->resizeSection(NAME_FLD_COLUMN, ui->treeWidget_local->geometry().width()/2);
}

void MainWindow::listDiskDrives(QString drives, bool flag)
{
    QStringList drive_list = drives.split(QChar('\0'), QString::SkipEmptyParts);
    (flag)? ui->treeWidget_remote->clear() : ui->treeWidget_local->clear();
    for (int i = 0; i < drive_list.size(); i++) {
        QString drive_name = drive_list[i];
        QTreeWidgetItem * drive_leaf = new QTreeWidgetItem();
        drive_leaf->setText(NAME_FLD_COLUMN, drive_name);
        drive_leaf->setIcon(NAME_FLD_COLUMN, QIcon(":/res/drive.png"));
        drive_leaf->setText(TYPE_FLD_COLUMN, DRV_TYPE_STRING);
        (flag) ? ui->treeWidget_remote->addTopLevelItem(drive_leaf):ui->treeWidget_local->addTopLevelItem(drive_leaf);
    }
    (flag) ? ui->treeWidget_remote->sortByColumn(NAME_FLD_COLUMN, Qt::AscendingOrder):ui->treeWidget_local->sortByColumn(NAME_FLD_COLUMN, Qt::AscendingOrder);
}

void MainWindow::listRemoteDirs(QString dirInfo, bool flag)
{
    // dirInfo=parent_path\r\n{DIRENTRY}*
    // DIRENTRY=NAME;TYPE;SIZE;DATE;ATTR\r\n
    QString parent_path = dirInfo.section(HTTP_CRLF_MARK, 0, 0);
    QString drive_name = parent_path.section(":\\", 0, 0) + ":\\";
    parent_path = parent_path.section(":\\", 1);
    for (int i = 0; i < ui->treeWidget_remote->topLevelItemCount(); i++ ) {
        QTreeWidgetItem * item = ui->treeWidget_remote->topLevelItem(i);
        if (item->text(NAME_FLD_COLUMN) == drive_name) {
            while (!parent_path.isEmpty()) {
                QString folder = parent_path.section("\\", 0, 0);
                parent_path = parent_path.section("\\", 1);
                for (int j = 0; j < item->childCount(); j++) {
                    QTreeWidgetItem * subitem = item->child(j);
                    if (subitem->text(NAME_FLD_COLUMN) == folder) item = subitem;
                }
            }
            // reached to the leaf folder, clean first
            if (flag) for (int i = item->childCount() - 1; i >= 0 ; i-- ) item->removeChild(item->child(i));
            QString dir_entries = dirInfo.section(HTTP_CRLF_MARK, 1);
            if (dir_entries.isEmpty()) {
                if (flag) statusBar()->showMessage(parent_path + " is emptied", MSG_TIMER_PERIOD);
                else statusBar()->showMessage(parent_path + " is not permitted for new folder", MSG_TIMER_PERIOD);
            }
            else {
                do {
                    QString dir_entry = dir_entries.section(HTTP_CRLF_MARK, 0, 0);
                    QString entryName = dir_entry.section(FILED_DELIMETER, NAME_FLD_COLUMN, NAME_FLD_COLUMN).remove('"');
                    QTreeWidgetItem *node = NULL;
                    for (int i = 0; i < item->childCount(); i++) {
                        if (item->child(i)->text(NAME_FLD_COLUMN) == entryName) {
                            node = item->child(i);
                            break;
                        }
                    }
                    if (node == NULL) {
                        node = new QTreeWidgetItem;
                        node->setText(NAME_FLD_COLUMN, entryName);
                        node->setText(TYPE_FLD_COLUMN, dir_entry.section(FILED_DELIMETER, TYPE_FLD_COLUMN, TYPE_FLD_COLUMN));
                        if (node->text(TYPE_FLD_COLUMN) == FIL_TYPE_STRING) node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/file.png"));
                        else node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/folder.png"));
                        item->addChild(node);
                    }
                    node->setText(SIZE_FLD_COLUMN, dir_entry.section(FILED_DELIMETER, SIZE_FLD_COLUMN, SIZE_FLD_COLUMN));
                    node->setText(DATE_FLD_COLUMN, dir_entry.section(FILED_DELIMETER, DATE_FLD_COLUMN, DATE_FLD_COLUMN));
                    node->setText(ATTR_FLD_COLUMN, dir_entry.section(FILED_DELIMETER, ATTR_FLD_COLUMN, ATTR_FLD_COLUMN));
                    dir_entries = dir_entries.section(HTTP_CRLF_MARK, 1);
                } while(!dir_entries.isEmpty());
                ui->treeWidget_remote->sortItems(TYPE_FLD_COLUMN, Qt::AscendingOrder);
                if (item->childCount() > 0) item->setExpanded(true);
            }
            return;
        }
    }
//    qDebug() << "Unmatched" << drive_name << parent_path;
}

/*
void MainWindow::uploadData(QTreeWidgetItem* localItem, QTreeWidgetItem* remoteItem)
{
    QString localPath = getPathname(localItem);
    QString remotePath = getPathname(remoteItem);
    QString localType = localItem->text(TYPE_FLD_COLUMN);
    QString remoteType = remoteItem->text(TYPE_FLD_COLUMN);
    // if upload local folder to remote file, reset as remote folder
    if (localType != FIL_TYPE_STRING && remoteType == FIL_TYPE_STRING) remotePath = remotePath.section("\\", 0, -2);
    // if upload local file to remote folder, append file name to remote path
    else if (localType == FIL_TYPE_STRING && remoteType != FIL_TYPE_STRING) remotePath.append("\\" + localPath.section("\\", -1, -1));
    qDebug() << "Upload" << localPath << "to" << remotePath;
    if (localType == FIL_TYPE_STRING) sendFileData(localPath, remotePath, 0);
    else {
        QString newFolder = localItem->text(NAME_FLD_COLUMN);
        std::string newFolderPath = QString(remotePath + "\\" + newFolder).toStdString();
        sendControlPacket(PACKET_TYPE_FIL_PUT, newFolderPath.c_str(), newFolderPath.size());
        // add new folder at remote view
        QTreeWidgetItem *node = new QTreeWidgetItem;
        node->setText(NAME_FLD_COLUMN, newFolder);
        node->setText(TYPE_FLD_COLUMN, DIR_TYPE_STRING);
        node->setText(SIZE_FLD_COLUMN, "0");
        node->setText(DATE_FLD_COLUMN, QDateTime::currentDateTime().toString("yyyy/MM/dd hh:ss:mm"));
        node->setText(ATTR_FLD_COLUMN, "rw-");
        node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/folder.png"));
        remoteItem->addChild(node);
        ui->treeWidget_remote->sortItems(TYPE_FLD_COLUMN, Qt::AscendingOrder);
        for (int i = 0; i < localItem->childCount(); i++) {
            QTreeWidgetItem* subItem = localItem->child(i);
            uploadData(subItem, node);
        }
    }
}
*/

void MainWindow::sendFileData(QString localPath, QString remotePath, int offset)
{   
    // file upload
    FILE * fp = fopen(localPath.toStdString().c_str(), "rb");
    if (fp) {        
        if (fseek(fp, offset, SEEK_SET) == -1) qDebug() << localPath << "send seek failed" << offset;
        else {
            char buffer[BROWSER_PACKET_MAX_DATA];
            memset(&buffer[0], 0, BROWSER_PACKET_MAX_DATA);
            size_t ret = fread(&buffer[0], 1, BROWSER_PACKET_MAX_DATA, fp);
            if (ret > 0) {
                // local_path;remote_path;offset;file_size\r\nfile_data
                std::string file_data = localPath.toStdString() + FILED_DELIMETER + remotePath.toStdString() + FILED_DELIMETER + std::to_string(offset) + FILED_DELIMETER + std::to_string(ret) + HTTP_CRLF_MARK + std::string(&buffer[0], ret);
                sendControlPacket(PACKET_TYPE_FIL_PUT, file_data.c_str(), file_data.size());
            }
            else {
                // create new file treeview item
                QString drive_name = remotePath.section(":\\", 0, 0) + ":\\";
                remotePath = remotePath.section(":\\", 1);
                QString file_name = remotePath.section("\\", -1, -1);
                remotePath = remotePath.section("\\", 0, -2);
                for (int i = 0; i < ui->treeWidget_remote->topLevelItemCount(); i++ ) {
                    QTreeWidgetItem * item = ui->treeWidget_remote->topLevelItem(i);
                    if (item->text(NAME_FLD_COLUMN) == drive_name) {
                        while (!remotePath.isEmpty()) {
                            QString folder = remotePath.section("\\", 0, 0);
                            remotePath = remotePath.section("\\", 1);
                            for (int j = 0; j < item->childCount(); j++) {
                                QTreeWidgetItem * subitem = item->child(j);
                                if (subitem->text(NAME_FLD_COLUMN) == folder) item = subitem;
                            }
                        }
                        // reached to the leaf folder, create file node
                        QTreeWidgetItem * node = NULL;
                        for (int j = 0; j < item->childCount(); j++) {
                            if (item->child(j)->text(NAME_FLD_COLUMN) == file_name) {
                                node = item->child(j);
                                break;
                            }
                        }
                        if (!node) {
                            node = new QTreeWidgetItem;
                            node->setText(NAME_FLD_COLUMN, file_name);
                            node->setText(TYPE_FLD_COLUMN, FIL_TYPE_STRING);
                            node->setText(ATTR_FLD_COLUMN, "rw-");
                            node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/file.png"));
                            node->setText(SIZE_FLD_COLUMN, QString::number(offset));
                            node->setText(DATE_FLD_COLUMN, QDateTime::currentDateTime().toString("yyyy/MM/dd hh:ss:mm"));
                            item->addChild(node);
                            ui->treeWidget_remote->sortItems(TYPE_FLD_COLUMN, Qt::AscendingOrder);
                        }
                        else qDebug()  << "remote file is already exists" << file_name;
                        break;
                    }
                }
            }
        }
        fclose(fp);
    }
    else statusBar()->showMessage(localPath + " opening failed", MSG_TIMER_PERIOD);
}

/*
void MainWindow::downloadData(QTreeWidgetItem* localItem, QTreeWidgetItem* remoteItem)
{
    QString localPath = getPathname(localItem);
    QString remotePath = getPathname(remoteItem);
    QString localType = localItem->text(TYPE_FLD_COLUMN);
    QString remoteType = remoteItem->text(TYPE_FLD_COLUMN);
    // if download remote folder to local file, reset as local folder
    if (remoteType != FIL_TYPE_STRING && localType == FIL_TYPE_STRING) localPath = localPath.section("\\", 0, -2);
    // if download remote file to local folder, append filename to local path
    else if (remoteType == FIL_TYPE_STRING && localType != FIL_TYPE_STRING) localPath.append("\\" + remotePath.section("\\", -1, -1));
    qDebug() << "Download" << remotePath << "to" << localPath;
    if (remoteType == FIL_TYPE_STRING) {
        // local_path;remote_path;offset
        std::string file_data = localPath.toStdString() + FILED_DELIMETER + remotePath.toStdString() + FILED_DELIMETER + std::string("0");
        sendControlPacket(PACKET_TYPE_FIL_GET, file_data.c_str(), file_data.size());
    }
    else {  // folder copy
        QString newFolder = remoteItem->text(NAME_FLD_COLUMN);
        // create new folder
        QDir dir(localPath);
        dir.mkdir(newFolder);
        // add new folder at local view
        QTreeWidgetItem *node = new QTreeWidgetItem;
        node->setText(NAME_FLD_COLUMN, newFolder);
        node->setText(TYPE_FLD_COLUMN, DIR_TYPE_STRING);
        node->setText(SIZE_FLD_COLUMN, "0");
        node->setText(DATE_FLD_COLUMN, QDateTime::currentDateTime().toString("yyyy/MM/dd hh:ss:mm"));
        node->setText(ATTR_FLD_COLUMN, "rw-");
        node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/folder.png"));
        localItem->addChild(node);
        for (int i = 0; i < remoteItem->childCount(); i++) {
            QTreeWidgetItem* subItem = remoteItem->child(i);
            downloadData(node, subItem);
        }
    }
}
*/
QTime t;
int fsize = 0;
void MainWindow::saveFileData(QString localPath, QString remotePath, int offset, std::string file_data)
{    
    int file_size = (int)file_data.size();    
    if (file_size > 0) {
        if (offset == 0) {
            t.start();
            fsize = 0;
            remove(localPath.toStdString().c_str()); // try to remove original file
        }
        // save file data
        FILE* fp = fopen(localPath.toStdString().c_str(), "ab");
        if (!fp) {
            qDebug() << localPath << offset << "open failed";
            return;
        }
        if (fseek(fp, offset, SEEK_SET) == -1) qDebug() << localPath << "save seek failed" << offset; // move to file offset
        else {
            size_t ret = fwrite(file_data.c_str(), 1, file_size, fp);
            fsize += ret;
            if (ret > 0) {
                offset += ret;
                // request next file chunk: local_path;remote_path;offset
                std::string file_info = localPath.toStdString() + FILED_DELIMETER + remotePath.toStdString() + FILED_DELIMETER + std::to_string(offset);
                sendControlPacket(PACKET_TYPE_FIL_GET, file_info.c_str(), file_info.size());
            }
        }
        fclose(fp);
    }
    // if file data is arrived completely
    else {
        qDebug() << t.restart() << "file size" << fsize;
        // create new file treeview item
        QString drive_name = localPath.section(":\\", 0, 0) + ":\\";
        localPath = localPath.section(":\\", 1);
        QString file_name = localPath.section("\\", -1, -1);
        localPath = localPath.section("\\", 0, -2);
        for (int i = 0; i < ui->treeWidget_local->topLevelItemCount(); i++ ) {
            QTreeWidgetItem * item = ui->treeWidget_local->topLevelItem(i);
            if (item->text(NAME_FLD_COLUMN) == drive_name) {
                while (!localPath.isEmpty()) {
                    QString folder = localPath.section("\\", 0, 0);
                    localPath = localPath.section("\\", 1);
                    for (int j = 0; j < item->childCount(); j++) {
                        QTreeWidgetItem * subitem = item->child(j);
                        if (subitem->text(NAME_FLD_COLUMN) == folder) item = subitem;
                    }
                }
                // reached to the leaf folder, create file node
                QTreeWidgetItem * node = NULL;
                for (int j = 0; j < item->childCount(); j++) {
                    if (item->child(j)->text(NAME_FLD_COLUMN) == file_name) {
                        node = item->child(j);
                        break;
                    }
                }
                if (!node) {
                    node = new QTreeWidgetItem;
                    node->setText(NAME_FLD_COLUMN, file_name);
                    node->setText(TYPE_FLD_COLUMN, FIL_TYPE_STRING);
                    node->setText(ATTR_FLD_COLUMN, "rw-");
                    node->setIcon(NAME_FLD_COLUMN, QIcon(":/res/file.png"));
                    node->setText(SIZE_FLD_COLUMN, QString::number(offset));
                    node->setText(DATE_FLD_COLUMN, QDateTime::currentDateTime().toString("yyyy/MM/dd hh:ss:mm"));
                    item->addChild(node);
                    ui->treeWidget_local->sortItems(TYPE_FLD_COLUMN, Qt::AscendingOrder);
                }
                else qDebug()  << "local file is already exists" << file_name;
                return;
            }
        }
    }
}


