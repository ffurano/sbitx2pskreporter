#include "pskrworker.h"
#include "qobjectdefs.h"
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <vector>


#include "config.h"

// Helper to append integer in network byte order:
void appendUint32(std::vector<uint8_t>& buf, uint32_t value) {
    uint32_t v = htonl(value);
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + sizeof(v));
}


// Helper to append integer in network byte order:
void appendUint16(std::vector<uint8_t>& buf, uint16_t value) {
    uint16_t v = htons(value);
    buf.insert(buf.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + sizeof(v));
}

// Helper to append a length-prefixed string:
void appendStringField(std::vector<uint8_t>& buf, const std::string& s) {
    buf.push_back(static_cast<uint8_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}


// append the record format descriptor. We use the long one that also contains the antenna info
// Docs at: https://pskreporter.info/pskdev.html
void appendRecordFormatDesc(std::vector<uint8_t>& buf) {

    // This is information on the receiving station, i.e. us
    // receiverCallsign, receiverLocator, decodingSoftware, antennaInformation
    uint8_t hdr[] = { 0x00, 0x03,  0x00,  0x2C,  0x99,  0x92,  0x00,  0x04,  0x00,  0x01,
                       0x80,  0x02,  0xFF,  0xFF,  0x00,  0x00,  0x76,  0x8F,
                       0x80,  0x04,  0xFF,  0xFF,  0x00,  0x00,  0x76,  0x8F,
                       0x80,  0x08,  0xFF,  0xFF,  0x00,  0x00,  0x76,  0x8F,
                       0x80,  0x09,  0xFF,  0xFF,  0x00,  0x00,  0x76,  0x8F,
                       0x00,  0x00 };
    int sz = sizeof(hdr) / sizeof(uint8_t);

    for (int i = 0; i < sz; i++)
        buf.push_back(hdr[i]);

    // This is information on the received spots
    // senderCallsign, frequency, sNR (1 byte), iMD (1 byte), mode, informationSource (1 byte), senderLocator, flowStartSeconds
    uint8_t hdr2[] = { 0x00, 0x02, 0x00, 0x44, 0x99, 0x93, 0x00, 0x08,
                     0x80, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x05, 0x00, 0x04, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x07, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x0A, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x0B, 0x00, 0x01, 0x00, 0x00, 0x76, 0x8F,
                     0x80, 0x03, 0xFF, 0xFF, 0x00, 0x00, 0x76, 0x8F,
                     0x00, 0x96, 0x00, 0x04 };
    sz = sizeof(hdr2) / sizeof(uint8_t);

    for (int i = 0; i < sz; i++)
        buf.push_back(hdr2[i]);
}



int pskrworker::addPskReporterSpot(
    const std::string& txCall, const std::string& txLocator,
    uint32_t freqHz,
    int8_t snr, uint32_t timestamp
    ) {
    timestamp = timestamp ? timestamp : static_cast<uint32_t>(std::time(nullptr));

    mylog(QString("Adding spot'") + txCall.c_str() + "'");

    // Placeholder for header (0x00, 0x0A,length, time, seq, sessionID)
    // Note that the atctual header content can only be set just before sending
    if (!packet.size()) {
        packet.resize(2 + 2 + 4 + 4 + 4);

        // Note: we shall do this for the first three packets only
        // or every hour or so
        appendRecordFormatDesc(packet);

        // Receiver info record. This is us
        std::vector<uint8_t> record, hdr;
        int pos = 0;
        record.resize(2 + 2);
        record[pos++] = 0x99;
        record[pos++] = 0x92;
        appendStringField(record, myCall.toStdString());
        appendStringField(record, myLocator.toStdString());
        appendStringField(record, mySw.toStdString());
        appendStringField(record, myAntinfo.toStdString());
        if (record.size() % 4)
            record.resize(record.size() / 4 * 4 + 4); // padding to 4 bytes
        hdr.clear();
        appendUint32(hdr, record.size()); // Fill the missing part of the header
        record[pos++] = hdr[0];
        record[pos++] = hdr[1];
        packet.insert( packet.end(), record.begin(), record.end() );

    }


    // Now the record that we got from sbitx
    // senderCallsign, frequency, sNR (1 byte), iMD (1 byte), mode,
    // informationSource (1 byte), senderLocator, flowStartSeconds
    std::vector<uint8_t> record, hdr;
    record.clear();
    hdr.clear();
    int pos = 0;
    record.resize(2 + 2);
    record[pos++] = 0x99;
    record[pos++] = 0x93;
    appendStringField(record, txCall);
    appendUint32(record, freqHz);
    record.push_back(static_cast<uint8_t>(snr));
    record.push_back(static_cast<uint8_t>(0)); //iMD
    appendStringField(record, "FT8");
    record.push_back(static_cast<uint8_t>(1)); // informationSource = automatic (1)
    appendStringField(record, txLocator);
    appendUint32(record, timestamp);
    if (record.size() % 4)
        record.resize(record.size() / 4 * 4 + 4); // padding to 4 bytes
    hdr.clear();
    appendUint32(hdr, record.size()); // Fill the missing part of the header
    record[pos++] = hdr[0];
    record[pos++] = hdr[1];
    packet.insert( packet.end(), record.begin(), record.end() );


    return 0;
}



// We send the packet
int pskrworker::sendAllSpots() {

    if (!packet.size())
        return 0;

    // Now we can fill the initial global header... FIXME
    uint16_t totalLen = packet.size();
    uint32_t sessionID = 0x12345678; // static per app session

    // Overwrite header:...
    std::vector<uint8_t> hdr;

    appendUint16(hdr, htons(0x0a00));
    appendUint16(hdr, totalLen); // Insert the final fields into the header
    appendUint32(hdr, time(0));
    appendUint32(hdr, ++seqNum);
    appendUint32(hdr, sessionID);

    for (ulong i = 0; i < hdr.size(); i++) {
        packet[i] = hdr[i];
    }

    // UDP send
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    if (getaddrinfo(CFG->GetString("pskreporter.host", (char*)"pskreporter.info").c_str(),
                    CFG->GetString("pskreporter.port", (char*)"14739").c_str(), &hints, &res) != 0) {
        std::cerr << "DNS lookup failed\n";
        return -1;
    }

    int sock = socket(res->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        freeaddrinfo(res);
        return -2;
    }

    ssize_t sent = sendto(sock, packet.data(), packet.size(), 0, res->ai_addr, res->ai_addrlen);
    close(sock);
    freeaddrinfo(res);

    if (sent < 0) {
        std::cerr << "sendto failed\n";
        return -3;
    }

    mylog(QString("UDP sent ") + QString::number(sent) + " bytes");

    // Dump to a file
    /* Write your buffer to disk. */
    if (CFG->GetBool("pskreporter.dump", false)) {
      char templ[] = "/tmp/sbitx-dumpXXXXXX";
      int fd = mkstemp(templ);

      if (fd > 0) {
        int towrite = sent;  
        while ( towrite > 0 ) {
          int res = write(fd, packet.data(), sent);
          if (res > 0) towrite -= res;
          if ((res == EAGAIN) && (res == EINTR)) continue;
          else {
            mylog(QString("Error writing dump packet: ") + QString::number(errno) + strerror(errno));
            break;
          }

        }
      } else
        mylog(QString("Error dumping packet: ") + QString::number(errno) + strerror(errno));
     

      close(fd);
    }

    mylog(QString("UDP sent ") + QString::number(sent) + " bytes");
    return 0;
}


int pskrworker::packetLoadAndSend(std::string &fn) {
    std::ifstream ifs;
    ifs.open(fn, std::ios::in|std::ios::binary);

    int32_t sz = 0;
    ifs.read((char *)&sz, sizeof(sz));

    if (sz > 65535 || sz <= 0)
        mylog(QString("Wrong file length fn'") + fn.c_str() + "'");

    packet.resize(sz);
    if (!ifs.read(reinterpret_cast<std::ifstream::char_type*>(&packet.front()), sz) ) {
        ifs.close();
        std::remove(fn.c_str());
        packet.clear();

        mylog(QString("Error reading spool file '") + fn.c_str() + "'");
        return -1;
    }

    if (!sendAllSpots()) {
        // Success sending, we can delete the spool file
        ifs.close();
        std::remove(fn.c_str());
        packet.clear();
        mylog(QString("Successfully sent older spool file '") + fn.c_str() + "'");
        return 0;
    }

    return -2;
}

int pskrworker::packetSpool() {
    // Save the packet into the spool directory
    std::ofstream of;


    std::string fn = SPOOL_DIR + std::to_string(time(0)) + ".report";
    of.open(fn, std::ios::out|std::ios::binary);

    int32_t sz = packet.size();
    of.write((char *)&sz, sizeof(sz));
    of.write((char *)packet.data(), packet.size());
    of.close();

    return 0;
}


