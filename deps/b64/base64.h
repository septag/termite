#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int Base64decode(char *bufplain, const char *bufcoded);
int Base64encode(char *encoded, const char *string, int len);
int Base64decode_len(const char *bufcoded);
int Base64encode_len(int len);

#ifdef __cplusplus
}
#endif
