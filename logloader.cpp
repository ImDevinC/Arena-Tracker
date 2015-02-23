#include "logloader.h"
#include <QtWidgets>

LogLoader::LogLoader(QObject *parent) : QObject(parent)
{
    logWorker = NULL;
}


void LogLoader::init(qint64 &logSize)
{
    readSettings();
    logSize = getLogFileSize();

    if(logSize >= 0)
    {
        qDebug() << "LogLoader: Log encontrado.";
        emit sendLog(tr("Log: Log found."));

        if(logSize == 0)
        {
            qDebug() << "LogLoader: Log vacio.";
            emit sendLog(tr("Log: Log is empty."));
        }

        this->logSize = logSize;
        firstRun = true;
        updateTime = 1000;

        logWorker = new LogWorker(this, logPath);
        connect(logWorker, SIGNAL(newLogLineRead(QString)),
                this, SLOT(emitNewLogLineRead(QString)));
        connect(logWorker, SIGNAL(seekChanged(qint64)),
                this, SLOT(updateSeek(qint64)));
        connect(logWorker, SIGNAL(sendLog(QString)),
                this, SLOT(emitSendLog(QString)));

        QTimer::singleShot(updateTime, this, SLOT(sendLogWorker()));
    }
    else
    {
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("logPath", "");
        qDebug() << "LogLoader: Log no encontrado.";
        emit sendLog(tr("Log: Log not found. Restart Arena Tracker and set the path again."));
    }
}


void LogLoader::readSettings()
{
    readLogPath();
    readLogConfigPath();
}


void LogLoader::readLogPath()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    logPath = settings.value("logPath", "").toString();

    if(logPath.isEmpty() || getLogFileSize()==-1)
    {
        QMessageBox::information(0, tr("Arena Tracker"), tr("The first time you run Arena Tracker you will be asked for:\n"
                                    "1) output_log.txt location (If not default).\n"
                                    "2) log.config location (If not default).\n"
                                    "3) Your Arena Mastery user/password.\n"
                                    "4) Restart Hearthstone (If running).\n\n"
                                    "After your first game:\n"
                                    "5) Your Hearthstone name."));

        QString initDir;
#ifdef Q_OS_WIN32
        initDir = "C:/Program Files (x86)/Hearthstone/Hearthstone_Data/output_log.txt";
        QFileInfo logFI(initDir);
        if(logFI.exists())
        {
            logPath = initDir;
        }
#endif
#ifndef Q_OS_WIN32
        initDir = QDir::homePath();
#endif
        if(logPath.isEmpty())
        {
            logPath = QFileDialog::getOpenFileName(0,
                tr("Find Hearthstone log (output_log.txt)"), initDir,
                tr("Hearthstone log (output_log.txt)"));
        }

        settings.setValue("logPath", logPath);
    }

#ifdef QT_DEBUG
//    logPath = QString("/home/triodo/Documentos/arenaMagoFull.txt");
#endif

    qDebug() << "LogLoader: " << "Path output_log.txt " << logPath;
    emit sendLog(tr("Settings: Path output_log.txt: ") + logPath);
}


void LogLoader::readLogConfigPath()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    QString logConfig = settings.value("logConfig", "").toString();

    if(logConfig.isEmpty())
    {
        QString initDir;
#ifdef Q_OS_WIN32
        //initDir = QDir::homePath() + "/Local Settings/Application Data/Blizzard/Hearthstone/log.config";
        initDir = QDir::homePath() + "/AppData/Local/Blizzard/Hearthstone/log.config";
        QFileInfo logConfigFI(initDir);
        if(logConfigFI.exists())
        {
            logConfig = initDir;
        }
        else
        {
            QString hsDir = QDir::homePath() + "/AppData/Local/Blizzard/Hearthstone";
            logConfigFI = QFileInfo(hsDir);
            if(logConfigFI.exists() && logConfigFI.isDir())
            {
                //Creamos log.config
                QFile logConfigFile(initDir);
                if(!logConfigFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    qDebug() << "LogLoader: "<< "ERROR: No se puede crear default log.config.";
                    emit sendLog(tr("Log: ERROR: Cannot create default log.config"));
                    settings.setValue("logConfig", "");
                    return;
                }
                logConfigFile.close();
                logConfig = initDir;
            }
        }
#endif
#ifndef Q_OS_WIN32
        initDir = QDir::homePath();
#endif
        if(logConfig.isEmpty())
        {
            logConfig = QFileDialog::getOpenFileName(0,
                tr("Find Hearthstone config log (log.config)"), initDir,
                tr("log.config (log.config)"));
        }
        settings.setValue("logConfig", logConfig);
    }

    if(!logConfig.isEmpty()) checkLogConfig(logConfig);

    qDebug() << "LogLoader: " << "Path log.config " << logConfig;
    emit sendLog(tr("Settings: Path log.config: ") + logConfig);
}


void LogLoader::checkLogConfig(QString logConfig)
{
    qDebug() << "LogLoader: " << "Verificando log.config";

    QFile file(logConfig);
    if(!file.open(QIODevice::ReadWrite | QIODevice::Text))
    {
        qDebug() << "LogLoader: "<< "ERROR: No se puede acceder a log.config.";
        emit sendLog(tr("Log: ERROR: Cannot access log.config"));
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("logConfig", "");
        return;
    }

    QString data = QString(file.readAll());
    QTextStream stream(&file);

    checkLogConfigOption("[Bob]", data, stream);
    checkLogConfigOption("[Power]", data, stream);
    checkLogConfigOption("[Rachelle]", data, stream);
    checkLogConfigOption("[Zone]", data, stream);
    checkLogConfigOption("[Ben]", data, stream);
    checkLogConfigOption("[Asset]", data, stream);

    file.close();
}


void LogLoader::checkLogConfigOption(QString option, QString &data, QTextStream &stream)
{
    if(!data.contains(option))
    {
        qDebug() << "LogLoader: " << "Configurando log.config";
        emit sendLog(tr("Log: Setting log.config"));
        stream << endl << option << endl;
        stream << "LogLevel=1" << endl;
        stream << "ConsolePrinting=true" << endl;
    }
}


/*
 * Return
 * -1 no existe
 * 0 inaccesible
 * n size
 */
qint64 LogLoader::getLogFileSize()
{
    QFileInfo log(logPath);
    if(log.exists())
    {
        return log.size();
    }
    return -1;
}


LogLoader::~LogLoader()
{
    if(logWorker != NULL)   delete logWorker;
}


void LogLoader::sendLogWorker()
{
    if(!isLogReset())
    {
        logWorker->readLog();
    }

    workerFinished();
}


void LogLoader::workerFinished()
{
    checkFirstRun();
    QTimer::singleShot(updateTime, this, SLOT(sendLogWorker()));

    if(updateTime < 4000) updateTime += 500;
}


void LogLoader::checkFirstRun()
{
    if(firstRun)
    {
        firstRun = false;
        emit synchronized();
    }
}


bool LogLoader::isLogReset()
{
    qint64 newSize = getLogFileSize();

    if(newSize == 0)    return false;

    if((newSize == -1) || (newSize < logSize))
    {
        //Log se ha reiniciado
        qDebug() << "LogLoader: "<< "Log reiniciado. FileSize: " << newSize << " < " << logSize;
        emit sendLog(tr("Log: Hearthstone started. Log reset."));
        emit seekChanged(0);
        logWorker->resetSeek();
        logSize = 0;
        return true;
    }
    else
    {
        logSize = newSize;
        return false;
    }
}


void LogLoader::updateSeek(qint64 logSeek)
{
    if(firstRun)    emit seekChanged(logSeek);
}


//LogWorker signal reemit
void LogLoader::emitNewLogLineRead(QString line)
{
    updateTime = 500;
    emit newLogLineRead(line);
}
void LogLoader::emitSendLog(QString line){emit sendLog(line);}
