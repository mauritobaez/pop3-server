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
#define PEEP_HELP OK "19" CRLF "a" CRLF "q" CRLF "u+" CRLF "u-" CRLF "u?" CRLF "c=3-1000" CRLF "c?" CRLF "m=" CRLF "m?" CRLF "t=0-86400" CRLF "t?" CRLF "rb?" CRLF "re?" CRLF "xe?" CRLF "cc=" CRLF "cu?" CRLF "hc?" CRLF "hu?" CRLF "h?" CRLF

#endif