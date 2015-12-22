#ifndef HOTRUMOR_CLASS_HH
#define HOTRUMOR_CLASS_HH

#include <QVariantMap>
#include <QTimer>

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
    void eliminateRumor(QString);
    void sendRandomMessage(QVariantMap);

  private:
    int kTimeout, kRumorProb;
    QVariantMap msg;
};

#endif
