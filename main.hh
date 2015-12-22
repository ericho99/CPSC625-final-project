#ifndef EPIDEMIC_MAIN_HH
#define EPIDEMIC_MAIN_HH

// inspired by Bryan Ford's CPSC 426 Assignment

#include <QDialog>
#include <QTextEdit>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMap>
#include <QVariantMap>
#include <QPushButton>
#include <QTimer>

#include "netsocket.hh"
#include "hotrumor.hh"
#include "quorum.hh"

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
