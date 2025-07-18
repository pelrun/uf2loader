// Minimal unicode support for FatFS
// This provides only ASCII support to save space

#include "ff.h"

#if FF_USE_LFN

// Simple ASCII-only implementations
WCHAR ff_uni2oem(DWORD uni, WORD cp) {
    (void)cp;
    return (uni < 0x80) ? (WCHAR)uni : '?';
}

WCHAR ff_oem2uni(WCHAR oem, WORD cp) {
    (void)cp;
    return (oem < 0x80) ? oem : '?';
}

DWORD ff_wtoupper(DWORD uni) {
    if (uni >= 'a' && uni <= 'z') {
        return uni - 'a' + 'A';
    }
    return uni;
}

#endif 