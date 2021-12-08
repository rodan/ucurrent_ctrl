#ifndef __SYS_MESSAGEBUS_H__
#define __SYS_MESSAGEBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "proj.h"

/*!
	\brief List of possible message types for the message bus.
	\sa sys_messagebus_register()
*/

// should be re-created in local proj.h

//#define           SYS_MSG_NULL 0
//#define    SYS_MSG_TIMER0_CRR1 0x1   // timer_a0_delay_noblk_ccr1
//#define    SYS_MSG_TIMER0_CRR2 0x2   // timer_a0_delay_noblk_ccr2
//#define     SYS_MSG_TIMER0_IFG 0x4   // timer0 overflow
//#define       SYS_MSG_UART0_RX 0x8   // UART received something

/*!
	\brief Linked list of nodes listening to the message bus.
*/
struct sys_messagebus {
    /*! callback for receiving messages from the system bus */
    void (*fn) (const uint16_t sys_message);
    /*! bitfield of message types that the node wishes to receive */
    uint16_t listens;
    /*! pointer to the next node in the list */
    struct sys_messagebus *next;
};

/*!
	\brief Registers a node in the message bus.
	\details Registers (add) a node to the message bus. A node can filter what message(s) are to be received by setting the bitfield \b listens.
	\sa sys_message, sys_messagebus, sys_messagebus_unregister
*/
void sys_messagebus_register(
        // callback to receive messages from the message bus
        void (*callback) (const uint16_t sys_message),
        // only receive messages of this type
        const uint16_t listens
    );

/*!
	\brief Unregisters a node from the message bus.
	\sa sys_messagebus_register
*/
void sys_messagebus_unregister(
        // the same callback used on sys_messagebus_register()
        void (*callback) (const uint16_t sys_message)
    );

struct sys_messagebus * sys_messagebus_getp(void);

#ifdef __cplusplus
}
#endif

#endif
