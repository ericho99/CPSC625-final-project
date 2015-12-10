#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <string.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

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
  qDebug() << "host name is " << msg[QString("Host")].toString();
  
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

VersionTracker::VersionTracker()
{
  versions = new QVariantMap();
}

// returns the most recent version of the key in the QMap, 0 if non existent
int VersionTracker::findVersion(QString key)
{
  if (versions->contains(key)) {
    return (*versions)[key].toInt();
  } else {
    return 0;
  }
}

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

// writes the key/value pair to local storage
void FrontDialog::put(QString key, QString value)
{
  std::fstream fs;
  fs.open((sock->dir_name + "/" + key).toStdString().c_str(), std::fstream::out);
  fs << value.toStdString();
  fs.close();
}

// gets value from key
QString FrontDialog::get(QString key)
{
  std::ifstream f((sock->dir_name + "/" + key).toStdString().c_str());
  std::string str;
  f.seekg(0, std::ios::end);   
  str.reserve(f.tellg());
  f.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(f)),
              std::istreambuf_iterator<char>());
  return QString(str.c_str());
}

// remove rumor with key if it exists
void FrontDialog::eliminateRumorByKey(QString key)
{
  HotRumor *rumor;
  for (int i = 0; i < hotRumors->size(); ++i) {
    if (hotRumors->at(i)->key == key) {
      rumor = hotRumors->at(i);
      hotRumors->remove(i);
      delete(rumor);
    }
  }
}

// attaches ack message to proper rumor
void FrontDialog::attachAckMessage(QVariantMap msg)
{
  for (int i = 0; i < hotRumors->size(); ++i) {
    HotRumor *rumor = hotRumors->at(i);
    if (rumor->key == msg[QString("Key")].toString() and
        rumor->version == msg[QString("Version")].toInt()) {
      rumor->ackmsg = msg;
    }
  }
}

// updates versioning and writes key/value pair if necessary, returns an ack number
int FrontDialog::processRumor(QVariantMap msg)
{
  QString key = msg[QString("Key")].toString();
  QString value = msg[QString("Value")].toString();
  int new_version = msg[QString("Version")].toInt();
  if (vt->findVersion(key) < new_version) {
    eliminateRumorByKey(key);
    vt->versions->insert(key, new_version);
    put(key, value);

    msg.insert("Host", sock->address.toString());
    msg.insert("Port", sock->boundPort);

    HotRumor *rumor = new HotRumor(msg);
    // connection to delete rumor if necessary
    connect(rumor, SIGNAL(eliminateRumor(QString)), this, SLOT(eliminateRumorByKey(QString)));
    // connection to send rumor
    connect(rumor, SIGNAL(sendRandomMessage(QVariantMap)), sock, SLOT(sendRandomMessage(QVariantMap)));
    hotRumors->append(rumor);
    return 1;
  } else {
    return 0;
  }
}

// place updates into storage
void FrontDialog::placeUpdates(QVariantMap updates)
{
  for (QVariantMap::const_iterator i = updates.begin(); i != updates.end(); ++i) {
    if (i.value().toMap().contains(QString("Version")) and i.value().toMap().contains(QString("Value"))) {
      if (vt->findVersion(i.key()) < i.value().toMap()[QString("Version")].toInt()) {
        vt->versions->insert(i.key(), i.value().toMap()[QString("Version")].toInt());
        put(i.key(), i.value().toMap()[QString("Value")].toString());
      }
    }
  }
}

// maybe this should be in netsocket class?
void FrontDialog::readPendingMessages()
{
  while (sock->hasPendingDatagrams()) {
    QVariantMap msg = sock->deserialize();
    if (msg.contains(QString("Key")) and msg.contains(QString("Value")) and 
        msg.contains(QString("Version")) and msg.contains(QString("Host")) and
        msg.contains(QString("Port"))) {
      int ack = processRumor(msg);
      qDebug() << "received rumor version " << msg[QString("Version")].toInt() << "sending ack " << ack;
      sock->sendAck(ack, msg);
    } else if (msg.contains(QString("Ack")) and msg.contains(QString("Key")) and
        msg.contains(QString("Version"))) {
      attachAckMessage(msg);
    } else if (msg.contains(QString("State")) and msg.contains(QString("Host")) and
        msg.contains(QString("Port"))) {
      processEntropy(msg);
    } else if (msg.contains(QString("Updates"))) {
      qDebug() << "third one now";
      placeUpdates(msg[QString("Updates")].toMap());
    }
  }
}

// checks new state for required updates, returns required update list
QVariantMap FrontDialog::findRequiredUpdates(QVariantMap newstate, QVariantMap oldstate)
{
  QVariantMap updates;
  for (QVariantMap::const_iterator i = newstate.begin(); i != newstate.end(); ++i) {
    if (oldstate.contains(i.key())) {
      if (oldstate[i.key()].toInt() < i.value().toInt()) {
        updates.insert(i.key(), i.value());
      }
    } else {
      // new key here
      updates.insert(i.key(), i.value());
    }
  }
  return updates;
}

// seeks values from database and attaches them in a variantmap
QVariantMap FrontDialog::attachValuesToUpdates(QVariantMap updates)
{
  QVariantMap updatesWithValues;
  for (QVariantMap::const_iterator i = updates.begin(); i != updates.end(); ++i) {
    QVariantMap m;
    m.insert(QString("Version"), i.value().toInt());
    m.insert(QString("Value"), get(i.key()));
    updatesWithValues.insert(i.key(), m);
  }
  return updatesWithValues; 
}

// creates variantmap with host and port
QVariantMap FrontDialog::createBaseMap()
{
  QVariantMap m;
  m.insert(QString("Host"), sock->address.toString());
  m.insert(QString("Port"), sock->boundPort);
  return m;
}

// processes an anti-entropy message
void FrontDialog::processEntropy(QVariantMap msg)
{
  if (msg.contains(QString("UpdatesFromOrigin")) and msg.contains(QString("UpdatesToOrigin"))) {
    qDebug() << "getting 2nd update";
    QVariantMap updatesWithValues = attachValuesToUpdates(msg[QString("UpdatesFromOrigin")].toMap());
    QVariantMap updatemsg;
    updatemsg.insert("Updates", updatesWithValues);
    sock->sendResponseMessage(updatemsg, QHostAddress(msg[QString("Host")].toString()), msg[QString("Port")].toInt());

    placeUpdates(msg[QString("UpdatesToOrigin")].toMap());
  } else {
    qDebug() << "sending first response";
    QVariantMap newstate = msg[QString("State")].toMap();
    // contains keys that this node needs
    QVariantMap updatesFromOrigin = findRequiredUpdates(newstate, *(vt->versions));

    // obtaining <version, value> pairs that the messaging node requires
    QVariantMap updatesToOrigin = attachValuesToUpdates(findRequiredUpdates(*(vt->versions), newstate));

    QVariantMap ackmsg = createBaseMap();
    ackmsg.insert("State", *(vt->versions));
    ackmsg.insert(QString("UpdatesFromOrigin"), updatesFromOrigin);
    ackmsg.insert(QString("UpdatesToOrigin"), updatesToOrigin);
    
    sock->sendResponseMessage(ackmsg, QHostAddress(msg[QString("Host")].toString()), msg[QString("Port")].toInt());
  }
}

// sends anti entropy status
void FrontDialog::sendAntiEntropy()
{
  QVariantMap msg = createBaseMap();
  msg.insert("State", *(vt->versions));

  sock->sendRandomMessage(msg);
}

// processes a put request
void FrontDialog::putRequest()
{
  // invalid put request with empty key
  if (keyfield->text().isEmpty()) {
    return;
  }

  // grabs input key/value
	QString key = keyfield->text();
  QString value = valuefield->toPlainText();
  qDebug() << "Adding file : " << key;

  // creates message for processing
  QVariantMap msg;
  msg.insert(QString("Key"), key);
  msg.insert(QString("Value"), value);
  msg.insert(QString("Version"), vt->findVersion(key) + 1);
  msg.insert(QString("Host"), sock->address.toString());
  msg.insert(QString("Port"), sock->boundPort);

  processRumor(msg);

  // Clear the inputs to get ready for the next input message.
  keyfield->clear();
  valuefield->clear();
}

// instantiates the application
FrontDialog::FrontDialog()
{
	setWindowTitle("DB");
  vt = new VersionTracker();
  srand(time(0));

	// Create a UDP network socket
	sock = new NetSocket();
	if (!sock->bind())
		exit(1); 

  sock->findNeighbors();

  // directory to store key/values is just dir plus the port number, stored in directory db
  sock->dir_name = "db/dir" + QString::number(sock->boundPort);
  mkdir(sock->dir_name.toStdString().c_str(), S_IRWXU); // creates the directory

  hotRumors = new QVector<HotRumor *>();

  // starts listening for messages
  connect(sock, SIGNAL(readyRead()),
          this, SLOT(readPendingMessages()));
  connect(this, SIGNAL(startRumor(QVariantMap)),
          sock, SLOT(sendRandomMessage(QVariantMap)));

  // adding front end fields
	keyfield = new QLineEdit(this);
  keyfield->setFocus();
	valuefield = new QTextEdit(this);
  putbutton = new QPushButton("Put", this); // submit button

  connect(putbutton, SIGNAL(clicked()),
          this, SLOT(putRequest()));

  // adding antientropy timer
  kAntiEntropyTimeout = 10000;
  antiTimer = new QTimer(this);
  connect(antiTimer, SIGNAL(timeout()), this, SLOT(sendAntiEntropy()));
  antiTimer->start(kAntiEntropyTimeout);

	// Lay out the widgets to appear in the main window.
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(keyfield);
	layout->addWidget(valuefield);
  layout->addWidget(putbutton);
	setLayout(layout);
}

int main(int argc, char **argv)
{
	// Initialize Qt toolkit
	QApplication app(argc,argv);

	// Create an initial chat dialog window
	FrontDialog dialog;
	dialog.show();

	// Enter the Qt main loop; everything else is event driven
	return app.exec();
}
