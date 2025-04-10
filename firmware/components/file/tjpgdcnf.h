/*----------------------------------------------*/
/* TJpgDec System Configurations R0.03          */
/*----------------------------------------------*/
#include "esp_system.h"

#define	JD_SZBUF		512
//#define	JD_SZBUF		1024
/* Specifies size of stream input buffer */

#ifdef CONFIG_BUILD_ICAM_MINI
#define JD_FORMAT		0
#else
#define JD_FORMAT		1
#endif
/* Specifies output pixel format.
/  0: RGB888 (24-bit/pix)
/  1: RGB565 (16-bit/pix)
/  2: Grayscale (8-bit/pix)
*/

#define JD_SWAP_RGB565  1
/* Swap the 2 bytes of RGB565 color.
/  0: Don't swap
/  1: Swap
*/ 

#define	JD_USE_SCALE	0
/* Switches output descaling feature.
/  0: Disable
/  1: Enable
*/

#define JD_TBLCLIP		1
/* Use table conversion for saturation arithmetic. A bit faster, but increases 1 KB of code size.
/  0: Disable
/  1: Enable
*/

#define JD_FASTDECODE	1
/* Optimization level
/  0: Basic optimization. Suitable for 8/16-bit MCUs.
/  1: + 32-bit barrel shifter. Suitable for 32-bit MCUs.
/  2: + Table conversion for huffman decoding (wants 6 << HUFF_BIT bytes of RAM)
*/

