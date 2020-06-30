#include "logloader.h"
#include <QtWidgets>
#include "Utils/filefinderdialog.h"

LogLoader::LogLoader(QObject *parent) : QObject(parent)
{
    updateTime = MIN_UPDATE_TIME;
    setUpdateTimeMax();

    logComponentList.append("LoadingScreen");
    logComponentList.append("Power");
    logComponentList.append("Zone");
    logComponentList.append("Arena");
    logComponentList.append("Asset");

    match = new QRegularExpressionMatch();
}


bool LogLoader::init()
{
    if(!readSettings()) return false;

    emit pDebug("Log found.");
    emit pLog(tr("Log: Log found."));

    updateTime = 1000;

    createLogWorkers();

    QTimer::singleShot(1, this, SLOT(sendLogWorkerFirstRun())); //Retraso para dejar que la aplicacion se pinte.
    return true;
}


void LogLoader::createLogWorkers()
{
    foreach(QString component, logComponentList)
    {
        createLogWorker(component);
    }
}


void LogLoader::createLogWorker(QString logComponent)
{
    LogWorker *logWorker;
    logWorker = new LogWorker(this, logsDirPath, logComponent);
    connect(logWorker, SIGNAL(pLog(QString)),
            this, SIGNAL(pLog(QString)));
    connect(logWorker, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SIGNAL(pDebug(QString,DebugLevel,QString)));

    if(logComponent == "LoadingScreen")
    {
        connect(logWorker, SIGNAL(logReset()),
                this, SIGNAL(logReset()));
        connect(logWorker, SIGNAL(newLogLineRead(LogComponent, QString, qint64, qint64)),
                this, SLOT(emitNewLogLineRead(LogComponent, QString, qint64, qint64)));
    }

    logWorkerMap[logComponent] = logWorker;
}


bool LogLoader::readSettings()
{
    sortLogs = true;
    return readLogsDirPath() && readLogConfigPath();
}

QString LogLoader::findLinuxLogs(QString pattern)
{
    QProcess p;
    p.start("find \"" + QDir::homePath() + "\" -wholename \"" + pattern + "\"");
    p.waitForFinished(-1);
    return QString(p.readAll()).trimmed();
}

QString LogLoader::findLogs(const QString rootDir, const QString namePattern, const QString dirPattern = "")
{
    FileFinderDialog dialog;
    int result = dialog.showFileSearchDialog(rootDir, QStringList() << namePattern);
    if (QDialog::Rejected == result || dialog.getFiles().size() == 0) {
        return QString();
    }
    QString file = dialog.getFiles()[0];
    if (!dirPattern.isEmpty() && !file.contains(dirPattern)) {
        file = "";
    }
    return file;
}


bool LogLoader::readLogsDirPath()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    logsDirPath = settings.value("logsDirPath", "").toString();

    if(logsDirPath.isEmpty())
    {
        QString dirPattern;
#ifdef Q_OS_WIN
//        initPath = "C:/Program Files (x86)/Hearthstone";
        QString srcPath = "C:/Program Files (x86)/Hearthstone";
        QString nameFilter = "Hearthstone.exe";
#endif
#ifdef Q_OS_MAC
//        initPath = "/Applications/Hearthstone";
        QString srcPath = "/Applications/Hearthstone";
        QString nameFilter = "Hearthstone.app";
#endif
#ifdef Q_OS_LINUX
//        initPath = findLinuxLogs("*/Program Files/Hearthstone");
        QString srcPath = QDir::homePath();
        QString nameFilter = "Hearthstone.exe";
        dirPattern = "/Program Files/Hearthstone/";
#endif
        QString initPath = findLogs(srcPath, nameFilter, dirPattern);
        logsDirPath = "";
        if(!initPath.isEmpty())
        {
            initPath = initPath.left(initPath.lastIndexOf("/"));
            if(QFileInfo (initPath).exists())
            {
                logsDirPath = initPath + "/Logs";
                if(!QFileInfo (logsDirPath).exists())
                {
                    QDir().mkdir(logsDirPath);
                    emit pDebug(logsDirPath + " created.");
                }
            }
        }

        if(logsDirPath.isEmpty())
        {
            emit pDebug("Show Find Logs dir dialog.");
            logsDirPath = QFileDialog::getExistingDirectory(nullptr,
                "Find Hearthstone Logs dir",
                QDir::homePath());
        }

        settings.setValue("logsDirPath", logsDirPath);
    }

    emit pDebug("Path Logs Dir: " + logsDirPath + " - " + QString::number(logsDirPath.length()));
    emit pLog("Settings: Path Logs Dir: " + logsDirPath);

    if(!QFileInfo(logsDirPath).exists())
    {
        settings.setValue("logsDirPath", "");
        emit pDebug("Logs dir not found.");
        emit pLog(tr("Log: Logs dir not found. Restart Arena Tracker and set the path again."));
        QMessageBox::information(static_cast<QWidget*>(this->parent()), tr("Logs dir not found"), tr("Logs dir not found. Restart Arena Tracker and set the path again."));
        return false;
    }
    return true;
}


bool LogLoader::readLogConfigPath()
{
    bool isOk = true;
    QSettings settings("Arena Tracker", "Arena Tracker");
    logConfig = settings.value("logConfig", "").toString();

    if(logConfig.isEmpty())
    {
        logConfig = createDefaultLogConfig();
        if(logConfig.isEmpty())
        {
            emit pDebug("Show Find log.config dialog.");
            logConfig = QFileDialog::getOpenFileName(nullptr,
                tr("Find Hearthstone config log (log.config)"), QDir::homePath(),
                tr("log.config (log.config)"));
        }
        settings.setValue("logConfig", logConfig);
        if(!logConfig.isEmpty())
        {
            //Remove old log.config
            QFile file(logConfig);
            if(file.exists())   file.remove();

            isOk = checkLogConfig();
        }
    }
    else
    {
        isOk = checkLogConfig();
    }

    emit pDebug("Path log.config: " + logConfig + " - " + QString::number(logConfig.length()));
    emit pLog(tr("Settings: Path log.config: ") + logConfig);

    if(!QFileInfo(logConfig).exists())
    {
        settings.setValue("logConfig", "");
        emit pDebug("log.config not found.");
        emit pLog(tr("Log: log.config not found. Restart Arena Tracker and set the path again."));
        QMessageBox::information(static_cast<QWidget*>(this->parent()), tr("log.config not found"), tr("log.config not found. Restart Arena Tracker and set the path again."));
        return false;
    }

    if(isOk)    emit logConfigSet();
    return isOk;
}


QString LogLoader::createDefaultLogConfig()
{
    QString dirPattern = "";
#ifdef Q_OS_WIN
    QString rootDir = QDir::homePath() + "/AppData/Local/Blizzard/Hearthstone";
//    initPath = QDir::homePath() + "/AppData/Local/Blizzard/Hearthstone/log.config";
#endif
#ifdef Q_OS_MAC
    QString rootDir = QDir::homePath() + "/Library/Preferences/Blizzard/Hearthstone";
//    initPath = QDir::homePath() + "/Library/Preferences/Blizzard/Hearthstone/log.config";
#endif
#ifdef Q_OS_LINUX
    QString rootDir = QDir::homePath();
    dirPattern = "/Blizzard/Hearthstone/";
//    initPath = findLinuxLogs("*/Local Settings/Application Data/Blizzard/Hearthstone");
//    if(!initPath.isEmpty())     initPath += "/log.config";
#endif
    QString filePattern = "log.config";
    QString initPath = findLogs(rootDir, filePattern, dirPattern);
    if(initPath.isEmpty()) return "";

    QFileInfo logConfigFI(initPath);
    if(logConfigFI.exists())
    {
        return initPath;
    }
    else
    {
        QString hsDir = logConfigFI.absolutePath();
        logConfigFI = QFileInfo(hsDir);
        if(logConfigFI.exists() && logConfigFI.isDir())
        {
            return initPath;
        }
    }

    return "";
}


bool LogLoader::checkLogConfig()
{
    emit pDebug("Checking log.config");

    QFile file(logConfig);
    if(!file.open(QIODevice::ReadWrite | QIODevice::Text))
    {
        emit pDebug("Cannot access log.config", DebugLevel::Error);
        emit pLog(tr("Log: ERROR: Cannot access log.config"));
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("logConfig", "");
        QMessageBox::information(static_cast<QWidget*>(this->parent()), tr("log.config not found"), tr("log.config not found. Restart Arena Tracker and set the path again."));
        return false;
    }

    QString data = QString(file.readAll());
    QTextStream stream(&file);

    bool logConfigChanged = false;
    foreach(QString component, logComponentList)
    {
        logConfigChanged = checkLogConfigOption("["+component+"]", data, stream) || logConfigChanged;
    }

    file.close();

    if(logConfigChanged)
    {
        emit showMessageProgressBar("Restart Hearthstone");
    }

    return true;
}


bool LogLoader::checkLogConfigOption(QString option, QString &data, QTextStream &stream)
{
    if(!data.contains(option))
    {
        emit pDebug("Setting log.config");
        emit pLog(tr("Log: Setting log.config"));
        stream << endl << option << endl;
        stream << "LogLevel=1" << endl;
        stream << "FilePrinting=true" << endl;
        if(option == "[Power]") stream << "Verbose=1" << endl;

        return true;
    }
    else    return false;
}


LogLoader::~LogLoader()
{
    foreach(LogWorker *worker, logWorkerMap.values())   delete worker;
    logWorkerMap.clear();
    delete match;
}


void LogLoader::sendLogWorkerFirstRun()
{
    foreach(QString logComponent, logComponentList)
    {
        LogWorker *logWorker = logWorkerMap[logComponent];
        logWorker->readLog();

        if(logComponent != "LoadingScreen")
        {
            connect(logWorker, SIGNAL(newLogLineRead(LogComponent, QString, qint64, qint64)),
                    this, SLOT(emitNewLogLineRead(LogComponent, QString, qint64, qint64)));
        }
    }

    QTimer::singleShot(updateTime, this, SLOT(sendLogWorker()));
    emit synchronized();
}


void LogLoader::sendLogWorker()
{
    foreach(QString logComponent, logComponentList)     logWorkerMap[logComponent]->readLog();
    processDataLogs();

    QTimer::singleShot(updateTime, this, SLOT(sendLogWorker()));
    if(updateTime < maxUpdateTime)  updateTime += UPDATE_TIME_STEP;
}


void LogLoader::processDataLogs()
{
    if(dataLogs.isEmpty())  return;

    QList<qint64> timeStamps = dataLogs.keys();
    qSort(timeStamps);

    foreach(qint64 timeStamp, timeStamps)
    {
        DataLog dataLog = dataLogs[timeStamp];
        emit newLogLineRead(dataLog.logComponent, dataLog.line, dataLog.numLine, dataLog.logSeek);
    }
    dataLogs.clear();
}


void LogLoader::setUpdateTimeMin()
{
    setMaxUpdateTime(MIN_UPDATE_TIME);
}


void LogLoader::setUpdateTimeMax()
{
    setMaxUpdateTime(MAX_UPDATE_TIME);
}


void LogLoader::setMaxUpdateTime(int value)
{
    maxUpdateTime = value;
    updateTime = std::min(updateTime,maxUpdateTime);
}


void LogLoader::copyGameLog(qint64 logSeekCreate, qint64 logSeekWon, QString fileName)
{
    logWorkerMap["Power"]->copyGameLog(logSeekCreate, logSeekWon, fileName);
}


QString LogLoader::getLogConfigPath()
{
    return this->logConfig;
}


QString LogLoader::getLogsDirPath()
{
    return this->logsDirPath;
}


void LogLoader::addToDataLogs(LogComponent logComponent, QString line, qint64 numLine, qint64 logSeek)
{
    if(line.contains(QRegularExpression("(\\d+):(\\d+):(\\d+).(\\d+) (.*)"), match))
    {
        DataLog dataLog;
        dataLog.logComponent = logComponent;
        dataLog.line = match->captured(5);
        dataLog.numLine = numLine;
        dataLog.logSeek = logSeek;

        qint64 timeStamp = QString(match->captured(1) + match->captured(2) + match->captured(3) + match->captured(4)).toLongLong();
        while(dataLogs.contains(timeStamp))     timeStamp++;
        dataLogs[timeStamp] = dataLog;
    }
    else
    {
        emit pDebug("Log timestamp invalid: " + line, DebugLevel::Error);
        emit newLogLineRead(logComponent, line, numLine, logSeek);
    }
}


//LogWorker signal reemit
void LogLoader::emitNewLogLineRead(LogComponent logComponent, QString line, qint64 numLine, qint64 logSeek)
{
    updateTime = MIN_UPDATE_TIME;
    if(sortLogs)    addToDataLogs(logComponent, line, numLine, logSeek);
    else            emit newLogLineRead(logComponent, line, numLine, logSeek);
}


