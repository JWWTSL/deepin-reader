/*
 * Copyright (C) 2019 ~ 2020 UOS Technology Co., Ltd.
 *
 * Author:     wangzhxiaun
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
#include "docview/commonstruct.h"
#include "pdfControl/SheetSidebarPDF.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStackedWidget>
#include <QMimeData>

#include "widgets/SpinnerWidget.h"
#include "pdfControl/SheetBrowserPDF.h"
#include "CentralDocPage.h"
#include "app/ProcessController.h"
#include "docview/docummentproxy.h"

DocSheet::DocSheet(DocType_EM type, DWidget *parent)
    : DSplitter(parent), m_type(type)
{
    if (DocType_PDF == m_type) {
        initPDF();
    }
}

DocSheet::~DocSheet()
{
}

void DocSheet::openSliderShow()
{
//    int nState = getCurrentState();
//    if (nState != SLIDER_SHOW) {
//        setCurrentState(SLIDER_SHOW);

//        m_pTabBar->setVisible(false);

//        auto sheet = qobject_cast<DocSheet *>(m_pStackedLayout->currentWidget());
//        if (sheet) {
//            sheet->OnOpenSliderShow();

//            MainWindow::Instance()->SetSliderShowState(0);

//            QString sPath = sheet->qGetPath();

//            m_strSliderPath = sPath;

//            auto _proxy = getCurFileAndProxy(sPath);
//            _proxy->setAutoPlaySlide(true);
//            _proxy->showSlideModel();

//            if (m_pctrlwidget == nullptr) {
//                m_pctrlwidget = new PlayControlWidget(this);
//            }

//            m_pctrlwidget->setSliderPath(sPath);
//            int nScreenWidth = qApp->desktop()->geometry().width();
//            int inScreenHeght = qApp->desktop()->geometry().height();
//            m_pctrlwidget->activeshow((nScreenWidth - m_pctrlwidget->width()) / 2, inScreenHeght - 100);
//        }
//    }
}

void DocSheet::pageJump(const int &pagenum)
{
    if (DocType_PDF == m_type) {
        DocummentProxy *_proxy =  static_cast<SheetBrowserPDF *>(m_browser)->GetDocProxy();
        if (_proxy) {
            int nPageSize = _proxy->getPageSNum();      //  总页数
            if (pagenum < 0 || pagenum == nPageSize) {
                return;
            }

            int nCurPage = _proxy->currentPageNo();
            if (nCurPage != pagenum) {
                _proxy->pageJump(pagenum);
            }
        }
    }
}

void DocSheet::pageJumpByMsg(const int &iType, const QString &param)
{
    DocummentProxy *_proxy =  getDocProxy();

    if (_proxy) {
        int iPage = -1;
        if (iType == MSG_OPERATION_FIRST_PAGE) {
            iPage = 0;
        } else if (iType == MSG_OPERATION_PREV_PAGE) {
            int nCurPage = _proxy->currentPageNo();
            iPage = nCurPage - (param == "doubleshow" ? 2 : 1);
        } else if (iType == MSG_OPERATION_NEXT_PAGE) {
            int nCurPage = _proxy->currentPageNo();
            iPage = nCurPage + (param == "doubleshow" ? 2 : 1);
        } else if (iType == MSG_OPERATION_END_PAGE) {
            int nCurPage = _proxy->getPageSNum();
            iPage = nCurPage - 1;
        }

        pageJump(iPage);
    }
}

void DocSheet::setFileChanged(bool hasChanged)
{
    if (DocType_PDF == m_type)
        static_cast<SheetBrowserPDF *>(m_browser)->setFileChanged(hasChanged);
}

void DocSheet::initPDF()
{
    m_pRightWidget = new QStackedWidget(this);
    m_pSpinnerWidget = new SpinnerWidget(this);

    setHandleWidth(5);
    setChildrenCollapsible(false);  //  子部件不可拉伸到 0

    SheetBrowserPDF *browser = new SheetBrowserPDF(this);
    SheetSidebarPDF *sidebar = new SheetSidebarPDF(this);

    m_sidebar = sidebar;
    m_browser = browser;

    connect(sidebar, SIGNAL(sigDeleteAnntation(const int &, const QString &)), browser, SIGNAL(sigDeleteAnntation(const int &, const QString &)));
    connect(browser, SIGNAL(sigFileOpenResult(const QString &, const bool &)), SLOT(SlotFileOpenResult(const QString &, const bool &)));
    connect(browser, SIGNAL(sigFindOperation(const int &)), sidebar, SLOT(onSearch(const int &)));
    connect(browser, SIGNAL(sigAnntationMsg(const int &, const QString &)), sidebar, SIGNAL(sigAnntationMsg(const int &, const QString &)));
    connect(browser, SIGNAL(sigBookMarkMsg(const int &, const QString &)), sidebar, SIGNAL(sigBookMarkMsg(const int &, const QString &)));
    connect(browser, SIGNAL(sigUpdateThumbnail(const int &)), sidebar, SIGNAL(sigUpdateThumbnail(const int &)));
    connect(browser, SIGNAL(sigFileChanged(bool)), this, SIGNAL(sigFileChanged(bool)));

    int tW = 36;
    int tH = 36;
    dApp->adaptScreenView(tW, tH);
    m_pSpinnerWidget->setSpinnerSize(QSize(tW, tW));
    m_pSpinnerWidget->startSpinner();

    m_pRightWidget->addWidget(m_pSpinnerWidget);
    m_pRightWidget->addWidget(browser);

    addWidget(sidebar);
    addWidget(m_pRightWidget);

    QList<int> list_src;
    tW = LEFTNORMALWIDTH;
    dApp->adaptScreenView(tW, tH);
    list_src.append(tW);
    tW = 1000 - LEFTNORMALWIDTH;
    dApp->adaptScreenView(tW, tH);
    list_src.append(tW);

    setSizes(list_src);

    setAcceptDrops(true);

    //文档刚打开时，模拟鼠标点击文档区域事件
    QPoint pos(sidebar->geometry().x() + 10, sidebar->geometry().y() + 10);
    QMouseEvent event0(QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(sidebar, &event0);
}

void DocSheet::SlotFileOpenResult(const QString &s, const bool &res)
{
    if (res) {
        if (m_pRightWidget && m_pSpinnerWidget) {
            m_pRightWidget->removeWidget(m_pSpinnerWidget);

            delete  m_pSpinnerWidget;
            m_pSpinnerWidget = nullptr;
        }

        reloadFile();
    }
    emit sigOpenFileResult(s, res);
}

void DocSheet::reloadFile()
{
    if (DocType_PDF == m_type) {
        SheetSidebarPDF *sidebar = static_cast<SheetSidebarPDF *>(m_sidebar);
        if (sidebar) sidebar->handleOpenSuccess();
    }

    //title widget 也需要通知reload

}

void DocSheet::setSidebarVisible(bool isVisible)
{
    if (DocType_PDF == m_type) {
        SheetSidebarPDF *sidebar = static_cast<SheetSidebarPDF *>(m_sidebar);
        if (sidebar) sidebar->setVisible(isVisible);
    }
}

void DocSheet::SlotNotifyMsg(const int &msgType, const QString &msgContent)
{
    if (this->isVisible()) {    //  只有显示的窗口 处理 CentralDocPage 发送的信号
        if (msgType == MSG_NOTIFY_KEY_MSG) {
            auto cwlist = this->findChildren<CustomWidget *>();
            foreach (auto cw, cwlist) {
                int nRes = cw->qDealWithShortKey(msgContent);
                if (nRes == MSG_OK) {
                    break;
                }
            }
        } else {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(msgContent.toLocal8Bit().data(), &error);
            if (error.error == QJsonParseError::NoError) {
                QJsonObject obj = doc.object();

                QString sContent = obj.value("content").toString();

                int nRes = MSG_NO_OK;

                if (DocType_PDF == m_type)
                    nRes = static_cast<SheetSidebarPDF *>(m_sidebar)->dealWithData(msgType, sContent);

                if (nRes != MSG_OK) {
                    nRes = static_cast<SheetBrowserPDF *>(m_browser)->dealWithData(msgType, sContent);
                    if (nRes == MSG_OK)
                        return;
                }
            }
        }
    }
}

void DocSheet::onShowTips(const QString &tips)
{
    showTips(tips);
}

QString DocSheet::qGetPath()
{
    return static_cast<SheetBrowserPDF *>(m_browser)->getFilePath();
}

void DocSheet::qSetPath(const QString &strPath)
{
    static_cast<SheetBrowserPDF *>(m_browser)->OpenFilePath(strPath);  //  proxy 打开文件
}

int DocSheet::qGetFileChange()
{
    int istatus = -1;
    if (nullptr != static_cast<SheetBrowserPDF *>(m_browser))
        istatus = static_cast<SheetBrowserPDF *>(m_browser)->getFileChange() ? 1 : 0;
    return  istatus;
}

void DocSheet::saveData()
{
    static_cast<SheetBrowserPDF *>(m_browser)->saveData();
}

void DocSheet::setData(const int &nType, const QString &sValue)
{
    return static_cast<SheetBrowserPDF *>(m_browser)->setData(nType, sValue);
}

FileDataModel DocSheet::qGetFileData()
{
    return static_cast<SheetBrowserPDF *>(m_browser)->qGetFileData();
}

DocummentProxy *DocSheet::getDocProxy()
{
    if (DocType_PDF == m_type) {
        SheetBrowserPDF *browser = static_cast<SheetBrowserPDF *>(m_browser);
        if (browser)
            return browser->GetDocProxy();
    }

    return nullptr;
}

void DocSheet::OnOpenSliderShow()
{
    m_bOldState = static_cast<SheetSidebarPDF *>(m_sidebar)->isVisible();
    static_cast<SheetSidebarPDF *>(m_sidebar)->setVisible(false);
}

void DocSheet::OnExitSliderShow()
{
    static_cast<SheetSidebarPDF *>(m_sidebar)->setVisible(m_bOldState);
}

void DocSheet::ShowFindWidget()
{
    static_cast<SheetBrowserPDF *>(m_browser)->ShowFindWidget();
}

DocType_EM DocSheet::type()
{
    return m_type;
}

void DocSheet::showTips(const QString &tips)
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return;

    doc->showTips(tips);
}

int DocSheet::getCurrentState()
{
    CentralDocPage *doc = static_cast<CentralDocPage *>(parent());
    if (nullptr == doc)
        return Default_State;

    return doc->getCurrentState();
}

void DocSheet::setCurrentState(int state)
{
    m_currentState = state;
}