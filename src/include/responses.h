#ifndef RESPONSES_H
#define RESPONSES_H

#define SEPARATOR ".\r\n"
#define CRLF "\r\n"
#define OK_MSG "+OK "
#define ERR_MSG "-ERR "
#define GREETING_MSG OK_MSG "POP3 preparado <pampero.itba.edu.ar>" CRLF
#define NOOP_MSG OK_MSG CRLF
#define CAPA_MSG OK_MSG CRLF "USER" CRLF "PIPELINING" CRLF "." CRLF
#define USER_OK_MSG OK_MSG "ahora pone la PASS :)"CRLF
#define USER_ERR_MSG ERR_MSG "quien sos? >:("CRLF
#define RETR_OK_MSG OK_MSG "message follows"CRLF
#define RETR_OK_MSG_LENGTH (strlen(RETR_OK_MSG)+1)
#define RETR_ERR_MISS_MSG ERR_MSG "missing message argument"CRLF
#define RETR_ERR_FOUND_MSG ERR_MSG "no such message was found"CRLF
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
#define QUIT_AUTHENTICATED_MSG OK_MSG "See you next time (%d messages left)" CRLF
#define QUIT_AUTHENTICATED_MSG_LENGTH (strlen(QUIT_AUTHENTICATED_MSG)+1)
#define QUIT_UNAUTHENTICATED_MSG_ERROR ERR_MSG "There was an error deleting some emails" CRLF
#define QUIT_UNAUTHENTICATED_MSG_ERROR_LENGTH (strlen(QUIT_UNAUTHENTICATED_MSG_ERROR) +1 )
#define MAX_DIGITS_INT 11

#define RSET_MSG OK_MSG "maildrop has been resetted" CRLF
#define DELETED_ALREADY_MSG ERR_MSG "message %d ya fue borrado" CRLF
#define DELETED_MSG OK_MSG "message %d borrado" CRLF
#define FINAL_MESSAGE_RETR CRLF"."CRLF
#define FINAL_MESSAGE_RETR_LENGTH 6

#endif