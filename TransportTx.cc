#ifndef TRANSPORT_TX
#define TRANSPORT_TX

#include "ControlPacket_m.h"

#include <string.h>
#include <omnetpp.h>

using namespace omnetpp;

class TransportTx: public cSimpleModule {
private:
    cQueue buffer;
    cMessage *endServiceEvent;

    cOutVector bufferSizeVector;
    cOutVector packetDropVector;
    cOutVector controlPacketReceivedVector;
    cOutVector packetTxVector;

    double controlFactor;

    void controlFlow(cMessage *message);

    bool isControlPacket(cMessage *message);

    void sendDataPacket();

    void scheduleSendPacketWithDelay(simtime_t delay);
    void addPacket(cMessage *message);

    bool isFullQueue();

    void activeQueue();

public:
    TransportTx();
    virtual ~TransportTx();

protected:
    virtual void initialize();
    virtual void finish();
    virtual void handleMessage(cMessage *message);
};
Define_Module(TransportTx);

TransportTx::TransportTx() {
    endServiceEvent = NULL;
}

TransportTx::~TransportTx() {
    cancelAndDelete(endServiceEvent);
}

void TransportTx::initialize() {
    buffer.setName("buffer");
    bufferSizeVector.setName("bufferSize");
    packetDropVector.setName("packetsDropped");
    controlPacketReceivedVector.setName("controlPacketsReceived");
    packetTxVector.setName("packetsTransmitted");

    endServiceEvent = new cMessage("endService");
    controlFactor = 0;
}

void TransportTx::finish() {
}

void TransportTx::controlFlow(cMessage *message) {
    controlPacketReceivedVector.record(1);

    ControlPacket *controlPacket = (ControlPacket*) message;

    int totalBuffer = controlPacket->getTotalBuffer();
    int remainingBuffer = controlPacket->getRemainingBuffer();

    if (remainingBuffer == 0 && controlFactor < 0.1) {
        if(controlFactor == 0) controlFactor = 1e-300;
        else controlFactor *= 100;
    } else if (remainingBuffer != 0) {
        controlFactor /= 10*(totalBuffer/remainingBuffer + 1);
    }

    delete(message);
}

bool TransportTx::isControlPacket(cMessage *message) {
    return message->getKind() == 2;
}

void TransportTx::sendDataPacket() { // Only if there is any packet
    if (!buffer.isEmpty()) {
        cPacket *packet = (cPacket*) buffer.pop();
        send(packet, "toOut$o");
        scheduleSendPacketWithDelay(packet->getDuration());

        packetTxVector.record(1);
    }
}

void TransportTx::scheduleSendPacketWithDelay(simtime_t delay) {
    scheduleAt(simTime() + delay + controlFactor, endServiceEvent);
}

void TransportTx::addPacket(cMessage *message) {
    if (isFullQueue()) {
        // Drop packet
        delete(message);
        this->bubble("packet dropped");
        packetDropVector.record(1);

    } else {
        // Add packet
        buffer.insert(message);
        bufferSizeVector.record(buffer.getLength());

        activeQueue();
    }
}

bool TransportTx::isFullQueue() {
    return buffer.getLength() >= par("bufferSize").intValue();
}

void TransportTx::activeQueue() {
    if (!endServiceEvent->isScheduled()) { // TransportTx is inactive
        scheduleSendPacketWithDelay(0);
    }
}

void TransportTx::handleMessage(cMessage *message) {
    if (message == endServiceEvent) {
        sendDataPacket();
    } else if(isControlPacket(message)) {
        controlFlow(message);
    } else {
        addPacket(message);
    }
}

#endif
