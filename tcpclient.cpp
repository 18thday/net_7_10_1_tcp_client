#include "tcpclient.h"




/* ServiceHeader
 * Для работы с потоками наши данные необходимо сериализовать.
 * Поскольку типы данных не стандартные перегрузим оператор << Для работы с ServiceHeader
*/
QDataStream &operator >>(QDataStream &out, ServiceHeader &data) {

    out >> data.id;
    out >> data.idData;
    out >> data.status;
    out >> data.len;

    return out;
};
QDataStream &operator <<(QDataStream &in, ServiceHeader &data) {

    in << data.id;
    in << data.idData;
    in << data.status;
    in << data.len;

    return in;
};

QDataStream &operator >>(QDataStream &out, StatServer &stat) {
    out >> stat.incBytes;
    out >> stat.sendBytes;
    out >> stat.revPck;
    out >> stat.sendPck;
    out >> stat.workTime;
    out >> stat.clients;

    return out;
};

QDataStream &operator <<(QDataStream &in, StatServer &stat) {
    in >> stat.incBytes;
    in >> stat.sendBytes;
    in >> stat.revPck;
    in >> stat.sendPck;
    in >> stat.workTime;
    in >> stat.clients;

    return in;
};


/*
 * Поскольку мы являемся клиентом, инициализацию сокета
 * проведем в конструкторе. Также необходимо соединить
 * сокет со всеми необходимыми нам сигналами.
*/
TCPclient::TCPclient(QObject *parent) : QObject(parent)
{
    socket = new QTcpSocket(this);

    connect(socket, &QTcpSocket::readyRead, this, &TCPclient::ReadyReed);

    connect(socket, &QTcpSocket::connected, this, [&]{
        emit sig_connectStatus(STATUS_SUCCES);
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [&]{
        emit sig_connectStatus(ERR_CONNECT_TO_HOST);
    });

    connect(socket, &QTcpSocket::disconnected, this, &TCPclient::sig_Disconnected);
}

/* write
 * Метод отправляет запрос на сервер. Сериализировать будем
 * при помощи QDataStream
*/
void TCPclient::SendRequest(ServiceHeader head)
{
    QByteArray send_header;
    QDataStream ods(&send_header, QIODevice::WriteOnly);
    ods << head;

    socket->write(send_header);
}

/* write
 * Такой же метод только передаем еще данные.
*/
void TCPclient::SendData(ServiceHeader head, QString str)
{
    QByteArray send_data;
    QDataStream ods(&send_data, QIODevice::WriteOnly);
    ods << head;
    ods << str;

    socket->write(send_data);

}

/*
 * \brief Метод подключения к серверу
 */
void TCPclient::ConnectToHost(QHostAddress host, uint16_t port)
{
    socket->connectToHost(host, port);
}
/*
 * \brief Метод отключения от сервера
 */
void TCPclient::DisconnectFromHost()
{
    socket->disconnectFromHost();
}


void TCPclient::ReadyReed()
{

    QDataStream ids(socket);

    if (ids.status() != QDataStream::Ok) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Warning);
        msg.setText("Ошибка открытия входящего потока для чтения данных!");
        msg.exec();
    }


    //Читаем до конца потока
    while(ids.atEnd() == false) {
        //Если мы обработали предыдущий пакет, мы скинули значение idData в 0
        if(servHeader.idData == 0) {

            //Проверяем количество полученных байт. Если доступных байт меньше чем
            //заголовок, то выходим из обработчика и ждем новую посылку. Каждая новая
            //посылка дописывает данные в конец буфера
            if(socket->bytesAvailable() < sizeof(ServiceHeader)) {
                return;
            }
            else {
                //Читаем заголовок
                ids >> servHeader;
                //Проверяем на корректность данных. Принимаем решение по заранее известному ID
                //пакета. Если он "битый" отбрасываем все данные в поисках нового ID.
                if(servHeader.id != ID){
                    uint16_t hdr = 0;
                    while(ids.atEnd()){
                        ids >> hdr;
                        if(hdr == ID){
                            ids >> servHeader.idData;
                            ids >> servHeader.status;
                            ids >> servHeader.len;
                            break;
                        }
                    }
                }
            }
        }
        //Если получены не все данные, то выходим из обработчика. Ждем новую посылку
        if(socket->bytesAvailable() < servHeader.len) {
            return;
        }
        else {
            //Обработка данных
            ProcessingData(servHeader, ids);
            servHeader.idData = 0;
            servHeader.status = 0;
            servHeader.len = 0;
        }
    }
}


/*
 * Остался метод обработки полученных данных. Согласно протоколу
 * мы должны прочитать данные из сообщения и вывести их в ПИ.
 * Поскольку все типы сообщений нам известны реализуем выбор через
 * switch. Реализуем получение времени.
*/

void TCPclient::ProcessingData(ServiceHeader header, QDataStream &stream)
{

    switch (header.idData) {
        case GET_TIME : {
            QDateTime time;
            stream >> time;
            emit sig_sendTime(time);
            break;
        }
        case GET_SIZE: {
            uint32_t free_space;
            stream >> free_space;
            emit sig_sendFreeSize(free_space);
            break;
        }
        case GET_STAT: {
            StatServer stat;
            stream >> stat;
            emit sig_sendStat(stat);
            break;
        }
        case SET_DATA: {
            if (header.status == STATUS_SUCCES) {
                QString data;
                stream >> data;
                emit sig_SendReplyForSetData(data);
            } else if (header.status == ERR_NO_FREE_SPACE) {
                emit sig_Error(header.status);
            } else {
                emit sig_Error(ERR_NO_FUNCT);
            }
            break;
        }
        case CLEAR_DATA: {
            emit sig_Success(CLEAR_DATA);
            break;
        }
        default:
            return;
    }

}
