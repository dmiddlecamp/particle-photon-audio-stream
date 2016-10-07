#include "SparkIntervalTimer.h"
#include "SimpleRingBuffer.h"
#include <math.h>

#define MICROPHONE_PIN DAC1
#define AUDIO_BUFFER_MAX 8192

#define SINGLE_PACKET_MIN 512
#define SINGLE_PACKET_MAX 1024

#define TCP_SERVER_PORT 1234
#define SERIAL_DEBUG_ON true

// 1/8000th of a second is 125 microseconds

//#define AUDIO_TIMING_VAL 125 /* 8,000 hz */
//#define AUDIO_TIMING_VAL 83 /* 12,000 hz */
#define AUDIO_TIMING_VAL 62 /* 16kHz */

//#define AUDIO_TIMING_VAL 30 /* 16,000 hz */

//#define AUDIO_TIMING_VAL 50  /* 20,000 hz */


uint8_t txBuffer[SINGLE_PACKET_MAX + 1];

//uint8_t *txBuffer = &buffer1;
//uint8_t *rxBuffer = &buffer2;
//
//bool bufferSelector = true;
//int bufferIndex1 = 0;
//int bufferIndex2 = 0;
//
//uint8_t buffer1[AUDIO_BUFFER_MAX];
//uint8_t buffer2[AUDIO_BUFFER_MAX];


SimpleRingBuffer audio_buffer;
//SimpleRingBuffer recv_buffer;



// version without timers
unsigned long lastRead = micros();
unsigned long lastSend = millis();
char myIpAddress[24];

TCPClient audioClient;
TCPClient checkClient;
TCPServer audioServer = TCPServer(TCP_SERVER_PORT);

IntervalTimer readMicTimer;

//float _volumeRatio = 0.50;
int _sendBufferLength = 0;
unsigned int lastPublished = 0;
bool _isRecording = false;

volatile int counter = 0;


void setup() {
    #if SERIAL_DEBUG_ON
    Serial.begin(115200);
    #endif

    setADCSampleTime(ADC_SampleTime_3Cycles);

    pinMode(MICROPHONE_PIN, INPUT);
    pinMode(D7, OUTPUT);

    //DAC_Cmd(DAC_Channel_1, ENABLE);
    //DAC_SetChannel1Data(DAC_Align_12b_R, 0);


//    Particle.function("setVolume", onSetVolume);

    int mySampleRate = AUDIO_TIMING_VAL;

    Particle.variable("ipAddress", myIpAddress, STRING);
    Particle.variable("sample_rate", &mySampleRate, INT);
    Particle.publish("sample_rate", " my sample rate is: " + String(AUDIO_TIMING_VAL));

    IPAddress myIp = WiFi.localIP();
    sprintf(myIpAddress, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);

    //recv_buffer.init(AUDIO_BUFFER_MAX);
    audio_buffer.init(AUDIO_BUFFER_MAX);


    // // send a chunk of audio every 1/2 second
    // sendAudioTimer.begin(sendAudio, 1000, hmSec);

    audioServer.begin();

    lastRead = micros();

    readMicTimer.begin(readMic, AUDIO_TIMING_VAL, uSec);

    //startRecording();
}

unsigned int lastLog = 0;
unsigned int lastClientCheck = 0;


void loop() {
    unsigned int now = millis();

    if ((now - lastClientCheck) > 100) {
        lastClientCheck = now;

        checkClient = audioServer.available();
        if (checkClient.connected()) {
            audioClient = checkClient;
        }
    }


    #if SERIAL_DEBUG_ON
    if ((now - lastLog) > 1000) {
        lastLog = now;

        Serial.println("counter was " + String(counter));
        //Serial.println("audio buffer size is now " + String(audio_buffer.getSize()));
        counter = 0;

    }

    #endif

    sendEvery(100);

}

// Callback for Timer 1
void readMic(void) {
    //read audio
    uint16_t value = analogRead(MICROPHONE_PIN);
    //int32_t value = pinReadFast(MICROPHONE_PIN);

    //uint16_t value = (int)DAC_GetDataOutputValue(DAC_Channel_1);

    value = map(value, 0, 4095, 0, 255);
    audio_buffer.put(value);


    counter++;
}




//void startRecording() {
//    if (!_isRecording) {
//        readMicTimer.begin(readMic, AUDIO_TIMING_VAL, uSec);
//    }
//    _isRecording = true;
//}
//
//void stopRecording() {
//    if (_isRecording) {
//        readMicTimer.end();
//    }
//    _isRecording = false;
//}



//int onSetVolume(String cmd) {
//    _volumeRatio = cmd.toFloat() / 100;
//}

//
//void listenAndSend(int delay) {
//    unsigned long startedListening = millis();
//
//    while ((millis() - startedListening) < delay) {
//        unsigned long time = micros();
//
//        if (lastRead > time) {
//            // time wrapped?
//            //lets just skip a beat for now, whatever.
//            lastRead = time;
//        }
//
//        //125 microseconds is 1/8000th of a second
//        if ((time - lastRead) > 125) {
//            lastRead = time;
//            readMic();
//        }
//    }
//    sendAudio();
//}


void sendEvery(int delay) {
    // if it's been longer than 100ms since our last broadcast, then broadcast.
    if ((millis() - lastSend) >= delay) {
        sendAudio();
        lastSend = millis();
    }
}



// Callback for Timer 1
void sendAudio(void) {
    if (!audioClient.connected()) {
        return;
    }
    int count = 0;
    int storedSoundBytes = audio_buffer.getSize();

    // don't read out more than the max of our ring buffer
    // remember, we're also recording while we're sending
    while (count < storedSoundBytes) {

        if (audio_buffer.getSize() < SINGLE_PACKET_MIN) {
            break;
        }

        // read out max packet size at a time

        // for loop should be faster, since we can check our buffer size just once?
        int size = min(audio_buffer.getSize(), SINGLE_PACKET_MAX);
        int c = 0;
        for(int c = 0; c < size; c++) {
            txBuffer[c] = audio_buffer.get();
        }
        count += size;

//        while ((audio_buffer.getSize() > 0) && (c < SINGLE_PACKET_MAX)) {
//            txBuffer[c++] = audio_buffer.get();
//            count++;
//        }
        //_sendBufferLength = c - 1;

        // send it!
        audioClient.write(txBuffer, size);
    }
}


//void copyAudio(uint8_t *bufferPtr) {
//
//    int c = 0;
//    while ((audio_buffer.getSize() > 0) && (c < AUDIO_BUFFER_MAX)) {
//        bufferPtr[c++] = audio_buffer.get();
//    }
//    _sendBufferLength = c - 1;
//}



void write_socket(TCPClient socket, uint8_t *buffer, int count) {

    socket.write(buffer, count);

//
//
//    int i=0;
//    uint16_t val = 0;
//
//    int tcpIdx = 0;
//    uint8_t tcpBuffer[1024];
//
//    while( (val = buffer[i++]) < 65535 ) {
//        if ((tcpIdx+1) >= 1024) {
//            socket.write(tcpBuffer, tcpIdx);
//            tcpIdx = 0;
//        }
//
//        tcpBuffer[tcpIdx] = val & 0xff;
//        tcpBuffer[tcpIdx+1] = (val >> 8);
//        tcpIdx += 2;
//    }
//
//    // any leftovers?
//    if (tcpIdx > 0) {
//        socket.write(tcpBuffer, tcpIdx);
//    }
}


//void write_UDP(uint8_t *buffer) {
//    int stopIndex=_sendBufferLength;
//
//
//    #if SERIAL_DEBUG_ON
//        Serial.println("SENDING " + String(stopIndex));
//    #endif
//    Udp.sendPacket(buffer, stopIndex, broadcastAddress, UDP_BROADCAST_PORT);
//}

bool ledState = false;
void toggleLED() {
    ledState = !ledState;
    digitalWrite(D7, (ledState) ? HIGH : LOW);
}