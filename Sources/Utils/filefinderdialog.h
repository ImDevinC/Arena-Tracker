#ifndef FILEFINDERDIALOG_H
#define FILEFINDERDIALOG_H

#include <QProgressDialog>
#include <QDir>
#include <QObject>
#include <QThread>

class FileFinderThread : public QThread
{
    Q_OBJECT

public:
    FileFinderThread(const QString &path, const QStringList &filters, QObject *parent = nullptr)
        : QThread(parent), m_path(path), m_filters(filters) {};
    void run() override;

signals:
    void resultsReady(const QStringList &files);

private:
    QString m_path;
    QStringList m_filters;
};

class FileFinderDialog : public QProgressDialog
{
    Q_OBJECT

public:
    FileFinderDialog(QWidget *parent = nullptr);
    ~FileFinderDialog() {
        if (fileFinderThread != nullptr) {
            fileFinderThread->requestInterruption();
            fileFinderThread->disconnect();
            fileFinderThread->quit();
            fileFinderThread->wait();
            fileFinderThread = nullptr;
        }
        disconnect();
    }

    int showFileSearchDialog(const QString &paths, QStringList filters);
    Q_SLOT void resultsReady(QStringList paths);
    Q_SLOT void handleCancel();
    QStringList getFiles() { return m_files; };

signals:
    void searchCompleted(QStringList files);

private:
    FileFinderThread *fileFinderThread;
    QStringList m_files;
    bool canceled;
};

#endif // FILEFINDERDIALOG_H
