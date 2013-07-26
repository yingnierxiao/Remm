#include <QDebug>
#include <typeinfo>
#include "../packet.h"
#include "connection.h"

using namespace std;

Connection::Connection(QObject *parent) :
    QObject(parent)
{
    udpSocket = new QUdpSocket(this);

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagram()));
    connect(udpSocket, SIGNAL(connected()), this, SLOT(startCommunicate()));
    connect(udpSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(udpError(QAbstractSocket::SocketError)));
    connect(udpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
            this, SLOT(udpStateChanged(QAbstractSocket::SocketState)));
}

void Connection::tryDirectConnectToRPi(QString address, quint16 port)
{
    udpSocket->close();
    udpSocket->connectToHost(address, port);
}

void Connection::startCommunicate()
{
    qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()";
//    udpSocket->write("Hello!");
//    qDebug() << __PRETTY_FUNCTION__;
}

void Connection::udpError(QAbstractSocket::SocketError error)
{
    qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()"
             << "Error #" << error << " has occured." << udpSocket->errorString();
}

void Connection::udpStateChanged(QAbstractSocket::SocketState state)
{
    qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()"
             << "Current state is " << state;
}

static status_packet_s last_status;

void Connection::data_rx(unsigned char *data, unsigned int length)
{
    status_packet_s *status_packet;

    if (length != sizeof(status_packet_s)) {
        printf("ERROR %s() Unexpected packet size %d: %.*s\n", __FUNCTION__, length, length, data);
        return;
    }
    status_packet = (status_packet_s *)data;
    if (status_packet->magic != MAGIC_STATUS) {
        printf("ERROR %s() Unexpected packet type %d: %.*s\n", __FUNCTION__, status_packet->magic, length, data);
        return;
    }

    memcpy(&last_status, status_packet, sizeof(status_packet_s));
    printf("INFO  %s() height=%d, direction=%d, gps_latitude=%f, gps_longitude=%f, "
            "slope=%d, battery_charge=%d, info=%s.\n", __FUNCTION__,
            status_packet->height, status_packet->direction, status_packet->gps_latitude,
            status_packet->gps_longitude, status_packet->slope, status_packet->battery_charge, status_packet->info);
}

void Connection::picture_rx(unsigned char *data, unsigned int length)
{
    printf("INFO  %s() Picture has just received.\n", __FUNCTION__);

    if ((length == 0) || (data == NULL)) {
        printf("ERROR %s() Incorrect arguments data=0x%X, length=%d\n", __FUNCTION__, (unsigned int)data, length);
        return;
    }

    QImage jpgImage;
    jpgImage.loadFromData(data, length, "JPG");
    emit pictureReceived(jpgImage);

    char filename[] = "captured.jpg";
    FILE *file = fopen(filename, "wb");
    fwrite(data, 1, length, file);
    fclose(file);
}

/**	Check for picture inside packet.
 *  Return 1 if picture has found. Otherwise return 0.
 */
int Connection::picture_assembly(unsigned char *data, unsigned int length)
{
    static picture_packet_s prev_packet = { /*magic*/ MAGIC_STATUS, /*picture_id*/ -1, /*picture_size*/ 0, /*fragment_id*/ 0, /*fragment_size*/ 0, /*data*/ "" } ;
    static unsigned char *picture;
    picture_packet_s *packet = (picture_packet_s *)data;

    if (packet->magic != MAGIC_PICTURE)
        return 0;

    if (packet->picture_id < prev_packet.picture_id) {
        printf("INFO  %s() Received frame %d for old picture %d. Skip it.\n",
               __FUNCTION__, packet->picture_id, prev_packet.picture_id);
        return 1;
    }
    if (packet->picture_id > prev_packet.picture_id) {
        picture_rx(picture, prev_packet.picture_size);
        free(picture);
        printf("INFO  %s() Start receiving new picture %d.\n", __FUNCTION__, packet->picture_id);
        prev_packet = *packet;
        picture = (unsigned char *)malloc(packet->picture_size);
    }
    unsigned int dest_pos = packet->fragment_id * FRAGMENT_SIZE;
    if ((dest_pos + packet->fragment_size) > packet->picture_size) {
        printf("ERROR %s() Incorrect packet - May occur \"Out of memory\"."
               "Allocated memory size = %dbytes, request %dbytes. fragment_id=%d, fragment_size=%d.\n",
               __FUNCTION__, packet->picture_size, dest_pos + packet->fragment_size, packet->fragment_id, packet->fragment_size);
        return 1;
    }
    memcpy(&picture[dest_pos], packet->data, packet->fragment_size);
    return 1;
}

void Connection::readPendingDatagram()
{
    unsigned char *data;
    unsigned int length = udpSocket->bytesAvailable();
    qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()";
    qDebug() << length;
    data = new (nothrow) unsigned char[length];
    if (data == NULL) {
        qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()"
                 << "Can't allocate" << length << "bytes for datagramm";
        return;
    }
    int readed = udpSocket->read((char *)data, length);
    if (readed != (int)length) {
        qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()"
                 << "Can't read all data. Received" << length << "bytes, readed -" << readed;
        delete[] data;
        return;
    }

    qDebug() << typeid(*this).name() << ":" << __FUNCTION__ << "()"
             << "Received" << length << "bytes";
    if (picture_assembly(data, length) == 1)
        return;

    data_rx(data, length);
    delete[] data;
}