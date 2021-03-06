#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <fstream>
#include <string.h>

#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include "main.hh"

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
    QVariantMap msg;
    msg.insert(QString("Key"), i.key());
    msg.insert(QString("Value"), i.value().toMap()[QString("Value")].toString());
    msg.insert(QString("Version"), i.value().toMap()[QString("Version")].toInt());
    processRumor(msg);
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
      placeUpdates(msg[QString("Updates")].toMap());
    } else if (msg.contains(QString("QuorumCall")) and msg.contains(QString("Key")) and 
        msg.contains(QString("Version"))) {
      sendQuorumResponse(msg);
    } else if (msg.contains(QString("QuorumAck"))) {
      processQuorumResponse(msg);
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
    QVariantMap updatesWithValues = attachValuesToUpdates(msg[QString("UpdatesFromOrigin")].toMap());
    QVariantMap updatemsg;
    updatemsg.insert("Updates", updatesWithValues);
    sock->sendResponseMessage(updatemsg, QHostAddress(msg[QString("Host")].toString()), msg[QString("Port")].toInt());

    placeUpdates(msg[QString("UpdatesToOrigin")].toMap());
  } else {
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

// clears all input lines in the front end dialog
void FrontDialog::clearAllInputs()
{
  putKeyField->clear();
  putValueField->clear();
  getKeyField->clear();
  deleteKeyField->clear();
  getValueField->setPlaceholderText("will contain value of get request");
}

// processes a put request
void FrontDialog::putRequest()
{
  // invalid put request with empty key
  if (putKeyField->text().isEmpty()) {
    return;
  }

  // grabs input key/value
	QString key = putKeyField->text();
  QString value = putValueField->text();
  qDebug() << "Adding file : " << key;

  // creates message for processing
  QVariantMap msg = createBaseMap();
  msg.insert(QString("Key"), key);
  msg.insert(QString("Value"), value);
  msg.insert(QString("Version"), vt->findVersion(key) + 1);

  processRumor(msg);

  // Clear the inputs to get ready for the next input message.
  clearAllInputs();
}

// processes a quorum response msg
void FrontDialog::processQuorumResponse(QVariantMap msg)
{
  if (msg[QString("Key")] == quorum->key) {
    quorum->processQuorumResponse(msg); 
  }
}

// sending response to quorum call
void FrontDialog::sendQuorumResponse(QVariantMap msg)
{
  QString key = msg[QString("Key")].toString();
  int version = msg[QString("Version")].toInt();

  // only send the value back if the version is as fresh or fresher 
  // than the one the requester has
  if (version <= vt->findVersion(key)) {
    QVariantMap ackmsg = createBaseMap();
    ackmsg.insert(QString("Key"), key);
    ackmsg.insert(QString("Value"), get(key));
    ackmsg.insert(QString("Version"), vt->findVersion(key));
    ackmsg.insert(QString("QuorumAck"), QString("QuorumAck"));

    sock->sendResponseMessage(ackmsg, QHostAddress(msg[QString("Host")].toString()), msg[QString("Port")].toInt());
  }
}

// quorum over, decision made 
void FrontDialog::quorumDecision(QString value)
{
  getValueField->setPlaceholderText(value);

  if (quorum) {
    delete(quorum);
  }
}

// sends a request to all nodes for a key/value
void FrontDialog::gatherQuorum(QString key)
{
  QVariantMap msg = createBaseMap();
  msg.insert(QString("Key"), key);
  msg.insert(QString("Version"), vt->findVersion(key));
  msg.insert(QString("QuorumCall"), QString("QuorumCall"));

  for (int i = 0; i < sock->neighbors->size(); ++i) {
    QPair<QHostAddress, int> neighbor = sock->neighbors->at(i);
    sock->sendResponseMessage(msg, neighbor.first, neighbor.second); 
  }
}

// processes a get request
void FrontDialog::getRequest()
{
  // invalid get request with empty key
  if (getKeyField->text().isEmpty()) {
    return;
  }
  
  QString key = getKeyField->text();
  qDebug() << "Getting file : " << key;

  getValueField->setPlaceholderText("please wait...");
  putKeyField->clear();
  putValueField->clear();
  deleteKeyField->clear();

  quorum = new Quorum(key, get(key), vt->findVersion(key));
  gatherQuorum(key);
  connect(quorum, SIGNAL(quorumDecision(QString)), this, SLOT(quorumDecision(QString)));
}

// processes a delete request
void FrontDialog::deleteRequest()
{
  // invalid delete request with empty key
  if (deleteKeyField->text().isEmpty()) {
    return;
  }
  
  QString key = deleteKeyField->text();
  qDebug() << "Deleting file : " << key;

  clearAllInputs();
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

  // adding put fields
	putKeyField = new QLineEdit(this);
  putKeyField->setFocus();
	putValueField = new QLineEdit(this);
  putButton = new QPushButton("Put (key/value)", this); // submit button

  // adding get fields
  getKeyField = new QLineEdit(this);
  getValueField = new QLineEdit(this);
  getValueField->setPlaceholderText("will contain value of get request");
  getValueField->setReadOnly(true);
  getButton = new QPushButton("Get (key)", this);
  
  // adding delete fields
  deleteKeyField = new QLineEdit(this);
  deleteButton = new QPushButton("Delete (key)", this);

  connect(putButton, SIGNAL(clicked()),
          this, SLOT(putRequest()));
  connect(getButton, SIGNAL(clicked()),
          this, SLOT(getRequest()));
  connect(deleteButton, SIGNAL(clicked()),
          this, SLOT(deleteRequest()));

  // adding antientropy timer
  kAntiEntropyTimeout = 15000;
  antiTimer = new QTimer(this);
  connect(antiTimer, SIGNAL(timeout()), this, SLOT(sendAntiEntropy()));
  antiTimer->start(kAntiEntropyTimeout);

	// Lay out the widgets to appear in the main window.
	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(putKeyField);
	layout->addWidget(putValueField);
  layout->addWidget(putButton);
  layout->addWidget(getKeyField);
  layout->addWidget(getValueField);
  layout->addWidget(getButton);
  layout->addWidget(deleteKeyField);
  layout->addWidget(deleteButton);

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
