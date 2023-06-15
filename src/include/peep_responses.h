#ifndef RESPONSES_H
#define RESPONSES_H

#define CRLF "\r\n"
#define NEG_RESP "-%d" CRLF
#define OK "+" CRLF
#define OK_INT "+%ld" CRLF
#define OK_STRING "+%s" CRLF
#define LINE_RESP "%s" CRLF
#define SHORT_RESP_LENGTH 24
#define MAX_LINE_LENGTH 512

#endif