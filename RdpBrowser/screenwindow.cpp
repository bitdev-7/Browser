#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "../RdpProxy/browserclient.h"


void MainWindow::saveImageFile(std::string image_data , int iGridX, int iGridY) {

    if (!m_isCapturing)
        return;
   // imgData.clear();
    if (m_originalX != iGridX || m_originalY != iGridY) {   //a piece of screen completely transferred
        scrview->image[m_originalX][m_originalY].loadFromData(m_imgData);
        scrview->repaint();
        m_imgData.clear();
    }
    m_imgData.append(image_data.c_str(),image_data.size());
    m_originalX = iGridX;
    m_originalY = iGridY;
}
