/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2006, 2007 crypton
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#include <QContextMenuEvent>
#include <QMenu>
#include <QCheckBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QUrl>
#include <QMimeData>

#include <retroshare/rsfiles.h>
#include <retroshare/rstypes.h>
#include <retroshare/rspeers.h>

#include "ShareManager.h"
#include "ShareDialog.h"
#include "settings/rsharesettings.h"
#include "gui/common/GroupFlagsWidget.h"
#include "gui/common/GroupSelectionBox.h"
#include "gui/common/GroupDefs.h"
#include "gui/notifyqt.h"
#include "util/QtVersion.h"

/* Images for context menu icons */
#define IMAGE_CANCEL               ":/images/delete.png"
#define IMAGE_EDIT                 ":/images/edit_16.png"

#define COLUMN_PATH         0
#define COLUMN_VIRTUALNAME  1
#define COLUMN_SHARE_FLAGS  2
#define COLUMN_GROUPS       3

ShareManager *ShareManager::_instance = NULL ;

/** Default constructor */
ShareManager::ShareManager()
  : QDialog(NULL, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint)
{
    /* Invoke Qt Designer generated QObject setup routine */
    ui.setupUi(this);

    ui.headerFrame->setHeaderImage(QPixmap(":/images/fileshare64.png"));
    ui.headerFrame->setHeaderText(tr("Share Manager"));

    isLoading = false;
    load();

    Settings->loadWidgetInformation(this);

    connect(ui.addButton, SIGNAL(clicked( bool ) ), this , SLOT( addShare() ) );
    connect(ui.closeButton, SIGNAL(clicked()), this, SLOT(applyAndClose()));
    connect(ui.cancelButton, SIGNAL(clicked()), this, SLOT(cancel()));

    connect(ui.shareddirList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(shareddirListCostumPopupMenu(QPoint)));
    connect(ui.shareddirList, SIGNAL(currentCellChanged(int,int,int,int)), this, SLOT(shareddirListCurrentCellChanged(int,int,int,int)));

    connect(ui.shareddirList, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(doubleClickedCell(int,int)));

    connect(NotifyQt::getInstance(), SIGNAL(groupsChanged(int)), this, SLOT(reload()));

    QHeaderView* header = ui.shareddirList->horizontalHeader();
    QHeaderView_setSectionResizeModeColumn(header, COLUMN_PATH, QHeaderView::Stretch);

    //header->setResizeMode(COLUMN_NETWORKWIDE, QHeaderView::Fixed);
    //header->setResizeMode(COLUMN_BROWSABLE, QHeaderView::Fixed);

    header->setHighlightSections(false);

    setAcceptDrops(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
}

void ShareManager::doubleClickedCell(int row,int column)
{
    if(column == COLUMN_PATH)
    {
        QString dirname = QFileDialog::getExistingDirectory(NULL,tr("Choose directory"),QString(),QFileDialog::DontUseNativeDialog | QFileDialog::ShowDirsOnly);

        if(!dirname.isNull())
        {
            std::string new_name( dirname.toUtf8() );

            for(uint32_t i=0;i<mDirInfos.size();++i)
                if(mDirInfos[row].filename == new_name)
                    return ;

            mDirInfos[row].filename = new_name ;
            load();
        }
    }
    else if(column == COLUMN_GROUPS)
    {
        std::list<RsNodeGroupId> selected_groups = GroupSelectionDialog::selectGroups(std::list<RsNodeGroupId>()) ;

        mDirInfos[row].parent_groups = selected_groups ;
        load();
    }
}

ShareManager::~ShareManager()
{
    _instance = NULL;

    Settings->saveWidgetInformation(this);
}

void ShareManager::cancel()
{
    close();
}
void ShareManager::applyAndClose()
{
    // This is the only place where we change things.

    std::list<SharedDirInfo> infos ;

    for(uint32_t i=0;i<mDirInfos.size();++i)
        infos.push_back(mDirInfos[i]) ;

    rsFiles->setSharedDirectories(infos) ;

	close() ;
}

void ShareManager::shareddirListCostumPopupMenu( QPoint /*point*/ )
{
    QMenu contextMnu( this );

    QAction *editAct = new QAction(QIcon(IMAGE_EDIT), tr( "Edit" ), &contextMnu );
    connect( editAct , SIGNAL( triggered() ), this, SLOT( editShareDirectory() ) );

    QAction *removeAct = new QAction(QIcon(IMAGE_CANCEL), tr( "Remove" ), &contextMnu );
    connect( removeAct , SIGNAL( triggered() ), this, SLOT( removeShareDirectory() ) );

    contextMnu.addAction( editAct );
    contextMnu.addAction( removeAct );

    contextMnu.exec(QCursor::pos());
}

void ShareManager::reload()
{
    std::list<SharedDirInfo> dirs ;
    rsFiles->getSharedDirectories(dirs) ;

    for(std::list<SharedDirInfo>::const_iterator it(dirs.begin());it!=dirs.end();++it)
        mDirInfos.push_back(*it) ;

    load();
}

/** Loads the settings for this page */
void ShareManager::load()
{
    if(isLoading)
        return ;

    isLoading = true;

    /* get a link to the table */
    QTableWidget *listWidget = ui.shareddirList;

    /* set new row count */
    listWidget->setRowCount(mDirInfos.size());

    for(uint32_t row=0;row<mDirInfos.size();++row)
    {
        listWidget->setItem(row, COLUMN_PATH, new QTableWidgetItem(QString::fromUtf8(mDirInfos[row].filename.c_str())));
        listWidget->setItem(row, COLUMN_VIRTUALNAME, new QTableWidgetItem(QString::fromUtf8(mDirInfos[row].virtualname.c_str())));

        GroupFlagsWidget *widget = new GroupFlagsWidget(NULL,mDirInfos[row].shareflags);

        listWidget->setRowHeight(row, 32 * QFontMetricsF(font()).height()/14.0);
        listWidget->setCellWidget(row, COLUMN_SHARE_FLAGS, widget);

        listWidget->setItem(row, COLUMN_GROUPS, new QTableWidgetItem()) ;
        listWidget->item(row,COLUMN_GROUPS)->setBackgroundColor(QColor(183,236,181)) ;
        listWidget->item(row,COLUMN_GROUPS)->setText(getGroupString(mDirInfos[row].parent_groups));

        connect(widget,SIGNAL(flagsChanged(FileStorageFlags)),this,SLOT(updateFlags())) ;

        listWidget->item(row,COLUMN_PATH)->setToolTip(tr("Double click to change shared directory path")) ;
        listWidget->item(row,COLUMN_GROUPS)->setToolTip(tr("Double click to select which groups of friends can see the files")) ;
        listWidget->item(row,COLUMN_VIRTUALNAME)->setToolTip(tr("Double click to change the cirtual file name")) ;
    }

    listWidget->setColumnWidth(COLUMN_SHARE_FLAGS,132 * QFontMetricsF(font()).height()/14.0) ;

    listWidget->update(); /* update display */
    update();

    isLoading = false ;
}

void ShareManager::showYourself()
{
    if(_instance == NULL)
        _instance = new ShareManager() ;

    _instance->reload() ;
    _instance->show() ;
    _instance->activateWindow();
}

/*static*/ void ShareManager::postModDirectories(bool update_local)
{
   if (_instance == NULL || _instance->isHidden()) {
       return;
   }

   if (update_local) {
       _instance->reload();
   }
}

void ShareManager::updateFlags()
{
    if(isLoading)
        return ;

    isLoading = true ;	// stops GUI update. Otherwise each call to rsFiles->updateShareFlags() modifies the GUI that we count on to check
    // what has changed => fail!

    for(int row=0;row<ui.shareddirList->rowCount();++row)
    {
        FileStorageFlags flags = (dynamic_cast<GroupFlagsWidget*>(ui.shareddirList->cellWidget(row,COLUMN_SHARE_FLAGS)))->flags() ;

        mDirInfos[row].shareflags = flags ;
    }

    isLoading = false ;	// re-enable GUI load
    load() ;				// update the GUI.
}

// void ShareManager::updateFromWidget()
// {
//     mDirInfos.clear();
//
//     for(uint32_t i=0;i<ui.shareddirList.rows();++i)
//     {
//         SharedDirInfo sdi ;
//         sdi.filename       = ui.shareddirList->item(i,COLUMN_PATH)->text().toUtf8() ;
//         sdi.virtualname    = ui.shareddirList->item(i,COLUMN_VIRTUALNAME)->text().toUtf8() ;
//         sdi.shareflags     = dynamic_cast<GroupFlagsWidget*>(ui.shareddirList->item(i,COLUMN_SHARE_FLAGS))->flags();
//         sdi.parent_groups  = std::list<RsNodeGroupId>();//ui.shareddirList->item(i,COLUMN_GROUPS)->text();
//     }
// }

QString ShareManager::getGroupString(const std::list<RsNodeGroupId>& groups)
{
    int n = 0;
    QString group_string ;

    for (std::list<RsNodeGroupId>::const_iterator it(groups.begin());it!=groups.end();++it,++n)
    {
        if (n>0)
            group_string += ", " ;

        RsGroupInfo groupInfo;
        rsPeers->getGroupInfo(*it, groupInfo);
        group_string += GroupDefs::name(groupInfo);
    }

    return group_string ;
}

// void ShareManager::editShareDirectory()
// {
//     /* id current dir */
//     int row = ui.shareddirList->currentRow();
//     QTableWidgetItem *item = ui.shareddirList->item(row, COLUMN_PATH);
//
//     if (item) {
//         std::string filename = item->text().toUtf8().constData();
//
//         std::list<SharedDirInfo> dirs;
//         rsFiles->getSharedDirectories(dirs);
//
//         std::list<SharedDirInfo>::const_iterator it;
//         for (it = dirs.begin(); it != dirs.end(); ++it) {
//             if (it->filename == filename) {
//                 /* file name found, show dialog */
//                 ShareDialog sharedlg (it->filename, this);
//                 sharedlg.setWindowTitle(tr("Edit Shared Folder"));
//                 sharedlg.exec();
//                 load();
//                 break;
//             }
//         }
//     }
// }

void ShareManager::removeShareDirectory()
{
    /* id current dir */
    /* ask for removal */
    QTableWidget *listWidget = ui.shareddirList;
    int row = listWidget -> currentRow();
    QTableWidgetItem *qdir = listWidget->item(row, COLUMN_PATH);

    if (qdir)
    {
        if ((QMessageBox::question(this, tr("Warning!"),tr("Do you really want to stop sharing this directory ?"),QMessageBox::Yes|QMessageBox::No, QMessageBox::Yes))== QMessageBox::Yes)
        {
            for(uint32_t i=row;i+1<mDirInfos.size();++i)
                mDirInfos[i] = mDirInfos[i+1] ;

            load();
        }
    }
}

void ShareManager::showEvent(QShowEvent *event)
{
    if (!event->spontaneous())
    {
        load();
    }
}

void ShareManager::addShare()
{
    QString fname = QFileDialog::getExistingDirectory(NULL,tr("Choose a directory to share"),QString(),QFileDialog::DontUseNativeDialog | QFileDialog::ShowDirsOnly);

    if(fname.isNull())
        return;

    std::string dir_name ( fname.toUtf8() );

    // check that the directory does not already exist

    for(uint32_t i=0;i<mDirInfos.size();++i)
        if(mDirInfos[i].filename == dir_name)
            return ;

    mDirInfos.push_back(SharedDirInfo());
    mDirInfos.back().filename = dir_name ;
    mDirInfos.back().virtualname = std::string();
    mDirInfos.back().shareflags = DIR_FLAGS_ANONYMOUS_DOWNLOAD | DIR_FLAGS_ANONYMOUS_SEARCH;
    mDirInfos.back().parent_groups.clear();

    load();
}

void ShareManager::showShareDialog()
{
    ShareDialog sharedlg ("", this);
    sharedlg.exec();
    load();
}

void ShareManager::shareddirListCurrentCellChanged(int currentRow, int currentColumn, int previousRow, int previousColumn)
{
    Q_UNUSED(currentColumn);
    Q_UNUSED(previousRow);
    Q_UNUSED(previousColumn);
}

void ShareManager::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		event->acceptProposedAction();
	}
}

void ShareManager::dropEvent(QDropEvent *event)
{
	if (!(Qt::CopyAction & event->possibleActions())) {
		/* can't do it */
		return;
	}

	QStringList formats = event->mimeData()->formats();
	QStringList::iterator it;

	bool errorShown = false;

	if (event->mimeData()->hasUrls()) {
		QList<QUrl> urls = event->mimeData()->urls();
		QList<QUrl>::iterator it;
		for (it = urls.begin(); it != urls.end(); ++it) {
			QString localpath = it->toLocalFile();

			if (localpath.isEmpty() == false) {
				QDir dir(localpath);
				if (dir.exists()) {
					SharedDirInfo sdi;
					sdi.filename = localpath.toUtf8().constData();
					sdi.virtualname.clear();

					sdi.shareflags.clear() ;

					/* add new share */
					rsFiles->addSharedDirectory(sdi);
				} else if (QFile::exists(localpath)) {
					if (errorShown == false) {
						QMessageBox mb(tr("Drop file error."), tr("File can't be dropped, only directories are accepted."), QMessageBox::Information, QMessageBox::Ok, 0, 0, this);
						mb.exec();
						errorShown = true;
					}
				} else {
					QMessageBox mb(tr("Drop file error."), tr("Directory not found or directory name not accepted."), QMessageBox::Information, QMessageBox::Ok, 0, 0, this);
					mb.exec();
				}
			}
		}
	}

	event->setDropAction(Qt::CopyAction);
	event->accept();

    load();
}
