#include <initializer_list>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "io_pins.h"
#include "can.h"
#include "test_logic.h"
#include "can/can_common.h"
#include "containers/fifo_buffer.h"
#include "global.h"

extern BaseSequentialStream *chp;
extern OutputMode outputMode;

static const CANConfig cancfg = {
  CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
  CAN_BTR_SJW(0) | CAN_BTR_TS2(1) |
  CAN_BTR_TS1(8) | CAN_BTR_BRP(6)
};

static bool isGoodCanPackets = true;
static bool hasReceivedAnalog = false;
static int outputCount = -1;
static int lowSideOutputCount = -1;

static fifo_buffer_sync<CANTxFrame> txFifo;

static void canPacketError(const char *msg, ...) {
	va_list vl;
	va_start(vl, msg);
	chvprintf(chp, msg, vl);
	va_end(vl);

	isGoodCanPackets = false;
}

void startNewCanTest() {
    isGoodCanPackets = true;
    hasReceivedAnalog = false;
}

bool isHappyCanTest() {
    return isGoodCanPackets && hasReceivedAnalog;
}

int getOutputCount() {
	return outputCount;
}

int getLowSideOutputCount() {
	return lowSideOutputCount;
}

static bool wasBoardDetectError = false;

static void receiveBoardStatus(const uint8_t msg[8]) {
	int boardId = (msg[0] << 8) | msg[1];
	int numSecondsSinceReset = (msg[2] << 16) | (msg[3] << 8) | msg[4];

	if (outputMode.displayCanReceive) {
	    chprintf(chp, "       CAN RX BoardStatus: BoardID=%d numSecs=%d\r\n", boardId, numSecondsSinceReset);
	}
	if (currentBoard == nullptr) {
		for (int boardIdx = 0; boardIdx < getBoardsCount(); boardIdx++) {
			BoardConfig &c = getBoardConfigs()[boardIdx];
			for (int boardRev = 0; c.boardIds[boardRev] > 0; boardRev++) {
				if (boardId == c.boardIds[boardRev]) {
					currentBoard = &c;
					currentBoardRev = boardRev;
					chprintf(chp, " * Board detected: %s rev.%c\r\n", currentBoard->boardName, 'A' + currentBoardRev);
				}
			}
		}
	}
	if (currentBoard == nullptr && !wasBoardDetectError) {
		canPacketError("Error! Couldn't detect, unknown board!\r\n");
		wasBoardDetectError = true;
	}
}

static void receiveOutputMetaInfo(const uint8_t msg[8]) {
	if (msg[0] == CAN_BENCH_HEADER) {
		outputCount = msg[2];
		lowSideOutputCount = msg[3];
    	if (outputMode.displayCanReceive) {
    	    chprintf(chp, "       CAN RX outputCount total=%d low=%d \r\n", outputCount, lowSideOutputCount);
    	}
	}
}

static void receiveRawAnalog(const uint8_t msg[8]) {
	// wait for the BoardStatus package first
	if (currentBoard == nullptr)
		return;
	hasReceivedAnalog = true;

	for (int ch = 0; ch < 8; ch++) {
		// channel not used for this board
		if (currentBoard->channels[ch].name == nullptr)
			continue;
		float voltage = getVoltageFrom8Bit(msg[ch]) * currentBoard->channels[ch].mulCoef;
		// check if in acceptable range for this board
		if (voltage < currentBoard->channels[ch].acceptMin || voltage > currentBoard->channels[ch].acceptMax) {
			canPacketError(" * BAD channel %d (%s): voltage %f (raw %d) not in range (%f..%f)\r\n",
				ch, currentBoard->channels[ch].name, voltage, msg[ch], 
				currentBoard->channels[ch].acceptMin, currentBoard->channels[ch].acceptMax);
		}
	}
}

static void printRxFrame(const CANRxFrame& frame, const char *msg) {
    if (!outputMode.displayCanReceive) {
        return;
    }
		chprintf(chp, "Processing %s ID=%x/l=%x %x %x %x %x %x %x %x %x\r\n",
		msg,
		        CAN_EID(frame),
		        frame.DLC,
				frame.data8[0], frame.data8[1],
				frame.data8[2], frame.data8[3],
				frame.data8[4], frame.data8[5],
				frame.data8[6], frame.data8[7]);
}

void processCanRxMessage(const CANRxFrame& frame) {
	if (CAN_EID(frame) == BENCH_TEST_BOARD_STATUS) {
          printRxFrame(frame, "BENCH_TEST_BOARD_STATUS");
		receiveBoardStatus(frame.data8);
	} else if (CAN_EID(frame) == BENCH_TEST_RAW_ANALOG) {
	    printRxFrame(frame, "BENCH_TEST_RAW_ANALOG");
		receiveRawAnalog(frame.data8);
	} else if (CAN_EID(frame) == BENCH_TEST_EVENT_COUNTERS) {
	    printRxFrame(frame, "BENCH_TEST_EVENT_COUNTERS");
	} else if (CAN_EID(frame) == BENCH_TEST_BUTTON_COUNTERS) {
	    printRxFrame(frame, "BENCH_TEST_BUTTON_COUNTERS");
	} else if (CAN_EID(frame) == BENCH_TEST_IO_META_INFO) {
	    printRxFrame(frame, "BENCH_TEST_IO_META_INFO");
	    receiveOutputMetaInfo(frame.data8);
	}
}

static void sendCanTxMessage(const CANTxFrame & frame) {
	if (!txFifo.put(frame)) {
		chprintf(chp, "CAN sendCanTxMessage() problems");
	}
}

static void sendCanTxMessage(int EID, std::initializer_list<uint8_t> data) {
	CANTxFrame txmsg;
	txmsg.IDE = CAN_IDE_EXT;
	txmsg.EID = EID;
	txmsg.RTR = CAN_RTR_DATA;
	txmsg.DLC = 8;
	
	size_t idx = 0;
    for (uint8_t v : data) {
		txmsg.data8[idx] = v;
		idx++;
	}
	sendCanTxMessage(txmsg);
}

void sendCanPinState(uint8_t pinIdx, bool isSet) {
	sendCanTxMessage(BENCH_TEST_IO_CONTROL, { CAN_BENCH_HEADER, (uint8_t)(isSet ? CAN_BENCH_GET_SET : CAN_BENCH_GET_CLEAR), pinIdx });
}

void setOutputCountRequest() {
	sendCanTxMessage(BENCH_TEST_IO_CONTROL, { CAN_BENCH_HEADER, CAN_BENCH_GET_COUNT });
}

static THD_WORKING_AREA(can_tx_wa, THREAD_STACK);
static THD_FUNCTION(can_tx, p) {
  CANTxFrame txmsg;

  (void)p;
  chRegSetThreadName("transmitter");

  while (true) {
	if (txFifo.get(txmsg, TIME_MS2I(100))) {
    	canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg, TIME_MS2I(100));
    }
  }
}

static THD_WORKING_AREA(can_rx_wa, THREAD_STACK);
static THD_FUNCTION(can_rx, p) {
  CANRxFrame rxmsg;

  (void)p;
  while (true) {
    msg_t result = canReceiveTimeout(&CAND1, CAN_ANY_MAILBOX, &rxmsg, TIME_INFINITE);
	if (result != MSG_OK) {
		continue;
	}

    processCanRxMessage(rxmsg);
  }
}

void initCan() {
  palSetPadMode(CAN_PORT,CAN_PIN_RX, PAL_MODE_ALTERNATE(EFI_CAN_AF));
  palSetPadMode(CAN_PORT,CAN_PIN_TX, PAL_MODE_ALTERNATE(EFI_CAN_AF));

  canStart(&CAND1, &cancfg);

  chThdCreateStatic(can_tx_wa, sizeof(can_tx_wa), NORMALPRIO + 7,
                    can_tx, NULL);
  chThdCreateStatic(can_rx_wa, sizeof(can_rx_wa), NORMALPRIO + 7,
                    can_rx, NULL);
}
