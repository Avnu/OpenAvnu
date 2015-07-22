/* inih -- simple .INI file parser

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

http://code.google.com/p/inih/

*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "ini.h"

#if !INI_USE_STACK
#include <stdlib.h>
#endif

#define MAX_SECTION 50
#define MAX_NAME 50

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace(*s))
        s++;
    return (char*)s;
}

/* Return pointer to first char c or ';' comment in given string, or pointer to
   null at end of string if neither found. ';' must be prefixed by a whitespace
   character to register as a comment. */
static char* find_char_or_comment(const char* s, char c)
{
    int was_whitespace = 0;
    while (*s && *s != c && !(was_whitespace && *s == ';')) {
        was_whitespace = isspace(*s);
        s++;
    }
    return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

/* See documentation in header file. */
int ini_parse_file(FILE* file,
                   int (*handler)(void*, const char*, const char*,
                                  const char*),
                   void* user)
{
    /* Uses a fair bit of stack (use heap instead if you need to) */
#if INI_USE_STACK
    char line[INI_MAX_LINE];
#else
    char* line;
#endif
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

#if !INI_USE_STACK
    line = (char*)malloc(INI_MAX_LINE);
    if (!line) {
        return -2;
    }
#endif

    /* Scan through file line by line */
    while (fgets(line, INI_MAX_LINE, file) != NULL) {
        lineno++;

        start = line;
#if INI_ALLOW_BOM
        if (lineno == 1 && (unsigned char)start[0] == 0xEF &&
                           (unsigned char)start[1] == 0xBB &&
                           (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* Per Python ConfigParser, allow '#' comments at start of line */
        }
#if INI_ALLOW_MULTILINE
        else if (*prev_name && *start && start > line) {
            /* Non-black line with leading whitespace, treat as continuation
               of previous name's value (as per Python ConfigParser). */
            if (!handler(user, section, prev_name, start) && !error)
                error = lineno;
        }
#endif
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                /* No ']' found on section line */
                error = lineno;
            }
        }
        else if (*start && *start != ';') {
            /* Not a comment, must be a name[=:]value pair */
            end = find_char_or_comment(start, '=');
            if (*end != '=') {
                end = find_char_or_comment(start, ':');
            }
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
                rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value) && !error)
                    error = lineno;
            }
            else if (!error) {
                /* No '=' or ':' found on name[=:]value line */
                error = lineno;
            }
        }
    }

#if !INI_USE_STACK
    free(line);
#endif

    return error;
}

#define NULL_CHAR  '\0'
#define COMMA ','

int ini_parse_override(char *override,
                   int (*handler)(void*, const char*, const char*,
                                  const char*),
                   void* user)
{
#if INI_USE_STACK
    char line[INI_MAX_LINE];
#else
    char* line;
#endif
    char section[MAX_SECTION] = "";
    char *name, *value;

	char *src = override;
    int error = -1;
	int i, j;
	
#if !INI_USE_STACK
    line = (char*)malloc(INI_MAX_LINE);
    if (!line) {
        return -2;
    }
#endif

	while (*src != NULL_CHAR)
	{
		if (*src == COMMA) {
			src++;
		}
		else if (*src == '[') {
			// section
			src++; // skip opening delim
			for (i = 0; i < MAX_SECTION; i++) {
				section[i] = *src++;
				if (section[i] == NULL_CHAR) {
					// error, section not completed
					goto out;
				}
				else if (section[i] == ']') {
					section[i] = NULL_CHAR;
					break;
				}
			}
			if (i > MAX_SECTION)
				goto out; // error, overflowed buffer
		}
		else {
			// name/value
			name = &line[0];
			for (i = 0; i < MAX_NAME; i++) {
				line[i] = *src++;
				if (line[i] == NULL_CHAR) {
					// error, name not completed
					goto out; 
				}
				else if (line[i] == COMMA) {
					// error, no value
					goto out; 
				}
				else if (line[i] == '=') {
					line[i] = NULL_CHAR;
					break;
				}
			}
			if (i > MAX_NAME)
				goto out; // error, overflowed buffer

			value = &line[i + 1];
			for (j = i + 1; j < INI_MAX_LINE; j++) {
				line[j] = *src++;
				if (line[j] == ',') {
					line[j] = NULL_CHAR;
					break;
				}
				if (line[j] == NULL_CHAR) {
					src--;  // oops!
					break;
				}
				
			}
			if (j > INI_MAX_LINE)
				goto out; // error, overflowed buffer

			if (!handler(user, section, name, value) && !error)
				error = -3;
		}
	}

	// all parsed!
	error = 0;

  out:		
#if !INI_USE_STACK
    free(line);
#endif
	return error;
}

/* See documentation in header file. */
int ini_parse(const char* filename,
              int (*handler)(void*, const char*, const char*, const char*),
              void* user)
{
    FILE* file;
    int error;
	char *pvtFilename = strdup(filename);
	if (!pvtFilename) {
		return -1;
	}

	char *override = strchr(pvtFilename, COMMA);
	if (override)
		*override++ = '\0';

    file = fopen(pvtFilename, "r");
    if (!file) {
		free (pvtFilename);
        return -1;
	}
    error = ini_parse_file(file, handler, user);

	if (error == 0 && override)
		error = ini_parse_override(override, handler, user);
    fclose(file);
	free (pvtFilename);

	return error;
}
