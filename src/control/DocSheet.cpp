/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd.
*
* Author:     zhangsong<zhangsong@uniontech.com>
*
* Maintainer: zhangsong<zhangsong@uniontech.com>
*
* Central(NaviPage ViewPage)
*
* CentralNavPage(openfile)
*
* CentralDocPage(DocTabbar Sheets)
*
* Sheet(SheetSidebar SheetBrowser document)
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "DocSheet.h"
#include "lpreviewControl/SheetSidebar.h"
#include "SheetBrowser.h"
#include "Database.h"
#include "CentralDocPage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStackedWidget>
#include <QMimeData>
#include <QUuid>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>

DWIDGET_USE_NAMESPACE

QMap<QString, DocSheet *> DocSheet::g_map;
DocSheet::DocSheet(Dr::FileType type, QString filePath,  QWidget *parent)
    : DSplitter(parent), m_filePath(filePath), m_type(type)
{
    m_uuid = QUuid::createUuid().toString();
    g_map[m_uuid] = this;

    Database::instance()->readOperation(this);
    Database::instance()->readBookmarks(m_filePath, m_bookmarks);

    setHandleWidth(5);
    setChildrenCollapsible(false);  //  子部件不可拉伸到 0

//    m_sidebar = new SheetSidebar(this, PREVIEW_THUMBNAIL | PREVIEW_CATALOG | PREVIEW_BOOKMARK | PREVIEW_NOTE);
//    m_sidebar->setMinimumWidth(266);

    m_browser = new SheetBrowser(this);
    m_browser->setMinimumWidth(481);

    connect(m_browser, SIGNAL(sigPageChanged(int)), this, SLOT(onBrowserPageChanged(int)));
    connect(m_browser, SIGNAL(sigSizeChanged()), this, SLOT(onBrowserSizeChanged()));
    connect(m_browser, SIGNAL(sigWheelUp()), this, SLOT(onBrowserWheelUp()));
    connect(m_browser, SIGNAL(sigWheelDown()), this, SLOT(onBrowserWheelDown()));
    connect(m_browser, SIGNAL(sigNeedPagePrev()), this, SLOT(onBrowserPagePrev()));
    connect(m_browser, SIGNAL(sigNeedPageNext()), this, SLOT(onBrowserPageNext()));
    connect(m_browser, SIGNAL(sigNeedPageFirst()), this, SLOT(onBrowserPageFirst()));
    connect(m_browser, SIGNAL(sigNeedPageLast()), this, SLOT(onBrowserPageLast()));
    connect(m_browser, SIGNAL(sigNeedBookMark(int, bool)), this, SLOT(onBrowserBookmark(int, bool)));
}

DocSheet::~Sheet()
{
    Database::instance()->saveOperation(this);
    g_map.remove(m_uuid);
}

bool DocSheet::existFileChanged()
{
    foreach (DocSheet *sheet, g_map.values()) {
        if (sheet->fileChanged())
            return true;
    }

    return false;
}

QUuid DocSheet::getUuid(DocSheet *sheet)
{
    return g_map.key(sheet);
}

Sheet *DocSheet::getSheet(QString uuid)
{
    if (g_map.contains(uuid))
        return g_map[uuid];

    return nullptr;
}

void DocSheet::initOperationData(const SheetOperation &opera)
{
    m_operation = opera;
}

bool DocSheet::isOpen()
{
    //...
    return false;
}

void DocSheet::openFile()
{

}

void DocSheet::jumpToPage(int page)
{
    m_browser->setCurrentPage(page);
}

void DocSheet::jumpToIndex(int index)
{
    m_browser->setCurrentPage(index + 1);
}

void DocSheet::jumpToFirstPage()
{
    m_browser->setCurrentPage(1);
}

void DocSheet::jumpToLastPage()
{
    jumpToPage(m_browser->allPages());
}

void DocSheet::jumpToNextPage()
{
    int page = m_browser->currentPage() + (m_operation.layoutMode == Dr::TwoPagesMode ? 2 : 1);

    page = page >= m_browser->allPages() ? m_browser->allPages() : page;

    jumpToPage(page);
}

void DocSheet::jumpToPrevPage()
{
    int page = m_browser->currentPage() - (m_operation.layoutMode == Dr::TwoPagesMode ? 2 : 1);

    page = page < 1 ? 1 : page;

    jumpToPage(page);
}

deepin_reader::Outline DocSheet::outline()
{
    m_browser->outline();
}

void DocSheet::jumpToOutline(const qreal  &left, const qreal &top, unsigned int page)
{
    m_browser->jumpToOutline(left, top, page);
}

void DocSheet::jumpToHighLight(deepin_reader::Annotation *annotation)
{
    m_browser->jumpToHighLight(annotation);
}

void DocSheet::rotateLeft()
{
    if (Dr::RotateBy0 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy270;
    else if (Dr::RotateBy270 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy180;
    else if (Dr::RotateBy180 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy90;
    else if (Dr::RotateBy90 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy0;

    m_browser->deform(m_operation);
    m_sidebar->handleRotate(m_operation.rotation);
    emit sigFileChanged(this);
}

void DocSheet::rotateRight()
{
    if (Dr::RotateBy0 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy90;
    else if (Dr::RotateBy90 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy180;
    else if (Dr::RotateBy180 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy270;
    else if (Dr::RotateBy270 == m_operation.rotation)
        m_operation.rotation = Dr::RotateBy0;

    m_browser->deform(m_operation);
    m_sidebar->handleRotate(m_operation.rotation);
    emit sigFileChanged(this);
}

bool DocSheet::openFileExec()
{
    if (m_browser->openFilePath(filePath())) {
        if (!m_browser->loadPages(m_operation, m_bookmarks))
            return false;
        handleOpenSuccess();
        emit sigFileChanged(this);
        return true;
    }

    return false;
}

void DocSheet::setBookMark(int index, int state)
{
    if (state)
        m_bookmarks.insert(index);
    else {
        showTips(tr("The bookmark has been removed"));
        m_bookmarks.remove(index);
    }

    m_sidebar->setBookMark(index, state);
    m_browser->setBookMark(index, state);

    m_fileChanged = true;
    emit sigFileChanged(this);
}

int DocSheet::pagesNumber()
{
    if (m_browser)
        return m_browser->allPages();

    return 0;
}

int DocSheet::currentPage()
{
    if (m_operation.currentPage < 1 || m_operation.currentPage > pagesNumber())
        return 1;

    return m_operation.currentPage;
}

int DocSheet::currentIndex()
{
    if (m_operation.currentPage < 1 || m_operation.currentPage > pagesNumber())
        return 0;

    return m_operation.currentPage - 1;
}

void DocSheet::setLayoutMode(Dr::LayoutMode mode)
{
    if (mode == m_operation.layoutMode)
        return;

    if (mode >= Dr::SinglePageMode && mode < Dr::NumberOfLayoutModes) {
        m_operation.layoutMode = mode;
        m_browser->deform(m_operation);
        emit sigFileChanged(this);
    }
}

void DocSheet::setMouseShape(Dr::MouseShape shape)
{
    if (shape >= Dr::MouseShapeNormal && shape < Dr::NumberOfMouseShapes) {
        closeMagnifier();
        m_operation.mouseShape = shape;
        m_browser->setMouseShape(m_operation.mouseShape);
        emit sigFileChanged(this);
    }
}

void DocSheet::setScaleMode(Dr::ScaleMode mode)
{
    if (mode >= Dr::ScaleFactorMode && mode <= Dr::FitToPageWorHMode) {
        m_operation.scaleMode = mode;
        m_browser->deform(m_operation);
        emit sigFileChanged(this);
    }
}

void DocSheet::setScaleFactor(qreal scaleFactor)
{
    if (Dr::ScaleFactorMode == m_operation.scaleMode && qFuzzyCompare(scaleFactor, m_operation.scaleFactor))
        return;

    if (scaleFactor > maxScaleFactor())
        scaleFactor = maxScaleFactor();

    m_operation.scaleMode = Dr::ScaleFactorMode;
    m_operation.scaleFactor = scaleFactor;
    m_browser->deform(m_operation);
    emit sigFileChanged(this);
}

bool DocSheet::getImage(int index, QImage &image, double width, double height, Qt::AspectRatioMode mode)
{
    if (m_browser)
        return m_browser->getImage(index, image, width, height, mode);

    return false;
}

bool DocSheet::fileChanged()
{
    return m_fileChanged;
}

bool DocSheet::saveData()
{
    if (Database::instance()->saveBookmarks(filePath(), m_bookmarks)) {
        m_fileChanged = false;
        return true;
    }
    return false;
}

bool DocSheet::saveAsData(QString filePath)
{
    Database::instance()->saveBookmarks(filePath, m_bookmarks);
    return Utils::copyFile(this->filePath(), filePath);
}

void DocSheet::handleSearch()
{

}

void DocSheet::stopSearch()
{

}

void DocSheet::copySelectedText()
{

}

void DocSheet::highlightSelectedText()
{

}

void DocSheet::addSelectedTextHightlightAnnotation()
{

}

void DocSheet::openMagnifier()
{
    if (m_browser)
        m_browser->openMagnifier();
}

void DocSheet::closeMagnifier()
{
    if (m_browser)
        m_browser->closeMagnifier();
}

void DocSheet::defaultFocus()
{
    if (m_browser)
        m_browser->setFocus();
}

bool DocSheet::magnifierOpened()
{
    if (m_browser)
        return m_browser->magnifierOpened();
    return false;
}

//void DocSheet::docBasicInfo(stFileInfo &info)
//{
//    info.strFilepath = filePath();

//    QFileInfo fileInfo(filePath());
//    info.size = fileInfo.size();
//    info.CreateTime = fileInfo.birthTime();
//    info.ChangeTime = fileInfo.lastModified();
//    info.strAuther = fileInfo.owner();
//    info.strFilepath = fileInfo.filePath();

//    if (m_browser) {
//        info.strFormat = QString("PDFL");
////        info.boptimization = document->isLinearized();
////        info.strKeyword = document->keywords();
////        info.strTheme = document->title();
////        info.strProducter = document->producer();
////        info.strCreater = document->creator();
////        info.bsafe = document->isEncrypted();
//        info.iWidth = static_cast<unsigned int>(m_browser->maxWidth());
//        info.iHeight = static_cast<unsigned int>(m_browser->maxHeight());
//        info.iNumpages = static_cast<unsigned int>(m_browser->allPages());
//    }

//}

QList<deepin_reader::Annotation *> DocSheet::annotations()
{
    if (nullptr == m_browser)
        return QList< deepin_reader::Annotation * > ();
    return m_browser->annotations();
}

QString DocSheet::addIconAnnotation(const QPoint &pos)
{

}

bool DocSheet::removeAnnotation(deepin_reader::Annotation *annotation)
{
    if (nullptr == m_browser)
        return false;
    return m_browser->removeAnnotation(annotation);
}

QList<qreal> DocSheet::scaleFactorList()
{
    QList<qreal> dataList = {0.1, 0.25, 0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 3, 4, 5};
    QList<qreal> factorList;

    qreal maxFactor = maxScaleFactor();

    foreach (qreal factor, dataList) {
        if (maxFactor - factor > 0.001)
            factorList.append(factor);
    }

    return  factorList;
}

qreal DocSheet::maxScaleFactor()
{
    qreal maxScaleFactor = 20000 / (m_browser->maxHeight() * qApp->devicePixelRatio());

    if (maxScaleFactor < 0.1)
        maxScaleFactor = 0.1;

    return maxScaleFactor;
}

SheetOperation DocSheet::operation()
{
    return m_operation;
}

Dr::FileType DocSheet::type()
{
    return m_type;
}

QString DocSheet::filePath()
{
    return m_filePath;
}

bool DocSheet::hasBookMark(int index)
{
    return m_bookmarks.contains(index);
}

void DocSheet::zoomin()
{
    QList<qreal> dataList = scaleFactorList();

    for (int i = 0; i < dataList.count(); ++i) {
        if (dataList[i] > (m_operation.scaleFactor)) {
            setScaleFactor(dataList[i]);
            return;
        }
    }
}

void DocSheet::zoomout()
{
    QList<qreal> dataList = scaleFactorList();

    for (int i = dataList.count() - 1; i >= 0; --i) {
        if (dataList[i] < (m_operation.scaleFactor)) {
            setScaleFactor(dataList[i]);
            return;
        }
    }
}

void DocSheet::setSidebarVisible(bool isVisible)
{
    m_operation.sidebarVisible = isVisible;
    if (m_sidebar)
        m_sidebar->setVisible(isVisible);
    emit sigFileChanged(this);
}

void DocSheet::handleOpenSuccess()
{
    if (m_sidebar)
        m_sidebar->handleOpenSuccess();
}

void DocSheet::showTips(const QString &tips, int iconIndex)
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return;

    doc->showTips(tips, iconIndex);
}

void DocSheet::openSlide()
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return;

    doc->openSlide();
}

void DocSheet::closeSlide()
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return;

    doc->quitSlide();
}

bool DocSheet::slideOpened()
{
    return false;
}

void DocSheet::openFullScreen()
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return;

    doc->openFullScreen();
}

int DocSheet::label2pagenum(QString label)
{
    return 0;
}

bool DocSheet::haslabel()
{
    return false;
}

void DocSheet::docBasicInfo(deepin_reader::FileInfo &info)
{

}

QString DocSheet::pagenum2label(int index)
{
    return "";
}

void DocSheet::onBrowserPageChanged(int page)
{
    if (m_operation.currentPage != page) {
        m_operation.currentPage = page;
        m_sidebar->setCurrentPage(page);
    }
}

void DocSheet::onBrowserSizeChanged()
{
    if (m_operation.scaleMode != Dr::ScaleFactorMode) {
        m_browser->deform(m_operation);
        emit sigFileChanged(this);
    }
}

void DocSheet::onBrowserWheelUp()
{
    zoomin();
}

void DocSheet::onBrowserWheelDown()
{
    zoomout();
}

void DocSheet::onBrowserPageFirst()
{
    jumpToFirstPage();
}

void DocSheet::onBrowserPagePrev()
{
    jumpToPrevPage();
}

void DocSheet::onBrowserPageNext()
{
    jumpToNextPage();
}

void DocSheet::onBrowserPageLast()
{
    jumpToLastPage();
}

void DocSheet::onBrowserBookmark(int index, bool state)
{
    setBookMark(index, state);
}