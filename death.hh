#ifndef DEATH_CLASS_HH
#define DEATH_CLASS_HH

#include <QVariantMap>
#include <QTimer>

class Death : public QObject
{
  Q_OBJECT
  public:
    Death();
    ~Death();

    QTimer *timer;
    QString key;
    QVariantMap ackmsg;

  public slots:
    void checkAcks();

  private:
    int kTimeout, kRumorProb, kExpiration, kDeletion;
}

#endif
