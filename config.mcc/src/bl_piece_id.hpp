#ifndef BL_PIECE_ID_HPP
#define BL_PIECE_ID_HPP

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void pieceIdInit();
bool pieceIdReady(uint32_t now);
void pieceIdStart();

void pieceIdTask();
bool pieceIdInProgress();

void pieceIdAddPrioritySquares(uint64_t bitboard);

void pieceIdSetQuiet(bool quiet);

bool pieceIdSetFrequencyParams(unsigned idx, uint32_t frequency, uint16_t drainCounts48MHz, uint16_t ringCounts48MHz);
void pieceIdSetFrequencyCount(unsigned count);
unsigned pieceIdGetFrequencyCount();
uint32_t pieceIdGetFrequency(unsigned idx);
uint16_t pieceIdGetDelay(unsigned idx);
uint16_t pieceIdGetRing(unsigned idx);

#ifdef __cplusplus
}
#endif

#endif // BL_PIECE_ID_HPP
