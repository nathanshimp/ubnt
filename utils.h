/**
 * @file utils.h
 * @author Nathan Shimp
 * @brief Utility and helper functions
 */
#ifndef UTILS_H
#define UTILS_H
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void rstrip(char *str);
char *strip(char *str);
char *convert_port_to_string(int port);

#endif
