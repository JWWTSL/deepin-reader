/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     duanxiaohui
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
#include "ThumbnailWidget.h"

#include <QJsonObject>

#include "controller/DataManager.h"
#include "controller/AppSetting.h"
#include "docview/docummentproxy.h"

ThumbnailWidget::ThumbnailWidget(DWidget *parent)
    : CustomWidget(QString("ThumbnailWidget"), parent)
{
    m_ThreadLoadImage.setThumbnail(this);

    initWidget();

    initConnection();

    dApp->m_pModelService->addObserver(this);
}

ThumbnailWidget::~ThumbnailWidget()
{
    dApp->m_pModelService->removeObserver(this);

    // 等待子线程退出
    if (m_ThreadLoadImage.isRunning()) {
        m_ThreadLoadImage.stopThreadRun();
        m_ThreadLoadImage.clearList();
    }
}

// 处理消息事件
int ThumbnailWidget::dealWithData(const int &msgType, const QString &msgContent)
{
    if (MSG_OPERATION_OPEN_FILE_OK == msgType) {
        emit sigOpenFileOk();
    } else if (MSG_CLOSE_FILE == msgType) {
        emit sigCloseFile();
    } else if (msgType == MSG_OPERATION_UPDATE_THEME) {
        emit sigUpdateTheme();
    } else if (MSG_FILE_PAGE_CHANGE == msgType) {
        emit sigFilePageChanged(msgContent);
    } else if (msgType == MSG_FILE_ROTATE) {  //  文档旋转了
        emit sigSetRotate(msgContent.toInt());
    }
    return 0;
}

// 初始化界面
void ThumbnailWidget::initWidget()
{
    m_pThumbnailListWidget = new CustomListWidget;
    m_pThumbnailListWidget->setSpacing(8);

    m_pPageWidget = new PagingWidget;

    auto m_pvBoxLayout = new QVBoxLayout;
    m_pvBoxLayout->setContentsMargins(0, 0, 0, 0);
    m_pvBoxLayout->setSpacing(0);

    m_pvBoxLayout->addWidget(m_pThumbnailListWidget);
    m_pvBoxLayout->addWidget(new DHorizontalLine(this));
    m_pvBoxLayout->addWidget(m_pPageWidget);

    this->setLayout(m_pvBoxLayout);
}

//  画 选中项的背景颜色
void ThumbnailWidget::setSelectItemBackColor(QListWidgetItem *item)
{
    //  先清空之前的选中颜色
    auto curItem = m_pThumbnailListWidget->currentItem();
    if (curItem) {
        auto itemWidget =
            reinterpret_cast<ThumbnailItemWidget *>(m_pThumbnailListWidget->itemWidget(curItem));
        if (itemWidget) {
            itemWidget->setBSelect(false);
        }
    }

    if (item) {
        m_pThumbnailListWidget->setCurrentItem(item);
        auto pWidget =
            reinterpret_cast<ThumbnailItemWidget *>(m_pThumbnailListWidget->itemWidget(item));
        if (pWidget) {
            pWidget->setBSelect(true);
        }
    }
}

//  添加项
void ThumbnailWidget::addThumbnailItem(const int &iIndex)
{
    auto widget = new ThumbnailItemWidget(this);
    widget->setLabelPage(iIndex);

    auto item = new QListWidgetItem(m_pThumbnailListWidget);
    item->setFlags(Qt::NoItemFlags);

    item->setSizeHint(QSize(LEFTMINWIDTH, 212));

    m_pThumbnailListWidget->addItem(item);
    m_pThumbnailListWidget->setItemWidget(item, widget);
}

void ThumbnailWidget::initConnection()
{
    connect(&m_ThreadLoadImage, SIGNAL(sigLoadImage(const int &, const QImage &)),
            this, SLOT(slotLoadImage(const int &, const QImage &)));

    connect(this, SIGNAL(sigOpenFileOk()), this, SLOT(slotOpenFileOk()));
    connect(this, SIGNAL(sigCloseFile()), this, SLOT(slotCloseFile()));
    connect(this, SIGNAL(sigUpdateTheme()), SLOT(slotUpdateTheme()));
    connect(this, SIGNAL(sigFilePageChanged(const QString &)),
            SLOT(slotDocFilePageChanged(const QString &)));
    connect(this, SIGNAL(sigSetRotate(int)), this, SLOT(slotSetRotate(int)));
    connect(&m_ThreadLoadImage, SIGNAL(sigRotateImage(int)), this,
            SLOT(slotRotateThumbnail(int)));
    connect(m_pThumbnailListWidget, SIGNAL(sigValueChanged(int)), this, SLOT(slotLoadThumbnail(int)));

}

//  文件  当前页变化, 获取与 文档页  对应的 item, 设置 选中该item, 绘制item
void ThumbnailWidget::slotDocFilePageChanged(const QString &sPage)
{
    int page = sPage.toInt();
    auto curPageItem = m_pThumbnailListWidget->item(page);
    if (curPageItem) {
        auto curItem = m_pThumbnailListWidget->currentItem();
        if (curItem == curPageItem)
            return;

        setSelectItemBackColor(curPageItem);
    }
}

// 关联关闭文件槽函数
void ThumbnailWidget::slotCloseFile()
{
    if (m_ThreadLoadImage.isRunning()) {
        m_ThreadLoadImage.clearList();
        m_ThreadLoadImage.stopThreadRun();
    }
}

void ThumbnailWidget::slotUpdateTheme()
{
    updateWidgetTheme();
}

void ThumbnailWidget::slotSetRotate(int angle)
{
    m_nRotate = angle;
    if (m_ThreadLoadImage.isRunning()) {
        m_ThreadLoadImage.stopThreadRun();
    }

    m_ThreadLoadImage.setIsLoaded(true);
    m_ThreadLoadImage.start();
}

void ThumbnailWidget::slotRotateThumbnail(int index)
{
    if (m_pThumbnailListWidget) {
        if (index >= m_pThumbnailListWidget->count()) {
            return;
        }
        auto item = m_pThumbnailListWidget->item(index);
        if (item) {
            auto pWidget = reinterpret_cast<ThumbnailItemWidget *>(m_pThumbnailListWidget->itemWidget(item));
            if (pWidget) {
                pWidget->rotateThumbnail(m_nRotate);
            }
        }
    }
}

/**
 * @brief ThumbnailWidget::slotLoadThumbnail
 * 根据纵向滚动条来加载缩略图
 * value 为当前滚动条移动的数值,最小是0，最大是滚动条的最大值
 */
void ThumbnailWidget::slotLoadThumbnail(int value)
{
    if (m_nValuePreIndex < 1 || value < 1)
        return;

    int loadStart = 0;
    int loadEnd = 0;
    int indexPage = 0;

    indexPage = value / m_nValuePreIndex;

    if (indexPage < 0 || indexPage > m_totalPages || m_ThreadLoadImage.inLoading(indexPage)) {
        return;
    }

    loadStart = (indexPage - (FIRST_LOAD_PAGES / 2)) < 0 ? 0 : (indexPage - (FIRST_LOAD_PAGES / 2));
    loadEnd = (indexPage + (FIRST_LOAD_PAGES / 2)) > m_totalPages ? m_totalPages : (indexPage + (FIRST_LOAD_PAGES / 2));
    if (m_ThreadLoadImage.isRunning()) {
        m_ThreadLoadImage.stopThreadRun();
    }
    m_ThreadLoadImage.setStartAndEndIndex(loadStart, loadEnd);
    m_ThreadLoadImage.setIsLoaded(true);
    m_ThreadLoadImage.start();
}

void ThumbnailWidget::slotLoadImage(const int &row, const QImage &image)
{
    if (!m_pThumbnailListWidget) {
        return;
    }
    auto item = m_pThumbnailListWidget->item(row);
    if (item) {
        auto t_ItemWidget = reinterpret_cast<ThumbnailItemWidget *>(m_pThumbnailListWidget->itemWidget(item));
        if (t_ItemWidget) {
            t_ItemWidget->rotateThumbnail(m_nRotate);
            t_ItemWidget->setLabelImage(image);
        }
    }
}

// 初始化缩略图列表list，无缩略图
void ThumbnailWidget::fillContantToList()
{
    for (int idex = 0; idex < m_totalPages; ++idex) {
        addThumbnailItem(idex);
    }

    if (m_totalPages > 0) {
        showItemBookMark();
        auto item = m_pThumbnailListWidget->item(0);
        if (item) {
            m_pThumbnailListWidget->setCurrentItem(item);
            auto pWidget =
                reinterpret_cast<ThumbnailItemWidget *>(m_pThumbnailListWidget->itemWidget(item));
            if (pWidget) {
                pWidget->setBSelect(true);
            }
        }
        /**
         * @brief listCount
         * 计算每个item的大小
         */
        int listCount = 0;
        listCount = m_pThumbnailListWidget->count();
        m_nValuePreIndex = m_pThumbnailListWidget->verticalScrollBar()->maximum() / listCount;
    }

    m_isLoading = false;
}

void ThumbnailWidget::showItemBookMark(int ipage)
{
    if (ipage >= 0) {
        auto pWidget = reinterpret_cast<ThumbnailItemWidget *>(
                           m_pThumbnailListWidget->itemWidget(m_pThumbnailListWidget->item(ipage)));
        if (pWidget) {
            pWidget->slotBookMarkShowStatus(true);
        }
    } else {
        QList<int> pageList = dApp->m_pDBService->getBookMarkList();
        foreach (int index, pageList) {
            auto pWidget = reinterpret_cast<ThumbnailItemWidget *>(
                               m_pThumbnailListWidget->itemWidget(m_pThumbnailListWidget->item(index)));
            if (pWidget) {
                pWidget->slotBookMarkShowStatus(true);
            }
        }
    }
}

/**
 * @brief ThumbnailWidget::prevPage
 * 上一页
 */
void ThumbnailWidget::prevPage()
{
    notifyMsg(MSG_OPERATION_PREV_PAGE);
}

/**
 * @brief ThumbnailWidget::nextPage
 下一页
 */
void ThumbnailWidget::nextPage()
{
    notifyMsg(MSG_OPERATION_NEXT_PAGE);
}

/**
 * @brief ThumbnailWidget::forScreenPageing
 * 全屏和放映时快捷键切换页 true:向下翻页  false:向上翻页
 */
//void ThumbnailWidget::forScreenPageing(bool direction)
//{
//    if (DataManager::instance()->CurShowState() != FILE_NORMAL) {

//        auto helper = DocummentProxy::instance();
//        if (helper) {
//            bool bstart = false;
//            if (helper->getAutoPlaySlideStatu()) {
//                helper->setAutoPlaySlide(false);
//                bstart = true;
//            }
//            int nCurPage = helper->currentPageNo();
//            direction ? nCurPage++ : nCurPage--;
//            notifyMsg(MSG_DOC_JUMP_PAGE, QString::number(nCurPage));
////            jumpToSpecifiedPage(nCurPage);
//            if (bstart) {
//                helper->setAutoPlaySlide(true);
//                bstart = false;
//            }
//        }
//    }
//}

// 关联成功打开文件槽函数
void ThumbnailWidget::slotOpenFileOk()
{
    DocummentProxy *_proxy = DocummentProxy::instance();
    if (_proxy) {
        m_isLoading = true;

        m_ThreadLoadImage.setIsLoaded(false);
        if (m_ThreadLoadImage.isRunning()) {
            m_ThreadLoadImage.stopThreadRun();
        }

        int pages = _proxy->getPageSNum();

        m_totalPages = pages;
//    m_pPageWidget->setTotalPages(m_totalPages);

        m_pThumbnailListWidget->clear();
        m_nValuePreIndex = 0;
        fillContantToList();

        QJsonObject obj = dApp->m_pDBService->getHistroyData();
        m_nRotate = obj["rotate"].toInt();

        if (m_nRotate < 0) {
            m_nRotate = qAbs(m_nRotate);
        }
        m_nRotate %= 360;

        int currentPage = _proxy->currentPageNo();

//        qDebug() << "     currentPage:" << currentPage << "  m_nRotate:" << m_nRotate;
        m_ThreadLoadImage.setPages(m_totalPages);
        if (!m_ThreadLoadImage.isRunning()) {
            m_ThreadLoadImage.clearList();;
            m_ThreadLoadImage.setStartAndEndIndex(currentPage - (FIRST_LOAD_PAGES / 2), currentPage + (FIRST_LOAD_PAGES / 2));
        }
        m_ThreadLoadImage.setIsLoaded(true);
        m_ThreadLoadImage.start();
        slotDocFilePageChanged(QString::number(currentPage));
    }
}

/*******************************ThreadLoadImage*************************************************/

ThreadLoadImage::ThreadLoadImage(QObject *parent)
    : QThread(parent)
{
}

void ThreadLoadImage::stopThreadRun()
{
    m_isLoaded = false;
    quit();
    //    terminate();    //终止线程
    wait();  //阻塞等待
}

bool ThreadLoadImage::inLoading(int index)
{
    if (index >=  m_nStartPage && index < m_nEndPage) {
        return true;
    }

    return false;
}

void ThreadLoadImage::setStartAndEndIndex(int startIndex, int endIndex)
{
    m_nStartPage = startIndex;  // 加载图片起始页码
    m_nEndPage = endIndex;   // 加载图片结束页码

    if (m_nStartPage < 0) {
        m_nStartPage = 0;
    }
    if (m_nEndPage >= m_pages) {
        m_nEndPage = m_pages - 1;
    }
}

// 加载缩略图线程
void ThreadLoadImage::run()
{
    do {
        msleep(50);

        if (!m_pThumbnailWidget) {
            break;
        }

        if (m_pThumbnailWidget->isLoading()) {
            break;
        }

        if (m_nStartPage < 0) {
            m_nStartPage = 0;
        }
        if (m_nEndPage >= m_pages) {
            m_nEndPage = m_pages - 1;
        }

        auto dproxy = DocummentProxy::instance();
        if (nullptr == dproxy) {
            break;
        }

        for (int page = m_nStartPage; page <= m_nEndPage; page++) {
            if (!m_isLoaded)
                break;
            if (m_listLoad.contains(page)) {
                emit sigRotateImage(page);
                msleep(50);
                continue;
            }
            QImage image;
            bool bl = dproxy->getImage(page, image, 146, 174);
            if (bl) {
                m_listLoad.append(page);
                emit sigLoadImage(page, image);
                msleep(50);
            }
        }

    } while (0);
}