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
#include <QTimer>

class NetSocket : public QUdpSocket
{
  Q_OBJECT

  public:
    NetSocket();
    bool bind(); // Bind this socket to a Peerster-specific default port.
    void findNeighbors();
    QVariantMap deserialize();
    void sendAck(int ack, QVariantMap msg);

    int boundPort, kRumorProb; // const kRumorProb?
    QHostAddress address;
    QString dir_name;

  public slots:
    void sendRumor(QVariantMap msg);

  private:
    int myPortMin, myPortMax;
    QVector<QPair<QHostAddress, int> > *neighbors; // vector of <address, port> pairs
};

class HotRumor : public QObject
{
  Q_OBJECT

  public:
    HotRumor(QVariantMap);
    ~HotRumor();
    QTimer *timer;

    QString key;
    int version;
    QVariantMap ackmsg;

  public slots:
    void checkAcks();

  signals:
    void eliminateRumor(QString key);
    void sendRumor(QVariantMap);

  private:
    int kTimeout, kRumorProb;
    QVariantMap msg;
};

class VersionTracker
{
  public:
    VersionTracker();
    int findVersion(QString key); 
    void updateVersion(QString key, int version);

    QMap<QString, QPair<QString, int> > *versions; // map of key to <value, version>
};

class FrontDialog : public QDialog
{
	Q_OBJECT

  public:
    FrontDialog();
    void put(QString dir_name, QString key, QString value);
    bool shouldUpdate(int, int);
    int processRumor(QVariantMap);
    void resendRumor(QVariantMap);
    void attachAckMessage(QVariantMap);

    NetSocket *sock;
    VersionTracker *vt;
    QVector<HotRumor *> *hotRumors;

  public slots:
    void putRequest();
    void readPendingMessages();
    void eliminateRumorByKey(QString key);

  signals:
    void startRumor(QVariantMap msg);

  private:
    QLineEdit *keyfield;
    QTextEdit *valuefield;
    QPushButton *putbutton;
};

#endif // PEERSTER_MAIN_HH
