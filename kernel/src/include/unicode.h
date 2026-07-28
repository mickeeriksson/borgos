#ifndef _UNICODE_H_
#define _UNICODE_H_



/**
 * Lazy conversion from UTF-16LE to ASCII that replaces non-ASCII UTF-16LE code
 * points with question marks. STOLEN FROM XINU
 */
static inline void utf16le_to_ascii(uint16_t utf16le_str[], unsigned int nchars, char *ascii_str)
{
    unsigned int i;

    for (i = 0; i < nchars; i++)
    {
        if (utf16le_str[i] <= 0x7f)
        {
            ascii_str[i] = utf16le_str[i];
        }
        else
        {
            ascii_str[i] = '?';
        }
    }
}

#endif //_UNICODE_H_
