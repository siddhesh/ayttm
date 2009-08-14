
#ifndef _MSN_MESSAGE_H_
#define _MSN_MESSAGE_H_

#include "msn.h"
#include "msn-errors.h"

#define MSN_MSG_NO_ACK		"U"
#define MSN_MSG_NACK_FAIL	"N"
#define MSN_MSG_ACK_ALL		"A"

typedef enum {
	MSN_COMMAND_INVALID = -1,
	MSN_COMMAND_VER = 0,
	MSN_COMMAND_CVR = 1,
	MSN_COMMAND_USR = 2,
	MSN_COMMAND_XFR = 3,

	MSN_COMMAND_ILN = 4,
	MSN_COMMAND_BLP = 5,
	MSN_COMMAND_MSG = 6,
	       
	MSN_COMMAND_ADL = 7,
	MSN_COMMAND_ADG = 8,
	MSN_COMMAND_CHG = 9,
	MSN_COMMAND_FQY = 10,
	MSN_COMMAND_GCF = 11,
	MSN_COMMAND_OUT = 12,
	MSN_COMMAND_PNG = 13,
	MSN_COMMAND_QNG = 14,
	MSN_COMMAND_QRY = 15,
	MSN_COMMAND_SBS = 16,
	MSN_COMMAND_REA = 17,
	MSN_COMMAND_RML = 18,
	MSN_COMMAND_RMG = 19,
	MSN_COMMAND_UBX = 20,
	MSN_COMMAND_SDC = 21,
	MSN_COMMAND_IMS = 22,
	       
	MSN_COMMAND_CHL = 23,
	MSN_COMMAND_FLN = 24,
	MSN_COMMAND_NLN = 25,
	MSN_COMMAND_RNG = 26,
	MSN_COMMAND_NOT = 27,
	       
	MSN_COMMAND_ANS = 28,
	MSN_COMMAND_IRO = 29,
	MSN_COMMAND_CAL = 30,
	MSN_COMMAND_JOI = 31,
	MSN_COMMAND_BYE = 32,
	MSN_COMMAND_PRP = 33,
	MSN_COMMAND_COUNT
} MsnCommand ;

struct _MsnMessage {
	int argc;				/* Number of arguments in the current command */
	char **argv;				/* Arguments in the current command */
	MsnCommand command;			/* Message ID, easier to validate with it */
	int size;				/* Size of the payload */
	char *payload;				/* Payload Data */
	int payload_offset;			/* Current size of the payload */
	int state;				/* State of the message. 0 when complete. */
	void *payload_info;			/* Payload parsed into message-specific information.
							For now it only houses objects of MsnMessagePayload */
	int capacity;				/* Capacity of the payload */
};

/* Returns number of characters remaining after constructing a message from the string.
 * Look for msg->state to see if the message is complete. message is complete
 * if msg->state is 0 */
int msn_message_concat(MsnMessage *msg, char *data, int len) ;

MsnMessage *msn_message_new();
void msn_message_free(MsnMessage *msg);

/* Send a command with the given arguments */
void msn_message_send(MsnConnection *mc, const char *payload, MsnCommand cmd, ...);

/* Converts name-value parameters to a string. The variable
 * list must be terminated with a -1 */
void msn_message_parm_to_string(char *buf, int size, ...);

#define msn_message_validate_error(msg) { \
	( msg->command < MSN_COMMAND_COUNT ) \
}

#define MAX_PAYLOAD_SIZE 1664
typedef void (*MsnCommandPayloadHandler) (MsnMessage *msg);
typedef void (*MsnCommandHandler) (MsnConnection *mc);


const char *msn_command_get_name(MsnCommand cmd);
int msn_command_get_num_args(MsnCommand cmd);
MsnCommandPayloadHandler msn_command_get_payload_handler(MsnMessage *msg);

MsnCommand msn_command_get_from_string(char *cmd);

int msn_command_handle(MsnConnection *mc);

int msn_command_set_payload_size(MsnMessage *msg);

void msn_command_build_payload (MsnMessage *msg);

void msn_command_parse_payload (MsnMessage *msg);

void msn_message_handle_incoming(MsnConnection *mc);

void msn_send_IM_to_sb(MsnConnection *sb, MsnIM *im);

void msn_send_IM(MsnAccount *ma, MsnBuddy *buddy);

int msn_message_is_error(MsnConnection *mc);

#endif

