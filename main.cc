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

void NetSocket::sendRumors(QVariantMap msg)
{
  QByteArray *serialized = new QByteArray();
  QDataStream out(serialized, QIODevice::OpenMode(QIODevice::ReadWrite));
  out << msg;

  // send message to all neighbors
  for (int i = 0; i < neighbors->size(); ++i) {
    QPair<QHostAddress, int> neighbor = neighbors->at(i); 
    if (QUdpSocket::writeDatagram(serialized->data(), serialized->size(), neighbor.first, neighbor.second) == -1) {
      qDebug() << "failed to send";
    } 
  }
}

VersionTracker::VersionTracker()
{
  versions = new QMap<QString, QPair<QString, int> >();
}

// returns the most recent version of the key in the QMap, -1 if non existent
int VersionTracker::findMostRecentVersion(QString key)
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

// writes the key/value pair to local storage
void FrontDialog::put(QString dir_name, QString key, QString value)
{
  std::fstream fs;
  fs.open((dir_name + "/" + key).toStdString().c_str(), std::fstream::out);
  fs << value.toStdString();
  fs.close();
}

// updates versioning and writes key/value pair
void FrontDialog::writeKey(QString key, QString value, int version)
{
  vt->versions->insert(key, qMakePair(value, version));
  vt->updateVersion(key, version);
  put(sock->dir_name, key, value);
}

void FrontDialog::readPendingMessages()
{
  while (sock->hasPendingDatagrams()) {
    QByteArray *deserialized = new QByteArray();
    int size = sock->pendingDatagramSize();
    deserialized->resize(size);
    sock->readDatagram(deserialized->data(), size);

    QDataStream in(deserialized, QIODevice::OpenMode(QIODevice::ReadOnly));
    QVariantMap msg;
    in >> msg;

    //for (QVariantMap::const_iterator i = msg.begin(); i != msg.end(); ++i) {
    //  qDebug() << i.key() << ": " << i.value();
    //}
    if (msg.contains(QString("Key")) and msg.contains(QString("Value")) and msg.contains(QString("Version"))) {
      writeKey(msg[QString("Key")].toString(), msg[QString("Value")].toString(), msg[QString("Version")].toInt());
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

  // writes key
  int version = vt->findMostRecentVersion(key) + 1;
  writeKey(key, value, version);
  
  // sending messages
  QVariantMap msg;
  msg.insert(QString("Key"), key);
  msg.insert(QString("Value"), value);
  msg.insert(QString("Version"), version);
  msg.insert(QString("Source"), sock->address.toString());

  sock->sendRumors(msg); 

  // Clear the inputs to get ready for the next input message.
  keyfield->clear();
  valuefield->clear();
}

// instantiates the application
FrontDialog::FrontDialog()
{
	setWindowTitle("DB");
  vt = new VersionTracker();

	// Create a UDP network socket
	sock = new NetSocket();
	if (!sock->bind())
		exit(1); 

  sock->findNeighbors();

  // directory to store key/values is just dir plus the port number, stored in directory db
  sock->dir_name = "db/dir" + QString::number(sock->boundPort);
  mkdir(sock->dir_name.toStdString().c_str(), S_IRWXU); // creates the directory

  // starts listening for messages
  connect(sock, SIGNAL(readyRead()),
          this, SLOT(readPendingMessages()));

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
