#include <stddef.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

#define WIDGET_USE_FLEX_SKIN	(1)
#define WM_SUPPORT_TRANSPARENCY (0)

#include "GUI.h"
#include "FRAMEWIN.h"
#include "PROGBAR.h"

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

#define	GAME_NAME_X_SIZE	((FB_XSIZE - (4 * GAP_X_SIZE)) / 3)
#define	GAME_NAME_Y_SIZE	(20)

#define	NEWGAME_X_START		(GAME_NAME_X_START)
#define	NEWGAME_Y_START		(GAME_NAME_Y_START + GAME_NAME_Y_SIZE + GAP_Y_SIZE)

#define	NEWGAME_X_SIZE		(GAME_NAME_X_SIZE)
#define	NEWGAME_Y_SIZE		(GAME_NAME_Y_SIZE)

#define	PROGBAR_X_START		(NEWGAME_X_START)
#define	PROGBAR_Y_START		(NEWGAME_Y_START + NEWGAME_Y_SIZE + GAP_Y_SIZE)

#define	PROGBAR_X_SIZE		(FB_XSIZE - NEWGAME_X_START - GAP_X_SIZE)
#define	PROGBAR_Y_SIZE		((2 * GAP_X_SIZE))

#define	MOVE_X_START		((GAME_NAME_X_START + GAME_NAME_X_SIZE) + GAP_X_SIZE)
#define	MOVE_Y_START		(GAME_NAME_Y_START)

#define	MOVE_X_SIZE			(GAME_NAME_X_SIZE)
#define	MOVE_Y_SIZE			(GAME_NAME_Y_SIZE + NEWGAME_Y_SIZE + GAP_Y_SIZE)

#define	BEST_X_START		((MOVE_X_START + MOVE_X_SIZE) + GAP_X_SIZE)
#define	BEST_Y_START		(GAME_NAME_Y_START)

#define	BEST_X_SIZE			(GAME_NAME_X_SIZE)
#define	BEST_Y_SIZE			(MOVE_Y_SIZE)

#define	TILE_X_NUM			(4)		// Number of pieces on table
#define	TILE_Y_NUM			(TILE_X_NUM)

#define	TABLE_X_START		(GAP_X_SIZE)
#define	TABLE_Y_START		(PROGBAR_Y_START + PROGBAR_Y_SIZE + GAP_Y_SIZE)

#define	TILE_X_PIXEL_SIZE	(((FB_XSIZE - TABLE_X_START) - ((GAP_X_SIZE * (TILE_X_NUM + 1)) + TABLE_X_START)) / TILE_X_NUM)	// Size of each tile
#define	TILE_Y_PIXEL_SIZE	(TILE_X_PIXEL_SIZE)

#define	TABLE_X_SIZE		(((TILE_X_NUM + 1) * GAP_X_SIZE) + (TILE_X_NUM * TILE_X_PIXEL_SIZE))
#define	TABLE_Y_SIZE		(((TILE_Y_NUM + 1) * GAP_Y_SIZE) + (TILE_Y_NUM * TILE_Y_PIXEL_SIZE))

#define POS_TILE_X(x)		(GAP_X_SIZE + (x * GAP_X_SIZE) + (x * TILE_X_PIXEL_SIZE))	// Position of each piece inside table
#define POS_TILE_Y(y)		(GAP_Y_SIZE + (y * GAP_X_SIZE) + (y * TILE_X_PIXEL_SIZE))

#define	FOOTER_X_START		(GAP_X_SIZE)
#define	FOOTER_Y_START		(TABLE_Y_START + TABLE_Y_SIZE + GAP_Y_SIZE)

#define	FOOTER_X_SIZE		(FB_XSIZE - TABLE_X_START - GAP_X_SIZE)
#define	FOOTER_Y_SIZE		((2 * GAP_X_SIZE))

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
typedef enum
{
	FALSE = 0,
	TRUE,
} bool;

typedef enum
{
	EN_MOVE_MIN = 0,

	EN_MOVE_UP = EN_MOVE_MIN,
	EN_MOVE_DOWN,
	EN_MOVE_LEFT,
	EN_MOVE_RIGHT,

	EN_MOVE_MAX,
} t_en_Direction;

typedef struct
{
	WM_HWIN hWin;
	U8 u8_Value;
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
static U32 u32_Best, u32_Move;
static PROGBAR_Handle hProgBar;
static WM_HWIN WM_HWIN_Score;
static WM_HWIN WM_HWIN_Best;
static t_st_Table st_Table;

static osThreadId id_GUIThread;

/*
********************************************************************
*
* Prototypes
*
********************************************************************
*/
static U16 getU32Size(U32 value);
static void startTheGame(void);
static void addMove(U32 u32_Value);

static bool getTilePos(WM_HWIN hWin, t_st_TilePos *pst_TilePos);
static t_st_Tile *getTileStruct(WM_HWIN hWin);

static U16 getNumberTilesInPlace(void);
static void setRandomTilesOnTable(void);

static void updateProgBar(void);
static void updateTileWin(WM_HWIN hWin);
static void paintTileWin(WM_HWIN hWin);
static void cbRedrawTileWin(WM_HWIN hWin, void *pData);

static void moveTileOnDirection(t_en_Direction en_Direction);
static bool moveTile(t_st_TilePos *pst_TilePosFrom);
static void moveOnDirection(t_en_Direction en_Direction);
static bool moveTileWin(WM_HWIN hWin);

static void touchTileWin(WM_MESSAGE *pMsg);

static void cbTilesWin(WM_MESSAGE *pMsg);
static void cbFTableWin(WM_MESSAGE * pMsg);
static void cbFooterWin(WM_MESSAGE *pMsg);
static void cbBestWin(WM_MESSAGE *pMsg);
static void cbScoreWin(WM_MESSAGE *pMsg);
static void cbNewGameWin(WM_MESSAGE *pMsg);
static void cbGameNameWin(WM_MESSAGE *pMsg);

/*
********************************************************************
*
* Return the U16 len as strlen();
*
********************************************************************
*/
static U16 getU32Size(U32 u32_value)
{
	U16 u16_ret = 0;

	do
	{
		u16_ret++;
		u32_value /= 10;
	} while (u32_value);

	return u16_ret;
}

/*
********************************************************************
*
* Routine to start the tiles to begin the game
*
********************************************************************
*/
static void startTheGame(void)
{
	t_st_TilePos st_TilePos;

	// No Score yet
	addMove(-((I32) u32_Move));

	// Clear the tiles
	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value = 0;
		}
	}

	// Fill the table
	setRandomTilesOnTable();

	updateProgBar();
}

/*
********************************************************************
*
* Routine to add values on score
*
********************************************************************
*/
static void addMove(U32 u32_Value)
{
	u32_Move += u32_Value;

	// Update the Score
	WM_Paint(WM_HWIN_Score);
}

/*
********************************************************************
*
* Returns the position of tile received
*
********************************************************************
*/
static bool getTilePos(WM_HWIN hWin, t_st_TilePos *pst_TilePos)
{
	t_st_TilePos st_TilePos;

	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			if (st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin == hWin)
			{
				*pst_TilePos = st_TilePos;
				return TRUE;
			}
		}
	}

	return FALSE;
}

/*
********************************************************************
*
* Routine to get the internal values of tile based on its ID received
*
********************************************************************
*/
static t_st_Tile *getTileStruct(WM_HWIN hWin)
{
	t_st_TilePos st_TilePos;

	return (getTilePos(hWin, &st_TilePos) ?
			&st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x] :
			NULL);
}

/*
********************************************************************
*
* Get number tile on right place
*
********************************************************************
*/
static U16 getNumberTilesInPlace(void)
{
	t_st_TilePos st_TilePos;
	U16 u16_Number = 0;

	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			if (((st_TilePos.u8_y * TILE_Y_NUM) + (st_TilePos.u8_x + 1)) ==
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value))
			{
				u16_Number++;
			}
		}
	}

	return u16_Number;
}

/*
********************************************************************
*
* Get random free tile
*
********************************************************************
*/
static void setRandomTilesOnTable(void)
{
	t_st_TilePos st_TilePos;
	U16 u16_NumTile, u16_RandomCell;

	/* initialize random seed: */
//	srand(time(NULL));

	for (u16_NumTile = 1; u16_NumTile < (TILE_X_NUM * TILE_Y_NUM); u16_NumTile++)
	{
//		u16_RandomCell = (((rand() % (TILE_X_NUM * TILE_Y_NUM))) + 1);
		u16_RandomCell = (((osKernelSysTick() % (TILE_X_NUM * TILE_Y_NUM))) + 1);

		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value == 0) && (--u16_RandomCell == 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value = u16_NumTile;
					// Next Tile
					st_TilePos.u8_y = TILE_Y_NUM;
					st_TilePos.u8_x = TILE_X_NUM;
				}
			}
		}

		// If didn´t find the place, try again
		if (u16_RandomCell > 0)
		{
			u16_NumTile--;
		}
	}
}

/*
********************************************************************
*
* Routine to update the progress bar
*
********************************************************************
*/
static void updateProgBar(void)
{
	PROGBAR_SetValue(hProgBar, ((getNumberTilesInPlace() * 100) / (TILE_X_NUM * TILE_Y_NUM)));
}

/*
********************************************************************
*
* Routine to redraw the tile
*
********************************************************************
*/
static void updateTileWin(WM_HWIN hWin)
{
	WM_SelectWindow(hWin);
	paintTileWin(hWin);
}

/*
********************************************************************
*
* Routine to paint each piece with its value
*
********************************************************************
*/
static void paintTileWin(WM_HWIN hWin)
{
	t_st_Tile *pst_Tile = getTileStruct(hWin);

	// Use the same color of name
	GUI_SetBkColor((pst_Tile->u8_Value == 0) ? TILE0_WIN_COLOR : TILE_WIN_COLOR);
	GUI_Clear();

	// Print its value if there is
	if (pst_Tile->u8_Value > 0)
	{
		GUI_SetColor(TILE_NUM_WIN_COLOR);
		GUI_SetFont(GUI_FONT_24B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispDecAt(pst_Tile->u8_Value, (TILE_X_PIXEL_SIZE / 2), (TILE_Y_PIXEL_SIZE / 2), 2);
	}
}

/*
********************************************************************
*
* Routine to redraw each tile
*
********************************************************************
*/
static void cbRedrawTileWin(WM_HWIN hWin, void *pData)
{
	updateTileWin(hWin);
}

/*
********************************************************************
*
* Shift the tile on the direction received
*
********************************************************************
*/
static void moveTileOnDirection(t_en_Direction en_Direction)
{
	t_st_TilePos st_TilePos;

	switch (en_Direction)
	{
	case EN_MOVE_UP:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = 0; st_TilePos.u8_y < (TILE_Y_NUM - 1); st_TilePos.u8_y++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u8_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value =
							st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u8_Value;
					st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u8_Value = 0;

					return;
				}
			}
		}
		break;

	case EN_MOVE_DOWN:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = (TILE_Y_NUM - 1); st_TilePos.u8_y > 0; st_TilePos.u8_y--)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u8_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value =
							st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u8_Value;
					st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u8_Value = 0;

					return;
				}
			}
		}
		break;

	case EN_MOVE_LEFT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < (TILE_X_NUM - 1); st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u8_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value =
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u8_Value;
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u8_Value = 0;

					return;
				}
			}
		}
		break;

	case EN_MOVE_RIGHT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = (TILE_X_NUM - 1); st_TilePos.u8_x > 0; st_TilePos.u8_x--)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u8_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u8_Value =
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u8_Value;
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u8_Value = 0;

					return;
				}
			}
		}
		break;
	}
}

/*
********************************************************************
*
* Move the tiles from to positions received.
* Merge them if destination is not blank
*
********************************************************************
*/
static bool moveTile(t_st_TilePos *pst_TilePosFrom)
{
	if (pst_TilePosFrom)
	{
		if (((pst_TilePosFrom->u8_x) > 0) &&
			(st_Table.st_Tiles[pst_TilePosFrom->u8_y][pst_TilePosFrom->u8_x - 1].u8_Value == 0))
		{
			moveOnDirection(EN_MOVE_LEFT);
		}
		else if (((pst_TilePosFrom->u8_x) < (TILE_X_NUM - 1)) &&
				(st_Table.st_Tiles[pst_TilePosFrom->u8_y][pst_TilePosFrom->u8_x + 1].u8_Value == 0))
		{
			moveOnDirection(EN_MOVE_RIGHT);
		}
		else if (((pst_TilePosFrom->u8_y) > 0) &&
			(st_Table.st_Tiles[pst_TilePosFrom->u8_y - 1][pst_TilePosFrom->u8_x].u8_Value == 0))
		{
			moveOnDirection(EN_MOVE_UP);
		}
		else if (((pst_TilePosFrom->u8_y) < (TILE_Y_NUM - 1)) &&
				(st_Table.st_Tiles[pst_TilePosFrom->u8_y + 1][pst_TilePosFrom->u8_x].u8_Value == 0))
		{
			moveOnDirection(EN_MOVE_DOWN);
		}
		else
		{
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

/*
********************************************************************
*
* Shift the tile on the direction received
*
********************************************************************
*/
static void moveOnDirection(t_en_Direction en_Direction)
{
	moveTileOnDirection(en_Direction);

	// Redraw every tile on table
	WM_ForEachDesc(st_Table.WM_HWIN_Table, cbRedrawTileWin, NULL);

	// One tile by time
	addMove(1);

	// New progress
	updateProgBar();

	if (getNumberTilesInPlace() == ((TILE_X_NUM * TILE_Y_NUM) - 1))
	{
		if ((u32_Best == 0) || (u32_Best > u32_Move))
		{
			u32_Best = u32_Move;

			// Update the Best
			WM_Paint(WM_HWIN_Best);
		}

		updateProgBar();

		// You Win
		GUI_MessageBox("YOU WIN", "YOU WIN", (GUI_MESSAGEBOX_CF_MOVEABLE | GUI_MESSAGEBOX_CF_MODAL));

		// New Game
		startTheGame();
	}
}

/*
********************************************************************
*
* Move the tiles from touched tile to tile received
*
********************************************************************
*/
static bool moveTileWin(WM_HWIN hWin)
{
	t_st_TilePos st_TilePosFrom;

	if (getTilePos(hWin, &st_TilePosFrom) && moveTile(&st_TilePosFrom))
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/*
********************************************************************
*
* Routine to handle each tile when touched
*
********************************************************************
*/
static void touchTileWin(WM_MESSAGE *pMsg)
{
	const GUI_PID_STATE *pState = (const GUI_PID_STATE *) pMsg->Data.p;

	/* Something happened in our area (pressed or released) */
	if ((pState) && (pState->Pressed))
	{
		// Move the tiles on table
		moveTileWin(pMsg->hWin);
	}
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
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		paintTileWin(pMsg->hWin);
		break;

	case WM_TOUCH:
		touchTileWin(pMsg);
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
* Callback routine for Game Name window
*
********************************************************************
*/
static void cbBestWin(WM_MESSAGE *pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		GUI_SetBkColor(BEST_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_16B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("BEST", (BEST_X_SIZE / 2), (BEST_Y_SIZE / 3));
		GUI_DispDecAt(u32_Best, (BEST_X_SIZE / 2), ((BEST_Y_SIZE * 2) / 3), getU32Size(u32_Best));
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
static void cbScoreWin(WM_MESSAGE *pMsg)
{
	switch (pMsg->MsgId)
	{
	case WM_PAINT:
		GUI_SetBkColor(MOVE_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_16B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("MOVE", (MOVE_X_SIZE / 2), (MOVE_Y_SIZE / 3));
		GUI_DispDecAt(u32_Move, (MOVE_X_SIZE / 2), ((MOVE_Y_SIZE * 2) / 3), getU32Size(u32_Move));
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
		GUI_SetFont(GUI_FONT_16B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("NEW GAME", (NEWGAME_X_SIZE / 2), (NEWGAME_Y_SIZE / 2));
		break;

	case WM_TOUCH:
		{
			const GUI_PID_STATE *pState = (const GUI_PID_STATE *) pMsg->Data.p;

			/* Something happened in our area (pressed or released) */
			if ((pState) && (pState->Pressed))
			{
				startTheGame();

				// Redraw every tile on table
				WM_ForEachDesc(st_Table.WM_HWIN_Table, cbRedrawTileWin, NULL);
			}
		}
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
		GUI_DispStringAt("Puzzle", (GAME_NAME_X_SIZE / 2), (GAME_NAME_Y_SIZE / 2));
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

	// Create the NewGame Button
	WM_CreateWindow(NEWGAME_X_START, NEWGAME_Y_START, NEWGAME_X_SIZE, NEWGAME_Y_SIZE, WM_CF_SHOW, cbNewGameWin, 0);

	// Create the Progress bar
	hProgBar = PROGBAR_Create(PROGBAR_X_START, PROGBAR_Y_START, PROGBAR_X_SIZE, PROGBAR_Y_SIZE, WM_CF_SHOW);
	PROGBAR_SetBarColor(hProgBar, 0, PROGBAR0_WIN_COLOR);
	PROGBAR_SetBarColor(hProgBar, 1, PROGBAR1_WIN_COLOR);

	// Create the Score
	WM_HWIN_Score = WM_CreateWindow(MOVE_X_START, MOVE_Y_START, MOVE_X_SIZE, MOVE_Y_SIZE, WM_CF_SHOW, cbScoreWin, 0);

	// Create the Best
	WM_HWIN_Best = WM_CreateWindow(BEST_X_START, BEST_Y_START, BEST_X_SIZE, BEST_Y_SIZE, WM_CF_SHOW, cbBestWin, 0);

	// Create the table window
	st_Table.WM_HWIN_Table = WM_CreateWindow(TABLE_X_START, TABLE_Y_START, TABLE_X_SIZE, TABLE_Y_SIZE, WM_CF_SHOW, cbFTableWin, 0);

	// Create the footer
	WM_CreateWindow(FOOTER_X_START, FOOTER_Y_START, FOOTER_X_SIZE, FOOTER_Y_SIZE, WM_CF_SHOW, cbFooterWin, 0);

	startTheGame();

	while (1)
	{
		osEvent evt = osSignalWait (0, 100);
		
		if (evt.status == osEventSignal)
		{
			// handle event status
			switch (evt.value.signals)
			{
				case GYRO_TILT_X_P:
					moveOnDirection(EN_MOVE_DOWN);
					break;
				
				case GYRO_TILT_X_M:
					moveOnDirection(EN_MOVE_UP);
					break;
				
				case GYRO_TILT_Y_P:
					moveOnDirection(EN_MOVE_RIGHT);
					break;
				
				case GYRO_TILT_Y_M:
					moveOnDirection(EN_MOVE_LEFT);
					break;
				
				case GYRO_TILT_Z_P:
					break;
				
				case GYRO_TILT_Z_M:
					break;
				
				case GYRO_TILT_NULL:
					break;
				
			}
			
			osDelay(500);
		}
		
		/* All GUI related activities might only be called from here */
		GUI_TOUCH_Exec();     /* Optional Touch support, uncomment if required */
		GUI_Exec();             /* Execute all GUI jobs ... Return 0 if nothing was done. */
		GUI_X_ExecIdle();       /* Nothing left to do for the moment ... Idle processing */
	}
}

