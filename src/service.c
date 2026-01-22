/**
 * @file service.c
 * @brief Minimal formatted output services (lightweight printf).
 * @version 1.0.0
 * @date 2026-01-19
 * @author
 *   Donzel
 */

#include "ToRTOS.h"
#include <stdarg.h>

/**
 * @brief Weak putc hook (user may override for UART / SWO / etc.).
 */
__weak void t_putc(char c)
{
    (void)c;
}

/**
 * @brief Very small vsnprintf-like formatter (supports %d %s %c).
 * @param buffer Output buffer.
 * @param size Buffer size (including terminator).
 * @param fmt Format string.
 * @param args Vararg list.
 */
int t_vsnprintf(char *buffer, size_t size, const char *fmt, va_list args)
{
    char *ptr       = buffer;
    const char *end = buffer + size - 1; /* Reserve space for null terminator '\0' */

    while (*fmt && ptr < end)
    {
        if ('%' == *fmt)
        {
            fmt++;
            if ('d' == *fmt)
            {
                int value = va_arg(args, int);
                char temp[12];
                int len = 0;

                if (value < 0)
                {
                    if (ptr < end) *ptr++ = '-';
                    value = -value;
                }
                do
                {
                    temp[len++] = (char)('0' + (value % 10));
                    value /= 10;
                } while (value && len < (int)sizeof(temp));

                while (len-- && ptr < end)
                    *ptr++ = temp[len];
            }
            else if ('s' == *fmt)
            {
                const char *str = va_arg(args, const char *);
                while (*str && ptr < end)
                    *ptr++ = *str++;
            }
            else if ('c' == *fmt)
            {
                char c = (char)va_arg(args, int);
                if (ptr < end) *ptr++ = c;
            }
            else if ('x' == *fmt)
            {
                unsigned int value = va_arg(args, unsigned int);
                char temp[16];
                int len = 0;

                do
                {
                    int digit = value % 16;
                    temp[len++] = (char)(digit < 10 ? ('0' + digit) : ('a' + digit - 10));
                    value /= 16;
                } while (value && len < (int)sizeof(temp));

                while (len-- && ptr < end)
                    *ptr++ = temp[len];
            }
            else if ('f' == *fmt)
            {
                double value = va_arg(args, double);
                if (value < 0)
                {
                    if (ptr < end) *ptr++ = '-';
                    value = -value;
                }

                /* Integer part */
                int int_part = (int)value;
                double frac_part = value - (double)int_part;

                /* Print integer part */
                char temp[32];
                int len = 0;
                do
                {
                    temp[len++] = (char)('0' + (int_part % 10));
                    int_part /= 10;
                } while (int_part && len < (int)sizeof(temp));

                while (len-- && ptr < end)
                    *ptr++ = temp[len];

                /* Decimal point and fractional part */
                if (ptr < end) *ptr++ = '.';

                /* Print 6 decimal places - adjust precision as needed */
                for (int i = 0; i < 6 && ptr < end; i++)
                {
                    frac_part *= 10.0;
                    int digit = (int)frac_part;
                    if (ptr < end)
                        *ptr++ = (char)('0' + digit);
                    frac_part -= digit;
                }
            }
            else
            {
                if (ptr < end) *ptr++ = '%';
                if (ptr < end) *ptr++ = *fmt;
            }
        }
        else
        {
            if (ptr < end) *ptr++ = *fmt;
        }
        fmt++;
    }

    *ptr = '\0';
    return (int)(ptr - buffer);
}
/**
 * @brief Lightweight printf forwarding to t_putc().
 */
void t_printf(const char *fmt, ...)
{
    char buffer[TO_PRINTF_BUF_SIZE];
    va_list args;
    int length;

    va_start(args, fmt);
    length = t_vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    for (int i = 0; i < length; i++)
        t_putc(buffer[i]);
}
