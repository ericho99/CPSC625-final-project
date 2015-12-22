#include <unistd.h>

#include "death.hh"

void HotRumor::checkAcks()
{
  QVariantMap t;
  ackmsg = t;
}

Death::Death(QString key)
{
  kTimeout = 2000;
  kRumorProb = 2;
  kExpiration = 60000;
  kDeletion = 120000;
  key = key;

  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(checkAcks()));
  timer->start(kTimeout);

  QVariantMap t;
  ackmsg = t;
}

Death::~Death()
{
  if (timer) {
    delete(timer);
  }
}
