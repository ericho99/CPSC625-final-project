#include <unistd.h>

#include "quorum.hh"

Quorum::Quorum(QString key, QString value, int version)
{
  kTimeout = 1000;
  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(decideQuorum()));
  timer->start(kTimeout);
  key = key;
  responses = new QVector<QPair<QString, int> >();
  responses->append(qMakePair(value, version));
}

// processes a quorum response msg
void Quorum::processQuorumResponse(QVariantMap msg)
{
  QPair<QString, int> response = qMakePair(msg[QString("Value")].toString(), msg[QString("Version")].toInt());
  responses->append(response);
}

// decides from the list of responses the proper quorum result
void Quorum::decideQuorum()
{
  if (responses->size() > 0) {
    int maxVersion = responses->at(0).second;
    QVector<QPair<QString, int> > valueCounts;
    for (int i = 0; i < responses->size(); ++i) {
      QPair<QString, int> response = responses->at(i);
      // new max version, so new valueCounts as well
      if (response.second > maxVersion) {
        maxVersion = response.second;
        QVector<QPair<QString, int> > temp;
        valueCounts = temp;
        valueCounts.append(response);
      } else if (response.second == maxVersion) {
        bool foundValue = false;
        for (int j = 0; j < valueCounts.size(); ++j) {
          QPair<QString, int> vc = valueCounts.at(i);
          if (vc.first == response.first) {
            vc.second = vc.second + 1;
            foundValue = true;
            break;
          }
        }

        // add new valueCount to the mix
        if (not foundValue) {
          valueCounts.append(qMakePair(response.first, 1));
        }
      }
    }

    // find largest count
    int largestCount = valueCounts.at(0).second;
    for (int i = 0; i < valueCounts.size(); ++i) {
      if (valueCounts.at(i).second > largestCount) {
        largestCount = valueCounts.at(i).second;
      }
    }

    // return first instance of that largest count
    for (int i = 0; i < valueCounts.size(); ++i) {
      if (valueCounts.at(i).second == largestCount) {
        emit(quorumDecision(valueCounts.at(i).first));
        return;
      }
    }
  }
}

Quorum::~Quorum()
{
  if (timer) {
    delete(timer);
  }
}
