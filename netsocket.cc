#include <unistd.h>

#include "netsocket.hh"

NetSocket::NetSocket()
{
	// Pick a range of four UDP ports to try to allocate by default,
	// computed based on my Unix user ID.
	// This makes it trivial for up to four Peerster instances per user
	// to find each other on the same host,
	// barring UDP port conflicts with other applications
	// (which are quite possible).
	// We use the range from 32768 to 49151 for this purpose.
	myPortMin = 32768 + (getuid() % 4096)*4;
	myPortMax = myPortMin + 3;
  kRumorProb = 2; // 1/kRumorProb is probability rumors stop when encountering infected node
}

bool NetSocket::bind()
{
	// Try to bind to each of the range myPortMin..myPortMax in turn.
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (QUdpSocket::bind(p)) {
      boundPort = p;
      address = QHostAddress(QHostAddress::LocalHost);
			qDebug() << "bound to UDP port " << p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

void NetSocket::findNeighbors()
{
  neighbors = new QVector<QPair<QHostAddress, int> >();
  for (int p = myPortMin; p <= myPortMax; p++) {
    if (boundPort != p) {
      // running on zoo machines, so our neighbors are all on localhost
      neighbors->append(qMakePair(QHostAddress(QHostAddress::LocalHost), p));
    }
  }
}

QByteArray* NetSocket::serialize(QVariantMap msg)
{
  QByteArray *serialized = new QByteArray();
  QDataStream out(serialized, QIODevice::OpenMode(QIODevice::ReadWrite));
  out << msg;
  return serialized;
}

// writes datagram with message, host, and port
void NetSocket::sendResponseMessage(QVariantMap msg, QHostAddress host, int port)
{
  QByteArray *serialized = serialize(msg);
  if (QUdpSocket::writeDatagram(serialized->data(), serialized->size(), host, port) == -1) {
    qDebug() << "failed to send";
    sleep(1000);
    sendResponseMessage(msg, host, port);
  }
}

// sends the specified rumor to a random neighboring node
void NetSocket::sendRandomMessage(QVariantMap msg)
{
  QPair<QHostAddress, int> neighbor = neighbors->at(rand() % neighbors->size()); 
  qDebug() << "Sending message to port " << neighbor.second;
  
  sendResponseMessage(msg, neighbor.first, neighbor.second); 
}

// sends acknowledgement of rumor
void NetSocket::sendAck(int ack, QVariantMap msg)
{
  // create message
  QVariantMap ackmsg;
  ackmsg.insert(QString("Ack"), ack);
  ackmsg.insert(QString("Key"), msg[QString("Key")].toString());
  ackmsg.insert(QString("Version"), msg[QString("Version")].toInt());

  qDebug() << "Sending ack to port " << msg[QString("Port")].toInt();
  
  sendResponseMessage(ackmsg, QHostAddress(msg[QString("Host")].toString()), msg[QString("Port")].toInt());
}

// deserialize a datagram into a QVariantMap msg
QVariantMap NetSocket::deserialize()
{
  QByteArray *deserialized = new QByteArray();
  int size = pendingDatagramSize();
  deserialized->resize(size);
  readDatagram(deserialized->data(), size);

  QDataStream in(deserialized, QIODevice::OpenMode(QIODevice::ReadOnly));
  QVariantMap msg;
  in >> msg;
  return msg;
}
