#ifndef QUORUM_CLASS_HH
#define QUORUM_CLASS_HH

#include <QVariantMap>
#include <QTimer>

class Quorum : public QObject
{
  Q_OBJECT

  public:
    Quorum();
    ~Quorum();
    QTimer *timer;

  public slots:
    void decideQuorum();

  signals:
    void quorumDecision(QVariantMap);

  private:
    int kTimeout;
};

#endif
