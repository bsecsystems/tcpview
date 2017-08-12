/* This file is part of tcpview - network connections viewer for Linux
 * Copyright (C) 2017 chipmunk-sm <dannico@linuxmail.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCloseEvent>
#include <QSettings>
#include <QMessageBox>
#include <QTextEdit>
#include <QClipboard>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_captureEnable(false)
{

    ui->setupUi(this);

    setWindowIcon(QPixmap(":/data/tcpviewb.svg"));

    m_cconnectionstree.InitConnectonsTree(ui->treeView_connection);

    m_ccfontsize.ConfigureSlider(ui->horizontalSlider, this);

    connect(this, SIGNAL(callUpdateGui()),   this, SLOT(updateGui()));

    connect(ui->treeView_connection->selectionModel(),
            SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            this,
            SLOT(currentSelectionChanged(const QModelIndex, const QModelIndex)));

    ui->pushButton_whois->setEnabled(false);

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_displayNames_clicked()
{
    ui->pushButton_displayNames->setEnabled(false);

    m_rootResolver.start( "pkexec " + QCoreApplication::applicationFilePath() + " --rootmodule");

    m_rootResolver.waitForStarted();

    if(!m_NetData.InitRootLoader())
    {
        ui->pushButton_displayNames->setEnabled(true);
        QMessageBox::critical(this, QObject::tr("Run as root"), QObject::tr("Failed"), QMessageBox::Ok);
        return;
    }
}

void MainWindow::on_pushButton_Pause_toggled(bool checked)
{
    m_NetData.pauseUpdate(checked);
}

void MainWindow::on_pushButton_Record_toggled(bool checked)
{
    m_captureEnable = checked;
}

void MainWindow::on_pushButton_copyToClipboard_clicked()
{
    auto pClp = QApplication::clipboard();
    if( pClp != nullptr )
        pClp->setText(m_ClipBoardString);
}

void MainWindow::on_pushButton_SaveToFile_clicked()
{
    auto pause = m_NetData.IsPause();
    m_NetData.pauseUpdate(true);
    m_cconnectionstree.Save(ui->treeView_connection);
    m_NetData.pauseUpdate(pause);
}

void MainWindow::on_pushButton_whois_clicked()
{
    ShowWhois(m_RowText, m_whoisText);
}

void MainWindow::on_horizontalSlider_valueChanged(int value)
{
    m_ccfontsize.SetFontSize(value);
}

void MainWindow::on_lineEdit_include_textChanged(const QString &arg1)
{
    m_cconnectionstree.UpdateIncludeFilter(arg1);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_NetData.DeleteRootLoader();
    SaveAppState();
    event->accept();
}

void MainWindow::SaveAppState()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue(DEFCFG_MAINWINDOWGEOM, saveGeometry());
    settings.setValue(DEFCFG_CONNECTIONTABLE, ui->treeView_connection->header()->saveState());
    m_ccfontsize.SaveConfig();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent( event );

    try
    {
        m_ccfontsize.Init();

        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        auto testv = settings.value(DEFCFG_MAINWINDOWGEOM);

        if(testv.isValid() && !testv.isNull())
        {
            restoreGeometry(settings.value(DEFCFG_MAINWINDOWGEOM).toByteArray());
            ui->treeView_connection->header()->restoreState(settings.value(DEFCFG_CONNECTIONTABLE, "").toByteArray());
        }

        m_cconnectionstree.SetDataColumnHiden(ui->treeView_connection);

        m_NetData.setUpdateCallback([&](void)->void{ emit callUpdateGui(); });

    }
    catch(...)
    {
        QMessageBox::critical(this, windowTitle(), QObject::tr("Failed on showEvent"), QMessageBox::Ok);
    }
}

void MainWindow::updateGui()
{

    try
    {

        auto dataHelper = m_NetData.GetData();
        if(dataHelper != nullptr)
        {

            /* start update code here */
            auto captureEnable = m_captureEnable;

            auto connList = dataHelper->GetConnectionsList();
            for (auto it = connList->begin(); it != connList->end(); it++ )
            {
                m_cconnectionstree.UpdateData(&it->second, captureEnable);
            }

            if(!captureEnable)
            {
                for (auto it = connList->begin(); it != connList->end(); )
                {
                    if (it->second.deleteItem > 0)
                        it = connList->erase(it);
                    else
                        ++it;
                }
            }

            m_NetData.EnableUpdateData();

            /* end update code here */

        }
    }
    catch(std::exception &e)
    {
        QMessageBox::critical(this, this->windowTitle(), e.what(), QMessageBox::Ok);
        exit(-1);
    }
    catch(...)
    {
        QMessageBox::critical(this, this->windowTitle(), "Unexpected exception", QMessageBox::Ok);
        exit(-1);
    }


}

void MainWindow::currentSelectionChanged(const QModelIndex current, const QModelIndex previous)
{

    Q_UNUSED(previous);

    m_whoisText = "";
    m_RowText = "";

    if(!current.isValid())
    {
        ui->pushButton_whois->setEnabled(false);
        ui->pushButton_whois->setText("Whois");
        return;
    }

    auto row = current.row();

    auto src = qobject_cast<QSortFilterProxyModel*>(ui->treeView_connection->model());
    if(src == nullptr)
    {
        ui->pushButton_whois->setEnabled(false);
        ui->pushButton_whois->setText("Whois");
        return;
    }

    QString tmpToolTipStr;

    for(auto index = 0; index < CDataSource::COLUMN_DATA_DATA; index++)
    {
        auto tmpStr = src->data(src->index(row, index)).toString();

        m_RowText += tmpStr + "\t";
        tmpToolTipStr +=  tmpStr + "\n";

        if(index == CDataSource::COLUMN_DATA_REMOTEADDRESS)
        {
           auto whoisText = src->data(src->index(row, index)).toString();
            if(QString::compare(whoisText, "0.0.0.0", Qt::CaseInsensitive) != 0 &&
               QString::compare(whoisText, "::", Qt::CaseInsensitive) != 0)
            {
                m_whoisText = whoisText;
                ui->pushButton_whois->setText("Whois \"" + whoisText + "\"");
                ui->pushButton_whois->setEnabled(true);
            }
            else
            {
                ui->pushButton_whois->setEnabled(false);
                ui->pushButton_whois->setText("Whois");
            }

        }
    }

    ui->pushButton_copyToClipboard->setToolTip(tmpToolTipStr);

    m_ClipBoardString = m_RowText;

}

void MainWindow::ShowWhois(QString rowText, QString whoisText)
{
    rowText.replace("\t"," ");
    rowText += "\n\n";

    QProcess exec;
    QString commandString = "whois " + whoisText;
    exec.start(commandString);
    exec.waitForFinished();
    rowText += exec.readAllStandardOutput();
    ShowInfoDialog(commandString, rowText, true);
}

void MainWindow::ShowInfoDialog(QString title, QString dialogText, bool readonly)
{
    auto infoDialog = new QDialog(this);
    infoDialog->setWindowTitle(title);
    infoDialog->setWindowFlags(infoDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto infoEditor = new QTextEdit;
    infoEditor->setLineWrapMode(QTextEdit::NoWrap);
    infoEditor->setPlainText(dialogText);

    if(readonly)
    {
        infoEditor->setReadOnly(true);
     }

    QFont newfont("monospace");
    newfont.setStyleHint(QFont::Monospace);
    newfont.setPointSize((this->font()).pointSize());
    infoEditor->setFont(newfont);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(infoEditor );
    infoDialog->setLayout(mainLayout);

    infoDialog->setAttribute(Qt::WA_DeleteOnClose);// ?
    auto newsize = this->size();

    infoDialog->resize(newsize.width()/2, newsize.height()/2);
    infoDialog->show();
    infoDialog->raise();
    infoDialog->activateWindow();
}

