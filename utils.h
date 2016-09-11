/**
 * @file
 * @author Nathan Shimp
 * @brief Utility and helper functions
 */
#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/**
 * Remove all whitespace from the right of a string
 *
 * @param str String to strip
 */
void rstrip(char *str)
{
    if (str)
    {
        int i = strlen(str);

        if (i > 0)
        {
            while (isspace(str[i - 1]))
                i--;

            str[i] = '\0';
        }
    }
}

/**
 * Remove all occurances of \\n, \\t and \\r from a string
 * and return the stripped string
 * @param str String to strip
 * @returns Stripped String
 */
char *strip(char *str)
{
    int length, j;
    char *stripped_str = NULL;

    if (str)
    {
        length = strlen(str) + 100;
        stripped_str = malloc(length);

        j = 0;
        for (int i = 0; i < length; i++)
            if (str[i] != '\n' && str[i] != '\t' && str[i] != '\r')
                stripped_str[j++] = str[i];

        stripped_str[j] = '\0';
    }

    return stripped_str;
}

char *convert_port_to_string(int port)
{
    char *port_str = malloc(6);
    int rc = snprintf(port_str, 6, "%d", port);
    if (rc < 0)
        strcpy(port_str, "22");

    return port_str;
}

#endif
