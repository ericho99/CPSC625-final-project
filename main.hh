#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

// inspired by Bryan Ford's CPSC 426 Assignment

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QUdpSocket>
#include <QVariantMap>
#include <QPushButton>

class NetSocket : public QUdpSocket
  {
    Q_OBJECT

  public:
    NetSocket();
    int boundPort;
    QString dir_name;
    void sendDatagrams(QVariantMap msg);

    // Bind this socket to a Peerster-specific default port.
    bool bind();

  private:
    int myPortMin, myPortMax;
};

class FrontDialog : public QDialog
{
	Q_OBJECT

  public:
    FrontDialog();
    NetSocket *sock;

  public slots:
    void gotReturnPressed();
    void readPendingDatagrams();

  signals:
    void keyPressEvent(QKeyEvent *e);

  private:
    QTextEdit *textview;
    QTextEdit *textline;
    QPushButton *putbutton;
};

#endif // PEERSTER_MAIN_HH
