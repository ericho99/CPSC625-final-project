#ifndef PEERSTER_MAIN_HH
#define PEERSTER_MAIN_HH

// inspired by Bryan Ford's CPSC 426 Assignment

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QUdpSocket>
#include <QMap>
#include <QVariantMap>
#include <QPushButton>

class NetSocket : public QUdpSocket
{
  Q_OBJECT

  public:
    NetSocket();
    void sendRumors(QVariantMap msg);
    bool bind(); // Bind this socket to a Peerster-specific default port.
    void findNeighbors();

    int boundPort;
    QHostAddress address;
    QString dir_name;

  private:
    int myPortMin, myPortMax;
    QVector<QPair<QHostAddress, int> > *neighbors; // vector of <address, port> pairs
};

class VersionTracker
{
  public:
    VersionTracker();
    int findMostRecentVersion(QString key); 
    void updateVersion(QString key, int version);

    QMap<QString, QPair<QString, int> > *versions; // map of key to <value, version>
};

class FrontDialog : public QDialog
{
	Q_OBJECT

  public:
    FrontDialog();
    void put(QString dir_name, QString key, QString value);
    void writeKey(QString, QString, int);

    NetSocket *sock;
    VersionTracker *vt;

  public slots:
    void putRequest();
    void readPendingMessages();

  signals:
    void keyPressEvent(QKeyEvent *e);

  private:
    QLineEdit *keyfield;
    QTextEdit *valuefield;
    QPushButton *putbutton;
};

#endif // PEERSTER_MAIN_HH
