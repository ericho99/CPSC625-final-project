#ifndef EPIDEMIC_MAIN_HH
#define EPIDEMIC_MAIN_HH

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

#include "quorum.hh"

class NetSocket : public QUdpSocket
{
  Q_OBJECT

  public:
    NetSocket();
    bool bind(); // Bind this socket to a Peerster-specific default port.
    void findNeighbors();
    QByteArray* serialize(QVariantMap);
    QVariantMap deserialize();
    void sendAck(int ack, QVariantMap msg);
    void sendResponseMessage(QVariantMap, QHostAddress, int);

    int boundPort, kRumorProb; // const kRumorProb?
    QHostAddress address;
    QString dir_name;

    QVector<QPair<QHostAddress, int> > *neighbors; // vector of <address, port> pairs

  public slots:
    void sendRandomMessage(QVariantMap msg);

  private:
    int myPortMin, myPortMax;
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
    void sendRandomMessage(QVariantMap);

  private:
    int kTimeout, kRumorProb;
    QVariantMap msg;
};

class VersionTracker
{
  public:
    VersionTracker();
    int findVersion(QString key); 

    QVariantMap *versions; // map of key to version
};

class FrontDialog : public QDialog
{
	Q_OBJECT

  public:
    FrontDialog();
    void put(QString key, QString value);
    QString get(QString key);
    int processRumor(QVariantMap);
    void attachAckMessage(QVariantMap);
    void processEntropy(QVariantMap);
    void placeUpdates(QVariantMap);
    void gatherQuorum(QString);
    void sendQuorumResponse(QVariantMap);
    QVariantMap createBaseMap();
    QVariantMap attachValuesToUpdates(QVariantMap);
    QVariantMap findRequiredUpdates(QVariantMap, QVariantMap);

    NetSocket *sock;
    VersionTracker *vt;
    QVector<HotRumor *> *hotRumors;
    QTimer *antiTimer;
    Quorum *quorum;

  public slots:
    void putRequest();
    void getRequest();
    void deleteRequest();
    void readPendingMessages();
    void eliminateRumorByKey(QString key);
    void sendAntiEntropy();
    void quorumDecision(QVariantMap);

  signals:
    void antiEntropy();
    void startRumor(QVariantMap msg);

  private:
    void clearAllInputs();

    QLineEdit *putKeyField;
    QLineEdit *putValueField;
    QLineEdit *getKeyField;
    QLineEdit *getValueField;
    QLineEdit *deleteKeyField;

    QPushButton *putButton;
    QPushButton *getButton;
    QPushButton *deleteButton;
    int kAntiEntropyTimeout;
};

#endif
