#include <unistd.h>

#include "hotrumor.hh"

void HotRumor::checkAcks()
{
  // if we don't receive an ack at all, or if the node responded positively, we keep sending out messages
  if ((ackmsg.contains(QString("Ack")) and ackmsg.contains(QString("Key")) and
      ackmsg.contains(QString("Version")) and ackmsg[QString("Ack")] == 1 and
      ackmsg[QString("Key")] == key and ackmsg[QString("Version")] == version) or
      not ackmsg.contains(QString("Ack"))) {
    emit sendRandomMessage(msg);
  } else {
    if (rand() % kRumorProb == 0) {
      emit eliminateRumor(key);
    } else {
      emit sendRandomMessage(msg);
    }
  }

  // wipe ackmsg
  QVariantMap t;
  ackmsg = t;
}

HotRumor::HotRumor(QVariantMap inmsg)
{
  kTimeout = 2000;
  kRumorProb = 2;
  key = inmsg[QString("Key")].toString();
  version = inmsg[QString("Version")].toInt();
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(checkAcks()));
  timer->start(kTimeout);
  
  QVariantMap t;
  ackmsg = t;
  msg = inmsg;
}

HotRumor::~HotRumor()
{
  if (timer) {
    delete(timer);
  }
}
