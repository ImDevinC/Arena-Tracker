#include "linuxlogloader.h"
#include "ui_linuxlogloader.h"
#include <QProcess>
#include <QDir>

LinuxLogLoader::LinuxLogLoader(QString pattern, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LinuxLogLoader)
{
    ui->setupUi(this);
    setFixedSize(size());
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    pid = -1;
    startLogFileSearch(pattern);
}

LinuxLogLoader::~LinuxLogLoader()
{
    delete ui;
}

void LinuxLogLoader::searchCompleted(QString pattern)
{
//    emit logLocationFound(QString(p.readAll()).trimmed());
}

void LinuxLogLoader::startLogFileSearch(QString pattern)
{
    QString program = "find";
    QStringList arguments;
    arguments.append(QDir::homePath());
    arguments.append("-wholename");
    arguments.append(pattern);
    QProcess::startDetached(program, arguments, "", &pid);
}
