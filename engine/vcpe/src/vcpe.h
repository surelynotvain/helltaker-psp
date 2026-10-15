/* Vain's C Portable Engine (VCPE): the shared PSP base of the VCPE demakes.
 * Games include this header; each module documents itself in its own header. Headers only declare: a game links
 * the modules it uses (VCPE_MODULES in its Makefile). */
#ifndef VCPE_H
#define VCPE_H
#define VCPE_VERSION_MAJOR 3
#define VCPE_VERSION_MINOR 4
#define VCPE_VERSION "3.4"
#include "vcpe_sys.h"
#include "vcpe_pak.h"
#include "vcpe_audio.h"
#include "vcpe_test.h"
#include "vcpe_gfx.h"
#include "vcpe_3d.h"
#include "vcpe_node3d.h"
#include "vcpe_co.h"
#include "vcpe_font.h"
#include "vcpe_post.h"
#include "vcpe_lua.h"
#include "vcpe_math.h"
#include "vcpe_pad.h"
#include "vcpe_save.h"
#include "vcpe_log.h"
#include "vcpe_hud.h"
#include "vcpe_ttf.h"
#include "vcpe_mouse.h"
#include "vcpe_mem.h"
#include "vcpe_load.h"
#endif
