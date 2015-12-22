#ifndef NETSOCKET_CLASS_HH
#define NETSOCKET_CLASS_HH

#include <QUdpSocket>
#include <QVariantMap>

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

#endif
