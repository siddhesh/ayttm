/*****************************************************************************/
/*** zephyr_mod.h ************************************************************/
/*****************************************************************************/

/*

Interface for Zephyr functionality

*/

/*** Changelog ***************************************************************/

/*

* Sat Jan 06 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- Code cleanups

* Wed Jan 03 2001 Sourav K. Mandal <Sourav.Mandal@ikaran.com>
- First incarnation
 
*/

/*****************************************************************************/
/*** Normal header stuff *****************************************************/
/*****************************************************************************/

#ifndef __ZEPHYR_H__
#define __ZEPHYR_H__
 
#include "service.h"

struct service_callbacks *eb_zephyr_query_callbacks(void) ;

#endif /*__ZEPHYR_H__*/
