#include "filefinderdialog.h"
#include <QDirIterator>
#include <QApplication>
#include <QDebug>

FileFinderDialog::FileFinderDialog(QWidget *parent)
    : QProgressDialog(parent)
{
    setModal(true);
    setRange(0, 0);
    setWindowTitle(tr("Find Files"));
    setCancelButtonText(tr("&Cancel"));
    setLabelText(tr("Looking for files, this may take awhile..."));
    connect(this, SIGNAL(canceled()), this, SLOT(handleCancel()));
}

int FileFinderDialog::showFileSearchDialog(const QString &path, QStringList filters)
{
    canceled = false;
    show();
    fileFinderThread = new FileFinderThread(path, filters, this);
    connect(fileFinderThread, &FileFinderThread::resultsReady, this, &FileFinderDialog::resultsReady);
    connect(fileFinderThread, &FileFinderThread::finished, fileFinderThread, &QObject::deleteLater);
    fileFinderThread->start();
    while (fileFinderThread->isRunning()) {
        QApplication::processEvents();
    }
    return canceled ? QDialog::Rejected : QDialog::Accepted;
}

void FileFinderDialog::resultsReady(QStringList results)
{
    m_files = results;
    emit searchCompleted(results);
}

void FileFinderDialog::handleCancel()
{
    if (fileFinderThread == nullptr) {
        return;
    }
    fileFinderThread->requestInterruption();
    canceled = true;
}

void FileFinderThread::run()
{
    QDirIterator it(m_path, m_filters, QDir::AllEntries | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }
    QStringList files;
    while (it.hasNext()) {
        it.next();
        if (QThread::currentThread()->isInterruptionRequested()) {
            return;
        }
        files.append(it.filePath());
    }
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }
    files.sort();
    if (QThread::currentThread()->isInterruptionRequested()) {
        return;
    }
    emit resultsReady(files);
}
