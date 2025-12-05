#ifndef PSKRWORKER_H
#define PSKRWORKER_H

// This class manages the thread that receives from sbitx and sends the udp packets to pskreporter
// sudo apt install libtelnet-dev


#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <libtelnet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QDateTime>

#include "logger.h"


#define SPOOL_DIR "/tmp/sbit2pskr"

class myresult {
public:
    int code;
    QString msg;
    myresult(int c, QString m): code(c), msg(m) {};

    bool Ok() {
        return (code == 0);
    }
};

    class NotImplemented : public std::logic_error
    {
    public:
        NotImplemented()
            : std::logic_error("Function not yet implemented") {}
    };

    class SocketConnectionError : public std::runtime_error
    {
    public:
        explicit SocketConnectionError(const std::string & err)
            : std::runtime_error(err) {}
    };






class pskrworker : public QThread
{
    Q_OBJECT
public:
    pskrworker();

    QMutex mutex;
    QWaitCondition condition;

    const size_t timeOut = 2;
    const size_t bufferSize = 2048;

    bool connected, interrupt;

    // execute a command on the server
    myresult sbitxcommand(const QString & command);

    // receive the response from the server and pass it to the telnet library
    myresult sbitxread();

    // These need to be filled on startup
    QString pskr_host, sbitx_host;
    int pskr_port;
    int sbitx_port;
    QString myCall, myLocator, mySw, myAntinfo;

    int join();
    ~pskrworker() {
        // clean up
        telnet_free(mTelnet);
        close(mSockFd);
    };

    int parseOneReport(std::string &msg, int pend, int pstart);

protected:
    void run() override;

    int addPskReporterSpot(
        const std::string& txCall,
        const std::string& txLocator, uint32_t freqHz,
        int8_t snr, uint32_t timestamp = 0
        );
    int sendAllSpots();
    int packetSpool();
    int packetLoadAndSend(std::string &fn);
signals:
    void log(QString line);

private:

    // Stuff for the telnet client to work
    int mSockFd;
    std::string mReceivedMsg; // The received message
    fd_set mReadFd;
    fd_set mWriteFd;
    struct timeval mTimeout;
    telnet_t * mTelnet;

    int myFreq;
    time_t mLastreport_t;

    static void trampoline(
        telnet_t * telnet,
        telnet_event_t * event,
        void * user_data);

    int sbitxMakeConnection();
    void sbitxConfigureTimeout(const size_t seconds);
    void sbitxConfigureReadWriteFd();

    void telnetEvent(telnet_event_t * event);

    // Send the accumulated packet to pskreporter
    int sendAll(const char * buffer, size_t size);

    myresult mythread();

    // The packet where we accumulate spots. Let's be kind to psk reporter
    std::vector<uint8_t> packet;
    uint32_t seqNum;
    int preamblelen;

};

#endif // PSKRWORKER_H
