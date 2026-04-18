#include "types.h"
//#include "error.h"
#include <stdarg.h>
//#include "kconsole.h"
#include "string.h"

//TODO include error.h instead
#define EOF -1


/* Number of binare bits in an 'unsigned long',ie the maximum size an unsigned long can be in format 'b01010101...'  */
//#define BITS_IN_UNSIGNEDLONG (8 * sizeof(unsigned long))
#define BITS_IN_UNSIGNEDLONG 66


enum value_size_enum {
    SHORT_SHORT_LEN,        //hh
    SHORT_LEN,              //h
    NORMAL_INT_LEN,         //
    LONG_LEN                //l
};

/*
Convert integer to string

PARAMS:
- value     An unsigned long number to convert
- str       Destination buffer; should be 66 characters long for radix2, 24 - radix8, 22 - radix10, 18 - radix16.
- radix     Radix must be in range -36 .. 36. Negative values used for signed numbers.

 OBS! itoa is not a standard c function, therefore we put it here, for other cases use sprintf instead of itoa.

*/

char* _itoa (unsigned long value,  char str[],  int radix)
{
    char        buf [66];
    //char        buf [128];
    char*       dest = buf + sizeof(buf);
    //boolean     sign = false;

    if (value == 0) {
        memcpy (str, "0", 2);
        return str;
    }

//    if (radix < 0) {
//        radix = -radix;
//        if ( (long long) value < 0) {
//            value = -value;
//            sign = true;
//        }
//    }

    *--dest = '\0';

    switch (radix)
    {
        case 16:
            while (value) {
                * --dest = '0' + (value & 0xF);
                if (*dest > '9')
                    *dest += 'A' - '9' - 1;
                value >>= 4;
            }
            break;
        case 10:
            while (value) {
                *--dest = '0' + (value % 10);
                value /= 10;
            }
            break;

        case 8:
            while (value) {
                *--dest = '0' + (value & 7);
                value >>= 3;
            }
            break;

        case 2:
            while (value) {
                *--dest = '0' + (value & 1);
                value >>= 1;
            }
            break;

        default:            // The slow version, but universal
            while (value) {
                *--dest = '0' + (value % radix);
                if (*dest > '9')
                    *dest += 'A' - '9' - 1;
                value /= radix;
            }
            break;
    }

    //if (sign) *--dest = '-';

    memcpy (str, dest, buf +sizeof(buf) - dest);
    return str;
}


/**
 *
 *
 * .
 *
 * @param format
 *      Format string.
 * @param ap
 *      Variable-length list of values that will be formatted.
 * @param putc_func
 *      Character output function.
 *      It is passed two arguments; the first is the char to output.
 *      The second will be arg oto putc. anything that fits in a regular cpu register. (putc will know what to make of it....)
 * @param putc_arg
 *      Second argument to putc_func.
 *
 * @return
 *      number of characters written on success, or ??? on failure
 */
int _doprnt(const char *format, va_list ap, int (*putc_func) (int, reg_t), reg_t putc_arg)
{
    //kconsole_print("In _doprnt\n");
    //kconsole_print("Format = ");
    //kconsole_print(format);
    //kconsole_print("\n");
    //putc_func('.',putc_arg);

    const char* fmt=format;
    int charcount = 0;
    int ret;
    int i;
    const char *cur_spec;     /* Current specifier */
    unsigned int base;          /* Base to use for printing.            */
    int len_str;                /* No. of chars from str to output      */
    char string[BITS_IN_UNSIGNEDLONG + 1]; /* Buffer for numeric conversions       */
    char *str;                  /* Pointer to char in current section to output      */
    enum value_size_enum value_size;     /* Length modifier                      */
    long long_arg;                  /* used for Numeric argument value  */
    unsigned long ulong_arg;        /* used for Numeric unsigned argument value                    */
    char sign;                  /* Set to '-' for negative decimals     */
    unsigned char hashtype;     /* 0 = no hash flag, 1 = hashflag set   */
                                /* 2 = octal 0 prefix 3 = 0x hex prefix */
    char prefix[3];				/* Prefix characters                    */
    int prefix_len;				/* No of characters in prefix string    */
    prefix[2] = '\0';  //NEver used, just for safetey....
    int fmin;                   /* Minimum field width                  */
    int leftjust;              /* 1 = left-justified,
                                   0 = right-justified              */
    char pad_char;              /* Padding character                    */
    int num_zeroes;             /* No. of zeroes to precede number with
                                   (for precision, not zero padding)    */
    int len_nonpadding;         /* Total No. of non-padding chars to
                                   output                               */
    int len_padding;            /* No. of padding chars to output       */

    while(*fmt!='\0'){
        //kconsole_print("\n.\n");
        if (*fmt == '%' && *++fmt != '%') {
            //start parsing of %-output-section
            //each section has 5 fields
            //* Flags:
            //* Minimum field width:
            //* Precision:
            //* Length modifier
            //* Conversion specifier

            cur_spec = fmt-1;
            //kconsole_print("\ncurrent_spec=[");
            //kconsole_print(cur_spec);
            //kconsole_print("]\n");

            /*************************************
             * 1. PARSE FLAGS  *
             *************************************/
            prefix_len = 0;			/* Default: No prefix string */
            pad_char = ' ';			/* Default: space padding    */
            leftjust = 0;		/* Default: right-justified  */
            hashtype = 0;			/* Default: no hash flag     */
            for ( ; ; fmt++)
            {
                switch (*fmt)
                {
                    case '-':
                        /* '-' flag: left-justified conversion  */
                        leftjust = 1;
                        break;
                    case '#':
                        /* '#' flag: alternative conversion     */
                        hashtype = 1;
                        break;
                    case '0':
                        /* '0' flag: pad field width with zeroes
                         * (valid for numeric conversions only)  */
                        pad_char = '0';
                        break;

                    default:
                        goto end_flags_scanning; //all flags scanned
                }
            }
            end_flags_scanning:

            /*************************************
             * 2. Optional minimum field width   *
             *************************************/
            fmin = 0;
            if (*fmt == '*')
            {
                fmin = va_arg(ap, int);
                if (fmin < 0)
                {
                    /* C99 7.19.6.1:  A negative field width argument is taken
                     * as a '-' flag followed by a positive field width.  */
                    fmin = -fmin;
                    leftjust = 1;
                }
                fmt++;
            }
            else
            {
                while ('0' <= *fmt && *fmt <= '9')
                {
                    fmin *= 10;
                    fmin += (*fmt - '0');
                    fmt++;
                }
            }

            /* C99 7.19.6.1:  If both the '0' and '-' flags appear, the '0'
             * flag is ignored.  */
            if (leftjust)
            {
                pad_char = ' ';
            }
//
//            /*************************************
//             * 3. Optional precision             *
//             *************************************/
//            prec = -1;
//            if (*fmt == '.')
//            {
//                fmt++;
//                if (*fmt == '*')
//                {
//                    prec = va_arg(ap, int);
//                    fmt++;
//                    /* C99 7.19.6.1:  A negative precision argument is taken as
//                     * if the precision were omitted.  */
//                }
//                else
//                {
//                    prec = 0;
//                    while ('0' <= *fmt && *fmt <= '9')
//                    {
//                        prec *= 10;
//                        prec += (*fmt - '0');
//                        fmt++;
//                    }
//                    /* C99 7.19.6.1:  If only the period is specified, the
//                     * precision is taken as zero.  */
//                }
//            }

            /*************************************
             * 4. Optional length modifier       *
             *************************************/
            value_size = NORMAL_INT_LEN;
            if (*fmt == 'l') {
                value_size = LONG_LEN;   //l
                fmt++;
            } else if (*fmt == 'h') {
                fmt++;
                if (*fmt == 'h') {
                    value_size = SHORT_SHORT_LEN; //hh
                    fmt++;
                }else{
                    value_size = SHORT_LEN;   //h
                }
            }


            /*************************************
             * 5. Conversion specifier character *
             *************************************/

            /* Set defaults  */
            base = 0;              /* Not numeric          */
            sign = '\0';           /* No sign              */
            str = string;          /* Use temporary space  */
            //alt_digits = false;    /* Use normal digits    */

            /* Switch on the format specifier character.  */
            switch (*fmt)
            {
//                case 'c':
//                    /* Format a character.  */
//                    /* Note: 'char' is promoted to 'int' when passed as a variadic
//                     * argument.  */
//                    string[0] = (unsigned char)va_arg(ap, int);
//                    string[1] = '\0';
//                    break;
//
                case 's':
                    /* Format a string.  */
                    str = va_arg(ap, char *);
                    if (str == NULL) {
                        str = "(NULL)";
                    }
                    break;

                case 'i':
                case 'd':
                    /* Format a signed integer in base 10  */
                    base = 10;
                    if (value_size == LONG_LEN) {
                        long_arg = va_arg(ap, long);
                    }else{
                        /* Note: 'signed char' and 'short' are promoted to 'int' when passed as variadic arguments.  */
                        long_arg = va_arg(ap, int);
                    }
                    ulong_arg = long_arg;
                    if (long_arg < 0) {
                        sign = '-';
                        ulong_arg = -ulong_arg;
                        /* Note: negating the argument while still in signed form
                         * would produce undefined behavior in the case of the most
                         * negative value.  */
                        /* MKE TODO, find a less hacky way of doing this??? */
                    }
                    break;

                case 'u':
                    /* Format an unsigned integer in base 10  */
                    base = 10;
                    goto handle_unsigned;
//
//                case 'o':
//                    /* Format an unsigned integer in base 8  */
//                    base = 8;
//                    /* Hashflag set on octal display means put 0 at front   */
//                    if (hashtype == 1) { hashtype = 2; };
//                    goto handle_unsigned;
//
//                case 'X':
//                    /* Format an unsigned integer in base 16 (upper case)  */
//                    alt_digits = true;
//                    /* case X drops into case x ... only alt_digits diff */
                case 'x':
                    /* Format an unsigned integer in base 16 (lower case)  */
                    base = 16;
                    /* Hashflag set on hex display means put 0x at front   */
                    if (hashtype == 1){
                        hashtype = 3;
                    }
                    goto handle_unsigned;
//
//                case 'p':
//                    /* Format an unsigned integer in base 16 (lower case)  */
//                    base = 16;
//                    /* Hashflag set on hex display means put 0x at front   */
//                    hashtype = 3;
//                    ularg = (unsigned long) va_arg(ap, void*);
//                    pad_char = '0';
//                    fmin = sizeof(void*);
//                    break;
//
//                case 'b':
//                    /* Format an unsigned integer in base 2  */
//                    base = 2;
//                    goto handle_unsigned;
//
                handle_unsigned:
                    if (value_size == LONG_LEN) {
                        ulong_arg = va_arg(ap, unsigned long);
                    } else {
                        /* Note: 'unsigned char' and 'unsigned short' are promoted
                         * to 'unsigned int' when passed as variadic arguments.  */
                        ulong_arg = va_arg(ap, unsigned int);
                    }
                    break;

                default:
                    /* Unknown format specifier; this also includes the case where
                     * we encounted the end of the format string prematurely.  Write
                     * the '%' literally and continue parsing from the next
                     * character.  */
                    fmt = cur_spec;
                    goto printliteral;
            }

            /* Advance past format specifier character.  */
            fmt++;

            /* If an integer conversion, convert the absolute value of the
             * number to a string in the temporary buffer.  */
            if (base != 0) {
//                /* If hash type octal and it is not zero */
//                if ( hashtype == 2 && ularg != 0 )
//                {
//                    /* add a 0 to front as prefix */
//                    prefix[0] = '0';
//                    prefix_len = 1;
//                }
//
//                /* If hash type hex  */
                if (hashtype == 3)
                {
                    /* add a x0 to front as prefix */
                    prefix[0] = '0';
                    prefix[1] = 'x';
                    prefix_len = 2;
                }

                /* run conversion avoiding prefix that may have been added */
                //ulong_to_string(ularg, &str[0], base, alt_digits);
                //char* itoa (unsigned long value,  char str[],  int radix);
                _itoa (ulong_arg, &str[0], base);
            }

            /* Do length computations.  */
            num_zeroes = 0;
            len_str = strlen(str);

            if(len_str>10){
                //hashtype=hashtype;
            }


//            if (prec >= 0)
//            {
//                /* Precision specified.  */
//                if (base == 0)
//                {
//                    /* String conversions:  Precision specifies *maximum* number
//                     * of string characters to output.  */
//                    if (prec < len_str)
//                    {
//                        len_str = prec;
//                    }
//                }
//                else
//                {
//                    /* Integer conversions:  Precision specifies *minimum*
//                     * number of integer digits to output.  */
//                    if (prec > len_str)
//                    {
//                        num_zeroes = prec - len_str;
//                    }
//                    /* C99 7.19.6.1:  For integer conversions, if a precision is
//                     * specified, the '0' flag is ignored.  */
//                    pad_char = ' ';
//                }
//            }

            /* Calculate length of everything except the field padding.  */
            len_nonpadding = len_str + num_zeroes + (sign != '\0');

            /* Calculate number of padding characters to use.  */
            len_padding = 0;
            if (len_nonpadding < fmin)
            {
                len_padding = fmin - len_nonpadding;
            }

            /* As a shortcut (especially with regards to sign handling), if the
             * output is right-justified with zero padding, treat the padding
             * zeroes in the same way as leading zeroes generated from integer
             * precision specifications.  */
            if (!leftjust && pad_char == '0')
            {
                num_zeroes = len_padding;
                len_padding = 0;
            }

            /* If we have a prefix string output it  */
            for (int i = 0; i < prefix_len; i++)
            {
                ret = putc_func(prefix[i], putc_arg);
                if (ret < 0) {
                    return ret; // return same error as produced by putc
                }
                charcount++;
            }


            /* If right-justified, pad on left.  */
            if (!leftjust)
            {
                for (int i = 0; i < len_padding; i++)
                {
                    ret = putc_func(pad_char, putc_arg);
                    if (ret < 0) {
                        return ret; // return same error as produced by putc
                    }
                    charcount++;
                }
            }

            /* Output sign if needed.  */
            if (sign != '\0'){
                ret = putc_func(sign, putc_arg);
                if (ret < 0) {
                    return ret; // return same error as produced by putc
                }
                charcount++;
            }

            /* Output any zeroes needed because of precision specified in
             * integer conversions.  */
            for (int i = 0; i < num_zeroes; i++)
            {
                //if ((*putc_func) ('0', putc_arg) == EOF)
                ret = putc_func('0', putc_arg);
                if (ret < 0) {
                    return ret; // return same error as produced by putc
                }
                charcount++;
            }

            /* Output any needed characters from str.  */
            for (i = 0; i < len_str; i++)
            {
                ret = putc_func(str[i], putc_arg);
                if (ret < 0) {
                    return ret; // return same error as produced by putc
                }
                charcount++;
            }

//            /* If left-justified, pad on right.  */
//            if (leftjust)
//            {
//                for (i = 0; i < len_padding; i++)
//                {
//                    if ((*putc_func) (pad_char, putc_arg) == EOF)
//                    {
//                        return EOF;
//                    }
//                    chars_written++;
//                }
//            }


        }else{
printliteral:
            /* This is a Literal character, just output char  */
            ret = putc_func(*fmt, putc_arg);
            if (ret < 0) {
                return ret; // return same error as produced by putc
            }
            charcount++;
            fmt++;
        }

    }

    return charcount;
}