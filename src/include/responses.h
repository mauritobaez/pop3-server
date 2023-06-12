#ifndef RESPONSES_H
#define RESPONSES_H

#define SEPARATOR ".\r\n"
#define CRLF "\r\n"
#define OK_MSG "+OK "
#define ERR_MSG "-ERR "
#define GREETING_MSG OK_MSG "POP3 preparado <pampero.itba.edu.ar>" CRLF
#define NOOP_MSG OK_MSG CRLF
#define USER_OK_MSG OK_MSG "ahora pone la PASS :)" CRLF
#define USER_ERR_MSG ERR_MSG "quien sos? >:(" CRLF
#define RETR_OK_MSG OK_MSG "message follows" CRLF
#define RETR_OK_MSG_LENGTH (strlen(RETR_OK_MSG) + 1)
#define RETR_ERR_MISS_MSG ERR_MSG "missing message argument" CRLF
#define RETR_ERR_FOUND_MSG ERR_MSG "no such message was found" CRLF
#define PASS_OK_MSG OK_MSG "listo el pollo :)" CRLF
#define PASS_ERR_LOCK_MSG ERR_MSG "el email esta lockeado :(" CRLF
#define PASS_ERR_MSG ERR_MSG "INCORRECTO >:(" CRLF

#define INVALID_MSG ERR_MSG "invalid command" CRLF
#define INVALID_NUMBER_ARGUMENT ERR_MSG "argumento invalido debe ser un numero entero mayor a uno" CRLF
#define NON_EXISTANT_EMAIL_MSG ERR_MSG "no existe un mail en esa posicion" CRLF
#define OCTETS_FORMAT_MSG OK_MSG "%d %ld octets" CRLF
#define LISTING_SIZE_ERR_MSG ERR_MSG "tamaño de email maximo excedido" CRLF
#define OK_LIST_RESPONSE OK_MSG "%ld messages (%ld octets)" CRLF
#define LISTING_RESPONSE_FORMAT "%ld %ld" CRLF
#define STAT_OK_MSG OK_MSG "%ld %ld" CRLF
#define STAT_OK_MSG_LENGTH 64
#define QUIT_MSG OK_MSG "closing" CRLF
#define QUIT_AUTHENTICATED_MSG OK_MSG "See you next time" CRLF
#define RSET_MSG OK_MSG "maildrop has been resetted" CRLF

#endif