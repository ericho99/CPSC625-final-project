#include <unistd.h>

#include "quorum.hh"

Quorum::Quorum()
{
  kTimeout = 1000;
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(decideQuorum()));
  timer->start(kTimeout);
}

void Quorum::decideQuorum()
{
  QVariantMap msg;
  emit(quorumDecision(msg));
}

Quorum::~Quorum()
{
  if (timer) {
    delete(timer);
  }
}
