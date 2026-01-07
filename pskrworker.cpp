#include "pskrworker.h"
#include "qobjectdefs.h"
#include <sys/socket.h>
#include <stdio.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include "logger.h"
#include <stdlib.h>

pskrworker::pskrworker() {  // let's make socket

    // telnet options
    const telnet_telopt_t telnetOpts[] = {
                                          {     TELNET_TELOPT_ECHO, TELNET_WONT,   TELNET_DO},
                                          {    TELNET_TELOPT_TTYPE, TELNET_WILL, TELNET_DONT},
                                          {TELNET_TELOPT_COMPRESS2, TELNET_WONT,   TELNET_DO},
                                          {     TELNET_TELOPT_MSSP, TELNET_WONT,   TELNET_DO},
                                          {                     -1,           0,           0}};

    // initialize telnet box
    mTelnet = telnet_init(telnetOpts, trampoline, 0, this);
    connected = false;
    interrupt = false;

    seqNum = 1;
    myFreq = 0;
    mLastreport_t = time(0);
}



int pskrworker::parseOneReport(std::string &msg, int pend, int pstart)
{

        // NB we could have several reports in the buffer. Make sure we treat just the first one here

        // Vector of string to save tokens
        std::vector <std::string> tokens;
        char buf[64];

        // Copy the {} block here before parsing
        // If the {} block is larger than 64 characters it's clearly an error. Abort the parsing
        int nn = pend-pstart-1;
        if (nn > 64) {
            mylog(Logger::Lvl0, "Buffer overflow '" << msg <<  "'");
            return -1;
        }
        strncpy(buf, &msg[pstart+1], nn);
        buf[nn] = '\0';

        // Returns first token
        char *token = strtok(buf, " ");

        // Keep getting tokens while one of the
        // delimiters present in buf.

        while (token != NULL){
            tokens.push_back(token);
            token = strtok(NULL, " ");
        }

        // This may be the response to a frequency enquiry. We must set our base freq with it
        if (tokens.size() == 2 && tokens[0] == "freq:") {
            int frq = atoi(tokens[1].c_str());
            if (frq != myFreq) {
              myFreq = frq;
              mylog(Logger::Lvl0, "Changing base frequency " << myFreq);
            }
            // Now delete the processed part from the beginning
            msg.erase(0, pend+1);
            // Remember the time of the last report
            mLastreport_t = time(0);
            return 0;
        }
        if (tokens.size() < 4) {
            mylog(Logger::Lvl0, "Invalid token count " << tokens.size());
            // Now delete the processed part from the beginning
            msg.erase(0, pend+1);
            return -3;
        }
        int freq = atoi(tokens[3].c_str());
        if ((freq <= 0) || (freq > 5000)) {
            mylog(Logger::Lvl0, "Invalid ft8 freq '" << tokens[3] << "' buf: '" << msg << "'");
            // Now delete the processed part from the beginning
            msg.erase(0, pend+1);
            return -2;
        }
        int snr = atoi(tokens[2].c_str());
        if ((snr < -50) || (snr > 20)) {
            mylog(Logger::Lvl0,"Invalid snr '" << tokens[2] << "' buf: '" << msg << "'");
            // Now delete the processed part from the beginning
            msg.erase(0, pend+1);
            return -3;
        }

        // OK, here we have the tokens of the FT8 message
        if (tokens[4] == "CQ") {
            // We must also support calls like CQ POTA, hence the callsign is not necessarily the 5th position
            // Since the number of tokens in a CQ is very variable, we just do our best to detect known CQ decorations
            //this->addPskReporterSpot(tokens[tokens.size()-2], tokens[tokens.size()-1], freq, snr, time(0));
            if ( (tokens[5] == "POTA") || (tokens[5] == "SOTA") || (tokens[5] == "IOTA") || (tokens[5] == "YOTA") ||
                (tokens[5].size() <= 3) )
                this->addPskReporterSpot(tokens[5], tokens[6], freq, snr, time(0));
            else
                this->addPskReporterSpot(tokens[5], tokens[6], freq, snr, time(0));
        }
        else {
            // Avoid adding ourself
            if ( strcasecmp(tokens[5].c_str(), this->myCall.toStdString().c_str()) )
              this->addPskReporterSpot(tokens[5], "", freq, snr, time(0));
        }

        // Now delete the processed part from the beginning
        msg.erase(0, pend+1);

        // Remember the time of the last report
        mLastreport_t = time(0);

        return 0;
}

// The main loop of the reader thread
void pskrworker::run() {

    bool needmoredata = true;
    while (!interrupt) {
        if (!connected) {

            // Free the previous connection resources, if any
            if (mSockFd)
                ::close(mSockFd);

            // Let's connect to the sbitx via telnet
            if (!sbitxMakeConnection()) {
                // Ask the sbitx what is the current base frequency
                mReceivedMsg.clear();
                sbitxcommand("freq ?");
                connected = true;
                needmoredata = true;
            }
            else {
                mylog(Logger::Lvl0, "Error connecting to sbitx " << sbitx_host.toStdString() << ":" << sbitx_port);
                mReceivedMsg.clear();
                ::sleep(10);
                continue;
            }
        }

        // Wait for some message to come from the telnet conn. There is a reasonable timeout
        if (needmoredata) {


            myresult r = sbitxread();
            if (r.code == -2) {// Timeout, nothing happened
                time_t t = time(0);
                if (t - mLastreport_t > 2) {
                    // Ask again the sbitx what is the current base frequency
                    sbitxcommand("freq ?");
                }
                continue;
            }
            //mylog(Logger::Lvl2, mReceivedMsg.c_str());
            // Treat various errors
            if (r.code) {
                // Reading error. We disconnect/reconnect after a pause. Timeout is not an error
                mReceivedMsg.clear();
                mylog(Logger::Lvl0, "Error reading from sbitx " << sbitx_host.toStdString() << ":" << sbitx_port << " - "  + r.msg.toStdString());
                connected = false;
                ::sleep(10);
                continue;
            }
            needmoredata = false;
        }


        while (1) {
            // Got a string that looks like an ft8 report? Incredible! Try to decode the ft8 activity
            // Look for a {} closed block in the buffer. Otherwise continue
            int pcnt = 0, pstart = -1, pend = -1;
            for (uint p = 0; p < mReceivedMsg.length(); p++) {
                if (mReceivedMsg[p] == '{') {
                    if (!pcnt) pstart = p;
                    pcnt++;
                }
                else if (mReceivedMsg[p] == '}') {
                    if (pcnt)
                        pcnt--;
                    if (!pcnt) pend = p;
                    break; // found!
                }
            }

            // No {} block found ... we need more data!
            // If there was no open { we can just clear the buffer so far
            if (pstart < 0) {
                mReceivedMsg.clear();
                needmoredata = true;
                break;
            }
            // Open { found but not yet a matching closed } ... wait for more data
            if (pend < 0) {
                needmoredata = true;
                break;
            }


            // Parse. Does it look like an FT8 report? Loop through the entries and add them
            // Example ft8 report from telnet:
            // {201845  35 +00 1678 ~  BI4IWL ON9DJ R-23
            // }{201845  33 -02 2444 ~  AP2N PA8R 73
            // }{201845  33 -06 1481 ~  CQ EA7KN IM76
            // }{201845  33 -03 903  ~  IU1FJV RU5A RR73
            // }{201845  32 -01 812  ~  R6LFO R2BNC RR73
            // }{201845  32 -06 1909 ~  CQ SP2DW JO94
            // }{201845  31 -06 1962 ~  VU2LMO RW1CW KO59
            // }{201845  30 -02 2197 ~  AC8RJ SV1PMQ RR73
            // }{201845  26 -08 1284 ~  R1BOY 5Z4VJ -08
            // }{201845  26 -06 200  ~  IZ4JMA <...> -04
            // }{201845  26 -03 2141 ~  DG0OFT DL3RAJ R-04
            // }{201845  25 -04 2072 ~  YO5ONK K1WY R+12
            // }{201845  25 -06 2603 ~  CT4KQ UA6YW -18
            // }{201845  23 -06 1131 ~  PE1PEO KA9ONW -15
            // }{201845  21 -06 2547 ~  CT7AIU R3BDI KO85
            // }{201845  21 -06 294  ~  VK3YW G8OO JO02
            // }{201845  21 -13 2803 ~  K3RH SP0VIP -11
            // }{201845  20 -11 978  ~  CQ CT2FPY IN50
            // }{201845  20 -09 1344 ~  VK3YW M0VTS/P R-05
            // }{201845  20 -12 1034 ~  CQ MW0USK IO81
            // }{201845  19 -03 747  ~  VU2LMO PA7HPH JO22
            // }{201845  17 -08 694  ~  S21IM OE1TRB JN88
            // }{201845  14 -01 1544 ~  BG5JGG DL7UKA JO50
            // }

            // NOTE: the format changed in version 3.5
            // {194615    31 -03 2488 II6WWA EA4LX RR73
            // }{194615    30 -07 1634 CQ CS26REP Portugal 1443 243°
            // }{194615    29 -07  519 OD5KU EA7LGO IM67
            // }{194615    29 -05  241 UP7WWA US1EA KN78
            // }{194615    29 -10 1409 SO3WWA CT1EEX R-07
            // }{194615    28 -09 1544 EM0WWA TA3CKY R-25
            // }{194615    28 -12  663 PD0GD IT9JQN -10
            // }{194615    27 -03 2553 9M8WWA RZ6L KN97
            // }{194615    25 -04  397 BI4SSB UR3QZ R-04
            // }{194615    25 -02 2347 HS0ZOY SV1SJJ KM17
            // }{194615    24 -03  856 BI4SSB RC5F KO85
            // }{194615    24 -09 2091 CT3IQ E2WWA -21
            // }{194615    23 -05 2391 BI6PWL JA4FKX -02
            // }{194615    23 -09 1359 BD6IIC R6OQ -17
            // }{194615    22 -11 1931 EA1FE SV4LGX -20
            // }{194615    22 -04 1194 CQ BH4UTT PM02 China 9025 51°
            // }{194615    19 -10 2134 8Q7PR UT7AM KO70
            // }

            // NB This also deletes the processed message from the beginning of the string
            if (parseOneReport(mReceivedMsg, pend, pstart) < 0) {
                needmoredata = true;
                mReceivedMsg.clear();
                break;
            }
        }

        // TODO: Send the packet to pskreporter, when it's sufficiently full or the timeout elapsed
        time_t t = time(0);
        if ((packet.size() > 0) && ((t - mLastreport_t > 30) || (packet.size() > 1024))) {

            if (!this->sendAllSpots()) {

                // Sending success? Then flush also the older packets that may have been stored in files in the spool directory

                // Loop across files in the spool dir
                struct dirent *entry;
                DIR *dp;

                dp = opendir(SPOOL_DIR);
                if (dp != NULL) {
                    while ((entry = readdir(dp))) {
                        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                            continue;
                        std::string fn = std::string(SPOOL_DIR) + "/" + entry->d_name;
                        packetLoadAndSend(fn);
                    }
                    closedir(dp);
                } else {
                    mylog(Logger::Lvl0, "opendir: Spool path does not exist or could not be read.");
                    int r = mkdir(SPOOL_DIR, 0755);
                    if (r && r != EEXIST)
                        mylog(Logger::Lvl0, "Can't create spool path '" << SPOOL_DIR << "' " << errno << ":" << strerror(errno));
                }


            } else {
                // Otherwise save packet to a new spool file, hoping for better times ahead
                this->packetSpool();
            }
            packet.clear();
        }



    }

}

int pskrworker::sbitxMakeConnection()
{
    int retVal;

    QString host = sbitx_host;
    QString port = QString::number(sbitx_port);
    struct addrinfo * addrInfo;
    struct addrinfo addrHints = {};

    // look up server host
    addrHints.ai_family = AF_UNSPEC; // IPv4 and IPv6 allowed
    addrHints.ai_socktype = SOCK_STREAM;
    addrHints.ai_flags = AI_NUMERICSERV;
    addrHints.ai_protocol = IPPROTO_TCP;

    mylog(Logger::Lvl0, "Connecting to sbitx " << host.toStdString() << ":" << sbitx_port);

    if ((retVal = getaddrinfo(host.toStdString().c_str(), port.toStdString().c_str(), &addrHints, &addrInfo)) != 0) {
        mylog(Logger::Lvl0, "getaddrinfo() failed: " + host.toStdString() << ":" << sbitx_port << " " << gai_strerror(retVal));
        mSockFd = 0;
        return(-1);
    }

    // create socket
    if ((mSockFd = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol)) == -1) {
        mylog(Logger::Lvl0, "socket() failed: " << errno << strerror(errno));
        mSockFd = 0;
        return -2;
    }

    // connect
    if (::connect(mSockFd, addrInfo->ai_addr, addrInfo->ai_addrlen) == -1) {
        close(mSockFd);
        mSockFd = 0;
        mylog(Logger::Lvl0, "connect() failed: " << errno << strerror(errno));

        return -3;
    }

    // free address lookup info
    freeaddrinfo(addrInfo);
    return 0;
}



void pskrworker::trampoline(
    telnet_t *,
    telnet_event_t * event,
    void * user_data)
{
    pskrworker * me = static_cast<pskrworker *>(user_data);
    me->telnetEvent(event);
}

void pskrworker::telnetEvent(telnet_event_t * event)
{
    switch (event->type) {
    // data received
    case TELNET_EV_DATA:
        mReceivedMsg.append( std::string(event->data.buffer, event->data.size) );
#if DEBUG_MODE
            // print only for debugging
        mylog("response: [" << mReceivedMsg << "]");
#endif

        break;
    // data must be sent
    case TELNET_EV_SEND:
        sendAll(event->data.buffer, event->data.size);
        break;
    // execute to enable local feature (or receipt)
    case TELNET_EV_DO:
        throw NotImplemented();
    // demand to disable local feature (or receipt)
    case TELNET_EV_DONT:
        throw NotImplemented();
    // respond to TTYPE commands
    case TELNET_EV_TTYPE:
        throw NotImplemented();
    // respond to particular subnegotiations
    case TELNET_EV_SUBNEGOTIATION:
        throw NotImplemented();
    // error
    case TELNET_EV_ERROR:
        throw std::runtime_error("telnet error: " + std::string(event->error.msg));
    default:
        // ignore
        break;
    }
}

int pskrworker::sendAll(const char * buffer, size_t size)
{
    int retVal = -1;  // default value

    // send data
    while (size > 0) {
        retVal = send(mSockFd, buffer, size, 0);
        if (retVal == 0 || errno == EINTR) {
            // try again
            continue;
        } else if (retVal <= -1) {
            throw std::runtime_error("send() failed: " + std::string(strerror(errno)));
        }

        // update pointer and size to see if we've got more to send
        buffer += retVal;
        size -= retVal;
    }
    return retVal;
}

void pskrworker::sbitxConfigureReadWriteFd()
{
    // clear the set ahead of time
    FD_ZERO(&mReadFd);
    FD_ZERO(&mWriteFd);

    // add our descriptors to the set
    FD_SET(mSockFd, &mReadFd);
    FD_SET(mSockFd, &mWriteFd);
}

void pskrworker::sbitxConfigureTimeout(const size_t seconds)
{
    mTimeout.tv_sec = seconds;
    mTimeout.tv_usec = 0;
}

myresult pskrworker::sbitxcommand(const QString & command)
{
    auto retVal = select(mSockFd + 1, NULL, &mWriteFd, NULL, &mTimeout);
    QString cmd(command);

    cmd += "\n";
    cmd.insert(0, "\n");

    if (retVal == -1) {
        // error occurred in select()
        return myresult(-1, "select() failed: " + QString(strerror(errno)));
    } else if (retVal == 0) {
        return myresult(-2, "timeout occurred");
    } else {
        // send the execute
        if (FD_ISSET(mSockFd, &mWriteFd)) {
#if DEBUG_MODE
            // print only for debugging
            std::cerr << "request: [" << command << "]" << std::endl;
#endif
            telnet_send(mTelnet, command.toStdString().c_str(), command.length());
        }
    }

    return myresult(0, "");
}

myresult pskrworker::sbitxread()
{

    // socket related configuarations
    sbitxConfigureReadWriteFd();
    sbitxConfigureTimeout(timeOut);

    auto retVal = select(mSockFd + 1, &mReadFd, NULL, NULL, &mTimeout);
    if (retVal == -1) {
        // error occurred in select()
        return myresult(-1, "select() failed: " + QString(strerror(errno)));
    } else if (retVal == 0) {
        return myresult(-2, "timeout occurred");
    } else {
        // response the response
        if (FD_ISSET(mSockFd, &mReadFd)) {
            do {
                char buf[1024];
                auto received = recv(mSockFd, buf, 1023, 0);

                if (received > 0) {
                    telnet_recv(mTelnet, buf, received);
                } else if (received == 0) {
                    return myresult(-3, "sbitx connection has been closed.");
                } else {
                    return myresult(-4, "recv() failed: " + QString(strerror(errno)));
                }
            } while (errno == EINTR);
        }
    }

    // NB: here we just pass the read bytes to the telnet library. Here we can't tell if a message is complete or not

    return myresult(0, "");
}


