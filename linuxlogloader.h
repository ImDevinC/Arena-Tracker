#ifndef LINUXLOGLOADER_H
#define LINUXLOGLOADER_H

#include <QDialog>

namespace Ui {
class LinuxLogLoader;
}

class LinuxLogLoader : public QDialog
{
    Q_OBJECT

public:
    explicit LinuxLogLoader(QString pattern = "", QWidget *parent = nullptr);
    ~LinuxLogLoader();

private:
    void startLogFileSearch(QString pattern);
    void searchCompleted(QString pattern);

private:
    Ui::LinuxLogLoader *ui;
    qint64 pid;

signals:
    void logLocationFound(QString location);

private slots:
    void onCancelButtonClicked() { QDialog::reject(); };


};

#endif // LINUXLOGLOADER_H
