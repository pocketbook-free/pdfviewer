#ifndef PBTOUCHZOOMDLG_H
#define PBTOUCHZOOMDLG_H

#include "settings.h"
#include <pbframework/pbframework.h>

class PBTouchZoomDlg : public PBWindow
{
public:
	static const int WND_WIDTH = 459;
	static const int WND_HEIGHT = 460;

	PBTouchZoomDlg();

	// virtual overloads
	int OnDraw(bool force);
	int OnCreate();

	void Show(int x, int y, TZoomType zoom_type, int zoom_param);
	void Hide();

	int MainHandler(int type, int par1, int par2);

	PBBitmapButton* CreateButton(int x, int y, int width, int height, int commandID, const ibitmap* pIcon, const char* label);
	PBLabel* CreateLabel(int x, int y, int width, int height, const char* label);

	PBWindow* FindNearestWindow(int x, int y, int direction);
	PBWindow* FindChildByCommandId(int commandId);

	static int GetNextZoomParam(TZoomType zoom_type, int zoom_param, bool decrement);
	static void GetNextZoomParam(TZoomType zoom_type, int zoom_param, bool decrement, TZoomType & new_zoom_type, int & new_zoom_param);

protected:
	ibitmap *m_pImgSave;
	PBBitmapButton* m_pPreview4;
	PBBitmapButton* m_pPreview9;

protected:
	PBWindow* MoveFocus(int direction);
	PBWindow* MoveFocusToNextTabstop();
	PBWindow* MoveFocusToPrevTabstop();

	void Command2Param(int commandId, TZoomType& zoom_type, int& zoom_param);
	int Param2Command(TZoomType zoom_type, int zoom_param);

	int OnCommand(int commandId, int par1, int par2);
};

#endif // PBTOUCHZOOMDLG_H
