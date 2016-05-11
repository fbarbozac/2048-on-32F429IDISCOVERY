#include <stddef.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

#define WIDGET_USE_FLEX_SKIN	(1)
#define WM_SUPPORT_TRANSPARENCY	(0)

#include "GUI.h"
#include "FRAMEWIN.h"
#include "SPINBOX.h"

#include "cmsis_os.h"                               // CMSIS RTOS header file
#include "Gyro.h"

/*
**********************************************************************
*
*       Defines
*
**********************************************************************
*/
#define FB_XSIZE			(240)
#define FB_YSIZE			(320)

#define	GAP_X_SIZE			(5)		// GAP between the tiles
#define	GAP_Y_SIZE			(GAP_X_SIZE)

#define	GAME_NAME_X_START	(GAP_X_SIZE)
#define	GAME_NAME_Y_START	(GAME_NAME_X_START)

#define	GAME_NAME_X_SIZE	((FB_XSIZE - (3 * GAP_X_SIZE)) / 2)
#define	GAME_NAME_Y_SIZE	(30)

#define	MENU_X_START		(GAME_NAME_X_START + GAME_NAME_X_SIZE + GAP_X_SIZE)
#define	MENU_Y_START		(GAME_NAME_Y_START)

#define	MENU_X_SIZE			(GAME_NAME_X_SIZE)
#define	MENU_Y_SIZE			(GAME_NAME_Y_SIZE)

#define	TILE_X_NUM			(3)		// Number of pieces on table
#define	TILE_Y_NUM			(TILE_X_NUM)

#define	TABLE_X_START		(GAP_X_SIZE)
#define	TABLE_Y_START		((FB_YSIZE - TABLE_Y_SIZE) / 2)

#define	TILE_X_PIXEL_SIZE	(((FB_XSIZE - TABLE_X_START) - ((GAP_X_SIZE * (TILE_X_NUM + 1)) + TABLE_X_START)) / TILE_X_NUM)	// Size of each tile
#define	TILE_Y_PIXEL_SIZE	(TILE_X_PIXEL_SIZE)

#define	TABLE_X_SIZE		(((TILE_X_NUM + 1) * GAP_X_SIZE) + (TILE_X_NUM * TILE_X_PIXEL_SIZE))
#define	TABLE_Y_SIZE		(((TILE_Y_NUM + 1) * GAP_Y_SIZE) + (TILE_Y_NUM * TILE_Y_PIXEL_SIZE))

#define POS_TILE_X(x)		(GAP_X_SIZE + (x * GAP_X_SIZE) + (x * TILE_X_PIXEL_SIZE))	// Position of each piece inside table
#define POS_TILE_Y(y)		(GAP_Y_SIZE + (y * GAP_X_SIZE) + (y * TILE_X_PIXEL_SIZE))

#define	FOOTER_X_START		(GAP_X_SIZE)
#define	FOOTER_Y_START		(FB_YSIZE - FOOTER_Y_SIZE - GAP_X_SIZE)

#define	FOOTER_X_SIZE		(TABLE_X_SIZE)
#define	FOOTER_Y_SIZE		((2 * GAP_X_SIZE))

#define	TIME_BLINK_ON_MERGE	(300)	// Time of tile blink when it is merged in ms. 0 to disable

//RGB
#define	RGB_TO_HEX(r,g,b)	((r << 0) + (g << 8) + (b << 16))

// Table color
#define	TABLE_WIN_COLOR		GUI_DARKGREEN
#define	TILE_WIN_COLOR		GUI_LIGHTYELLOW
#define	TILE0_WIN_COLOR		GUI_LIGHTGRAY
#define	TILE_NUM_WIN_COLOR	GUI_BLUE
#define	GAME_NAME_WIN_COLOR	GUI_GREEN
#define	NEWGAME_WIN_COLOR	GUI_DARKGREEN
#define	MOVE_WIN_COLOR		TABLE_WIN_COLOR
#define	BEST_WIN_COLOR		TABLE_WIN_COLOR
#define	FOOTER_WIN_COLOR	TABLE_WIN_COLOR
#define	PROGBAR0_WIN_COLOR	GUI_LIGHTBLUE
#define	PROGBAR1_WIN_COLOR	GUI_LIGHTRED

/*
**********************************************************************
*
*       Types
*
**********************************************************************
*/
typedef struct
{
	WM_HWIN hWin;
} t_st_Tile;

typedef struct
{
	U8 u8_x, u8_y;
} t_st_TilePos;

typedef struct
{
	WM_HWIN WM_HWIN_Table;
	t_st_Tile st_Tiles[TILE_Y_NUM][TILE_X_NUM];
} t_st_Table;

/*
********************************************************************
*
*       static code
*
********************************************************************
*/
static t_st_Table st_Table;

static osThreadId id_GUIThread;

/*
********************************************************************
*
* Prototypes
*
********************************************************************
*/
static void cbTilesWin(WM_MESSAGE *pMsg);
static void cbFTableWin(WM_MESSAGE * pMsg);
static void cbFooterWin(WM_MESSAGE *pMsg);
static void cbNewGameWin(WM_MESSAGE *pMsg);
static void cbGameNameWin(WM_MESSAGE *pMsg);

/*
********************************************************************
*
* Routine to blink the tile
*
********************************************************************
*/
static void blinkTileWin(WM_HWIN hWin)
{
#if	(TIME_BLINK_ON_MERGE > 0)
	WM_ResizeWindow(hWin, GAP_X_SIZE, GAP_Y_SIZE);
	WM_MoveWindow(hWin, -(GAP_X_SIZE / 2), -(GAP_Y_SIZE / 2));
	WM_CreateTimer(hWin, 0, TIME_BLINK_ON_MERGE, 0);
#endif
}

/*
********************************************************************
*
* Callback routine for foreground window
*
********************************************************************
*/
static void cbTilesWin(WM_MESSAGE *pMsg)
{
	static uint8_t u8_Moved;

	switch (pMsg->MsgId)
	{
	case WM_TIMER:
		if (WM_GetTimerId(pMsg->Data.v) == 1)
		{
			u8_Moved = 0;
		}
		#if	(TIME_BLINK_ON_MERGE > 0)
		else
		{
			WM_MoveWindow(pMsg->hWin, (GAP_X_SIZE / 2), (GAP_Y_SIZE / 2));
			WM_ResizeWindow(pMsg->hWin, -(GAP_X_SIZE), -(GAP_Y_SIZE));
		}
		#endif
		break;

	case WM_PAINT:
		GUI_SetBkColor(TILE_WIN_COLOR);
		GUI_Clear();
		break;

	case WM_TOUCH:
		if (u8_Moved == 0)
		{
			u8_Moved = 1;

			WM_CreateTimer(pMsg->hWin, 1, 1000, 0);

			blinkTileWin(pMsg->hWin);
		}
		break;

	case WM_NOTIFY_PARENT:
		break;

	default:
		WM_DefaultProc(pMsg);
		break;
	}
}

/*******************************************************************
*
* Callback routine for table window
*
********************************************************************
*/
static void cbFTableWin(WM_MESSAGE * pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_CREATE:
	{
		t_st_TilePos st_TilePos;

		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
			{
				// Create tiles window
				st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin =
						WM_CreateWindowAsChild(POS_TILE_X(st_TilePos.u8_x), POS_TILE_Y(st_TilePos.u8_y),
												TILE_X_PIXEL_SIZE, TILE_Y_PIXEL_SIZE,
													pMsg->hWin, WM_CF_SHOW, cbTilesWin, 0);
			}
		}
	}
		break;

	case WM_PAINT:
		GUI_SetBkColor(TABLE_WIN_COLOR);
		GUI_Clear();
		break;

	default:
		WM_DefaultProc(pMsg);
		break;
	}
}

/*
********************************************************************
*
* Callback routine for Game Name window
*
********************************************************************
*/
static void cbFooterWin(WM_MESSAGE *pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		GUI_SetBkColor(FOOTER_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_8_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("@fbarbozac", (FOOTER_X_SIZE / 2), (FOOTER_Y_SIZE / 2));
		break;

	case WM_TOUCH:
		break;

	case WM_NOTIFY_PARENT:
		break;

	default:
		WM_DefaultProc(pMsg);
		break;
	}
}

/*
********************************************************************
*
* Callback routine for NewGame Button window
*
********************************************************************
*/
static void cbNewGameWin(WM_MESSAGE *pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		GUI_SetBkColor(NEWGAME_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_24B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("Menu", (MENU_X_SIZE / 2), (MENU_Y_SIZE / 2));
		break;

	case WM_TOUCH:
		break;

	case WM_NOTIFY_PARENT:
		break;

	default:
		WM_DefaultProc(pMsg);
		break;
	}
}

/*
********************************************************************
*
* Callback routine for Game Name window
*
********************************************************************
*/
static void cbGameNameWin(WM_MESSAGE *pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		GUI_SetBkColor(GAME_NAME_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_24B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("Gyro", (GAME_NAME_X_SIZE / 2), (GAME_NAME_Y_SIZE / 2));
		break;

	case WM_TOUCH:
		break;

	case WM_NOTIFY_PARENT:
		break;

	default:
		WM_DefaultProc(pMsg);
		break;
	}
}

static void GUIThread (void const *argument);		// thread function
osThreadDef(GUIThread, osPriorityNormal, 1, 2048);	// thread object

/*********************************************************************
*
*       MainTask
*/
int Init_GUIThread (void)
{
	id_GUIThread = osThreadCreate(osThread(GUIThread), NULL);
	
	return ((id_GUIThread == NULL) ? -1 : 0);
}

osThreadId GetGUIThreadId (void)
{
	return id_GUIThread;
}

/*********************************************************************
*
*       Start code
*
**********************************************************************
*/
static void GUIThread (void const *argument)
{
	GUI_Init();

	WM_SetDesktopColor(GUI_WHITE);

	// Create the Game Name
	WM_CreateWindow(GAME_NAME_X_START, GAME_NAME_Y_START, GAME_NAME_X_SIZE, GAME_NAME_Y_SIZE, WM_CF_SHOW, cbGameNameWin, 0);

	// Create the Menu Button
	WM_CreateWindow(MENU_X_START, MENU_Y_START, MENU_X_SIZE, MENU_Y_SIZE, WM_CF_SHOW, cbNewGameWin, 0);

	// Create the table window
	st_Table.WM_HWIN_Table = WM_CreateWindow(TABLE_X_START, TABLE_Y_START, TABLE_X_SIZE, TABLE_Y_SIZE, WM_CF_SHOW, cbFTableWin, 0);

	// Create the footer
	WM_CreateWindow(FOOTER_X_START, FOOTER_Y_START, FOOTER_X_SIZE, FOOTER_Y_SIZE, WM_CF_SHOW, cbFooterWin, 0);

	while (1)
	{
		osEvent evt = osSignalWait (0, 100);
		
		if (evt.status == osEventSignal)
		{
			// handle event status
			switch (evt.value.signals)
			{
				case GYRO_TILT_X_P:
					blinkTileWin(st_Table.st_Tiles[TILE_Y_NUM - 1][TILE_X_NUM >> 1].hWin);
					break;
				
				case GYRO_TILT_X_M:
					blinkTileWin(st_Table.st_Tiles[0][TILE_X_NUM >> 1].hWin);
					break;
				
				case GYRO_TILT_Y_P:
					blinkTileWin(st_Table.st_Tiles[TILE_Y_NUM >> 1][TILE_X_NUM - 1].hWin);
					break;
				
				case GYRO_TILT_Y_M:
					blinkTileWin(st_Table.st_Tiles[TILE_Y_NUM >> 1][0].hWin);
					break;
				
				case GYRO_TILT_Z_P:
					blinkTileWin(st_Table.st_Tiles[0][TILE_X_NUM - 1].hWin);
					break;
				
				case GYRO_TILT_Z_M:
					blinkTileWin(st_Table.st_Tiles[TILE_Y_NUM - 1][0].hWin);
					break;
				
				case GYRO_TILT_NULL:
					break;
				
			}
		}
		
		/* All GUI related activities might only be called from here */
		GUI_TOUCH_Exec();     /* Optional Touch support, uncomment if required */
		GUI_Exec();             /* Execute all GUI jobs ... Return 0 if nothing was done. */
		GUI_X_ExecIdle();       /* Nothing left to do for the moment ... Idle processing */
	}
}

