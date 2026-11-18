// Unicode text sorting (src/port/collate.cpp).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Compares UTF-8 strings `a` and `b` in language `lang` (enum PortLanguage) the way a dictionary would: base letters
// first (case and accents ignored, digits before letters), then accents, then case. <0, 0 or >0 like strcmp.
int Port_Collate(const char* a, const char* b, int lang);

#ifdef __cplusplus
}
#endif
