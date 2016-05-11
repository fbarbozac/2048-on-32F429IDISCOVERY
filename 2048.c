#include <stddef.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

#define WIDGET_USE_FLEX_SKIN	(1)
#define WM_SUPPORT_TRANSPARENCY	(0)
#define GUI_SUPPORT_ROTATION	(0)

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

#define	SCORE_X_START		((GAME_NAME_X_START + GAME_NAME_X_SIZE) + GAP_X_SIZE)
#define	SCORE_Y_START		(GAME_NAME_Y_START)

#define	SCORE_X_SIZE		(GAME_NAME_X_SIZE)
#define	SCORE_Y_SIZE		(GAME_NAME_Y_SIZE + NEWGAME_Y_SIZE + GAP_Y_SIZE)

#define	BEST_X_START		((SCORE_X_START + SCORE_X_SIZE) + GAP_X_SIZE)
#define	BEST_Y_START		(GAME_NAME_Y_START)

#define	BEST_X_SIZE			(GAME_NAME_X_SIZE)
#define	BEST_Y_SIZE			(SCORE_Y_SIZE)

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

#define	NUM_TILES_ON_START	(2)		// Number of tiles to fill at start

#define	TIME_BLINK_ON_MERGE	(300)	// Time of tile blink when it is merged in ms. 0 to disable

//RGB
#define	RGB_TO_HEX(r,g,b)	((r << 0) + (g << 8) + (b << 16))

// Table color
#define	TABLE_WIN_COLOR		RGB_TO_HEX(0xBB, 0xAD, 0xA0)
#define	GAME_NAME_WIN_COLOR	RGB_TO_HEX(0xED, 0xC5, 0x3F)
#define	NEWGAME_WIN_COLOR	RGB_TO_HEX(0xF2, 0xB1, 0x79)
#define	SCORE_WIN_COLOR		TABLE_WIN_COLOR
#define	BEST_WIN_COLOR		TABLE_WIN_COLOR
#define	FOOTER_WIN_COLOR	GUI_DARKGREEN
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
	U16 u16_Value;
	bool bool_Touched;
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
static U32 u32_Score, u32_Best;
static PROGBAR_Handle hProgBar;
static WM_HWIN WM_HWIN_Score;
static WM_HWIN WM_HWIN_Best;
static t_st_Table st_Table;

static osThreadId id_GUIThread;

static const U32 u32_RGBTile[16] = {
	RGB_TO_HEX(0xCD, 0xC0, 0xB4),	/*	(0 << 0)	*/
	RGB_TO_HEX(0xEE, 0xE4, 0xDA),	/*	(1 << 1)	*/
	RGB_TO_HEX(0xED, 0xE0, 0xC8),	/*	(1 << 2)	*/
	RGB_TO_HEX(0xF2, 0xB1, 0x79),	/*	(1 << 3)	*/
	RGB_TO_HEX(0xF5, 0x95, 0x63),	/*	(1 << 4)	*/
	RGB_TO_HEX(0xF6, 0x7C, 0x5F),	/*	(1 << 5)	*/
	RGB_TO_HEX(0xF6, 0x5E, 0x3B),	/*	(1 << 6)	*/
	RGB_TO_HEX(0xED, 0xCF, 0x72),	/*	(1 << 7)	*/
	RGB_TO_HEX(0xED, 0xCC, 0x61),	/*	(1 << 8)	*/
	RGB_TO_HEX(0xED, 0xC8, 0x50),	/*	(1 << 9)	*/
	RGB_TO_HEX(0xED, 0xC5, 0x3F),	/*	(1 << 10)	*/
	RGB_TO_HEX(0xED, 0xC2, 0x2E),	/*	(1 << 11)	*/
	RGB_TO_HEX(0x3C, 0x3A, 0x32),	/*	(1 << 12)	*/
	RGB_TO_HEX(0x3C, 0x3A, 0x32),	/*	(1 << 13)	*/
	RGB_TO_HEX(0x3C, 0x3A, 0x32),	/*	(1 << 14)	*/
	RGB_TO_HEX(0x3C, 0x3A, 0x32),	/*	(1 << 15)	*/
};

static const U32 u32_RGBNumbers[16] = {
	RGB_TO_HEX(0x77, 0x6E, 0x65),	/*	(0 << 0)	*/
	RGB_TO_HEX(0x77, 0x6E, 0x65),	/*	(1 << 1)	*/
	RGB_TO_HEX(0x77, 0x6E, 0x65),	/*	(1 << 2)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 3)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 4)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 5)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 6)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 7)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 8)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 9)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 10)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 11)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 12)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 13)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 14)	*/
	RGB_TO_HEX(0xF9, 0xF6, 0xF2),	/*	(1 << 15)	*/
};

/*
********************************************************************
*
* Prototypes
*
********************************************************************
*/
static U16 getU32Size(U32 value);
static void startTheGame(void);
static void addScore(U32 u32_Value);

static bool getTouchedTilePos(t_st_TilePos *pst_TilePos);
static bool getTilePos(WM_HWIN hWin, t_st_TilePos *pst_TilePos);
static t_st_Tile *getTileStruct(WM_HWIN hWin);

static bool isNewMatchPossible(void);
static U16 getAvailableTileNumber(void);
static void setRandomFreeTile(void);

static void updateProgBar(void);
static void updateTileWin(WM_HWIN hWin);
static void paintTileWin(WM_HWIN hWin);
static void cbRedrawTileWin(WM_HWIN hWin, void *pData);
static void blinkTileWin(WM_HWIN hWin);

static void shiftTileOnDirection(t_en_Direction en_Direction);
static void mergeTileOnDirection(t_en_Direction en_Direction);
static bool isMergeTileOnDirectionPossible(t_en_Direction en_Direction);
static void moveTileOnDirection(t_en_Direction en_Direction);
static bool moveTile(t_st_TilePos *pst_TilePosFrom, t_st_TilePos *pst_TilePosTo);
static void moveOnDirection(t_en_Direction en_Direction);
static bool moveTileWin(WM_HWIN hWin, t_st_TilePos *pst_TilePosFrom);

static bool touchTileWin(WM_MESSAGE *pMsg);

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
	U8 u8_Tile;

	// No Score yet
	addScore(-((I32) u32_Score));

	// Clear the tiles
	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value = 0;
		}
	}

	// Fill some ones
	for (u8_Tile = 0; u8_Tile < NUM_TILES_ON_START; u8_Tile++)
	{
		setRandomFreeTile();
	}

	updateProgBar();
}

/*
********************************************************************
*
* Routine to add values on score
*
********************************************************************
*/
static void addScore(U32 u32_Value)
{
	u32_Score += u32_Value;

	// Update the Score
	WM_Paint(WM_HWIN_Score);

	if (u32_Score > u32_Best)
	{
		u32_Best = u32_Score;

		// Update the Best
		WM_Paint(WM_HWIN_Best);
	}
}

/*
********************************************************************
*
* Returns the position of already touched tile
*
********************************************************************
*/
static bool getTouchedTilePos(t_st_TilePos *pst_TilePos)
{
	t_st_TilePos st_TilePos;

	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			if (st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].bool_Touched)
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
* Returns TRUE if there is any possible Match
*
********************************************************************
*/
static bool isNewMatchPossible(void)
{
	t_en_Direction en_Direction;

	// If there is place to tile, there is new possible match
	if (getAvailableTileNumber())
	{
		return TRUE;
	}

	// Since the table is already full, only check if there is anyone to merge
	for (en_Direction = EN_MOVE_MIN; en_Direction < EN_MOVE_MAX; en_Direction++)
	{
		if (isMergeTileOnDirectionPossible(en_Direction))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/*
********************************************************************
*
* Get random free tile
*
********************************************************************
*/
static U16 getAvailableTileNumber(void)
{
	t_st_TilePos st_TilePos;
	U16 u16_Number = 0;

	for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
	{
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			if (st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0)
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
static void setRandomFreeTile(void)
{
	U16 u16_NumFreeTiles = getAvailableTileNumber();

	/* initialize random seed: */
//	srand(time(NULL));

	if (u16_NumFreeTiles)
	{
		t_st_TilePos st_TilePos;
//		U16 u16_RandomCell = ((rand() % u16_NumFreeTiles) + 1);
		U16 u16_RandomCell = ((osKernelSysTick() % u16_NumFreeTiles) + 1);

		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0) && (--u16_RandomCell == 0))
				{
					// Fill with random value 2 or 4
//					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value = (U16) ((rand() & 2) | 4);
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value = (U16) ((osKernelSysTick() & 2) | 4);
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value -= 2;

					return;
				}
			}
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
	PROGBAR_SetValue(hProgBar, ((((TILE_X_NUM * TILE_Y_NUM) - getAvailableTileNumber()) * 100) / (TILE_X_NUM * TILE_Y_NUM)));
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
	U16 u16_HigherBit = 16;

	while (--u16_HigherBit)
	{
		if (pst_Tile->u16_Value & (1 << u16_HigherBit))
		{
			break;
		}
	}

	GUI_SetBkColor(pst_Tile->bool_Touched ? GUI_LIGHTMAGENTA : u32_RGBTile[u16_HigherBit]);
	GUI_Clear();

	// Print its value if there is
	if (pst_Tile->u16_Value > 0)
	{
		GUI_SetColor(u32_RGBNumbers[u16_HigherBit]);
		GUI_SetFont(GUI_FONT_24B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispDecAt(pst_Tile->u16_Value, (TILE_X_PIXEL_SIZE / 2), (TILE_Y_PIXEL_SIZE / 2), getU32Size(pst_Tile->u16_Value));
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
* Shift the tile on the direction received
*
********************************************************************
*/
static void shiftTileOnDirection(t_en_Direction en_Direction)
{
	t_st_TilePos st_TilePos;

	switch (en_Direction)
	{
	case EN_MOVE_UP:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = 0; st_TilePos.u8_y < (TILE_Y_NUM - 1); st_TilePos.u8_y++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value =
							st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value;
					st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value = 0;

					shiftTileOnDirection(en_Direction);
				}
			}
		}
		break;

	case EN_MOVE_DOWN:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = (TILE_Y_NUM - 1); st_TilePos.u8_y > 0; st_TilePos.u8_y--)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value =
							st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value;
					st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value = 0;

					shiftTileOnDirection(en_Direction);
				}
			}
		}
		break;

	case EN_MOVE_LEFT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < (TILE_X_NUM - 1); st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value =
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value;
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value = 0;

					shiftTileOnDirection(en_Direction);
				}
			}
		}
		break;

	case EN_MOVE_RIGHT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = (TILE_X_NUM - 1); st_TilePos.u8_x > 0; st_TilePos.u8_x--)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value == 0) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value > 0))
				{
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value =
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value;
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value = 0;

					shiftTileOnDirection(en_Direction);
				}
			}
		}
		break;
	}
}

/*
********************************************************************
*
* Merge the tiles on the direction received
*
********************************************************************
*/
static void mergeTileOnDirection(t_en_Direction en_Direction)
{
	t_st_TilePos st_TilePos;

	switch (en_Direction)
	{
	case EN_MOVE_UP:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = 0; st_TilePos.u8_y < (TILE_Y_NUM - 1); st_TilePos.u8_y++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value))
				{
					// Merge them and sum to score
					addScore(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value *= 2);
					st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value = 0;

					// Blink the merged Tile
					blinkTileWin(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin);
				}
			}
		}
		break;

	case EN_MOVE_DOWN:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = TILE_Y_NUM; st_TilePos.u8_y--;)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value))
				{
					// Merge them and sum to score
					addScore(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value *= 2);
					st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value = 0;

					// Blink the merged Tile
					blinkTileWin(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin);
				}
			}
		}
		break;

	case EN_MOVE_LEFT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < (TILE_X_NUM - 1); st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value))
				{
					// Merge them and sum to score
					addScore(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value *= 2);
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value = 0;

					// Blink the merged Tile
					blinkTileWin(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin);
				}
			}
		}
		break;

	case EN_MOVE_RIGHT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = TILE_X_NUM; st_TilePos.u8_x--;)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value))
				{
					// Merge them and sum to score
					addScore(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value *= 2);
					st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value = 0;

					// Blink the merged Tile
					blinkTileWin(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin);
				}
			}
		}
		break;
	}
}

/*
********************************************************************
*
* Return TRUE if there is any possible merge on the direction received.
* Must be called only after the tiles is shifted or no available space
*
********************************************************************
*/
static bool isMergeTileOnDirectionPossible(t_en_Direction en_Direction)
{
	t_st_TilePos st_TilePos;

	switch (en_Direction)
	{
	case EN_MOVE_UP:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = 0; st_TilePos.u8_y < (TILE_Y_NUM - 1); st_TilePos.u8_y++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y + 1][st_TilePos.u8_x].u16_Value))
				{
					return TRUE;
				}
			}
		}
		break;

	case EN_MOVE_DOWN:
		for (st_TilePos.u8_x = 0; st_TilePos.u8_x < TILE_X_NUM; st_TilePos.u8_x++)
		{
			for (st_TilePos.u8_y = TILE_Y_NUM; st_TilePos.u8_y--;)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y - 1][st_TilePos.u8_x].u16_Value))
				{
					return TRUE;
				}
			}
		}
		break;

	case EN_MOVE_LEFT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = 0; st_TilePos.u8_x < (TILE_X_NUM - 1); st_TilePos.u8_x++)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x + 1].u16_Value))
				{
					return TRUE;
				}
			}
		}
		break;

	case EN_MOVE_RIGHT:
		for (st_TilePos.u8_y = 0; st_TilePos.u8_y < TILE_Y_NUM; st_TilePos.u8_y++)
		{
			for (st_TilePos.u8_x = TILE_X_NUM; st_TilePos.u8_x--;)
			{
				if ((st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value) &&
					(st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].u16_Value ==
							st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x - 1].u16_Value))
				{
					return TRUE;
				}
			}
		}
		break;
	}

	return FALSE;
}

/*
********************************************************************
*
* Move the tiles on the direction received
*
********************************************************************
*/
static void moveTileOnDirection(t_en_Direction en_Direction)
{
	shiftTileOnDirection(en_Direction);
	mergeTileOnDirection(en_Direction);
	shiftTileOnDirection(en_Direction);
}

/*
********************************************************************
*
* Move the tiles from to positions received.
* Merge them if destination is not blank
*
********************************************************************
*/
static bool moveTile(t_st_TilePos *pst_TilePosFrom, t_st_TilePos *pst_TilePosTo)
{
	if (pst_TilePosFrom && pst_TilePosTo)
	{
		if (pst_TilePosFrom->u8_x > pst_TilePosTo->u8_x)
		{
			moveOnDirection(EN_MOVE_LEFT);
		}
		else if (pst_TilePosFrom->u8_x < pst_TilePosTo->u8_x)
		{
			moveOnDirection(EN_MOVE_RIGHT);
		}
		else if (pst_TilePosFrom->u8_y > pst_TilePosTo->u8_y)
		{
			moveOnDirection(EN_MOVE_UP);
		}
		else if (pst_TilePosFrom->u8_y < pst_TilePosTo->u8_y)
		{
			moveOnDirection(EN_MOVE_DOWN);
		}
		else
		{
			return FALSE;
		}
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

	if (isNewMatchPossible())
	{
		// Since it moved, add new tile
		setRandomFreeTile();

		// New progress
		updateProgBar();
	}
	else
	{
		// Game Over
		GUI_MessageBox("GAME OVER", "GAME OVER", (GUI_MESSAGEBOX_CF_MOVEABLE | GUI_MESSAGEBOX_CF_MODAL));

		// New Game
		startTheGame();
	}

	// Redraw every tile on table
	WM_ForEachDesc(st_Table.WM_HWIN_Table, cbRedrawTileWin, NULL);
}

/*
********************************************************************
*
* Move the tiles from touched tile to tile received
*
********************************************************************
*/
static bool moveTileWin(WM_HWIN hWin, t_st_TilePos *pst_TilePosFrom)
{
	t_st_TilePos st_TilePosTo;

	if (getTilePos(hWin, &st_TilePosTo) && moveTile(pst_TilePosFrom, &st_TilePosTo))
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
static bool touchTileWin(WM_MESSAGE *pMsg)
{
	bool bool_ret = FALSE;
	const GUI_PID_STATE *pState = (const GUI_PID_STATE *) pMsg->Data.p;

	/* Something happened in our area (pressed or released) */
	if ((pState) && (pState->Pressed))
	{
		t_st_Tile *pst_Tile = getTileStruct(pMsg->hWin);
		t_st_TilePos st_TilePos;

		// If there is already other touched, move to it
		if (getTouchedTilePos(&st_TilePos) && (st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].hWin != pst_Tile->hWin))
		{
			// Untouched any more
			st_Table.st_Tiles[st_TilePos.u8_y][st_TilePos.u8_x].bool_Touched = FALSE;

			// Move the tiles on table
			bool_ret = moveTileWin(pMsg->hWin, &st_TilePos);

			// Redraw just it if it didn't move
			updateTileWin(pMsg->hWin);
		}
		else if (pst_Tile->bool_Touched == FALSE)
		{
			// Otherwise, touch this
			pst_Tile->bool_Touched = TRUE;

			// Redraw just it
			updateTileWin(pMsg->hWin);
		}
	}

	return bool_ret;
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
	static bool bool_Moved;

	switch (pMsg->MsgId)
	{
	case WM_TIMER:
		if (WM_GetTimerId(pMsg->Data.v) == 1)
		{
			bool_Moved = FALSE;
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
		paintTileWin(pMsg->hWin);
		break;

	case WM_TOUCH:
		if ((bool_Moved == FALSE) && touchTileWin(pMsg))
		{
			bool_Moved = TRUE;

			WM_CreateTimer(pMsg->hWin, 1, 1000, 0);
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
		GUI_SetBkColor(SCORE_WIN_COLOR);
		GUI_Clear();
		GUI_SetColor(GUI_WHITE);
		GUI_SetFont(GUI_FONT_16B_1);
		GUI_SetTextMode(GUI_TM_TRANS);
		GUI_SetTextAlign(GUI_TA_HCENTER | GUI_TA_VCENTER);
		GUI_DispStringAt("SCORE", (SCORE_X_SIZE / 2), (SCORE_Y_SIZE / 3));
		GUI_DispDecAt(u32_Score, (SCORE_X_SIZE / 2), ((SCORE_Y_SIZE * 2) / 3), getU32Size(u32_Score));
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
		GUI_DispStringAt("2048", (GAME_NAME_X_SIZE / 2), (GAME_NAME_Y_SIZE / 2));
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
	WM_HWIN_Score = WM_CreateWindow(SCORE_X_START, SCORE_Y_START, SCORE_X_SIZE, SCORE_Y_SIZE, WM_CF_SHOW, cbScoreWin, 0);

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
		
		
		GUI_TOUCH_Exec();	/* Optional Touch support, uncomment if required */
		GUI_Exec();			/* Execute all GUI jobs ... Return 0 if nothing was done. */
		GUI_X_ExecIdle();	/* Nothing left to do for the moment ... Idle processing */
	}
}

