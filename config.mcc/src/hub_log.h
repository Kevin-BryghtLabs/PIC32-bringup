#ifndef HUB_LOG_H
#define HUB_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

int hubLog(char type, const char * fmt, ...) __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif // HUB_LOG_H
