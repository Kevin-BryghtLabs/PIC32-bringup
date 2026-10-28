#ifndef BL_PIECE_ID_HPP
#define BL_PIECE_ID_HPP

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void initPieceId();
void setupPieceId();
void startPieceId(uint8_t spot);
void switchPieceId(uint8_t spot);
void pieceIdTask();
bool pieceIdComplete();
bool getPieceIdResult(const uint16_t ** ptr, size_t * count);
bool pieceIdSetFrequencyParams(unsigned idx, uint32_t frequency, uint16_t drainCounts48MHz, uint16_t ringCounts48MHz);

#ifdef __cplusplus
}
#endif

#endif // BL_PIECE_ID_HPP
