#ifndef BL_PIECE_ID_HPP
#define BL_PIECE_ID_HPP

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void initPieceId();
void setupPieceId();
void startPieceId();
void switchPieceId(uint8_t spot);

#ifdef __cplusplus
}
#endif

#endif // BL_PIECE_ID_HPP
