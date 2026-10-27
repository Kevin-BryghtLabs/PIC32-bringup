#ifndef BL_PIECE_ID_HPP
#define BL_PIECE_ID_HPP

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

#ifdef __cplusplus
}
#endif

#endif // BL_PIECE_ID_HPP
