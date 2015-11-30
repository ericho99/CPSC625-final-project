#include <unistd.h>
#include <sys/stat.h>
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
      neighbors->append(qMakePair(address, p));
    }
  }
}

// sends the specified rumor to a random neighboring node
void NetSocket::sendRumor(QVariantMap msg)
{
  QByteArray *serialized = new QByteArray();
  QDataStream out(serialized, QIODevice::OpenMode(QIODevice::ReadWrite));
  out << msg;

  QPair<QHostAddress, int> neighbor = neighbors->at(rand() % neighbors->size()); 
  qDebug() << "Sending rumor to port " << neighbor.second;
  if (QUdpSocket::writeDatagram(serialized->data(), serialized->size(), neighbor.first, neighbor.second) == -1) {
    qDebug() << "failed to send";
    sleep(1000);
    sendRumor(msg);
  }
}

// sends acknowledgement of rumor
void NetSocket::sendAck(int ack, QVariantMap msg)
{
  // create message
  QVariantMap ackmsg;
  ackmsg.insert(QString("Ack"), ack);
  ackmsg.insert(QString("Key"), msg[QString("Key")].toString());
  ackmsg.insert(QString("Version"), msg[QString("Version")].toInt());

  // serialize
  QByteArray *serialized = new QByteArray();
  QDataStream out(serialized, QIODevice::OpenMode(QIODevice::ReadWrite));
  out << ackmsg;

  qDebug() << "Sending ack to port " << msg[QString("Port")].toInt();

  // write msg
  if (QUdpSocket::writeDatagram(
      serialized->data(),
      serialized->size(),
      //QHostAddress(msg[QString("Host")].toString()),
      QHostAddress(QHostAddress::LocalHost),
      msg[QString("Port")].toInt()) == -1) {
    qDebug() << "failed to send";
  }
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
  versions = new QMap<QString, QPair<QString, int> >();
}

// returns the most recent version of the key in the QMap, 0 if non existent
int VersionTracker::findVersion(QString key)
{
  if (versions->contains(key)) {
    return (*versions)[key].second;
  } else {
    return 0;
  }
}

// updates to a new version within the QMap
void VersionTracker::updateVersion(QString key, int version)
{
  if (versions->contains(key)) {
    versions->insert(key, qMakePair((*versions)[key].first, version));
  }
}

void HotRumor::checkAcks()
{
  // if we don't receive an ack at all, or if the node responded positively, we keep sending out messages
  if ((ackmsg.contains(QString("Ack")) and ackmsg.contains(QString("Key")) and
      ackmsg.contains(QString("Version")) and ackmsg[QString("Ack")] == 1 and
      ackmsg[QString("Key")] == key and ackmsg[QString("Version")] == version) or
      not ackmsg.contains(QString("Ack"))) {
    emit sendRumor(msg);
  } else {
    if (rand() % kRumorProb == 0) {
      emit eliminateRumor(key);
    } else {
      emit sendRumor(msg);
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
void FrontDialog::put(QString dir_name, QString key, QString value)
{
  std::fstream fs;
  fs.open((dir_name + "/" + key).toStdString().c_str(), std::fstream::out);
  fs << value.toStdString();
  fs.close();
}

bool FrontDialog::shouldUpdate(int current_version, int new_version)
{
  if (current_version < new_version) {
    return true;
  } else if (current_version == new_version) {
    return false; // NEED TIEBREAKER HERE IF VALUES DIFFER
  } else {
    return false;
  }
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
  if (shouldUpdate(vt->findVersion(key), new_version)) {
    eliminateRumorByKey(key);
    vt->versions->insert(key, qMakePair(value, new_version));
    vt->updateVersion(key, new_version);
    put(sock->dir_name, key, value);

    msg.insert("Host", sock->address.toString());
    msg.insert("Port", sock->boundPort);

    HotRumor *rumor = new HotRumor(msg);
    // connection to delete rumor if necessary
    connect(rumor, SIGNAL(eliminateRumor(QString)), this, SLOT(eliminateRumorByKey(QString)));
    // connection to send rumor
    connect(rumor, SIGNAL(sendRumor(QVariantMap)), sock, SLOT(sendRumor(QVariantMap)));
    hotRumors->append(rumor);
    return 1;
  } else {
    return 0;
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
    }
  }
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
          sock, SLOT(sendRumor(QVariantMap)));

  // adding front end fields
	keyfield = new QLineEdit(this);
  keyfield->setFocus();
	valuefield = new QTextEdit(this);
  putbutton = new QPushButton("Put", this); // submit button

  connect(putbutton, SIGNAL(clicked()),
          this, SLOT(putRequest()));

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
