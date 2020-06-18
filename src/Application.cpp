#include "Application.h"

#include <QIcon>
#include <QTranslator>
#include <QDebug>
#include <QDir>

//#include "pdfControl/AppConfig.h"
#include "app/AppInfo.h"
#include "utils/Utils.h"
#include "ModuleHeader.h"
#include "MsgHeader.h"
#include "MainWindow.h"
#include "djvuControl/RenderThreadDJVU.h"

Application::Application(int &argc, char **argv)
    : DApplication(argc, argv)
{
    initI18n();
    initCfgPath();
    initChildren();

    setAttribute(Qt::AA_UseHighDpiPixmaps);
    setApplicationName(ConstantMsg::g_app_name);
    setOrganizationName("deepin");
    setWindowIcon(QIcon::fromTheme(ConstantMsg::g_app_name));
    setApplicationVersion(DApplication::buildVersion("1.0"));
    setApplicationAcknowledgementPage(Constant::sAcknowledgementLink);
    setApplicationDisplayName(tr("Document Viewer"));
    setApplicationDescription(tr("Document Viewer is a tool for reading document files, supporting PDF, DJVU, etc."));

    QPixmap px(QIcon::fromTheme(ConstantMsg::g_app_name).pixmap(static_cast<int>(256 * qApp->devicePixelRatio()), static_cast<int>(256 * qApp->devicePixelRatio())));
    px.setDevicePixelRatio(qApp->devicePixelRatio());
    setProductIcon(QIcon(px));
}

void Application::setSreenRect(const QRect &rect)
{
    if (m_pAppInfo) {
        m_pAppInfo->setScreenRect(rect);
    }
}

void Application::handleQuitAction()
{
    //线程退出
    RenderThreadDJVU::destroy();
    foreach (MainWindow *window, MainWindow::m_list)
        window->close();
}

//  初始化 deepin-reader 的配置文件目录, 包含 数据库, conf.cfg
void Application::initCfgPath()
{
    QString sDbPath = Utils::getConfigPath();
    QDir dd(sDbPath);
    if (! dd.exists()) {
        dd.mkpath(sDbPath);
        if (dd.exists())
            qDebug() << __FUNCTION__ << "  create app dir succeed!";
    }
}

void Application::initChildren()
{
    m_pDBService = new DBService(this);
    m_pAppInfo = new AppInfo(this);
}

void Application::initI18n()
{
    // install translators
    loadTranslator();
}