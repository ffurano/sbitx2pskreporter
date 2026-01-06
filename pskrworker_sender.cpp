#include "logger.h"
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
    timestamp = (timestamp ? timestamp : time(0));

    if (txCall == "<...>") return 0;

    mylog(Logger::Lvl1, "Adding spot '" << txCall << "'");

    // Placeholder for header (0x00, 0x0A,length, time, seq, sessionID)
    // Note that the atctual header content can only be set just before sending
    bool starting = !packet.size();
    if (starting) {
        packet.resize(2 + 2 + 4 + 4 + 4);

        // Note: we shall do this for the first three packets only
        // or every hour or so
        appendRecordFormatDesc(packet);

        // Receiver info record. This is us
        std::vector<uint8_t> record, hdr;

        record.resize(2 + 2);
        record[0] = 0x99;
        record[1] = 0x92;
        appendStringField(record, myCall.toStdString());
        appendStringField(record, myLocator.toStdString());
        appendStringField(record, mySw.toStdString());
        appendStringField(record, myAntinfo.toStdString());
        if (record.size() % 4)
            record.resize(record.size() / 4 * 4 + 4); // padding to 4 bytes
        hdr.clear();
        appendUint16(hdr, record.size()); // Fill the missing part of the header
        record[2] = hdr[0];
        record[3] = hdr[1];
        packet.insert( packet.end(), record.begin(), record.end() );

        preamblelen = packet.size();

    }


    // Now the record that we got from sbitx
    // senderCallsign, frequency, sNR (1 byte), iMD (1 byte), mode,
    // informationSource (1 byte), senderLocator, flowStartSeconds
    std::vector<uint8_t> record;
    record.clear();

    if (starting) {
      record.resize(2 + 2);
      record[0] = 0x99;
      record[1] = 0x93;
    }
    appendStringField(record, txCall);
    appendUint32(record, freqHz+myFreq);
    record.push_back(static_cast<uint8_t>(snr));
    record.push_back(static_cast<uint8_t>(0)); //iMD
    appendStringField(record, "FT8");
    record.push_back(static_cast<uint8_t>(1)); // informationSource = automatic (1)
    appendStringField(record, txLocator);
    appendUint32(record, timestamp);

    packet.insert( packet.end(), record.begin(), record.end() );


    return 0;
}



// We send the packet
int pskrworker::sendAllSpots() {


    // Now we can fill the initial global header... FIXME
    if (packet.size() % 4)
        packet.resize(packet.size() / 4 * 4 + 4); // padding to 4 bytes
    uint16_t totalLen = packet.size();
    uint32_t sessionID = 0x12345679; // static per app session

    // Overwrite header:...
    std::vector<uint8_t> hdr;

    appendUint16(hdr, 0x000a);
    appendUint16(hdr, totalLen); // Insert the final fields into the header
    appendUint32(hdr, time(0));
    appendUint32(hdr, ++seqNum);
    appendUint32(hdr, sessionID);

    for (ulong i = 0; i < hdr.size(); i++) {
        packet[i] = hdr[i];
    }

    hdr.clear();
    appendUint16(hdr, packet.size()-preamblelen); // Adjust the data size in the data header
    packet[preamblelen+2] = hdr[0];
    packet[preamblelen+3] = hdr[1];

    // UDP send
    struct addrinfo hints{}, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;

    if (getaddrinfo(CFG->GetString("pskreporter.host", (char*)"pskreporter.info").c_str(),
                    CFG->GetString("pskreporter.port", (char*)"14739").c_str(), &hints, &res) != 0) {
        mylog(Logger::Lvl0, "DNS lookup failed: " << errno << ":" << strerror(errno));
        return -1;
    }

    int sock = socket(res->ai_family, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        freeaddrinfo(res);
        return -2;
    }


    ssize_t sz =packet.size();
    ssize_t sent = 0;

    // send data
    while (sz > 0) {
        ssize_t r = sendto(sock, packet.data()+sent, sz, 0, res->ai_addr, res->ai_addrlen);
        if (r <= 0) {
          if (errno == EINTR || errno == EAGAIN) {
            mylog(Logger::Lvl0, "udp sendto must retry: "  << errno << ":" << strerror(errno));
            // try again
            continue;
          } else {
            mylog(Logger::Lvl0, "udp sendto failed: "  << errno << ":" << strerror(errno));
            close(sock);
            freeaddrinfo(res);
            return -3;
          }
        }
        // update pointer and size to see if we've got more to send
        sent += r;
        sz -= r;
    }

    close(sock);
    freeaddrinfo(res);

    mylog(Logger::Lvl2, "UDP sent " << sent  << " bytes");

    // Dump to a file
    /* Write your buffer to disk. */
    if (CFG->GetBool("pskreporter.dump", false)) {
      char templ[] = "/tmp/sbitx-dumpXXXXXX";
      int fd = mkstemp(templ);

      if (fd > 0) {
        int towrite = sent;
        int written = 0;
        while ( towrite > 0 ) {
          int res = write(fd, packet.data()+written, towrite);
          if (res > 0) {
              towrite -= res;
              written += res;
          }
          if (res <= 0) {
            if ((errno == EAGAIN) || (errno == EINTR)) continue;
            else {
              mylog(Logger::Lvl0, "Error writing dump packet: " << errno << ":" << strerror(errno));
              break;
            }
          }

        }
      } else
        mylog(Logger::Lvl0, "Error dumping packet: " << errno << ":" << strerror(errno));
     

      close(fd);
    }

    return 0;
}


int pskrworker::packetLoadAndSend(std::string &fn) {

    mylog(Logger::Lvl0, "Loading spool file '" << fn.c_str() << "'");

    std::ifstream ifs;
    ifs.open(fn, std::ios::in|std::ios::binary);

    int32_t sz = 0;
    ifs.read((char *)&sz, sizeof(sz));

    if (sz > 65535 || sz <= 0)
        mylog(Logger::Lvl0, "Wrong file length fn'" << fn.c_str() << "'");

    packet.resize(sz);
    if (!ifs.read(reinterpret_cast<std::ifstream::char_type*>(&packet.front()), sz) ) {
        ifs.close();
        std::remove(fn.c_str());
        packet.clear();

        mylog(Logger::Lvl0, "Error reading spool file '" << fn.c_str() << "'");
        return -1;
    }

    if (!sendAllSpots()) {
        // Success sending, we can delete the spool file
        ifs.close();
        std::remove(fn.c_str());
        packet.clear();
        mylog(Logger::Lvl0, "Successfully sent older spool file '" << fn.c_str() << "'");
        return 0;
    }

    packet.clear();
    return -2;
}

int pskrworker::packetSpool() {
    // Save the packet into the spool directory

    std::ofstream of;


    std::string fn = SPOOL_DIR;
    fn += "/";
    fn += std::to_string(time(0));
    fn += ".report";
    of.open(fn, std::ios::out|std::ios::binary);

    int32_t sz = packet.size();
    of.write((char *)&sz, sizeof(sz));
    of.write((char *)packet.data(), packet.size());
    of.close();
    mylog(Logger::Lvl0, "Written spool file '" << fn.c_str() << "' sz: " << sz);

    return 0;
}



