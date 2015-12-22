#ifndef QUORUM_CLASS_HH
#define QUORUM_CLASS_HH

#include <QVariantMap>
#include <QTimer>
#include <QVector>

class Quorum : public QObject
{
  Q_OBJECT

  public:
    Quorum(QString, QString, int);
    ~Quorum();
    void processQuorumResponse(QVariantMap);

    QTimer *timer;
    QString key;
    QVector<QPair<QString, int> > *responses;

  public slots:
    void decideQuorum();

  signals:
    void quorumDecision(QString);

  private:
    int kTimeout;
};

#endif
