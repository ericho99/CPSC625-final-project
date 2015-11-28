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
			qDebug() << "bound to UDP port " << p;
			return true;
		}
	}

	qDebug() << "Oops, no ports in my default range " << myPortMin
		<< "-" << myPortMax << " available";
	return false;
}

void NetSocket::sendDatagrams(QVariantMap msg)
{
  // send message to all other udp ports
	for (int p = myPortMin; p <= myPortMax; p++) {
		if (boundPort != p) {
      QByteArray *serialized = new QByteArray();
      QDataStream out(serialized, QIODevice::OpenMode(QIODevice::ReadWrite));
      out << msg;

      if (QUdpSocket::writeDatagram(serialized->data(), serialized->size(), QHostAddress(QHostAddress::LocalHost), p) == -1) {
        qDebug() << "failed to send";
      } 
		}
	}
}

VersionTracker::VersionTracker()
{
  versions = new QMap<QString, QPair<QString, int> >();
}

int VersionTracker::findMostRecentVersion(QString key)
{
  return 1;
}

void VersionTracker::eliminateOldVersions(QString key)
{
  return;
}

void FrontDialog::readPendingMessages()
{
  while (sock->hasPendingDatagrams()) {
    QByteArray *g = new QByteArray();
    int size = sock->pendingDatagramSize();
    g->resize(size);
    sock->readDatagram(g->data(), size);

    QDataStream in(g, QIODevice::OpenMode(QIODevice::ReadOnly));
    QVariantMap msg;
    in >> msg;

    //for (QVariantMap::const_iterator i = msg.begin(); i != msg.end(); ++i) {
    //  qDebug() << i.key() << ": " << i.value();
    //}
    if (msg.contains(QString("Key"))) {
      // writes key
      std::fstream fs;
      QString filename = sock->dir_name + "/" + msg[QString("Key")].toString();
      fs.open(filename.toStdString().c_str(), std::fstream::out);
      if (msg.contains(QString("Value"))) {
        fs << msg[QString("Value")].toString().toStdString();
      }
      fs.close();
    }
  }
}

void FrontDialog::put(QString dir_name, QString key, QString value)
{
  std::fstream fs;
  fs.open((dir_name + "/" + key).toStdString().c_str(), std::fstream::out);
  fs << value.toStdString();
  fs.close();
}

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
  vt->versions->insert(key, qMakePair(value, version));
  put(sock->dir_name, key, value);
 
  // sending messages
  QVariantMap msg;
  msg.insert(QString("Key"), key);
  msg.insert(QString("Value"), value);

  sock->sendDatagrams(msg); 

  // Clear the inputs to get ready for the next input message.
  keyfield->clear();
  valuefield->clear();
}

FrontDialog::FrontDialog()
{
	setWindowTitle("DB");
  vt = new VersionTracker();

	// Create a UDP network socket
	sock = new NetSocket();
	if (!sock->bind())
		exit(1);

  // directory to store key/values is just dir plus the port number, stored in directory db
  sock->dir_name = "db/dir" + QString::number(sock->boundPort);
  mkdir(sock->dir_name.toStdString().c_str(), S_IRWXU); // creates the directory

  // starts listening for messages
  connect(sock, SIGNAL(readyRead()),
          this, SLOT(readPendingMessages()));

	keyfield = new QLineEdit(this);

	valuefield = new QTextEdit(this);
  valuefield->setFocus();

  // submit button for putting key/value
  putbutton = new QPushButton("Put", this);

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
